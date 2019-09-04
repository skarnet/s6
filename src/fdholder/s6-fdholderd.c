/* ISC license. */

#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <regex.h>

#include <skalibs/posixplz.h>
#include <skalibs/posixishard.h>
#include <skalibs/types.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/sgetopt.h>
#include <skalibs/env.h>
#include <skalibs/bytestr.h>
#include <skalibs/error.h>
#include <skalibs/strerr2.h>
#include <skalibs/tai.h>
#include <skalibs/djbunix.h>
#include <skalibs/sig.h>
#include <skalibs/iopause.h>
#include <skalibs/selfpipe.h>
#include <skalibs/cdb.h>
#include <skalibs/getpeereid.h>
#include <skalibs/webipc.h>
#include <skalibs/genset.h>
#include <skalibs/avltreen.h>
#include <skalibs/unixmessage.h>
#include <skalibs/unixconnection.h>
#include <s6/accessrules.h>
#include <s6/s6-fdholder.h>

#define USAGE "s6-fdholderd [ -v verbosity ] [ -1 ] [ -c maxconn ] [ -n maxfds ] [ -t timeout ] [ -T lameducktimeout ] [ -i rulesdir | -x rulesfile ]"
#define dieusage() strerr_dieusage(100, USAGE) ;

static unsigned int verbosity = 1 ;
static int cont = 1 ;
static tain_t answertto = TAIN_INFINITE_RELATIVE ;
static tain_t lameduckdeadline = TAIN_INFINITE_RELATIVE ;
static tain_t const nano1 = { .sec = TAI_ZERO, .nano = 1 } ;

static unsigned int rulestype = 0 ;
static char const *rules = 0 ;
static int cdbfd = -1 ;
static struct cdb cdbmap = CDB_ZERO ;

static void handle_signals (void)
{
  for (;;) switch (selfpipe_read())
  {
    case -1 : strerr_diefu1sys(111, "selfpipe_read()") ;
    case 0 : return ;
    case SIGTERM :
    {
      if (cont)
      {
        cont = 0 ;
        tain_add_g(&lameduckdeadline, &lameduckdeadline) ;
      }
      break ;
    }
    case SIGHUP :
    {
      int fd ;
      struct cdb c = CDB_ZERO ;
      if (rulestype != 2) break ;
      fd = open_readb(rules) ;
      if (fd < 0) break ;
      if (cdb_init(&c, fd) < 0)
      {
        fd_close(fd) ;
        break ;
      }
      cdb_free(&cdbmap) ;
      fd_close(cdbfd) ;
      cdbfd = fd ;
      cdbmap = c ;
    }
    break ;
    default : break ;
  }
}


 /* fd store */

static genset *fdstore ;
#define FD(i) genset_p(s6_fdholder_fd_t, fdstore, (i))
static unsigned int maxfds = 1000 ;
#define numfds genset_n(fdstore)
static avltreen *fds_by_id ;
static avltreen *fds_by_deadline ;

static void *fds_id_dtok (uint32_t d, void *x)
{
  (void)x ;
  return FD(d)->id ;
}

static int fds_id_cmp (void const *a, void const *b, void *x)
{
  (void)x ;
  return strcmp((char const *)a, (char const *)b) ;
}

static void *fds_deadline_dtok (uint32_t d, void *x)
{
  (void)x ;
  return &FD(d)->limit ;
}

static int fds_deadline_cmp (void const *a, void const *b, void *x)
{
  tain_t const *aa = (tain_t const *)a ;
  tain_t const *bb = (tain_t const *)b ;
  (void)x ;
  return tain_less(aa, bb) ? -1 : tain_less(bb, aa) ;
}

static void fds_delete (uint32_t pp)
{
  avltreen_delete(fds_by_id, fds_id_dtok(pp, 0)) ;
  avltreen_delete(fds_by_deadline, fds_deadline_dtok(pp, 0)) ;
  genset_delete(fdstore, pp) ;
}

static void fds_close_and_delete (uint32_t pp)
{
  fd_close(FD(pp)->fd) ;
  fds_delete(pp) ;
}


 /* client connection */

