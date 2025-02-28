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
#include <strings.h>
#include <cctype>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "sgeobj/cull/sge_resource_utilization_RUE_L.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_attr.h"
#include "sgeobj/sge_calendar.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_feature.h"
#include "sgeobj/sge_href.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_resource_quota.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_sharetree.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_subordinate.h"
#include "sgeobj/sge_usage.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_mailrec.h"

#include "spool/flatfile/sge_flatfile.h"
#include "spool/flatfile/sge_flatfile_obj_rsmap.h"
#include "spool/flatfile/msg_spoollib_flatfile.h"

#include "msg_common.h"

/* This file defines variables and functions that are used to create field
 * lists to pass to the flatfile spooling framework.  The reason that some
 * field lists are represented as structs and some are represented as
 * functions is that some field lists are always that same, while others
 * change depending on the context from which they're called. */

/* read_func's and write_func's -- signature is defined in
 * sge_spooling_utilities.h:spooling_field. */
static void create_spooling_field (
   spooling_field *field,
   int nm, 
   int width, 
   const char *name,
   bool free_name,
   struct spooling_field *sub_fields,
   bool free_sub_fields,
   const void *clientdata, 
   int (*read_func) (lListElem *ep, int nm, const char *buffer, lList **alp), 
   int (*write_func) (const lListElem *ep, int nm, dstring *buffer, lList **alp)
);
static int read_SC_queue_sort_method(lListElem *ep, int nm, const char *buffer, lList **alp);
static int write_SC_queue_sort_method(const lListElem *ep, int nm, dstring *buffer, lList **alp);
static int read_CF_value(lListElem *ep, int nm, const char *buf, lList **alp);
static int read_CQ_ulng_attr_list(lListElem *ep, int nm, const char *buffer, lList **alp);
static int write_CQ_ulng_attr_list(const lListElem *ep, int nm, dstring *buffer, lList **alp);
static int read_CQ_mem_attr_list(lListElem *ep, int nm, const char *buffer, lList **alp);
static int write_CQ_mem_attr_list(const lListElem *ep, int nm, dstring *buffer, lList **alp);
static int read_CQ_time_attr_list(lListElem *ep, int nm, const char *buffer, lList **alp);
static int write_CQ_time_attr_list(const lListElem *ep, int nm, dstring *buffer, lList **alp);
static int read_CQ_prjlist_attr_list(lListElem *ep, int nm, const char *buffer, lList **alp);
static int write_CQ_prjlist_attr_list(const lListElem *ep, int nm, dstring *buffer, lList **alp);
static int read_CQ_solist_attr_list(lListElem *ep, int nm, const char *buffer, lList **alp);
static int write_CQ_solist_attr_list(const lListElem *ep, int nm, dstring *buffer, lList **alp);
static int read_CQ_usrlist_attr_list(lListElem *ep, int nm, const char *buffer, lList **alp);
static int write_CQ_usrlist_attr_list(const lListElem *ep, int nm, dstring *buffer, lList **alp);
static int read_CQ_bool_attr_list(lListElem *ep, int nm, const char *buffer, lList **alp);
static int write_CQ_bool_attr_list(const lListElem *ep, int nm, dstring *buffer, lList **alp);
static int read_CQ_strlist_attr_list(lListElem *ep, int nm, const char *buffer, lList **alp);
static int write_CQ_strlist_attr_list(const lListElem *ep, int nm, dstring *buffer, lList **alp);
static int read_CQ_qtlist_attr_list(lListElem *ep, int nm, const char *buffer, lList **alp);
static int write_CQ_qtlist_attr_list(const lListElem *ep, int nm, dstring *buffer, lList **alp);
static int read_CQ_str_attr_list(lListElem *ep, int nm, const char *buffer, lList **alp);
static int write_CQ_str_attr_list(const lListElem *ep, int nm, dstring *buffer, lList **alp);
static int read_CQ_inter_attr_list(lListElem *ep, int nm, const char *buffer, lList **alp);
static int write_CQ_inter_attr_list(const lListElem *ep, int nm, dstring *buffer, lList **alp);
static int read_CQ_celist_attr_list(lListElem *ep, int nm, const char *buffer, lList **alp);
static int write_CQ_celist_attr_list(const lListElem *ep, int nm, dstring *buffer, lList **alp);
static int read_CQ_hostlist(lListElem *ep, int nm, const char *buffer, lList **alp);
static int write_CQ_hostlist(const lListElem *ep, int nm, dstring *buffer, lList **alp);
static int write_CE_stringval(const lListElem *ep, int nm, dstring *buffer,
                       lList **alp);
static int read_RQR_obj(lListElem *ep, int nm, const char *buffer,
                                    lList **alp);
static int write_RQR_obj(const lListElem *ep, int nm, dstring *buffer,
                       lList **alp);

/* Field lists for context-independent spooling of sub-lists */
static spooling_field AMEM_sub_fields[] = {
   {  AMEM_href,           0, nullptr,                false, nullptr, false, nullptr, nullptr},
   {  AMEM_value,          0, nullptr,                false, nullptr, false, nullptr, nullptr},
   {  NoName,              0, nullptr,                false, nullptr, false, nullptr, nullptr}
};

static spooling_field ATIME_sub_fields[] = {
   {  ATIME_href,          0, nullptr,                false, nullptr, false, nullptr, nullptr},
   {  ATIME_value,         0, nullptr,                false, nullptr,false,  nullptr, nullptr},
   {  NoName,              0, nullptr,                false, nullptr, false, nullptr, nullptr}
};

static spooling_field PR_sub_fields[] = {
   {  PR_name,             0, nullptr,                false, nullptr, false, nullptr, nullptr},
   {  NoName,              0, nullptr,                false, nullptr, false, nullptr, nullptr}
};

static spooling_field APRJLIST_sub_fields[] = {
   {  APRJLIST_href,       0, nullptr,                false, nullptr, false, nullptr, nullptr},
   {  APRJLIST_value,      0, nullptr,                false, PR_sub_fields, false, nullptr, nullptr},
   {  NoName,              0, nullptr,                false, nullptr, false, nullptr, nullptr}
};

static spooling_field SO_sub_fields[] = {
   {  SO_name,            11, nullptr, false, nullptr, false, nullptr, nullptr, nullptr},
   {  SO_threshold,       11, nullptr, false, nullptr, false, nullptr, nullptr, nullptr},
   {  SO_slots_sum,       11, nullptr, false, nullptr, false, nullptr, nullptr, nullptr},
   {  SO_seq_no,          11, nullptr, false, nullptr, false, nullptr, nullptr, nullptr},
   {  SO_action,          11, nullptr, false, nullptr, false, nullptr, nullptr, nullptr},
   {  NoName,             11, nullptr, false, nullptr, false, nullptr, nullptr, nullptr}
};

static spooling_field ASOLIST_sub_fields[] = {
   {  ASOLIST_href,        0, nullptr,                false, nullptr, false, nullptr, nullptr},
   {  ASOLIST_value,       0, nullptr,                false, SO_sub_fields, false, nullptr, nullptr},
   {  NoName,              0, nullptr,                false, nullptr, false, nullptr, nullptr}
};

static spooling_field US_sub_fields[] = {
   {  US_name,             0, nullptr,                false, nullptr, false, nullptr, nullptr},
   {  NoName,              0, nullptr,                false, nullptr, false, nullptr, nullptr}
};

static spooling_field AUSRLIST_sub_fields[] = {
   {  AUSRLIST_href,       0, nullptr,                false, nullptr, false, nullptr, nullptr},
   {  AUSRLIST_value,      0, nullptr,                false, US_sub_fields, false, nullptr, nullptr},
   {  NoName,              0, nullptr,                false, nullptr, false, nullptr, nullptr}
};

static spooling_field ABOOL_sub_fields[] = {
   {  ABOOL_href,          0, nullptr,                false, nullptr, false, nullptr, nullptr},
   {  ABOOL_value,         0, nullptr,                false, nullptr, false, nullptr, nullptr},
   {  NoName,              0, nullptr,                false, nullptr, false, nullptr, nullptr}
};

static spooling_field ST_sub_fields[] = {
   {  ST_name,             0, nullptr,                false, nullptr, false, nullptr, nullptr},
   {  NoName,              0, nullptr,                false, nullptr, false, nullptr, nullptr}
};

static spooling_field ASTRLIST_sub_fields[] = {
   {  ASTRLIST_href,       0, nullptr,                false, nullptr, false, nullptr, nullptr},
   {  ASTRLIST_value,      0, nullptr,                false, ST_sub_fields, false, nullptr, nullptr},
   {  NoName,              0, nullptr,                false, nullptr, false, nullptr, nullptr}
};

static spooling_field AQTLIST_sub_fields[] = {
   {  AQTLIST_href,        0, nullptr,                false, nullptr, false, nullptr, nullptr},
   {  AQTLIST_value,       0, nullptr,                false, nullptr, false, nullptr, nullptr},
   {  NoName,              0, nullptr,                false, nullptr, false, nullptr, nullptr}
};

static spooling_field ASTR_sub_fields[] = {
   {  ASTR_href,           0, nullptr,                false, nullptr, false, nullptr, nullptr},
   {  ASTR_value,          0, nullptr,                false, nullptr, false, nullptr, nullptr},
   {  NoName,              0, nullptr,                false, nullptr, false, nullptr, nullptr}
};

