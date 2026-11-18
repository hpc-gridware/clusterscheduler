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
 *  Portions of this software are Copyright (c) 2011-2012 Univa Corporation
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Variable lists: name/value pairs such as the job environment
 *
 * Several places need to store variables and their values - the job
 * environment, the job context and so on - in the form
 *
 *     variable[=value][,variable[=value],...]
 *
 * The `VA_Type` CULL element holds one such pair; the functions here work on
 * lists of them.
 */

#include <cstdio>
#include <cstring>

#include "uti/sge_arch.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"

#include "cull/cull.h"

#include "sgeobj/sge_var.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "sgeobj/tom/ocs_tom_VA.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

/**
 * @brief Name of sharedlib path variable
 *
 * Returns the operating dependent name of the shared library path
 * (e.g. LIBPATH, SHLIB_PATH, LD_LIBRARY_PATH).
 * If the name of the sharedlib path is not known for an operating
 * system (the port has not yet been done), a compile time error
 * is raised.
 *
 * @return Name of the shared lib path variable
 *
 * @note Raising a compile time error (instead of e.g. just returning nullptr
 *       or LD_LIBRARY_PATH as default) has the following reason:
 *       Setting the shared lib path is a very sensible operation
 *       concerning security.
 *       Example: If a shared linked rshd (called for qrsh execution) is
 *       executed with a faked shared lib path, operations defined in
 *       a non sge library libgdi.so might be executed as user root.
 */
const char *var_get_sharedlib_path_name() {
#if defined(LINUX) || defined(SOLARIS) || defined(FREEBSD) || defined(NETBSD)
   return "LD_LIBRARY_PATH";
#elif defined(DARWIN)
   return "DYLD_LIBRARY_PATH";
#else
#error "don't know how to set shared lib path on this architecture"
   return nullptr; /* never reached */
#endif
}

/**
 * @brief Add/change an variable
 *
 * If the variable `name` does not already exist in `varl`,
 * it is created and initialized with `value`.
 * Otherwise, its value is overwritten with `value`
 *
 * @param[in,out] varl VA_Type list, created if the handle is empty
 * @param name the name of the variable
 * @param value the (new) value of the variable
 *
 * @see #var_list_set_int, #var_list_set_uint32t, #var_list_set_sharedlib_path
 */
void var_list_set_string(ocs::tom::ListPtr<ocs::tom::VA> &varl, const char *name, const char *value) {
   DENTER(TOP_LAYER);

   if (name == nullptr || value == nullptr) {
      DRETURN_VOID;
   }
   if (!varl) {
      varl = ocs::tom::ListPtr<ocs::tom::VA>::create("");
   }
   auto elem = varl->find_first<VA_variable>(name);
   if (!elem) {
      elem = varl->emplace_back<VA_variable>(name);
   }
   ocs::tom::set<VA_value>(elem, value);
   DRETURN_VOID;
}

/**
 * @brief var_list_set_string() for an untyped caller
 *
 * @param[in,out] varl VA_Type list, checked; nothing happens for nullptr
 * @param name the name of the variable
 * @param value the (new) value of the variable
 */
void var_list_set_string(lList **varl, const char *name, const char *value) {
   if (varl != nullptr) {
      ocs::tom::ListPtr<ocs::tom::VA> list(*varl);
      var_list_set_string(list, name, value);
      *varl = list.release();
   }
}

/**
 * @brief Add/change an variable
 *
 * If the variable `name` does not already exist in `varl`,
 * it is created and initialized with `value`.
 * Otherwise, its value is overwritten with `value`
 *
 * @param[in,out] varl VA_Type list, created if the handle is empty
 * @param name the name of the variable
 * @param value the (new) value of the variable
 *
 * @see #var_list_set_string, #var_list_set_uint32t, #var_list_set_sharedlib_path
 */
void var_list_set_int(ocs::tom::ListPtr<ocs::tom::VA> &varl, const char *name, const int value) {
   DENTER(TOP_LAYER);

   char buffer[2048];

   snprintf(buffer, sizeof(buffer), "%d", value);
   var_list_set_string(varl, name, buffer);
   DRETURN_VOID;
}


