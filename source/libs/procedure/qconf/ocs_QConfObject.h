#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

/** @file
 * @brief The `qconf` object descriptor table
 */

/** @defgroup qconf_object The qconf object types
 * @brief One row per configuration object, read by the generic engines
 *
 * The table used to live in `clients/qconf/ocs_qconf_parse.cc`. It moved here
 * with CS-2799, because the read path renders inside qmaster from now on and
 * the table has to be reachable from both the client and the daemon. Nothing
 * about it changed in the move.
 *
 * @{
 */

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <tuple>
#include <type_traits>
#include <utility>

#include "cull/cull.h"

#include "gdi/ocs_gdi_Target.h"
#include "gdi/ocs_gdi_Command.h"

#include "tom/ocs_Element.h"
#include "tom/ocs_FieldTraits.h"
#include "tom/ocs_List.h"

#include "sgeobj/sge_object.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/tom/ocs_tom_AN.h"
#include "sgeobj/tom/ocs_tom_CAL.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CK.h"
#include "sgeobj/tom/ocs_tom_CONF.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_CT.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_EV.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_HR.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_RL.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "sgeobj/tom/ocs_tom_SC.h"
#include "sgeobj/tom/ocs_tom_STN.h"
#include "sgeobj/tom/ocs_tom_UE.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_UU.h"

#include "spool/sge_spooling_utilities.h"
#include "spool/flatfile/sge_flatfile.h"
#include "spool/flatfile/sge_flatfile_obj.h"
#include "sgeobj/sge_answer.h"

#include "uti/sge_rmon_macros.h"

/** @brief Builds the field list one call renders or parses with
 *
 * @param format    the format the text is written in
 * @param more_info the client's `MORE_INFO`, see QConfParameter::get_more_info(). It is an
 *                  argument and not an environment lookup because the list is built where the
 *                  object is rendered, which for a migrated read is inside qmaster.
 * @param owned     set to true when the caller has to free the returned list
 * @return the field list
 */
typedef spooling_field *(*qconf_build_fields_fn)(spool_flatfile_format format, bool more_info,
                                                 bool *owned);

/** @brief The configuration object types the generic engines handle.
 *
 * CONF and EV carry read-only rows (CS-2808). The cluster configuration is keyed
 * on the host basename rather than on a field, so its write path keeps its own
 * qconf_conf_export()/qconf_conf_apply_path() and the write hooks of its row
 * stay empty; only -sconfl uses it. EV is not a configuration object at all -
 * event clients live in the event master, not in a master list - and only
 * -sss uses it.
 *
 * AH, SH, MGR and OP are not object types either (CS-2820): each names one
 * *reserved* object whose member sublist is what the switch reports - the host
 * groups @admin_hosts and @submit_hosts for -sh and -ss, the access lists
 * manager and operator for -sm and -so. They share the descriptor of their
 * owner type and differ only in which object to read and which sublist to
 * render, so they are rows here rather than columns in the read table; that
 * keeps the views taking nothing but a qconf_object_desc.
 *
 * Values are **appended, never inserted**: the value is the index of its entry in
 * #ocs_qconf_objects. The order is checked at compile time
 * (ocs_qconf_object_entry_at()), so an insertion fails the build instead of
 * shifting every row below it in silence. */
enum class QConfObj {
   CAL, CK, CE, EH, HGRP, PE, PR, CQ, RL, RQS, US, UU, STN, SC, CONF, EV, CAT,
   AH, SH, MGR, OP
};

/** @brief Where a member name list comes from (CS-2820)
 *
 * The four reserved lists are read as "one named object, then its sublist",
 * which no other read kind expresses: NAME_LIST reads every object of a type,
 * SINGLE_OBJECT renders one in full. What the output looks like is a name list
 * all the same, so QConfReadKind::MEMBER_LIST reuses the whole rendering and
 * empty-list machinery and only the source differs - this struct says how. */
struct qconf_member_desc {
   const char *owner;        ///< name of the reserved object, e.g. "@admin_hosts"; its sublist field and the
                             ///< name field of a sublist element are the owner type's, see ocs_qconf_member_of
   bool split_matchers;      ///< plain output only: host names and @group references
                             ///< first, matchers after them, each block still sorted.
                             ///< A property of the reserved host groups (CS-2680) and
                             ///< deliberately not of the JSON form, whose array order
                             ///< must stay the plain sort order
};

