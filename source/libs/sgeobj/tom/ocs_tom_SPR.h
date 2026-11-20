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
 * This code was generated from file source/libs/sgeobj/json/SPR.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Spooling Rule: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of SPR, Spooling Rule
 *
 * The enumeration is the type of every field number of SPR, so a parameter
 * of this type takes the field numbers of SPR only.
 */
enum SPR_Field {
   /**
    * @brief Name
    *
    * Unique name of the rule.
    */
   SPR_name = SPR_LOWERBOUND,
   /**
    * @brief URL
    *
    * An url, e.g. a spool directory, a database url etc.
    */
   SPR_url,
   /**
    * @brief Option Function
    *
    * Function pointer to a function to set any database specific options,
    * e.g. whether to do database recovery at startup or not.
    */
   SPR_option_func,
   /**
    * @brief Startup Function
    *
    * Function pointer to a startup function,
    * e.g. establishing a connection to a database.
    */
   SPR_startup_func,
   /**
    * @brief Shutdown Function
    *
    * Function pointer to a shutdown function,
    * e.g. disconnecting from a database or closing file handles.
    */
   SPR_shutdown_func,
   /**
    * @brief Maintenance Function
    *
    * Function pointer to a maintenance function for
    * - creating the database tables / directories in case of filebased spooling
    * - switching between spooling with/without history
    * - backup
    * - cleaning up / compressing database
    * - etc.
    */
   SPR_maintenance_func,
   /**
    * @brief Trigger Function
    *
    * Function pointer to a trigger function.
    * A trigger function is used to trigger regular actions, e.g.
    * checkpointing and cleaning the transaction log in case of the
    * Berkeley DB or vacuuming in case of PostgreSQL.
    */
   SPR_trigger_func,
   /**
    * @brief Transaction Function
    *
    * Function pointer to a function beginning and ending transactions.
    *  
    */
   SPR_transaction_func,
   /**
    * @brief List Function
    *
    * Pointer to a function reading complete lists (master lists)
    * from the spooling data source.
    */
   SPR_list_func,
   /**
    * @brief Read Function
    *
    * Pointer to a function reading a single object from the
    * spooling data source.
    */
   SPR_read_func,
   /**
    * @brief Read Keys Function
    *
    * Reads all keys from a spooling database
    * matching beginning with a certain pattern.
    * @see e.g. spool_berkeleydb_read_keys()
    */
   SPR_read_keys_func,
   /**
    * @brief Write Function
    *
    * Pointer to a function writing a single object.
    */
   SPR_write_func,
   /**
    * @brief Delete Function
    *
    * Pointer to a function deleting a single object.
    *  
    */
   SPR_delete_func,
   /**
    * @brief Validate Function
    *
    * Pointer to a function validating a single object.
    */
   SPR_validate_func,
   /**
    * @brief Validate List Function
    *
    * Pointer to a function validating a list of objects.
    *  
    */
   SPR_validate_list_func,
   /**
    * @brief Client Data
    *
    * Clientdata; any pointer, can be used to store and
    * reference rule specific data, e.g. file or database handles.
    */
   SPR_clientdata,
};

namespace ocs::tom {

   /**
    * @brief Spooling Rule
    *
    * A spooling rule describes a certain way to store and retrieve
    * data from a defined storage facility.
    *  
    * Spooling rules can implement spooling to files in a certain
    * directory or spooling into a database, to an LDAP repository, etc.
    *  
    * A spooling context can contain multiple spooling rules.
    */
   struct SPR {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {SPR_name, "SPR_name", FieldType::String, Flag::Unique | Flag::Hash},
         {SPR_url, "SPR_url", FieldType::String, Flag::None},
         {SPR_option_func, "SPR_option_func", FieldType::Ref, Flag::NoTransfer},
         {SPR_startup_func, "SPR_startup_func", FieldType::Ref, Flag::NoTransfer},
         {SPR_shutdown_func, "SPR_shutdown_func", FieldType::Ref, Flag::NoTransfer},
         {SPR_maintenance_func, "SPR_maintenance_func", FieldType::Ref, Flag::NoTransfer},
         {SPR_trigger_func, "SPR_trigger_func", FieldType::Ref, Flag::NoTransfer},
         {SPR_transaction_func, "SPR_transaction_func", FieldType::Ref, Flag::NoTransfer},
         {SPR_list_func, "SPR_list_func", FieldType::Ref, Flag::NoTransfer},
         {SPR_read_func, "SPR_read_func", FieldType::Ref, Flag::NoTransfer},
         {SPR_read_keys_func, "SPR_read_keys_func", FieldType::Ref, Flag::NoTransfer},
         {SPR_write_func, "SPR_write_func", FieldType::Ref, Flag::NoTransfer},
         {SPR_delete_func, "SPR_delete_func", FieldType::Ref, Flag::NoTransfer},
         {SPR_validate_func, "SPR_validate_func", FieldType::Ref, Flag::NoTransfer},
         {SPR_validate_list_func, "SPR_validate_list_func", FieldType::Ref, Flag::NoTransfer},
         {SPR_clientdata, "SPR_clientdata", FieldType::Ref, Flag::NoTransfer},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of SPR belongs to SPR */
   template <>
   struct TypeOf<SPR_Field> {
      using type = SPR;   ///< the object type
   };

} // namespace ocs::tom