/**
 * @brief Add/change a variable
 *
 * If the variable `name` does not already exist in `varl`,
 * it is created and initialized with `value`.
 * Otherwise, its value is overwritten with `value`
 *
 * @param[in,out] varl VA_Type list, created if the handle is empty
 * @param name the name of the variable
 * @param value the (new) value of the variable
 *
 * @see #var_list_set_string, #var_list_set_int, #var_list_set_sharedlib_path
 */
void var_list_set_uint32t(ocs::tom::ListPtr<ocs::tom::VA> &varl, const char *name, const uint32_t value) {
   DENTER(TOP_LAYER);
   char buffer[2048];
   snprintf(buffer, sizeof(buffer), sge_u32, value);
   var_list_set_string(varl, name, buffer);
   DRETURN_VOID;
}


/**
 * @brief Set shared lib path
 *
 * Sets or replaces the shared lib path in the list of variables.
 * The SGE shared lib path is always set to the beginning of the
 * resulting shared lib path
 * (security, see var_get_sharedlib_path_name())
 *
 * @param[in,out] varl list of environment variables (VA_Type), created if the handle is empty
 *
 * @see #var_get_sharedlib_path_name, #var_list_set_string, #var_list_set_int, #var_list_set_uint32t
 */
void var_list_set_sharedlib_path(ocs::tom::ListPtr<ocs::tom::VA> &varl) {
   DENTER(TOP_LAYER);

   char *sharedlib_path;
   char *sge_sharedlib_path;
   size_t sge_sharedlib_path_size;
   const char *sge_root = sge_get_root_dir(0, nullptr, 0, 1);
   const char *sharedlib_path_name = var_get_sharedlib_path_name();

   /* this is the SGE sharedlib path */
   sge_sharedlib_path_size = strlen(sge_root) + strlen("/lib/") + strlen(sge_get_arch()) + 1;
   sge_sharedlib_path = sge_malloc(sge_sharedlib_path_size);
   snprintf(sge_sharedlib_path, sge_sharedlib_path_size, "%s/lib/%s", sge_root, sge_get_arch());

   /* if already in environment: extend by SGE sharedlib path, else set */
   const auto sharedlib_elem = varl ? varl->find_first<VA_variable>(sharedlib_path_name)
                                    : ocs::tom::Element<ocs::tom::VA>();
   if (sharedlib_elem) {
      const char *old_value = ocs::tom::get<VA_value>(sharedlib_elem);

      if(old_value && strlen(old_value) > 0) {
         DPRINTF("sharedlib path %s already set:\n", sharedlib_path_name);
         
         sharedlib_path = sge_malloc(strlen(old_value) + 1 + 
                          strlen(sge_sharedlib_path) + 1);
         strcpy(sharedlib_path, sge_sharedlib_path);
         strcat(sharedlib_path, ":");
         strcat(sharedlib_path, old_value);
         ocs::tom::set<VA_value>(sharedlib_elem, sharedlib_path);
         sge_free(&sharedlib_path);
      } else {
         DPRINTF("overwriting empty sharedlib path %s\n", sharedlib_path_name);
         ocs::tom::set<VA_value>(sharedlib_elem, sge_sharedlib_path);
      }
   } else {
      DPRINTF("creating new sharedlib path %s\n", sharedlib_path_name);
      if (!varl) {
         varl = ocs::tom::ListPtr<ocs::tom::VA>::create("");
      }
      const auto new_elem = varl->emplace_back<VA_variable>(sharedlib_path_name);
      ocs::tom::set<VA_value>(new_elem, sge_sharedlib_path);
   }

   sge_free(&sge_sharedlib_path);
   DRETURN_VOID;
}


/**
 * @brief Dump variables from list to file
 *
 * Parses the list of type VA_Type `varl` containing the
 * description of variables.
 * Dumps all list elements to `file` in the format:
 * `name`=`value`
 * `name` is read from VA_variable, `value` from VA_value.
 *
 * @param varl list of variables (VA_Type), may be an empty handle
 * @param file filehandle of a previously opened (for writing) file
 */