/** @brief What the read path needs on top of the common descriptor (CS-2796).
 *
 * There is one deliberate difference between showing an object and writing it
 * to a file, and it is in @c fields: -se builds its field list with
 * to_stdout=true, so it shows load_values and processors, which -Se leaves out
 * on purpose - the export has to round-trip. Pinned by exechost.show.plain vs
 * exechost.export.plain (CS-2794). nullptr means "the same as the write path".
 *
 * The layout itself is now the same for both: -suser used to render with
 * qconf_param_sfi where -Suser uses qconf_sfi, which never reached the output
 * (every UU field has width 0, so align_names does nothing, and the sublist
 * fields only exist with spool=true, which qconf does not use). -Suser is new
 * in 9.2, so there was no compatibility reason to keep the two apart. */
struct qconf_read_desc {
   qconf_build_fields_fn fields;   ///< field list for -s<obj>, nullptr to use build_fields
   const char *list_name;          ///< object name in the "no X defined" message of -s<obj>l
   const char *not_found_msg;      ///< format string of the "X is not a Y" message of -s<obj>
   const char *empty_msg;          ///< message of a singleton read that finds nothing,
                                   ///< takes no argument; nullptr for a type that is not one
   const spool_flatfile_instr *list_sfi;  ///< the aligned table form of -s<obj>l, used by
                                         ///< the table sub-procedures; nullptr for a type
                                         ///< that has no such form (CS-2819)
   const char *json_list_id;       ///< the table form's JSON schema id when it differs from
                                   ///< the single record's - the category list is written
                                   ///< as "category-list" holding "category" objects, so
                                   ///< the two do not collide; nullptr uses the plain
                                   ///< flatfile writer, as the complex entries do
   const char *not_found_noun;     ///< OBJECT_LIST: what a name that matched nothing is
                                   ///< called in "denied: <noun> \"x\" does not exist";
                                   ///< nullptr reports the whole read as empty instead
   bool name_header;               ///< SINGLE_OBJECT: write a "#<name>:" comment line before
                                   ///< the object in a text format. Only the configuration
                                   ///< does this, and only because -sconf has always done
                                   ///< it: the header is what tells the blocks of
                                   ///< "-sconf host1,host2" apart, and omitting it in JSON
                                   ///< is what CS-2313a decided (a comment is not JSON)
   bool write_as_list;             ///< OBJECT_LIST: render the whole list through the
                                   ///< spooling instruction in one go, rather than one
                                   ///< object at a time separated by blank lines. It is a
                                   ///< property of the type, not of the switch: -srqs
                                   ///< wraps every record in braces (qconf_rqs_sfi),
                                   ///< -su prints plain blocks (CS-2819)
   const char *json_name_id;       ///< name list JSON: the envelope stem, for a list whose
                                   ///< elements are not a registered object type and would
                                   ///< otherwise be reported as "unknown"; nullptr derives
                                   ///< it from the first element, as the object name lists
                                   ///< do (CS-2820)
   bool json_name_id_when_empty;   ///< name list JSON: whether json_name_id is still given
                                   ///< for an empty list. It decides between a bare
                                   ///< {"names": []} and the full "<stem>-list" envelope,
                                   ///< and the two reserved pairs really do differ: -sh and
                                   ///< -ss passed the type name only for a non-empty list,
                                   ///< -sm and -so passed it always (CS-2820)
   qconf_member_desc member;       ///< MEMBER_LIST: which object and which sublist the
                                   ///< names come from; unused by every other kind
};

/** @brief What the write path needs on top of the common descriptor (CS-2823)
 *
 * The typed hooks of the write path - the whole-list checks and the retain
 * predicate of a replace - are in #ocs_qconf_hooks; what is left here is the
 * message that goes with the retain predicate. A type whose text is one object
 * leaves the whole struct empty.
 */
struct qconf_write_desc {
   const char *retain_msg;   ///< the one line that names what was retained, taking
                             ///< the comma separated names; required with
                             ///< ocs_qconf_hooks::retain_on_replace, because silently
                             ///< keeping an object would hide an edit that was not
                             ///< carried out
};

/** @brief Everything the generic engines need to know about one object type.
 *
 * The untyped part of an entry of #ocs_qconf_objects: what the descriptor-generic
 * views and the spooling functions take. The descriptor and the name field are
 * not written in the rows; they are filled in from the entry's object type and
 * key field (see OcsQConfObject), so there is one source for both.
 */
struct qconf_object_desc {
   ocs::gdi::Target target;                          ///< GDI target list
   qconf_build_fields_fn build_fields;               ///< field list provider (may allocate)
   const spool_flatfile_instr *sfi;                  ///< flatfile spooling instruction
   const char *json_type_name;                       ///< explicit JSON $id, nullptr derives it
   ocs::gdi::SubCommand sub_cmd;                     ///< GDI sub-command of an upsert
   bool as_list;                                     ///< file holds a list, not one object
   bool parse_values;                                ///< parse attribute values when reading a file
   bool is_root;                                     ///< singleton export: object is the root
   bool (*export_exclude)(const char *name);         ///< bulk export: leave this object out
   qconf_read_desc read;                             ///< the read path additions (CS-2796)
   qconf_write_desc write;                           ///< the write path additions (CS-2823)
   const lDescr *descr = nullptr;                    ///< CULL descriptor, from the entry's object type
   int name_nm = NoName;                             ///< name field, NoName for the singletons; from the
                                                     ///< entry's key field
};