static spooling_field AINTER_sub_fields[] = {
   {  AINTER_href,         0, nullptr,                false, nullptr, false, nullptr, nullptr},
   {  AINTER_value,        0, nullptr,                false, nullptr, false, nullptr, nullptr},
   {  NoName,              0, nullptr,                false, nullptr, false, nullptr, nullptr}
};

static spooling_field CE_sub_fields[] = {
   {  CE_name,            11, "name",    false, nullptr, false, nullptr, nullptr, nullptr},
   {  CE_stringval,       11, "value",   false, nullptr, false, nullptr, nullptr, write_CE_stringval},
   {  NoName,             11, nullptr,      false, nullptr,false,  nullptr, nullptr, nullptr}
};

/* in order to distinguish between host level and queue level consumables */
static spooling_field CE_host_sub_fields[] = {
   {  CE_name,            11, "name",    false, nullptr, false, nullptr, nullptr, nullptr},
   {  CE_stringval,       11, "value",   false, nullptr, false, nullptr, read_CE_stringval_host, write_CE_stringval_host},
   {  NoName,             11, nullptr,      false, nullptr, false, nullptr, nullptr, nullptr}
};

static spooling_field RUE_sub_fields[] = {
   {  RUE_name,           11, nullptr,   false, nullptr, false, nullptr, nullptr, nullptr},
   {  RUE_utilized_now,   11, nullptr,   false, nullptr, false, nullptr, nullptr, nullptr},
   {  NoName,             11, nullptr,   false, nullptr, false, nullptr, nullptr, nullptr}
};

static spooling_field ACELIST_sub_fields[] = {
   {  ACELIST_href,        0, nullptr, false, nullptr, false, nullptr, nullptr, nullptr},
   {  ACELIST_value,       0, nullptr, false, CE_sub_fields, false, nullptr, nullptr, nullptr},
   {  NoName,              0, nullptr, false, nullptr, false, nullptr, nullptr, nullptr}
};

static spooling_field AULNG_sub_fields[] = {
   {  AULNG_href,          0, nullptr, false, nullptr, false, nullptr, nullptr, nullptr},
   {  AULNG_value,         0, nullptr, false, nullptr, false, nullptr, nullptr, nullptr},
   {  NoName,              0, nullptr, false, nullptr, false, nullptr, nullptr, nullptr}
};

static spooling_field CF_sub_fields[] = {
   {  CF_name,             28, nullptr, false, nullptr, false, nullptr, nullptr, nullptr},
   {  CF_value,            28, nullptr, false, nullptr, false, nullptr, read_CF_value, nullptr},
   {  NoName,              28, nullptr, false, nullptr, false, nullptr, nullptr, nullptr}
};

static spooling_field STN_sub_fields[] = {
   {  STN_id,              0, nullptr, false, nullptr, false, nullptr, nullptr, nullptr},
   {  NoName,              0, nullptr,false,  nullptr, false, nullptr, nullptr, nullptr}
};

static spooling_field HS_sub_fields[] = {
   {  HS_name,             0, nullptr, false, nullptr, false, nullptr, nullptr, nullptr},
   {  HS_value,            0, nullptr, false, nullptr, false, nullptr, nullptr, nullptr},
   {  NoName,              0, nullptr, false, nullptr, false, nullptr, nullptr, nullptr}
};

static spooling_field RU_sub_fields[] = {
   {  RU_job_number,         0, nullptr, false, nullptr, false, nullptr, nullptr, nullptr},
   {  RU_task_number,        0, nullptr, false, nullptr, false, nullptr, nullptr, nullptr},
   {  RU_state,              0, nullptr, false, nullptr, false, nullptr, nullptr, nullptr},
   {  NoName,                0, nullptr, false, nullptr, false, nullptr, nullptr, nullptr}
};

static spooling_field HL_sub_fields[] = {
   {  HL_name,             0, nullptr, false, nullptr, false, nullptr, nullptr, nullptr},
   {  HL_value,            0, nullptr, false, nullptr, false, nullptr, nullptr, nullptr},
   {  NoName,              0, nullptr, false, nullptr, false, nullptr, nullptr, nullptr}
};

static spooling_field STU_sub_fields[] = {
   {  STU_name,            0, nullptr, false, nullptr, false, nullptr, nullptr, nullptr},
   {  NoName,              0, nullptr, false, nullptr, false, nullptr, nullptr, nullptr}
};

static spooling_field UE_sub_fields[] = {
   {  UE_name,             0, nullptr, false, nullptr, false, nullptr, nullptr, nullptr},
   {  NoName,              0, nullptr, false, nullptr, false, nullptr, nullptr, nullptr}
};

static spooling_field UA_sub_fields[] = {
   {  UA_name,             0, nullptr,                false, nullptr, false, nullptr, nullptr, nullptr},
   {  UA_value,            0, nullptr,                false, nullptr, false, nullptr, nullptr, nullptr},
   {  NoName,              0, nullptr,                false, nullptr, false, nullptr, nullptr, nullptr}
};

static spooling_field UPP_sub_fields[] = {
   {  UPP_name,            0, nullptr,                false, nullptr, false, nullptr, nullptr, nullptr},
   {  UPP_usage,           0, nullptr,                false, UA_sub_fields, false, nullptr, nullptr, nullptr},
   {  UPP_long_term_usage, 0, nullptr,                false, UA_sub_fields, false, nullptr, nullptr, nullptr},
   {  NoName,              0, nullptr,                false, nullptr, false, nullptr, nullptr, nullptr}
};

static spooling_field UPU_sub_fields[] = {
   {  UPU_job_number,      0, nullptr,                false, nullptr, false, nullptr, nullptr, nullptr},
   {  UPU_old_usage_list,  0, nullptr,                false, UA_sub_fields, false, &qconf_sub_name_value_comma_sfi, nullptr, nullptr},
   {  NoName,              0, nullptr,                false, nullptr, false, nullptr, nullptr, nullptr}
};

static spooling_field HR_sub_fields[] = {
   {  HR_name,             0, nullptr,                false, nullptr, false, nullptr, nullptr, nullptr},
   {  NoName,              0, nullptr,                false, nullptr, false, nullptr, nullptr, nullptr}
};

static spooling_field RQRL_sub_fields[] = {
   {  RQRL_name,           0, nullptr,                false, nullptr, false, nullptr, nullptr, nullptr},
   {  RQRL_value,          0, nullptr,                false, nullptr, false, nullptr, nullptr, nullptr},
   {  NoName,              0, nullptr,                false, nullptr, false, nullptr, nullptr, nullptr}
};

static spooling_field RQR_sub_fields[] = {
   {  RQR_name,            0, "name",              false, nullptr, false, nullptr, nullptr, nullptr},
   {  RQR_filter_users,    0, "users",             false, nullptr, false, nullptr, read_RQR_obj, write_RQR_obj},
   {  RQR_filter_projects, 0, "projects",          false, nullptr, false, nullptr, read_RQR_obj, write_RQR_obj},
   {  RQR_filter_pes,      0, "pes",               false, nullptr, false, nullptr, read_RQR_obj, write_RQR_obj},
   {  RQR_filter_queues,   0, "queues",            false, nullptr,false,  nullptr, read_RQR_obj, write_RQR_obj},
   {  RQR_filter_hosts,    0, "hosts",             false, nullptr, false, nullptr, read_RQR_obj, write_RQR_obj},
   {  RQR_limit,           0, "to",                false, RQRL_sub_fields,  false, &qconf_sub_name_value_comma_sfi, nullptr, nullptr},
   {  NoName,              0, nullptr,                false, nullptr, false, nullptr, nullptr, nullptr}
};

/* Field lists for context-independent object spooling */
spooling_field CE_fields[] = {
   {  CE_name,            11, "name",          false, nullptr, false, nullptr, nullptr, nullptr},
   {  CE_shortcut,        11, "shortcut",      false, nullptr, false, nullptr, nullptr, nullptr},
   {  CE_valtype,         11, "type",          false, nullptr, false, nullptr, nullptr, nullptr},
   {  CE_relop,           11, "relop",         false, nullptr, false, nullptr, nullptr, nullptr},
   {  CE_requestable,     11, "requestable",   false, nullptr, false, nullptr, nullptr, nullptr},
   {  CE_consumable,      11, "consumable",    false, nullptr, false, nullptr, nullptr, nullptr},
   {  CE_defaultval,      11, "default",       false, nullptr,false,  nullptr, nullptr, nullptr},
   {  CE_urgency_weight,  11, "urgency",      false,  nullptr, false, nullptr, nullptr, nullptr},
   {  NoName,             11, nullptr,            false, nullptr, false, nullptr, nullptr, nullptr}
};

spooling_field CAL_fields[] = {
   {  CAL_name,           16, "calendar_name", false, nullptr, false, nullptr, nullptr, nullptr},
   {  CAL_year_calendar,  16, "year",          false, nullptr, false, nullptr, nullptr, nullptr},
   {  CAL_week_calendar,  16, "week",          false, nullptr, false, nullptr, nullptr, nullptr},
   {  NoName,             16, nullptr,            false, nullptr, false, nullptr, nullptr, nullptr}
};

