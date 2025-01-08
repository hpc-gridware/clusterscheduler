#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2024 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/KRB.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Kerberos Authentication
*
* This is the list type we use to hold the client connection list for kerberos authentication.
* @todo can probably be removed: There is no usage of KRB_* attributes in the code, not even in source/security/krb
*
*    SGE_STRING(KRB_commproc) - Commproc Id
*    Commproc Id of the connection (to sge_qmaster?) from commlib.
*
*    SGE_ULONG(KRB_id) - Id
*    @todo add description
*
*    SGE_HOST(KRB_host) - Host
*    @todo add description
*
*    SGE_ULONG64(KRB_timestamp) - Timestamp
*    @todo add description
*
*    SGE_STRING(KRB_auth_context) - Authentication Context
*    @todo add description
*
*    SGE_LIST(KRB_tgt_list) - TGT List
*    @todo add description
*
*/

enum {
   KRB_commproc = KRB_LOWERBOUND,
   KRB_id,
   KRB_host,
   KRB_timestamp,
   KRB_auth_context,
   KRB_tgt_list
};

LISTDEF(KRB_Type)
   SGE_STRING(KRB_commproc, CULL_DEFAULT)
   SGE_ULONG(KRB_id, CULL_DEFAULT)
   SGE_HOST(KRB_host, CULL_DEFAULT)
   SGE_ULONG64(KRB_timestamp, CULL_DEFAULT)
   SGE_STRING(KRB_auth_context, CULL_DEFAULT)
   SGE_LIST(KRB_tgt_list, KTGT_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(KRBN)
   NAME("KRB_commproc")
   NAME("KRB_id")
   NAME("KRB_host")
   NAME("KRB_timestamp")
   NAME("KRB_auth_context")
   NAME("KRB_tgt_list")
NAMEEND

#define KRB_SIZE sizeof(KRBN)/sizeof(char *)