/** @brief A check or a transform of one parsed object of type @p T
 *
 * The signature of ocs_qconf_hooks::validate and ocs_qconf_hooks::prepare.
 *
 * @tparam T the object type
 */
template <ocs::tom::ObjectType T>
using ocs_qconf_check_fn = int (*)(ocs::tom::Element<T>, ocs::tom::ListPtr<ocs::tom::AN> *);

/** @brief The object type specific hooks of one qconf object type, typed on the type
 *
 * The hooks used to be raw function pointers in qconf_object_desc taking
 * `lListElem *` and `lList *`; every implementation then took the element into
 * a handle of its type. Typed here, the compiler checks that a row pairs each
 * hook with its own type, and the callers reach them through the entry of
 * #ocs_qconf_objects, see ocs_with_qconf_object().
 *
 * @tparam T the object type
 */
template <ocs::tom::ObjectType T>
struct ocs_qconf_hooks {
   /// semantic check of a parsed object, nullptr for none
   ocs_qconf_check_fn<T> validate = nullptr;
   /// pre-send fixup of a parsed object (CS-2304: resolve a host name), nullptr for none
   ocs_qconf_check_fn<T> prepare = nullptr;
   /** SINGLE_OBJECT: build the type's template, for a read that feeds an editor
    * and found no object of that name (CS-2823). It is what makes
    * "-m<obj> <unknown>" offer a template and add on exit, and "-a<obj>" the
    * same operation as "-m<obj>" - see QConfModelBase::make_snapshot(). Built
    * here rather than in the client so that the text an editor sees comes out of
    * the same renderer and the same field list as a stored object's: two
    * renderers for one text format is how the exec host's read/write mismatch
    * arose, see QConfParameter::get_for_edit(). nullptr for a type that is not
    * created from a name - the exec host, whose template is an entry of its own
    * list, and every type whose write is not on the procedure yet. An empty
    * handle means the constructor refused the name, with the reason in the
    * answer list. */
   ocs::tom::ElementPtr<T> (*create_template)(ocs::tom::ListPtr<ocs::tom::AN> *, const char *) = nullptr;
   /// fixup before rendering, with the master complex list, nullptr for none
   void (*show_prepare)(ocs::tom::Element<T>, ocs::tom::ConstList<ocs::tom::CE>) = nullptr;
   /** MODE_LIST: the checks that need the whole list in hand, run in the
    * reading phase so that a text failing them is rejected as a whole.
    * Per-element checks belong in #validate, or in the type's own modifier where
    * they already are - this is only for what one element cannot answer: the
    * duplicate name of a rule set text, the duplicate name or shortcut of a
    * complex list. nullptr for a type with nothing to check */
   int (*verify_list)(ocs::tom::ConstList<T>, ocs::tom::ListPtr<ocs::tom::AN> *) = nullptr;
   /** MODE_REPLACE: true for an object the replace must *not* delete although
    * the text left it out. The complex list is the only user: a built-in cannot
    * be deleted at all, so asking would abort the load of an old backup that has
    * no row for it (CS-2523). nullptr deletes everything the text omits; with a
    * predicate, qconf_write_desc::retain_msg is required */
   bool (*retain_on_replace)(ocs::tom::ConstElement<T>) = nullptr;
};

/** @brief The member sublist of a reserved object, for the owner types that have reserved objects (CS-2820)
 *
 * The four reserved member lists (#QConfObj AH, SH, MGR, OP) are read as "one
 * named object, then its sublist". Which sublist and which name field is a
 * property of the owner type, so it is said once here, as template arguments,
 * and the read works on handles of the member type.
 *
 * @tparam T the owner's object type
 */
template <ocs::tom::ObjectType T>
struct ocs_qconf_member_of {
   /// whether the type owns reserved member lists
   static constexpr bool has_member = false;
};

/** @brief The host groups @admin_hosts and @submit_hosts: their host list, named by HR_name */
template <>
struct ocs_qconf_member_of<ocs::tom::HGRP> {
   /// whether the type owns reserved member lists
   static constexpr bool has_member = true;
   /// the sublist field
   static constexpr auto list = HGRP_host_list;
   /// the name field of a sublist element
   static constexpr auto key = HR_name;
   /// the object type of a sublist element
   using type = ocs::tom::HR;
};