typedef struct client_s client_t, *client_t_ref ;
struct client_s
{
  uint32_t next ;
  uint32_t xindex ;
  tain_t deadline ;
  regex_t rre ;
  regex_t wre ;
  unsigned int dumping ;
  unsigned int flags ;
  unixconnection_t connection ;
} ;

static genset *clients ;
static uint32_t sentinel ;
#define CLIENT(i) genset_p(client_t, clients, (i))
#define numconn (genset_n(clients) - 1)

static inline void client_free (client_t *c)
{
  fd_close(unixmessage_sender_fd(&c->connection.out)) ;
  unixconnection_free(&c->connection) ;
  regfree(&c->rre) ;
  regfree(&c->wre) ;
}

static inline void client_delete (uint32_t cc, uint32_t prev)
{
  client_t *c = CLIENT(cc) ;
  CLIENT(prev)->next = c->next ;
  client_free(c) ;
  genset_delete(clients, cc) ;
}

static void removeclient (uint32_t *i, uint32_t j)
{
  client_delete(*i, j) ;
  *i = j ;
}

static void client_setdeadline (client_t *c)
{
  tain_t blah ;
  tain_half(&blah, &tain_infinite_relative) ;
  tain_add_g(&blah, &blah) ;
  if (tain_less(&blah, &c->deadline))
    tain_add_g(&c->deadline, &answertto) ;
}

static inline int client_prepare_iopause (uint32_t i, tain_t *deadline, iopause_fd *x, uint32_t *j)
{
  client_t *c = CLIENT(i) ;
  if (tain_less(&c->deadline, deadline)) *deadline = c->deadline ;
  if (!unixmessage_sender_isempty(&c->connection.out) || !unixmessage_receiver_isempty(&c->connection.in) || (cont && !unixmessage_receiver_isfull(&c->connection.in)))
  {
    x[*j].fd = unixmessage_sender_fd(&c->connection.out) ;
    x[*j].events = (!unixmessage_receiver_isempty(&c->connection.in) || (cont && !unixmessage_receiver_isfull(&c->connection.in)) ? IOPAUSE_READ : 0) | (!unixmessage_sender_isempty(&c->connection.out) ? IOPAUSE_WRITE : 0) ;
    c->xindex = (*j)++ ;
  }
  else c->xindex = 0 ;
  return !!c->xindex ;
}

static inline void client_add (uint32_t *cc, int fd, regex_t const *rre, regex_t const *wre, unsigned int flags)
{
  uint32_t i ;
  client_t *c ;
  i = genset_new(clients) ;
  c = CLIENT(i) ;
  tain_add_g(&c->deadline, &answertto) ;
  c->rre = *rre ;
  c->wre = *wre ;
  c->dumping = 0 ;
  c->flags = flags ;
  unixconnection_init(&c->connection, fd, fd) ;
  c->next = CLIENT(sentinel)->next ;
  CLIENT(sentinel)->next = i ;
  *cc = i ;
}

static inline int client_flush (uint32_t i, iopause_fd const *x)
{
  client_t *c = CLIENT(i) ;
  if (c->xindex && (x[c->xindex].revents & IOPAUSE_WRITE))
  {
    if (unixconnection_flush(&c->connection))
      tain_add_g(&c->deadline, &tain_infinite_relative) ;
    else if (!error_isagain(errno)) return 0 ;
  }
  return 1 ;
}

static int answer (client_t *c, char e)
{
  unixmessage_t m = { .s = &e, .len = 1, .fds = 0, .nfds = 0 } ;
  if (!unixmessage_put(&c->connection.out, &m)) return 0 ;
  client_setdeadline(c) ;
  return 1 ;
}

