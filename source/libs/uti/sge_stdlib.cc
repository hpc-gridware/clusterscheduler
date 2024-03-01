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
#include <cstdlib>
#include <cstring>

#include "uti/msg_utilib.h"
#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

/****** uti/stdlib/sge_malloc() ***********************************************
*  NAME
*     sge_malloc() -- replacement for malloc() 
*
*  SYNOPSIS
*     char* sge_malloc(int size) 
*
*  FUNCTION
*     Allocates a memory block. Initilizes the block (0). Aborts in case
*     of error. 
*
*  INPUTS
*     int size - size in bytes 
*
*  RESULT
*     char* - pointer to memory block
*
*  NOTES
*     MT-NOTE: sge_malloc() is MT safe
******************************************************************************/
char *sge_malloc(size_t size) {
   DENTER_(BASIS_LAYER);

   if (!size) {
      DRETURN_(nullptr);
   }

   char *cp = (char *) malloc(size);
   if (!cp) {
      CRITICAL(SFNMAX, MSG_MEMORY_MALLOCFAILED);
      abort();
   }

   DRETURN_(cp);
}

/****** uti/stdlib/sge_realloc() **********************************************
*  NAME
*     sge_realloc() -- replacement for realloc 
*
*  SYNOPSIS
*     char* sge_realloc(char *ptr, int size, int abort) 
*
*  FUNCTION
*     Reallocates a memory block. Aborts in case of an error. 
*
*  INPUTS
*     char *ptr - pointer to a memory block
*     int size  - new size
*     int abort - do abort when realloc fails?
*
*  RESULT
*     char* - pointer to the (new) memory block
*
*  NOTES
*     MT-NOTE: sge_realloc() is MT safe
******************************************************************************/
void *sge_realloc(void *ptr, size_t size, int do_abort) {
   DENTER_(BASIS_LAYER);

   /* if new size is 0, just free the currently allocated memory */
   if (size == 0) {
      sge_free(&ptr);
      DRETURN_(nullptr);
   }

   void *cp = realloc(ptr, size);
   if (cp == nullptr) {
      CRITICAL(SFNMAX, MSG_MEMORY_REALLOCFAILED);
      if (do_abort) {
         abort();
      } else {
         sge_free(&ptr);
      }
   }

   DRETURN_(cp);
}

/****** uti/stdlib/sge_free() *************************************************
*  NAME
*     sge_free() -- replacement for free 
*
*  SYNOPSIS
*     void sge_free(char **cp) 
*
*  FUNCTION
*     Replacement for free function. Accepts nullptr pointers.
*
*  INPUTS
*     char **cp - pointer to a pointer of a memory block 
*
*  RESULT
*     char* - nullptr
*
*  NOTES
*     MT-NOTE: sge_free() is MT safe
******************************************************************************/
void sge_free(void *cp) {
   char **mem = (char **) cp;

   if (mem != nullptr && *mem != nullptr) {
      free(*mem);
      *mem = nullptr;
   }
}

/****** uti/stdlib/sge_getenv() ***********************************************
*  NAME
*     sge_getenv() -- get an environment variable 
*
*  SYNOPSIS
*     const char* sge_getenv(const char *env_str) 
*
*  FUNCTION
*     The function searches the environment list for a
*     string that matches the string pointed to by 'env_str'.
*
*  INPUTS
*     const char *env_str - name of env. varibale 
*
*  RESULT
*     const char* - value
*
*  SEE ALSO
*     uti/stdlib/sge_putenv()
*     uti/stdlib/sge_setenv() 
*
*  NOTES
*     MT-NOTE: sge_getenv() is MT safe
******************************************************************************/
const char *sge_getenv(const char *env_str) {
   DENTER_(BASIS_LAYER);
   const char *cp = (char *) getenv(env_str);
   DRETURN_(cp);
}

/****** uti/stdlib/sge_putenv() ***********************************************
*  NAME
*     sge_putenv() -- put an environment variable to environment
*
*  SYNOPSIS
*     static int sge_putenv(const char *var) 
*
*  FUNCTION
*     Duplicates the given environment variable and calls the system call
*     putenv.
*
*  INPUTS
*     const char *var - variable to put in the form <name>=<value>
*
*  RESULT
*     static int - 1 on success, else 0
*
*  SEE ALSO
*     uti/stdlib/sge_setenv() 
*     uti/stdlib/sge_getenv()
*
*  NOTES
*     MT-NOTE: sge_putenv() is MT safe
*******************************************************************************/
int sge_putenv(const char *var) {
   char *duplicate;

   if (var == nullptr) {
      return 0;
   }

   duplicate = strdup(var);

   if (duplicate == nullptr) {
      return 0;
   }

   if (putenv(duplicate) != 0) {
      return 0;
   }

   return 1;
}

/****** uti/stdlib/sge_setenv() ***********************************************
*  NAME
*     sge_setenv() -- Change or add an environment variable 
*
*  SYNOPSIS
*     int sge_setenv(const char *name, const char *value) 
*
*  FUNCTION
*     Change or add an environment variable 
*
*  INPUTS
*     const char *name  - variable name 
*     const char *value - new value 
*
*  RESULT
*     int - error state
*         1 - success
*         0 - error 
*
*  SEE ALSO
*     uti/stdlib/sge_putenv() 
*     uti/stdlib/sge_getenv()
*     uti/stdio/addenv()
*
*  NOTES
*     MT-NOTE: sge_setenv() is MT safe
*******************************************************************************/
int sge_setenv(const char *name, const char *value) {
   int ret = 0;

   if (name != nullptr && value != nullptr) {
      dstring variable = DSTRING_INIT;

      sge_dstring_sprintf(&variable, "%s=%s", name, value);
      ret = sge_putenv(sge_dstring_get_string(&variable));
      sge_dstring_free(&variable);
   }
   return ret;
}


/****** uti/stdlib/sge_unsetenv() *************************************************
*  NAME
*     sge_unsetenv() -- unset environment variable
*
*  SYNOPSIS
*     void sge_unsetenv(const char* varName) 
*
*  FUNCTION
*     Some architectures doesn't support unsetenv(), sge_unsetenv() is used
*     to unset an environment variable. 
*
*  INPUTS
*     const char* varName - name of envirionment variable
*
*  RESULT
*     void - no return value
*
*  NOTES
*     MT-NOTE: sge_unsetenv() is not MT safe 
*******************************************************************************/
void sge_unsetenv(const char *varName) {
#ifdef USE_SGE_UNSETENV
   extern char **environ;
   char* searchString = nullptr;

   if (varName != nullptr) {
      size_t length = (strlen(varName) + 2) * sizeof(char);
      searchString = malloc(length);
      if (searchString != nullptr) {
         bool found = false;
         int i;
         snprintf(searchString, length, "%s=", varName);
         
         /* At first we have to search the index of varName */
         for (i=0; i < ARG_MAX && environ[i] != nullptr; i++) {
            if (strstr(environ[i],searchString) != nullptr) {
               found = true;
               break;
            }
         }
        
         /* At second we remove varName by copying varName+1 to varName */ 
         if (found == true) {
            for (; i < ARG_MAX-1 && environ[i] != nullptr; i++) {
               environ[i] = environ[i+1];
            }
            environ[i] = nullptr;
         }
         sge_free(&searchString);
      }
   }
#else
   unsetenv(varName);
#endif
}



