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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/                                   

/** @file
 * @brief The attribute list to spool, per object type
 */

#include <cstring>
#include <strings.h>
#include <cctype>
#include <tuple>
#include <utility>

#include "uti/sge.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_time.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_attr.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_href.h"
#include "sgeobj/sge_resource_quota.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/ocs_Role.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_CF.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_HR.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/tom/ocs_tom_RQRF.h"
#include "sgeobj/tom/ocs_tom_SC.h"
#include "sgeobj/tom/ocs_tom_all.h"

#include "spool/flatfile/ocs_spool_fields.h"
#include "spool/flatfile/sge_flatfile.h"
#include "spool/flatfile/sge_flatfile_obj.h"
#include "spool/flatfile/sge_flatfile_obj_rsmap.h"
#include "spool/flatfile/msg_spoollib_flatfile.h"

#include "msg_common.h"

/* This file defines the field tables of the flatfile spooling framework. Every
 * table is a typed table (see ocs_spool_fields.h); the run-time arrays the
 * framework and its callers take are generated from them. Some arrays are
 * exported as they are, others are selected per call by a builder function,
 * because the fields to spool change with the context the builder is called
 * from. A builder selects the rows whose variant bits it sets. */

static bool read_SC_queue_sort_method(ocs::tom::Element<ocs::tom::SC> sc, const char *buffer,
                                      ocs::tom::ListPtr<ocs::tom::AN> *alp);
static bool write_SC_queue_sort_method(ocs::tom::ConstElement<ocs::tom::SC> sc, dstring *buffer,
                                       ocs::tom::ListPtr<ocs::tom::AN> *alp);
static bool read_CF_value(ocs::tom::Element<ocs::tom::CF> cf, const char *buf, ocs::tom::ListPtr<ocs::tom::AN> *alp);
template <auto F>
static bool read_CQ_attr_list(ocs::tom::Element<ocs::tom::CQ> cq, const char *buffer,
                              ocs::tom::ListPtr<ocs::tom::AN> *alp);
template <auto F>
static bool write_CQ_attr_list(ocs::tom::ConstElement<ocs::tom::CQ> cq, dstring *buffer,
                               ocs::tom::ListPtr<ocs::tom::AN> *alp);
static bool read_CQ_hostlist(ocs::tom::Element<ocs::tom::CQ> cq, const char *buffer,
                             ocs::tom::ListPtr<ocs::tom::AN> *alp);
static bool write_CQ_hostlist(ocs::tom::ConstElement<ocs::tom::CQ> cq, dstring *buffer,
                              ocs::tom::ListPtr<ocs::tom::AN> *alp);
static bool write_CE_stringval(ocs::tom::ConstElement<ocs::tom::CE> centry, dstring *buffer,
                               ocs::tom::ListPtr<ocs::tom::AN> *alp);
template <auto F>
static bool read_RQR_obj(ocs::tom::Element<ocs::tom::RQR> rule, const char *buffer,
                         ocs::tom::ListPtr<ocs::tom::AN> *alp);
template <auto F>
static bool write_RQR_obj(ocs::tom::ConstElement<ocs::tom::RQR> rule, dstring *buffer,
                          ocs::tom::ListPtr<ocs::tom::AN> *alp);
static bool read_SC_load_formula(ocs::tom::Element<ocs::tom::SC> sc, const char *buffer,
                                 ocs::tom::ListPtr<ocs::tom::AN> *alp);
static bool write_EH_auto_added_time(ocs::tom::ConstElement<ocs::tom::EH> host, dstring *buffer,
                                     ocs::tom::ListPtr<ocs::tom::AN> *alp);

/** @name The variant bits of the builders
 *
 * An entry of a table that a builder selects from names the bits it needs;
 * the builder sets the bits its arguments ask for.
 * @{
 */
static constexpr unsigned OCS_SPOOL_V_SPOOL = 1u << 0;      ///< the spool file layout, with the fields qmaster owns
static constexpr unsigned OCS_SPOOL_V_RECURSE = 1u << 1;    ///< a share tree node with its id and its children
static constexpr unsigned OCS_SPOOL_V_LOADS = 1u << 2;      ///< an execution host with its load values
static constexpr unsigned OCS_SPOOL_V_MORE_INFO = 1u << 3;  ///< an execution host with its actual complex values
static constexpr unsigned OCS_SPOOL_V_SHOW_MORE = 1u << 4;  ///< an execution host shown with the time it added itself
static constexpr unsigned OCS_SPOOL_V_HOSTLIST = 1u << 5;   ///< a cluster queue with its host list
static constexpr unsigned OCS_SPOOL_V_QU_SHOW = 1u << 6;    ///< a queue instance as `qconf -sq` shows it
static constexpr unsigned OCS_SPOOL_V_QU_FILE = 1u << 7;    ///< a queue instance as it is spooled
/** @} */

/* *INDENT-OFF* */

/* Field tables for context-independent spooling of sub-lists */
static constexpr std::tuple ocs_spool_AMEM_sub{
   OcsSpoolField<AMEM_href>{nullptr, 0},
   OcsSpoolField<AMEM_value>{nullptr, 0},
};

static constexpr std::tuple ocs_spool_ATIME_sub{
   OcsSpoolField<ATIME_href>{nullptr, 0},
   OcsSpoolField<ATIME_value>{nullptr, 0},
};

static constexpr std::tuple ocs_spool_PR_sub{
   OcsSpoolField<PR_name>{nullptr, 0},
};

static constexpr std::tuple ocs_spool_APRJLIST_sub{
   OcsSpoolField<APRJLIST_href>{nullptr, 0},
   OcsSpoolField<APRJLIST_value, ocs_spool_PR_sub>{nullptr, 0},
};

static constexpr std::tuple ocs_spool_SO_sub{
   OcsSpoolField<SO_name>{nullptr, 11},
   OcsSpoolField<SO_threshold>{nullptr, 11},
   OcsSpoolField<SO_slots_sum>{nullptr, 11},
   OcsSpoolField<SO_seq_no>{nullptr, 11},
   OcsSpoolField<SO_action>{nullptr, 11},
};

static constexpr std::tuple ocs_spool_ASOLIST_sub{
   OcsSpoolField<ASOLIST_href>{nullptr, 0},
   OcsSpoolField<ASOLIST_value, ocs_spool_SO_sub>{nullptr, 0},
};

static constexpr std::tuple ocs_spool_US_sub{
   OcsSpoolField<US_name>{nullptr, 0},
};

static constexpr std::tuple ocs_spool_AUSRLIST_sub{
   OcsSpoolField<AUSRLIST_href>{nullptr, 0},
   OcsSpoolField<AUSRLIST_value, ocs_spool_US_sub>{nullptr, 0},
};

static constexpr std::tuple ocs_spool_ABOOL_sub{
   OcsSpoolField<ABOOL_href>{nullptr, 0},
   OcsSpoolField<ABOOL_value>{nullptr, 0},
};

static constexpr std::tuple ocs_spool_ST_sub{
   OcsSpoolField<ST_name>{nullptr, 0},
};

#if 0
static spooling_field ST_sub_fields_binding[] = {
   {  ST_id,               0, nullptr,                false, nullptr, false, nullptr, nullptr},
   {  ST_name,             0, nullptr,                false, nullptr, false, nullptr, nullptr},
   {  NoName,              0, nullptr,                false, nullptr, false, nullptr, nullptr}
};
#endif

static constexpr std::tuple ocs_spool_ASTRLIST_sub{
   OcsSpoolField<ASTRLIST_href>{nullptr, 0},
   OcsSpoolField<ASTRLIST_value, ocs_spool_ST_sub>{nullptr, 0},
};

static constexpr std::tuple ocs_spool_AQTLIST_sub{
   OcsSpoolField<AQTLIST_href>{nullptr, 0},
   OcsSpoolField<AQTLIST_value>{nullptr, 0},
};

static constexpr std::tuple ocs_spool_ASTR_sub{
   OcsSpoolField<ASTR_href>{nullptr, 0},
   OcsSpoolField<ASTR_value>{nullptr, 0},
};

static constexpr std::tuple ocs_spool_AINTER_sub{
   OcsSpoolField<AINTER_href>{nullptr, 0},
   OcsSpoolField<AINTER_value>{nullptr, 0},
};

static constexpr std::tuple ocs_spool_CE_sub{
   OcsSpoolField<CE_name>{"name", 11},
   OcsSpoolField<CE_stringval>{"value", 11, nullptr, nullptr, write_CE_stringval},
};

/* in order to distinguish between host level and queue level consumables */
static constexpr std::tuple ocs_spool_CE_host_sub{
   OcsSpoolField<CE_name>{"name", 11},
   OcsSpoolField<CE_stringval>{"value", 11, nullptr, read_CE_stringval_host, write_CE_stringval_host},
};