spooling_field CK_fields[] = {
   {  CK_name,            18, "ckpt_name",        false, nullptr, false, nullptr, nullptr},
   {  CK_interface,       18, "interface",        false, nullptr, false, nullptr, nullptr},
   {  CK_ckpt_command,    18, "ckpt_command",     false, nullptr, false, nullptr, nullptr},
   {  CK_migr_command,    18, "migr_command",     false, nullptr, false, nullptr, nullptr},
   {  CK_rest_command,    18, "restart_command",  false, nullptr, false, nullptr, nullptr},
   {  CK_clean_command,   18, "clean_command",    false, nullptr, false, nullptr, nullptr},
   {  CK_ckpt_dir,        18, "ckpt_dir",         false, nullptr, false, nullptr, nullptr},
   {  CK_signal,          18, "signal",           false, nullptr, false, nullptr, nullptr},
   {  CK_when,            18, "when",            false,  nullptr, false, nullptr, nullptr},
   {  NoName,             18, nullptr,               false, nullptr, false, nullptr, nullptr}
};

spooling_field HGRP_fields[] = {
   {  HGRP_name,           0, "group_name",        false, nullptr, false, nullptr, nullptr},
   {  HGRP_host_list,      0, "hostlist",          false, HR_sub_fields, false, nullptr, nullptr},
   {  NoName,              0, nullptr,                false, nullptr, false, nullptr, nullptr}
};

spooling_field US_fields[] = {
   {  US_name,    7, "name",    false, nullptr,          false, nullptr, nullptr, nullptr},
   {  US_type,    7, "type",    false, nullptr,          false, nullptr, nullptr, nullptr},
   {  US_fshare,  7, "fshare",  false, US_sub_fields, false, nullptr, nullptr, nullptr},
   {  US_oticket, 7, "oticket", false, US_sub_fields, false, nullptr, nullptr, nullptr},
   {  US_entries, 7, "entries", false, UE_sub_fields,false,  nullptr, nullptr, nullptr},
   {  NoName,     7, nullptr,      false, nullptr,         false,  nullptr, nullptr, nullptr}
};
   
spooling_field SC_fields[] = {
   {  SC_algorithm,                       33, "algorithm",                       false, nullptr,          false, nullptr, nullptr,                      nullptr},
   {  SC_schedule_interval,               33, "schedule_interval",               false, nullptr,          false, nullptr, nullptr,                      nullptr},
   {  SC_maxujobs,                        33, "maxujobs",                       false,  nullptr,         false,  nullptr, nullptr,                      nullptr},
   {  SC_queue_sort_method,               33, "queue_sort_method",               false, nullptr,          false, nullptr, read_SC_queue_sort_method, write_SC_queue_sort_method},
   {  SC_job_load_adjustments,            33, "job_load_adjustments",           false,  CE_sub_fields, false, nullptr, nullptr,                      nullptr},
   {  SC_load_adjustment_decay_time,      33, "load_adjustment_decay_time",      false, CE_sub_fields, false, nullptr, nullptr,                      nullptr},
   {  SC_load_formula,                    33, "load_formula",                    false, nullptr,          false, nullptr, nullptr,                      nullptr},
   {  SC_schedd_job_info,                 33, "schedd_job_info",                 false, nullptr,          false, nullptr, nullptr,                      nullptr},
   {  SC_flush_submit_sec,                33, "flush_submit_sec",                false, nullptr,          false, nullptr, nullptr,                      nullptr},
   {  SC_flush_finish_sec,                33, "flush_finish_sec",                false, nullptr,         false,  nullptr, nullptr,                      nullptr},
   {  SC_params,                          33, "params",                          false, nullptr,         false,  nullptr, nullptr,                      nullptr},
   {  SC_reprioritize_interval,           33, "reprioritize_interval",           false, nullptr,         false,  nullptr, nullptr,                      nullptr},
   {  SC_halftime,                        33, "halftime",                       false,  nullptr,          false, nullptr, nullptr,                      nullptr},
   {  SC_usage_weight_list,               33, "usage_weight_list",               false, UA_sub_fields, false, nullptr, nullptr,                      nullptr},
   {  SC_compensation_factor,             33, "compensation_factor",             false, nullptr,         false,  nullptr, nullptr,                      nullptr},
   {  SC_weight_user,                     33, "weight_user",                     false, nullptr,          false, nullptr, nullptr,                      nullptr},
   {  SC_weight_project,                  33, "weight_project",                  false, nullptr,         false,  nullptr, nullptr,                      nullptr},
   {  SC_weight_department,               33, "weight_department",               false, nullptr,          false, nullptr, nullptr,                      nullptr},
   {  SC_weight_job,                      33, "weight_job",                      false, nullptr,          false, nullptr, nullptr,                      nullptr},
   {  SC_weight_tickets_functional,       33, "weight_tickets_functional",       false, nullptr,          false, nullptr, nullptr,                      nullptr},
   {  SC_weight_tickets_share,            33, "weight_tickets_share",            false, nullptr,          false, nullptr, nullptr,                      nullptr},
   {  SC_share_override_tickets,          33, "share_override_tickets",          false, nullptr,          false, nullptr, nullptr,                      nullptr},
   {  SC_share_functional_shares,         33, "share_functional_shares",         false, nullptr,         false,  nullptr, nullptr,                      nullptr},
   {  SC_max_functional_jobs_to_schedule, 33, "max_functional_jobs_to_schedule", false, nullptr,          false, nullptr, nullptr,                      nullptr},
   {  SC_report_pjob_tickets,             33, "report_pjob_tickets",             false, nullptr,          false, nullptr, nullptr,                      nullptr},
   {  SC_max_pending_tasks_per_job,       33, "max_pending_tasks_per_job",       false, nullptr,          false, nullptr, nullptr,                      nullptr},
   {  SC_halflife_decay_list,             33, "halflife_decay_list",            false,  nullptr,         false,  nullptr, nullptr,                      nullptr},
   {  SC_policy_hierarchy,                33, "policy_hierarchy",                false, nullptr,          false, nullptr, nullptr,                      nullptr},
   {  SC_weight_ticket,                   33, "weight_ticket",                   false, nullptr,          false, nullptr, nullptr,                      nullptr},
   {  SC_weight_waiting_time,             33, "weight_waiting_time",             false, nullptr,          false, nullptr, nullptr,                      nullptr},
   {  SC_weight_deadline,                 33, "weight_deadline",                 false, nullptr,          false, nullptr, nullptr,                      nullptr},
   {  SC_weight_urgency,                  33, "weight_urgency",                  false, nullptr,          false, nullptr, nullptr,                      nullptr},
   {  SC_weight_priority,                 33, "weight_priority",                 false, nullptr,          false, nullptr, nullptr,                      nullptr},
   {  SC_max_reservation,                 33, "max_reservation",                false,  nullptr,          false, nullptr, nullptr,                      nullptr},
   {  SC_default_duration,                33, "default_duration",                false, nullptr,          false, nullptr, nullptr,                      nullptr},
   {  NoName,                             33, nullptr,                              false, nullptr,          false, nullptr, nullptr,                      nullptr}
};

