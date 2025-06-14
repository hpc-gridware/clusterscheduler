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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file libs/sgeobj/sge_var.c
 * @brief Functions handline variable lists
 * This file contains functions handling variable lists (VA_Type).
 * Blah blah blah.
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

/****** sgeobj/var/-VariableList **********************************************
*  NAME
*     VariableList - Object to store variable name/value pairs
*
*  FUNCTION
*     In several places within SGE/EE it is necessary to store
*     variables and their values (e.g. job environment variable,
*     job context, ...) of following form: 
*  
*        variable[=value][,variable[=value],...]
*
*     The VA_Type CULL element is used to hold such data.
*     Funktions in the SEE ALSO section might be used to
*     work with VA_Type lists and elements.
*
*  SEE ALSO
*     sgeobj/var/--VA_Type
*     clients/var/var_list_parse_from_string()
*     sgeobj/var/var_list_dump_to_file()
*     sgeobj/var/var_list_copy_complex_vars_and_value()
*     sgeobj/var/var_list_copy_prefix_vars()
*     sgeobj/var/var_list_get_string()
*     sgeobj/var/var_list_set_string()
*     sgeobj/var/var_list_set_int()
*     sgeobj/var/var_list_set_uint32t()
*     sgeobj/var/var_list_set_sharedlib_path()
*     sgeobj/var/var_list_remove_prefix_vars()
******************************************************************************/

/****** sgeobj/var/var_get_sharedlib_path_name() ******************************
*  NAME
*     var_get_sharedlib_path_name -- name of sharedlib path variable
*
*  SYNOPSIS
*     static const char *var_get_sharedlib_path_name();
*
*  FUNCTION
*     Returns the operating dependent name of the shared library path
*     (e.g. LIBPATH, SHLIB_PATH, LD_LIBRARY_PATH).
*     If the name of the sharedlib path is not known for an operating
*     system (the port has not yet been done), a compile time error
*     is raised.
*
*  RESULT
*     Name of the shared lib path variable
*
*  NOTES
*     Raising a compile time error (instead of e.g. just returning nullptr
*     or LD_LIBRARY_PATH as default) has the following reason:
*     Setting the shared lib path is a very sensible operation 
*     concerning security.
*     Example: If a shared linked rshd (called for qrsh execution) is
*     executed with a faked shared lib path, operations defined in
*     a non sge library libgdi.so might be executed as user root.
******************************************************************************/
const char *var_get_sharedlib_path_name()
{
#if defined(AIX)
   return "LIBPATH";
#elif defined(LINUX) || defined(SOLARIS) || defined(DARWIN) || defined(FREEBSD) || defined(NETBSD)
   return "LD_LIBRARY_PATH";
#elif defined(DARWIN)
   return "DYLD_LIBRARY_PATH";
#else
#error "don't know how to set shared lib path on this architecture"
   return nullptr; /* never reached */
#endif
}

/****** sgeobj/var/var_list_set_string() **************************************
*  NAME
*     var_list_set_string -- add/change an variable
*
*  SYNOPSIS
*     void var_list_set_string(lList **varl, 
*                              const char *name, 
*                              const char *value);
*
*  FUNCTION
*     If the variable <name> does not already exist in <varl>, 
*     it is created and initialized with <value>.
*     Otherwise, its value is overwritten with <value>
*
*  INPUTS
*     lList **varl      - VA_Type list
*     const char *name  - the name of the variable
*     const char *value - the (new) value of the variable
*
*  SEE ALSO
*     sgeobj/var/var_list_set_int()
*     sgeobj/var/var_list_set_uint32t()
*     sgeobj/var/var_list_set_sharedlib_path()
******************************************************************************/
void var_list_set_string(lList **varl, const char *name, const char *value) 
{
   lListElem *elem;

   DENTER(TOP_LAYER);
   if (varl == nullptr || name == nullptr || value == nullptr) {
      DRETURN_VOID;
   }
   elem = lGetElemStrRW(*varl, VA_variable, name);
   if (elem == nullptr) {
      elem = lAddElemStr(varl, VA_variable, name, VA_Type);
   }
   lSetString(elem, VA_value, value);
   DRETURN_VOID;
}

