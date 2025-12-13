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

#include <vector>
#include <tuple>
#include <cctype>

#include "../cull/cull.h"
#include "../uti/sge_log.h"
#include "../uti/sge_rmon_macros.h"
#include "sge_answer.h"

#include "../sgeobj/ocs_Version.h"
#include "../../daemons/qmaster/msg_qmaster.h"

#ifdef ADD_COPYRIGHT
#  include "copyright.h"
#endif

// @todo CHANGE THE VERSION NUMBERS HERE. ADD A NEW VERSION TO THE LIST BELOW IF PACKING OR CULL CHANGES.
// dist/inst_sge has to be be updated too.
#define OCS_VERSION_MAJOR 9
#define OCS_VERSION_MINOR 0
#define OCS_VERSION_PATCH 10
#define OCS_VERSION_SUFFIX ""

static const std::string OCS_VERSION_STRING{std::to_string(OCS_VERSION_MAJOR) + "."
         + std::to_string(OCS_VERSION_MINOR) + "." + std::to_string(OCS_VERSION_PATCH) + OCS_VERSION_SUFFIX};
static const uint32_t OCS_VERSION{0x10009000};

static const std::vector<std::tuple<uint32_t, std::string>> OCS_ALL_VERSIONS_VECTOR{
   { 0x10000000, "5.0"},
   { 0x10000001, "5.1"},
   { 0x10000002, "5.2"},
   { 0x10000003, "5.2.3"},
   { 0x100000F0, "5.3alpha1"},
   { 0x100000F1, "5.3beta1 without hashing"},
   { 0x100000F2, "5.3beta1"},
   { 0x100000F3, "5.3beta2"},
   { 0x100000F4, "5.3"},
   { 0x10000FFF, "6.0"},
   { 0x10001000, "6.0u3"},
   { 0x10001001, "6.0u4"},
   { 0x10001002, "6.0u8_2"},
   { 0x10002000, "6.1"},
   { 0x100020F0, "6.1AR_snapshot1"},
   { 0x10002001, "6.1u7"},
   { 0x100020F1, "6.2"},
   { 0x100020F2, "6.2u3"},
   { 0x100020F3, "6.2u4"},
   { 0x100020F4, "6.2u5alpha1"},
   { 0x100020F5, "6.2u5alpha2"},
   { 0x100020F6, "6.2u5beta1"},
   { 0x100020F7, "6.2u5beta2"},
   { 0x100020F8, "6.2u5beta2"},
   { 0x100020F8, "6.2u5"},
   { 0x10003000, "8.0.x Univa"},
   { 0x10003001, "8.0.x Some Gridengine"},
   { OCS_VERSION, OCS_VERSION_STRING},
};

#ifdef ADD_GRIDWARE_COPYRIGHT
static const std::string OCS_LONG_PRODUCT_NAME{"Gridware Cluster Scheduler"};
static const std::string OCS_SHORT_PRODUCT_NAME{"GCS"};
#else
static const std::string OCS_LONG_PRODUCT_NAME{"Open Cluster Scheduler"};
static const std::string OCS_SHORT_PRODUCT_NAME{"OCS"};
#endif

std::string
ocs::Version::get_version_string() {
   return OCS_VERSION_STRING; // e.g "9.0.2alpha"
}

std::uint32_t
ocs::Version::get_version() {
   return OCS_VERSION;
}

std::string
ocs::Version::get_short_product_name() {
   return OCS_SHORT_PRODUCT_NAME;
}

std::string
ocs::Version::get_long_product_name() {
   return OCS_LONG_PRODUCT_NAME;
}

std::tuple<int, int, int, std::string>
ocs::Version::get_version_token() {
   std::string version = ocs::Version::get_version_string();
   size_t pos1 = version.find('.');
   size_t pos2 = version.find('.', pos1 + 1);
   size_t pos3 = version.find_first_not_of("0123456789", pos2 + 1);

   int major = 0;
   int minor = 0;
   int patch = 0;
   std::string suffix;
   if (pos1 != std::string::npos && pos2 != std::string::npos) {
      major = std::stoi(version.substr(0, pos1));
      minor = std::stoi(version.substr(pos1 + 1, pos2 - pos1 - 1));
      patch = std::stoi(version.substr(pos2 + 1, pos3 - pos2 - 1));
      if (pos3 != std::string::npos) {
         suffix = version.substr(pos3);
      }
   }

   return std::make_tuple(major, minor, patch, suffix);
}

/** @brief Return true if the version of the client matches the version of the server
 *
 * The message that might be added to the answer list will contain the product name and version if the client version
 * is known. Otherwise, it will just contain the version number.
 *
 * @param alpp In case of a version mismatch, this list will be filled with an error message
 * @param version The version of the client
 * @param host The host name of the client
 * @param commproc The commproc ID of the client
 * @param id The ID of the client
 * @return false if the versions do not match. true otherwise
 */
bool
ocs::Version::do_versions_match(lList **alpp, const uint32_t version, const char *host, const char *commproc, const int id) {
   DENTER(TOP_LAYER);

   // Do the clients version match the servers version?
   if (version != get_version()) {

      // find the version string for the client version
      std::string client_version;
      bool found = false;
      for (const auto& elem : OCS_ALL_VERSIONS_VECTOR) {
         if (version == std::get<0>(elem)) {
            client_version = std::get<1>(elem);
            found = true;
         }
      }

      // If we know the version string we can print it a specific warning message
      // otherwise we just print the version ID
      dstring ds;
      char buffer[256];
      sge_dstring_init(&ds, buffer, sizeof(buffer));
      if (found) {
         WARNING(MSG_GDI_WRONG_GDI_SSISS, host, commproc, id, client_version.c_str(), get_version_string().c_str());
      } else {
         WARNING(MSG_GDI_WRONG_GDI_SSIUS, host, commproc, id, sge_u32c(version), get_version_string().c_str());
      }
      answer_list_add(alpp, SGE_EVENT, STATUS_EVERSION, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }
   DRETURN(true);
}



#if !(ADD_COPYRIGHT || ADD_HPC_GRIDWARE_COPYRIGHT)
extern const char SFLN_ELN[]{"\n\
   Cluster Scheduler is based on code donated by Sun Microsystems.\n\
   The copyright is owned by Sun Microsystems and other contributors.\n\
   It has been made available to the open source community under the SISSL license.\n\
   For further information and the latest news visit: @fBhttp://gridengine.sunsource.net\n\n"};

extern const char DQS_ACK[]{"\n\
We would like to acknowledge and thank the efforts of the\n\
Florida State University in creating the DQS program.\n"};

#endif

#ifndef ADD_HPC_GRIDWARE_COPYRIGHT

extern const char SISSL[]{"\n\
The Contents of this file are made available subject to the terms of\n\
the Sun Industry Standards Source License Version 1.2\n\
\n\
Sun Microsystems Inc., March, 2001\n\
\n\
\n\
Sun Industry Standards Source License Version 1.2\n\
=================================================\n\
The contents of this file are subject to the Sun Industry Standards\n\
Source License Version 1.2 (the \"License\"); You may not use this file\n\
except in compliance with the License. You may obtain a copy of the\n\
License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html\n\
\n\
Software provided under this License is provided on an \"AS IS\" basis,\n\
WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,\n\
WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,\n\
MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.\n\
See the License for the specific provisions governing your rights and\n\
obligations concerning the Software.\n\
\n\
The Initial Developer of the Original Code is: Sun Microsystems, Inc.\n\
\n\
Copyright: 2001 by Sun Microsystems, Inc.\n\
\n\
All Rights Reserved.\n"};

#endif