static int do_store (uint32_t cc, unixmessage_t const *m)
{
  uint32_t pp, idlen ;
  client_t *c = CLIENT(cc) ;
  s6_fdholder_fd_t *p ;
  if (c->dumping || m->len < TAIN_PACK + 3 || m->nfds != 1) return (errno = EPROTO, 0) ;
  idlen = (unsigned char)m->s[TAIN_PACK] ;
  if (idlen > S6_FDHOLDER_ID_SIZE || idlen + 2 + TAIN_PACK != m->len || m->s[1 + TAIN_PACK + idlen]) return (errno = EPROTO, 0) ;
  if (regexec(&c->wre, m->s + TAIN_PACK + 1, 0, 0, 0))
  {
    unixmessage_drop(m) ;
    return answer(c, EPERM) ;
  }
  if (numfds >= maxfds)
  {
    unixmessage_drop(m) ;
    return answer(c, ENFILE) ;
  }
  if (avltreen_search(fds_by_id, m->s + TAIN_PACK + 1, &pp))
  {
    unixmessage_drop(m) ;
    return answer(c, EBUSY) ;
  }
  if (!answer(c, 0)) return 0 ;
  pp = genset_new(fdstore) ; p = FD(pp) ;
  tain_unpack(m->s, &p->limit) ; p->fd = m->fds[0] ;
  memcpy(p->id, m->s + TAIN_PACK + 1, idlen) ;
  memset(p->id + idlen, 0, S6_FDHOLDER_ID_SIZE + 1 - idlen) ;
  for (;;)
  {
    uint32_t dummy ;
    if (!avltreen_search(fds_by_deadline, &p->limit, &dummy)) break ;
    tain_add(&p->limit, &p->limit, &nano1) ;
  }
  avltreen_insert(fds_by_id, pp) ;
  avltreen_insert(fds_by_deadline, pp) ;
  return 1 ;
}

static int do_delete (uint32_t cc, unixmessage_t const *m)
{
  uint32_t pp, idlen ;
  client_t *c = CLIENT(cc) ;
  if (c->dumping || m->len < 3 || m->nfds) return (errno = EPROTO, 0) ;
  idlen = (unsigned char)m->s[0] ;
  if (idlen > S6_FDHOLDER_ID_SIZE || idlen + 2 != m->len || m->s[idlen + 1]) return (errno = EPROTO, 0) ;
  if (regexec(&c->wre, m->s + 1, 0, 0, 0)) return answer(c, EPERM) ;
  if (!avltreen_search(fds_by_id, m->s + 1, &pp)) return answer(c, ENOENT) ;
  if (!answer(c, 0)) return 0 ;
  fds_close_and_delete(pp) ;
  return 1 ;
}

static int do_retrieve (uint32_t cc, unixmessage_t const *m)
{
  int fd ;
  unixmessage_t ans = { .s = "", .len = 1, .fds = &fd, .nfds = 1 } ;
  uint32_t pp, idlen ;
  client_t *c = CLIENT(cc) ;
  if (c->dumping || m->len < 4 || m->nfds) return (errno = EPROTO, 0) ;
  idlen = (unsigned char)m->s[1] ;
  if (idlen > S6_FDHOLDER_ID_SIZE || idlen + 3 != m->len || m->s[idlen + 2]) return (errno = EPROTO, 0) ;
  if (regexec(&c->rre, m->s + 2, 0, 0, 0)) return answer(c, EPERM) ;
  if (m->s[0] && regexec(&c->wre, m->s + 2, 0, 0, 0)) return answer(c, EPERM) ;
  if (!avltreen_search(fds_by_id, m->s + 2, &pp)) return answer(c, ENOENT) ;
  fd = FD(pp)->fd ;
  if (!unixmessage_put_and_close(&c->connection.out, &ans, m->s[0] ? unixmessage_bits_closeall : unixmessage_bits_closenone)) return 0 ;
  if (m->s[0]) fds_delete(pp) ;
  return 1 ;
}

static int fill_siovec_with_ids_iter (char *thing, void *data)
{
  struct iovec *v = (*(struct iovec **)data)++ ;
  s6_fdholder_fd_t *p = (s6_fdholder_fd_t *)thing ;
  v->iov_base = p->id ;
  v->iov_len = strlen(p->id) + 1 ;
  return 1 ;
}

