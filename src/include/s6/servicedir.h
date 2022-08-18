/* ISC license. */

#ifndef S6_SERVICEDIR_H
#define S6_SERVICEDIR_H

#include <stdint.h>

#define S6_SERVICEDIR_FILE_MAXLEN 16

#define S6_FILETYPE_NORMAL 0
#define S6_FILETYPE_EMPTY 1
#define S6_FILETYPE_UINT 2
#define S6_FILETYPE_DIR 3

#define S6_SVFILE_EXECUTABLE 0x01
#define S6_SVFILE_MANDATORY 0x02
#define S6_SVFILE_ATOMIC 0x04

typedef struct s6_servicedir_desc_s s6_servicedir_desc, *s6_servicedir_desc_ref ;
struct s6_servicedir_desc_s
{
  char const *name ;
  uint8_t type : 3 ;
  uint8_t options : 5 ;
} ;

extern s6_servicedir_desc const *const s6_servicedir_file_list ;

#endif
