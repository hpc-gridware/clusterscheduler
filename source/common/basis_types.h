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
/* Portions of this code are Copyright (c) 2011 Univa Corporation. */
/*___INFO__MARK_END__*/

#include <sys/types.h>

#ifdef __SGE_COMPILE_WITH_GETTEXT__
#  include <libintl.h>
#  include <locale.h>
#  include "uti/sge_language.h"
#  define SGE_ADD_MSG_ID(x) (sge_set_message_id_output(1),(x),sge_set_message_id_output(0))
#  define _(x)               sge_gettext(x)
#  define _MESSAGE(x,y)      sge_gettext_((x),(y))
#  define _SGE_GETTEXT__(x)  sge_gettext__(x)
#else
#  define SGE_ADD_MSG_ID(x) (x)
#  define _(x)              (x)
#  define _MESSAGE(x,y)     (y)
#  define _SGE_GETTEXT__(x) (x)
#endif

#if !defined(__cplusplus) 
#include <stdbool.h>
#endif

#define SGE_EPSILON 0.00001

#define FALSE_STR "FALSE"
#define TRUE_STR  "TRUE"

#define NONE_STR  "NONE"

#if defined(TARGET_64BIT)
#  define sge_U32CFormat "%u"  
#  define sge_U32CLetter "u"
#  define sge_u32c(x)  (unsigned int)(x)

#  define sge_X32CFormat "%x"
#  define sge_x32c(x)  (unsigned int)(x)

#define SGE_STRTOU_LONG32(S) strtoul(S, nullptr, 10)
#else
#  define sge_U32CFormat "%ld"
#  define sge_U32CLetter "ld"
#  define sge_u32c(x)  (unsigned long)(x)

#  define sge_X32CFormat "%lx"
#  define sge_x32c(x)  (unsigned long)(x)

#define SGE_STRTOU_LONG32(S) strtoul(S, nullptr, 10)
#endif

#include <sys/param.h>

#if defined(TARGET_64BIT)
#  define u_long32 u_int
#elif defined(FREEBSD) || defined(NETBSD)
#  define u_long32 uint32_t
#else
#  define u_long32 u_long
#endif

#if defined(TARGET_64BIT)
#  define u_long64 u_long
#elif defined(FREEBSD) || defined(NETBSD)
#  define u_long64 uint64_t
#else
#  define u_long64 unsigned long long
#endif

#define U_LONG32_MAX 4294967295UL
#define LONG32_MAX   2147483647

/* set sge_u32 and sge_x32 for 64 or 32 bit machines */
/* sge_uu32 for strictly unsigned, not nice, but did I use %d for an unsigned? */
#if defined(TARGET_64BIT) || defined(FREEBSD) || defined(NETBSD)
#  define sge_u64    "%lu"
#  define sge_u32    "%d"
#  define sge_uu32   "%u"
#  define sge_x32    "%x"
#  define sge_fu32   "d"
#else
#  define sge_u64    "%llu"
#  define sge_u32    "%ld"
#  define sge_uu32   "%lu"
#  define sge_x32    "%lx"
#  define sge_fu32   "ld"
#endif

#define uid_t_fmt "%u"
#define gid_t_fmt "%u"
#define pid_t_fmt "%d"

/* _POSIX_PATH_MAX is only 255 and this is less than in most real systmes */
#define SGE_PATH_MAX    1024

#define MAX_STRING_SIZE 2048
typedef char stringT[MAX_STRING_SIZE];

#define MAX_VERIFY_STRING 512

#define INTSIZE     4           /* (4) 8 bit bytes */
#define INTOFF      0           /* the rest of the world; see comments in request.c */

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) ((a)<(b)?(b):(a))
#endif

/* types */
/* these are used for complexes */
#define TYPE_INT          1
#define TYPE_FIRST        TYPE_INT
#define TYPE_STR          2
#define TYPE_TIM          3
#define TYPE_MEM          4
#define TYPE_BOO          5
#define TYPE_CSTR         6
#define TYPE_HOST         7
#define TYPE_DOUBLE       8
#define TYPE_RESTR        9
#define TYPE_CE_LAST      TYPE_RESTR

/* used in config */
#define TYPE_ACC          10 
#define TYPE_LOG          11
#define TYPE_LAST         TYPE_LOG

/* save string format quoted */
#define SFQ  "\"%-.100s\""
/* save string format non-quoted */
#define SFN  "%-.100s"
#define SFN2 "%-.200s"
#define SFNMAX "%-.2047s"  /* write to buffer of size MAX_STRING_SIZE */
#define PFNMAX "%-.1023s"  /* write to buffer of size SGE_PATH_MAX */
/* non-quoted string not limited intentionally */
#define SN_UNLIMITED  "%s"

#ifndef TRUE
#  define TRUE 1
#  define FALSE !TRUE
#endif

#define GET_SPECIFIC(type, variable, init_func, key) \
   auto variable = (type *)pthread_getspecific((pthread_key_t)key); \
   if (variable == nullptr) { \
      variable = (type *)malloc(sizeof(type)); \
      init_func(variable); \
      int ret = pthread_setspecific(key, (void*)variable); \
      if (ret != 0) { \
         fprintf(stderr, "pthread_setspecific(%s) failed: %s\n", __func__, strerror(ret)); \
         abort(); \
      } \
   } \
   void()

#define HAS_GETPWNAM_R
#define HAS_GETGRNAM_R
#define HAS_GETPWUID_R
#define HAS_GETGRGID_R

#define HAS_LOCALTIME_R
#define HAS_CTIME_R

typedef enum {
   NO    = 0,
   YES   = 1,
   UNSET = 2
} ternary_t;
