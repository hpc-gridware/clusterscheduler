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
 *  Portions of this code are Copyright 2011 Univa Corporation.
 *
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstring>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/sge_cull_xml.h"

#include "ocs_qrstat_report_handler.h"
#include "ocs_qrstat_report_handler_xml.h"

#include "basis_types.h"
#include "msg_common.h"


namespace {
   /**
    * @brief XML-escaped copy of a string for the qrstat XML output (CS-1980).
    *
    * Values like the AR name, owner, account or resource strings are chosen by
    * users; written unescaped they could inject elements into the XML output.
    * The copy lives until the end of the full expression it is created in, so
    * it can be passed directly as an argument of sge_dstring_sprintf_append().
    */
   class XmlEscaped {
      dstring buf_ = DSTRING_INIT; ///< the escaped text
   public:
      /**
       * @brief Escape @p str
       * @param str the text to escape; nullptr becomes the empty string
       */
      explicit XmlEscaped(const char *str) {
         escape_string(str != nullptr ? str : "", &buf_);
      }
      ~XmlEscaped() {
         sge_dstring_free(&buf_);
      }
      XmlEscaped(const XmlEscaped &) = delete;
      XmlEscaped &operator=(const XmlEscaped &) = delete;

      /**
       * @brief The escaped text
       * @return the escaped string, never nullptr
       */
      const char *c_str() const {
         const char *str = sge_dstring_get_string(&buf_);
         return str != nullptr ? str : "";
      }
   };
}

static bool
qrstat_report_start(qrstat_report_handler_t* handler, lList **alpp);

static bool
qrstat_report_finish(qrstat_report_handler_t* handler, lList **alpp);

static bool
qrstat_report_start_ar(qrstat_report_handler_t* handler, qrstat_env_t *qrstat_env, lList **alpp);

static bool
qrstat_report_start_unknown_ar(qrstat_report_handler_t* handler, qrstat_env_t *qrstat_env, lList **alpp);

static bool
qrstat_report_finish_ar(qrstat_report_handler_t* handler, lList **alpp);

static bool
qrstat_report_finish_unknown_ar(qrstat_report_handler_t* handler, lList **alpp);

static bool
qrstat_report_ar_node_ulong(qrstat_report_handler_t* handler, qrstat_env_t *qrstat_env, lList **alpp,
                                const char *name, u_long32 value);

static bool
qrstat_report_ar_node_duration(qrstat_report_handler_t* handler, lList **alpp,
                               const char *name, u_long64 value);

static bool
qrstat_report_ar_node_string(qrstat_report_handler_t* handler, lList **alpp,
                             const char *name, const char *value);

static bool
qrstat_report_ar_node_time(qrstat_report_handler_t* handler, lList **alpp,
                           const char *name, u_long64 value);

static bool
qrstat_report_ar_node_state(qrstat_report_handler_t* handler, lList **alpp,
                                const char *name, u_long32 value);

static bool
qrstat_report_start_resource_list(qrstat_report_handler_t* handler, lList **alpp);

static bool
qrstat_report_finish_resource_list(qrstat_report_handler_t* handler, lList **alpp);

static bool
qrstat_report_resource_list_node(qrstat_report_handler_t* handler, lList **alpp,
                                     const char *name, const char *value);
static bool
qrstat_report_ar_node_boolean(qrstat_report_handler_t* handler, lList **alpp,
                                     const char *name, bool value);
static bool
qrstat_report_start_exec_queue_list(qrstat_report_handler_t* handler, lList **alpp);

static bool
qrstat_report_finish_exec_queue_list(qrstat_report_handler_t* handler, lList **alpp);

static bool
qrstat_report_exec_queue_list_node(qrstat_report_handler_t* handler, lList **alpp, const char *name, u_long32 value);

static bool
qrstat_report_start_exec_binding_list(qrstat_report_handler_t* handler, lList **alpp);

static bool
qrstat_report_finish_exec_binding_list(qrstat_report_handler_t* handler, lList **alpp);

static bool
qrstat_report_exec_binding_list_node(qrstat_report_handler_t* handler, lList **alpp, const char *name, const char *value);