/****** sgeobj/var/var_list_delete_string() ***********************************
*  NAME
*     var_list_delete_string -- delete a variable
*
*  SYNOPSIS
*     void var_list_delete_string(lList **varl, 
*                                 const char *name);
*
*  FUNCTION
*     Deletes the variable <name> from the list <varl> if it exists.
*
*  INPUTS
*     lList **varl      - VA_Type list
*     const char *name  - the name of the variable
*
*  SEE ALSO
*     sgeobj/var/var_list_set_string() 
******************************************************************************/
void var_list_delete_string(lList **varl, const char *name)
{
   lListElem *elem;

   DENTER(TOP_LAYER);
   if (varl == nullptr || name == nullptr) {
      DRETURN_VOID;
   }
   elem = lGetElemStrRW(*varl, VA_variable, name);
   if (elem != nullptr) {
      lRemoveElem(*varl, &elem);
   }
   DRETURN_VOID;
}

/****** sgeobj/var/var_list_set_int() *****************************************
*  NAME
*     var_list_set_int -- add/change an variable
*
*  SYNOPSIS
*     void var_list_set_int(lList *varl, 
*                           const char *name, 
*                           int value);
*
*  FUNCTION
*     If the variable <name> does not already exist in <varl>, 
*     it is created and initialized with <value>.
*     Otherwise, its value is overwritten with <value>
*
*  INPUTS
*     varl  - VA_Type list
*     name  - the name of the variable
*     value - the (new) value of the variable
*
*  SEE ALSO
*     sgeobj/var/var_list_set_string()
*     sgeobj/var/var_list_set_uint32t()
*     sgeobj/var/var_list_set_sharedlib_path()
******************************************************************************/
void var_list_set_int(lList **varl, const char *name, int value)
{
   char buffer[2048];

   DENTER(TOP_LAYER);
   snprintf(buffer, sizeof(buffer), "%d", value);
   var_list_set_string(varl, name, buffer);
   DRETURN_VOID;
}

/****** sgeobj/var/var_list_set_uint32t() *****************************************
*  NAME
*     var_list_set_uint32t -- add/change a variable
*
*  SYNOPSIS
*     void var_list_set_uint32t(lList **varl,
*                           const char *name, 
*                           u_long32 value);
*
*  FUNCTION
*     If the variable <name> does not already exist in <varl>, 
*     it is created and initialized with <value>.
*     Otherwise, its value is overwritten with <value>
*
*  INPUTS
*     lList **varl      - VA_Type list
*     const char *name  - the name of the variable
*     u_long32 value    - the (new) value of the variable
*
*  SEE ALSO
*     sgeobj/var/var_list_set_string()
*     sgeobj/var/var_list_set_int()
*     sgeobj/var/var_list_set_sharedlib_path()
******************************************************************************/
void var_list_set_uint32t(lList **varl, const char *name, u_long32 value)
{
   DENTER(TOP_LAYER);
   char buffer[2048];
   snprintf(buffer, sizeof(buffer), sge_u32, value);
   var_list_set_string(varl, name, buffer);
   DRETURN_VOID;
}

/****** sgeobj/var/var_list_set_sharedlib_path() ******************************
*  NAME
*     var_list_set_sharedlib_path -- set shared lib path
*
*  SYNOPSIS
*     void var_list_set_sharedlib_path(lList **varl);
*
*  FUNCTION
*     Sets or replaces the shared lib path in the list of variables.
*     The SGE shared lib path is always set to the beginning of the
*     resulting shared lib path 
*     (security, see var_get_sharedlib_path_name())
*
*  INPUTS
*     lList **varl - list of nment variables
*
*  SEE ALSO
*     sgeobj/var/var_get_sharedlib_path_name()
*     sgeobj/var/var_list_set_string()
*     sgeobj/var/var_list_set_int()
*     sgeobj/var/var_list_set_uint32t()
******************************************************************************/
void var_list_set_sharedlib_path(lList **varl)
{
   char *sharedlib_path;
   char *sge_sharedlib_path;
   size_t sge_sharedlib_path_size;
   const char *sge_root = sge_get_root_dir(0, nullptr, 0, 1);
   const char *sharedlib_path_name = var_get_sharedlib_path_name();
   lListElem *sharedlib_elem = nullptr;

   DENTER(TOP_LAYER);

   /* this is the SGE sharedlib path */
   sge_sharedlib_path_size = strlen(sge_root) + strlen("/lib/") + strlen(sge_get_arch()) + 1;
   sge_sharedlib_path = sge_malloc(sge_sharedlib_path_size);
   snprintf(sge_sharedlib_path, sge_sharedlib_path_size, "%s/lib/%s", sge_root, sge_get_arch());

   /* if already in environment: extend by SGE sharedlib path, else set */
   sharedlib_elem = lGetElemStrRW(*varl, VA_variable, sharedlib_path_name);
   if(sharedlib_elem != nullptr) {
      const char *old_value = lGetString(sharedlib_elem, VA_value);

      if(old_value && strlen(old_value) > 0) {
         DPRINTF("sharedlib path %s already set:\n", sharedlib_path_name);
         
         sharedlib_path = sge_malloc(strlen(old_value) + 1 + 
                          strlen(sge_sharedlib_path) + 1);
         strcpy(sharedlib_path, sge_sharedlib_path);
         strcat(sharedlib_path, ":");
         strcat(sharedlib_path, old_value);
         lSetString(sharedlib_elem, VA_value, sharedlib_path);
         sge_free(&sharedlib_path);
      } else {
         DPRINTF("overwriting empty sharedlib path %s\n", sharedlib_path_name);
         lSetString(sharedlib_elem, VA_value, sge_sharedlib_path);
      }
   } else {
      DPRINTF("creating new sharedlib path %s\n", sharedlib_path_name);
      sharedlib_elem = lAddElemStr(varl, VA_variable, sharedlib_path_name, VA_Type);
      lSetString(sharedlib_elem, VA_value, sge_sharedlib_path);
   }

   sge_free(&sge_sharedlib_path);
   DRETURN_VOID;
}

