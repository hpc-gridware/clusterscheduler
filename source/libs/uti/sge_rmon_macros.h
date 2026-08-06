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
 * @brief RMON debug tracing macros: DENTER, DRETURN, DPRINTF
 */

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

/** @brief Open monitoring and trace entry into a program's main function
 *
 * @param layer debug layer this translation unit belongs to
 * @param program name used in the trace output
 */
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

/** @brief Trace entry into a function
 *
 * Must be the first statement of the function, and must be paired with
 * #DRETURN or #DRETURN_VOID on every return path.
 *
 * @param layer debug layer this translation unit belongs to
 */
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

/** @brief As #DENTER, but without opening a new trace level
 *
 * @param layer debug layer this translation unit belongs to
 */
#define DENTER_(layer) \
   static const int xaybzc = layer; \
   if (rmon_condition(xaybzc, TRACE)) { \
      rmon_menter (__func__, nullptr, -1); \
   } \
   void()

/** @brief Trace the exit of a function and return @p ret
 *
 * @param ret the value to return
 */
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

/** @brief As #DRETURN, but without closing a trace level
 *
 * @param ret the value to return
 */
#define DRETURN_(ret) \
   if (rmon_condition(xaybzc, TRACE)) { \
      rmon_mexit(__func__, __FILE__, __LINE__, nullptr, -1); \
   } \
   return ret

/** @brief Trace the exit of a void function and return
 */
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

/** @brief As #DRETURN_VOID, but without closing a trace level
 */
#define DRETURN_VOID_ \
   if (rmon_condition(xaybzc, TRACE)) { \
      rmon_mexit(__func__, __FILE__, __LINE__, nullptr, -1); \
   } \
   return

/** @brief Trace that execution reached this point
 */
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

/** @brief As #DTRACE, but without opening a trace level
 */
#define DTRACE_                                                                  \
   if (rmon_condition(xaybzc, TRACE)) {                                          \
      rmon_mtrace(__func__, __FILE__, __LINE__, nullptr, -1);                    \
   }

/** @brief True when #DPRINTF output would actually be written
 */
#define DPRINTF_IS_ACTIVE rmon_condition(xaybzc, INFOPRINT)

/** @brief Write a printf style debug message, if the layer has INFOPRINT enabled
 */
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

/** @brief As #DPRINTF, but without the trace level prefix
 */
#define DPRINTF_(...) \
   if (rmon_condition(xaybzc, INFOPRINT)) { \
      rmon_mprintf(__VA_ARGS__); \
   }

/** @brief True when tracing is enabled for this translation unit's layer
 */
#define ISTRACE (rmon_condition(xaybzc, TRACE))

/** @brief True when monitoring is enabled and at least one layer is traced
 */
#define TRACEON  (rmon_is_enabled() && !rmon_mliszero(&RMON_DEBUG_ON))

#else /* NO_SGE_COMPILE_DEBUG */

#define DENTER_MAIN( layer, program )
#define DENTER(layer)
#define DRETURN(x) return x
#define DRETURN_VOID return
#define DTRACE
#define DPRINTF(...)
#define TRACEON
#define ISTRACE

#endif /* NO_SGE_COMPILE_DEBUG */

/// currently enabled debug classes per layer
extern monitoring_level RMON_DEBUG_ON;

int rmon_condition(int layer, int debug_class);

int rmon_is_enabled();
/// switch monitoring off for this process
void rmon_disable();
/// switch monitoring on for this process
void rmon_enable();

void rmon_mopen();

void rmon_menter(const char *func, const char *thread_name, int thread_id);

void rmon_mtrace(const char *func, const char *file, int line, const char *thread_name, int thread_id);

void rmon_mexit(const char *func, const char *file, int line, const char *thread_name, int thread_id);

void rmon_mprintf(const char *fmt, ...);

/** @brief Thread local data used to label trace output */
struct rmon_helper_t {
    char thread_name[32];   ///< name of the thread, truncated to 31 characters
    int thread_id;          ///< id of the thread
};

/// helper of the calling thread, allocated on first use
rmon_helper_t *rmon_get_helper();