static bool
qrstat_report_start_granted_resource_list(qrstat_report_handler_t* handler, lList **alpp);

static bool
qrstat_report_finish_granted_resource_list(qrstat_report_handler_t* handler, lList **alpp);

static bool
qrstat_report_granted_resource_list_node(qrstat_report_handler_t* handler, lList **alpp,
                                         const char *host, const char *value);

static bool
qrstat_report_start_granted_parallel_environment(qrstat_report_handler_t* handler, lList **alpp);

static bool
qrstat_report_finish_granted_parallel_environment(qrstat_report_handler_t* handler, lList **alpp);

static bool
qrstat_report_granted_parallel_environment_node(qrstat_report_handler_t* handler,
                                                    lList **alpp,
                                                    const char *name, const char *slots_range);
static bool
qrstat_report_start_mail_list(qrstat_report_handler_t* handler, lList **alpp);

static bool
qrstat_report_finish_mail_list(qrstat_report_handler_t* handler, lList **alpp);

static bool
qrstat_report_mail_list_node(qrstat_report_handler_t* handler,
                             lList **alpp,
                             const char *name, const char *hostname);

static bool
qrstat_report_start_acl_list(qrstat_report_handler_t* handler, lList **alpp);

static bool
qrstat_report_finish_acl_list(qrstat_report_handler_t* handler, lList **alpp);

static bool
qrstat_report_acl_list_node(qrstat_report_handler_t* handler,
                            lList **alpp, const char *name);

static bool
qrstat_report_start_xacl_list(qrstat_report_handler_t* handler, lList **alpp);

static bool
qrstat_report_finish_xacl_list(qrstat_report_handler_t* handler, lList **alpp);

static bool
qrstat_report_xacl_list_node(qrstat_report_handler_t* handler,
                             lList **alpp, const char *name);

static bool
qrstat_report_newline(qrstat_report_handler_t* handler, lList **alpp);


qrstat_report_handler_t *
qrstat_create_report_handler_xml(qrstat_env_t *qrstat_env, lList **answer_list)
{
   qrstat_report_handler_t* ret = nullptr;

   DENTER(TOP_LAYER);

   ret = (qrstat_report_handler_t*)sge_malloc(sizeof(qrstat_report_handler_t));
   if (ret == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EMALLOC, ANSWER_QUALITY_ERROR,
                              MSG_MEM_MEMORYALLOCFAILED_S, __func__);
   } else {
     /*
      * xml report handler ctx is a dstring
      */
      ret->ctx = sge_malloc(sizeof(dstring));
      if (ret->ctx == nullptr ) {
         answer_list_add_sprintf(answer_list, STATUS_EMALLOC, ANSWER_QUALITY_ERROR,
                                 MSG_MEM_MEMORYALLOCFAILED_S, __func__);
      } else {
         /*
          * corresponds to initializing with DSTRING_INIT
          */
         memset(ret->ctx, 0, sizeof(dstring));

         ret->show_summary = qrstat_env->is_summary;

         ret->report_start = qrstat_report_start;
         ret->report_finish = qrstat_report_finish;
         ret->report_start_ar = qrstat_report_start_ar;
         ret->report_start_unknown_ar = qrstat_report_start_unknown_ar;
         ret->report_finish_ar = qrstat_report_finish_ar;
         ret->report_finish_unknown_ar = qrstat_report_finish_unknown_ar;
         ret->report_ar_node_ulong = qrstat_report_ar_node_ulong;
         ret->report_ar_node_ulong_unknown = qrstat_report_ar_node_ulong;
         ret->report_ar_node_duration = qrstat_report_ar_node_duration;
         ret->report_ar_node_string = qrstat_report_ar_node_string;
         ret->report_ar_node_time = qrstat_report_ar_node_time;
         ret->report_ar_node_state = qrstat_report_ar_node_state;

         ret->report_start_resource_list = qrstat_report_start_resource_list;
         ret->report_finish_resource_list = qrstat_report_finish_resource_list;
         ret->report_resource_list_node = qrstat_report_resource_list_node;

         ret->report_ar_node_boolean = qrstat_report_ar_node_boolean;

         ret->report_start_exec_queue_list = qrstat_report_start_exec_queue_list;
         ret->report_finish_exec_queue_list = qrstat_report_finish_exec_queue_list;
         ret->report_exec_queue_list_node = qrstat_report_exec_queue_list_node;

         ret->report_start_exec_binding_list = qrstat_report_start_exec_binding_list;
         ret->report_finish_exec_binding_list = qrstat_report_finish_exec_binding_list;
         ret->report_exec_binding_list_node = qrstat_report_exec_binding_list_node;

         ret->report_start_granted_resource_list = qrstat_report_start_granted_resource_list;
         ret->report_finish_granted_resource_list = qrstat_report_finish_granted_resource_list;
         ret->report_granted_resource_list_node = qrstat_report_granted_resource_list_node;

         ret->report_start_granted_parallel_environment = qrstat_report_start_granted_parallel_environment;
         ret->report_finish_granted_parallel_environment = qrstat_report_finish_granted_parallel_environment;
         ret->report_granted_parallel_environment_node = qrstat_report_granted_parallel_environment_node;

         ret->report_start_mail_list = qrstat_report_start_mail_list;
         ret->report_finish_mail_list = qrstat_report_finish_mail_list;
         ret->report_mail_list_node = qrstat_report_mail_list_node;

         ret->report_start_acl_list = qrstat_report_start_acl_list;
         ret->report_finish_acl_list = qrstat_report_finish_acl_list;
         ret->report_acl_list_node = qrstat_report_acl_list_node;

         ret->report_start_xacl_list = qrstat_report_start_xacl_list;
         ret->report_finish_xacl_list = qrstat_report_finish_xacl_list;
         ret->report_xacl_list_node = qrstat_report_xacl_list_node;
         ret->report_newline = qrstat_report_newline;
      }
   }

   DRETURN(ret);
}

