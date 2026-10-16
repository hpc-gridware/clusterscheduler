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

#include <string>
#include <vector>

#include "cull/cull.h"

namespace ocs {
   /** @brief The rules of the AUTO_ADD_HOSTS qmaster parameter.
    *
    * AUTO_ADD_HOSTS names the hosts which may add themselves to the cluster when their execution
    * daemon registers at the qmaster, and the host group they are added to. Its value is a list
    * of rules
    *
    *    AUTO_ADD_HOSTS=<rule>[;<rule>...]
    *
    * where a rule is a host matcher with an optional host group
    *
    *    <matcher>[:@<hostgroup>]
    *
    * A matcher containing a slash is an IPv4 network in CIDR notation. Every other matcher is a
    * pattern in the syntax of fnmatch(3) and is matched against the name of the host as well as
    * against its address, so a name, a name pattern, an address and an address pattern all work.
    * The rules are evaluated in the order in which they are given and the first matching rule
    * decides. The rule separator is a semicolon because a value in qmaster_params can contain
    * neither a comma nor a space - both are part of PARAMS_DELIMITER.
    */
   class AutoAddHosts {
   public:
      /** @brief One rule of the AUTO_ADD_HOSTS parameter. */
      struct Rule {
         bool is_network{false};   ///< the matcher is a network, else it is a pattern
         std::string pattern;      ///< name or address pattern, used if is_network is false
         u_long32 network{0};      ///< network address in network byte order, if is_network
         u_long32 netmask{0};      ///< netmask in network byte order, if is_network
         std::string hostgroup;    ///< host group the host is added to, empty if there is none
      };

      static bool parse(const char *value, std::vector<Rule> &rules, lList **answer_list);
      static bool match_host(const char *hostname, const char *ip, std::string &hostgroup);
   };
}
