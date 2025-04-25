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

#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_advance_reservation.h"

#include "ocs_qrstat_filter.h"
#include "ocs_qrstat_report_handler.h"
#include "basis_types.h"
#include "msg_common.h"

#define SFN_FIRST_COLUMN "%-30.30s"

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
qrstat_report_ar_node_ulong_unknown(qrstat_report_handler_t* handler, qrstat_env_t *qrstat_env, lList **alpp,
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
qrstat_report_start_granted_slots_list(qrstat_report_handler_t* handler, lList **alpp);

static bool
qrstat_report_finish_granted_slots_list(qrstat_report_handler_t* handler, lList **alpp);

static bool
qrstat_report_granted_slots_list_node(qrstat_report_handler_t* handler,
                                          lList **alpp,
                                          const char *name, u_long32 value);

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
qrstat_create_report_handler_stdout(qrstat_env_t *qrstat_env, 
                                    lList **answer_list)
{
   qrstat_report_handler_t* ret = nullptr;

   DENTER(TOP_LAYER);

   ret = (qrstat_report_handler_t*)sge_malloc(sizeof(qrstat_report_handler_t));
   if (ret == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EMALLOC, ANSWER_QUALITY_ERROR,
                              MSG_MEM_MEMORYALLOCFAILED_S, __func__);
   } else {
     /*
      * report handler ctx is stdout 
      */
      ret->ctx = stdout;

      ret->show_summary = qrstat_env->is_summary;

      ret->report_start = qrstat_report_start;
      ret->report_finish = qrstat_report_finish;
      ret->report_start_ar = qrstat_report_start_ar;
      ret->report_start_unknown_ar = qrstat_report_start_unknown_ar;
      ret->report_finish_ar = qrstat_report_finish_ar;
      ret->report_finish_unknown_ar = qrstat_report_finish_unknown_ar;
      ret->report_ar_node_ulong = qrstat_report_ar_node_ulong;
      ret->report_ar_node_ulong_unknown = qrstat_report_ar_node_ulong_unknown;
      ret->report_ar_node_duration = qrstat_report_ar_node_duration;
      ret->report_ar_node_string = qrstat_report_ar_node_string;
      ret->report_ar_node_time = qrstat_report_ar_node_time;
      ret->report_ar_node_state = qrstat_report_ar_node_state;

      ret->report_start_resource_list = qrstat_report_start_resource_list;
      ret->report_finish_resource_list = qrstat_report_finish_resource_list;
      ret->report_resource_list_node = qrstat_report_resource_list_node;
      
      ret->report_ar_node_boolean = qrstat_report_ar_node_boolean;

      ret->report_start_granted_slots_list = qrstat_report_start_granted_slots_list;
      ret->report_finish_granted_slots_list = qrstat_report_finish_granted_slots_list;
      ret->report_granted_slots_list_node = qrstat_report_granted_slots_list_node;

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

   DRETURN(ret);
}

bool
qrstat_destroy_report_handler_stdout(qrstat_report_handler_t** handler, lList **answer_list)
{
   bool ret = true;

   DENTER(TOP_LAYER);

   if (handler != nullptr && *handler != nullptr ) {
      sge_free(handler);
   }

   DRETURN(ret);
}

static bool
qrstat_report_start(qrstat_report_handler_t* handler, lList **alpp) 
{
   bool ret = true;

   DENTER(TOP_LAYER);
   DRETURN(ret); 
}

static bool
qrstat_report_finish(qrstat_report_handler_t* handler, lList **alpp)
{
   bool ret = true;

   DENTER(TOP_LAYER);
   DRETURN(ret); 
}

static bool
qrstat_report_start_ar(qrstat_report_handler_t* handler, qrstat_env_t *qrstat_env, lList **alpp) 
{
   bool ret = true;
   FILE *out = (FILE*)handler->ctx;

   DENTER(TOP_LAYER);

   if (!handler->show_summary) {
      fprintf(out, "----------");
      fprintf(out, "----------");
      fprintf(out, "----------");
      fprintf(out, "----------");
      fprintf(out, "----------");
      fprintf(out, "----------");
      fprintf(out, "----------");
      fprintf(out, "----------\n");
   } else if (!qrstat_env->header_printed) {
      const char *head_format = "%-7.7s %-10.10s %-12.12s %-5.5s %-20.20s %-20.20s %8s\n";

      fprintf(out, head_format, "ar-id", "name", "owner", "state", "start at", 
              "end at", "duration");
      fprintf(out, "----------------------------------------"
                 "--------------------------------------------------\n");
      qrstat_env->header_printed = true;
   }

   DRETURN(ret); 
}

static bool
qrstat_report_start_unknown_ar(qrstat_report_handler_t* handler, qrstat_env_t *qrstat_env, lList **alpp) 
{
   bool ret = true;
   FILE *out = (FILE*)handler->ctx;

   DENTER(TOP_LAYER);

   if (!qrstat_env->header_printed) {
      fprintf(out, "Following advance reservations do not exist:\n");
   }

   DRETURN(ret); 
}

static bool
qrstat_report_finish_ar(qrstat_report_handler_t* handler, lList **alpp)
{
   bool ret = true;
   FILE *out = (FILE*)handler->ctx;

   DENTER(TOP_LAYER);
   if (handler->show_summary) {
      fprintf(out, "\n");
   }
   DRETURN(ret); 
}

static bool
qrstat_report_finish_unknown_ar(qrstat_report_handler_t* handler, lList **alpp)
{
   bool ret = true;

   DENTER(TOP_LAYER);
   DRETURN(ret); 
}

static bool
qrstat_report_ar_node_ulong(qrstat_report_handler_t* handler, qrstat_env_t *qrstat_env, lList **alpp,
                            const char *name, u_long32 value)
{
   bool ret = true;
   FILE *out = (FILE*)handler->ctx;

   DENTER(TOP_LAYER);
   if (handler->show_summary) {
      fprintf(out, "%7" sge_u32_letter " ", value);
   } else {
      fprintf(out, SFN_FIRST_COLUMN" " sge_u32 "\n", name, value);
   }
   DRETURN(ret); 
}

static bool
qrstat_report_ar_node_ulong_unknown(qrstat_report_handler_t* handler, qrstat_env_t *qrstat_env, lList **alpp,
                            const char *name, u_long32 value)
{
   bool ret = true;
   FILE *out = (FILE*)handler->ctx;

   DENTER(TOP_LAYER);
   if (qrstat_env->header_printed) {
      fprintf(out, ", ");
   } else {
      qrstat_env->header_printed = true;
   }
   fprintf(out, sge_u32, value);

   DRETURN(ret); 
}

static bool
qrstat_report_ar_node_duration(qrstat_report_handler_t* handler, lList **alpp,
                               const char *name, u_long64 value)
{
   bool ret = true;
   FILE *out = (FILE*)handler->ctx;
   u_long32 value32 = sge_gmt64_to_gmt32(value);
   int seconds = value32 % 60;
   int minutes = ((value32 - seconds) / 60) % 60;
   int hours = ((value32 - seconds - minutes * 60) / 3600);

   DENTER(TOP_LAYER);

   if (handler->show_summary) {
      fprintf(out, "%02d:%02d:%02d", hours, minutes, seconds); 
   } else {
      fprintf(out, SFN_FIRST_COLUMN" %02d:%02d:%02d\n", name, hours, minutes, seconds);  
   }

   DRETURN(ret);
}

static bool
qrstat_report_ar_node_string(qrstat_report_handler_t* handler, lList **alpp,
                             const char *name, const char *value)
{
   bool ret = true;
   FILE *out = (FILE*)handler->ctx;

   DENTER(TOP_LAYER);
   if (value == nullptr) {
      value = "";
   }
   if (handler->show_summary) {
      if (strcmp("owner", name) == 0) {
         fprintf(out, "%-12.12s ", value);
      } else if (strcmp("name", name) == 0) {
         fprintf(out, "%-10.10s ", value);
      } else if (strcmp("message", name) == 0) {
         fprintf(out, "\n       " SFN, value);
      }
   } else {
      fprintf(out, SFN_FIRST_COLUMN" " SFN "\n", name, value);
   }
   DRETURN(ret); 
} 

static bool
qrstat_report_ar_node_time(qrstat_report_handler_t* handler, lList **alpp,
                           const char *name, u_long64 value)
{
   bool ret = true;
   FILE *out = (FILE*)handler->ctx;
   DSTRING_STATIC(time_string, 64);

   DENTER(TOP_LAYER);
 
   if (handler->show_summary) {
      if (strcmp("start_time", name) == 0 || strcmp("end_time", name) == 0) {
         sge_ctime64_short(value, &time_string);
         fprintf(out, "%-20.20s ", sge_dstring_get_string(&time_string));
      }
   } else {
      sge_ctime64(value, &time_string);
      fprintf(out, SFN_FIRST_COLUMN" " SFN "\n", name, sge_dstring_get_string(&time_string));
   }

   DRETURN(ret); 
} 
 
static bool
qrstat_report_ar_node_state(qrstat_report_handler_t* handler, lList **alpp,
                            const char *name, u_long32 state)
{
   bool ret = true;
   FILE *out = (FILE*)handler->ctx;
   dstring state_string = DSTRING_INIT;

   DENTER(TOP_LAYER);
 
   ar_state2dstring((ar_state_t)state, &state_string);
   if (handler->show_summary) {
      fprintf(out, "%-5.5s ", sge_dstring_get_string(&state_string));
   } else {
      fprintf(out, SFN_FIRST_COLUMN" " SFN "\n", name, sge_dstring_get_string(&state_string));
   }
   sge_dstring_free(&state_string);

   DRETURN(ret); 
} 

static bool
qrstat_report_start_resource_list(qrstat_report_handler_t* handler, lList **alpp) 
{
   bool ret = true;
   FILE *out = (FILE*)handler->ctx;

   DENTER(TOP_LAYER);
   if (!handler->show_summary) {
      fprintf(out, SFN_FIRST_COLUMN" ", "resource_list");
      handler->first_resource = true;
   }
   DRETURN(ret); 
}

static bool
qrstat_report_finish_resource_list(qrstat_report_handler_t* handler, lList **alpp)
{
   bool ret = true;
   FILE *out = (FILE*)handler->ctx;

   DENTER(TOP_LAYER);
   if (!handler->show_summary) {
      fprintf(out, "\n");
   }
   DRETURN(ret); 
}

static bool
qrstat_report_resource_list_node(qrstat_report_handler_t* handler, lList **alpp,
                                 const char *name, const char *value)
{
   bool ret = true;
   FILE *out = (FILE*)handler->ctx;

   DENTER(TOP_LAYER);
   if (!handler->show_summary) {
      fprintf(out, SFN SFN "=" SFN, (handler->first_resource ? "" : ", "), name, value);
      if (handler->first_resource) {
         handler->first_resource = false;
      } 
   }
   DRETURN(ret); 
}

static bool
qrstat_report_ar_node_boolean(qrstat_report_handler_t* handler, lList **alpp, const char *name, bool value)
{
   bool ret = true;
   FILE *out = (FILE*)handler->ctx;
   const char* chvalue = value ? "true":"false";

   DENTER(TOP_LAYER);
   if (handler->show_summary) {
      fprintf(out, "       " SFN, chvalue);
   } else {
      fprintf(out, SFN_FIRST_COLUMN" " SFN "\n", name, chvalue);
   }
   DRETURN(ret); 

}


static bool
qrstat_report_start_granted_slots_list(qrstat_report_handler_t* handler, lList **alpp) 
{
   bool ret = true;
   FILE *out = (FILE*)handler->ctx;

   DENTER(TOP_LAYER);
   if (!handler->show_summary) {
      fprintf(out, SFN_FIRST_COLUMN" ", "granted_slots_list");
      handler->first_granted_slot = true;
   }
   DRETURN(ret); 
}

static bool
qrstat_report_finish_granted_slots_list(qrstat_report_handler_t* handler, lList **alpp)
{
   bool ret = true;
   FILE *out = (FILE*)handler->ctx;

   DENTER(TOP_LAYER);
   if (!handler->show_summary) {
      fprintf(out, "\n");
   }
   DRETURN(ret); 
}

static bool
qrstat_report_granted_slots_list_node(qrstat_report_handler_t* handler, 
                                      lList **alpp,
                                      const char *name, u_long32 value)
{
   bool ret = true;
   FILE *out = (FILE*)handler->ctx;

   DENTER(TOP_LAYER);
   if (!handler->show_summary) {
      fprintf(out, SFN SFN "=" sge_u32, (handler->first_granted_slot ? "" : ","), name, value);
      if (handler->first_granted_slot) {
         handler->first_granted_slot = false;
      } 
   }
   DRETURN(ret); 
}
 
static bool
qrstat_report_start_granted_parallel_environment(qrstat_report_handler_t* handler, lList **alpp) 
{
   bool ret = true;
   FILE *out = (FILE*)handler->ctx;

   DENTER(TOP_LAYER);
   if (!handler->show_summary) {
      fprintf(out, SFN_FIRST_COLUMN" ", "granted_parallel_environment");
   }
   DRETURN(ret); 
}

static bool
qrstat_report_finish_granted_parallel_environment(qrstat_report_handler_t* handler, lList **alpp)
{
   bool ret = true;
   FILE *out = (FILE*)handler->ctx;

   DENTER(TOP_LAYER);
   if (!handler->show_summary) {
      fprintf(out, "\n");
   }
   DRETURN(ret); 
}

static bool
qrstat_report_granted_parallel_environment_node(qrstat_report_handler_t* handler, 
                                                lList **alpp,
                                                const char *name, const char *slots_range)
{
   bool ret = true;
   FILE *out = (FILE*)handler->ctx;

   DENTER(TOP_LAYER);
   if (!handler->show_summary) {
      fprintf(out, SFN " slots " SFN, name, slots_range);
   }
   DRETURN(ret); 
}

static bool 
qrstat_report_start_mail_list(qrstat_report_handler_t* handler, lList **alpp) 
{
   bool ret = true;
   FILE *out = (FILE*)handler->ctx;

   DENTER(TOP_LAYER);
   if (!handler->show_summary) {
      fprintf(out, SFN_FIRST_COLUMN" ", "mail_list");
      handler->first_mail = true;
   }
   DRETURN(ret); 
}

static bool
qrstat_report_finish_mail_list(qrstat_report_handler_t* handler, lList **alpp)
{
   bool ret = true;
   FILE *out = (FILE*)handler->ctx;

   DENTER(TOP_LAYER);
   if (!handler->show_summary) {
      fprintf(out, "\n");
   }
   DRETURN(ret); 
}

static bool
qrstat_report_mail_list_node(qrstat_report_handler_t* handler, 
                             lList **alpp,
                             const char *name, const char *host)
{
   bool ret = true;
   FILE *out = (FILE*)handler->ctx;

   DENTER(TOP_LAYER);
   if (!handler->show_summary) {
      fprintf(out, SFN SFN "@" SFN, (handler->first_mail ? "" : ","), name?name:"", host?host:"");
      if (handler->first_mail) {
         handler->first_mail = false;
      } 
   }
   DRETURN(ret); 
}

static bool
qrstat_report_start_acl_list(qrstat_report_handler_t* handler, lList **alpp) 
{
   bool ret = true;
   FILE *out = (FILE*)handler->ctx;

   DENTER(TOP_LAYER);
   if (!handler->show_summary) {
      fprintf(out, SFN_FIRST_COLUMN" ", "acl_list");
      handler->first_acl = true;
   }
   DRETURN(ret); 
}

static bool
qrstat_report_finish_acl_list(qrstat_report_handler_t* handler, lList **alpp)
{
   bool ret = true;
   FILE *out = (FILE*)handler->ctx;

   DENTER(TOP_LAYER);
   if (!handler->show_summary) {
      fprintf(out, "\n");
   }
   DRETURN(ret); 
}

static bool
qrstat_report_acl_list_node(qrstat_report_handler_t* handler, 
                            lList **alpp,
                            const char *name)
{
   bool ret = true;
   FILE *out = (FILE*)handler->ctx;

   DENTER(TOP_LAYER);
   if (!handler->show_summary) {
      fprintf(out, SFN SFN, (handler->first_acl ? "" : ","), name);
      if (handler->first_acl) {
         handler->first_acl = false;
      } 
   }
   DRETURN(ret); 
}

static bool 
qrstat_report_start_xacl_list(qrstat_report_handler_t* handler, lList **alpp) 
{
   bool ret = true;
   FILE *out = (FILE*)handler->ctx;

   DENTER(TOP_LAYER);
   if (!handler->show_summary) {
      fprintf(out, SFN_FIRST_COLUMN" ", "xacl_list");
      handler->first_xacl = true;
   }
   DRETURN(ret); 
}

static bool
qrstat_report_finish_xacl_list(qrstat_report_handler_t* handler, lList **alpp)
{
   bool ret = true;
   FILE *out = (FILE*)handler->ctx;

   DENTER(TOP_LAYER);
   if (!handler->show_summary) {
      fprintf(out, "\n");
   }
   DRETURN(ret); 
}

static bool
qrstat_report_xacl_list_node(qrstat_report_handler_t* handler, 
                             lList **alpp,
                             const char *name)
{
   bool ret = true;
   FILE *out = (FILE*)handler->ctx;

   DENTER(TOP_LAYER);
   if (!handler->show_summary) {
      fprintf(out, SFN SFN, (handler->first_xacl ? "" : ","), name);
      if (handler->first_xacl) {
         handler->first_xacl = false;
      } 
   } 
   DRETURN(ret); 
}
 
static bool
qrstat_report_newline(qrstat_report_handler_t* handler, lList **alpp)
{
   bool ret = true;
   FILE *out = (FILE*)handler->ctx;

   DENTER(TOP_LAYER);
   fprintf(out, "\n");
   DRETURN(ret);
}
