#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024 HPC-Gridware GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

namespace ocs {
   class BindingFinder {
#if defined(OCS_HWLOC) || defined(BINDING_SOLARIS)
   private:
      static bool parse_job_accounting_and_create_logical_list(const char *binding_string, char **rankfileinput);
#endif

#if defined(OCS_HWLOC)
   private:
      static bool linear_linux(dstring *result, const lListElem *binding_elem, bool automatic);
      static bool striding_linux(dstring *result, const lListElem *binding_elem, bool automatic);
      static bool explicit_linux(dstring *result, const lListElem *binding_elem);
   public:
      /* creates string with core binding which is written to job "config" file */
      static bool create_binding_strategy_string_linux(dstring *result, lListElem *jep, char **rankfileinput);
#endif

#ifdef BINDING_SOLARIS
   private:
      static bool linear_automatic_solaris(dstring* result, const lListElem* binding_elem, char** env);
      static bool striding_solaris(dstring* result, const lListElem* binding_elem, const bool automatic, const bool do_linear, char* err_str, int err_length, char** env);
      static bool explicit_solaris(dstring* result, const lListElem* binding_elem, char* err_str, int err_length, char** env);
   public:
      static bool create_binding_strategy_string_solaris(dstring* result, lListElem *jep, char* err_str, int err_length, char** env, char** rankfileinput);
#endif
   };
}