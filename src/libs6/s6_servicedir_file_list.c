/* ISC license. */

#include <s6/servicedir.h>

static s6_servicedir_desc const s6_servicedir_file_list_[] =
{
  { .name = "finish", .type = S6_FILETYPE_NORMAL, .options = S6_SVFILE_EXECUTABLE | S6_SVFILE_ATOMIC },
  { .name = "run", .type = S6_FILETYPE_NORMAL, .options = S6_SVFILE_EXECUTABLE | S6_SVFILE_MANDATORY | S6_SVFILE_ATOMIC },
  { .name = "notification-fd", .type = S6_FILETYPE_UINT, .options = 0 },
  { .name = "lock-fd", .type = S6_FILETYPE_UINT, .options = 0 },
  { .name = "timeout-kill", .type = S6_FILETYPE_UINT, .options = 0 },
  { .name = "timeout-finish", .type = S6_FILETYPE_UINT, .options = 0 },
  { .name = "max-death-tally", .type = S6_FILETYPE_UINT, .options = 0 },
  { .name = "down-signal", .type = S6_FILETYPE_NORMAL, .options = 0 },
  { .name = "template", .type = S6_FILETYPE_DIR, .options = 0 },
  { .name = "data", .type = S6_FILETYPE_DIR, .options = 0 },
  { .name = "env", .type = S6_FILETYPE_DIR, .options = 0 },
  { .name = 0, .options = 0 }
} ;

s6_servicedir_desc const *const s6_servicedir_file_list = s6_servicedir_file_list_ ;
