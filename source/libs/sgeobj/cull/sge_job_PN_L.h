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
 * This code was generated from file source/libs/sgeobj/json/PN.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Path Name
*
* An object of the PathName type specifies a certain path on a host.
* It is used for specifying stdin/stdout/stderr paths via submit options -i/-o/-e/-j.
* For different hosts different paths can be specified in the form [host:]path[,[host:]path...].
* There is some simple file staging facility, to be evaluated.
*
*    SGE_STRING(PN_path) - Path
*    Path to a file or directory.
*
*    SGE_HOST(PN_host) - Host Name
*    Name of the host where this specification is valid.
*    If it is left empty (nullptr), then the path is valid on all hosts (default).
*
*    SGE_HOST(PN_file_host) - File Host
*    @todo for file staging, the host where the file is available?
*
*    SGE_BOOL(PN_file_staging) - Do File Staging
*    Do file staging when set to true. @todo status of filestaging?
*
*/

enum {
   PN_path = PN_LOWERBOUND,
   PN_host,
   PN_file_host,
   PN_file_staging
};

LISTDEF(PN_Type)
   SGE_STRING(PN_path, CULL_PRIMARY_KEY | CULL_SUBLIST)
   SGE_HOST(PN_host, CULL_DEFAULT)
   SGE_HOST(PN_file_host, CULL_DEFAULT)
   SGE_BOOL(PN_file_staging, CULL_DEFAULT)
LISTEND

NAMEDEF(PNN)
   NAME("PN_path")
   NAME("PN_host")
   NAME("PN_file_host")
   NAME("PN_file_staging")
NAMEEND

#define PN_SIZE sizeof(PNN)/sizeof(char *)