bool
qrstat_destroy_report_handler_xml(qrstat_report_handler_t** handler, lList **answer_list)
{
   bool ret = true;

   DENTER(TOP_LAYER);

   if (handler != nullptr && *handler != nullptr ) {
      sge_dstring_free((dstring*)(*handler)->ctx);
      sge_free(&((*handler)->ctx));
      sge_free(handler);
   }

   DRETURN(ret);
}

static bool
qrstat_report_start(qrstat_report_handler_t* handler, lList **alpp) 
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);

   sge_dstring_append(buffer, "<?xml version='1.0'?>\n");
   sge_dstring_append(buffer, "<qrstat xmlns:xsd=\"https://github.com/hpc-gridware/clusterscheduler/raw/master/source/dist/util/resources/schemas/qrstat/qrstat.xsd\">\n");

   DRETURN(ret); 
}

static bool
qrstat_report_finish(qrstat_report_handler_t* handler, lList **alpp)
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);
  
   sge_dstring_append(buffer, "</qrstat>\n");
   printf("%s", sge_dstring_get_string((dstring *)handler->ctx));

   DRETURN(ret); 
}

static bool
qrstat_report_start_ar(qrstat_report_handler_t* handler, qrstat_env_t *qrstat_env, lList **alpp) 
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);

   sge_dstring_append(buffer, "   <ar_summary>\n");
     
   DRETURN(ret); 
}

static bool
qrstat_report_start_unknown_ar(qrstat_report_handler_t* handler, qrstat_env_t *qrstat_env, lList **alpp) 
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);

   sge_dstring_append(buffer, "   <ar_unknown>\n");
     
   DRETURN(ret); 
}

static bool
qrstat_report_finish_ar(qrstat_report_handler_t* handler, lList **alpp)
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);
  
   sge_dstring_append(buffer, "   </ar_summary>\n");

   DRETURN(ret); 
}

static bool
qrstat_report_finish_unknown_ar(qrstat_report_handler_t* handler, lList **alpp)
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);
  
   sge_dstring_append(buffer, "   </ar_unknown>\n");

   DRETURN(ret); 
}

static bool
qrstat_report_ar_node_ulong(qrstat_report_handler_t* handler, qrstat_env_t *qrstat_env, lList **alpp,
                            const char *name, u_long32 value)
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);
  
   sge_dstring_sprintf_append(buffer, "      <" SFN ">" sge_u32 "</" SFN ">\n", name, value, name);

   DRETURN(ret); 
}