static int do_list (uint32_t cc, unixmessage_t const *m)
{
  client_t *c = CLIENT(cc) ;
  struct iovec v[1+numfds] ;
  unixmessage_v_t ans = { .v = v, .vlen = 1+numfds, .fds = 0, .nfds = 0 } ;
  struct iovec *vp = v + 1 ;
  char pack[5] = "" ;
  if (c->dumping || m->len || m->nfds) return (errno = EPROTO, 0) ;
  if (!(c->flags & 4)) return answer(c, EPERM) ;
  uint32_pack_big(pack + 1, (uint32_t)numfds) ;
  v[0].iov_base = pack ; v[0].iov_len = 5 ;
  genset_iter(fdstore, &fill_siovec_with_ids_iter, &vp) ;
  if (!unixmessage_putv(&c->connection.out, &ans)) return answer(c, errno) ;
  client_setdeadline(c) ;
  return 1 ;
}

typedef struct getdumpiter_s getdumpiter_t, *getdumpiter_t_ref ;
struct getdumpiter_s
{
  struct iovec *v ;
  int *fd ;
  char *limit ;
} ;

static int getdump_iter (char *thing, void *stuff)
{
  s6_fdholder_fd_t *p = (s6_fdholder_fd_t *)thing ;
  getdumpiter_t *blah = stuff ;
  unsigned char len = strlen(p->id) ;
  tain_pack(blah->limit, &p->limit) ;
  blah->limit[TAIN_PACK] = len ;
  blah->v->iov_base = p->id ;
  blah->v->iov_len = len + 1 ;
  *blah->fd++ = p->fd ;
  blah->v += 2 ;
  blah->limit += TAIN_PACK + 1 ;
  return 1 ;
}

static int do_getdump (uint32_t cc, unixmessage_t const *m)
{
  uint32_t n = numfds ? 1 + ((numfds-1) / UNIXMESSAGE_MAXFDS) : 0 ;
  client_t *c = CLIENT(cc) ;
  if (c->dumping || m->len || m->nfds) return (errno = EPROTO, 0) ;
  if (!(c->flags & 1)) return answer(c, EPERM) ;
  {
    char pack[9] = "" ;
    unixmessage_t ans = { .s = pack, .len = 9, .fds = 0, .nfds = 0 } ;
    uint32_pack_big(pack+1, n) ;
    uint32_pack_big(pack+5, numfds) ;
    if (!unixmessage_put(&c->connection.out, &ans)) return answer(c, errno) ;
  }
  if (n)
  {
    uint32_t i = 0 ;
    unixmessage_v_t ans[n] ;
    struct iovec v[numfds << 1] ;
    int fds[numfds] ;
    char limits[(TAIN_PACK+1) * numfds] ;
    for (; i < numfds ; i++)
    {
      v[i<<1].iov_base = limits + i * (TAIN_PACK+1) ;
      v[i<<1].iov_len = TAIN_PACK+1 ;
    }
    {
      getdumpiter_t state = { .v = v+1, .fd = fds, .limit = limits } ;
      genset_iter(fdstore, &getdump_iter, &state) ;
    }
    for (i = 0 ; i < n-1 ; i++)
    {
      ans[i].v = v + (i<<1) * UNIXMESSAGE_MAXFDS ;
      ans[i].vlen = UNIXMESSAGE_MAXFDS << 1 ;
      ans[i].fds = fds + i * UNIXMESSAGE_MAXFDS ;
      ans[i].nfds = UNIXMESSAGE_MAXFDS ;
    }
    ans[n-1].v = v + ((n-1)<<1) * UNIXMESSAGE_MAXFDS ;
    ans[n-1].vlen = (1 + (numfds-1) % UNIXMESSAGE_MAXFDS) << 1 ;
    ans[n-1].fds = fds + (n-1) * UNIXMESSAGE_MAXFDS ;
    ans[n-1].nfds = 1 + (numfds - 1) % UNIXMESSAGE_MAXFDS ;
    for (i = 0 ; i < n ; i++)
    {
      if (!unixmessage_putv(&c->connection.out, ans + i))
      {
        int e = errno ;
        ++i ;
        while (i--) unixmessage_unput(&c->connection.out) ;
        return answer(c, e) ;
      }
    }
  }
  client_setdeadline(c) ;
  return 1 ;
}

