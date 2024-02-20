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

#include <cstring>
#include <cstdlib>
#include <cstdio>

#include "uti/msg_utilib.h"
#include "uti/sge_arch.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"

#include "sge.h"

#include "msg_common.h"

/****** uti/prog/sge_get_arch() ************************************************
*  NAME
*     sge_get_arch() -- SGE/EE architecture string
*
*  SYNOPSIS
*     const char* sge_get_arch(void) 
*
*  FUNCTION
*     This function returns the SGE/EE architecture string of that
*     host where the application is running which called this 
*     functionon.
*
*  NOTES:
*     MT-NOTE: sge_get_arch() is MT safe
*
*  RESULT
*     const char* - architecture string
******************************************************************************/
const char *sge_get_arch() {
#ifndef SGE_ARCH_STRING
#   pragma "Define an architecture for SGE"
#endif

   return (SGE_ARCH_STRING);
}

/****** uti/prog/sge_get_root_dir() *******************************************
*  NAME
*     sge_get_root_dir() -- SGE/SGEEE installation directory 
*
*  SYNOPSIS
*     const char* sge_get_root_dir(int do_exit, 
*                                  char *buffer, 
*                                  size_t size,
*                                  int do_error_log ) 
*
*  FUNCTION
*     This function returns the installation directory of SGE/SGEEE.
*     This directory is defined by the SGE_ROOT environment variable 
*     of the calling process. 
*     If the environment variable does not exist or is not set then
*     this function will handle this as error and return nullptr
*     (do_exit = 0). If 'do_exit' is 1 and an error occures, the 
*     function will terminate the  calling application.
*
*  INPUTS
*     int do_exit - Terminate the application in case of an error
*     char *buffer - buffer to be used for error message
*     size_t size - size of buffer
*     int do_error_log - enable/disable error logging
*
*  RESULT
*     const char* - Root directory of the SGE/SGEEE installation
*
*  NOTES
*     MT-NOTE: sge_get_arch() is MT safe
*******************************************************************************/
const char *sge_get_root_dir(int do_exit, char *buffer, size_t size, int do_error_log) {
   char *sge_root;
   char *s;

   DENTER_(TOP_LAYER);

   /*
    * Read some env variables
    */
   sge_root = getenv("SGE_ROOT");

   /*
    * Check the env variables
    */
   if (sge_root) {
      s = sge_root;
   } else {
      goto error;
   }
   if (!s || strlen(s) == 0) {
      goto error;
   } else {
      /*
       * Get rid of trailing slash
       */
      if (s[strlen(s) - 1] == '/') {
         s[strlen(s) - 1] = '\0';
      }
   }
   DRETURN_(s);

   error:
   if (do_error_log) {
      if (buffer != nullptr) {
         sge_strlcpy(buffer, MSG_SGEROOTNOTSET, size);
      } else {
         CRITICAL((SGE_EVENT, SFNMAX, MSG_SGEROOTNOTSET));
      }
   }

   if (do_exit) {
      sge_exit(1);
   }
   return nullptr;
}

/****** uti/prog/sge_get_lib_dir() ********************************************
*  NAME
*     sge_get_lib_dir() -- Path to SGE libraries
*
*  SYNOPSIS
*     int sge_get_lib_dir(char *buffer, size_t size)
*
*  FUNCTION
*     This function stores the path to the SGE libraries in the buffer
*
*  INPUTS
*     char     *buffer - Buffer where to store the SGE library dir
*     size_t   size    - Size of the given buffer
*  NOTES:
*     MT-NOTE: sge_get_lib_dir() is MT safe
*
*  RESULT
*     int - Return code
*       0 - Success
*      -1 - Given buffer is nullptr
*      -2 - SGE_ROOT cannot be obtained
*      -3 - given buffer is to small
******************************************************************************/
int sge_get_lib_dir(char *buffer, size_t size) {
   if (buffer == nullptr) {
      return -1;
   }

   buffer[0] = '\0';

   /* obtain SGE_ROOT
    * error-logging ist done by sge_get_root_dir()
    */
   const char *sge_root = sge_get_root_dir(0, nullptr, 0, 1);
   if (sge_root == nullptr) {
      return -2;
   }

   /* obtain SGE_ARCH */
   const char *sge_arch = sge_get_arch();

   /* check if given lib_dir-buffer is big enough
    * sge_root + sge_arch + /lib/ + \0
    */
   if (sge_strlen(sge_root) + sge_strlen(sge_arch) + 6 > size) {
      return -3;
   }

   /* Build lib-dir path */
   sge_strlcat(buffer, sge_root, size);
   sge_strlcat(buffer, "/lib/", size);
   sge_strlcat(buffer, sge_arch, size);

   return 0;
}

/****** uti/prog/sge_get_default_cell() ***************************************
*  NAME
*     sge_get_default_cell() -- get cell name and remove trailing slash 
*
*  SYNOPSIS
*     const char* sge_get_default_cell(void) 
*
*  FUNCTION
*     This function returns the defined cell name of SGE/SGEEE.
*     This directory is defined by the SGE_CELL environment variable
*     of the calling process.
*     If the environment variable does not exist or is not set then
*     this function will return the 'DEFAULT_CELL'.
*
*  RESULT
*     const char* - Cell name of this SGE/SGEEE installation
*
*  NOTES
*     MT-NOTE: sge_get_default_cell() is MT safe
******************************************************************************/
const char *sge_get_default_cell() {
   char *sge_cell;
   char *s;

   DENTER_(TOP_LAYER);
   /*
    * Read some env variables
    */
   sge_cell = getenv("SGE_CELL");

   /*
    * Check the env variables
    */
   if (sge_cell) {
      s = sge_cell;
   } else {
      s = nullptr;
   }

   /*
    * Use default? 
    */
   if (!s || strlen(s) == 0) {
      s = DEFAULT_CELL;
   } else {
      /*
       * Get rid of trailing slash
       */
      if (s[strlen(s) - 1] == '/') {
         s[strlen(s) - 1] = '\0';
      }
   }
   DRETURN_(s);
}

/****** uti/prog/sge_get_alias_path() *****************************************
*  NAME
*     sge_get_alias_path() -- Return the path of the 'alias_file' 
*
*  SYNOPSIS
*     const char* sge_get_alias_path(void) 
*
*  FUNCTION
*     Return the path of the 'alias_file' 
*
*  NOTES
*     MT-NOTE: sge_get_alias_path() is MT safe
*
******************************************************************************/
const char *sge_get_alias_path() {
   const char *sge_root, *sge_cell;
   char *cp;
   int len;
   SGE_STRUCT_STAT sbuf;

   DENTER_(TOP_LAYER);

   sge_root = sge_get_root_dir(1, nullptr, 0, 1);
   sge_cell = sge_get_default_cell();

   if (SGE_STAT(sge_root, &sbuf)) {
      CRITICAL((SGE_EVENT, MSG_SGETEXT_SGEROOTNOTFOUND_S, sge_root));
      sge_exit(1);
   }

   len = strlen(sge_root) + strlen(sge_cell) + strlen(COMMON_DIR) + strlen(ALIAS_FILE) + 5;
   if (!(cp = sge_malloc(len))) {
      CRITICAL((SGE_EVENT, SFNMAX, MSG_MEMORY_MALLOCFAILEDFORPATHTOHOSTALIASFILE));
      sge_exit(1);
   }

   sprintf(cp, "%s/%s/%s/%s", sge_root, sge_cell, COMMON_DIR, ALIAS_FILE);
   DRETURN_(cp);
}