static bool
qrstat_report_ar_node_duration(qrstat_report_handler_t* handler, lList **alpp,
                               const char *name, u_long64 value)
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;
   u_long32 value32 = sge_gmt64_to_gmt32(value);
   int seconds = value32 % 60;
   int minutes = ((value32 - seconds) / 60) % 60;
   int hours = ((value32 - seconds - minutes * 60) / 3600);

   DENTER(TOP_LAYER);
  
   sge_dstring_sprintf_append(buffer, "      <" SFN ">%02d:%02d:%02d</" SFN ">\n",
                              name, hours, minutes, seconds, name);

   DRETURN(ret); 
}

/**
 * @brief Write an AR attribute with a string value as XML element.
 *
 * @param handler qrstat report handler, its context is the output buffer
 * @param alpp    answer list, unused
 * @param name element name
 * @param value the value, XML-escaped (CS-1980); nullptr gives an empty element
 * @return true
 */
static bool
qrstat_report_ar_node_string(qrstat_report_handler_t* handler, lList **alpp,
                                 const char *name, const char *value)
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);
 
   if (value != nullptr) {
      sge_dstring_sprintf_append(buffer, "      <" SFN ">" SFN "</" SFN ">\n", name, XmlEscaped(value).c_str(), name);
   } else {
      sge_dstring_sprintf_append(buffer, "      <" SFN "/>\n", name);
   }

   DRETURN(ret); 
} 

static bool
qrstat_report_ar_node_time(qrstat_report_handler_t* handler, lList **alpp,
                               const char *name, u_long64 value)
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;
   DSTRING_STATIC(time_string, 64);

   DENTER(TOP_LAYER);
 
   append_time(value, &time_string, true);
   sge_dstring_sprintf_append(buffer, "      <" SFN ">" SFN "</" SFN ">\n",
                              name, sge_dstring_get_string(&time_string), name);

   DRETURN(ret); 
} 
 
static bool
qrstat_report_ar_node_state(qrstat_report_handler_t* handler, lList **alpp,
                                const char *name, u_long32 state)
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;
   dstring state_string = DSTRING_INIT;

   DENTER(TOP_LAYER);
 
   ar_state2dstring((ar_state_t)state, &state_string);
   sge_dstring_sprintf_append(buffer, "      <" SFN ">" SFN "</" SFN ">\n",
                              name, sge_dstring_get_string(&state_string), name);
   sge_dstring_free(&state_string);

   DRETURN(ret); 
} 

static bool
qrstat_report_start_resource_list(qrstat_report_handler_t* handler, lList **alpp) 
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);

   sge_dstring_append(buffer, "      <resource_list>\n");
     
   DRETURN(ret); 
}

static bool
qrstat_report_finish_resource_list(qrstat_report_handler_t* handler, lList **alpp)
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);
  
   sge_dstring_append(buffer, "      </resource_list>\n");

   DRETURN(ret); 
}

/**
 * @brief Write one requested resource of an AR as XML element.
 *
 * @param handler qrstat report handler, its context is the output buffer
 * @param alpp    answer list, unused
 * @param name resource name, XML-escaped (CS-1980)
 * @param value requested value, XML-escaped (CS-1980)
 * @return true
 */
static bool
qrstat_report_resource_list_node(qrstat_report_handler_t* handler, lList **alpp,
                                     const char *name, const char *value)
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);
  
   sge_dstring_sprintf_append(buffer, "         <resource name=" SFQ " type=" SFQ "/>\n",
                              XmlEscaped(name).c_str(), XmlEscaped(value).c_str());

   DRETURN(ret); 
}


static bool
qrstat_report_ar_node_boolean(qrstat_report_handler_t* handler, lList **alpp, const char *name, bool value)
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);
  
   sge_dstring_sprintf_append(buffer,"      <" SFN ">" SFN "</" SFN ">\n",
                              name, value ? "true":"false", name);

   DRETURN(ret); 
}