void var_list_dump_to_file(const ocs::tom::ConstList<ocs::tom::VA> varl, FILE *file) {
   DENTER(TOP_LAYER);

   if (!varl || file == nullptr) {
      DRETURN_VOID;
   }

   for (const auto elem : varl) {
      /*
       * Replace <LF> by \n sequence for multiline environment
       * variable support.
       */
      const char *env_name = ocs::tom::get<VA_variable>(elem);
      const char *env_value = ocs::tom::get<VA_value>(elem);
      const char *new_env_value = sge_replace_substring(env_value, "\n", "\\n");

      if (new_env_value == nullptr) {
         fprintf(file, "%s=%s\n", env_name, env_value);
      } else {
         fprintf(file, "%s=%s\n", env_name, new_env_value);
         sge_free(&new_env_value);
      }
   }
   DRETURN_VOID;
}


/**
 * @brief Get value of certain variable
 *
 * Return the string value of a variable
 * with the name "variable" which is stored in "varl".
 *
 * @param varl VA_Type list, may be an empty handle
 * @param variable variable name
 *
 * @return value or nullptr
 *
 * @see #var_list_set_string, #var_list_set_int, #var_list_set_uint32t, #var_list_set_sharedlib_path
 */
const char *var_list_get_string(const ocs::tom::ConstList<ocs::tom::VA> varl, const char *variable) {
   const char *ret = nullptr;
   const auto var = ocs::tom::find_first<VA_variable>(varl, variable);

   if (var) {
      ret = ocs::tom::get<VA_value>(var);
   }
   return ret;
}


/**
 * @brief Copy vars with certain prefix
 *
 * Make a copy of all entries in "src_varl"
 * beginning with "prefix". "prefix" is replaced by "new_prefix"
 * for all created elements. The new elements will be added to
 * "varl".
 *
 * @code
 * "__SGE_PREFIX__O_HOME" ===> "SGE_O_HOME
 * @endcode
 *
 * @param[in,out] varl VA_Type list the copies are added to, created if the handle is empty
 * @param src_varl the list to copy from (VA_Type), may be an empty handle and may be varl itself
 * @param prefix prefix string (e.g. VAR_PREFIX)
 * @param new_prefix new prefix string (e.g. "SGE_")
 *
 * @see #var_list_remove_prefix_vars
 */
void var_list_copy_prefix_vars(ocs::tom::ListPtr<ocs::tom::VA> &varl, const ocs::tom::ConstList<ocs::tom::VA> src_varl,
                               const char *prefix, const char *new_prefix) {
   DENTER(TOP_LAYER);
   const size_t prefix_len = strlen(prefix);
   ocs::tom::ListPtr<ocs::tom::VA> var_list2;

   for (const auto var_elem : src_varl) {
      const char *prefix_name = ocs::tom::get<VA_variable>(var_elem);

      if (strncmp(prefix_name, prefix, prefix_len) == 0) {
         char name[MAX_STRING_SIZE];
         const char *name_without_prefix = &prefix_name[prefix_len];
         const char *value = ocs::tom::get<VA_value>(var_elem);

         snprintf(name, sizeof(name), "%s%s", new_prefix, name_without_prefix);
         var_list_set_string(var_list2, name, value);
      }
   }
   if (!varl) {
      varl = ocs::tom::ListPtr<ocs::tom::VA>::create("");
   }
   var_list_add_as_set(*varl, std::move(var_list2));

   DRETURN_VOID;
}


/**
 * @brief Copy vars with certain prefix
 *
 * Make a copy of all entries in "src_varl"
 * beginning with "prefix". "prefix" is replaced by "new_prefix"
 * for all created elements. The new elements will be added to
 * "varl" if it is undefined in "varl".
 *
 * @code
 * "__SGE_PREFIX2__TASK_ID" ===> "SGE_TASK_ID
 * @endcode
 *
 * @param[in,out] varl VA_Type list the copies are added to, created if the handle is empty
 * @param src_varl the list to copy from (VA_Type), may be an empty handle and may be varl itself
 * @param prefix prefix string (e.g. VAR_PREFIX_NR)
 * @param new_prefix new prefix string (e.g. "SGE_")
 *
 * @see #var_list_remove_prefix_vars
 */
