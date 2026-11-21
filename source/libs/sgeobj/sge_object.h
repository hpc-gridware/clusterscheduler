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
 * These utilities let it ask an object what type it is, compare and verify
 * objects of any type, and convert between an attribute and the text an
 * administrator writes: typed for a field known at compile time
 * (ocs_object_append_field(), ocs_object_parse_field()), per value for the
 * spooling walkers, which know a field by its record (ocs_object_append_symbolic(),
 * ocs_object_parse_symbolic(), ocs_object_parse_uint32() and its siblings).
 *
 * @see sge_object.cc
 */

#include <cstdint>
#include <optional>
#include <string>
#include <strings.h>
#include <type_traits>
#include <vector>

#include "uti/sge_dstring.h"

#include "tom/ocs_Types.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_Raw.h"
#include "sgeobj/ocs_CEntry.h"
#include "sgeobj/sge_answer.h"

namespace ocs {
   /** @brief A key that a unique attribute of a list holds a second time */
   struct RepeatedKey {
      ocs::tom::ConstRawList list;  ///< the list holding the key twice
      int nm;                       ///< the unique attribute
      std::string key;              ///< the key as the repeating element holds it
      std::string path;             ///< the attribute names from the object to the list, separated by '/'
   };
} // namespace ocs

/**
 * @brief Replace the string "none" in a string field with nullptr
 *
 * Users write `none` where the object model wants an unset field. The
 * comparison is case insensitive; a field that is already nullptr is left
 * alone. The field is a template argument and fixes the element's type, so a
 * field that is no string field of the element's type does not compile.
 *
 * @tparam F the string field
 * @param ep the element, of the field's object type; an empty handle is left alone
 */
template <auto F>
inline void ocs_null_out_none(const ocs::tom::Element<typename ocs::tom::FieldTraits<F>::Type> ep) {
   if (const char *value = ocs::tom::get<F>(ep); value != nullptr && strcasecmp(value, "none") == 0) {
      ocs::tom::set<F>(ep, nullptr);
   }
}


/**
 * @brief The object and message types the master keeps a list of
 *
 * The value is an index, not a name: `ocs_object_types` in `ocs_object_type.h`
 * and `dev_mirror_base` in `libs/mir/sge_mirror.cc` are both indexed by this
 * enum. `ocs_object_types` names the value in every entry and fails to compile
 * when it is out of order; the rows of `dev_mirror_base` carry no type name to
 * grep for, so a mistake there is silent. Count its rows after editing.
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
    *    'ocs_object_types' in libs/sgeobj/ocs_object_type.h
    *
    *    'sge_mirror_unsubscribe_internal' libs/mir/sge_mirror.cc
    *    'sge_mirror_subscribe_internal' libs/mir/sge_mirror.cc
    * if something is changed here!
    *
    * Both tables are indexed BY THIS ENUM. ocs_object_types checks its order at
    * compile time; the rows of dev_mirror_base carry no type name to grep for,
    * so a value added or removed here silently shifts every row below it there.
    * Count them after editing.
    */

   SGE_TYPE_ALL,                          ///< number of real types; must be the second to the last entry
   SGE_TYPE_NONE                          ///< not a type; must be the last entry
} sge_object_type;