static constexpr std::tuple ocs_spool_RUE_sub{
   OcsSpoolField<RUE_name>{nullptr, 11},
   OcsSpoolField<RUE_utilized_now>{nullptr, 11},
};

static constexpr std::tuple ocs_spool_ACELIST_sub{
   OcsSpoolField<ACELIST_href>{nullptr, 0},
   OcsSpoolField<ACELIST_value, ocs_spool_CE_sub>{nullptr, 0},
};

static constexpr std::tuple ocs_spool_AULNG_sub{
   OcsSpoolField<AULNG_href>{nullptr, 0},
   OcsSpoolField<AULNG_value>{nullptr, 0},
};

static constexpr std::tuple ocs_spool_CF_sub{
   OcsSpoolField<CF_name>{nullptr, 28},
   OcsSpoolField<CF_value>{nullptr, 28, nullptr, read_CF_value},
};

static constexpr std::tuple ocs_spool_STN_sub{
   OcsSpoolField<STN_id>{nullptr, 0},
};

static constexpr std::tuple ocs_spool_HS_sub{
   OcsSpoolField<HS_name>{nullptr, 0},
   OcsSpoolField<HS_value>{nullptr, 0},
};

static constexpr std::tuple ocs_spool_RU_sub{
   OcsSpoolField<RU_job_number>{nullptr, 0},
   OcsSpoolField<RU_task_number>{nullptr, 0},
   OcsSpoolField<RU_state>{nullptr, 0},
};

static constexpr std::tuple ocs_spool_HL_sub{
   OcsSpoolField<HL_name>{nullptr, 0},
   OcsSpoolField<HL_value>{nullptr, 0},
};

static constexpr std::tuple ocs_spool_STU_sub{
   OcsSpoolField<STU_name>{nullptr, 0},
};

static constexpr std::tuple ocs_spool_UE_sub{
   OcsSpoolField<UE_name>{nullptr, 0},
};

/* UA_svalue (string usage values, CS-849) is deliberately not listed here.
 * String usage values only ever occur in job accounting usage lists, which are
 * not spooled through this table. Every consumer of this table - the
 * scheduler config weight list and the sharetree usage lists, which are
 * filtered down to weight-list names by Usage::strip_irrelevant_usage() - is
 * numeric by construction, so an entry here would only ever emit NONE and read
 * that NONE back as a literal string value. */
static constexpr std::tuple ocs_spool_UA_sub{
   OcsSpoolField<UA_name>{nullptr, 0},
   OcsSpoolField<UA_value>{nullptr, 0},
};

static constexpr std::tuple ocs_spool_UPP_sub{
   OcsSpoolField<UPP_name>{nullptr, 0},
   OcsSpoolField<UPP_usage, ocs_spool_UA_sub>{nullptr, 0},
   OcsSpoolField<UPP_long_term_usage, ocs_spool_UA_sub>{nullptr, 0},
};

static constexpr std::tuple ocs_spool_UPU_sub{
   OcsSpoolField<UPU_job_number>{nullptr, 0},
   OcsSpoolField<UPU_old_usage_list, ocs_spool_UA_sub>{nullptr, 0, &qconf_sub_name_value_comma_sfi},
};

static constexpr std::tuple ocs_spool_HR_sub{
   OcsSpoolField<HR_name>{nullptr, 0},
};

static constexpr std::tuple ocs_spool_RQRL_sub{
   OcsSpoolField<RQRL_name>{nullptr, 0},
   OcsSpoolField<RQRL_value>{nullptr, 0},
};

/* CS-2313a: structural JSON shape of an RQS filter (RQRF). The ASCII path renders
 * these via write_RQR_obj/read_RQR_obj (the flat "{a,b,!c}" string and takes
 * precedence there); the JSON path instead recurses these sub_fields, emitting
 * { "expand": <bool>, "scope": [..], "xscope": [..] }. */
static constexpr std::tuple ocs_spool_RQRF_sub{
   OcsSpoolField<RQRF_expand>{"expand", 0},
   OcsSpoolField<RQRF_scope, ocs_spool_ST_sub>{"scope", 0},
   OcsSpoolField<RQRF_xscope, ocs_spool_ST_sub>{"xscope", 0},
};

static constexpr std::tuple ocs_spool_RQR_sub{
   OcsSpoolField<RQR_name>{"name", 0},
   OcsSpoolField<RQR_filter_users, ocs_spool_RQRF_sub>{"users", 0, nullptr, read_RQR_obj<RQR_filter_users>, write_RQR_obj<RQR_filter_users>},
   OcsSpoolField<RQR_filter_projects, ocs_spool_RQRF_sub>{"projects", 0, nullptr, read_RQR_obj<RQR_filter_projects>, write_RQR_obj<RQR_filter_projects>},
   OcsSpoolField<RQR_filter_pes, ocs_spool_RQRF_sub>{"pes", 0, nullptr, read_RQR_obj<RQR_filter_pes>, write_RQR_obj<RQR_filter_pes>},
   OcsSpoolField<RQR_filter_queues, ocs_spool_RQRF_sub>{"queues", 0, nullptr, read_RQR_obj<RQR_filter_queues>, write_RQR_obj<RQR_filter_queues>},
   OcsSpoolField<RQR_filter_hosts, ocs_spool_RQRF_sub>{"hosts", 0, nullptr, read_RQR_obj<RQR_filter_hosts>, write_RQR_obj<RQR_filter_hosts>},
   OcsSpoolField<RQR_limit, ocs_spool_RQRL_sub>{"to", 0, &qconf_sub_name_value_comma_sfi},
};

static constexpr std::tuple ocs_spool_RN_sub{
   OcsSpoolField<RN_min>{nullptr, 0},
   OcsSpoolField<RN_max>{nullptr, 0},
   OcsSpoolField<RN_step>{nullptr, 0},
};

static constexpr std::tuple ocs_spool_QR_sub{
   OcsSpoolField<QR_name>{nullptr, 0},
};

static constexpr std::tuple ocs_spool_JG_sub{
   OcsSpoolField<JG_qname>{nullptr, 0},
   OcsSpoolField<JG_slots>{nullptr, 0},
// OcsSpoolField<JG_binding_to_use, ST_sub_fields_binding>{nullptr, 0, &qconf_sub_sub_name_value_comma_sfi},
};

static constexpr std::tuple ocs_spool_MR_sub{
   OcsSpoolField<MR_user>{nullptr, 0},
   OcsSpoolField<MR_host>{nullptr, 0},
};

static constexpr std::tuple ocs_spool_ARA_sub{
   OcsSpoolField<ARA_name>{nullptr, 0},
   OcsSpoolField<ARA_group>{nullptr, 0},
};

/* Field tables for context-independent object spooling */
static constexpr std::tuple ocs_spool_CE_table{
   OcsSpoolField<CE_name>{"name", 11},
   OcsSpoolField<CE_shortcut>{"shortcut", 11},
   OcsSpoolField<CE_valtype>{"type", 11},
   OcsSpoolField<CE_relop>{"relop", 11},
   OcsSpoolField<CE_requestable>{"requestable", 11},
   OcsSpoolField<CE_consumable>{"consumable", 11},
   OcsSpoolField<CE_defaultval>{"default", 11},
   OcsSpoolField<CE_urgency_weight>{"urgency", 11},
};

static constexpr std::tuple ocs_spool_CAT_table{
   OcsSpoolField<CT_id>{"id", 10},
   OcsSpoolField<CT_refcount>{"rcount", 10},
   OcsSpoolField<CT_str>{"str", 10},
};

static constexpr std::tuple ocs_spool_CAL_table{
   OcsSpoolField<CAL_name>{"calendar_name", 16},
   OcsSpoolField<CAL_year_calendar>{"year", 16},
   OcsSpoolField<CAL_week_calendar>{"week", 16},
};

static constexpr std::tuple ocs_spool_CK_table{
   OcsSpoolField<CK_name>{"ckpt_name", 18},
   OcsSpoolField<CK_interface>{"interface", 18},
   OcsSpoolField<CK_ckpt_command>{"ckpt_command", 18},
   OcsSpoolField<CK_migr_command>{"migr_command", 18},
   OcsSpoolField<CK_rest_command>{"restart_command", 18},
   OcsSpoolField<CK_clean_command>{"clean_command", 18},
   OcsSpoolField<CK_ckpt_dir>{"ckpt_dir", 18},
   OcsSpoolField<CK_signal>{"signal", 18},
   OcsSpoolField<CK_when>{"when", 18},
};

static constexpr std::tuple ocs_spool_HGRP_table{
   OcsSpoolField<HGRP_name>{"group_name", 0},
   OcsSpoolField<HGRP_host_list, ocs_spool_HR_sub>{"hostlist", 0},
};

