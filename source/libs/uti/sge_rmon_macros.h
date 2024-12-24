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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstring>
#include <cstdarg>
#include <sys/types.h>

#include "uti/sge_rmon_monitoring_level.h"
#include "uti/sge_component.h"

#include "comm/cl_commlib.h"

#ifndef NO_SGE_COMPILE_DEBUG

#if defined(SOLARIS)
#  include <note.h>
#endif

#define DENTER_MAIN(layer, program) \
   static const char *SGE_FUNC = program; \
   static const int xaybzc = layer; \
   rmon_mopen(); \
   if (rmon_condition(xaybzc, TRACE)) { \
      const char *__thread_name = component_get_thread_name(); \
      int __thread_id = component_get_thread_id(); \
      if (__thread_name != nullptr) { \
         rmon_menter (SGE_FUNC, __thread_name, __thread_id); \
      } else { \
         rmon_menter (SGE_FUNC, nullptr, -1); \
      } \
   } \
   void()

#define DENTER(layer) \
   static const int xaybzc = layer; \
   if (rmon_condition(xaybzc, TRACE)) { \
      const char *__thread_name = component_get_thread_name(); \
      int __thread_id = component_get_thread_id(); \
      if (__thread_name != nullptr) { \
         rmon_menter (__func__, __thread_name, __thread_id); \
      } else { \
         rmon_menter (__func__, nullptr, -1); \
      } \
   } \
   void()

#define DENTER_(layer) \
   static const int xaybzc = layer; \
   if (rmon_condition(xaybzc, TRACE)) { \
      rmon_menter (__func__, nullptr, -1); \
   } \
   void()

#define DRETURN(ret) \
   if (rmon_condition(xaybzc, TRACE)) { \
      const char *__thread_name = component_get_thread_name(); \
      int __thread_id = component_get_thread_id(); \
      if (__thread_name != nullptr) { \
         rmon_mexit(__func__, __FILE__, __LINE__, __thread_name, __thread_id); \
      } else { \
         rmon_mexit(__func__, __FILE__, __LINE__, nullptr, -1); \
      } \
   } \
   return ret

#define DRETURN_(ret) \
   if (rmon_condition(xaybzc, TRACE)) { \
      rmon_mexit(__func__, __FILE__, __LINE__, nullptr, -1); \
   } \
   return ret

#define DRETURN_VOID \
   if (rmon_condition(xaybzc, TRACE)) { \
      const char *__thread_name = component_get_thread_name(); \
      int __thread_id = component_get_thread_id(); \
      if (__thread_name != nullptr) { \
         rmon_mexit(__func__, __FILE__, __LINE__, __thread_name, __thread_id); \
      } else { \
         rmon_mexit(__func__, __FILE__, __LINE__, nullptr, -1); \
      } \
   } \
   return

#define DRETURN_VOID_ \
   if (rmon_condition(xaybzc, TRACE)) { \
      rmon_mexit(__func__, __FILE__, __LINE__, nullptr, -1); \
   } \
   return

#define DTRACE \
   if (rmon_condition(xaybzc, TRACE)) { \
      const char *__thread_name = component_get_thread_name(); \
      int __thread_id = component_get_thread_id(); \
      if (__thread_name != nullptr) { \
         rmon_mtrace(__func__, __FILE__, __LINE__, __thread_name, __thread_id); \
      } else { \
         rmon_mtrace(__func__, __FILE__, __LINE__, nullptr, -1); \
      } \
   } \
   void()

#define DTRACE_                                                                  \
   if (rmon_condition(xaybzc, TRACE)) {                                          \
      rmon_mtrace(__func__, __FILE__, __LINE__, nullptr, -1);                    \
   }

#define DPRINTF_IS_ACTIVE rmon_condition(xaybzc, INFOPRINT)

#define DPRINTF(...) \
   if (rmon_condition(xaybzc, INFOPRINT)) { \
      rmon_helper_t *helper = rmon_get_helper(); \
      if (helper != nullptr) { \
         const char *__thread_name = component_get_thread_name(); \
         int __thread_id = component_get_thread_id(); \
         if (__thread_name != nullptr) { \
            strcpy(helper->thread_name, __thread_name); \
            helper->thread_id = __thread_id; \
         } \
      } \
      rmon_mprintf(__VA_ARGS__); \
      if (helper != nullptr) { \
         helper->thread_name[0] = '\0'; \
         helper->thread_id = -1; \
      } \
   } \
   void()

#define DPRINTF_(...) \
   if (rmon_condition(xaybzc, INFOPRINT)) { \
      rmon_mprintf(__VA_ARGS__); \
   }

#define ISTRACE (rmon_condition(xaybzc, TRACE))

#define TRACEON  (rmon_is_enabled() && !rmon_mliszero(&RMON_DEBUG_ON))

#else /* NO_SGE_COMPILE_DEBUG */

#define DENTER_MAIN( layer, program )
#define DENTER(layer)
#define DRETURN(x) return x
#define DRETURN_VOID return
#define DTRACE
#define DPRINTF(...)
#define DPRINTF(...)
#define DTIMEPRINTF(x)
#define DSPECIALPRINTF(x)
#define TRACEON
#define ISTRACE

#endif /* NO_SGE_COMPILE_DEBUG */

extern monitoring_level RMON_DEBUG_ON;

int rmon_condition(int layer, int debug_class);

int rmon_is_enabled();
void rmon_disable();
void rmon_enable();

void rmon_mopen();

void rmon_menter(const char *func, const char *thread_name, int thread_id);

void rmon_mtrace(const char *func, const char *file, int line, const char *thread_name, int thread_id);

void rmon_mexit(const char *func, const char *file, int line, const char *thread_name, int thread_id);

void rmon_mprintf(const char *fmt, ...);

struct rmon_helper_t {
    char thread_name[32];
    int thread_id;
};

rmon_helper_t *rmon_get_helper();
