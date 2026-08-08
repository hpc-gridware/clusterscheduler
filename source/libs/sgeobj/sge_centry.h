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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Declarations and constants of the complex entry object
 *
 * @see sge_centry.cc
 */

#include "sgeobj/cull/sge_centry_CE_L.h"
#include "sgeobj/cull/sge_ct_SCT_L.h"
#include "sgeobj/cull/sge_ct_REF_L.h"
#include "sgeobj/cull/sge_ct_CT_L.h"
#include "sgeobj/cull/sge_ct_CCT_L.h"
#include "sgeobj/cull/sge_ct_CTI_L.h"

#include "sgeobj/ocs_CEntry.h"

/* 
 * This is the list type we use to hold the complex list in qmaster.
 *
 * We also use it for the queue information which administrator defined 
 * complexes aply to this queue. In this case CX_entries is unused. 
 * At the moment this applies only for the gdi. Internal the old list is
 * used.  
 */

/**
 * @brief How a request is compared against what a queue or host offers
 *
 * Stored in `CE_relop`. The relation is part of the complex entry's
 * definition, not of the request: the administrator decides once per resource
 * whether more is better, less is better, or only equality counts.
 */
enum {
   CMPLXEQ_OP = 1, ///< the offered value must equal the request
   CMPLXGE_OP,     ///< the offered value must be at least the request
   CMPLXGT_OP,     ///< the offered value must exceed the request
   CMPLXLT_OP,     ///< the offered value must be below the request
   CMPLXLE_OP,     ///< the offered value must be at most the request
   CMPLXNE_OP,     ///< the offered value must differ from the request
   CMPLXEXCL_OP    ///< the resource is granted exclusively to one job
};

/// Whether a job may, must or must not request a resource; stored in `CE_requestable`
enum {
   REQU_NO = 1, ///< the resource cannot be requested by a job
   REQU_YES,    ///< a job may request it
   REQU_FORCED  ///< a job must request it to be scheduled at all
};

/**
 * @brief What a job consumes of a resource, stored in `CE_consumable`
 *
 * A consumable is booked when a job starts and released when it ends. The
 * three yes-variants differ in how much a parallel job consumes: per slot, per
 * job, or once per host the job runs on.
 */
enum {
   CONSUMABLE_NO = 0,  ///< not consumable; the value is only compared, never booked
   CONSUMABLE_YES,     ///< consumed once per slot
   CONSUMABLE_JOB,     ///< consumed once per job, regardless of its slot count
   CONSUMABLE_HOST     ///< consumed once per host the job occupies
};

/**
 * @brief Where a resource value came from, stored as a bit mask in `CE_dominant`
 *
 * The same resource can be defined at several layers and in several ways; the
 * value a job actually sees is the most restrictive one. `CE_dominant` records
 * which layer and which kind that was, so `qstat -F` can show it.
 *
 * The low byte is the layer, the high byte the kind.
 */
enum {
   DOMINANT_LAYER_GLOBAL = 0x0001,      ///< the global host
   DOMINANT_LAYER_HOST = 0x0002,        ///< an execution host
   DOMINANT_LAYER_QUEUE = 0x0004,       ///< a queue instance
   DOMINANT_LAYER_RQS = 0x0008,         ///< a resource quota set
   DOMINANT_LAYER_MASK = 0x00ff,        ///< all layers

   DOMINANT_TYPE_VALUE = 0x0100,        ///< value from complex template
   DOMINANT_TYPE_FIXED = 0x0200,        ///< fixed value from object configuration
   DOMINANT_TYPE_LOAD = 0x0400,         ///< load value
   DOMINANT_TYPE_CLOAD = 0x0800,        ///< corrected load value
   DOMINANT_TYPE_CONSUMABLE = 0x1000,   ///< consumable
   DOMINANT_TYPE_MASK = 0xff00          ///< all types
};

