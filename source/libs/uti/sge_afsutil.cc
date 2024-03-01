/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 * 
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 * 
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 * 
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

#include "basis_types.h"

#include "uti/msg_utilib.h"
#include "uti/sge_afsutil.h"
#include "uti/sge_io.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_unistd.h"

/****** uti/afsutil/sge_read_token() ******************************************
*  NAME
*     sge_read_token() -- read token from file 
*
*  SYNOPSIS
*     char* sge_read_token(const char *file) 
*
*  FUNCTION
*     Read token from file, malloc buffer, add '\0' byte and
*     return pointer to buffer.
*
*  INPUTS
*     const char *file - filename 
*
*  NOTES
*     MT-NOTE: sge_read_token() is MT safe
*
*  RESULT
*     char* - pointer to a malloced buffer or 
*             nullptr if error occured
******************************************************************************/
char *sge_read_token(const char *file) {
   SGE_STRUCT_STAT sb;
   int fd;
   char *tokenbuf;
   size_t size;

   DENTER(TOP_LAYER);

   if (SGE_STAT(file, &sb)) {
      DRETURN(nullptr);
   }

   size = sb.st_size + 1;
   if (((SGE_OFF_T) size != sb.st_size + 1)
       || (tokenbuf = sge_malloc(size)) == nullptr) {
      DRETURN(nullptr);
   }

   if ((fd = SGE_OPEN2(file, O_RDONLY)) == -1) {
      DRETURN(nullptr);
   }

   if (read(fd, tokenbuf, sb.st_size) != sb.st_size) {
      close(fd);
      DRETURN(nullptr);
   }

   tokenbuf[sb.st_size] = '\0';

   close(fd);
   DRETURN(tokenbuf);
}

/****** uti/afsutil/sge_afs_extend_token() ************************************
*  NAME
*     sge_afs_extend_token() -- Extend an AFS token
*
*  SYNOPSIS
*     int sge_afs_extend_token(const char *command, char *tokenbuf, 
*                              const char *user, int token_extend_time, 
*                              char *err_str) 
*
*  FUNCTION
*     Call 'command', pipe content of 'tokenbuf' to 'command', using
*     'user' as arg1 and 'token_extend_time' as arg2 
*
*  INPUTS
*     const char *command   - command 
*     char *tokenbuf        - input for command 
*     const char *user      - 1st argument for command 
*     int token_extend_time - 2nd argument for command 
*     char *err_str         - error message 
*
*  NOTES
*     MT-NOTE: sge_afs_extend_token() is not MT safe because it uses MT unsafe 
*     MT-NOTE: sge_peopen()
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error 
******************************************************************************/
int sge_afs_extend_token(const char *command, char *tokenbuf, const char *user,
                         int token_extend_time, char *err_str, size_t err_str_size) {
   pid_t command_pid;
   FILE *fp_in, *fp_out, *fp_err;
   int ret;
   char cmdbuf[SGE_PATH_MAX + 128];

   DENTER(TOP_LAYER);

   snprintf(cmdbuf, sizeof(cmdbuf), "%s %s %d", command, user, token_extend_time);
   if (err_str) {
      strcpy(err_str, cmdbuf);
   }

   command_pid = sge_peopen("/bin/sh", 0, cmdbuf, nullptr, nullptr, &fp_in, &fp_out, &fp_err, false);
   if (command_pid == -1) {
      if (err_str) {
         snprintf(err_str, err_str_size, MSG_TOKEN_NOSTART_S, cmdbuf);
      }
      DRETURN(-1);
   }
   if (sge_string2bin(fp_in, tokenbuf) == -1) {
      if (err_str) {
         snprintf(err_str, err_str_size, MSG_TOKEN_NOWRITEAFS_S, cmdbuf);
      }
      DRETURN(-1);
   }

   if ((ret = sge_peclose(command_pid, fp_in, fp_out, fp_err, nullptr)) != 0) {
      if (err_str) {
         snprintf(err_str, err_str_size, MSG_TOKEN_NOSETAFS_SI, cmdbuf, ret);
      }
      DRETURN(-1);
   }

   return 0;
}

/****** uti/afsutil/sge_get_token_cmd() ***************************************
*  NAME
*     sge_get_token_cmd() -- Check if 'tokencmdname' is executable 
*
*  SYNOPSIS
*     int sge_get_token_cmd(const char *tokencmdname, char *buf) 
*
*  FUNCTION
*     Check if 'tokencmdname' exists and is executable. If an error
*     occures write a error message into 'buf' if not nullptr.
*     Otherwise write error messages to stderr.
*
*  INPUTS
*     const char *tokencmdname - command 
*     char *buf                - nullptr or buffer for error message
*
*  NOTES
*     MT-NOTE: sge_get_token_cmd() is MT safe 
*
*  RESULT
*     int - error state
*         0 - OK
*         1 - Error
******************************************************************************/
int sge_get_token_cmd(const char *tokencmdname, char *buf, size_t buf_size) {
   SGE_STRUCT_STAT sb{};

   if (!tokencmdname || !strlen(tokencmdname)) {
      if (!buf) {
         fprintf(stderr, "%s\n", MSG_COMMAND_NOPATHFORTOKEN);
      } else {
         strcpy(buf, MSG_COMMAND_NOPATHFORTOKEN);
      }
      return 1;
   }

   if (SGE_STAT(tokencmdname, &sb) == -1) {
      if (!buf) {
         fprintf(stderr, MSG_COMMAND_NOFILESTATUS_S, tokencmdname);
         fprintf(stderr, "\n");
      } else {
         snprintf(buf, buf_size, MSG_COMMAND_NOFILESTATUS_S, tokencmdname);
      }
      return 1;
   }

   if (!(sb.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
      if (!buf) {
         fprintf(stderr, MSG_COMMAND_NOTEXECUTABLE_S, tokencmdname);
         fprintf(stderr, "\n");
      } else {
         snprintf(buf, buf_size, MSG_COMMAND_NOTEXECUTABLE_S, tokencmdname);
      }
      return 1;
   }

   return 0;
}