static int do_setdump (uint32_t cc, unixmessage_t const *m)
{
  char pack[5] = "" ;
  uint32_t n ;
  unixmessage_t ans = { .s = pack, .len = 5, .fds = 0, .nfds = 0 } ;
  client_t *c = CLIENT(cc) ;
  if (c->dumping || m->len != 4 || m->nfds) return (errno = EPROTO, 0) ;
  if (!(c->flags & 2)) return answer(c, EPERM) ;
  uint32_unpack_big(m->s, &n) ;
  if (n > maxfds || n + numfds > maxfds) return answer(c, ENFILE) ;
  c->dumping = n ;
  n = n ? 1 + (n-1) / UNIXMESSAGE_MAXFDS : 0 ;
  uint32_pack_big(pack+1, n) ;
  if (!unixmessage_put(&c->connection.out, &ans)) return answer(c, errno) ;
  return 1 ;
}

static int do_setdump_data (uint32_t cc, unixmessage_t const *m)
{
  client_t *c = CLIENT(cc) ;
  if (!m->nfds || m->nfds > c->dumping || m->len < m->nfds * (TAIN_PACK + 3))
    return (errno = EPROTO, 0) ;
  if (!(c->flags & 2))
  {
    unixmessage_drop(m) ;
    return answer(c, EPERM) ;
  }
  if (numfds + m->nfds > maxfds)
  {
    unixmessage_drop(m) ;
    return answer(c, ENFILE) ;
  }
  {
    char const *s = m->s ;
    size_t len = m->len ;
    uint32_t i = 0 ;
    uint32_t indices[m->nfds] ;
    for (; i < m->nfds ; i++)
    {
      s6_fdholder_fd_t *p ;
      uint32_t oldid, idlen ;
      if (len < TAIN_PACK + 3) break ;
      idlen = (unsigned char)s[TAIN_PACK] ;
      if (idlen > S6_FDHOLDER_ID_SIZE || len < TAIN_PACK + 2 + idlen || s[TAIN_PACK + 1 + idlen]) break ;
      indices[i] = genset_new(fdstore) ;
      p = FD(indices[i]) ;
      tain_unpack(s, &p->limit) ;
      memcpy(p->id, s + TAIN_PACK + 1, idlen+1) ;
      p->fd = m->fds[i] ;
      if (avltreen_search(fds_by_id, p->id, &oldid)) fds_close_and_delete(oldid) ;
      avltreen_insert(fds_by_id, indices[i]) ;
      avltreen_insert(fds_by_deadline, indices[i]) ;
      s += TAIN_PACK + 2 + idlen ; len -= TAIN_PACK + 2 + idlen ;
    }
    if (i < m->nfds || len)
    {
      while (i--) fds_delete(indices[i]) ;
      return (errno = EPROTO, 0) ;
    }
  }
  c->dumping -= m->nfds ;
  return answer(c, c->dumping ? EAGAIN : 0) ;
}

static int do_error (uint32_t cc, unixmessage_t const *m)
{
  (void)cc ;
  (void)m ;
  return (errno = EPROTO, 0) ;
}

typedef int parsefunc_t (uint32_t, unixmessage_t const *) ;
typedef parsefunc_t *parsefunc_t_ref ;

static inline int parse_protocol (unixmessage_t const *m, void *p)
{
  static parsefunc_t_ref const f[8] =
  {
    &do_store,
    &do_delete,
    &do_retrieve,
    &do_list,
    &do_getdump,
    &do_setdump,
    &do_setdump_data,
    &do_error
  } ;
  unixmessage_t mcopy = { .s = m->s + 1, .len = m->len - 1, .fds = m->fds, .nfds = m->nfds } ;
  if (!m->len)
  {
    unixmessage_drop(m) ;
    return (errno = EPROTO, 0) ;
  }
  if (!(*f[byte_chr("SDRL?!.", 7, m->s[0])])(*(uint32_t *)p, &mcopy))
  {
    unixmessage_drop(m) ;
    return 0 ;
  }
  return 1 ;
}