static constexpr std::tuple ocs_spool_US_table{
   OcsSpoolField<US_name>{"name", 7},
   OcsSpoolField<US_type>{"type", 7},
   OcsSpoolField<US_fshare, ocs_spool_US_sub>{"fshare", 7},
   OcsSpoolField<US_oticket, ocs_spool_US_sub>{"oticket", 7},
   OcsSpoolField<US_entries, ocs_spool_UE_sub>{"entries", 7},
};

static constexpr std::tuple ocs_spool_SC_table{
   OcsSpoolField<SC_algorithm>{"algorithm", 33},
   OcsSpoolField<SC_schedule_interval>{"schedule_interval", 33},
   OcsSpoolField<SC_maxujobs>{"maxujobs", 33},
   OcsSpoolField<SC_queue_sort_method>{"queue_sort_method", 33, nullptr, read_SC_queue_sort_method, write_SC_queue_sort_method},
   OcsSpoolField<SC_job_load_adjustments, ocs_spool_CE_sub>{"job_load_adjustments", 33},
   OcsSpoolField<SC_load_adjustment_decay_time, ocs_spool_CE_sub>{"load_adjustment_decay_time", 33},
   OcsSpoolField<SC_load_formula>{"load_formula", 33, nullptr, read_SC_load_formula},
   OcsSpoolField<SC_schedd_job_info>{"schedd_job_info", 33},
   OcsSpoolField<SC_flush_submit_sec>{"flush_submit_sec", 33},
   OcsSpoolField<SC_flush_finish_sec>{"flush_finish_sec", 33},
   OcsSpoolField<SC_params>{"params", 33},
   OcsSpoolField<SC_reprioritize_interval>{"reprioritize_interval", 33},
   OcsSpoolField<SC_halftime>{"halftime", 33},
   OcsSpoolField<SC_usage_weight_list, ocs_spool_UA_sub>{"usage_weight_list", 33},
   OcsSpoolField<SC_compensation_factor>{"compensation_factor", 33},
   OcsSpoolField<SC_weight_user>{"weight_user", 33},
   OcsSpoolField<SC_weight_project>{"weight_project", 33},
   OcsSpoolField<SC_weight_department>{"weight_department", 33},
   OcsSpoolField<SC_weight_job>{"weight_job", 33},
   OcsSpoolField<SC_weight_tickets_functional>{"weight_tickets_functional", 33},
   OcsSpoolField<SC_weight_tickets_share>{"weight_tickets_share", 33},
   OcsSpoolField<SC_share_override_tickets>{"share_override_tickets", 33},
   OcsSpoolField<SC_share_functional_shares>{"share_functional_shares", 33},
   OcsSpoolField<SC_max_functional_jobs_to_schedule>{"max_functional_jobs_to_schedule", 33},
   OcsSpoolField<SC_report_pjob_tickets>{"report_pjob_tickets", 33},
   OcsSpoolField<SC_max_pending_tasks_per_job>{"max_pending_tasks_per_job", 33},
   OcsSpoolField<SC_halflife_decay_list>{"halflife_decay_list", 33},
   OcsSpoolField<SC_policy_hierarchy>{"policy_hierarchy", 33},
   OcsSpoolField<SC_weight_ticket>{"weight_ticket", 33},
   OcsSpoolField<SC_weight_waiting_time>{"weight_waiting_time", 33},
   OcsSpoolField<SC_weight_deadline>{"weight_deadline", 33},
   OcsSpoolField<SC_weight_urgency>{"weight_urgency", 33},
   OcsSpoolField<SC_weight_priority>{"weight_priority", 33},
   OcsSpoolField<SC_max_reservation>{"max_reservation", 33},
   OcsSpoolField<SC_default_duration>{"default_duration", 33},
};

/// the layout of every per host attribute list of a cluster queue
static constexpr const spool_flatfile_instr *CQ_SFI = &qconf_sub_name_value_comma_braced_sfi;