/// One row of the type table: what a #sge_object_type value actually refers to; generated from `ocs_object_types`
typedef struct {
   const char *type_name;                 ///< type name, e.g. "JOB"
   const lDescr *descr;                         ///< descriptor, e.g. JB; nullptr for a type that carries no object
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
object_has_type(ocs::tom::ConstRawElement object, const lDescr *descr);

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
object_get_type_name(ocs::tom::ConstRawElement object);

const char *
object_get_name_prefix(const lDescr *descr, dstring *buffer);

/**
 * @brief The fields whose value is written as a word rather than in the default form of the field's type
 *
 * Each of them is an unsigned 32 bit number: an enumeration of the complex entry, or a bit field of names.
 * Usually such special cases are evidence of bad data base design: QU_qtype is a bit field of types (BATCH,
 * INTERACTIVE) where boolean fields would do. ocs_object_append_symbolic() and ocs_object_parse_symbolic() render
 * and parse a value of such a kind, the typed ocs_object_append_field() / ocs_object_parse_field() and the
 * spooling record of the field (ocs_spool_field_ops) choose the kind from the field, at compile time.
 */
enum class ocs_object_symbolic : std::uint8_t {
   none,              ///< no symbolic form, the default form of the field's type
   ce_valtype,        ///< `CE_valtype`: the name of an ocs::CEntry::Type, e.g. `MEMORY`
   ce_relop,          ///< `CE_relop`: a relational operator, e.g. `<=`
   ce_requestable,    ///< `CE_requestable`: `NO`, `YES` or `FORCED`
   ce_consumable,     ///< `CE_consumable`: `NO`, `YES`, `JOB` or `HOST`
   qinstance_type,    ///< `QU_qtype`: the names of the queue types, each followed by a blank
   userset_type,      ///< `US_type`: the names of the userset types, separated by blanks
   attr_queue_type    ///< `AQTLIST_value`: the names of the queue types, separated by blanks
};

/**
 * @brief The symbolic kind of a field
 *
 * Evaluated at compile time only: for the typed forms below and for the spooling records.
 *
 * @param nm the field
 * @return the kind, ocs_object_symbolic::none for a field written in the default form of its type
 */
consteval ocs_object_symbolic ocs_object_symbolic_of(const int nm) {
   switch (nm) {
      case CE_valtype:
         return ocs_object_symbolic::ce_valtype;
      case CE_relop:
         return ocs_object_symbolic::ce_relop;
      case CE_requestable:
         return ocs_object_symbolic::ce_requestable;
      case CE_consumable:
         return ocs_object_symbolic::ce_consumable;
      case QU_qtype:
         return ocs_object_symbolic::qinstance_type;
      case US_type:
         return ocs_object_symbolic::userset_type;
      case AQTLIST_value:
         return ocs_object_symbolic::attr_queue_type;
      default:
         return ocs_object_symbolic::none;
   }
}

/** @brief The result of ocs_object_parse_symbolic() */
struct ocs_object_parsed_symbol {
   bool ok;               ///< the text was understood; on false a message may be in the answer list
   bool store;            ///< the value is to be stored into the field - for some kinds after a failure as well
   std::uint32_t value;   ///< the value to store
};

const char *
ocs_object_append_symbolic(dstring *buffer, ocs_object_symbolic kind, std::uint32_t value);

ocs_object_parsed_symbol
ocs_object_parse_symbolic(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs_object_symbolic kind, const char *value);

std::optional<bool>
ocs_object_parse_bool(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *string);

std::optional<std::uint32_t>
ocs_object_parse_uint32(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *string);

std::optional<std::uint64_t>
ocs_object_parse_uint64(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *string);

std::optional<std::int32_t>
ocs_object_parse_int32(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *string);

std::optional<std::int64_t>
ocs_object_parse_int64(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *string);

std::optional<double>
ocs_object_parse_double(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *string);

bool
ocs_object_check_value_string(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::CEntry::Type type,
                              const char *string);

/**
 * @brief The value fields of the cluster queue attribute types (sge_attr.h)
 *
 * ocs_object_append_field() and ocs_object_parse_field() are defined in sge_object.cc and instantiated there for
 * exactly these fields, the only ones their callers name; the constraint turns a use with another field into a
 * compile error instead of an undefined reference at link time.
 *
 * @tparam F the field
 */
template <auto F>
concept OcsAttrValueField =
   static_cast<int>(F) == ASTR_value || static_cast<int>(F) == AULNG_value || static_cast<int>(F) == ABOOL_value ||
   static_cast<int>(F) == ATIME_value || static_cast<int>(F) == AMEM_value || static_cast<int>(F) == AINTER_value ||
   static_cast<int>(F) == AQTLIST_value || static_cast<int>(F) == ASTRLIST_value ||
   static_cast<int>(F) == AUSRLIST_value || static_cast<int>(F) == APRJLIST_value ||
   static_cast<int>(F) == ACELIST_value || static_cast<int>(F) == ASOLIST_value;

template <auto F>
   requires OcsAttrValueField<F>
const char *
ocs_object_append_field(ocs::tom::ConstElement<typename ocs::tom::FieldTraits<F>::Type> ep, dstring *buffer);

template <auto F>
   requires OcsAttrValueField<F>
bool
ocs_object_parse_field(ocs::tom::Element<typename ocs::tom::FieldTraits<F>::Type> ep,
                       ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *value);

template <auto F>
   requires ocs::tom::FieldTraits<F>::has_sub &&
            std::is_same_v<typename ocs::tom::FieldTraits<F>::Sub, ocs::tom::RN>
void
object_delete_range_id(ocs::tom::Element<typename ocs::tom::FieldTraits<F>::Type> object,
                       ocs::tom::ListPtr<ocs::tom::AN> *answer_list, uint32_t id);

template <auto F>
   requires ocs::tom::FieldTraits<F>::has_sub &&
            std::is_same_v<typename ocs::tom::FieldTraits<F>::Sub, ocs::tom::RN>
int
object_set_range_id(ocs::tom::Element<typename ocs::tom::FieldTraits<F>::Type> object, uint32_t start, uint32_t end,
                    uint32_t step);

bool
object_has_differences(ocs::tom::ConstRawElement this_elem, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                       ocs::tom::ConstRawElement old_elem);

bool
object_list_has_differences(ocs::tom::ConstRawList this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                            ocs::tom::ConstRawList old_list);

bool
object_unpack_elem_verify(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, sge_pack_buffer *pb, lListElem **epp,
                          const lDescr *descr);

bool
object_list_verify_cull(ocs::tom::ConstRawList lp, const lDescr *descr);

bool
object_verify_cull(ocs::tom::ConstRawElement ep, const lDescr *descr);

bool
ocs_object_list_verify_type(ocs::tom::ConstRawList lp, const lDescr *descr);

bool
ocs_object_verify_type(ocs::tom::ConstRawElement ep, const lDescr *descr);

bool
ocs_object_verify_unique_keys(ocs::tom::ConstRawElement ep, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                              const char *object_name);

bool
ocs_object_list_verify_unique_keys(ocs::tom::ConstRawList lp, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                   const char *object_name);

void
ocs_object_list_remove_repeated_keys(ocs::tom::RawList lp, int nm);

void
ocs_object_list_add_unique(lList **lpp, lList **src);

void
ocs_object_find_repeated_keys(ocs::tom::ConstRawElement ep, std::vector<ocs::RepeatedKey> &found);

bool
ocs_object_repair_repeated_keys(ocs::tom::RawElement ep, std::string &repairs,
                                ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *object_name);

// The object_verify_*() templates are defined in sge_object.cc and instantiated there for the fields in use,
// because their messages come from the module's message catalogue.
template <auto F>
bool
object_verify_ulong_not_null(ocs::tom::ConstElement<typename ocs::tom::FieldTraits<F>::Type> ep,
                             ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
template <auto F>
bool
object_verify_ulong_null(ocs::tom::ConstElement<typename ocs::tom::FieldTraits<F>::Type> ep,
                         ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
template <auto F>
bool
object_verify_ulong64_null(ocs::tom::ConstElement<typename ocs::tom::FieldTraits<F>::Type> ep,
                           ocs::tom::ListPtr<ocs::tom::AN> *answer_list);

template <auto F>
bool
object_verify_double_null(ocs::tom::ConstElement<typename ocs::tom::FieldTraits<F>::Type> ep,
                          ocs::tom::ListPtr<ocs::tom::AN> *answer_list);

template <auto F>
bool
object_verify_string_not_null(ocs::tom::ConstElement<typename ocs::tom::FieldTraits<F>::Type> ep,
                              ocs::tom::ListPtr<ocs::tom::AN> *answer_list);

bool
object_verify_expression_syntax(ocs::tom::ConstElement<ocs::tom::CE> elem,
                                ocs::tom::ListPtr<ocs::tom::AN> *answer_list);

int
ocs_object_verify_name_value(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *object_name,
                             const char *attribute_name);

/**
 * @brief Verify that a name attribute is usable as an object name
 *
 * Two checks are applied to the attribute:
 *
 * 1. the name must not start with a digit, and
 * 2. it must pass the general key check for the qsub character table.
 *
 * A detailed problem description is added to the answer list. An attribute
 * that is not set at all passes. The checks are ocs_object_verify_name_value()'s.
 *
 * @tparam F the string field holding the object name
 * @param object the element carrying the name
 * @param[out] answer_list receives the problem description
 *
 * @return 0 when the name is usable, otherwise `STATUS_EUNKNOWN`
 *
 * @note MT-NOTE: object_verify_name() is MT safe
 */
template <auto F>
   requires (ocs::tom::FieldTraits<F>::type == ocs::tom::FieldType::String)
int
object_verify_name(const ocs::tom::ConstElement<typename ocs::tom::FieldTraits<F>::Type> object,
                   ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   return ocs_object_verify_name_value(answer_list, ocs::tom::get<F>(object), ocs::tom::FieldTraits<F>::info.name);
}

int
object_verify_pe_range(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const char *pe_name,
                       ocs::tom::List<ocs::tom::RN> pe_range, const char *object_descr);

int
compress_ressources(ocs::tom::ListPtr<ocs::tom::AN> *alpp, ocs::tom::List<ocs::tom::CE> rl, const char *object_descr);