/// At which level a resource request was satisfied
enum{
   NO_TAG = 0, ///< not satisfied anywhere
   QUEUE_TAG,  ///< satisfied by a queue instance
   HOST_TAG,   ///< satisfied by an execution host
   GLOBAL_TAG, ///< satisfied by the global host
   PE_TAG,     ///< not really used as a tag
   RQS_TAG,    ///< not really used as a tag
   MAX_TAG     ///< not a tag; the number of values above
};

/// The letter a tag level is printed as, indexed by the tag
#define CENTRY_LEVEL_TO_CHAR(level) "NQHGPLM"[level]

/// Maps one queue or host attribute onto the complex entry it is reported as
struct queue2cmplx {
   const char *name;    ///< name of the centry element, not the shortcut
   int  field;    ///< name of the element in the queue structure
   int  cqfld;    ///< cluster queue field
   int  valfld;   ///< value field in cluster queue sublist
   ocs::CEntry::Type  type;     ///< type of the element in the queue structure
};
extern const int max_host_resources;
extern const struct queue2cmplx host_resource[]; 
extern const int max_queue_resources;
extern const struct queue2cmplx queue_resource[];

int 
get_rsrc(const char *name, bool queue, int *field, int *cqfld, int *valfld, ocs::CEntry::Type *type);

int
centry_fill_and_check(lListElem *this_elem, lList** answer_list, bool allow_empty_boolean,
                      bool allow_neg_consumable);

const char *
map_op2str(uint32_t op);

const char *
map_type2str(ocs::CEntry::Type type);

const char *
map_req2str(uint32_t op);

const char *
map_consumable2str(uint32_t op);

lListElem *
centry_create(lList **answer_list, const char *name);

bool
centry_is_referenced(const lListElem *centry, lList **answer_list,
                     const lList *master_cqueue_list,
                     const lList *master_exechost_list,
                     const lList *master_lirs_list);

bool
centry_print_resource_to_dstring(const lListElem *this_elem, 
                                 dstring *string);

/**
 * @brief The complex configuration of the active data store
 *
 * @return a pointer to the master list of complex entries
 */
lList **
centry_list_get_master_list();

lListElem *
centry_list_locate(const lList *this_list, 
                   const char *name);

bool
centry_elem_validate(lListElem *centry, const lList *centry_list, lList **answer_list);


bool
centry_list_sort(lList *this_list);

void
centry_list_init_double(const lList *this_list);

int
centry_list_fill_request(const lList *centry_list, lList **answer_list, const lList *master_centry_list,
                         bool allow_non_requestable, bool allow_empty_boolean,
                         bool allow_neg_consumable);

void
centry_list_fill_config(lList *centry_list, const lList *master_centry_list);

bool
centry_list_are_queues_requestable(const lList *this_list);

const char *
centry_list_append_to_dstring(const lList *this_list, dstring *string); 

int
centry_list_append_to_string(lList *this_list, char *buff, uint32_t max_len);

lList *
centry_list_parse_from_string(lList *complex_attributes,
                              const char *str, bool check_value);

void
centry_list_remove_duplicates(lList *this_list);

double 
centry_urgency_contribution(int slots, const char *name, double value, 
                            const lListElem *centry);

bool
centry_list_do_all_exists(const lList *this_list, lList **answer_list,
                          const lList *centry_list);

bool
centry_list_is_correct(lList *this_list, lList **answer_list);

int ensure_attrib_available(lList **alpp, lListElem *ep, int nm, const lList *master_centry_list);

int host_ensure_slots_are_defined(lListElem *ehost, uint32_t processors);

bool
validate_load_formula(const char *formula, lList **answer_list, const lList *centry_list, const char *name);

bool load_formula_is_centry_referenced(const char *load_formula, const lListElem *centry);

const char* sge_get_dominant_stringval(const lListElem *rep, uint32_t *dominant_p, dstring *resource_string_p, double *dbl_value, uint64_t *uint64_value);

int slot_signum(int slots);
bool consumable_do_booking(uint32_t consumable, bool is_master_task, bool do_per_host_booking);
int consumable_get_debit_slots(uint32_t consumable, int slots);
