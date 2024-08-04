#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024 HPC-Gridware GmbH
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

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

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
*
*    SGE_STRING(SPR_name) - Name
*    Unique name of the rule.
*
*    SGE_STRING(SPR_url) - URL
*    An url, e.g. a spool directory, a database url etc.
*
*    SGE_REF(SPR_option_func) - Option Function
*    Function pointer to a function to set any database specific options,
*    e.g. whether to do database recovery at startup or not.
*
*    SGE_REF(SPR_startup_func) - Startup Function
*    Function pointer to a startup function,
*    e.g. establishing a connection to a database.
*
*    SGE_REF(SPR_shutdown_func) - Shutdown Function
*    Function pointer to a shutdown function,
*    e.g. disconnecting from a database or closing file handles.
*
*    SGE_REF(SPR_maintenance_func) - Maintenance Function
*    Function pointer to a maintenance function for
*    - creating the database tables / directories in case of filebased spooling
*    - switching between spooling with/without history
*    - backup
*    - cleaning up / compressing database
*    - etc.
*
*    SGE_REF(SPR_trigger_func) - Trigger Function
*    Function pointer to a trigger function.
*    A trigger function is used to trigger regular actions, e.g.
*    checkpointing and cleaning the transaction log in case of the
*    Berkeley DB or vacuuming in case of PostgreSQL.
*
*    SGE_REF(SPR_transaction_func) - Transaction Function
*    Function pointer to a function beginning and ending transactions.
*    
*
*    SGE_REF(SPR_list_func) - List Function
*    Pointer to a function reading complete lists (master lists)
*    from the spooling data source.
*
*    SGE_REF(SPR_read_func) - Read Function
*    Pointer to a function reading a single object from the
*    spooling data source.
*
*    SGE_REF(SPR_read_keys_func) - Read Keys Function
*    Reads all keys from a spooling database
*    matching beginning with a certain pattern.
*    @see e.g. spool_berkeleydb_read_keys()
*
*    SGE_REF(SPR_write_func) - Write Function
*    Pointer to a function writing a single object.
*
*    SGE_REF(SPR_delete_func) - Delete Function
*    Pointer to a function deleting a single object.
*    
*
*    SGE_REF(SPR_validate_func) - Validate Function
*    Pointer to a function validating a single object.
*
*    SGE_REF(SPR_validate_list_func) - Validate List Function
*    Pointer to a function validating a list of objects.
*    
*
*    SGE_REF(SPR_clientdata) - Client Data
*    Clientdata; any pointer, can be used to store and
*    reference rule specific data, e.g. file or database handles.
*
*/

enum {
   SPR_name = SPR_LOWERBOUND,
   SPR_url,
   SPR_option_func,
   SPR_startup_func,
   SPR_shutdown_func,
   SPR_maintenance_func,
   SPR_trigger_func,
   SPR_transaction_func,
   SPR_list_func,
   SPR_read_func,
   SPR_read_keys_func,
   SPR_write_func,
   SPR_delete_func,
   SPR_validate_func,
   SPR_validate_list_func,
   SPR_clientdata
};

LISTDEF(SPR_Type)
   SGE_STRING(SPR_name, CULL_UNIQUE | CULL_HASH)
   SGE_STRING(SPR_url, CULL_DEFAULT)
   SGE_REF(SPR_option_func, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(SPR_startup_func, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(SPR_shutdown_func, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(SPR_maintenance_func, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(SPR_trigger_func, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(SPR_transaction_func, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(SPR_list_func, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(SPR_read_func, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(SPR_read_keys_func, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(SPR_write_func, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(SPR_delete_func, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(SPR_validate_func, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(SPR_validate_list_func, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(SPR_clientdata, CULL_ANY_SUBTYPE, CULL_DEFAULT)
LISTEND

NAMEDEF(SPRN)
   NAME("SPR_name")
   NAME("SPR_url")
   NAME("SPR_option_func")
   NAME("SPR_startup_func")
   NAME("SPR_shutdown_func")
   NAME("SPR_maintenance_func")
   NAME("SPR_trigger_func")
   NAME("SPR_transaction_func")
   NAME("SPR_list_func")
   NAME("SPR_read_func")
   NAME("SPR_read_keys_func")
   NAME("SPR_write_func")
   NAME("SPR_delete_func")
   NAME("SPR_validate_func")
   NAME("SPR_validate_list_func")
   NAME("SPR_clientdata")
NAMEEND

#define SPR_SIZE sizeof(SPRN)/sizeof(char *)