/****** sgeobj/var/var_list_dump_to_file() ************************************
*  NAME
*     var_list_dump_to_file -- dump variables from list to file
*
*  SYNOPSIS
*     void var_list_dump_to_file(const lList *varl, FILE *file);
*
*  FUNCTION
*     Parses the list of type VA_Type <varl> containing the
*     description of variables.
*     Dumps all list elements to <file> in the format:
*     <name>=<value>
*     <name> is read from VA_variable, <value> from VA_value.
*
*  INPUTS
*     varl - list of variables
*     file - filehandle of a previously opened (for writing) file
******************************************************************************/
void var_list_dump_to_file(const lList *varl, FILE *file)
{
   const lListElem *elem;

   if (varl == nullptr || file == nullptr) {
      return;
   }

   for_each_ep(elem, varl) {
      /*
       * Replace <LF> by \n sequence for multiline environment
       * variable support.
       */
      const char *env_name = lGetString(elem, VA_variable);
      const char *env_value = lGetString(elem, VA_value);
      const char *new_env_value = sge_replace_substring(env_value, "\n", "\\n");

      if (new_env_value == nullptr) {
         fprintf(file, "%s=%s\n", env_name, env_value);
      } else {
         fprintf(file, "%s=%s\n", env_name, new_env_value);
         sge_free(&new_env_value);
      }
   }
}

/****** sgeobj/var/var_list_get_string() **************************************
*  NAME
*     var_list_get_string() -- get value of certain variable 
*
*  SYNOPSIS
*     const char* var_list_get_string(lList *varl, 
*                                     const char *variable) 
*
*  FUNCTION
*     Return the string value of a variable
*     with the name "variable" which is stored in "varl". 
*
*  INPUTS
*     lList *varl          - VA_Type list 
*     const char *variable - variable name 
*
*  RESULT
*     const char* - value or nullptr
*
*  SEE ALSO
*     sgeobj/var/var_list_set_string()
*     sgeobj/var/var_list_set_int()
*     sgeobj/var/var_list_set_uint32t()
*     sgeobj/var/var_list_set_sharedlib_path()
******************************************************************************/
const char* var_list_get_string(const lList *varl, const char *variable)
{
   const char *ret = nullptr;
   const lListElem *var = lGetElemStr(varl, VA_variable, variable);

   if (var != nullptr) {
      ret = lGetString(var, VA_value);
   }
   return ret;
}