static inline int client_read (uint32_t cc, iopause_fd const *x)
{
  client_t *c = CLIENT(cc) ;
  return !unixmessage_receiver_isempty(&c->connection.in) || (c->xindex && (x[c->xindex].revents & IOPAUSE_READ)) ?
    unixmessage_handle(&c->connection.in, &parse_protocol, &cc) > 0 : 1 ;
}


 /* Environment on new connections */

static int makere (regex_t *re, char const *s, char const *var)
{
  size_t varlen = strlen(var) ;
  if (str_start(s, var) && (s[varlen] == '='))
  {
    int r = skalibs_regcomp(re, s + varlen + 1, REG_EXTENDED | REG_NOSUB) ;
    if (r)
    {
      if (verbosity)
      {
        char buf[256] ;
        regerror(r, re, buf, 256) ;
        strerr_warnw6x("invalid ", var, " value: ", s + varlen + 1, ": ", buf) ;
      }
      return -1 ;
    }
    else return 1 ;
  }
  return 0 ;
}

static void defaultre (regex_t *re)
{
  int r = skalibs_regcomp(re, ".^", REG_EXTENDED | REG_NOSUB) ;
  if (r)
  {
    char buf[256] ;
    regerror(r, re, buf, 256) ;
    strerr_diefu2x(100, "compile .^ into a regular expression: ", buf) ;
  }
}

static inline int parse_env (char const *const *envp, regex_t *rre, regex_t *wre, unsigned int *flags, unsigned int *donep)
{
  unsigned int done = 0 ;
  unsigned int fl = 0 ;
  for (; *envp ; envp++)
  {
    if (str_start(*envp, "S6_FDHOLDER_GETDUMP=")) fl |= 1 ;
    if (str_start(*envp, "S6_FDHOLDER_SETDUMP=")) fl |= 2 ;
    if (str_start(*envp, "S6_FDHOLDER_LIST=")) fl |= 4 ;
    if (!(done & 1))
    {
      int r = makere(rre, *envp, "S6_FDHOLDER_RETRIEVE_REGEX") ;
      if (r < 0)
      {
        if (done & 2) regfree(wre) ;
        return 0 ;
      }
      else if (r) done |= 1 ;
    }
    if (!(done & 2))
    {
      int r = makere(wre, *envp, "S6_FDHOLDER_STORE_REGEX") ;
      if (r < 0)
      {
        if (done & 1) regfree(rre) ;
        return 0 ;
      }
      else if (r) done |= 2 ;
    }
  }
  *flags = fl ;
  *donep = done ;
  return 1 ;
}

static inline int new_connection (int fd, regex_t *rre, regex_t *wre, unsigned int *flags)
{
  s6_accessrules_params_t params = S6_ACCESSRULES_PARAMS_ZERO ;
  s6_accessrules_result_t result = S6_ACCESSRULES_ERROR ;
  uid_t uid ;
  gid_t gid ;
  unsigned int done = 0 ;

  if (getpeereid(fd, &uid, &gid) < 0)
  {
    if (verbosity) strerr_warnwu1sys("getpeereid") ;
    return 0 ;
  }

  switch (rulestype)
  {
    case 1 :
      result = s6_accessrules_uidgid_fs(uid, gid, rules, &params) ; break ;
    case 2 :
      result = s6_accessrules_uidgid_cdb(uid, gid, &cdbmap, &params) ; break ;
    default : break ;
  }
  if (result != S6_ACCESSRULES_ALLOW)
  {
    if (verbosity && (result == S6_ACCESSRULES_ERROR))
       strerr_warnw1sys("error while checking rules") ;
    return 0 ;
  }
  if (params.exec.len && verbosity)
  {
    char fmtuid[UID_FMT] ;
    char fmtgid[GID_FMT] ;
    fmtuid[uid_fmt(fmtuid, uid)] = 0 ;
    fmtgid[gid_fmt(fmtgid, gid)] = 0 ;
    strerr_warnw4x("unused exec string in rules for uid ", fmtuid, " gid ", fmtgid) ;
  }
  if (params.env.s)
  {
    size_t n = byte_count(params.env.s, params.env.len, '\0') ;
    char const *envp[n+1] ;
    if (!env_make(envp, n, params.env.s, params.env.len))
    {
      if (verbosity) strerr_warnwu1sys("env_make") ;
      s6_accessrules_params_free(&params) ;
      return 0 ;
    }
    envp[n] = 0 ;
    if (!parse_env(envp, rre, wre, flags, &done))
    {
      if (verbosity) strerr_warnwu1sys("parse_env") ;
      s6_accessrules_params_free(&params) ;
      return 0 ;
    }
  }
  s6_accessrules_params_free(&params) ;
  if (!(done & 1)) defaultre(rre) ;
  if (!(done & 2)) defaultre(wre) ;
  return 1 ;
}


