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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Declarations for per host attributes: one value per host or host group
 *
 * A per host attribute is a list of entries, each naming a host, a host group or #HOSTREF_DEFAULT and the
 * value that applies there. There is one object type per value type (ocs::tom::ASTR, ocs::tom::AULNG, ...);
 * #ocs_attr_traits names the host reference and the value field of each, so every operation is one template
 * over the attribute type.
 *
 * @see sge_attr.cc
 */

#include "sgeobj/cull/sge_attr_ASTR_L.h"
#include "sgeobj/cull/sge_attr_AULNG_L.h"
#include "sgeobj/cull/sge_attr_ABOOL_L.h"
#include "sgeobj/cull/sge_attr_ATIME_L.h"
#include "sgeobj/cull/sge_attr_AMEM_L.h"
#include "sgeobj/cull/sge_attr_AINTER_L.h"
#include "sgeobj/cull/sge_attr_ASTRING_L.h"
#include "sgeobj/cull/sge_attr_ASTRLIST_L.h"
#include "sgeobj/cull/sge_attr_AUSRLIST_L.h"
#include "sgeobj/cull/sge_attr_APRJLIST_L.h"
#include "sgeobj/cull/sge_attr_ACELIST_L.h"
#include "sgeobj/cull/sge_attr_ASOLIST_L.h"
#include "sgeobj/cull/sge_attr_AQTLIST_L.h"
#include "sgeobj/sge_answer.h"

#include "tom/ocs_Element.h"
#include "tom/ocs_FieldTraits.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_ABOOL.h"
#include "sgeobj/tom/ocs_tom_ACELIST.h"
#include "sgeobj/tom/ocs_tom_AINTER.h"
#include "sgeobj/tom/ocs_tom_AMEM.h"
#include "sgeobj/tom/ocs_tom_AN.h"
#include "sgeobj/tom/ocs_tom_APRJLIST.h"
#include "sgeobj/tom/ocs_tom_AQTLIST.h"
#include "sgeobj/tom/ocs_tom_ASOLIST.h"
#include "sgeobj/tom/ocs_tom_ASTR.h"
#include "sgeobj/tom/ocs_tom_ASTRLIST.h"
#include "sgeobj/tom/ocs_tom_ATIME.h"
#include "sgeobj/tom/ocs_tom_AULNG.h"
#include "sgeobj/tom/ocs_tom_AUSRLIST.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_SO.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_US.h"

/**
 * @name Flags for per host attribute lists
 *
 * A value can be given for a host or for a host group, and a host may be in
 * several groups - so two settings can both apply to it. These decide what
 * happens then.
 * @{
 */
#define HOSTATTR_DEFAULT            0x0000 ///< reject an ambiguous setting
#define HOSTATTR_ALLOW_AMBIGUITY    0x0001 ///< accept it; the queue instance becomes ambiguous
#define HOSTATTR_OVERWRITE          0x0002 ///< a later setting replaces an earlier one for the same host
/** @} */

/**
 * @brief The host reference standing for "every host"
 *
 * Handled internally as a host group name, hence the leading `@`. The slash
 * makes a collision with a user defined host group impossible, since those may
 * not contain one.
 */
#define HOSTREF_DEFAULT             "@/"

/**
 * @brief The fields of a per host attribute type
 *
 * A specialisation exists for every attribute type: `href` is the field naming the host, host group or
 * #HOSTREF_DEFAULT of an entry, `value` the field holding its value, and `value_type` what reading that value
 * from a read-only entry gives - a number, a string, or a read-only handle of the value list. A value that is a
 * list also names `key`, the field keying the entries of that list. What an administrator writes is always
 * parsed from a string by the value field's own parser.
 *
 * @tparam A the attribute type, e.g. ocs::tom::ASTR
 */
template <ocs::tom::ObjectType A>
struct ocs_attr_traits;

/** @brief String values (ASTR_Type), e.g. `tmpdir` */
template <>
struct ocs_attr_traits<ocs::tom::ASTR> {
   static constexpr auto href = ASTR_href;    ///< the host reference
   static constexpr auto value = ASTR_value;  ///< the value
   using value_type = const char *;           ///< the value as read
};

/** @brief Unsigned values (AULNG_Type), e.g. `slots` */
template <>
struct ocs_attr_traits<ocs::tom::AULNG> {
   static constexpr auto href = AULNG_href;    ///< the host reference
   static constexpr auto value = AULNG_value;  ///< the value
   using value_type = uint32_t;                ///< the value as read
};

/** @brief Boolean values (ABOOL_Type), e.g. `rerun` */
template <>
struct ocs_attr_traits<ocs::tom::ABOOL> {
   static constexpr auto href = ABOOL_href;    ///< the host reference
   static constexpr auto value = ABOOL_value;  ///< the value
   using value_type = bool;                    ///< the value as read
};

