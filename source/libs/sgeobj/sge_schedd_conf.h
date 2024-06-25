#pragma once
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

#include "cull/cull.h"

#include "sgeobj/cull/sge_schedd_conf_PARA_L.h"
#include "sgeobj/cull/sge_schedd_conf_SC_L.h"

/* 
 * valid values for SC_queue_sort_method 
 */
enum {
   QSM_LOAD = 0,
   QSM_SEQNUM = 1
};

/* defines the last dispatched job */
enum {
    DISPATCH_TYPE_NONE = 0,      /* did not dispatch a job */
    DISPATCH_TYPE_FAST,          /* dispatched a sequential job */
    DISPATCH_TYPE_FAST_SOFT_REQ, /* dispatch a sequential job with soft requests */
    DISPATCH_TYPE_PE,            /* dispatched a pe job*/
    DISPATCH_TYPE_PE_SOFT_REQ    /* dispatched a pe job*/
};

enum schedd_job_info_key {
   SCHEDD_JOB_INFO_FALSE=0,
   SCHEDD_JOB_INFO_TRUE,
   SCHEDD_JOB_INFO_JOB_LIST,
   SCHEDD_JOB_INFO_UNDEF
};

/* defines the algorithm that should be used to compute pe-ranges */
typedef enum {
   SCHEDD_PE_AUTO=-1,      /* automatic, the scheduler will decide */
   SCHEDD_PE_LOW_FIRST=0,  /* least slot first */
   SCHEDD_PE_HIGH_FIRST,   /* highest slot first */
   SCHEDD_PE_BINARY,       /* binary search */
   SCHEDD_PE_ALG_MAX       /* number of algorithms */    /* number of algorithms */
} schedd_pe_algorithm;

typedef enum {
   FIRST_POLICY_VALUE,
   INVALID_POLICY = FIRST_POLICY_VALUE,

   OVERRIDE_POLICY,
   FUNCTIONAL_POLICY,
   SHARE_TREE_POLICY,

   /* TODO: shouldn't LAST_POLICY_VALUE equal SHARE_TREE_POLICY? 
    * POLICY_VALUES = 4, should probably be 3
    */
   LAST_POLICY_VALUE,
   POLICY_VALUES = (LAST_POLICY_VALUE - FIRST_POLICY_VALUE)
} policy_type_t;

typedef struct {
   policy_type_t policy;
   int dependent;
} policy_hierarchy_t;

void sconf_ph_fill_array(policy_hierarchy_t array[]);

void sconf_ph_print_array(policy_hierarchy_t array[]);

void sconf_print_config();

lListElem *sconf_create_default();

bool sconf_set_config(lList **config, lList **answer_list);

bool sconf_is_valid_load_formula(lList **answer_list, const lList *cmplx_list);

bool
sconf_is_centry_referenced(const lListElem *centry);

bool sconf_validate_config(lList **answer_list, lList *config);

bool sconf_validate_config_(lList **answer_list);

lListElem *sconf_get_config();

lList *sconf_get_config_list();

bool sconf_is_new_config();
void sconf_reset_new_config();

bool sconf_is();

u_long32 sconf_get_load_adjustment_decay_time();

lList *sconf_get_job_load_adjustments();

char *sconf_get_load_formula();

u_long32 sconf_get_queue_sort_method();

u_long32 sconf_get_maxujobs();

u_long32 sconf_get_schedule_interval();

u_long32 sconf_get_reprioritize_interval();

u_long32 sconf_get_weight_tickets_share();

lList *sconf_get_schedd_job_info_range();

bool sconf_is_id_in_schedd_job_info_range(u_long32 job_number);

lList *sconf_get_usage_weight_list();

double sconf_get_weight_user();

double sconf_get_weight_department();

double sconf_get_weight_project();

double sconf_get_weight_job();

u_long32 sconf_get_weight_tickets_share();

u_long32 sconf_get_weight_tickets_functional();

u_long32 sconf_get_halftime();

void sconf_set_weight_tickets_override(u_long32 active);

u_long32 sconf_get_weight_tickets_override();

double sconf_get_compensation_factor();

bool sconf_get_share_override_tickets();

bool sconf_get_share_functional_shares();

bool sconf_get_report_pjob_tickets();

bool sconf_is_job_category_filtering();

u_long32 sconf_get_flush_submit_sec();

u_long32 sconf_get_flush_finish_sec();

u_long32 sconf_get_max_functional_jobs_to_schedule();

u_long32 sconf_get_max_pending_tasks_per_job();

lList* sconf_get_halflife_decay_list();

double sconf_get_weight_ticket();
double sconf_get_weight_waiting_time();
double sconf_get_weight_deadline();
double sconf_get_weight_urgency();

u_long32 sconf_get_max_reservations();

double sconf_get_weight_priority();
bool sconf_get_profiling();

u_long32 sconf_get_default_duration();

typedef enum {
   QS_STATE_EMPTY,
   QS_STATE_FULL
} qs_state_t;

u_long32 sconf_get_schedd_job_info();
void sconf_disable_schedd_job_info();
void sconf_enable_schedd_job_info();

void sconf_set_qs_state(qs_state_t state);
qs_state_t sconf_get_qs_state();

void sconf_set_global_load_correction(bool flag);
bool sconf_get_global_load_correction();

bool sconf_get_host_order_changed();
void sconf_set_host_order_changed(bool changed);

int  sconf_get_last_dispatch_type();
void sconf_set_last_dispatch_type(int changed);

u_long32  sconf_get_duration_offset();

bool serf_get_active();

schedd_pe_algorithm sconf_best_pe_alg();
void sconf_update_pe_alg(int runs, int current, int max);
int  sconf_get_pe_alg_value(schedd_pe_algorithm alg);

void sconf_inc_fast_jobs();
int sconf_get_fast_jobs();

void sconf_inc_pe_jobs();
int sconf_get_pe_jobs();

void sconf_set_decay_constant(double decay);
double sconf_get_decay_constant();

void sconf_set_mes_schedd_info(bool newval);
bool sconf_get_mes_schedd_info();

void schedd_mes_set_logging(int bval);
int schedd_mes_get_logging();

lListElem *sconf_get_sme();
void sconf_set_sme(lListElem *sme);

lListElem *sconf_get_tmp_sme();
void sconf_set_tmp_sme(lListElem *sme);

void sconf_reset_jobs();

void sconf_get_weight_ticket_urgency_priority(double *ticket, double *urgency, double *priority);