/****** sgeobj/var/var_list_copy_prefix_vars() ********************************
*  NAME
*     var_list_copy_prefix_vars() -- copy vars with certain prefix 
*
*  SYNOPSIS
*     void var_list_copy_prefix_vars(lList **varl, 
*                                    const lList *src_varl,
*                                    const char *prefix, 
*                                    const char *new_prefix) 
*
*  FUNCTION
*     Make a copy of all entries in "src_varl" 
*     beginning with "prefix". "prefix" is replaced by "new_prefix"
*     for all created elements. The new elements will be added to 
*     "varl".
*
*  INPUTS
*     lList **varl           - VA_Type list 
*     const char *prefix     - prefix string (e.g. VAR_PREFIX) 
*     const char *new_prefix - new prefix string (e.g. "SGE_") 
*
*  EXAMPLE
*     "__SGE_PREFIX__O_HOME" ===> "SGE_O_HOME 
*
*  SEE ALSO
*     sgeobj/var/var_list_remove_prefix_vars()
******************************************************************************/
void var_list_copy_prefix_vars(lList **varl, 
                               const lList *src_varl,
                               const char *prefix, 
                               const char *new_prefix)
{
   int prefix_len = strlen(prefix);
   const lListElem *var_elem = nullptr;
   lList *var_list2 = nullptr;

   DENTER(TOP_LAYER);
   for_each_ep(var_elem, src_varl) {
      const char *prefix_name = lGetString(var_elem, VA_variable);

      if (strncmp(prefix_name, prefix, prefix_len) == 0) {
         char name[MAX_STRING_SIZE];
         const char *name_without_prefix = &prefix_name[prefix_len];
         const char *value = lGetString(var_elem, VA_value);

         snprintf(name, sizeof(name), "%s%s", new_prefix, name_without_prefix);
         var_list_set_string(&var_list2, name, value);
      }
   }
   if (*varl == nullptr) {
      *varl = lCreateList("", VA_Type);
   }
   var_list_add_as_set(*varl, var_list2); 

   DRETURN_VOID;
}

/****** sgeobj/var/var_list_copy_prefix_vars_undef() **************************
*  NAME
*     var_list_copy_prefix_vars_undef() -- copy vars with certain prefix 
*
*  SYNOPSIS
*     void 
*     var_list_copy_prefix_vars_undef(lList **varl, 
*                                     const lList *src_varl,
*                                     const char *prefix, 
*                                     const char *new_prefix) 
*
*  FUNCTION
*     Make a copy of all entries in "src_varl" 
*     beginning with "prefix". "prefix" is replaced by "new_prefix"
*     for all created elements. The new elements will be added to 
*     "varl" if it is undefined in "varl".
*
*  INPUTS
*     lList **varl           - VA_Type list 
*     const char *prefix     - prefix string (e.g. VAR_PREFIX_NR) 
*     const char *new_prefix - new prefix string (e.g. "SGE_") 
*
*  EXAMPLE
*     "__SGE_PREFIX2__TASK_ID" ===> "SGE_TASK_ID 
*
*  SEE ALSO
*     sgeobj/var/var_list_remove_prefix_vars()
******************************************************************************/
void var_list_copy_prefix_vars_undef(lList **varl, 
                                     const lList *src_varl,
                                     const char *prefix, 
                                     const char *new_prefix)
{
   int prefix_len = strlen(prefix);
   const lListElem *var_elem = nullptr;
   lList *var_list2 = nullptr;

   DENTER(TOP_LAYER);
   for_each_ep(var_elem, src_varl) {
      const char *prefix_name = lGetString(var_elem, VA_variable);

      if (!strncmp(prefix_name, prefix, prefix_len)) {
         char name[MAX_STRING_SIZE];
         const char *value = lGetString(var_elem, VA_value);
         const char *name_without_prefix = &prefix_name[prefix_len];
         const lListElem *existing_variable;

         snprintf(name, sizeof(name), "%s%s", new_prefix, name_without_prefix);
         existing_variable = lGetElemStr(*varl, VA_variable, name);
         if (existing_variable == nullptr) {
            var_list_set_string(&var_list2, name, value);
         }
      }
   }
   if (*varl == nullptr) {
      *varl = lCreateList("", VA_Type);
   }
   lAddList(*varl, &var_list2);
   DRETURN_VOID;
}

/****** sgeobj/var/var_list_copy_env_vars_and_value() *************************
*  NAME
*     var_list_copy_env_vars_and_value() -- Copy env. vars 
*
*  SYNOPSIS
*     void 
*     var_list_copy_env_vars_and_value(lList **varl, 
*                                      const lList *src_varl, 
*                                      const char *ignore_prefix) 
*
*  FUNCTION
*     Copy all variables from "src_varl" into "varl". Ignore
*     all variables beginning with "ignore_prefix".
*
*  INPUTS
*     lList **varl              - VA_Type list 
*     const lList *src_varl     - source VA_Type list 
*     const char *ignore_prefix - prefix 
*
*  RESULT
*     void - none
******************************************************************************/
void var_list_copy_env_vars_and_value(lList **varl,
                                      const lList* src_varl)
{
   const lListElem *env;

   for_each_ep(env, src_varl) {
      const char *s, *name;

      name = lGetString(env, VA_variable);
      s = lGetString(env, VA_value);
      var_list_set_string(varl, name, s ? s : "");
   }
}