void var_list_copy_prefix_vars_undef(ocs::tom::ListPtr<ocs::tom::VA> &varl,
                                     const ocs::tom::ConstList<ocs::tom::VA> src_varl,
                                     const char *prefix, const char *new_prefix) {
   DENTER(TOP_LAYER);
   const size_t prefix_len = strlen(prefix);
   ocs::tom::ListPtr<ocs::tom::VA> var_list2;

   for (const auto var_elem : src_varl) {
      const char *prefix_name = ocs::tom::get<VA_variable>(var_elem);

      if (!strncmp(prefix_name, prefix, prefix_len)) {
         char name[MAX_STRING_SIZE];
         const char *value = ocs::tom::get<VA_value>(var_elem);
         const char *name_without_prefix = &prefix_name[prefix_len];

         snprintf(name, sizeof(name), "%s%s", new_prefix, name_without_prefix);
         if (!ocs::tom::find_first<VA_variable>(varl.get(), name)) {
            var_list_set_string(var_list2, name, value);
         }
      }
   }
   if (!varl) {
      varl = ocs::tom::ListPtr<ocs::tom::VA>::create("");
   }
   varl->splice(std::move(var_list2));
   DRETURN_VOID;
}


/**
 * @brief Copy env. vars
 *
 * Copies every variable from `src_varl` into `varl`. A variable without a
 * value is copied as the empty string, never as nullptr.
 *
 * @param[in,out] varl VA_Type list the copies are added to, created if the handle is empty
 * @param src_varl source VA_Type list, may be an empty handle
 *
 * @note The banner this was converted from also described an `ignore_prefix`
 *       parameter that filtered the copied variables. Neither the parameter
 *       nor the filtering exist any more.
 */
void var_list_copy_env_vars_and_value(ocs::tom::ListPtr<ocs::tom::VA> &varl,
                                      const ocs::tom::ConstList<ocs::tom::VA> src_varl) {
   DENTER(TOP_LAYER);

   for (const auto env : src_varl) {
      const char *name = ocs::tom::get<VA_variable>(env);
      const char *s = ocs::tom::get<VA_value>(env);
      var_list_set_string(varl, name, s ? s : "");
   }

   DRETURN_VOID;
}

/**
 * @brief var_list_copy_env_vars_and_value() for an untyped caller
 *
 * @param[in,out] varl VA_Type list the copies are added to, checked; nothing happens for nullptr
 * @param src_varl source VA_Type list, may be an empty handle
 */
void var_list_copy_env_vars_and_value(lList **varl, const ocs::tom::ConstList<ocs::tom::VA> src_varl) {
   if (varl != nullptr) {
      ocs::tom::ListPtr<ocs::tom::VA> list(*varl);
      var_list_copy_env_vars_and_value(list, src_varl);
      *varl = list.release();
   }
}

/**
 * @brief Remove vars with certain prefix
 *
 * Remove all entries from "varl" where the name
 * beginns with "prefix"
 *
 * The list itself stays, also when it becomes empty.
 *
 * @param varl VA_Type list, may be an empty handle
 * @param prefix prefix string (e.g. VAR_PREFIX)
 *
 * @see #var_list_split_prefix_vars
 */
void var_list_remove_prefix_vars(const ocs::tom::List<ocs::tom::VA> list, const char *prefix) {
   DENTER(TOP_LAYER);

   const size_t prefix_len = strlen(prefix);

   auto var_elem = list.front();
   while (var_elem) {
      const char *prefix_name = ocs::tom::get<VA_variable>(var_elem);
      const auto next_var_elem = var_elem.next();

      if (!strncmp(prefix_name, prefix, prefix_len)) {
         list.erase(var_elem);
      }
      var_elem = next_var_elem;
   }
   DRETURN_VOID;
}

/**
 * @brief var_list_remove_prefix_vars() for an untyped caller
 *
 * @param varl pointer to the VA_Type list, checked; the list may be nullptr
 * @param prefix prefix string (e.g. VAR_PREFIX)
 */
void var_list_remove_prefix_vars(lList **varl, const char *prefix) {
   var_list_remove_prefix_vars(ocs::tom::List<ocs::tom::VA>(*varl), prefix);
}

