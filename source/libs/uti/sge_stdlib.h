#pragma once
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

#include <cstdlib>

/****** uti/stdlib/FREE() *****************************************************
*  NAME
*     FREE() -- replacement for sge_free()
*
*  SYNOPSIS
*     #define FREE(x)
*     void FREE(char *cp) 
*
*  FUNCTION
*     Replacement for sge_free(). Accepts nullptr pointers.
*     After a call of this macro "cp" will be nullptr.
*
*  INPUTS
*     char *cp - pointer to a memory block 
*
*  RESULT
*     char* - nullptr
*
*  SEE ALSO
*     uti/stdlib/sge_free()
******************************************************************************/
#define FREE(x) \
   if (x != nullptr) { \
      free((char *)x); \
      x = nullptr; \
   }

char *sge_malloc(size_t size);

void *sge_realloc(void *ptr, size_t size, int do_abort);

void sge_free(void *cp);

const char *sge_getenv(const char *env_str);

int sge_putenv(const char *var);

int sge_setenv(const char *name, const char *value);

void sge_unsetenv(const char *name);