/****** sgeobj/var/var_list_remove_prefix_vars() ******************************
*  NAME
*     var_list_remove_prefix_vars() -- remove vars with certain prefix 
*
*  SYNOPSIS
*     void var_list_remove_prefix_vars(lList **varl, 
*                                      const char *prefix) 
*
*  FUNCTION
*     Remove all entries from "varl" where the name
*     beginns with "prefix" 
*
*  INPUTS
*     lList **varl       - VA_Type list 
*     const char *prefix - prefix string (e.g. VAR_PREFIX) 
*
*  SEE ALSO
*     sgeobj/var/var_list_remove_prefix_vars()
******************************************************************************/
void var_list_remove_prefix_vars(lList **varl, const char *prefix)
{
   int prefix_len = strlen(prefix);
   lListElem *var_elem = nullptr;
   lListElem *next_var_elem = nullptr;

   DENTER(TOP_LAYER);
   next_var_elem = lFirstRW(*varl);
   while((var_elem = next_var_elem)) {
      const char *prefix_name = lGetString(var_elem, VA_variable);
      next_var_elem = lNextRW(var_elem);

      if (!strncmp(prefix_name, prefix, prefix_len)) {
         lRemoveElem(*varl, &var_elem);
      } 
   }
   DRETURN_VOID;
}

/****** sgeobj/var/var_list_split_prefix_vars() *******************************
*  NAME
*     var_list_split_prefix_vars() -- split a list of variables 
*
*  SYNOPSIS
*     void var_list_split_prefix_vars(lList **varl, 
*                                     lList **pefix_vars, 
*                                     const char *prefix) 
*
*  FUNCTION
*     Move all variable elements from "varl" to "pefix_vars" which
*     begin with "prefix". *pefix_vars will be created if is does not
*     exist.
*
*  INPUTS
*     lList **varl        - VA_Type list 
*     lList **pefix_vars - pointer to VA_Type list 
*     const char *prefix - string (e.g. VAR_PREFIX) 
*
*  RESULT
*     void - None
*
*  SEE ALSO
*     sgeobj/var/var_list_remove_prefix_vars()
******************************************************************************/
void var_list_split_prefix_vars(lList **varl, 
                                lList **pefix_vars, 
                                const char *prefix)
{
   int prefix_len = strlen(prefix);
   lListElem *var_elem = nullptr;
   lListElem *next_var_elem = nullptr;

   DENTER(TOP_LAYER);
   next_var_elem = lFirstRW(*varl);
   while((var_elem = next_var_elem)) {
      const char *prefix_name = lGetString(var_elem, VA_variable);
      next_var_elem = lNextRW(var_elem);

      if (!strncmp(prefix_name, prefix, prefix_len)) {
         lListElem *dechained_elem = lDechainElem(*varl, var_elem);

         if (*pefix_vars == nullptr) {
            *pefix_vars = lCreateList("", VA_Type);
         }

         lAppendElem(*pefix_vars, dechained_elem);
      }
   }
   DRETURN_VOID;
}