/** @brief The access lists manager and operator: their entries, named by UE_name */
template <>
struct ocs_qconf_member_of<ocs::tom::US> {
   /// whether the type owns reserved member lists
   static constexpr bool has_member = true;
   /// the sublist field
   static constexpr auto list = US_entries;
   /// the name field of a sublist element
   static constexpr auto key = UE_name;
   /// the object type of a sublist element
   using type = ocs::tom::UE;
};

/** @brief Whether a key field is a name - a string or a host field
 *
 * The category is the one type whose key is a number; the singletons have no
 * key. Neither is looked up by name.
 *
 * @tparam K the key field, `NoName` for none
 * @return true for a string or host field
 */
template <auto K>
consteval bool ocs_qconf_is_name_key() {
   if constexpr (ocs::tom::TypedField<decltype(K)>) {
      constexpr ocs::tom::FieldType type = ocs::tom::FieldTraits<K>::type;
      return type == ocs::tom::FieldType::String || type == ocs::tom::FieldType::Host;
   } else {
      return false;
   }
}

/** @brief One qconf object type: its value, its object type, its key field, its row and its typed hooks
 *
 * The object type and the key field are template arguments, so code holding an
 * entry knows both at compile time and calls the hooks with handles of the
 * type. The row (#qconf_object_desc) is what the descriptor-generic views and
 * the spooling API take; its descriptor and name field are filled in from the
 * template arguments here.
 *
 * The entries cannot be `constexpr`: the messages in the rows are gettext calls
 * at run time on every platform that translates.
 *
 * @tparam O the #QConfObj value
 * @tparam T the struct of the object type, e.g. ocs::tom::CAL
 * @tparam K the key field, e.g. CAL_name; `NoName` for a singleton
 */
template <QConfObj O, ocs::tom::ObjectType T, auto K = NoName>
struct OcsQConfObject {
   /// the #QConfObj value
   static constexpr QConfObj value = O;
   /// the struct of the object type
   using type = T;
   /// whether the object type has a key field
   static constexpr bool has_key = ocs::tom::TypedField<decltype(K)>;
   /// whether the key field is a name, a string or a host field
   static constexpr bool has_name_key = ocs_qconf_is_name_key<K>();
   /// the key field, `NoName` if there is none
   static constexpr auto key = K;

   qconf_object_desc desc;       ///< the untyped row
   sge_object_type master_list;  ///< the master list holding the objects, see qconf_master_list_type()
   ocs_qconf_hooks<T> hooks;     ///< the typed hooks

   /** @brief Build the entry from its row, filling in the descriptor and the name field
    * @param row the row, without descriptor and name field
    * @param list the master list holding the objects
    * @param typed_hooks the typed hooks
    */
   constexpr OcsQConfObject(const qconf_object_desc &row, const sge_object_type list,
                            const ocs_qconf_hooks<T> &typed_hooks)
      : desc(row), master_list(list), hooks(typed_hooks) {
      desc.descr = T::descriptor.e;
      desc.name_nm = static_cast<int>(K);
   }
};

/**
 * @brief The key field of an entry with a key: a field of the entry's object type
 *
 * @tparam E the entry type
 * @return true; a mismatch fails the static assertion
 */
template <typename E>
consteval bool ocs_qconf_object_key_fits() {
   if constexpr (E::has_key) {
      static_assert(std::is_same_v<typename ocs::tom::FieldTraits<E::key>::Type, typename E::type>,
                    "the key field is a field of the object type");
   }
   return true;
}

/* *INDENT-OFF* */

/** @brief The types of the entries of #ocs_qconf_objects, at the index of their #QConfObj value
 *
 * This is where each value is given its object type and key field. The rows
 * and the hooks are in the definition of #ocs_qconf_objects in
 * ocs_QConfObject.cc, next to the hook functions they point at.
 */
