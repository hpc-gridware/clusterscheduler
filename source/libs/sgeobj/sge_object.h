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
 * @brief Generic access to any CULL object, without knowing its type
 *
 * Code that has to treat every object alike - spooling, the flat file
 * readers/writers, the GDI verification layer - cannot name each attribute.
 * These utilities let it ask an object what type it is, read and write an
 * attribute given only its field number, and convert between an attribute and
 * its string representation.
 *
 * @see sge_object.cc
 */

#include "uti/sge_dstring.h"

#include "cull/cull.h"
#include "cull/cull_list.h"

#include "sgeobj/cull/sge_all_listsL.h"

/**
 * @brief Replace the string "none" in a field with nullptr
 *
 * Users write `none` where the object model wants an unset field. The
 * comparison is case insensitive; a field that is already nullptr is left
 * alone.
 */
#define NULL_OUT_NONE(ep, nm) \
   if (lGetString(ep, nm) != nullptr && strcasecmp(lGetString(ep, nm), "none") == 0) { \
      lSetString(ep, nm, nullptr); \
   }

/**
 * @brief The object and message types the master keeps a list of
 *
 * The value is an index, not a name: `object_base` in `sge_object.cc` and
 * `dev_mirror_base` in `libs/mir/sge_mirror.cc` are both sized
 * `[SGE_TYPE_ALL]` and indexed by this enum. Inserting or removing a value
 * shifts every row below it in both tables, and the rows carry no type name to
 * grep for, so the mistake is silent. Count the rows after editing.
 */
typedef enum {
   SGE_TYPE_FIRST = 0,                    ///< lowest valid value, for iteration
   SGE_TYPE_CALENDAR = SGE_TYPE_FIRST,    ///< calendars
   SGE_TYPE_CKPT,                         ///< checkpointing environments
   SGE_TYPE_CONFIG,                       ///< global and host local configurations
   SGE_TYPE_EXECHOST,                     ///< execution hosts
   SGE_TYPE_JATASK,                       ///< array tasks of a job
   SGE_TYPE_PETASK,                       ///< tasks of a parallel job
   SGE_TYPE_JOB,                          ///< jobs
   SGE_TYPE_JOB_SCHEDD_INFO,              ///< the scheduler's reason messages for pending jobs
   SGE_TYPE_SHARETREE,                    ///< the share tree
   SGE_TYPE_PE,                           ///< parallel environments
   SGE_TYPE_PROJECT,                      ///< projects
   SGE_TYPE_CQUEUE,                       ///< cluster queues
   SGE_TYPE_QINSTANCE,                    ///< queue instances, one per cluster queue and host
   SGE_TYPE_SCHEDD_CONF,                  ///< the scheduler configuration
   SGE_TYPE_SCHEDD_MONITOR,               ///< the scheduler monitoring trigger; carries no object
   SGE_TYPE_SHUTDOWN,                     ///< the shutdown notification; carries no object
   SGE_TYPE_MARK_4_REGISTRATION,          ///< tells an event client to register again; carries no object
   SGE_TYPE_USER,                         ///< users
   SGE_TYPE_USERSET,                      ///< user sets
   SGE_TYPE_HGROUP,                       ///< host groups
   SGE_TYPE_CENTRY,                       ///< complex entries, i.e. the definitions of resources
   SGE_TYPE_SUSER,                        ///< submit users, used to enforce the per user job limit
   SGE_TYPE_RQS,                          ///< resource quota sets
   SGE_TYPE_AR,                           ///< advance reservations
   SGE_TYPE_JOBSCRIPT,                    ///< job scripts
   SGE_TYPE_CATEGORY,                     ///< job categories
   SGE_TYPE_PROCEDURE,                    ///< procedures; carries no object
   SGE_TYPE_RL,                           ///< RBAC roles


   /*
    * Don't forget to edit
    *
    *    'dev_mirror_base' in libs/mir/sge_mirror.cc
    *    'object_base' in libs/sgeobj/sge_object.cc
    *
    *    'sge_mirror_unsubscribe_internal' libs/mir/sge_mirror.cc
    *    'sge_mirror_subscribe_internal' libs/mir/sge_mirror.cc
    * if something is changed here!
    *
    * Both arrays are indexed BY THIS ENUM and are sized [SGE_TYPE_ALL], so a
    * value added or removed here silently shifts every row below it -- the rows
    * carry no type name to grep for. Count them after editing.
    */

   SGE_TYPE_ALL,                          ///< number of real types; must be the second to the last entry
   SGE_TYPE_NONE                          ///< not a type; must be the last entry
} sge_object_type;


/// One row of the type table: what a #sge_object_type value actually refers to
typedef struct {
   const char *type_name;                 ///< type name, e.g. "JOB"
   lDescr *descr;                         ///< descriptor, e.g. JB_Type; nullptr for a type that carries no object
   const int key_nm;                      ///< nm of the key attribute, or `NoName` when the type has no key
} object_description;