/** @brief Time values (ATIME_Type), e.g. `h_rt` */
template <>
struct ocs_attr_traits<ocs::tom::ATIME> {
   static constexpr auto href = ATIME_href;    ///< the host reference
   static constexpr auto value = ATIME_value;  ///< the value
   using value_type = const char *;            ///< the value as read
};

/** @brief Memory values (AMEM_Type), e.g. `h_vmem` */
template <>
struct ocs_attr_traits<ocs::tom::AMEM> {
   static constexpr auto href = AMEM_href;     ///< the host reference
   static constexpr auto value = AMEM_value;   ///< the value
   using value_type = const char *;            ///< the value as read
};

/** @brief Interval values (AINTER_Type), e.g. `notify` */
template <>
struct ocs_attr_traits<ocs::tom::AINTER> {
   static constexpr auto href = AINTER_href;   ///< the host reference
   static constexpr auto value = AINTER_value; ///< the value
   using value_type = const char *;            ///< the value as read
};

/** @brief Queue type bit masks (AQTLIST_Type), `qtype` */
template <>
struct ocs_attr_traits<ocs::tom::AQTLIST> {
   static constexpr auto href = AQTLIST_href;    ///< the host reference
   static constexpr auto value = AQTLIST_value;  ///< the value
   using value_type = uint32_t;                  ///< the value as read
};

/** @brief String lists (ASTRLIST_Type), e.g. `pe_list` */
template <>
struct ocs_attr_traits<ocs::tom::ASTRLIST> {
   static constexpr auto href = ASTRLIST_href;               ///< the host reference
   static constexpr auto value = ASTRLIST_value;             ///< the value
   static constexpr auto key = ST_name;                      ///< the key of an entry of the value list
   using value_type = ocs::tom::ConstList<ocs::tom::ST>;     ///< the value as read
};

/** @brief User set lists (AUSRLIST_Type), e.g. `user_lists` */
template <>
struct ocs_attr_traits<ocs::tom::AUSRLIST> {
   static constexpr auto href = AUSRLIST_href;               ///< the host reference
   static constexpr auto value = AUSRLIST_value;             ///< the value
   static constexpr auto key = US_name;                      ///< the key of an entry of the value list
   using value_type = ocs::tom::ConstList<ocs::tom::US>;     ///< the value as read
};

/** @brief Project lists (APRJLIST_Type), e.g. `projects` */
template <>
struct ocs_attr_traits<ocs::tom::APRJLIST> {
   static constexpr auto href = APRJLIST_href;               ///< the host reference
   static constexpr auto value = APRJLIST_value;             ///< the value
   static constexpr auto key = PR_name;                      ///< the key of an entry of the value list
   using value_type = ocs::tom::ConstList<ocs::tom::PR>;     ///< the value as read
};

/** @brief Complex value lists (ACELIST_Type), e.g. `complex_values` */
template <>
struct ocs_attr_traits<ocs::tom::ACELIST> {
   static constexpr auto href = ACELIST_href;                ///< the host reference
   static constexpr auto value = ACELIST_value;              ///< the value
   static constexpr auto key = CE_name;                      ///< the key of an entry of the value list
   using value_type = ocs::tom::ConstList<ocs::tom::CE>;     ///< the value as read
};

/** @brief Subordinate lists (ASOLIST_Type), `subordinate_list` */
template <>
struct ocs_attr_traits<ocs::tom::ASOLIST> {
   static constexpr auto href = ASOLIST_href;                ///< the host reference
   static constexpr auto value = ASOLIST_value;              ///< the value
   static constexpr auto key = SO_name;                      ///< the key of an entry of the value list
   using value_type = ocs::tom::ConstList<ocs::tom::SO>;     ///< the value as read
};

/**
 * @brief A per host attribute type: an object type with an #ocs_attr_traits specialisation
 * @tparam A the object type
 */
template <typename A>
concept OcsHostAttr = ocs::tom::ObjectType<A> && requires {
   ocs_attr_traits<A>::href;
   ocs_attr_traits<A>::value;
};

/**
 * @brief A per host attribute type whose value is a keyed list
 * @tparam A the object type
 */
template <typename A>
concept OcsHostAttrKeyedList = OcsHostAttr<A> && requires { ocs_attr_traits<A>::key; };

/**
 * @brief What a lookup in a per host attribute list found
 *
 * @tparam A the attribute type
 */
template <OcsHostAttr A>
struct ocs_attr_lookup {
   /// the entry whose value was read last, empty if none was; read its value with
   /// `ocs::tom::get<ocs_attr_traits<A>::value>(entry)`
   ocs::tom::ConstElement<A> entry;
   /// whether the lookup succeeded; false with an entry only when two host groups gave the host a value
   /// and there is no default entry
   bool found = false;
};