int main (int argc, char const *const *argv, char const *const *envp)
{
  int spfd ;
  int flag1 = 0 ;
  uint32_t maxconn = 16 ;
  PROG = "s6-fdholderd" ;

  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    unsigned int t = 0, T = 0 ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "v:1c:n:i:x:t:T:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'v' : if (!uint0_scan(l.arg, &verbosity)) dieusage() ; break ;
        case '1' : flag1 = 1 ; break ;
        case 'i' : rules = l.arg ; rulestype = 1 ; break ;
        case 'x' : rules = l.arg ; rulestype = 2 ; break ;
        case 't' : if (!uint0_scan(l.arg, &t)) dieusage() ; break ;
        case 'T' : if (!uint0_scan(l.arg, &T)) dieusage() ; break ;
        case 'c' : if (!uint0_scan(l.arg, &maxconn)) dieusage() ; break ;
        case 'n' : if (!uint0_scan(l.arg, &maxfds)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (t) tain_from_millisecs(&answertto, t) ;
    if (T) tain_from_millisecs(&lameduckdeadline, T) ;
  }
  if (!rulestype) strerr_dief1x(100, "no access rights specified!") ;
  if (maxconn > S6_FDHOLDER_MAX) maxconn = S6_FDHOLDER_MAX ;
  if (!maxconn) maxconn = 1 ;
  {
    struct rlimit fdlimit ;
    if (getrlimit(RLIMIT_NOFILE, &fdlimit) < 0)
      strerr_diefu1sys(111, "getrlimit") ;
    if (fdlimit.rlim_cur != RLIM_INFINITY)
    {
      if (fdlimit.rlim_cur < 7)
        strerr_dief1x(111, "open file limit too low") ;
      if (maxfds > fdlimit.rlim_cur) maxfds = fdlimit.rlim_cur - 6 ;
    }
  }
  if (!maxfds) maxfds = 1 ;
  {
    struct stat st ;
    if (fstat(0, &st) < 0) strerr_diefu1sys(111, "fstat stdin") ;
    if (!S_ISSOCK(st.st_mode)) strerr_dief1x(100, "stdin is not a socket") ;
  }
  if (flag1)
  {
    if (fcntl(1, F_GETFD) < 0)
      strerr_dief1sys(100, "called with option -1 but stdout said") ;
  }
  else close(1) ;
  spfd = selfpipe_init() ;
  if (spfd < 0) strerr_diefu1sys(111, "selfpipe_init") ;
  if (sig_ignore(SIGPIPE) < 0) strerr_diefu1sys(111, "ignore SIGPIPE") ;
  {
    sigset_t set ;
    sigemptyset(&set) ;
    sigaddset(&set, SIGTERM) ;
    sigaddset(&set, SIGHUP) ;
    if (selfpipe_trapset(&set) < 0) strerr_diefu1sys(111, "trap signals") ;
  }

  if (rulestype == 2)
  {
    cdbfd = open_readb(rules) ;
    if (cdbfd < 0) strerr_diefu3sys(111, "open ", rules, " for reading") ;
    if (cdb_init(&cdbmap, cdbfd) < 0)
      strerr_diefu2sys(111, "cdb_init ", rules) ;
  }

  {
     /* Hello, stack. I have a present for you. */
    genset clientinfo, fdinfo ;
    avltreen fdidinfo, fddeadlineinfo ;
    iopause_fd x[2 + maxconn] ;
    client_t clientstorage[1+maxconn] ;
    uint32_t clientfreelist[1+maxconn] ;
    s6_fdholder_fd_t fdstorage[maxfds] ;
    uint32_t fdfreelist[maxfds] ;
    avlnode fdidstorage[maxfds] ;
    uint32_t fdidfreelist[maxfds] ;
    avlnode fddeadlinestorage[maxfds] ;
    uint32_t fddeadlinefreelist[maxfds] ;
     /* Hope you enjoyed it! Have a nice day! */

    GENSET_init(&clientinfo, client_t, clientstorage, clientfreelist, 1+maxconn) ;
    clients = &clientinfo ;
    sentinel = genset_new(clients) ;
    clientstorage[sentinel].next = sentinel ;
    GENSET_init(&fdinfo, s6_fdholder_fd_t, fdstorage, fdfreelist, maxfds) ;
    fdstore = &fdinfo ;
    avltreen_init(&fdidinfo, fdidstorage, fdidfreelist, maxfds, &fds_id_dtok, &fds_id_cmp, 0) ;
    fds_by_id = &fdidinfo ;
    avltreen_init(&fddeadlineinfo, fddeadlinestorage, fddeadlinefreelist, maxfds, &fds_deadline_dtok, &fds_deadline_cmp, 0) ;
    fds_by_deadline = &fddeadlineinfo ;

    x[0].fd = spfd ; x[0].events = IOPAUSE_READ ;
    x[1].fd = 0 ;
      
    if (flag1)
    {
      fd_write(1, "\n", 1) ;
      fd_close(1) ;
    }

    /* We are long-lived and have to check absolute fd deadlines,
       so we purposefully remain in wallclock mode. */
    tain_now_g() ;

    for (;;)
    {
      tain_t deadline ;
      uint32_t j = 2 ;
      uint32_t i ;
      int r = 1 ;

      if (cont) tain_add_g(&deadline, &tain_infinite_relative) ;
      else deadline = lameduckdeadline ;
      if (avltreen_min(fds_by_deadline, &i) && tain_less(&FD(i)->limit, &deadline)) deadline = FD(i)->limit ;
      x[1].events = (cont && (numconn < maxconn)) ? IOPAUSE_READ : 0 ;
      for (i = clientstorage[sentinel].next ; i != sentinel ; i = clientstorage[i].next)
        if (client_prepare_iopause(i, &deadline, x, &j)) r = 0 ;
      if (!cont && r) break ;

      r = iopause_g(x, j, &deadline) ;
      if (r < 0) strerr_diefu1sys(111, "iopause") ;

      if (!r)
      {
        if (!cont && !tain_future(&lameduckdeadline)) break ;
        for (;;)
        {
          if (!avltreen_min(fds_by_deadline, &i)) break ;
          if (tain_future(&FD(i)->limit)) break ;
          fds_close_and_delete(i) ;
        }
        for (j = sentinel, i = clientstorage[sentinel].next ; i != sentinel ; j = i, i = clientstorage[i].next)
          if (!tain_future(&clientstorage[i].deadline)) removeclient(&i, j) ;
        continue ;
      }

      if (x[0].revents & IOPAUSE_READ) handle_signals() ;

      for (j = sentinel, i = clientstorage[sentinel].next ; i != sentinel ; j = i, i = clientstorage[i].next)
        if (!client_flush(i, x)) removeclient(&i, j) ;

      for (j = sentinel, i = clientstorage[sentinel].next ; i != sentinel ; j = i, i = clientstorage[i].next)
        if (!client_read(i, x)) removeclient(&i, j) ;

      if (x[1].revents & IOPAUSE_READ)
      {
        regex_t rre, wre ;
        unsigned int flags = 0 ;
        int dummy ;
        int fd = ipc_accept_nb(x[1].fd, 0, 0, &dummy) ;
        if (fd < 0)
          if (!error_isagain(errno)) strerr_diefu1sys(111, "accept") ;
          else continue ;
        else if (!new_connection(fd, &rre, &wre, &flags)) fd_close(fd) ;
        else client_add(&i, fd, &rre, &wre, flags) ;
      }
    }
    return ((!!numfds) | (!!numconn << 1)) ;
  }
}
