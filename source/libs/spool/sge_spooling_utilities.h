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
 * @brief Deciding which attributes of an object get spooled, and how
 */

#include <cstddef>
#include <cstdint>

#include "uti/sge_dstring.h"

#include "tom/ocs_Types.h"

#include "sgeobj/sge_object.h"

#include "tom/ocs_Element.h"
#include "tom/ocs_FieldTraits.h"
#include "tom/ocs_Raw.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_SPR.h"
#include "sgeobj/tom/ocs_tom_SPT.h"
#include "sgeobj/sge_answer.h"

/** @defgroup spool_utilities Spooling utilities
 * @brief Which attributes of an object are spooled, and under what name
 *
 * A cull object has more attributes than belong in a spool file: runtime
 * state, cached values, things derived on load. Which ones survive is not
 * decided here but in the object's own definition - every field carries a
 * `mt` bitmask, and #CULL_SPOOL is the bit that says "write me".
 *
 * This module turns that bitmask into something the backends can iterate.
 * #spool_get_fields_to_spool walks a `lDescr` under a #spool_instr_t and
 * returns the matching fields as a #spooling_field array, which is what the
 * flatfile writer formats and the reader parses back.
 * @{
 */


/** @brief How to select the fields of one object type for spooling */
typedef struct spool_instr {
   int selection;             ///< Bitmask ANDed with each field's `mt`, e.g. #CULL_SPOOL - a non-zero result means the field is spooled
   bool copy_field_names;     ///< Copy the attribute name into the #spooling_field instead of pointing into the descriptor
   bool strip_field_prefix;   ///< Drop the `XX_` type prefix from the name, so `QU_qname` is spooled as `qname`
   const struct spool_instr *sub_instr;   ///< The instruction to apply to elements of sublist fields
   const void *clientdata;    ///< Free for the caller; unused by the framework itself
} spool_instr_t;

/** @name The ready made selections
 *
 * Each `*_instr` is the top level instruction for one family of objects and
 * points at the matching `*_subinstr` for whatever sublists it contains.
 * @{
 */
extern const spool_instr_t spool_config_instr;      ///< The default: #CULL_SPOOL fields, names copied and un-prefixed
extern const spool_instr_t spool_config_subinstr;   ///< Sublists of the above: #CULL_SUBLIST fields, names left alone

extern const spool_instr_t spool_complex_instr;     ///< Complex entries: #CULL_SPOOL fields, names kept verbatim
extern const spool_instr_t spool_complex_subinstr;  ///< Sublists of a complex entry

extern const spool_instr_t spool_user_instr;        ///< Users and projects: #CULL_SPOOL plus #CULL_SPOOL_USER
extern const spool_instr_t spool_userprj_subinstr;  ///< Sublists of a user or project - it names *itself* as its own `sub_instr`, so nesting to any depth uses the same selection
/** @} */

/**
 * @brief How a walker renders and parses the value of a field that is neither a list nor an object
 *
 * A property of the field, fixed at compile time in its #ocs_spool_field_ops (#ocs_spool_field_ops::json). The
 * flatfile walkers do not use it: they render a field in the default form of its type or, for a field with a
 * symbolic kind (#ocs_spool_field_ops::symbolic), as a word, through ocs_spool_append_value() and
 * ocs_spool_parse_value().
 */
enum class ocs_spool_render : std::uint8_t {
   plain,         ///< the default conversion of the field's type
   by_name,       ///< a word, ocs_object_append_symbolic() / ocs_object_parse_symbolic() of the field's kind
   datetime,      ///< an absolute gmt64 time stamp, an ISO date-time in JSON
   bitfield,      ///< a bit field, an array of the names of the set bits in JSON (#ocs_spool_field_ops::names)
   enumeration,   ///< an enumeration, the name of the value in JSON (#ocs_spool_field_ops::names)
   avalue,        ///< a string holding a value of a complex type (#ocs_spool_field_ops::avalue_type)
   number,        ///< a string that holds a number, a native number in JSON
   perm_list      ///< an RBAC permission rule list (a string), an array of rule objects in JSON
};

/**
 * @brief The spooling properties of one field, as the run-time walkers use them
 *
 * Plain data, one small constant record per field: the field number, its type and the sub-type from the model's
 * field table, and the renderings classified at compile time (spool/ocs_spool_field_info.h). A row of a typed field
 * table points at the field's record #ocs_spool_ops; a row built from a descriptor or a field number at run time
 * points at the record in the per-type tables of ocs_spool_type_ops_of() / ocs_spool_field_ops_of(), which is made
 * by the same code. The element arrives untyped because the walkers are shared by all object types; they read and
 * write the field with the checked run-time accessors of tom/ocs_Raw.h, ocs::tom::get<V>() and ocs::tom::set(), the
 * value type `V` chosen from #type or, for a field of a rendering kind, from the kind, which is bound to the field
 * type at compile time (ocs_spool_make_ops()).
 */