spooling_field CQ_fields[] = {
   {  CQ_name,                   21, "qname",              false, nullptr,                false, nullptr,                                   nullptr,                      nullptr},
   {  CQ_hostlist,               21, "hostlist",           false, HR_sub_fields,       false, nullptr,                                   read_CQ_hostlist,          write_CQ_hostlist},
   {  CQ_seq_no,                 21, "seq_no",             false, AULNG_sub_fields,    false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_ulng_attr_list,    write_CQ_ulng_attr_list},
   {  CQ_load_thresholds,        21, "load_thresholds",    false, ACELIST_sub_fields,  false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_celist_attr_list,  write_CQ_celist_attr_list},
   {  CQ_suspend_thresholds,     21, "suspend_thresholds", false, ACELIST_sub_fields,  false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_celist_attr_list,  write_CQ_celist_attr_list},
   {  CQ_nsuspend,               21, "nsuspend",           false, AULNG_sub_fields,    false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_ulng_attr_list,    write_CQ_ulng_attr_list},
   {  CQ_suspend_interval,       21, "suspend_interval",   false, AINTER_sub_fields,   false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_inter_attr_list,   write_CQ_inter_attr_list},
   {  CQ_priority,               21, "priority",           false, ASTR_sub_fields,     false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_str_attr_list,     write_CQ_str_attr_list},
   {  CQ_min_cpu_interval,       21, "min_cpu_interval",   false, AINTER_sub_fields,   false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_inter_attr_list,   write_CQ_inter_attr_list},
   {  CQ_processors,             21, "processors",         false, ASTR_sub_fields,     false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_str_attr_list,     write_CQ_str_attr_list},
   {  CQ_qtype,                  21, "qtype",              false, AQTLIST_sub_fields,  false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_qtlist_attr_list,  write_CQ_qtlist_attr_list},
   {  CQ_ckpt_list,              21, "ckpt_list",          false, ASTRLIST_sub_fields, false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_strlist_attr_list, write_CQ_strlist_attr_list},
   {  CQ_pe_list,                21, "pe_list",            false, ASTRLIST_sub_fields, false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_strlist_attr_list, write_CQ_strlist_attr_list},
   {  CQ_rerun,                  21, "rerun",              false, ABOOL_sub_fields,    false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_bool_attr_list,    write_CQ_bool_attr_list},
   {  CQ_job_slots,              21, "slots",              false, AULNG_sub_fields,    false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_ulng_attr_list,    write_CQ_ulng_attr_list},
   {  CQ_tmpdir,                 21, "tmpdir",             false, ASTR_sub_fields,     false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_str_attr_list,     write_CQ_str_attr_list},
   {  CQ_shell,                  21, "shell",              false, ASTR_sub_fields,     false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_str_attr_list,     write_CQ_str_attr_list},
   {  CQ_prolog,                 21, "prolog",             false, ASTR_sub_fields,    false,  &qconf_sub_name_value_comma_braced_sfi, read_CQ_str_attr_list,     write_CQ_str_attr_list},
   {  CQ_epilog,                 21, "epilog",             false, ASTR_sub_fields,     false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_str_attr_list,     write_CQ_str_attr_list},
   {  CQ_shell_start_mode,       21, "shell_start_mode",   false, ASTR_sub_fields,     false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_str_attr_list,     write_CQ_str_attr_list},
   {  CQ_starter_method,         21, "starter_method",    false,  ASTR_sub_fields,     false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_str_attr_list,     write_CQ_str_attr_list},
   {  CQ_suspend_method,         21, "suspend_method",     false, ASTR_sub_fields,     false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_str_attr_list,     write_CQ_str_attr_list},
   {  CQ_resume_method,          21, "resume_method",      false, ASTR_sub_fields,     false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_str_attr_list,     write_CQ_str_attr_list},
   {  CQ_terminate_method,       21, "terminate_method",   false, ASTR_sub_fields,     false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_str_attr_list,     write_CQ_str_attr_list},
   {  CQ_notify,                 21, "notify",             false, AINTER_sub_fields,   false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_inter_attr_list,   write_CQ_inter_attr_list},
   {  CQ_owner_list,             21, "owner_list",         false, AUSRLIST_sub_fields, false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_usrlist_attr_list, write_CQ_usrlist_attr_list},
   {  CQ_acl,                    21, "user_lists",         false, AUSRLIST_sub_fields, false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_usrlist_attr_list, write_CQ_usrlist_attr_list},
   {  CQ_xacl,                   21, "xuser_lists",        false, AUSRLIST_sub_fields, false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_usrlist_attr_list, write_CQ_usrlist_attr_list},
   {  CQ_subordinate_list,       21, "subordinate_list",   false, ASOLIST_sub_fields,  false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_solist_attr_list,  write_CQ_solist_attr_list},
   {  CQ_consumable_config_list, 21, "complex_values",     false, ACELIST_sub_fields,  false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_celist_attr_list,  write_CQ_celist_attr_list},
   {  CQ_projects,               21, "projects",           false, APRJLIST_sub_fields, false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_prjlist_attr_list, write_CQ_prjlist_attr_list},
   {  CQ_xprojects,              21, "xprojects",          false, APRJLIST_sub_fields, false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_prjlist_attr_list, write_CQ_prjlist_attr_list},
   {  CQ_calendar,               21, "calendar",           false, ASTR_sub_fields,    false,  &qconf_sub_name_value_comma_braced_sfi, read_CQ_str_attr_list,     write_CQ_str_attr_list},
   {  CQ_initial_state,          21, "initial_state",      false, ASTR_sub_fields,     false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_str_attr_list,     write_CQ_str_attr_list},
   {  CQ_s_rt,                   21, "s_rt",               false, ATIME_sub_fields,    false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_time_attr_list,    write_CQ_time_attr_list},
   {  CQ_h_rt,                   21, "h_rt",               false, ATIME_sub_fields,    false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_time_attr_list,    write_CQ_time_attr_list},
   {  CQ_s_cpu,                  21, "s_cpu",              false, ATIME_sub_fields,    false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_time_attr_list,    write_CQ_time_attr_list},
   {  CQ_h_cpu,                  21, "h_cpu",             false,  ATIME_sub_fields,    false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_time_attr_list,    write_CQ_time_attr_list},
   {  CQ_s_fsize,                21, "s_fsize",            false, AMEM_sub_fields,     false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_mem_attr_list,     write_CQ_mem_attr_list},
   {  CQ_h_fsize,                21, "h_fsize",            false, AMEM_sub_fields,     false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_mem_attr_list,     write_CQ_mem_attr_list},
   {  CQ_s_data,                 21, "s_data",             false, AMEM_sub_fields,     false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_mem_attr_list,     write_CQ_mem_attr_list},
   {  CQ_h_data,                 21, "h_data",             false, AMEM_sub_fields,     false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_mem_attr_list,     write_CQ_mem_attr_list},
   {  CQ_s_stack,                21, "s_stack",            false, AMEM_sub_fields,    false,  &qconf_sub_name_value_comma_braced_sfi, read_CQ_mem_attr_list,     write_CQ_mem_attr_list},
   {  CQ_h_stack,                21, "h_stack",            false, AMEM_sub_fields,     false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_mem_attr_list,     write_CQ_mem_attr_list},
   {  CQ_s_core,                 21, "s_core",             false, AMEM_sub_fields,     false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_mem_attr_list,     write_CQ_mem_attr_list},
   {  CQ_h_core,                 21, "h_core",             false, AMEM_sub_fields,     false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_mem_attr_list,     write_CQ_mem_attr_list},
   {  CQ_s_rss,                  21, "s_rss",              false, AMEM_sub_fields,     false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_mem_attr_list,     write_CQ_mem_attr_list},
   {  CQ_h_rss,                  21, "h_rss",              false, AMEM_sub_fields,     false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_mem_attr_list,     write_CQ_mem_attr_list},
   {  CQ_s_vmem,                 21, "s_vmem",             false, AMEM_sub_fields,     false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_mem_attr_list,     write_CQ_mem_attr_list},
   {  CQ_h_vmem,                 21, "h_vmem",             false, AMEM_sub_fields,     false, &qconf_sub_name_value_comma_braced_sfi, read_CQ_mem_attr_list,     write_CQ_mem_attr_list},
   {  NoName,                    21, nullptr,                 false, nullptr,                false, nullptr,                                   nullptr,                      nullptr}
};

spooling_field SH_fields[] = {
   {  SH_name,           21, "hostname",   false, nullptr, false, nullptr, nullptr, nullptr},
   {  NoName,            21, nullptr,         false, nullptr, false, nullptr, nullptr, nullptr}
};

spooling_field AH_fields[] = {
   {  AH_name,           21, "hostname",   false, nullptr, false, nullptr, nullptr, nullptr},
   {  NoName,            21, nullptr,         false, nullptr, false, nullptr, nullptr, nullptr}
};

static spooling_field RN_sub_fields[] = {
   {  RN_min,             0, nullptr,                false, nullptr, false, nullptr, nullptr, nullptr},
   {  RN_max,             0, nullptr,                false, nullptr, false, nullptr, nullptr, nullptr},
   {  RN_step,             0, nullptr,                false, nullptr,false,  nullptr, nullptr, nullptr},
   {  NoName,             0, nullptr,                false, nullptr, false, nullptr, nullptr, nullptr}
};

static spooling_field QR_sub_fields[] = {
   {  QR_name,             0, nullptr,                false, nullptr, false, nullptr, nullptr, nullptr},
   {  NoName,             0, nullptr,                false, nullptr, false, nullptr, nullptr, nullptr}
};

static spooling_field JG_sub_fields[] = {
   {  JG_qname,             0, nullptr,                false, nullptr, false, nullptr, nullptr, nullptr},
   {  JG_slots,             0, nullptr,                false, nullptr, false, nullptr, nullptr, nullptr},
   {  NoName,             0, nullptr,                false, nullptr, false, nullptr, nullptr, nullptr}
};

static spooling_field MR_sub_fields[] = {
   {  MR_user,             0, nullptr,                false, nullptr, false, nullptr, nullptr, nullptr},
   {  MR_host,             0, nullptr,                false, nullptr, false, nullptr, nullptr, nullptr},
   {  NoName,             0, nullptr,                false, nullptr, false, nullptr, nullptr, nullptr}
};

static spooling_field ARA_sub_fields[] = {
   {  ARA_name,             0, nullptr,               false,  nullptr, false, nullptr, nullptr, nullptr},
   {  ARA_group,             0, nullptr,                false, nullptr, false, nullptr, nullptr, nullptr},
   {  NoName,             0, nullptr,                false, nullptr, false, nullptr, nullptr, nullptr}
};