/****** sgeobj/var/var_list_add_as_set() ***************************************
*  NAME
*     var_list_add_as_set() -- Concatenate two lists as sets
*
*  SYNOPSIS
*     int var_list_add_as_set(lList *lp0, lList *lp1) 
*
*  FUNCTION
*     Concatenate two lists of equal type throwing away the second list.
*     Elements in the second list will replace elements with the same key in the
*     the first list.  If the first list contains duplicate element keys, only
*     the first element with a given key will be replaced by an element from the
*     second list with the same key.
*
*  INPUTS
*     lList *lp0 - first list 
*     lList *lp1 - second list 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error
******************************************************************************/
int var_list_add_as_set(lList *lp0, lList *lp1) 
{
   lListElem *ep0, *ep1;
   const lDescr *dp0, *dp1;
   const char *name, *value;

   DENTER(CULL_LAYER);

   if (lp1 == nullptr || lp0 == nullptr) {
      DRETURN(-1);
   }

   /* Check if the two lists are equal */
   dp0 = lGetListDescr(lp0);
   dp1 = lGetListDescr(lp1);
   if (lCompListDescr(dp0, dp1) != 0) {
      DRETURN(-1);
   }

   while (lp1->first != nullptr) {
      /* Get the first element from the second list */
      if ((ep1 = lDechainElem(lp1, lp1->first)) == nullptr) {
         DRETURN(-1);
      }
   
      /* Get it's name, and use the name to look for a matching element in the
       * first list. */
      name = lGetString(ep1, VA_variable);
      ep0 = lGetElemStrRW(lp0, VA_variable, name);

      /* If there is a matching element in the first list, set it's value to the
       * value of the element from the second list. */
      if (ep0 != nullptr) {
         value = lGetString(ep1, VA_value);         
         lSetString(ep0, VA_value, value);
         lFreeElem(&ep1);
      }
      /* If there is no matching element, add the element from the second list
       * to the first list. */
      else {
         if (lAppendElem(lp0, ep1) == -1) {
            DRETURN(-1);
         }
      }
   }

   /* The second list is no longer needed. */
   lFreeList(&lp1);

   DRETURN(0);
}