static bool
qrstat_report_start_exec_queue_list(qrstat_report_handler_t* handler, lList **alpp)
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);

   sge_dstring_append(buffer, "      <exec_queue_list>\n");
     
   DRETURN(ret); 
}

static bool
qrstat_report_finish_exec_queue_list(qrstat_report_handler_t* handler, lList **alpp)
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);
  
   sge_dstring_append(buffer, "      </exec_queue_list>\n");

   DRETURN(ret); 
}

/**
 * @brief Write one queue instance of an AR with its slots as XML element.
 *
 * @param handler qrstat report handler, its context is the output buffer
 * @param alpp    answer list, unused
 * @param name queue instance name, XML-escaped (CS-1980)
 * @param value number of slots
 * @return true
 */
static bool
qrstat_report_exec_queue_list_node(qrstat_report_handler_t* handler, lList **alpp, const char *name, u_long32 value)
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);
  
   sge_dstring_sprintf_append(buffer, "         <exec_queue><queue_instance>" SFN
                              "</queue_instance><slots>" sge_u32 "</slots></exec_queue>\n", XmlEscaped(name).c_str(), value);

   DRETURN(ret); 
}

static bool
qrstat_report_start_granted_resource_list(qrstat_report_handler_t* handler, lList **alpp)
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);
   sge_dstring_append(buffer, "      <granted_resources_list>\n");
   DRETURN(ret);
}

static bool
qrstat_report_finish_granted_resource_list(qrstat_report_handler_t* handler, lList **alpp)
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);
   sge_dstring_append(buffer, "      </granted_resources_list>\n");
   DRETURN(ret);
}

/**
 * @brief Write one granted resource of an AR as XML element.
 *
 * @param handler qrstat report handler, its context is the output buffer
 * @param alpp    answer list, unused
 * @param host execution host, XML-escaped (CS-1980)
 * @param value granted resource, XML-escaped (CS-1980)
 * @return true
 */
static bool
qrstat_report_granted_resource_list_node(qrstat_report_handler_t* handler, lList **alpp,
                                         const char *host, const char *value)
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);
   sge_dstring_sprintf_append(buffer, "         <granted_resource><exec_host>" SFN
                              "</exec_host><resource>" SFN "</resource></granted_resource>\n",
                              XmlEscaped(host).c_str(), XmlEscaped(value).c_str());
   DRETURN(ret);
}

static bool
qrstat_report_start_exec_binding_list(qrstat_report_handler_t* handler, lList **alpp)
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);

   sge_dstring_append(buffer, "      <exec_binding_list>\n");

   DRETURN(ret);
}

static bool
qrstat_report_finish_exec_binding_list(qrstat_report_handler_t* handler, lList **alpp)
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);

   sge_dstring_append(buffer, "      </exec_binding_list>\n");

   DRETURN(ret);
}

/**
 * @brief Write the binding of an AR on one host as XML element.
 *
 * @param handler qrstat report handler, its context is the output buffer
 * @param alpp    answer list, unused
 * @param name execution host, XML-escaped (CS-1980)
 * @param value binding, XML-escaped (CS-1980)
 * @return true
 */
static bool
qrstat_report_exec_binding_list_node(qrstat_report_handler_t* handler, lList **alpp, const char *name, const char *value)
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);

   sge_dstring_sprintf_append(buffer, "         <exec_binding><exec_host>" SFN
                              "</exec_host><binding>" SFN "</binding></exec_binding>\n", XmlEscaped(name).c_str(), XmlEscaped(value).c_str());

   DRETURN(ret);
}

static bool
qrstat_report_start_granted_parallel_environment(qrstat_report_handler_t* handler, lList **alpp) 
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);

   sge_dstring_append(buffer, "      <granted_parallel_environment>\n");
     
   DRETURN(ret); 
}

static bool
qrstat_report_finish_granted_parallel_environment(qrstat_report_handler_t* handler, lList **alpp)
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);
  
   sge_dstring_append(buffer, "      </granted_parallel_environment>\n");

   DRETURN(ret); 
}

/**
 * @brief Write the granted parallel environment of an AR as XML elements.
 *
 * @param handler qrstat report handler, its context is the output buffer
 * @param alpp    answer list, unused
 * @param name PE name, XML-escaped (CS-1980)
 * @param slots_range slot range, XML-escaped (CS-1980)
 * @return true
 */