spooling_field AR_fields[] = {
   {  AR_id,              20,   "id",                false, nullptr, false, nullptr, nullptr},
   {  AR_name,            20,   "name",              false, nullptr,false,  nullptr, nullptr},
   {  AR_account,         20,   "account",           false, nullptr, false, nullptr, nullptr},
   {  AR_owner,           20,   "owner",             false, nullptr, false, nullptr, nullptr},
   {  AR_group,           20,   "group",             false, nullptr, false, nullptr, nullptr},
   {  AR_submission_time, 20,   "submission_time",   false, nullptr, false, nullptr, nullptr},
   {  AR_start_time,      20,   "start_time",        false, nullptr, false, nullptr, nullptr},
   {  AR_end_time,        20,   "end_time",          false, nullptr, false, nullptr, nullptr},
   {  AR_duration,        20,   "duration",          false, nullptr, false, nullptr, nullptr},
   {  AR_verify,          20,   "verify",            false, nullptr, false, nullptr, nullptr},
   {  AR_error_handling,  20,   "error_handling",   false,  nullptr, false, nullptr, nullptr},
   {  AR_state,           20,   "state",             false, nullptr, false, nullptr, nullptr},
   {  AR_checkpoint_name, 20,   "checkpoint_name",   false, nullptr, false, nullptr, nullptr},
   {  AR_resource_list,   20,   "resource_list",     false, CE_sub_fields, false, &qconf_sub_name_value_comma_sfi, nullptr},
   {  AR_queue_list,      20,   "queue_list",        false, QR_sub_fields, false, nullptr, nullptr},
   {  AR_granted_slots,   20,   "granted_slots",     false, JG_sub_fields, false, &qconf_sub_name_value_comma_sfi, nullptr},
   {  AR_mail_options,    20,   "mail_options",      false, nullptr, false, nullptr, nullptr},
   {  AR_mail_list,       20,   "mail_list",         false, MR_sub_fields, false, &qconf_sub_name_value_comma_sfi, nullptr},
   {  AR_pe,              20,   "pe",                false, nullptr, false, nullptr, nullptr},
   {  AR_pe_range,        20,   "pe_range",          false, RN_sub_fields, false, &qconf_sub_name_value_comma_sfi, nullptr, nullptr},
   {  AR_granted_pe,      20,   "granted_pe",        false, nullptr, false, nullptr, nullptr},
   {  AR_master_queue_list, 20, "master_queue_list", false, QR_sub_fields, false, nullptr, nullptr},
   {  AR_acl_list,        20,   "acl_list",          false, ARA_sub_fields, false, &qconf_sub_name_value_comma_sfi, nullptr},
   {  AR_xacl_list,       20,   "xacl_list",         false, ARA_sub_fields, false, &qconf_sub_name_value_comma_sfi, nullptr},
   {  AR_type,            20,   "type",              false, nullptr, false, nullptr, nullptr},
   {  NoName,             20,   nullptr,               false,  nullptr, false, nullptr, nullptr}
};

spooling_field PE_fields[] = {
   {  PE_name,                                 20,   "pe_name",              false, nullptr, false, nullptr, nullptr},
   {  PE_slots,                                20,   "slots",                false, nullptr, false, nullptr, nullptr},
   {  PE_user_list,                            20,   "user_lists",           false, US_sub_fields, false, nullptr, nullptr},
   {  PE_xuser_list,                           20,   "xuser_lists",          false, US_sub_fields, false, nullptr, nullptr},
   {  PE_start_proc_args,                      20,   "start_proc_args",      false, nullptr, false, nullptr, nullptr},
   {  PE_stop_proc_args,                       20,   "stop_proc_args",       false, nullptr, false, nullptr, nullptr},
   {  PE_allocation_rule,                      20,   "allocation_rule",      false, nullptr, false, nullptr, nullptr},
   {  PE_control_slaves,                       20,   "control_slaves",       false, nullptr, false, nullptr, nullptr},
   {  PE_job_is_first_task,                    20,   "job_is_first_task",    false,  nullptr, false, nullptr, nullptr},
   {  PE_urgency_slots,                        20,   "urgency_slots",        false, nullptr, false, nullptr, nullptr},
   {  PE_accounting_summary,                   20,   "accounting_summary",   false, nullptr, false, nullptr, nullptr},
   {  PE_ignore_slave_requests_on_master_host, 20,   "ign_sreq_on_mhost",    false, nullptr, false, nullptr, nullptr},
   {  PE_master_forks_slaves,                  20,   "master_forks_slaves",  false, nullptr, false, nullptr, nullptr},
   {  PE_daemon_forks_slaves,                  20,   "daemon_forks_slaves",  false, nullptr, false, nullptr, nullptr},
   {  NoName,                                  20,   nullptr,                false, nullptr, false, nullptr, nullptr}
};

spooling_field RQS_fields[] = {
   {  RQS_name,           12,   "name",              false, nullptr, false, nullptr, nullptr},
   {  RQS_description,    12,   "description",       false, nullptr, false, nullptr, nullptr},
   {  RQS_enabled,        12,   "enabled",           false, nullptr, false, nullptr, nullptr},
   {  RQS_rule,           12,   "limit",             false, RQR_sub_fields, false, &qconf_sub_rqs_sfi, nullptr},
   {  NoName,             12,   nullptr,                false, nullptr, false, nullptr, nullptr}
};

static void create_spooling_field (
   spooling_field *field,
   int nm, 
   int width, 
   const char *name,
   bool free_name,
   struct spooling_field *sub_fields,
   bool free_sub_fields,
   const void *clientdata, 
   int (*read_func) (lListElem *ep, int nm, const char *buffer, lList **alp), 
   int (*write_func) (const lListElem *ep, int nm, dstring *buffer, lList **alp)
)
{
   if (field != nullptr) {
      field->nm = nm;
      field->width = width;
      field->name = name;
      field->free_name = free_name;
      field->sub_fields = sub_fields;
      field->free_sub_fields = free_sub_fields;
      field->clientdata = clientdata;
      field->read_func = read_func;
      field->write_func = write_func;
   }
}

