#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2025 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/MR.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Mail Receiver
*
* One object of this type specifies a mail receiver, meaning an email address
* in the form user@host.
* It is used e.g. when submitting jobs via qsub -M user[@host][,user[@host],...]
* @todo why do we split it into user and host? We could just have a single string holding an email address.
*
*    SGE_STRING(MR_user) - User Name
*    User name of the mail receipient
*
*    SGE_HOST(MR_host) - Host Name
*    Host / Domain part of a mail receipient.
*
*/

enum {
   MR_user = MR_LOWERBOUND,
   MR_host
};

LISTDEF(MR_Type)
   SGE_STRING(MR_user, CULL_PRIMARY_KEY | CULL_SUBLIST)
   SGE_HOST(MR_host, CULL_SUBLIST)
LISTEND

NAMEDEF(MRN)
   NAME("MR_user")
   NAME("MR_host")
NAMEEND

#define MR_SIZE sizeof(MRN)/sizeof(char *)


