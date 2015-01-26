 /* ISC license. */

#ifndef S6_FDHOLDER_H
#define S6_FDHOLDER_H

#include <skalibs/tai.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/unixmessage.h>
#include <skalibs/unixconnection.h>

#define S6_FDHOLDER_ID_SIZE 255
#define S6_FDHOLDER_MAX 256

typedef struct s6_fdholder_s s6_fdholder_t, *s6_fdholder_t_ref ;
struct s6_fdholder_s
{
  unixconnection_t connection ;
} ;
#define S6_FDHOLDER_ZERO { .connection = UNIXCONNECTION_ZERO } ;

#define s6_fdholder_init(a, fd) unixconnection_init(&(a)->connection, fd, fd)
#define s6_fdholder_free(a) unixconnection_free(&(a)->connection)


 /* Individual fds */

extern int s6_fdholder_store_async (s6_fdholder_t *, int, char const *, tain_t const *) ;
extern int s6_fdholder_store (s6_fdholder_t *, int, char const *, tain_t const *, tain_t const *, tain_t *) ;
#define s6_fdholder_store_g(a, fd, id, limit, deadline) s6_fdholder_store(a, fd, id, limit, (deadline), &STAMP)

extern int s6_fdholder_delete_async (s6_fdholder_t *, char const *) ;
extern int s6_fdholder_delete (s6_fdholder_t *, char const *, tain_t const *, tain_t *) ;
#define s6_fdholder_delete_g(a, id, deadline) s6_fdholder_delete(a, id, (deadline), &STAMP)

typedef struct s6_fdholder_retrieve_result_s s6_fdholder_retrieve_result_t, *s6_fdholder_retrieve_result_t_ref ;
struct s6_fdholder_retrieve_result_s
{
  int fd ;
  unsigned char err ;
} ;

extern int s6_fdholder_retrieve_maybe_delete_async (s6_fdholder_t *, char const *, int) ;
extern unixmessage_handler_func_t s6_fdholder_retrieve_cb ;
extern int s6_fdholder_retrieve_maybe_delete (s6_fdholder_t *, char const *, int, tain_t const *, tain_t *) ;
#define s6_fdholder_retrieve_maybe_delete_g(a, id, h, deadline) s6_fdholder_retrieve_maybe_delete(a, id, h, (deadline), &STAMP)
#define s6_fdholder_retrieve(a, id, deadline, stamp) s6_fdholder_retrieve_maybe_delete(a, id, 0, deadline, stamp)
#define s6_fdholder_retrieve_g(a, id, deadline) s6_fdholder_retrieve(a, id, (deadline), &STAMP)
#define s6_fdholder_retrieve_delete(a, id, deadline, stamp) s6_fdholder_retrieve_maybe_delete(a, id, 1, deadline, stamp)
#define s6_fdholder_retrieve_delete_g(a, id, deadline) s6_fdholder_retrieve(a, id, (deadline), &STAMP)

typedef struct s6_fdholder_list_result_s s6_fdholder_list_result_t, *s6_fdholder_list_result_t_ref ;
struct s6_fdholder_list_result_s
{
  stralloc *sa ;
  unsigned int n ;
  unsigned char err ;
} ;

extern int s6_fdholder_list_async (s6_fdholder_t *) ;
extern unixmessage_handler_func_t s6_fdholder_list_cb ;
extern int s6_fdholder_list (s6_fdholder_t *, stralloc *, tain_t const *, tain_t *) ;
#define s6_fdholder_list_g(a, sa, deadline) s6_fdholder_list(a, sa, (deadline), &STAMP)


 /* Dumps */

typedef struct s6_fdholder_fd_s s6_fdholder_fd_t, *s6_fdholder_fd_t_ref ;
struct s6_fdholder_fd_s
{
  char id[S6_FDHOLDER_ID_SIZE + 1] ;
  int fd ;
  tain_t limit ;
} ;

extern int s6_fdholder_getdump (s6_fdholder_t *, genalloc *, tain_t const *, tain_t *) ;
#define s6_fdholder_getdump_g(a, g, deadline) s6_fdholder_getdump(a, g, (deadline), &STAMP)
extern int s6_fdholder_setdump (s6_fdholder_t *, s6_fdholder_fd_t const *, unsigned int, tain_t const *, tain_t *) ;
#define s6_fdholder_setdump_g(a, list, n, deadline) s6_fdholder_setdump(a, list, n, (deadline), &STAMP)

#endif