using ocs_qconf_object_tuple = std::tuple<
   OcsQConfObject<QConfObj::CAL,  ocs::tom::CAL,  CAL_name>,
   OcsQConfObject<QConfObj::CK,   ocs::tom::CK,   CK_name>,
   OcsQConfObject<QConfObj::CE,   ocs::tom::CE,   CE_name>,
   OcsQConfObject<QConfObj::EH,   ocs::tom::EH,   EH_name>,
   OcsQConfObject<QConfObj::HGRP, ocs::tom::HGRP, HGRP_name>,
   OcsQConfObject<QConfObj::PE,   ocs::tom::PE,   PE_name>,
   OcsQConfObject<QConfObj::PR,   ocs::tom::PR,   PR_name>,
   OcsQConfObject<QConfObj::CQ,   ocs::tom::CQ,   CQ_name>,
   OcsQConfObject<QConfObj::RL,   ocs::tom::RL,   RL_name>,
   OcsQConfObject<QConfObj::RQS,  ocs::tom::RQS,  RQS_name>,
   OcsQConfObject<QConfObj::US,   ocs::tom::US,   US_name>,
   OcsQConfObject<QConfObj::UU,   ocs::tom::UU,   UU_name>,
   OcsQConfObject<QConfObj::STN,  ocs::tom::STN>,
   OcsQConfObject<QConfObj::SC,   ocs::tom::SC>,
   OcsQConfObject<QConfObj::CONF, ocs::tom::CONF, CONF_name>,
   OcsQConfObject<QConfObj::EV,   ocs::tom::EV,   EV_host>,
   OcsQConfObject<QConfObj::CAT,  ocs::tom::CT,   CT_id>,
   OcsQConfObject<QConfObj::AH,   ocs::tom::HGRP, HGRP_name>,
   OcsQConfObject<QConfObj::SH,   ocs::tom::HGRP, HGRP_name>,
   OcsQConfObject<QConfObj::MGR,  ocs::tom::US,   US_name>,
   OcsQConfObject<QConfObj::OP,   ocs::tom::US,   US_name>
>;

/* *INDENT-ON* */

static_assert(std::tuple_size_v<ocs_qconf_object_tuple> == static_cast<std::size_t>(QConfObj::OP) + 1,
              "ocs_qconf_objects needs exactly one entry per QConfObj value");

/** @brief Every qconf object type with its row and its typed hooks, at the index of its #QConfObj value
 *
 * Defined in ocs_QConfObject.cc, where the hook functions are. Initialised
 * dynamically, as the array it replaces was, because the rows hold gettext
 * calls; nothing reads it before main().
 */
extern const ocs_qconf_object_tuple ocs_qconf_objects;

/**
 * @brief The type of the entry the dispatch passes to its callable, for its compile-time members
 *
 * Inside a generic lambda `[](const auto &entry)` write
 * `ocs_qconf_object_entry_t<decltype(entry)>::type` for the object type and
 * `::key` for the key field.
 *
 * @tparam E the declared type of the lambda parameter
 */
template <typename E>
using ocs_qconf_object_entry_t = std::remove_cvref_t<E>;

/**
 * @brief The entry of #ocs_qconf_objects at an index, checked to be the entry of that #QConfObj value
 *
 * @tparam I the index, a #QConfObj value
 * @return the entry
 */
template <std::size_t I>
const auto &ocs_qconf_object_entry_at() {
   using E = std::tuple_element_t<I, ocs_qconf_object_tuple>;
   static_assert(static_cast<std::size_t>(E::value) == I, "ocs_qconf_objects is in the order of QConfObj");
   static_assert(ocs_qconf_object_key_fits<E>());
   return std::get<I>(ocs_qconf_objects);
}

/**
 * @brief The entry of a #QConfObj value known at compile time
 * @tparam O the value
 * @return the entry
 */
template <QConfObj O>
const auto &ocs_qconf_object() {
   return ocs_qconf_object_entry_at<static_cast<std::size_t>(O)>();
}

/**
 * @brief The dispatch of ocs_with_qconf_object() over all indices
 *
 * @tparam F the callable
 * @tparam I the indices 0 .. QConfObj::OP
 * @param o the object type to dispatch
 * @param f the callable
 * @return as ocs_with_qconf_object()
 */
template <typename F, std::size_t... I>
auto ocs_with_qconf_object_impl(const QConfObj o, F &f, std::index_sequence<I...>) {
   using R = std::invoke_result_t<F &, decltype(ocs_qconf_object_entry_at<0>())>;
   const auto index = static_cast<std::size_t>(o);
   if constexpr (std::is_void_v<R>) {
      // stops at the matching index; the result of the fold itself is not needed
      static_cast<void>(((index == I && (f(ocs_qconf_object_entry_at<I>()), true)) || ...));
   } else {
      R result{};
      static_cast<void>(((index == I && (result = f(ocs_qconf_object_entry_at<I>()), true)) || ...));
      return result;
   }
}

