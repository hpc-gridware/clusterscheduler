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
 * @brief Choosing the spooling method at runtime, by loading a shared library
 */

#if defined (ULINUXAMD64)
// required on CentOS 6 to fix a compiler error about ::quick_exit
#include <cstdlib>
#endif

#include <dlfcn.h>

#ifdef SOLARIS
#include <link.h>
#endif

#include <cstring>
#include <string>
#include <unistd.h>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_log.h"

#include "sgeobj/sge_answer.h"

#include "spool/dynamic/msg_spoollib_dynamic.h"
#include "spool/dynamic/sge_spooling_dynamic.h"

static const char *spooling_method = "dynamic";

#ifdef SPOOLING_dynamic
const char *get_spooling_method()
#else
const char *get_dynamic_spooling_method()
#endif
{
   return spooling_method;
}

/**
 * @brief the path a spooling library is opened by
 *
 * The spooling libraries are installed next to this one, in $SGE_ROOT/lib/<arch>. Opened by their
 * bare name, they are found through the $ORIGIN relative RUNPATH of the object calling dlopen() -
 * but a preloaded library which wraps dlopen(), e.g. the one of a NoMachine desktop session,
 * becomes that caller, and it has no such RUNPATH. Opened by absolute path the library is found
 * whoever calls dlopen(), and its own dependencies are still resolved with its own RUNPATH
 * (CS-2839).
 *
 * @param shlib_fullname file name of the spooling library, e.g. libspoolb.so
 * @return the absolute path of shlib_fullname in the directory of this library, when it exists
 *         there - otherwise shlib_fullname unchanged, which leaves the search to dlopen(), as in a
 *         build directory, where every library has a directory of its own
 */
static std::string
spool_dynamic_get_shlib_path(const char *shlib_fullname) {
   DENTER(TOP_LAYER);

   std::string ret{shlib_fullname};

   Dl_info info;
   if (dladdr(reinterpret_cast<void *>(&spool_dynamic_get_shlib_path), &info) != 0 &&
       info.dli_fname != nullptr) {
      const char *slash = strrchr(info.dli_fname, '/');
      if (slash != nullptr) {
         std::string path{info.dli_fname, static_cast<size_t>(slash - info.dli_fname + 1)};
         path += shlib_fullname;
         if (access(path.c_str(), F_OK) == 0) {
            ret = path;
         }
      }
   }

   DPRINTF("opening spooling library %s\n", ret.c_str());
   DRETURN(ret);
}

/** @brief Load a spooling shared library and build its context
 *
 * @param answer_list to return error messages
 * @param method      the spooling method the library must report
 * @param shlib_name  the library to load, without the platform suffix
 * @param args        the argument string handed on to the library's own
 *                    create-context function
 *
 * @return the new spooling context, or nullptr if the library could not be
 *         loaded, lacks the expected symbols, or implements another method
 */
lListElem *
spool_dynamic_create_context(lList **answer_list, const char *method,
                             const char *shlib_name, const char *args) {
   DENTER(TOP_LAYER);

   bool ok = true;
   lListElem *context = nullptr;

   /* shared lib name buffer and handle */
   dstring shlib_dstring = DSTRING_INIT;
   const char *shlib_fullname;
   void *shlib_handle;

   /* get_method function pointer and result */
   spooling_get_method_func get_spooling_method = nullptr;
   const char *spooling_name = nullptr;

   /* build the full name of the shared lib - append architecture dependent
    * shlib postfix 
    */
   shlib_fullname = sge_dstring_sprintf(&shlib_dstring, "%s.%s", shlib_name, 
#if defined(DARWIN)
                                        "dylib"
#else
                                        "so"
#endif
                                       );

   // Open the shared lib, by absolute path where possible.
   // We need the symbols (esp. from the classic spooling library) in a local name space,
   // as we link sge_qmaster against the spoolc_static library for stored procedure output.
   // Use the RTLD_LOCAL flag explicitly, even if it should be the default.
   const std::string shlib_path = spool_dynamic_get_shlib_path(shlib_fullname);
   # if defined(DARWIN)
   # ifdef RTLD_NODELETE
   shlib_handle = dlopen(shlib_path.c_str(), RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE);
   # else
   shlib_handle = dlopen(shlib_path.c_str(), RTLD_NOW | RTLD_LOCAL );
   # endif /* RTLD_NODELETE */
   # else
   # ifdef RTLD_NODELETE
   shlib_handle = dlopen(shlib_path.c_str(), RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE);
   # else
   shlib_handle = dlopen(shlib_path.c_str(), RTLD_NOW | RTLD_LOCAL);
   # endif /* RTLD_NODELETE */
   #endif

   if (shlib_handle == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, 
                              MSG_SPOOL_ERROROPENINGSHAREDLIB_SS, 
                              shlib_path.c_str(), dlerror());
      ok = false;
   } 

   /* retrieve function pointer of get_method function in shared lib */
   if (ok) {
      dstring get_spooling_method_func_name = DSTRING_INIT;

      sge_dstring_sprintf(&get_spooling_method_func_name,
                          "get_%s_spooling_method", method);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
      get_spooling_method = (spooling_get_method_func)
                            dlsym(shlib_handle, 
                            sge_dstring_get_string(&get_spooling_method_func_name));
#pragma GCC diagnostic pop
      if (get_spooling_method == nullptr) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                 ANSWER_QUALITY_ERROR, 
                                 MSG_SPOOL_SHLIBDOESNOTCONTAINSPOOLING_SS, 
                                 shlib_fullname, dlerror());
         ok = false;
      }
      sge_dstring_free(&get_spooling_method_func_name);
   }

   /* retrieve name of spooling method in shared lib */
   if (ok) {
      spooling_name = get_spooling_method();

      if (spooling_name == nullptr) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                 ANSWER_QUALITY_INFO, 
                                 MSG_SPOOL_SHLIBGETMETHODRETURNSNULL_S, 
                                 shlib_fullname);
         ok = false;
      } else {
         if (strcmp(spooling_name, method) != 0) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                    ANSWER_QUALITY_INFO, 
                                    MSG_SPOOL_SHLIBCONTAINSXWENEEDY_SSS, 
                                    shlib_fullname, spooling_name, method);
            ok = false;
         }
      }
   }

   /* create spooling context from shared lib */
   if (ok) {
      dstring create_context_func_name = DSTRING_INIT;
      spooling_create_context_func create_context;

      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_INFO, 
                              MSG_SPOOL_LOADINGSPOOLINGMETHOD_SS, 
                              spooling_name, shlib_fullname);

      sge_dstring_sprintf(&create_context_func_name, 
                          "spool_%s_create_context", spooling_name);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
      create_context = 
            (spooling_create_context_func) 
            dlsym(shlib_handle, 
                  sge_dstring_get_string(&create_context_func_name));
#pragma GCC diagnostic pop
      if (create_context == nullptr) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                 ANSWER_QUALITY_ERROR, 
                                 MSG_SPOOL_SHLIBDOESNOTCONTAINSPOOLING_SS,
                                 shlib_fullname, dlerror());
         ok = false;
      } else {
         context = create_context(answer_list, args);
      }
      sge_dstring_free(&create_context_func_name);
   }

   /* cleanup in case of initialization error */
   if (context == nullptr) {
      if (shlib_handle != nullptr) {
         dlclose(shlib_handle);
      }
   }

   sge_dstring_free(&shlib_dstring);

   DRETURN(context);
}