/**
 * @brief Split a list of variables
 *
 * Move all variable elements from "varl" to "pefix_vars" which
 * begin with "prefix". *pefix_vars will be created if is does not
 * exist.
 *
 * @p pefix_vars may already hold variables and is appended to.
 *
 * @param list VA_Type list, may be an empty handle
 * @param[in,out] pefix_vars VA_Type list, created if the handle is empty and a variable is moved
 * @param prefix string (e.g. VAR_PREFIX)
 *
 * @see #var_list_remove_prefix_vars
 */
void var_list_split_prefix_vars(const ocs::tom::List<ocs::tom::VA> list, ocs::tom::ListPtr<ocs::tom::VA> &pefix_vars,
                                const char *prefix) {
   DENTER(TOP_LAYER);

   const size_t prefix_len = strlen(prefix);

   auto var_elem = list.front();
   while (var_elem) {
      const char *prefix_name = ocs::tom::get<VA_variable>(var_elem);
      const auto next_var_elem = var_elem.next();

      if (!strncmp(prefix_name, prefix, prefix_len)) {
         auto dechained_elem = list.extract(var_elem);

         if (!pefix_vars) {
            pefix_vars = ocs::tom::ListPtr<ocs::tom::VA>::create("");
         }

         pefix_vars->push_back(std::move(dechained_elem));
      }
      var_elem = next_var_elem;
   }
   DRETURN_VOID;
}


/**
 * @brief Concatenate two lists as sets
 *
 * Concatenate two lists of equal type throwing away the second list.
 * Elements in the second list will replace elements with the same key in the
 * the first list.  If the first list contains duplicate element keys, only
 * the first element with a given key will be replaced by an element from the
 * second list with the same key.
 *
 * The second list is consumed: it is freed on every return, also on an
 * error.
 *
 * @param lp0 first list (VA_Type), may be an empty handle
 * @param lp1 second list (VA_Type), consumed; may be empty
 *
 * @return error state 0 - OK -1 - Error
 */
int var_list_add_as_set(const ocs::tom::List<ocs::tom::VA> lp0, ocs::tom::ListPtr<ocs::tom::VA> &&lp1) {
   DENTER(CULL_LAYER);

   // taken over on every path, also the early returns
   const auto vars = std::move(lp1);
   if (!vars || !lp0) {
      DRETURN(-1);
   }

   /* Check if the two lists are equal */
   const auto src = *vars;
   if (lCompListDescr(lGetListDescr(lp0), lGetListDescr(src)) != 0) {
      DRETURN(-1);
   }

   for (auto first = src.front(); first; first = src.front()) {
      /* Get the first element from the second list */
      auto ep1 = src.extract(first);

      /* Get it's name, and use the name to look for a matching element in the
       * first list. */
      const char *name = ocs::tom::get<VA_variable>(ep1);
      const auto ep0 = ocs::tom::find_first_rw<VA_variable>(lp0, name);

      /* If there is a matching element in the first list, set it's value to the
       * value of the element from the second list; the element of the second
       * list is freed when ep1 goes out of scope. */
      if (ep0) {
         ocs::tom::set<VA_value>(ep0, ocs::tom::get<VA_value>(ep1));
      }
      /* If there is no matching element, add the element from the second list
       * to the first list. */
      else {
         lp0.push_back(std::move(ep1));
      }
   }

   /* The second list is no longer needed; vars frees it. */
   DRETURN(0);
}


/**
 * @brief Verify contents of a variable list
 *
 * Verifies the contents of a variable list.
 * Variable names may not be nullptr or empty strings.
 *
 * @param lp the list to verify (VA_Type), may be an empty handle
 * @param answer_list answer list to pass back error messages
 *
 * @return true on success, false in case of errors, error message in answer_list
 *
 * @note MT-NOTE: var_list_verify() is MT safe
 */
bool var_list_verify(const ocs::tom::ConstList<ocs::tom::VA> lp, ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   bool ret = true;
   for (const auto ep : lp) {
      const char *variable = ocs::tom::get<VA_variable>(ep);
      if (variable == nullptr || variable[0] == '\0') {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                                 MSG_OBJECT_VARIABLENAME_NOT_EMPTY);
         ret = false;
         break;
      }
   }

   /* TODO: further checks, e.g. length, format strings */

   return ret;
}