static constexpr std::tuple ocs_spool_CQ_table{
   OcsSpoolField<CQ_name>{"qname", 21},
   OcsSpoolField<CQ_hostlist, ocs_spool_HR_sub>{"hostlist", 21, nullptr, read_CQ_hostlist, write_CQ_hostlist, OCS_SPOOL_V_HOSTLIST},
   OcsSpoolField<CQ_seq_no, ocs_spool_AULNG_sub>{"seq_no", 21, CQ_SFI, read_CQ_attr_list<CQ_seq_no>, write_CQ_attr_list<CQ_seq_no>},
   OcsSpoolField<CQ_load_thresholds, ocs_spool_ACELIST_sub>{"load_thresholds", 21, CQ_SFI, read_CQ_attr_list<CQ_load_thresholds>, write_CQ_attr_list<CQ_load_thresholds>},
   OcsSpoolField<CQ_suspend_thresholds, ocs_spool_ACELIST_sub>{"suspend_thresholds", 21, CQ_SFI, read_CQ_attr_list<CQ_suspend_thresholds>, write_CQ_attr_list<CQ_suspend_thresholds>},
   OcsSpoolField<CQ_nsuspend, ocs_spool_AULNG_sub>{"nsuspend", 21, CQ_SFI, read_CQ_attr_list<CQ_nsuspend>, write_CQ_attr_list<CQ_nsuspend>},
   OcsSpoolField<CQ_suspend_interval, ocs_spool_AINTER_sub>{"suspend_interval", 21, CQ_SFI, read_CQ_attr_list<CQ_suspend_interval>, write_CQ_attr_list<CQ_suspend_interval>},
   OcsSpoolField<CQ_priority, ocs_spool_ASTR_sub>{"priority", 21, CQ_SFI, read_CQ_attr_list<CQ_priority>, write_CQ_attr_list<CQ_priority>},
   OcsSpoolField<CQ_min_cpu_interval, ocs_spool_AINTER_sub>{"min_cpu_interval", 21, CQ_SFI, read_CQ_attr_list<CQ_min_cpu_interval>, write_CQ_attr_list<CQ_min_cpu_interval>},
   OcsSpoolField<CQ_processors, ocs_spool_ASTR_sub>{"processors", 21, CQ_SFI, read_CQ_attr_list<CQ_processors>, write_CQ_attr_list<CQ_processors>},
   OcsSpoolField<CQ_qtype, ocs_spool_AQTLIST_sub>{"qtype", 21, CQ_SFI, read_CQ_attr_list<CQ_qtype>, write_CQ_attr_list<CQ_qtype>},
   OcsSpoolField<CQ_ckpt_list, ocs_spool_ASTRLIST_sub>{"ckpt_list", 21, CQ_SFI, read_CQ_attr_list<CQ_ckpt_list>, write_CQ_attr_list<CQ_ckpt_list>},
   OcsSpoolField<CQ_pe_list, ocs_spool_ASTRLIST_sub>{"pe_list", 21, CQ_SFI, read_CQ_attr_list<CQ_pe_list>, write_CQ_attr_list<CQ_pe_list>},
   OcsSpoolField<CQ_rerun, ocs_spool_ABOOL_sub>{"rerun", 21, CQ_SFI, read_CQ_attr_list<CQ_rerun>, write_CQ_attr_list<CQ_rerun>},
   OcsSpoolField<CQ_job_slots, ocs_spool_AULNG_sub>{"slots", 21, CQ_SFI, read_CQ_attr_list<CQ_job_slots>, write_CQ_attr_list<CQ_job_slots>},
   OcsSpoolField<CQ_tmpdir, ocs_spool_ASTR_sub>{"tmpdir", 21, CQ_SFI, read_CQ_attr_list<CQ_tmpdir>, write_CQ_attr_list<CQ_tmpdir>},
   OcsSpoolField<CQ_shell, ocs_spool_ASTR_sub>{"shell", 21, CQ_SFI, read_CQ_attr_list<CQ_shell>, write_CQ_attr_list<CQ_shell>},
   OcsSpoolField<CQ_prolog, ocs_spool_ASTR_sub>{"prolog", 21, CQ_SFI, read_CQ_attr_list<CQ_prolog>, write_CQ_attr_list<CQ_prolog>},
   OcsSpoolField<CQ_epilog, ocs_spool_ASTR_sub>{"epilog", 21, CQ_SFI, read_CQ_attr_list<CQ_epilog>, write_CQ_attr_list<CQ_epilog>},
   OcsSpoolField<CQ_shell_start_mode, ocs_spool_ASTR_sub>{"shell_start_mode", 21, CQ_SFI, read_CQ_attr_list<CQ_shell_start_mode>, write_CQ_attr_list<CQ_shell_start_mode>},
   OcsSpoolField<CQ_starter_method, ocs_spool_ASTR_sub>{"starter_method", 21, CQ_SFI, read_CQ_attr_list<CQ_starter_method>, write_CQ_attr_list<CQ_starter_method>},
   OcsSpoolField<CQ_suspend_method, ocs_spool_ASTR_sub>{"suspend_method", 21, CQ_SFI, read_CQ_attr_list<CQ_suspend_method>, write_CQ_attr_list<CQ_suspend_method>},
   OcsSpoolField<CQ_resume_method, ocs_spool_ASTR_sub>{"resume_method", 21, CQ_SFI, read_CQ_attr_list<CQ_resume_method>, write_CQ_attr_list<CQ_resume_method>},
   OcsSpoolField<CQ_terminate_method, ocs_spool_ASTR_sub>{"terminate_method", 21, CQ_SFI, read_CQ_attr_list<CQ_terminate_method>, write_CQ_attr_list<CQ_terminate_method>},
   OcsSpoolField<CQ_notify, ocs_spool_AINTER_sub>{"notify", 21, CQ_SFI, read_CQ_attr_list<CQ_notify>, write_CQ_attr_list<CQ_notify>},
   OcsSpoolField<CQ_owner_list, ocs_spool_AUSRLIST_sub>{"owner_list", 21, CQ_SFI, read_CQ_attr_list<CQ_owner_list>, write_CQ_attr_list<CQ_owner_list>},
   OcsSpoolField<CQ_acl, ocs_spool_AUSRLIST_sub>{"user_lists", 21, CQ_SFI, read_CQ_attr_list<CQ_acl>, write_CQ_attr_list<CQ_acl>},
   OcsSpoolField<CQ_xacl, ocs_spool_AUSRLIST_sub>{"xuser_lists", 21, CQ_SFI, read_CQ_attr_list<CQ_xacl>, write_CQ_attr_list<CQ_xacl>},
   OcsSpoolField<CQ_subordinate_list, ocs_spool_ASOLIST_sub>{"subordinate_list", 21, CQ_SFI, read_CQ_attr_list<CQ_subordinate_list>, write_CQ_attr_list<CQ_subordinate_list>},
   OcsSpoolField<CQ_consumable_config_list, ocs_spool_ACELIST_sub>{"complex_values", 21, CQ_SFI, read_CQ_attr_list<CQ_consumable_config_list>, write_CQ_attr_list<CQ_consumable_config_list>},
   OcsSpoolField<CQ_projects, ocs_spool_APRJLIST_sub>{"projects", 21, CQ_SFI, read_CQ_attr_list<CQ_projects>, write_CQ_attr_list<CQ_projects>},
   OcsSpoolField<CQ_xprojects, ocs_spool_APRJLIST_sub>{"xprojects", 21, CQ_SFI, read_CQ_attr_list<CQ_xprojects>, write_CQ_attr_list<CQ_xprojects>},
   OcsSpoolField<CQ_calendar, ocs_spool_ASTR_sub>{"calendar", 21, CQ_SFI, read_CQ_attr_list<CQ_calendar>, write_CQ_attr_list<CQ_calendar>},
   OcsSpoolField<CQ_initial_state, ocs_spool_ASTR_sub>{"initial_state", 21, CQ_SFI, read_CQ_attr_list<CQ_initial_state>, write_CQ_attr_list<CQ_initial_state>},
   OcsSpoolField<CQ_s_rt, ocs_spool_ATIME_sub>{"s_rt", 21, CQ_SFI, read_CQ_attr_list<CQ_s_rt>, write_CQ_attr_list<CQ_s_rt>},
   OcsSpoolField<CQ_h_rt, ocs_spool_ATIME_sub>{"h_rt", 21, CQ_SFI, read_CQ_attr_list<CQ_h_rt>, write_CQ_attr_list<CQ_h_rt>},
   OcsSpoolField<CQ_s_cpu, ocs_spool_ATIME_sub>{"s_cpu", 21, CQ_SFI, read_CQ_attr_list<CQ_s_cpu>, write_CQ_attr_list<CQ_s_cpu>},
   OcsSpoolField<CQ_h_cpu, ocs_spool_ATIME_sub>{"h_cpu", 21, CQ_SFI, read_CQ_attr_list<CQ_h_cpu>, write_CQ_attr_list<CQ_h_cpu>},
   OcsSpoolField<CQ_s_fsize, ocs_spool_AMEM_sub>{"s_fsize", 21, CQ_SFI, read_CQ_attr_list<CQ_s_fsize>, write_CQ_attr_list<CQ_s_fsize>},
   OcsSpoolField<CQ_h_fsize, ocs_spool_AMEM_sub>{"h_fsize", 21, CQ_SFI, read_CQ_attr_list<CQ_h_fsize>, write_CQ_attr_list<CQ_h_fsize>},
   OcsSpoolField<CQ_s_data, ocs_spool_AMEM_sub>{"s_data", 21, CQ_SFI, read_CQ_attr_list<CQ_s_data>, write_CQ_attr_list<CQ_s_data>},
   OcsSpoolField<CQ_h_data, ocs_spool_AMEM_sub>{"h_data", 21, CQ_SFI, read_CQ_attr_list<CQ_h_data>, write_CQ_attr_list<CQ_h_data>},
   OcsSpoolField<CQ_s_stack, ocs_spool_AMEM_sub>{"s_stack", 21, CQ_SFI, read_CQ_attr_list<CQ_s_stack>, write_CQ_attr_list<CQ_s_stack>},
   OcsSpoolField<CQ_h_stack, ocs_spool_AMEM_sub>{"h_stack", 21, CQ_SFI, read_CQ_attr_list<CQ_h_stack>, write_CQ_attr_list<CQ_h_stack>},
   OcsSpoolField<CQ_s_core, ocs_spool_AMEM_sub>{"s_core", 21, CQ_SFI, read_CQ_attr_list<CQ_s_core>, write_CQ_attr_list<CQ_s_core>},
   OcsSpoolField<CQ_h_core, ocs_spool_AMEM_sub>{"h_core", 21, CQ_SFI, read_CQ_attr_list<CQ_h_core>, write_CQ_attr_list<CQ_h_core>},
   OcsSpoolField<CQ_s_rss, ocs_spool_AMEM_sub>{"s_rss", 21, CQ_SFI, read_CQ_attr_list<CQ_s_rss>, write_CQ_attr_list<CQ_s_rss>},
   OcsSpoolField<CQ_h_rss, ocs_spool_AMEM_sub>{"h_rss", 21, CQ_SFI, read_CQ_attr_list<CQ_h_rss>, write_CQ_attr_list<CQ_h_rss>},
   OcsSpoolField<CQ_s_vmem, ocs_spool_AMEM_sub>{"s_vmem", 21, CQ_SFI, read_CQ_attr_list<CQ_s_vmem>, write_CQ_attr_list<CQ_s_vmem>},
   OcsSpoolField<CQ_h_vmem, ocs_spool_AMEM_sub>{"h_vmem", 21, CQ_SFI, read_CQ_attr_list<CQ_h_vmem>, write_CQ_attr_list<CQ_h_vmem>},
};

/// the layout of the sublists of an advance reservation and an execution host
static constexpr const spool_flatfile_instr *COMMA_SFI = &qconf_sub_name_value_comma_sfi;

static constexpr std::tuple ocs_spool_AR_table{
   OcsSpoolField<AR_id>{"id", 20},
   OcsSpoolField<AR_name>{"name", 20},
   OcsSpoolField<AR_account>{"account", 20},
   OcsSpoolField<AR_project>{"project", 20},
   OcsSpoolField<AR_owner>{"owner", 20},
   OcsSpoolField<AR_group>{"group", 20},
   OcsSpoolField<AR_submission_time>{"submission_time", 20},
   OcsSpoolField<AR_start_time>{"start_time", 20},
   OcsSpoolField<AR_end_time>{"end_time", 20},
   OcsSpoolField<AR_duration>{"duration", 20},
   OcsSpoolField<AR_verify>{"verify", 20},
   OcsSpoolField<AR_error_handling>{"error_handling", 20},
   OcsSpoolField<AR_state>{"state", 20},
   OcsSpoolField<AR_checkpoint_name>{"checkpoint_name", 20},
   OcsSpoolField<AR_resource_list, ocs_spool_CE_sub>{"resource_list", 20, COMMA_SFI},
   OcsSpoolField<AR_queue_list, ocs_spool_QR_sub>{"queue_list", 20},
   OcsSpoolField<AR_granted_slots, ocs_spool_JG_sub>{"granted_slots", 20, COMMA_SFI},
   OcsSpoolField<AR_mail_options>{"mail_options", 20},
   OcsSpoolField<AR_mail_list, ocs_spool_MR_sub>{"mail_list", 20, COMMA_SFI},
   OcsSpoolField<AR_pe>{"pe", 20},
   OcsSpoolField<AR_pe_range, ocs_spool_RN_sub>{"pe_range", 20, COMMA_SFI},
   OcsSpoolField<AR_granted_pe>{"granted_pe", 20},
   OcsSpoolField<AR_master_queue_list, ocs_spool_QR_sub>{"master_queue_list", 20},
   OcsSpoolField<AR_acl_list, ocs_spool_ARA_sub>{"acl_list", 20, COMMA_SFI},
   OcsSpoolField<AR_xacl_list, ocs_spool_ARA_sub>{"xacl_list", 20, COMMA_SFI},
   OcsSpoolField<AR_type>{"type", 20},
};