/**
 * @brief Call a callable with the entry of #ocs_qconf_objects for a #QConfObj known only at run time
 *
 * The callable, usually a generic lambda taking `const auto &entry`, is
 * compiled once per object type; inside it
 * `ocs_qconf_object_entry_t<decltype(entry)>` gives the object type (`::type`),
 * the key field (`::key`, valid if `::has_key`) and the value (`::value`) as
 * compile-time constants, and `entry.hooks` the typed hooks. So the body works
 * on handles of the type instead of a descriptor and a field number looked up
 * at run time.
 *
 * The result type is the one of the callable for the first entry; the other
 * instantiations must return a type assignable to it, and a non-void result
 * type must be default constructible - that is what a value outside the
 * enumeration gets, which no caller passes. The dispatch is a chain of integer
 * comparisons, without allocation.
 *
 * @tparam F the callable, taking `const OcsQConfObject<...> &`
 * @param o the object type
 * @param f the callable
 * @return the callable's result
 */
template <typename F>
auto ocs_with_qconf_object(const QConfObj o, F &&f) {
   return ocs_with_qconf_object_impl(o, f, std::make_index_sequence<std::tuple_size_v<ocs_qconf_object_tuple>>{});
}

/** @brief The descriptor of one object type.
 * @param o the object type
 * @return its row in the object table
 */
const qconf_object_desc &qconf_desc(QConfObj o);

/** @brief Which of qmaster's master lists holds the objects of a type.
 *
 * The write path cannot take this from gdi_object_t: CONF and SC carry
 * SGE_TYPE_NONE there and STN is one of the targets whose modify is a special
 * case in sge_c_gdi_mod() rather than a hook on the row, so that table knows the
 * list for the types that never needed it and not for the three that do. The
 * fact is a property of the object type, so it lives next to the descriptor and
 * both paths ask here (CS-2823).
 *
 * @param o the object type
 * @return its master list type, SGE_TYPE_NONE for an object that is not held in
 *         one - today only the event clients, which the event master builds on
 *         demand
 */
sge_object_type qconf_master_list_type(QConfObj o);

/** @brief What to call an object in a message
 *
 * A named object is called by its name; a singleton has none, and a message
 * about an empty name says less than nothing, so it is called by its type. Both
 * the client and qmaster need the same wording - the client for the refusals of
 * "-m<obj>", qmaster for the dry-run preview - so the answer lives with the
 * descriptor it is read from rather than once per caller (CS-2823).
 *
 * @param desc the object type's descriptor
 * @param name the object's name, or nullptr for a singleton
 * @return a label for a message, never nullptr
 */
const char *qconf_object_label(const qconf_object_desc &desc, const char *name);

/** @brief The key of a name field in the form a typed lookup takes it
 *
 * A host field is looked up with an ocs::tom::Host, which compares host names
 * case- and domain-insensitively as lGetElemHost() did; a string field with the
 * string, byte by byte (CS-2304).
 *
 * @tparam K the name field
 * @param name the name
 * @return the key
 */
template <auto K>
auto ocs_qconf_key_value(const char *name) {
   if constexpr (ocs::tom::FieldTraits<K>::type == ocs::tom::FieldType::Host) {
      return ocs::tom::Host(name);
   } else {
      return name;
   }
}

/** @brief An object's name, or nullptr when its type is not addressed by name
 *
 * Whether the field is a plain string or a host field (CS-2304), what the
 * caller wants is its text. The scheduler configuration and the share tree
 * carry no key: there is one object of the type and no name selects it. The
 * category's key is a number, which is not a name either.
 *
 * @tparam E the entry type of the object type
 * @param ep the object
 * @return the name, owned by @p ep, or nullptr
 */
template <typename E>
const char *ocs_qconf_key_of(const ocs::tom::ConstElement<typename E::type> ep) {
   if constexpr (E::has_name_key) {
      return static_cast<const char *>(ocs::tom::get<E::key>(ep));
   } else {
      return nullptr;
   }
}

/** @brief Write an object's name
 *
 * The counterpart of ocs_qconf_key_of(); a type that is not addressed by name
 * is left alone.
 *
 * @tparam E the entry type of the object type
 * @param ep the object
 * @param name the name to set
 * @return none
 */
template <typename E>
void ocs_qconf_set_name(const ocs::tom::Element<typename E::type> ep, [[maybe_unused]] const char *name) {
   if constexpr (E::has_name_key) {
      ocs::tom::set<E::key>(ep, name);
   }
}

/** @brief Whether a name is present in a list of objects
 *
 * Compared the way the name field demands, see ocs_qconf_key_value().
 *
 * @tparam E the entry type of the object type
 * @param names the list to search
 * @param name the name to look for
 * @return true when the list holds an element of that name; false for a type
 *         that is not addressed by name
 */
template <typename E>
bool ocs_qconf_name_in_list([[maybe_unused]] const ocs::tom::ConstList<typename E::type> names,
                            [[maybe_unused]] const char *name) {
   if constexpr (E::has_name_key) {
      if (!names || name == nullptr) {
         return false;
      }
      return names.template find_first<E::key>(ocs_qconf_key_value<E::key>(name)) != nullptr;
   } else {
      return false;
   }
}

