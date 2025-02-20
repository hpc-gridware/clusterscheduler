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

#if defined (ULINUXAMD64)
// required on CentOS 6 to fix a compiler error about ::quick_exit
#include <cstdlib>
#endif

#include <dlfcn.h>

#ifdef SOLARIS
#include <link.h>
#endif

#include <cstring>

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

lListElem *
spool_dynamic_create_context(lList **answer_list, const char *method,
                             const char *shlib_name, const char *args)
{
   bool ok = true;
   lListElem *context = nullptr;

   /* shared lib name buffer and handle */
   dstring shlib_dstring = DSTRING_INIT;
   const char *shlib_fullname;
   void *shlib_handle;

   /* get_method function pointer and result */
   spooling_get_method_func get_spooling_method = nullptr;
   const char *spooling_name = nullptr;

   DENTER(TOP_LAYER);

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

   /* open the shared lib */
   # if defined(DARWIN)
   # ifdef RTLD_NODELETE
   shlib_handle = dlopen(shlib_fullname, RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
   # else
   shlib_handle = dlopen(shlib_fullname, RTLD_NOW | RTLD_GLOBAL );
   # endif /* RTLD_NODELETE */
   # else
   # ifdef RTLD_NODELETE
   shlib_handle = dlopen(shlib_fullname, RTLD_NOW | RTLD_NODELETE);
   # else
   shlib_handle = dlopen(shlib_fullname, RTLD_NOW);
   # endif /* RTLD_NODELETE */
   #endif

   if (shlib_handle == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, 
                              MSG_SPOOL_ERROROPENINGSHAREDLIB_SS, 
                              shlib_fullname, dlerror());
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