static bool
qrstat_report_granted_parallel_environment_node(qrstat_report_handler_t* handler, 
                                                    lList **alpp,
                                                    const char *name, const char *slots_range)
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);
  
   sge_dstring_sprintf_append(buffer, "         <parallel_environment>" SFN "</parallel_environment>\n", XmlEscaped(name).c_str());
   sge_dstring_sprintf_append(buffer, "         <slots>" SFN "</slots>\n", XmlEscaped(slots_range).c_str());

   DRETURN(ret); 
}

static bool 
qrstat_report_start_mail_list(qrstat_report_handler_t* handler, lList **alpp) 
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);

   sge_dstring_append(buffer, "      <mail_list>\n");
     
   DRETURN(ret); 
}

static bool
qrstat_report_finish_mail_list(qrstat_report_handler_t* handler, lList **alpp)
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);
  
   sge_dstring_append(buffer, "      </mail_list>\n");

   DRETURN(ret); 
}

/**
 * @brief Write one mail recipient of an AR as XML element.
 *
 * @param handler qrstat report handler, its context is the output buffer
 * @param alpp    answer list, unused
 * @param name user name, XML-escaped (CS-1980)
 * @param host host name, XML-escaped (CS-1980); nullptr is written as "nullptr"
 * @return true
 */
static bool
qrstat_report_mail_list_node(qrstat_report_handler_t* handler, 
                                 lList **alpp,
                                 const char *name, const char *host)
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);
  
   sge_dstring_sprintf_append(buffer, "         <mail  user=" SFQ
                              " host=" SFQ "/>\n", XmlEscaped(name).c_str(), XmlEscaped(host ? host : "nullptr").c_str());

   DRETURN(ret); 
}

static bool
qrstat_report_start_acl_list(qrstat_report_handler_t* handler, lList **alpp) 
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);

   sge_dstring_append(buffer, "      <acl_list>\n");
     
   DRETURN(ret); 
}

static bool
qrstat_report_finish_acl_list(qrstat_report_handler_t* handler, lList **alpp)
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);
  
   sge_dstring_append(buffer, "      </acl_list>\n");

   DRETURN(ret); 
}

/**
 * @brief Write one entry of the AR access list as XML element.
 *
 * @param handler qrstat report handler, its context is the output buffer
 * @param alpp    answer list, unused
 * @param name user or userset name, XML-escaped (CS-1980)
 * @return true
 */
static bool
qrstat_report_acl_list_node(qrstat_report_handler_t* handler, 
                            lList **alpp,
                            const char *name)
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);
  
   sge_dstring_sprintf_append(buffer, "         <acl  user=" SFQ "/>\n", XmlEscaped(name).c_str());

   DRETURN(ret); 
}

static bool 
qrstat_report_start_xacl_list(qrstat_report_handler_t* handler, lList **alpp) 
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);

   sge_dstring_append(buffer, "      <xacl_list>\n");
     
   DRETURN(ret); 
}

static bool
qrstat_report_finish_xacl_list(qrstat_report_handler_t* handler, lList **alpp)
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);
  
   sge_dstring_append(buffer, "      </xacl_list>\n");

   DRETURN(ret); 
}

/**
 * @brief Write one entry of the AR exclusion list as XML element.
 *
 * @param handler qrstat report handler, its context is the output buffer
 * @param alpp    answer list, unused
 * @param name user or userset name, XML-escaped (CS-1980)
 * @return true
 */
static bool
qrstat_report_xacl_list_node(qrstat_report_handler_t* handler, 
                             lList **alpp,
                             const char *name)
{
   bool ret = true;
   dstring *buffer = (dstring*)handler->ctx;

   DENTER(TOP_LAYER);
  
   sge_dstring_sprintf_append(buffer, "         <acl  user=" SFQ "/>\n", XmlEscaped(name).c_str());

   DRETURN(ret); 
}
 
static bool
qrstat_report_newline(qrstat_report_handler_t* handler, lList **alpp)
{
   return true;
}
