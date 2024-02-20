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

#include <cstring>
#include <cstdarg>
#include <sys/types.h>

#include "uti/sge_rmon_monitoring_level.h"

#include "comm/cl_commlib.h"

#ifndef NO_SGE_COMPILE_DEBUG

#if defined(SOLARIS)
#  include <note.h>
#endif

#define DENTER_MAIN(layer, program) \
   static const char SGE_FUNC[] = program; \
   static const int xaybzc = layer; \
   rmon_mopen(); \
   if (rmon_condition(xaybzc, TRACE)) { \
      cl_thread_settings_t* ___thread_config = cl_thread_get_thread_config(); \
      if (___thread_config != nullptr) { \
         rmon_menter (SGE_FUNC, ___thread_config->thread_name, ___thread_config->thread_id); \
      } else { \
         rmon_menter (SGE_FUNC, nullptr, -1); \
      } \
   } \
   void()

#define DENTER(layer) \
   static const int xaybzc = layer; \
   if (rmon_condition(xaybzc, TRACE)) { \
      cl_thread_settings_t* ___thread_config = cl_thread_get_thread_config(); \
      if (___thread_config != nullptr) { \
         rmon_menter (__func__, ___thread_config->thread_name, ___thread_config->thread_id); \
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
      cl_thread_settings_t* ___thread_config = cl_thread_get_thread_config(); \
      if (___thread_config != nullptr) { \
         rmon_mexit(__func__, __FILE__, __LINE__, ___thread_config->thread_name, ___thread_config->thread_id); \
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
      cl_thread_settings_t* ___thread_config = cl_thread_get_thread_config(); \
      if (___thread_config != nullptr) { \
         rmon_mexit(__func__, __FILE__, __LINE__, ___thread_config->thread_name, ___thread_config->thread_id); \
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
      cl_thread_settings_t* ___thread_config = cl_thread_get_thread_config(); \
      if (___thread_config != nullptr) { \
         rmon_mtrace(__func__, __FILE__, __LINE__, ___thread_config->thread_name, ___thread_config->thread_id); \
      } else { \
         rmon_mtrace(__func__, __FILE__, __LINE__, nullptr, -1); \
      } \
   } \
   void()

#define DTRACE_                                                                  \
   if (rmon_condition(xaybzc, TRACE)) {                                          \
      rmon_mtrace(__func__, __FILE__, __LINE__, nullptr, -1);                    \
   }

#define DPRINTF(msg) \
   if (rmon_condition(xaybzc, INFOPRINT)) { \
      rmon_helper_t *helper = rmon_get_helper(); \
      if (helper != nullptr) { \
         cl_thread_settings_t* ___thread_config = cl_thread_get_thread_config(); \
         if (___thread_config != nullptr) { \
            strcpy(helper->thread_name, ___thread_config->thread_name); \
            helper->thread_id = ___thread_config->thread_id; \
         } \
      } \
      rmon_mprintf msg ; \
      if (helper != nullptr) { \
         helper->thread_name[0] = '\0'; \
         helper->thread_id = -1; \
      } \
   } \
   void()

#define DPRINTF_(msg) \
   if (rmon_condition(xaybzc, INFOPRINT)) { \
      rmon_mprintf msg ; \
   }

#define ISTRACE (rmon_condition(xaybzc, TRACE))

#define TRACEON  (rmon_is_enabled() && !rmon_mliszero(&RMON_DEBUG_ON))

#else /* NO_SGE_COMPILE_DEBUG */

#define DENTER_MAIN( layer, program )
#define DENTER(layer)
#define DRETURN(x) return x
#define DRETURN_VOID return
#define DTRACE
#define DPRINTF(x)
#define DPRINTF(x)
#define DTIMEPRINTF(x)
#define DSPECIALPRINTF(x)
#define TRACEON
#define ISTRACE

#endif /* NO_SGE_COMPILE_DEBUG */

extern monitoring_level RMON_DEBUG_ON;

int rmon_condition(int layer, int debug_class);

int rmon_is_enabled();

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
