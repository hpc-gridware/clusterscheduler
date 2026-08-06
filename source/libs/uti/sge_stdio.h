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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief stdio helpers, including popen/pclose with process control
 */

#include <cstdio>
#include <sys/time.h>
#include <sys/types.h>
#include <ctime>
#include <unistd.h>

#include <cinttypes>

/* On some systems, FOPEN is already defined as value -1 */
#undef FOPEN

/// open a file, jumping to the function's error label when it fails
#define FOPEN(var, fname, fmode) \
   if((var = fopen(fname,fmode)) == nullptr) { \
      goto FOPEN_ERROR; \
   }

/**
 * @brief Fprintf() macro
 *
 * This FPRINTF macro has to be used similar to the fprintf
 * function. It is not necessary to check the return value.
 * In case of an error the macro will jump to a defined label.
 * The label name is 'FPRINTF_ERROR'.
 *
 * @param ... the arguments of the wrapped stdio call
 *
 * @note Don't forget to define the 'FPRINTF_ERROR'-label
 */
#define FPRINTF(x) \
   if (fprintf x < 0) { \
      goto FPRINTF_ERROR; \
   } \
   void()

/**
 * @brief Fprintf() macro with return value assignment
 *
 * This FPRINTF macro has to be used similar to the fprintf
 * function. It is not necessary to check the return value.
 * In case of an error the macro will jump to a defined label.
 * The label name is 'FPRINTF_ERROR'. This is a variarion of
 * FPRINTF() that allows assigning the fprintf() return value to
 * the variable passed as first makro argument.
 *
 * @param ... the arguments of the wrapped stdio call
 *
 * @note Don't forget to define the 'FPRINTF_ERROR'-label
 */
#define FPRINTF_ASSIGN(var, x) \
   if ((var = fprintf x) < 0) { \
      goto FPRINTF_ERROR; \
   }

/**
 * @brief Fclose() macro
 *
 * This FCLOSE macro has to be used similar to the fclose
 * function. It is not necessary to check the return value.
 * In case of an error the macro will jump to a defined label.
 * The label name is 'FCLOSE_ERROR'.
 *
 * @param ... the arguments of the wrapped stdio call
 *
 * @note Don't forget to define the 'FCLOSE_ERROR'-label
 */
/// close a file, jumping to the function's error label when it fails
#define FCLOSE(x) \
   if (x != nullptr) { \
      if (fclose(x) != 0) { \
         goto FCLOSE_ERROR; \
      } \
   } \
   void()

/// close a file, ignoring a close error
#define FCLOSE_IGNORE_ERROR(x) fclose(x)

pid_t sge_peopen(const char *shell, int login_shell, const char *command,
                 const char *user, char **env, FILE **fp_in, FILE **fp_out,
                 FILE **fp_err, bool null_stderr);

int sge_peclose(pid_t pid, FILE *fp_in, FILE *fp_out, FILE *fp_err,
                struct timeval *timeout);

void print_option_syntax(FILE *fp, const char *option, const char *meaning);

bool sge_check_stdout_stream(FILE *file, int fd);

pid_t sge_peopen_r(const char *shell, int login_shell, const char *command,
                   const char *user, char **env, FILE **fp_in, FILE **fp_out,
                   FILE **fp_err, bool null_stderr);

/** @def SGE_DEFAULT_PATH
 * @brief PATH given to a child process when none is inherited
 */
#if defined(SOLARIS)
/// PATH used when a child process is started without inheriting one
#define SGE_DEFAULT_PATH "/usr/local/bin:/bin:/usr/bin:/usr/ucb"
#else
#define SGE_DEFAULT_PATH "/usr/local/bin:/bin:/usr/bin"
#endif