const char *
object_type_get_name(sge_object_type type);

sge_object_type 
object_name_get_type(const char *name);

const lDescr *
object_type_get_descr(sge_object_type type);

int
object_type_get_key_nm(sge_object_type type);

/* JG: TODO: rename to object_has_descr, make function object_has_type 
             and call this function where possible */
bool 
object_has_type(const lListElem *object, const lDescr *descr);

/* JG: TODO: rename to object_get_type_descr, check all calls, if possible pass sge_object_type */
const lDescr *
object_get_type(const lListElem *object);

/**
 * @brief The descriptor of the elements a list attribute holds
 *
 * @param nm the list attribute to ask about
 * @return the element descriptor, or nullptr when `nm` is not a list attribute
 *
 * @note Defined in the generated `sgeobj/cull/sge_sub_object.cc`, which is
 *       excluded from doxygen, so this declaration carries the documentation.
 */
const lDescr *
object_get_subtype(int nm);

int 
object_get_primary_key(const lDescr *descr);

const char *
object_get_name(const lDescr *descr);

/* CS-2313a: type name of an object identified by content (covers all registered
 * types and GDI-transported elements; see sge_object.cc). */
const char *
object_get_type_name(const lListElem *object);

const char *
object_get_name_prefix(const lDescr *descr, dstring *buffer);

const char *
object_append_field_to_dstring(const lListElem *object, lList **answer_list, dstring *buffer, int nm, char string_quotes);
bool 
object_parse_field_from_string(lListElem *object, lList **answer_list, int nm, const char *value);

void
object_delete_range_id(lListElem *object, lList **answer_list, int rnm, uint32_t id);

int 
object_set_range_id(lListElem *object, int rnm, uint32_t start, uint32_t end, uint32_t step);

bool
object_parse_bool_from_string(lListElem *this_elem, lList **answer_list, int name, const char *string);

bool
object_parse_ulong32_from_string(lListElem *this_elem, lList **answer_list, int name, const char *string);

bool
object_parse_ulong64_from_string(lListElem *this_elem, lList **answer_list, int name, const char *string);

bool
object_parse_int_from_string(lListElem *this_elem, lList **answer_list, int name, const char *string);

bool
object_parse_long_from_string(lListElem *this_elem, lList **answer_list, int name, const char *string);

bool
object_parse_double_from_string(lListElem *this_elem, lList **answer_list, int name, const char *string);

bool
object_parse_time_from_string(lListElem *this_elem, lList **answer_list, int name, const char *string);

bool
object_parse_mem_from_string(lListElem *this_elem, lList **answer_list, int name, const char *string);

bool
object_parse_inter_from_string(lListElem *this_elem, lList **answer_list, int name, const char *string);

bool
object_parse_list_from_string(lListElem *this_elem, lList **answer_list, int name, const char *string, const lDescr *descr, int nm);

bool
object_parse_celist_from_string(lListElem *this_elem, lList **answer_list, int name, const char *string);

bool
object_parse_solist_from_string(lListElem *this_elem, lList **answer_list, int name, const char *string);

bool
object_parse_qtlist_from_string(lListElem *this_elem, lList **answer_list, int name, const char *string);

bool
object_set_any_type(lListElem *this_elem, int name, void *value);

bool
object_replace_any_type(lListElem *this_elem, int name, lListElem *org_elem);

void
object_get_any_type(const lListElem *this_elem, int name, void *value);

bool  
object_has_differences(const lListElem *this_elem, lList **answer_list, const lListElem *old_elem);

bool
object_list_has_differences(const lList *this_elem, lList **answer_list, const lList *old_elem);

bool
object_unpack_elem_verify(lList **answer_list, sge_pack_buffer *pb, lListElem **epp, const lDescr *descr);

bool
object_list_verify_cull(const lList *lp, const lDescr *descr);

bool
object_verify_cull(const lListElem *ep, const lDescr *descr);

bool
object_verify_ulong_not_null(const lListElem *ep, lList **answer_list, int nm);
bool
object_verify_ulong64_not_null(const lListElem *ep, lList **answer_list, int nm);

bool
object_verify_ulong_null(const lListElem *ep, lList **answer_list, int nm);
bool
object_verify_ulong64_null(const lListElem *ep, lList **answer_list, int nm);

bool
object_verify_double_null(const lListElem *ep, lList **answer_list, int nm);

bool
object_verify_string_not_null(const lListElem *ep, lList **answer_list, int nm);

bool
object_verify_expression_syntax(const lListElem *ep, lList **answer_list);

int
object_verify_name(const lListElem *object, lList **answer_list, int name);

int
object_verify_pe_range(lList **alpp, const char *pe_name, lList *pe_range, const char *object_descr);

int
compress_ressources(lList **alpp, lList *rl, const char *object_descr );