/****** sge_var/var_list_verify() **********************************************
*  NAME
*     var_list_verify() -- verify contents of a variable list
*
*  SYNOPSIS
*     bool 
*     var_list_verify(const lList *lp, lList **answer_list) 
*
*  FUNCTION
*     Verifies the contents of a variable list.
*     Variable names may not be nullptr or empty strings.
*
*  INPUTS
*     const lList *lp     - the list to verify
*     lList **answer_list - answer list to pass back error messages
*
*  RESULT
*     bool - true on success, 
*            false in case of errors, error message in answer_list
*
*  NOTES
*     MT-NOTE: var_list_verify() is MT safe 
*******************************************************************************/
bool 
var_list_verify(const lList *lp, lList **answer_list)
{
   bool ret = true;
   const lListElem *ep;

   for_each_ep(ep, lp) {
      const char *variable = lGetString(ep, VA_variable);
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

/****** sge_var/var_list_parse_from_string() *******************************
*  NAME
*     var_list_parse_from_string() -- parse vars from string list 
*
*  SYNOPSIS
*     int var_list_parse_from_string(lList **lpp, 
*                                    const char *variable_str, 
*                                    int check_environment) 
*
*  FUNCTION
*     Parse a list of variables ("lpp") from a comma separated 
*     string list ("variable_str"). The boolean "check_environment"
*     defined wether the current value of a variable is taken from
*     the environment of the calling process.
*
*  INPUTS
*     lList **lpp              - VA_Type list 
*     const char *variable_str - source string 
*     int check_environment    - boolean
*
*  RESULT
*     int - error state
*         0 - OK
*        >0 - Error
*
*  NOTES
*     MT-NOTE: var_list_parse_from_string() is MT safe
*******************************************************************************/
int var_list_parse_from_string(lList **lpp, const char *variable_str,
                               int check_environment)
{
   char *variable;
   char *val_str;
   int var_len;
   char **str_str;
   char **pstr;
   lListElem *ep;
   char *va_string;

   DENTER(TOP_LAYER);

   if (!lpp) {
      DRETURN(1);
   }

   va_string = sge_strdup(nullptr, variable_str);
   if (!va_string) {
      *lpp = nullptr;
      DRETURN(2);
   }
   str_str = string_list(va_string, ",", nullptr);
   if (!str_str || !*str_str) {
      *lpp = nullptr;
      sge_free(&va_string);
      DRETURN(3);
   }

   if (!*lpp) {
      *lpp = lCreateList("variable list", VA_Type);
      if (!*lpp) {
         sge_free(&va_string);
         sge_free(&str_str);
         DRETURN(4);
      }
   }

   for (pstr = str_str; *pstr; pstr++) {
      struct saved_vars_s *context;
      ep = lCreateElem(VA_Type);
      /* SGE_ASSERT(ep); */
      lAppendElem(*lpp, ep);

      context = nullptr;
      variable = sge_strtok_r(*pstr, "=", &context);
      SGE_ASSERT((variable));
      var_len=strlen(variable);
      lSetString(ep, VA_variable, variable);
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
          lSetString(ep, VA_value, &val_str[var_len+1]);
      } else if (check_environment) {
         lSetString(ep, VA_value, sge_getenv(variable));
      } else {
         lSetString(ep, VA_value, nullptr);
      }
      sge_free_saved_vars(context);
   }
   sge_free(&va_string);
   sge_free(&str_str);
   DRETURN(0);
}

/****** sge_var/getenv_and_set() *******************************
*  NAME
*     getenv_and_set() -- obtain value for environment name, 
*                         remove \n characters from value string
*                         and set VA_Value member of *ep.
*
*  SYNOPSIS
*     void getenv_and_set(lListElem *ep, char *variable)
*
*  FUNCTION
*     Obtain value for environment name; check for any \n 
*     characters in input string. Shell enters this character 
*     for multi-line environment variables. If nothing found, 
*     just store value in VA_Value member of *ep.
*
*     Otherwise, count number of newline characters, figure out
*     string length and allocate new memory for the modified 
*     string. Next, copy over string, skipping over \n characters.
*     Then, store value in VA_Value member of *ep.
*
*  INPUTS
*     lListElem *ep                  - target list element
*     char *variable                 - environment variable
*
*  RESULT
*     void
*
*  NOTES
*     MT-NOTE: remove_newline_chars() is MT safe
*******************************************************************************/

void getenv_and_set(lListElem *ep, char *variable)
{
   const char *env_value = nullptr;
   char *new_env_value = nullptr;
   char *a = nullptr;
   char *b = nullptr;
   int i, nchars;
   int newline_chars = 0;

   env_value = sge_getenv(variable);
   if (env_value == nullptr) {
      lSetString(ep, VA_value, nullptr);
      return;
   }
   /*
    * Any work to do? Check for any newline
    * character.
    */
   a = strchr((char *)env_value, '\n');
   if (a == nullptr) {
      /*
       * Nothing to do. Just leave it alone.
       */
      lSetString(ep, VA_value, env_value);
      return;
   }
   /*
    * This is a multi-line environment variable. Allocate
    * new string and copy over, but without newline chars.
    */
   nchars = strlen(env_value);
   a = (char *)env_value;
   newline_chars = 0;
   for (i = 0; i < nchars; i++) {
      if (*a == '\n') {
         newline_chars++;
      }
      a++;
   }
   new_env_value = sge_malloc(nchars - newline_chars + 1);
   a = new_env_value;
   b = (char *)env_value;
   for (i = 0; i < nchars; i++) {
      if (*b != '\n') {
         *a = *b;
         a++;
      }
      b++;
   }
   *a = '\0';

   lSetString(ep, VA_value, new_env_value);
   sge_free(&new_env_value);
   return;

}

/**
* @brief remove potentially dangerous environment variables
*
* The function removes potentially dangerous environment variables from
* an environment list (e.g. from job or pe task submission).
* The following variables are removed:
*  - LD_PRELOAD
*  - all flavours of LD_LIBRARY_PATH, SHLIB_PATH etc.
*    unless qmaster param ENABLE_SUBMIT_LIB_PATH is set to TRUE
* We generate an INFO message to be output by the submit client.
*
* @param env_list       the environment list
* @param answer_list    answer_list to report removal of variables
*/
void var_list_filter_env_list(lList *env_list, lList **answer_list)
{
   lListElem *ep;

   /* LD_PRELOAD */
   ep = lGetElemStrRW(env_list, VA_variable, "LD_PRELOAD");
   if (ep != nullptr) {
      if (!mconf_get_enable_submit_ld_preload()) {
         lRemoveElem(env_list, &ep);
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_INFO,
                                 MSG_REMOVED_ENV_VAR_S, "LD_PRELOAD");
      }
   }

   /* other potentially dangerous variables */
   if (!mconf_get_enable_submit_lib_path()) {
      static const char *lib_path_names[] = {
         "LD_LIBRARY_PATH",
         "LD_LIBRARY_PATH_32",
         "LD_LIBRARY_PATH_64",
         "LIBPATH",
         "SHLIB_PATH",
         "DYLD_LIBRARY_PATH",
         "LD_ORIGIN_PATH",
         "LD_CONFIG",
         nullptr
      };
      const char *var_name;
      int i;
      for (i = 0; (var_name = lib_path_names[i]) != nullptr; i++) {
         ep = lGetElemStrRW(env_list, VA_variable, var_name);
         if (ep != nullptr) {
            lRemoveElem(env_list, &ep);
            answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_INFO,
                                    MSG_REMOVED_ENV_VAR_S, var_name);
         }
      }
   }
}