/** @brief Read a whole list of objects out of a text or a file (CS-2823)
 *
 * The counterpart of ocs_qconf_parse_object() for a type whose text holds a
 * list rather than one object - the resource quota sets and the complex list.
 * The list is one unit: a text that does not parse yields nothing, and an
 * element the validator refuses discards the whole list, which is what the
 * client's own reader did with such a file (rqs_modify_from_file() sent nothing
 * at all when one rule set was rejected).
 *
 * `fields_out` is deliberately not used. The resource quota sets cannot be
 * tracked that way - the same field name appears once per rule - which is why
 * the client passed a null pointer there too, so a list has no
 * unprocessed-field check.
 *
 * @tparam T the object type of one element
 * @param answer_list receives the messages of this parse
 * @param fields the field list to read
 * @param format the flatfile format the text is in
 * @param filename the file to read, or the name a text is reported under
 * @param content the text, or nullptr to read from @p filename
 * @param validate the object type's validator, run per element, or nullptr
 * @param sfi the object type's spooling instruction
 * @param prepare the object type's transform, run per element, or nullptr
 * @param parse_values whether attribute values are parsed
 * @return the list, or an empty handle when it could not be read
 */
template <ocs::tom::ObjectType T>
ocs::tom::ListPtr<T>
ocs_qconf_parse_list(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, spooling_field *fields,
                     const spool_flatfile_format format, const char *filename, const char *content,
                     const ocs_qconf_check_fn<T> validate, const spool_flatfile_instr *sfi,
                     const ocs_qconf_check_fn<T> prepare, const bool parse_values) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::AN> local;
   // the flatfile parser builds the list with the descriptor it is given, which is T's
   auto lp = ocs::tom::ListPtr<T>::from_typed(spool_flatfile_read_list(&local, T::descriptor.e, fields, nullptr,
                                                                       parse_values, sfi, format,
                                                                       nullptr, filename, content));
   bool failed = answer_list_has_error(&local) || !lp;

   if (!failed) {
      for (auto ep = lp->front(); !failed && ep != nullptr; ep = ep.next()) {
         if (prepare != nullptr && prepare(ep, &local) != STATUS_OK) {
            failed = true;
         } else if (validate != nullptr && validate(ep, &local) != STATUS_OK) {
            failed = true;
         }
      }
   }

   if (failed) {
      lp.reset();
   }

   answer_list_append_list(answer_list, &local);
   DRETURN(lp);
}

/** @brief Parse one object from a flat file or from text, collecting the messages
 *
 * The chain qconf has always run before it sends an object: read it, make sure
 * no field was left unprocessed, let the object type's `prepare` transform it
 * (resolving a host name, for instance), then let its `validate` judge it.
 *
 * It used to be a private function of the client. It lives here because a
 * stored procedure runs inside qmaster and needs the very same chain - and
 * because having it twice would mean two behaviours drifting apart, with only
 * one of them covered by the characterisation test.
 *
 * The one deliberate difference to the client's old copy: it **collects** its
 * messages in @p answer_list rather than printing them. Printing is not an
 * option in the daemon, and the messages are what travels back to the caller
 * as the request's answers. The client keeps a thin wrapper that collects and
 * then prints.
 *
 * Parsing goes into a local list first and is appended on the way out. That is
 * not tidiness: the test for "did this read fail" is whether an **error** was
 * produced, not only whether the reader returned nothing, and asking that of a
 * list shared by a whole batch would let one bad item discard every item after
 * it.
 *
 * @tparam T the object type
 * @param answer_list receives the messages of this parse
 * @param fields the field list to read
 * @param format the flatfile format the text is in
 * @param filename the file to read, `-` for stdin, or nullptr when @p content is given
 * @param content the object's text, or nullptr to read from @p filename
 * @param validate the object type's validator, or nullptr
 * @param sfi the object type's spooling instruction
 * @param prepare the object type's transform, or nullptr
 * @param parse_values whether attribute values are parsed
 * @return the parsed object, or an empty handle when it could not be read
 */