static constexpr std::tuple ocs_spool_PE_table{
   OcsSpoolField<PE_name>{"pe_name", 20},
   OcsSpoolField<PE_slots>{"slots", 20},
   OcsSpoolField<PE_user_list, ocs_spool_US_sub>{"user_lists", 20},
   OcsSpoolField<PE_xuser_list, ocs_spool_US_sub>{"xuser_lists", 20},
   OcsSpoolField<PE_start_proc_args>{"start_proc_args", 20},
   OcsSpoolField<PE_stop_proc_args>{"stop_proc_args", 20},
   OcsSpoolField<PE_allocation_rule>{"allocation_rule", 20},
   OcsSpoolField<PE_control_slaves>{"control_slaves", 20},
   OcsSpoolField<PE_job_is_first_task>{"job_is_first_task", 20},
   OcsSpoolField<PE_urgency_slots>{"urgency_slots", 20},
   OcsSpoolField<PE_accounting_summary>{"accounting_summary", 20},
   OcsSpoolField<PE_ignore_slave_requests_on_master_host>{"ign_sreq_on_mhost", 20},
   OcsSpoolField<PE_master_forks_slaves>{"master_forks_slaves", 20},
   OcsSpoolField<PE_daemon_forks_slaves>{"daemon_forks_slaves", 20},
};

static constexpr std::tuple ocs_spool_RL_table{
   OcsSpoolField<RL_name>{"name", 20},
   OcsSpoolField<RL_enabled>{"enabled", 20},
   OcsSpoolField<RL_user_list, ocs_spool_US_sub>{"user_list", 20},
   OcsSpoolField<RL_parent_role_list, ocs_spool_ST_sub>{"parent_role_list", 20},
   OcsSpoolField<RL_perm_list>{"perm_list", 20},
};

static constexpr std::tuple ocs_spool_RQS_table{
   OcsSpoolField<RQS_name>{"name", 12},
   OcsSpoolField<RQS_description>{"description", 12},
   OcsSpoolField<RQS_enabled>{"enabled", 12},
   OcsSpoolField<RQS_rule, ocs_spool_RQR_sub>{"limit", 12, &qconf_sub_rqs_sfi},
};

/* Field tables whose rows a builder selects per call */
static constexpr std::tuple ocs_spool_PR_table{
   OcsSpoolField<PR_name>{"name", 0},
   OcsSpoolField<PR_oticket>{"oticket", 0},
   OcsSpoolField<PR_fshare>{"fshare", 0},
   OcsSpoolField<PR_usage, ocs_spool_UA_sub>{"usage", 0, &qconf_sub_name_value_space_sfi, nullptr, nullptr, OCS_SPOOL_V_SPOOL},
   OcsSpoolField<PR_usage_time_stamp>{"usage_time_stamp", 0, nullptr, nullptr, nullptr, OCS_SPOOL_V_SPOOL},
   OcsSpoolField<PR_long_term_usage, ocs_spool_UA_sub>{"long_term_usage", 0, &qconf_sub_name_value_space_sfi, nullptr, nullptr, OCS_SPOOL_V_SPOOL},
   OcsSpoolField<PR_project, ocs_spool_UPP_sub>{"project", 0, &qconf_sub_spool_usage_sfi, nullptr, nullptr, OCS_SPOOL_V_SPOOL},
   OcsSpoolField<PR_acl, ocs_spool_US_sub>{"acl", 0},
   OcsSpoolField<PR_xacl, ocs_spool_US_sub>{"xacl", 0},
   OcsSpoolField<PR_debited_job_usage, ocs_spool_UPU_sub>{"debited_job_usage", 0, &qconf_sub_spool_usage_sfi, nullptr, nullptr, OCS_SPOOL_V_SPOOL},
};

static constexpr std::tuple ocs_spool_UU_table{
   OcsSpoolField<UU_name>{"name", 0},
   OcsSpoolField<UU_oticket>{"oticket", 0},
   OcsSpoolField<UU_fshare>{"fshare", 0},
   OcsSpoolField<UU_delete_time>{"delete_time", 0},
   OcsSpoolField<UU_usage, ocs_spool_UA_sub>{"usage", 0, &qconf_sub_name_value_space_sfi, nullptr, nullptr, OCS_SPOOL_V_SPOOL},
   OcsSpoolField<UU_usage_time_stamp>{"usage_time_stamp", 0, nullptr, nullptr, nullptr, OCS_SPOOL_V_SPOOL},
   OcsSpoolField<UU_long_term_usage, ocs_spool_UA_sub>{"long_term_usage", 0, &qconf_sub_name_value_space_sfi, nullptr, nullptr, OCS_SPOOL_V_SPOOL},
   OcsSpoolField<UU_project, ocs_spool_UPP_sub>{"project", 0, &qconf_sub_spool_usage_sfi, nullptr, nullptr, OCS_SPOOL_V_SPOOL},
   OcsSpoolField<UU_default_project>{"default_project", 0},
   OcsSpoolField<UU_debited_job_usage, ocs_spool_UPU_sub>{"debited_job_usage", 0, &qconf_sub_spool_usage_sfi, nullptr, nullptr, OCS_SPOOL_V_SPOOL},
};

static constexpr std::tuple ocs_spool_STN_table{
   OcsSpoolField<STN_id>{"id", 0, nullptr, nullptr, nullptr, OCS_SPOOL_V_RECURSE},
   OcsSpoolField<STN_version>{"version", 0, nullptr, nullptr, nullptr, OCS_SPOOL_V_SPOOL},
   OcsSpoolField<STN_name>{"name", 0},
   OcsSpoolField<STN_type>{"type", 0},
   OcsSpoolField<STN_shares>{"shares", 0},
   OcsSpoolField<STN_children, ocs_spool_STN_sub>{"childnodes", 0, nullptr, nullptr, nullptr, OCS_SPOOL_V_RECURSE},
};

/* The time at which a host added itself has two rows: the spool file carries
 * the raw value, as the classic format separates the name of an attribute from
 * its value by a blank and a formatted date and time could not be read back;
 * qconf -se with more info shows it formatted. It is not part of the field
 * list qconf -Ae and qconf -Me use, as it is not meant to be edited by hand.
 * The variants of the two rows exclude each other. */
static constexpr std::tuple ocs_spool_EH_table{
   OcsSpoolField<EH_name>{"hostname", 21},
   OcsSpoolField<EH_scaling_list, ocs_spool_HS_sub>{"load_scaling", 21, COMMA_SFI},
   OcsSpoolField<EH_consumable_config_list, ocs_spool_CE_host_sub>{"complex_values", 21, COMMA_SFI},
   OcsSpoolField<EH_resource_utilization, ocs_spool_RUE_sub>{"complex_values_actual", 21, COMMA_SFI, nullptr, nullptr, OCS_SPOOL_V_MORE_INFO},
   OcsSpoolField<EH_load_list, ocs_spool_HL_sub>{"load_values", 21, COMMA_SFI, nullptr, nullptr, OCS_SPOOL_V_LOADS},
   OcsSpoolField<EH_processors>{"processors", 21, nullptr, nullptr, nullptr, OCS_SPOOL_V_LOADS},
   OcsSpoolField<EH_reschedule_unknown_list, ocs_spool_RU_sub>{"reschedule_unknown_list", 21, COMMA_SFI, nullptr, nullptr, OCS_SPOOL_V_SPOOL},
   OcsSpoolField<EH_auto_added_time>{"auto_added_time", 21, nullptr, nullptr, nullptr, OCS_SPOOL_V_SPOOL},
   OcsSpoolField<EH_auto_added_time>{"auto_added_time", 21, nullptr, nullptr, write_EH_auto_added_time, OCS_SPOOL_V_SHOW_MORE},
   OcsSpoolField<EH_acl, ocs_spool_US_sub>{"user_lists", 21},
   OcsSpoolField<EH_xacl, ocs_spool_US_sub>{"xuser_lists", 21},
   OcsSpoolField<EH_prj, ocs_spool_PR_sub>{"projects", 21},
   OcsSpoolField<EH_xprj, ocs_spool_PR_sub>{"xprojects", 21},
   OcsSpoolField<EH_usage_scaling_list, ocs_spool_HS_sub>{"usage_scaling", 21, COMMA_SFI},
   OcsSpoolField<EH_report_variables, ocs_spool_STU_sub>{"report_variables", 21, COMMA_SFI},
};