/**
 * @brief Parse a comma-separated variable list into a VA_Type list.
 *
 * Splits @p variable_str on ',' into "name[=value]" tokens and appends a VA_Type
 * element per token to @p lpp (creating the list if the handle is empty). A name
 * that is already in the list keeps its element and takes the new value, so a
 * variable given twice holds the last value once (CS-3053). The input
 * is attacker-controlled (qsub/qalter -v/-V/-ac/-dc, JSV); a malformed token
 * that carries no name part (e.g. "==") is rejected with an error instead of
 * dereferencing NULL. MT-NOTE: this function is MT safe.
 *
 * @param[in,out] lpp              VA_Type list to append to; created if the
 *                                 handle is empty, freed on error 2 and 3
 * @param[in]     variable_str     source string ("a=1,b=2,...")
 * @param[in]     check_environment if true, a name without "=value" takes its
 *                                 value from the caller's environment
 * @return 0 on success, >0 on error (2 strdup, 3 empty list, 5 malformed token)
 */
int var_list_parse_from_string(ocs::tom::ListPtr<ocs::tom::VA> &lpp, const char *variable_str,
                               const int check_environment) {
   DENTER(TOP_LAYER);

   char *variable;
   char *val_str;
   int var_len;
   char **str_str;
   char **pstr;
   char *va_string;

   va_string = sge_strdup(nullptr, variable_str);
   if (!va_string) {
      lpp.reset();
      DRETURN(2);
   }
   str_str = string_list(va_string, ",", nullptr);
   if (!str_str || !*str_str) {
      lpp.reset();
      sge_free(&va_string);
      DRETURN(3);
   }

   if (!lpp) {
      lpp = ocs::tom::ListPtr<ocs::tom::VA>::create("variable list");
   }

   for (pstr = str_str; *pstr; pstr++) {
      struct saved_vars_s *context;

      context = nullptr;
      variable = sge_strtok_r(*pstr, "=", &context);
      // sge_strtok_r() returns nullptr when the token is all delimiters (e.g.
      // "=="); string_list() does not strip '=', so such a token is reachable
      // from client input. Reject it gracefully instead of asserting / running
      // strlen() on NULL (CS-2350, CWE-476/CWE-617). context is already
      // allocated by the first sge_strtok_r() call and must be freed here.
      if (variable == nullptr) {
         sge_free_saved_vars(context);
         sge_free(&va_string);
         sge_free(&str_str);
         DRETURN(5);
      }
      var_len=strlen(variable);
      auto ep = lpp->find_first<VA_variable>(variable);
      if (!ep) {
         ep = lpp->emplace_back<VA_variable>(variable);
      }
      val_str=*pstr;

      /* 
       * The character at the end of the first token must be either '=' or '\0'.
       * If it's a '=' then we treat the following string as the value 
       * If it's a '\0' and check_environment is set, then we get the value from
       * the environment variable value. 
       * If it's a '\0' and check_environment is not set, then we set the value
       * to nullptr.
       */
      if (val_str[var_len] == '=') {
          ocs::tom::set<VA_value>(ep, &val_str[var_len+1]);
      } else if (check_environment) {
         ocs::tom::set<VA_value>(ep, sge_getenv(variable));
      } else {
         ocs::tom::set<VA_value>(ep, nullptr);
      }
      sge_free_saved_vars(context);
   }
   sge_free(&va_string);
   sge_free(&str_str);
   DRETURN(0);
}

/**
 * @brief var_list_parse_from_string() for an untyped caller
 *
 * @param[in,out] lpp VA_Type list to append to, checked; created if *lpp is nullptr
 * @param[in] variable_str source string ("a=1,b=2,...")
 * @param[in] check_environment if true, a name without "=value" takes its value from the caller's environment
 * @return as var_list_parse_from_string(), and 1 if @p lpp is nullptr
 */
int var_list_parse_from_string(lList **lpp, const char *variable_str, int check_environment) {
   if (lpp == nullptr) {
      return 1;
   }
   ocs::tom::ListPtr<ocs::tom::VA> list(*lpp);
   const int ret = var_list_parse_from_string(list, variable_str, check_environment);
   *lpp = list.release();
   return ret;
}