template <ocs::tom::ObjectType T>
ocs::tom::ElementPtr<T>
ocs_qconf_parse_object(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, spooling_field *fields,
                       const spool_flatfile_format format, const char *filename, const char *content,
                       const ocs_qconf_check_fn<T> validate, const spool_flatfile_instr *sfi,
                       const ocs_qconf_check_fn<T> prepare, const bool parse_values) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::AN> local;
   int fields_out[MAX_NUM_FIELDS];
   fields_out[0] = NoName;

   /* CS-2299 H4: "-" reads the object from stdin. spool_flatfile_read_object()
    * only closes the stream when it opened it itself, so passing stdin is safe. */
   const bool from_stdin = (content == nullptr && filename != nullptr && strcmp(filename, "-") == 0);
   // the flatfile parser builds the element with the descriptor it is given, which is T's
   auto ep = ocs::tom::ElementPtr<T>::from_typed(spool_flatfile_read_object(&local, T::descriptor.e, nullptr, fields,
                                                                            fields_out, parse_values, sfi, format,
                                                                            from_stdin ? stdin : nullptr, filename,
                                                                            content));
   bool failed = answer_list_has_error(&local);

   if (!failed && ep != nullptr &&
       spool_get_unprocessed_field(fields, fields_out, &local) != NoName) {
      failed = true;
   }
   /* CS-2304: the transform runs before the name is used, so a prepare that
    * fails must stop the object from being applied. */
   if (!failed && ep != nullptr && prepare != nullptr && prepare(*ep, &local) != STATUS_OK) {
      failed = true;
   }
   if (!failed && ep != nullptr && validate != nullptr && validate(*ep, &local) != STATUS_OK) {
      failed = true;
   }
   if (failed) {
      ep.reset();
   }

   answer_list_append_list(answer_list, &local);
   DRETURN(ep);
}

/** @brief A private copy of a field list, for a renderer that mutates widths
 *
 * `spool_flatfile_align_list()` computes the column widths of a table and writes
 * them back into the `spooling_field` array it is given. Most providers hand out
 * a shared static table (`CE_fields`, `CAT_fields`, ...), which is harmless in a
 * client - one process, one call - and a data race inside qmaster, where reader
 * threads run procedures in parallel. A renderer that aligns therefore works on
 * its own copy (CS-2817).
 *
 * The copy is shallow: `align_list` only writes `width` of the top level, and
 * everything else in the struct is a pointer to static data.
 *
 * @param fields the list to copy, terminated by `nm == NoName`
 * @return the copy, which the caller frees with sge_free()
 */
spooling_field *qconf_copy_fields(const spooling_field *fields);

/**
 * @brief Fill one complex-value sublist with its CULL type and numeric value (CS-2313a)
 *
 * One step of ocs_qconf_json_fill_complex().
 *
 * @tparam F a field holding a complex entry (CE) sublist
 * @param obj the object whose sublist is filled (modified)
 * @param master_centry_list the complex entries the types are resolved from
 * @return none
 */
template <auto F>
void ocs_qconf_json_fill_complex_field(const ocs::tom::Element<typename ocs::tom::FieldTraits<F>::Type> obj,
                                       const ocs::tom::ConstList<ocs::tom::CE> master_centry_list) {
   static_assert(std::is_same_v<typename ocs::tom::FieldTraits<F>::Sub, ocs::tom::CE>,
                 "the field holds a complex entry sublist");
   const auto sub = ocs::tom::get<F>(obj);
   if (sub && !sub.empty()) {
      ocs::tom::ListPtr<ocs::tom::AN> fill_alp;
      centry_list_fill_request(sub, &fill_alp, master_centry_list, true, true, true);
      fill_alp.reset();
   }
}

/**
 * @brief Fill complex-value sublists with their CULL type/numeric value (CS-2313a).
 *
 * The raw complex_values / threshold entries stored on an exec host or queue carry
 * only a name + string value (CE_valtype is NONE). To emit native JSON numbers
 * (slots -> 32, h_vmem -> bytes, h_rt -> seconds) the writer needs CE_valtype and
 * CE_doubleval, which centry_list_fill_request() derives from the centry
 * definitions - the same fill qstat performs before display.
 *
 * Runs only for -fmt json (ASCII output uses CE_stringval and is unaffected), is
 * best-effort (unresolved entries keep their string value), and mutates only the
 * throwaway in-memory object that is about to be serialized. The sublists are
 * filled in the order of @p F.
 *
 * @tparam T the object type
 * @tparam F the fields holding complex entry (CE) sublists, e.g. EH_consumable_config_list
 * @param obj the object whose complex sublists should be filled (modified)
 * @param master_centry_list the complex entries the types are resolved from
 * @return none
 */
template <ocs::tom::ObjectType T, auto... F>
void ocs_qconf_json_fill_complex(const ocs::tom::Element<T> obj,
                                 const ocs::tom::ConstList<ocs::tom::CE> master_centry_list) {
   static_assert((std::is_same_v<typename ocs::tom::FieldTraits<F>::Type, T> && ...),
                 "the fields are fields of the object type");
   if (!obj || !master_centry_list) {
      return;
   }
   (ocs_qconf_json_fill_complex_field<F>(obj, master_centry_list), ...);
}

/** @} */