static constexpr std::tuple ocs_spool_CONF_table{
   OcsSpoolField<CONF_name>{"conf_name", 28, nullptr, nullptr, nullptr, OCS_SPOOL_V_SPOOL},
   OcsSpoolField<CONF_version>{"conf_version", 28, nullptr, nullptr, nullptr, OCS_SPOOL_V_SPOOL},
   OcsSpoolField<CONF_entries, ocs_spool_CF_sub>{nullptr, 28, &qconf_sub_param_sfi},
};

static constexpr unsigned QS = OCS_SPOOL_V_QU_SHOW;   ///< short for the rows of qconf -sq
static constexpr unsigned QF = OCS_SPOOL_V_QU_FILE;   ///< short for the rows of the spool file

static constexpr std::tuple ocs_spool_QU_table{
   OcsSpoolField<QU_qname>{"qname", 21},
   OcsSpoolField<QU_qhostname>{"hostname", 21},
   OcsSpoolField<QU_seq_no>{"seq_no", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_load_thresholds, ocs_spool_CE_sub>{"load_thresholds", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_suspend_thresholds, ocs_spool_CE_sub>{"suspend_thresholds", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_nsuspend>{"nsuspend", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_suspend_interval>{"suspend_interval", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_priority>{"priority", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_min_cpu_interval>{"min_cpu_interval", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_processors>{"processors", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_qtype>{"qtype", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_ckpt_list, ocs_spool_ST_sub>{"ckpt_list", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_pe_list, ocs_spool_ST_sub>{"pe_list", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_rerun>{"rerun", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_job_slots>{"slots", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_tmpdir>{"tmpdir", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_shell>{"shell", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_prolog>{"prolog", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_epilog>{"epilog", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_shell_start_mode>{"shell_start_mode", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_starter_method>{"starter_method", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_suspend_method>{"suspend_method", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_resume_method>{"resume_method", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_terminate_method>{"terminate_method", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_notify>{"notify", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_owner_list, ocs_spool_US_sub>{"owner_list", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_acl, ocs_spool_US_sub>{"user_lists", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_xacl, ocs_spool_US_sub>{"xuser_lists", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_subordinate_list, ocs_spool_SO_sub>{"subordinate_list", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_consumable_config_list, ocs_spool_CE_sub>{"complex_values", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_projects, ocs_spool_PR_sub>{"projects", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_xprojects, ocs_spool_PR_sub>{"xprojects", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_calendar>{"calendar", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_initial_state>{"initial_state", 21, nullptr, nullptr, nullptr, QS},
#if 0
   OcsSpoolField<QU_fshare>{"fshare", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_oticket>{"oticket", 21, nullptr, nullptr, nullptr, QS},
#endif
   OcsSpoolField<QU_s_rt>{"s_rt", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_h_rt>{"h_rt", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_s_cpu>{"s_cpu", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_h_cpu>{"h_cpu", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_s_fsize>{"s_fsize", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_h_fsize>{"h_fsize", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_s_data>{"s_data", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_h_data>{"h_data", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_s_stack>{"s_stack", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_h_stack>{"h_stack", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_s_core>{"s_core", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_h_core>{"h_core", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_s_rss>{"s_rss", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_h_rss>{"h_rss", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_s_vmem>{"s_vmem", 21, nullptr, nullptr, nullptr, QS},
   OcsSpoolField<QU_h_vmem>{"h_vmem", 21, nullptr, nullptr, nullptr, QS},
   /* the spool file holds only the attributes a queue instance does not take from its cluster queue */
   OcsSpoolField<QU_state>{"state", 21, nullptr, nullptr, nullptr, QF},
   OcsSpoolField<QU_pending_signal>{"pending_signal", 21, nullptr, nullptr, nullptr, QF},
   OcsSpoolField<QU_pending_signal_delivery_time>{"pending_signal_del", 21, nullptr, nullptr, nullptr, QF},
   OcsSpoolField<QU_version>{"version", 21, nullptr, nullptr, nullptr, QF},
   /* SG: not supported */
#if 0
   OcsSpoolField<QU_error_messages>{"error_messages", 21, nullptr, nullptr, nullptr, QF},
#endif
};

/* *INDENT-ON* */

/* The arrays of the tables whose rows never vary */
spooling_field *const CE_fields = ocs_spool_view_rows<ocs_spool_CE_table>();
spooling_field *const CAT_fields = ocs_spool_view_rows<ocs_spool_CAT_table>();
spooling_field *const CAL_fields = ocs_spool_view_rows<ocs_spool_CAL_table>();
spooling_field *const CK_fields = ocs_spool_view_rows<ocs_spool_CK_table>();
spooling_field *const HGRP_fields = ocs_spool_view_rows<ocs_spool_HGRP_table>();
spooling_field *const US_fields = ocs_spool_view_rows<ocs_spool_US_table>();
spooling_field *const SC_fields = ocs_spool_view_rows<ocs_spool_SC_table>();
spooling_field *const CQ_fields = ocs_spool_view_rows<ocs_spool_CQ_table>();
spooling_field *const AR_fields = ocs_spool_view_rows<ocs_spool_AR_table>();
spooling_field *const PE_fields = ocs_spool_view_rows<ocs_spool_PE_table>();
spooling_field *const RL_fields = ocs_spool_view_rows<ocs_spool_RL_table>();
spooling_field *const RQS_fields = ocs_spool_view_rows<ocs_spool_RQS_table>();

/**
 * @brief Copy the rows of a typed table a variant selects
 *
 * The type independent part of ocs_spool_select(). The copies own neither
 * their names nor their sub-tables; the views do.
 *
 * @param view the rows of the table and its terminating row
 * @param needs the variant bits each row needs
 * @param size the number of rows, without the terminating one
 * @param variant the variant bits the caller selects
 * @return the selected rows and the terminating row, allocated with sge_malloc()
 */
spooling_field *ocs_spool_select_rows(const spooling_field *view, const unsigned *needs, const size_t size,
                                      const unsigned variant) {
   size_t count = 0;
   for (size_t i = 0; i < size; i++) {
      if ((needs[i] & ~variant) == 0) {
         count++;
      }
   }

   auto *fields = reinterpret_cast<spooling_field *>(sge_malloc(sizeof(spooling_field) * (count + 1)));
   size_t j = 0;
   for (size_t i = 0; i < size; i++) {
      if ((needs[i] & ~variant) == 0) {
         fields[j++] = view[i];
      }
   }
   fields[j] = view[size];

   return fields;
}

/** @brief Fill one entry of a field list built at run time
 *
 * Only the self-referential share tree list for JSON is still built this way,
 * as its sub-table is the list itself.
 *
 * @param field the entry to fill
 * @param nm the field
 * @param width the width of the name column
 * @param name the name in the text
 * @param sub_fields the field list of a sublist, or nullptr
 * @param ops the typed code of the field, nullptr for the terminating entry
 */
static void create_spooling_field(spooling_field *field, const int nm, const int width, const char *name,
                                  spooling_field *sub_fields, const ocs_spool_field_ops *ops) {
   if (field != nullptr) {
      field->nm = nm;
      field->width = width;
      field->name = name;
      field->free_name = false;
      field->sub_fields = sub_fields;
      field->free_sub_fields = false;
      field->clientdata = nullptr;
      field->read_func = nullptr;
      field->write_func = nullptr;
      field->ops = ops;
   }
}

/** @brief Build the field list of a project
 * @param spool include the usage, the long term usage and the debited job usage
 * @return the field list, to be freed with #spool_free_spooling_fields
 */
spooling_field *sge_build_PR_field_list(const bool spool) {
   return ocs_spool_select<ocs_spool_PR_table>(spool ? OCS_SPOOL_V_SPOOL : 0);
}

/** @brief Build the field list of a user
 * @param spool include the accumulated usage, as for a project
 * @return the field list, to be freed with #spool_free_spooling_fields
 */
spooling_field *sge_build_UU_field_list(const bool spool) {
   return ocs_spool_select<ocs_spool_UU_table>(spool ? OCS_SPOOL_V_SPOOL : 0);
}

/** @brief Build the field list of a share tree node
 * @param spool   include the accumulated usage
 * @param recurse include the child nodes, which is what makes a whole tree
 *                one object to write
 * @return the field list, to be freed with #spool_free_spooling_fields
 */
spooling_field *sge_build_STN_field_list(const bool spool, const bool recurse) {
   return ocs_spool_select<ocs_spool_STN_table>((spool ? OCS_SPOOL_V_SPOOL : 0) |
                                                (recurse ? OCS_SPOOL_V_RECURSE : 0));
}

/** @brief Build the field list of a share tree node for JSON output
 *
 * No `recurse` flag: in JSON the nesting is expressed by the format itself.
 *
 * Unlike sge_build_STN_field_list (the flat ASCII model, where childnodes is a list of
 * child ids and the writer flattens the tree into separate id-keyed blocks), this list
 * renders childnodes as an array of full child node objects recursively (CS-2313a): the
 * "childnodes" field's sub_fields point back at the same field list, so the generic
 * JSON writer/reader descend the real element tree directly. STN_id and STN_version are
 * omitted (the nested form needs no id bookkeeping; ids are re-assigned via
 * id_sharetree() after a read). A typed table cannot name itself as its sub-table,
 * so this list is still built at run time; its rows carry the typed code of their
 * fields like the rows of a typed table.
 *
 * @return the field list, to be freed with #spool_free_spooling_fields
 */
spooling_field *sge_build_STN_json_field_list() {
   /* name, type, shares, childnodes, terminator = 5 fields */
   auto *fields = reinterpret_cast<spooling_field *>(sge_malloc(sizeof(spooling_field) * 5));
   int count = 0;

   create_spooling_field(&fields[count++], STN_name, 0, "name", nullptr, &ocs_spool_ops<STN_name>);
   create_spooling_field(&fields[count++], STN_type, 0, "type", nullptr, &ocs_spool_ops<STN_type>);
   create_spooling_field(&fields[count++], STN_shares, 0, "shares", nullptr, &ocs_spool_ops<STN_shares>);
   /* childnodes recurses with this very field list -> a nested tree of full child nodes */
   create_spooling_field(&fields[count++], STN_children, 0, "childnodes", fields, &ocs_spool_ops<STN_children>);
   create_spooling_field(&fields[count++], NoName, 0, nullptr, nullptr, nullptr);

   return fields;
}

/* The spool_flatfile_read_object() function will fail to read the
 * EH_reschedule_unknown_list field from a classic spooling file because
 * classic spooling uses two different field delimiters to represent the
 * field values. */
/** @brief Build the field list of an execution host
 * @param spool     include the fields the qmaster owns
 * @param to_stdout use the layout `qhost` prints rather than the spool layout
 * @param history   include the reporting fields
 * @param more_info include the attributes meant for diagnosis. The caller passes the client's
 *                  `MORE_INFO`; asking the environment here would ask qmaster's, because a
 *                  migrated read builds this list inside the daemon (CS-2799). A spool caller
 *                  passes false, so that no environment can add a field to a spool file.
 * @return the field list, to be freed with #spool_free_spooling_fields
 */
spooling_field *sge_build_EH_field_list(const bool spool, const bool to_stdout, const bool history,
                                        const bool more_info) {
   unsigned variant = 0;

   if (spool) {
      variant |= OCS_SPOOL_V_SPOOL;
   }
   if (more_info) {
      variant |= OCS_SPOOL_V_MORE_INFO;
   }
   if (spool || to_stdout || history) {
      variant |= OCS_SPOOL_V_LOADS;
   }
   // the formatted time; the spool file has the raw one, and the two exclude each other
   if (!spool && to_stdout && more_info) {
      variant |= OCS_SPOOL_V_SHOW_MORE;
   }

   return ocs_spool_select<ocs_spool_EH_table>(variant);
}

/**
 * @brief Read the queue sort method of the scheduler configuration
 *
 * The field reader of SC_queue_sort_method. Without DENTER, as the other
 * field readers.
 *
 * @param sc the scheduler configuration
 * @param buffer the text of the value, `load` or `seqno`; anything else leaves the field as it is
 * @param alp receives messages for the caller, unused here
 * @return true
 */
static bool read_SC_queue_sort_method(const ocs::tom::Element<ocs::tom::SC> sc, const char *buffer,
                                      [[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *alp) {
   if (!strncasecmp(buffer, "load", 4)) {
      sc.set<SC_queue_sort_method>(QSM_LOAD);
   } else if (!strncasecmp(buffer, "seqno", 5)) {
      sc.set<SC_queue_sort_method>(QSM_SEQNUM);
   }

   return true;
}

/**
 * @brief Read the load formula of the scheduler configuration
 *
 * The formula is stored without blanks once it is valid.
 *
 * @param sc the scheduler configuration
 * @param load_formula the text of the value, may be nullptr
 * @param alp receives the reason when the formula is not valid
 * @return true on success, false for an invalid formula
 */
static bool read_SC_load_formula(const ocs::tom::Element<ocs::tom::SC> sc, const char *load_formula,
                                 ocs::tom::ListPtr<ocs::tom::AN> *alp) {
   DENTER(TOP_LAYER);
   if (load_formula != nullptr) {
      int n = strlen(load_formula);
      char *copy = sge_malloc(n + 1);
      if (copy != nullptr) {
         strcpy(copy, load_formula);
         sge_strip_blanks(copy);
         // CS-3166: validate the text that is stored
         if (!validate_load_formula(copy, alp, ocs::tom::ConstList<ocs::tom::CE>(), SGE_ATTR_LOAD_FORMULA)) {
            sge_free(&copy);
            DRETURN(false);
         }
         sc.set<SC_load_formula>(copy);
         sge_free(&copy);
      }
   }
   DRETURN(true);
}

/**
 * @brief Write the queue sort method of the scheduler configuration
 *
 * The field writer of SC_queue_sort_method. Without DENTER, as the other
 * field writers.
 *
 * @param sc the scheduler configuration
 * @param buffer receives `seqno` or `load`
 * @param alp receives messages for the caller, unused here
 * @return true
 */
static bool write_SC_queue_sort_method(const ocs::tom::ConstElement<ocs::tom::SC> sc, dstring *buffer,
                                       [[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *alp) {
   if (sc.get<SC_queue_sort_method>() == QSM_SEQNUM) {
      sge_dstring_append(buffer, "seqno");
   } else {
      sge_dstring_append(buffer, "load");
   }

   return true;
}

/**
 * @brief Read the value of a configuration entry, checked per entry name
 *
 * The field reader of CF_value. `gid_range` must be `none` or a range list
 * whose ids are not below #GID_RANGE_NOT_ALLOWED_ID; most other entries take
 * the first white space separated word, some take the rest of the line.
 *
 * @param cf the configuration entry whose CF_name is set already
 * @param buf the text of the value
 * @param alp receives messages for the caller
 * @return true on success, false if the value is rejected
 */
static bool read_CF_value(const ocs::tom::Element<ocs::tom::CF> cf, const char *buf,
                          ocs::tom::ListPtr<ocs::tom::AN> *alp) {
   DENTER(TOP_LAYER);

   const char *name = cf.get<CF_name>();
   char *value = nullptr;
   char *buffer = strdup(buf);
   struct saved_vars_s *context = nullptr;

   if (!strcmp(name, "gid_range")) {
      if ((value = sge_strtok_r(buffer, " \t\n", &context))) {
         if (!strcasecmp(value, NONE_STR)) {
            cf.set<CF_value>(value);
         } else {
            ocs::tom::ListPtr<ocs::tom::RN> rlp;

            range_list_parse_from_string(rlp, alp, value,
                                         false, false, INF_NOT_ALLOWED);
            if (!rlp) {
               WARNING(MSG_CONFIG_CONF_INCORRECTVALUEFORCONFIGATTRIB_SS, name, value);

               sge_free_saved_vars(context);
               sge_free(&buffer);
               DRETURN(false);
            } else {
               for (const ocs::tom::ConstElement<ocs::tom::RN> rep : *rlp) {
                  const uint32_t min = rep.get<RN_min>();
                  if (min < GID_RANGE_NOT_ALLOWED_ID) {
                     WARNING(MSG_CONFIG_CONF_GIDRANGELESSTHANNOTALLOWED_I, GID_RANGE_NOT_ALLOWED_ID);

                     sge_free_saved_vars(context);
                     sge_free(&buffer);
                     DRETURN(false);
                  }
               }
               cf.set<CF_value>(value);
            }
         }
      }
   } else if (!strcmp(name, "admin_user")) {
      value = sge_strtok_r(buffer, " \t\n", &context);
      while (value[0] && isspace((int)value[0]))
         value++;
      if (value) {
         cf.set<CF_value>(value);
      } else {
         WARNING(MSG_CONFIG_CONF_NOVALUEFORCONFIGATTRIB_S, name);

         sge_free_saved_vars(context);
         sge_free(&buffer);
         DRETURN(false);
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
         DRETURN(false);
      }
      /* skip leading delimitors */
      while (value[0] && isspace((int)value[0]))
         value++;

      cf.set<CF_value>(value);
   } else {
      if (!(value = sge_strtok_r(buffer, " \t\n", &context))) {
         WARNING(MSG_CONFIG_CONF_NOVALUEFORCONFIGATTRIB_S, name);

         sge_free_saved_vars(context);
         sge_free(&buffer);
         DRETURN(false);
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
            sge_free_saved_vars(context);
            sge_free(&buffer);
            DRETURN(false);
         }
      }
      cf.set<CF_value>(value);

      if (sge_strtok_r(nullptr, " \t\n", &context)) {
         /* Allow only one value per line */
         WARNING(MSG_CONFIG_CONF_ONLYSINGLEVALUEFORCONFIGATTRIB_S, name);

         sge_free_saved_vars(context);
         sge_free(&buffer);
         DRETURN(false);
      }
   }

   sge_free_saved_vars(context);
   sge_free(&buffer);
   DRETURN(true);
}

/** @brief Build the field list of a global or local configuration
 * @param spool_config write the spool file layout rather than the one
 *                     `qconf -sconf` shows
 * @return the field list, to be freed with #spool_free_spooling_fields
 */
spooling_field *sge_build_CONF_field_list(const bool spool_config) {
   return ocs_spool_select<ocs_spool_CONF_table>(spool_config ? OCS_SPOOL_V_SPOOL : 0);
}

/** @brief Build the field list of a cluster queue
 *
 * A copy of #CQ_fields, optionally without the `hostlist` entry. The two
 * callers want different things from the same table, which is why this
 * exists at all: what an administrator edits under `qconf -mq` still carries
 * the host list, but the queue's spool file must not - the host list of a
 * cluster queue lives in its queue host group `@@<queue>` and is spooled with
 * that group (CS-2677). A second copy in the queue file would be a copy that
 * can disagree.
 *
 * @param with_hostlist keep the `hostlist` entry, the layout `qconf -sq` shows
 * @return the field list, to be freed with #spool_free_spooling_fields
 */
spooling_field *sge_build_CQ_field_list(const bool with_hostlist) {
   return ocs_spool_select<ocs_spool_CQ_table>(with_hostlist ? OCS_SPOOL_V_HOSTLIST : 0);
}

/** @brief Build the field list of a queue instance
 * @param to_stdout use the layout `qconf -sq` shows
 * @param to_file   use the spool file layout; ignored when @p to_stdout is set
 * @return the field list, to be freed with #spool_free_spooling_fields
 */
spooling_field *sge_build_QU_field_list(const bool to_stdout, const bool to_file) {
   unsigned variant = 0;

   if (to_stdout) {
      variant = OCS_SPOOL_V_QU_SHOW;
   } else if (to_file) {
      variant = OCS_SPOOL_V_QU_FILE;
   }

   return ocs_spool_select<ocs_spool_QU_table>(variant);
}

/**
 * @brief Read a per host attribute list of a cluster queue; the field reader of every such field
 *
 * @tparam F the attribute list field of the cluster queue; its sub-type is the attribute type
 * @param cq the cluster queue
 * @param buffer the text to parse
 * @param alp receives the errors
 * @return true if the list was read and set, false otherwise
 */
template <auto F>
static bool read_CQ_attr_list(const ocs::tom::Element<ocs::tom::CQ> cq, const char *buffer,
                              ocs::tom::ListPtr<ocs::tom::AN> *alp) {
   ocs::tom::ListPtr<typename ocs::tom::FieldTraits<F>::Sub> lp;

   if (!ocs_attr_list_parse_from_string(lp, alp, buffer, HOSTATTR_ALLOW_AMBIGUITY)) {
      return false;
   }

   if (lp) {
      cq.set<F>(std::move(lp));
      return true;
   }

   return false;
}

/**
 * @brief Write a per host attribute list of a cluster queue; the field writer of every such field
 *
 * @tparam F the attribute list field of the cluster queue; its sub-type is the attribute type
 * @param cq the cluster queue
 * @param buffer receives the text, appended
 * @param alp unused
 * @return true
 */
template <auto F>
static bool write_CQ_attr_list(const ocs::tom::ConstElement<ocs::tom::CQ> cq, dstring *buffer,
                               [[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *alp) {
   ocs_attr_list_append_to_dstring(cq.get<F>(), buffer);

   return true;
}

/**
 * @brief Read the host list of a cluster queue
 *
 * The field reader of CQ_hostlist. A list that starts with `none` leaves the
 * field as it is. Without DENTER, as the other field readers.
 *
 * @param cq the cluster queue
 * @param buffer the text of the host list
 * @param alp receives messages for the caller, unused here
 * @return true
 */
static bool read_CQ_hostlist(const ocs::tom::Element<ocs::tom::CQ> cq, const char *buffer,
                             [[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *alp) {
   char delims[] = "\t \v\r,";

   auto lp = ocs::tom::ListPtr<ocs::tom::HR>::from_string<HR_name>(buffer, delims);

   if (lp) {
      if (strcasecmp(NONE_STR, lp->front().get<HR_name>()) != 0) {
         cq.set<CQ_hostlist>(std::move(lp));
      }
   }

   return true;
}

/**
 * @brief Write the host list of a cluster queue
 *
 * The field writer of CQ_hostlist. Without DENTER, as the other field writers.
 *
 * @param cq the cluster queue
 * @param buffer receives the host list, or `none`
 * @param alp receives messages for the caller, unused here
 * @return true
 */
static bool write_CQ_hostlist(const ocs::tom::ConstElement<ocs::tom::CQ> cq, dstring *buffer,
                              [[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *alp) {
   const auto lp = cq.get<CQ_hostlist>();

   if (lp) {
      href_list_append_to_dstring(lp, buffer);
   } else {
      sge_dstring_append(buffer, NONE_STR);
   }

   return true;
}

/** @brief Writes the time at which an execution host added itself as a date and time
 *
 * An execution host which an administrator added carries 0, which is written as the empty value
 * of this format, so that it does not read as an instant in time.
 *
 * @param host the execution host
 * @param buffer receives the formatted value
 * @param alp receives messages for the caller, unused here
 * @return true
 */
static bool write_EH_auto_added_time(const ocs::tom::ConstElement<ocs::tom::EH> host, dstring *buffer,
                                     [[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *alp) {
   const uint64_t auto_added_time = host.get<EH_auto_added_time>();

   if (auto_added_time == 0) {
      sge_dstring_append(buffer, NONE_STR);
   } else {
      DSTRING_STATIC(dstr, 64);
      sge_dstring_append(buffer, sge_ctime64_short(auto_added_time, &dstr));
   }

   return true;
}

/**
 * @brief Write the value of a complex entry of a queue level list
 *
 * The string value, or the double value when there is no string.
 *
 * @param centry the complex entry
 * @param buffer receives the value
 * @param alp receives messages for the caller, unused here
 * @return true
 */
static bool write_CE_stringval(const ocs::tom::ConstElement<ocs::tom::CE> centry, dstring *buffer,
                               [[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *alp) {
   const char *s;

   if ((s = centry.get<CE_stringval>()) != nullptr) {
      sge_dstring_append(buffer, s);
   } else {
      sge_dstring_sprintf_append(buffer, "%f", centry.get<CE_doubleval>());
   }

   return true;
}

/**
 * @brief Parse a RQR object from string
 *
 * The field reader of the five RQR_filter_* fields.
 *
 * @tparam F the filter field
 * @param rule the rule the filter is stored in
 * @param buffer String of the elem to be parsed
 * @param alp Answer list
 * @return true on success, false on error
 *
 * @note MT-NOTE: read_RQR_obj() is MT safe
 */
template <auto F>
static bool read_RQR_obj(const ocs::tom::Element<ocs::tom::RQR> rule, const char *buffer,
                         ocs::tom::ListPtr<ocs::tom::AN> *alp) {
   DENTER(TOP_LAYER);

   bool ret = false;

   if (auto filter = rqs_parse_filter_from_string(buffer, alp)) {
      rule.set<F>(std::move(filter));
      ret = true;
   }

   DRETURN(ret);
}

/**
 * @brief Write a filter of a resource quota rule to a string
 *
 * The field writer of the five RQR_filter_* fields.
 *
 * @tparam F the filter field
 * @param rule the rule holding the filter
 * @param buffer receives the filter
 * @param alp Answer List
 * @return true on success, false on error
 *
 * @note MT-NOTE: write_RQR_obj() is MT safe
 */
template <auto F>
static bool write_RQR_obj(const ocs::tom::ConstElement<ocs::tom::RQR> rule, dstring *buffer,
                          ocs::tom::ListPtr<ocs::tom::AN> *alp) {
   DENTER(TOP_LAYER);
   const bool ret = rqs_append_filter_to_dstring(rule.get<F>(), buffer, alp);
   DRETURN(ret);
}