/**
 * @brief The operations on a per host attribute list, one class per attribute type
 *
 * Declared here and defined in sge_attr.cc, which instantiates the class for every attribute type that has
 * #ocs_attr_traits; so the bodies are compiled once. Call them through the free functions below, which deduce
 * the attribute type from the list.
 *
 * @tparam A the attribute type
 */
template <OcsHostAttr A>
struct ocs_attr_list_ops {
   static ocs_attr_lookup<A>
   find(ocs::tom::ConstList<A> this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *hostname,
        const char **matching_host_or_group, const char **matching_group, bool *is_ambiguous,
        ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list);

   static ocs::tom::ConstElement<A>
   find_href(ocs::tom::ConstList<A> this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *hostname,
             bool *found);

   static bool
   append_to_dstring(ocs::tom::ConstList<A> this_list, dstring *string);

   static bool
   parse_from_string(ocs::tom::ListPtr<A> &this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                     const char *string, int flags);
};

/**
 * @brief Find the value a per host attribute list gives a host
 *
 * A value for the host itself wins; otherwise the value of the one host group that contains the host, and if
 * no group does, the default value (#HOSTREF_DEFAULT). Two groups that both contain the host make the setting
 * ambiguous; the default value applies then.
 *
 * @tparam A the attribute type, deduced from the list
 * @param this_list the attribute list, may be an empty handle
 * @param[out] answer_list receives an error if there is no value at all
 * @param hostname the host, may be nullptr
 * @param[out] matching_host_or_group receives the host group the value comes from, if any
 * @param[out] matching_group receives the second host group if the setting is ambiguous
 * @param[out] is_ambiguous set to true if two host groups give the host a value; written only when the host
 *             has no value of its own
 * @param master_hgroup_list the host groups
 * @return the entry whose value applies and whether one was found; nothing for an empty list or no hostname
 */
template <OcsHostAttr A>
ocs_attr_lookup<A>
ocs_attr_list_find(const ocs::tom::ConstList<A> this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                   const char *hostname, const char **matching_host_or_group, const char **matching_group,
                   bool *is_ambiguous, const ocs::tom::ConstList<ocs::tom::HGRP> master_hgroup_list) {
   return ocs_attr_list_ops<A>::find(this_list, answer_list, hostname, matching_host_or_group, matching_group,
                                     is_ambiguous, master_hgroup_list);
}

/**
 * @brief Find the value a per host attribute list gives a host, without looking at host groups
 *
 * @tparam A the attribute type, deduced from the list
 * @param this_list the attribute list, may be an empty handle
 * @param[out] answer_list receives an error if there is neither a value for the host nor a default value
 * @param hostname the host, may be nullptr
 * @param[out] found true if the host has a value of its own, false if the default value applies; written only
 *             when an entry is returned
 * @return the entry whose value applies, empty if none
 */
template <OcsHostAttr A>
ocs::tom::ConstElement<A>
ocs_attr_list_find_href(const ocs::tom::ConstList<A> this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                        const char *hostname, bool *found) {
   return ocs_attr_list_ops<A>::find_href(this_list, answer_list, hostname, found);
}

/**
 * @brief Render a per host attribute list as an administrator writes it
 *
 * The default value first, then the host group values, then the host values, each as `[<ref>=<value>]`;
 * `NONE` for a list without entries.
 *
 * @tparam A the attribute type, deduced from the list
 * @param this_list the attribute list, may be an empty handle
 * @param[out] string receives the text, appended
 * @return true
 */
template <OcsHostAttr A>
bool
ocs_attr_list_append_to_dstring(const ocs::tom::ConstList<A> this_list, dstring *string) {
   return ocs_attr_list_ops<A>::append_to_dstring(this_list, string);
}

/**
 * @brief Parse a per host attribute list as an administrator writes it
 *
 * `value{,[<ref>=<value>]}`; the first token without a reference is the default value. The entries are added
 * to @p this_list, which is created if it is empty; a list created by this call is freed again on an error.
 *
 * @tparam A the attribute type
 * @param this_list the list the entries are added to
 * @param[out] answer_list receives the errors
 * @param string the text, may be nullptr
 * @param flags #HOSTATTR_OVERWRITE lets a later setting for a reference replace an earlier one, otherwise a
 *        reference given twice is an error; #HOSTATTR_ALLOW_AMBIGUITY and #HOSTATTR_DEFAULT make no difference,
 *        a host group setting is never checked for ambiguity here
 * @return true on success
 */
template <OcsHostAttr A>
bool
ocs_attr_list_parse_from_string(ocs::tom::ListPtr<A> &this_list, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                const char *string, const int flags) {
   return ocs_attr_list_ops<A>::parse_from_string(this_list, answer_list, string, flags);
}
