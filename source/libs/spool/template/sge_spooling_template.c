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

/** @file
 * @brief A skeleton backend to copy when writing a new one
 */

#include "uti/sge_rmon_macros.h"
#include "uti/sge_log.h"
#include "uti/sge_dstring.h"

#include "sgeobj/sge_object.h"

#include "spool/msg_spoollib.h"
#include "spool/template/msg_spoollib_template.h"
#include "spool/template/sge_spooling_template.h"

#include "msg_common.h"

static const char *spooling_method = "template";

/** @brief Report `"template"` as this library's spooling method
 *
 * @return the string `"template"`
 */
#ifdef SPOOLING_template
const char *get_spooling_method(void)
#else
const char *get_template_spooling_method(void)
#endif
{
   return spooling_method;
}


/**
 * @brief Create a template spooling context
 *
 * Create a spooling context for the template spooling.
 *
 * @param answer_list to return error messages
 * @param args the method specific argument string
 *
 * @return on success, the new spooling context, else nullptr
 *
 * @warning The body is empty and always returns nullptr - this is the
 *          skeleton to copy, not a working backend. Fill it in with the
 *          #spool_create_context, #spool_context_create_rule and
 *          #spool_context_create_type calls the real backends make.
 */
lListElem *
spool_template_create_context(lList **answer_list, const char *args) {
   DENTER(TOP_LAYER);

   lListElem *context = nullptr;


   DRETURN(context);
}

/**
 * @brief Setup
 *
 * @param answer_list to return error messages
 * @param rule the rule containing data necessary for the startup (e.g. path to the spool directory)
 * @param check check the spooling database
 *
 * @return true, if the startup succeeded, else false
 *
 * @note This function should not be called directly, it is called by the
 *       spooling framework.
 *
 * @see `spool_startup_context()`
 */
bool spool_template_default_startup_func(lList **answer_list,
                                         const lListElem *rule, bool check) {
   DENTER(TOP_LAYER);

   bool ret = true;
   const char *url;

   url = lGetString(rule, SPR_url);

   DRETURN(ret);
}

/**
 * @brief Shutdown spooling context
 *
 * Shuts down the context, e.g. the database connection.
 *
 * @param answer_list to return error messages
 * @param rule the rule containing data necessary for the shutdown (e.g. path to the spool directory)
 *
 * @return true, if the shutdown succeeded, else false
 *
 * @note This function should not be called directly, it is called by the
 *       spooling framework.
 *
 * @see `spool_shutdown_context()`
 */
bool spool_template_default_shutdown_func(lList **answer_list,
                                          const lListElem *rule) {
   DENTER(TOP_LAYER);

   bool ret = true;
   const char *url;

   url = lGetString(rule, SPR_url);


   DRETURN(ret);
}

/**
 * @brief Maintain database
 *
 * Maintains the database:
 *    - initialization
 *    - ...
 *
 * @param answer_list to return error messages
 * @param rule the rule containing data necessary for the maintenance (e.g. path to the spool directory)
 * @param cmd the command to execute
 * @param args arguments to the maintenance command
 *
 * @return true, if the maintenance succeeded, else false
 *
 * @note This function should not be called directly, it is called by the
 *       spooling framework.
 *
 * @see `spool_maintain_context()`
 */
bool spool_template_default_maintenance_func(lList **answer_list,
                                             const lListElem *rule,
                                             const spooling_maintenance_command cmd,
                                             const char *args) {
   DENTER(TOP_LAYER);

   bool ret = true;

   switch (cmd) {
      case SPM_init:
         break;
      default:
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                 ANSWER_QUALITY_WARNING, 
                                 "unknown maintenance command %d\n", cmd);
         ret = false;
         break;
         
   }

   DRETURN(ret);
}

/**
 * @brief Read lists through template spooling
 *
 * @param answer_list to return error messages
 * @param type object type description
 * @param rule rule to be used
 * @param list target list
 * @param object_type object type
 *
 * @return true, on success, else false
 *
 * @note This function should not be called directly, it is called by the
 *       spooling framework.
 *
 * @see `spool_read_list()`
 */
bool spool_template_default_list_func(lList **answer_list,
                                      const lListElem *type,
                                      const lListElem *rule, lList **list,
                                      const sge_object_type object_type) {
   DENTER(TOP_LAYER);

   bool ret = true;

   switch (object_type) {
      default:
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                 ANSWER_QUALITY_WARNING, 
                                 MSG_SPOOL_SPOOLINGOFXNOTSUPPORTED_S, 
                                 object_type_get_name(object_type));
         ret = false;
         break;
   }

   ret = spool_default_validate_list_func(answer_list, type, rule, object_type);

   DRETURN(ret);
}

/**
 * @brief Read objects through template spooling
 *
 * @param answer_list to return error messages
 * @param type object type description
 * @param rule rule to use
 * @param key unique key specifying the object
 * @param object_type object type
 *
 * @return the object, if it could be read, else nullptr
 *
 * @note This function should not be called directly, it is called by the
 *       spooling framework.
 *
 * @see `spool_read_object()`
 */
lListElem *
spool_template_default_read_func(lList **answer_list,
                                 const lListElem *type,
                                 const lListElem *rule, const char *key,
                                 const sge_object_type object_type) {
   DENTER(TOP_LAYER);

   lListElem *ep = nullptr;

   switch (object_type) {
      default:
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                 ANSWER_QUALITY_WARNING, 
                                 MSG_SPOOL_SPOOLINGOFXNOTSUPPORTED_S, 
                                 object_type_get_name(object_type));
         break;
   }

   DRETURN(ep);
}

/**
 * @brief Write objects through template spooling
 *
 * Writes an object through the appropriate template spooling functions.
 *
 * @param answer_list to return error messages
 * @param type object type description
 * @param rule rule to use
 * @param object object to spool
 * @param key unique key
 * @param object_type object type
 *
 * @return true on success, else false
 *
 * @note This function should not be called directly, it is called by the
 *       spooling framework.
 *
 * @see `spool_delete_object()`
 */
bool spool_template_default_write_func(lList **answer_list,
                                       const lListElem *type,
                                       const lListElem *rule,
                                       const lListElem *object,
                                       const char *key,
                                       const sge_object_type object_type) {
   DENTER(TOP_LAYER);

   bool ret = true;

   switch (object_type) {
      default:
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                 ANSWER_QUALITY_WARNING, 
                                 MSG_SPOOL_SPOOLINGOFXNOTSUPPORTED_S, 
                                 object_type_get_name(object_type));
         ret = false;
         break;
   }

   DRETURN(ret);
}

/**
 * @brief Delete object in template spooling
 *
 * Deletes an object in the template spooling.
 *
 * @param answer_list to return error messages
 * @param type object type description
 * @param rule rule to use
 * @param key unique key
 * @param object_type object type
 *
 * @return true on success, else false
 *
 * @note This function should not be called directly, it is called by the
 *       spooling framework.
 *
 * @see `spool_delete_object()`
 */
bool spool_template_default_delete_func(lList **answer_list,
                                        const lListElem *type,
                                        const lListElem *rule,
                                        const char *key,
                                        const sge_object_type object_type) {
   DENTER(TOP_LAYER);

   bool ret = true;

   switch (object_type) {
      default:
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                 ANSWER_QUALITY_WARNING, 
                                 MSG_SPOOL_SPOOLINGOFXNOTSUPPORTED_S, 
                                 object_type_get_name(object_type));
         ret = false;
         break;
   }

   DRETURN(ret);
}