spooling_field *sge_build_PR_field_list(bool spool)
{
   /* There are 11 possible PR_Type fields. */
   spooling_field *fields = (spooling_field *)sge_malloc(sizeof(spooling_field)*11);
   int count = 0;
   
   /* Build the list of fields to read and write */
   create_spooling_field(&fields[count++], PR_name, 0, "name", false, nullptr, false, nullptr, nullptr, nullptr);
   create_spooling_field(&fields[count++], PR_oticket, 0, "oticket", false, nullptr, false, nullptr, nullptr, nullptr);
   create_spooling_field(&fields[count++], PR_fshare, 0, "fshare", false, nullptr, false, nullptr, nullptr, nullptr);
   if (spool) {
      create_spooling_field(&fields[count++], PR_usage, 0, "usage", false, UA_sub_fields, false, &qconf_sub_name_value_space_sfi, nullptr, nullptr);
      create_spooling_field(&fields[count++], PR_usage_time_stamp, 0, "usage_time_stamp", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field(&fields[count++], PR_long_term_usage, 0, "long_term_usage", false, UA_sub_fields, false, &qconf_sub_name_value_space_sfi, nullptr, nullptr);
      create_spooling_field(&fields[count++], PR_project, 0, "project", false, UPP_sub_fields,  false, &qconf_sub_spool_usage_sfi, nullptr, nullptr);
   }
   create_spooling_field(&fields[count++], PR_acl, 0, "acl", false, US_sub_fields, false, nullptr, nullptr, nullptr);
   create_spooling_field(&fields[count++], PR_xacl, 0, "xacl", false, US_sub_fields, false, nullptr, nullptr, nullptr);
   if (spool) {
      create_spooling_field(&fields[count++], PR_debited_job_usage, 0, "debited_job_usage", false, UPU_sub_fields, false, &qconf_sub_spool_usage_sfi, nullptr, nullptr);
   }
   create_spooling_field(&fields[count++], NoName, 0, nullptr, false, nullptr, false, nullptr, nullptr, nullptr);
   
   return fields;
}

spooling_field *sge_build_UU_field_list(bool spool)
{
   /* There are 11 possible UU_Type fields. */
   spooling_field *fields = (spooling_field *)sge_malloc(sizeof(spooling_field)*11);
   int count = 0;
   
   /* Build the list of fields to read and write */
   create_spooling_field(&fields[count++], UU_name, 0, "name", false, nullptr, false, nullptr, nullptr, nullptr);
   create_spooling_field(&fields[count++], UU_oticket, 0, "oticket", false, nullptr, false, nullptr, nullptr, nullptr);
   create_spooling_field(&fields[count++], UU_fshare, 0, "fshare", false, nullptr, false, nullptr, nullptr, nullptr);
   create_spooling_field(&fields[count++], UU_delete_time, 0, "delete_time", false, nullptr, false, nullptr, nullptr, nullptr);
   if (spool) {
      create_spooling_field(&fields[count++], UU_usage, 0, "usage", false, UA_sub_fields, false, &qconf_sub_name_value_space_sfi, nullptr, nullptr);
      create_spooling_field(&fields[count++], UU_usage_time_stamp, 0, "usage_time_stamp", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field(&fields[count++], UU_long_term_usage, 0, "long_term_usage", false, UA_sub_fields, false, &qconf_sub_name_value_space_sfi, nullptr, nullptr);
      create_spooling_field(&fields[count++], UU_project, 0, "project", false, UPP_sub_fields, false, &qconf_sub_spool_usage_sfi, nullptr, nullptr);
   }
   create_spooling_field(&fields[count++], UU_default_project, 0, "default_project", false,  nullptr, false, nullptr, nullptr, nullptr);
   if (spool) {
      create_spooling_field(&fields[count++], UU_debited_job_usage, 0, "debited_job_usage", false, UPU_sub_fields, false, &qconf_sub_spool_usage_sfi, nullptr, nullptr);
   }
   create_spooling_field(&fields[count++], NoName, 0, nullptr, false, nullptr, false, nullptr, nullptr, nullptr);
   
   return fields;
}

spooling_field *sge_build_STN_field_list(bool spool, bool recurse)
{
   /* There are 7 possible STN_Type fields. */
   spooling_field *fields = (spooling_field *)sge_malloc (sizeof(spooling_field)*7);
   int count = 0;

   if (recurse) {
      create_spooling_field (&fields[count++], STN_id, 0, "id", false, nullptr, false, nullptr, nullptr, nullptr);
   }
  
   if (spool) {
      create_spooling_field (&fields[count++], STN_version, 0, "version", false, nullptr, false, nullptr, nullptr, nullptr);
   }

   create_spooling_field (&fields[count++], STN_name, 0, "name", false, nullptr, false, nullptr, nullptr, nullptr);
   create_spooling_field (&fields[count++], STN_type, 0, "type", false, nullptr, false, nullptr, nullptr, nullptr);
   create_spooling_field (&fields[count++], STN_shares, 0, "shares", false, nullptr, false, nullptr, nullptr, nullptr);
   
   if (recurse) {
      create_spooling_field (&fields[count++], STN_children, 0, "childnodes", false, STN_sub_fields, false, nullptr, nullptr, nullptr);
   }
   
   create_spooling_field (&fields[count++], NoName, 0, nullptr, false, nullptr, false, nullptr, nullptr, nullptr);
   
   return fields;
}

/* The spool_flatfile_read_object() function will fail to read the
 * EH_reschedule_unknown_list field from a classic spooling file because
 * classic spooling uses two different field delimiters to represent the
 * field values. */
spooling_field *sge_build_EH_field_list(bool spool, bool to_stdout,
                                        bool history)
{
   /* There are 14 possible EH_Type fields. */
   spooling_field *fields = (spooling_field *)sge_malloc(sizeof(spooling_field)*14);
   int count = 0;

   create_spooling_field(&fields[count++], EH_name, 21, "hostname", false, nullptr, false, nullptr, nullptr, nullptr);
   create_spooling_field(&fields[count++], EH_scaling_list, 21, "load_scaling", false, HS_sub_fields, false, &qconf_sub_name_value_comma_sfi, nullptr, nullptr);
   create_spooling_field(&fields[count++], EH_consumable_config_list, 21, "complex_values", false, CE_host_sub_fields, false, &qconf_sub_name_value_comma_sfi, nullptr, nullptr);
   
   if (getenv("MORE_INFO")) {
      create_spooling_field(&fields[count++], EH_resource_utilization, 21, "complex_values_actual", false, RUE_sub_fields, false, &qconf_sub_name_value_comma_sfi, nullptr, nullptr);
   }
   
   if (spool || (!spool && to_stdout) || history) {
      create_spooling_field(&fields[count++], EH_load_list, 21, "load_values", false, HL_sub_fields, false, &qconf_sub_name_value_comma_sfi, nullptr, nullptr);
      create_spooling_field(&fields[count++], EH_processors, 21, "processors", false, nullptr, false, nullptr, nullptr, nullptr);
   }

   if (spool) {
      create_spooling_field(&fields[count++], EH_reschedule_unknown_list, 21, "reschedule_unknown_list", false, RU_sub_fields, false, &qconf_sub_name_value_comma_sfi, nullptr, nullptr);
   }
   
   create_spooling_field(&fields[count++], EH_acl, 21, "user_lists", false, US_sub_fields, false, nullptr, nullptr, nullptr);
   create_spooling_field(&fields[count++], EH_xacl, 21, "xuser_lists", false, US_sub_fields, false, nullptr, nullptr, nullptr);
   
   create_spooling_field(&fields[count++], EH_prj, 21, "projects", false, PR_sub_fields, false, nullptr, nullptr, nullptr);
   create_spooling_field(&fields[count++], EH_xprj, 21, "xprojects", false, PR_sub_fields, false, nullptr, nullptr, nullptr);
   create_spooling_field(&fields[count++], EH_usage_scaling_list, 21, "usage_scaling", false, HS_sub_fields, false, &qconf_sub_name_value_comma_sfi, nullptr, nullptr);
   create_spooling_field(&fields[count++], EH_report_variables, 21, "report_variables", false, STU_sub_fields, false, &qconf_sub_name_value_comma_sfi, nullptr, nullptr);
   create_spooling_field(&fields[count++], NoName, 21, nullptr, false, nullptr, false, nullptr, nullptr, nullptr);
   
   return fields;
}

static int read_SC_queue_sort_method(lListElem *ep, int nm,
                                     const char *buffer, lList **alp)
{
   if (!strncasecmp(buffer, "load", 4)) {
      lSetUlong(ep, nm, QSM_LOAD);
   } else if (!strncasecmp(buffer, "seqno", 5)) {
      lSetUlong(ep, nm, QSM_SEQNUM);
   }
   
   return 1;
}

static int write_SC_queue_sort_method(const lListElem *ep, int nm,
                                      dstring *buffer, lList **alp)
{
   if (lGetUlong(ep, nm) == QSM_SEQNUM) {
      sge_dstring_append(buffer, "seqno");
   } else {
      sge_dstring_append(buffer, "load");
   }

   return 1;
}

static int read_CF_value(lListElem *ep, int nm, const char *buf,
                         lList **alp)
{
   const char *name = lGetString(ep, CF_name);
   char *value = nullptr;
   char *buffer = strdup(buf);
   struct saved_vars_s *context = nullptr;

   DENTER(TOP_LAYER);
   
   if (!strcmp(name, "gid_range")) {
      if ((value = sge_strtok_r(buffer, " \t\n", &context))) {
         if (!strcasecmp(value, NONE_STR)) {
            lSetString(ep, CF_value, value);
         } else {
            lList *rlp = nullptr;
            
            range_list_parse_from_string(&rlp, alp, value, 
                                         false, false, INF_NOT_ALLOWED);
            if (rlp == nullptr) {
               WARNING(MSG_CONFIG_CONF_INCORRECTVALUEFORCONFIGATTRIB_SS, name, value);
   
               sge_free_saved_vars(context);
               sge_free(&buffer);
               DRETURN(0);
            } else {
               const lListElem *rep;

               for_each_ep(rep, rlp) {
                  u_long32 min;

                  min = lGetUlong(rep, RN_min);
                  if (min < GID_RANGE_NOT_ALLOWED_ID) {
                     WARNING(MSG_CONFIG_CONF_GIDRANGELESSTHANNOTALLOWED_I, GID_RANGE_NOT_ALLOWED_ID);
   
                     sge_free_saved_vars(context);
                     sge_free(&buffer);
                     lFreeList(&rlp);
                     DRETURN(0);
                  }                  
               }
               lFreeList(&rlp);
               lSetString(ep, CF_value, value);
            }
         }
      }
   } else if (!strcmp(name, "admin_user")) {
      value = sge_strtok_r(buffer, " \t\n", &context);
      while (value[0] && isspace((int)value[0]))
         value++;
      if (value) {
         lSetString(ep, CF_value, value);
      } else {
         WARNING(MSG_CONFIG_CONF_NOVALUEFORCONFIGATTRIB_S, name);
   
         sge_free_saved_vars(context);
         sge_free(&buffer);
         DRETURN(0);
      }
   } else if (!strcmp(name, "user_lists") || 
      !strcmp(name, "xuser_lists") || 
      !strcmp(name, "projects") || 
      !strcmp(name, "xprojects") || 
      !strcmp(name, "prolog") || 
      !strcmp(name, "epilog") || 
      !strcmp(name, "starter_method") || 
      !strcmp(name, "suspend_method") || 
      !strcmp(name, "resume_method") || 
      !strcmp(name, "terminate_method") || 
      !strcmp(name, "qmaster_params") || 
      !strcmp(name, "execd_params") || 
      !strcmp(name, "reporting_params") || 
      !strcmp(name, "qlogin_command") ||
      !strcmp(name, "rlogin_command") ||
      !strcmp(name, "rsh_command") ||
      !strcmp(name, "jsv_url") ||
      !strcmp(name, "jsv_allowed_mod") ||
      !strcmp(name, "gdi_request_limits") ||
      !strcmp(name, "qlogin_daemon") ||
      !strcmp(name, "rlogin_daemon") ||
      !strcmp(name, "rsh_daemon") ||
      !strcmp(name, "additional_jvm_args")) {
      if (!(value = sge_strtok_r(buffer, "\t\n", &context))) {
         /* return line if value is empty */
         WARNING(MSG_CONFIG_CONF_NOVALUEFORCONFIGATTRIB_S, name);
   
         sge_free_saved_vars(context);
         sge_free(&buffer);
         DRETURN(0);
      }
      /* skip leading delimitors */
      while (value[0] && isspace((int)value[0]))
         value++;

      lSetString(ep, CF_value, value);
   } else {
      if (!(value = sge_strtok_r(buffer, " \t\n", &context))) {
         WARNING(MSG_CONFIG_CONF_NOVALUEFORCONFIGATTRIB_S, name);
   
         sge_free_saved_vars(context);
         sge_free(&buffer);
         DRETURN(0);
      }
      if (strcmp(name, "auto_user_oticket") == 0 || 
              strcmp(name, "auto_user_fshare") == 0 ) {
         char *end_ptr = nullptr;
         double dbl_value;

         dbl_value = strtod(value, &end_ptr);
         if ( dbl_value < 0) {
            answer_list_add_sprintf(alp, STATUS_EUNKNOWN,
                                 ANSWER_QUALITY_ERROR,
				 MSG_MUST_BE_POSITIVE_VALUE_S,
                                 name);
            DRETURN(0);
         }            
      } 
      lSetString(ep, CF_value, value);

      if (sge_strtok_r(nullptr, " \t\n", &context)) {
         /* Allow only one value per line */
         WARNING(MSG_CONFIG_CONF_ONLYSINGLEVALUEFORCONFIGATTRIB_S, name);
   
         sge_free_saved_vars(context);
         sge_free(&buffer);
         DRETURN(0);
      }
   }

   sge_free_saved_vars(context);
   sge_free(&buffer);
   DRETURN(1);
}

spooling_field *sge_build_CONF_field_list(bool spool_config)
{
   /* There are 4 possible CONF_Type fields. */
   spooling_field *fields = (spooling_field *)sge_malloc(sizeof(spooling_field)*4);
   int count = 0;
   
   if (spool_config) {
      create_spooling_field(&fields[count++], CONF_name, 28, "conf_name", false, nullptr, false,  nullptr, nullptr, nullptr);
      create_spooling_field(&fields[count++], CONF_version, 28, "conf_version", false, nullptr, false, nullptr, nullptr, nullptr);
   }
   
   create_spooling_field(&fields[count++], CONF_entries, 28, nullptr, false, CF_sub_fields, false, &qconf_sub_param_sfi, nullptr, nullptr);
   create_spooling_field(&fields[count++], NoName, 28, nullptr, false, nullptr, false, nullptr, nullptr, nullptr);
   
   return fields;
}

spooling_field *sge_build_QU_field_list(bool to_stdout, bool to_file)
{
   /* There are 52 possible QU_Type fields. */
   spooling_field *fields = (spooling_field *)sge_malloc(sizeof(spooling_field)*52);
   int count = 0;
   
   create_spooling_field (&fields[count++], QU_qname, 21, "qname", false, nullptr, false, nullptr, nullptr, nullptr);
   create_spooling_field (&fields[count++], QU_qhostname, 21, "hostname", false, nullptr, false, nullptr, nullptr, nullptr);
   
   if (to_stdout) {
      create_spooling_field (&fields[count++], QU_seq_no, 21, "seq_no", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_load_thresholds, 21, "load_thresholds", false, CE_sub_fields, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_suspend_thresholds, 21, "suspend_thresholds", false, CE_sub_fields, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_nsuspend, 21, "nsuspend", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_suspend_interval, 21, "suspend_interval", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_priority, 21, "priority", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_min_cpu_interval, 21, "min_cpu_interval", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_processors, 21, "processors", false, nullptr,false,  nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_qtype, 21, "qtype", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_ckpt_list, 21, "ckpt_list", false, ST_sub_fields, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_pe_list, 21, "pe_list", false, ST_sub_fields, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_rerun, 21, "rerun", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_job_slots, 21, "slots", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_tmpdir, 21, "tmpdir", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_shell, 21, "shell", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_prolog, 21, "prolog", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_epilog, 21, "epilog", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_shell_start_mode, 21, "shell_start_mode", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_starter_method, 21, "starter_method", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_suspend_method, 21, "suspend_method", false, nullptr,false,  nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_resume_method, 21, "resume_method", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_terminate_method, 21, "terminate_method", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_notify, 21, "notify", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_owner_list, 21, "owner_list", false, US_sub_fields, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_acl, 21, "user_lists", false, US_sub_fields, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_xacl, 21, "xuser_lists", false, US_sub_fields, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_subordinate_list, 21, "subordinate_list", false, SO_sub_fields, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_consumable_config_list, 21, "complex_values", false, CE_sub_fields,false,  nullptr, nullptr, nullptr);
      
      create_spooling_field (&fields[count++], QU_projects, 21, "projects", false, PR_sub_fields, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_xprojects, 21, "xprojects", false, PR_sub_fields, false, nullptr, nullptr, nullptr);
      
      create_spooling_field (&fields[count++], QU_calendar, 21, "calendar", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_initial_state, 21, "initial_state", false, nullptr, false, nullptr, nullptr, nullptr);
#if 0
      create_spooling_field (&fields[count++], QU_fshare, 21, "fshare", nullptr,
                             nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_oticket, 21, "oticket",
                             nullptr, nullptr, nullptr, nullptr);
#endif
      create_spooling_field (&fields[count++], QU_s_rt, 21, "s_rt", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_h_rt, 21, "h_rt", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_s_cpu, 21, "s_cpu", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_h_cpu, 21, "h_cpu", false,  nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_s_fsize, 21, "s_fsize", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_h_fsize, 21, "h_fsize", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_s_data, 21, "s_data", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_h_data, 21, "h_data", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_s_stack, 21, "s_stack", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_h_stack, 21, "h_stack", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_s_core, 21, "s_core", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_h_core, 21, "h_core", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_s_rss, 21, "s_rss", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_h_rss, 21, "h_rss", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_s_vmem, 21, "s_vmem", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_h_vmem, 21, "h_vmem", false, nullptr, false, nullptr, nullptr, nullptr);
   } else if (to_file) {
      /*
       * Spool only non-CQ attributes
       */
      create_spooling_field (&fields[count++], QU_state, 21, "state", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_pending_signal, 21, "pending_signal", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_pending_signal_delivery_time, 21, "pending_signal_del", false, nullptr, false, nullptr, nullptr, nullptr);
      create_spooling_field (&fields[count++], QU_version, 21, "version", false, nullptr, false, nullptr, nullptr, nullptr);
      /* SG: not supported */
#if 0      
      create_spooling_field (&fields[count++], QU_error_messages, 21,
                             "error_messages", nullptr, nullptr, nullptr, nullptr);
#endif      
   }
   create_spooling_field (&fields[count++], NoName, 21, nullptr, false, nullptr, false, nullptr, nullptr, nullptr);
   
   return fields;
}

static int read_CQ_ulng_attr_list(lListElem *ep, int nm, const char *buffer, lList **alp)
{
   lList *lp = nullptr;

   if (!ulng_attr_list_parse_from_string(&lp, alp, buffer,
                                         HOSTATTR_ALLOW_AMBIGUITY, nullptr)) {
      lFreeList(&lp);
      return 0;
   }

   if (lp != nullptr) {
      lSetList(ep, nm, lp);
      return 1;
   }

   return 0;
}

static int write_CQ_ulng_attr_list(const lListElem *ep, int nm,
                                   dstring *buffer, lList **alp)
{
   ulng_attr_list_append_to_dstring(lGetList (ep, nm), buffer);
   
   return 1;
}

static int read_CQ_celist_attr_list(lListElem *ep, int nm, const char *buffer,
                                     lList **alp)
{
   lList *lp = nullptr;

   if (!celist_attr_list_parse_from_string(&lp, alp, buffer,
                                          HOSTATTR_ALLOW_AMBIGUITY, nullptr)) {
      lFreeList(&lp);
      return 0;
   }

   if (lp != nullptr) {
      lSetList(ep, nm, lp);
      return 1;
   }

   return 0;
}

static int write_CQ_celist_attr_list(const lListElem *ep, int nm,
                                   dstring *buffer, lList **alp)
{
   celist_attr_list_append_to_dstring(lGetList(ep, nm), buffer);
   
   return 1;
}

static int read_CQ_inter_attr_list(lListElem *ep, int nm, const char *buffer,
                                    lList **alp)
{
   lList *lp = nullptr;
   
   if (!inter_attr_list_parse_from_string(&lp, alp, buffer,
                                          HOSTATTR_ALLOW_AMBIGUITY, nullptr)) {
      lFreeList(&lp);
      return 0;
   }
   
   if (lp != nullptr) {
      lSetList(ep, nm, lp);
      return 1;
   }
   
   return 0;
}

static int write_CQ_inter_attr_list(const lListElem *ep, int nm,
                                   dstring *buffer, lList **alp)
{
   inter_attr_list_append_to_dstring(lGetList (ep, nm), buffer);
   
   return 1;
}

static int read_CQ_str_attr_list(lListElem *ep, int nm, const char *buffer,
                                  lList **alp)
{
   lList *lp = nullptr;
   
   if (!str_attr_list_parse_from_string(&lp, alp, buffer,
                                          HOSTATTR_ALLOW_AMBIGUITY, nullptr)) {
      lFreeList(&lp);
      return 0;
   }
   
   if (lp != nullptr) {
      lSetList(ep, nm, lp);
      return 1;
   }
   
   return 0;
}

static int write_CQ_str_attr_list(const lListElem *ep, int nm,
                                   dstring *buffer, lList **alp)
{
   str_attr_list_append_to_dstring(lGetList(ep, nm), buffer);
   
   return 1;
}

static int read_CQ_qtlist_attr_list(lListElem *ep, int nm, const char *buffer,
                                     lList **alp)
{
   lList *lp = nullptr;
   
   if (!qtlist_attr_list_parse_from_string(&lp, alp, buffer,
                                          HOSTATTR_ALLOW_AMBIGUITY, nullptr)) {
      lFreeList(&lp);
      return 0;
   }
   
   if (lp != nullptr) {
      lSetList(ep, nm, lp);
      return 1;
   }
   
   return 0;
}

static int write_CQ_qtlist_attr_list(const lListElem *ep, int nm,
                                   dstring *buffer, lList **alp)
{
   qtlist_attr_list_append_to_dstring(lGetList (ep, nm), buffer);
   
   return 1;
}

static int read_CQ_strlist_attr_list(lListElem *ep, int nm, const char *buffer,
                                      lList **alp)
{
   lList *lp = nullptr;
   
   if (!strlist_attr_list_parse_from_string(&lp, alp, buffer,
                                          HOSTATTR_ALLOW_AMBIGUITY, nullptr)) {
      lFreeList(&lp);
      return 0;
   }
   
   if (lp != nullptr) {
      lSetList(ep, nm, lp);
      return 1;
   }
   
   return 0;
}

static int write_CQ_strlist_attr_list(const lListElem *ep, int nm,
                                   dstring *buffer, lList **alp)
{
   strlist_attr_list_append_to_dstring(lGetList (ep, nm), buffer);
   
   return 1;
}

static int read_CQ_bool_attr_list(lListElem *ep, int nm, const char *buffer,
                                   lList **alp)
{
   lList *lp = nullptr;
   
   if (!bool_attr_list_parse_from_string(&lp, alp, buffer,
                                          HOSTATTR_ALLOW_AMBIGUITY, nullptr)) {
      lFreeList(&lp);
      return 0;
   }
   
   if (lp != nullptr) {
      lSetList(ep, nm, lp);
      return 1;
   }
   
   return 0;
}

static int write_CQ_bool_attr_list(const lListElem *ep, int nm,
                                   dstring *buffer, lList **alp)
{
   bool_attr_list_append_to_dstring(lGetList (ep, nm), buffer);
   
   return 1;
}

static int read_CQ_usrlist_attr_list(lListElem *ep, int nm, const char *buffer,
                                      lList **alp)
{
   lList *lp = nullptr;
   
   if (!usrlist_attr_list_parse_from_string(&lp, alp, buffer,
                                          HOSTATTR_ALLOW_AMBIGUITY, nullptr)) {
      lFreeList(&lp);
      return 0;
   }
   
   if (lp != nullptr) {
      lSetList(ep, nm, lp);
      return 1;
   }
   
   return 0;
}

static int write_CQ_usrlist_attr_list(const lListElem *ep, int nm,
                                   dstring *buffer, lList **alp)
{
   usrlist_attr_list_append_to_dstring(lGetList (ep, nm), buffer);
   
   return 1;
}

static int read_CQ_solist_attr_list(lListElem *ep, int nm, const char *buffer,
                                     lList **alp)
{
   lList *lp = nullptr;
   
   if (!solist_attr_list_parse_from_string(&lp, alp, buffer,
                                          HOSTATTR_ALLOW_AMBIGUITY, nullptr)) {
      lFreeList(&lp);
      return 0;
   }
   
   if (lp != nullptr) {
      lSetList(ep, nm, lp);
      return 1;
   }
   
   return 0;
}

static int write_CQ_solist_attr_list(const lListElem *ep, int nm,
                                   dstring *buffer, lList **alp)
{
   const lList *lp = lGetList(ep, nm);
   
   solist_attr_list_append_to_dstring(lp, buffer);
   
   return 1;
}

static int read_CQ_prjlist_attr_list(lListElem *ep, int nm, const char *buffer,
                                      lList **alp)
{
   lList *lp = nullptr;
   
   if (!prjlist_attr_list_parse_from_string(&lp, alp, buffer,
                                          HOSTATTR_ALLOW_AMBIGUITY, nullptr)) {
      lFreeList(&lp);
      return 0;
   }
   
   if (lp != nullptr) {
      lSetList(ep, nm, lp);
      return 1;
   }
   
   return 0;
}

static int write_CQ_prjlist_attr_list(const lListElem *ep, int nm,
                                   dstring *buffer, lList **alp)
{
   prjlist_attr_list_append_to_dstring(lGetList(ep, nm), buffer);
   
   return 1;
}

static int read_CQ_time_attr_list(lListElem *ep, int nm, const char *buffer,
                                   lList **alp)
{
   lList *lp = nullptr;
   
   if (!time_attr_list_parse_from_string(&lp, alp, buffer,
                                          HOSTATTR_ALLOW_AMBIGUITY, nullptr)) {
      lFreeList(&lp);
      return 0;
   }
   
   if (lp != nullptr) {
      lSetList(ep, nm, lp);
      return 1;
   }
   
   return 0;
}

static int write_CQ_time_attr_list(const lListElem *ep, int nm,
                                   dstring *buffer, lList **alp)
{
   time_attr_list_append_to_dstring(lGetList(ep, nm), buffer);
   
   return 1;
}

static int read_CQ_mem_attr_list(lListElem *ep, int nm, const char *buffer,
                                  lList **alp)
{
   lList *lp = nullptr;
   
   if (!mem_attr_list_parse_from_string(&lp, alp, buffer,
                                          HOSTATTR_ALLOW_AMBIGUITY, nullptr)) {
      lFreeList(&lp);
      return 0;
   }
   
   if (lp != nullptr) {
      lSetList(ep, nm, lp);
      return 1;
   }
   
   return 0;
}

static int write_CQ_mem_attr_list(const lListElem *ep, int nm,
                                   dstring *buffer, lList **alp)
{
   mem_attr_list_append_to_dstring(lGetList(ep, nm), buffer);
   
   return 1;
}

static int read_CQ_hostlist(lListElem *ep, int nm, const char *buffer,
                             lList **alp)
{
   lList *lp = nullptr;
   char delims[] = "\t \v\r,"; 

   lString2List(buffer, &lp, HR_Type, HR_name, delims); 

   if (lp != nullptr) {
      if (strcasecmp(NONE_STR, lGetHost(lFirst(lp), HR_name)) != 0) {
         lSetList(ep, CQ_hostlist, lp);
      } else {
         lFreeList(&lp);
      }
   }

   return 1;
}

static int write_CQ_hostlist(const lListElem *ep, int nm,
                             dstring *buffer, lList **alp)
{
   const lList *lp = lGetList(ep, nm);
   
   if (lp != nullptr) {
      href_list_append_to_dstring(lp, buffer);
   } else {
      sge_dstring_append(buffer, NONE_STR);
   }
   
   return 1;
}

static int write_CE_stringval(const lListElem *ep, int nm, dstring *buffer,
                       lList **alp)
{
   const char *s;

   if ((s=lGetString(ep, CE_stringval)) != nullptr) {
      sge_dstring_append(buffer, s);
   } else {
      sge_dstring_sprintf_append(buffer, "%f", lGetDouble(ep, CE_doubleval));
   }
   
   return 1;
}

/****** sge_flatfile_obj/read_RQR_obj() ****************************************
*  NAME
*     read_RQR_obj() -- parse a RQR object from string
*
*  SYNOPSIS
*     static int read_RQR_obj(lListElem *ep, int nm, const char *buffer, lList 
*     **alp) 
*
*  FUNCTION
*     Reads in a RQR Element from string
*
*  INPUTS
*     lListElem *ep      - Store for parsed Elem
*     int nm             - nm to be parsed 
*     const char *buffer - String of the elem to be parsed
*     lList **alp        - Answer list
*
*  RESULT
*     static int - 1 on success
*                  0 on error
*
*  NOTES
*     MT-NOTE: read_RQR_obj() is MT safe 
*
*******************************************************************************/
static int read_RQR_obj(lListElem *ep, int nm, const char *buffer,
                             lList **alp) {
   lListElem *filter = nullptr;
   int ret = 1;

   DENTER(TOP_LAYER);

   if ((ret = rqs_parse_filter_from_string(&filter, buffer, alp)) == 1) {
      lSetObject(ep, nm, filter);
   } 

   DRETURN(ret);
}

/****** sge_flatfile_obj/write_RQR_obj() ***************************************
*  NAME
*     write_RQR_obj() -- converts a element to string
*
*  SYNOPSIS
*     static int write_RQR_obj(const lListElem *ep, int nm, dstring *buffer, lList 
*     **alp) 
*
*  FUNCTION
*     Prints out a RQR Element to a string
*
*  INPUTS
*     const lListElem *ep - Elem to be converted
*     int nm              - nm of Elem
*     dstring *buffer     - Element as string
*     lList **alp         - Answer List
*
*  RESULT
*     static int - 1 on success
*                  0 on error
*
*  NOTES
*     MT-NOTE: write_RQR_obj() is MT safe 
*
*******************************************************************************/
static int write_RQR_obj(const lListElem *ep, int nm, dstring *buffer,
                       lList **alp) {
   return rqs_append_filter_to_dstring(lGetObject(ep, nm), buffer, alp);
}
