#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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

/*
 * This code was generated from file source/libs/sgeobj/json/TEST.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Test Object: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of TEST, Test Object
 *
 * The enumeration is the type of every field number of TEST, so a parameter
 * of this type takes the field numbers of TEST only.
 */
enum TEST_Field {
   /**
    * @brief Boolean type attribute
    *
    * This field holds a boolean value.
    */
   TEST_bool = TEST_LOWERBOUND,
   /**
    * @brief uint32_t type attribute
    *
    * This field holds a uint32_t value.
    */
   TEST_uint32,
   /**
    * @brief uint64_t type attribute
    *
    * This field holds a uint64_t value.
    */
   TEST_uint64,
   /**
    * @brief int type attribute
    *
    * This field holds a int value.
    */
   TEST_int,
   /**
    * @brief long type attribute
    *
    * This field holds a long value.
    */
   TEST_long,
   /**
    * @brief double type attribute
    *
    * This field holds a double value.
    */
   TEST_double,
   /**
    * @brief host type attribute
    *
    * This field holds a host name.
    */
   TEST_host,
   /**
    * @brief string type attribute
    *
    * This field holds a string value.
    */
   TEST_string,
   /**
    * @brief reference type attribute to store a pointer
    *
    * This filed hold a pointer
    */
   TEST_ref,
   /**
    * @brief object type attribute to store JB_Type an object
    *
    * This filed hold a object reference.
    */
   TEST_obj_jb,
   /**
    * @brief object type attribute to store any other object
    *
    * This filed hold a object reference.
    */
   TEST_obj_any,
};

namespace ocs::tom {

   struct JB;

   /**
    * @brief Test Object
    *
    * Objects of this type are used for testing purposes with modules tests.
    */
   struct TEST {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {TEST_bool, "TEST_bool", FieldType::Bool, Flag::Spool},
         {TEST_uint32, "TEST_uint32", FieldType::Uint32, Flag::Spool | Flag::Unique | Flag::PrimaryKey},
         {TEST_uint64, "TEST_uint64", FieldType::Uint64, Flag::Spool | Flag::Hash},
         {TEST_int, "TEST_int", FieldType::Int32, Flag::Spool},
         {TEST_long, "TEST_long", FieldType::Int64, Flag::Spool},
         {TEST_double, "TEST_double", FieldType::Double, Flag::Spool},
         {TEST_host, "TEST_host", FieldType::Host, Flag::Spool},
         {TEST_string, "TEST_string", FieldType::String, Flag::Spool},
         {TEST_ref, "TEST_ref", FieldType::Ref, Flag::None},
         {TEST_obj_jb, "TEST_obj_jb", FieldType::Object, Flag::None},
         {TEST_obj_any, "TEST_obj_any", FieldType::Object, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of TEST belongs to TEST */
   template <>
   struct TypeOf<TEST_Field> {
      using type = TEST;   ///< the object type
   };

   /** @brief TEST_obj_jb holds elements of JB */
   template <>
   struct SubType<TEST_obj_jb> {
      using type = JB;   ///< the sub-type
   };

} // namespace ocs::tom

/** @brief The descriptor of TEST for the list library: a synonym for ocs::tom::TEST::descriptor */
inline constexpr const lDescr *TEST_Type = ocs::tom::TEST::descriptor.e;