struct ocs_spool_field_ops {
   int nm;                       ///< the field number
   ocs::tom::FieldType type;     ///< the field's type, from the field table of the object model
   /// the symbolic kind of the value (ocs_object_symbolic_of()), ocs_object_symbolic::none for a field written in
   /// the default form of its type; the flatfile walkers write and read a value of a kind as a word
   ocs_object_symbolic symbolic;
   ocs_spool_render json;        ///< how the JSON walkers render the value
   /// the complex type of a JSON #ocs_spool_render::avalue field as the value of an ocs::CEntry::Type,
   /// 0 (NONE) for other fields; read it with ocs_spool_avalue_type_of() (spool/ocs_spool_field_info.h)
   std::uint8_t avalue_type;
   /// the descriptor of the sub-type of a list or object field, nullptr if it has none
   const lDescr *sub_descr;
   /// the names of the bits of a JSON bitfield or of the values of a JSON enumeration, nullptr terminated;
   /// nullptr for other fields
   const char *const *names;

   /**
    * @brief Whether the field has a plain value: a number, a bool, a string or a host name
    * @return true for a plain value; false for a list, an object or a reference
    */
   [[nodiscard]] constexpr bool has_value() const noexcept {
      return type != ocs::tom::FieldType::List && type != ocs::tom::FieldType::Object &&
             type != ocs::tom::FieldType::Ref;
   }
};

/** @brief One attribute to spool, as the backends see it
 *
 * #spool_get_fields_to_spool returns these as an array terminated by an entry
 * with `nm == NoName`. The `free_*` flags say which pointers that array owns,
 * so that #spool_free_spooling_fields can release exactly those.
 *
 * The flatfile field tables are typed (spool/flatfile/ocs_spool_fields.h); the
 * arrays generated from them carry the record of each field in `ops`. An
 * array built at run time takes a record made the same way from
 * ocs_spool_field_ops_of() or ocs_spool_type_ops_of(), so every entry but the
 * terminating one has `ops`.
 */
typedef struct spooling_field {
   int nm;                    ///< The cull attribute number, `NoName` in the terminating entry
   int width;                 ///< Field width for formatted output, 0 for unformatted
   const char *name;          ///< The name to write into the spool file
   bool free_name;            ///< `name` was copied and must be freed with the array
   struct spooling_field *sub_fields;   ///< For a list attribute, the fields to spool per element
   bool free_sub_fields;      ///< `sub_fields` was built here and must be freed with the array
   const void *clientdata;    ///< Free for the backend; the flatfile writer keeps its format info here
   /// Backend hook parsing this attribute out of `buffer`, instead of the default conversion
   int (*read_func) (lListElem *ep, int nm, const char *buffer, ocs::tom::ListPtr<ocs::tom::AN> *alp);
   int (*write_func) (const lListElem *ep, int nm, dstring *buffer,
                      ocs::tom::ListPtr<ocs::tom::AN> *alp);   ///< Backend hook rendering this attribute into `buffer`
   /// The spooling record of the field, nullptr only in the terminating entry
   const ocs_spool_field_ops *ops;
} spooling_field;

/** @brief The spooling records of the fields of one object type, see ocs_spool_type_ops_of() */
struct ocs_spool_type_ops {
   int base;                         ///< the number of the type's first field
   std::size_t size;                 ///< the number of the type's fields
   const ocs_spool_field_ops *ops;   ///< the record of field `base + i` at index `i`
};

const ocs_spool_type_ops *
ocs_spool_type_ops_of(const lDescr *descr);

const ocs_spool_field_ops *
ocs_spool_field_ops_of(int nm);

const char *
ocs_spool_append_value(const ocs_spool_field_ops &ops, ocs::tom::ConstRawElement ep,
                       ocs::tom::ListPtr<ocs::tom::AN> *answer_list, dstring *buffer);

bool
ocs_spool_parse_value(const ocs_spool_field_ops &ops, ocs::tom::RawElement ep,
                      ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *value);

spooling_field *
spool_get_fields_to_spool(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lDescr *descr,
                          const spool_instr_t *instr);

spooling_field *
spool_free_spooling_fields(spooling_field *fields);

bool
spool_default_validate_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                          ocs::tom::ConstElement<ocs::tom::SPT> type,
                          ocs::tom::ConstElement<ocs::tom::SPR> rule,
                          ocs::tom::RawElement object,
                          const sge_object_type object_type);

bool
spool_default_validate_list_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                          ocs::tom::ConstElement<ocs::tom::SPT> type, ocs::tom::ConstElement<ocs::tom::SPR> rule,
                          const sge_object_type object_type);

ocs::tom::ElementPtr<ocs::tom::EH> ocs_spool_exechost_static_copy(ocs::tom::ConstElement<ocs::tom::EH> object);

/** @} */
