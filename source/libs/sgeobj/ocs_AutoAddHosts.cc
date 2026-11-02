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

#include <arpa/inet.h>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fnmatch.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "uti/ocs_Pattern.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "sgeobj/ocs_AutoAddHosts.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_hgroup.h"

#include "msg_sgeobjlib.h"

// the rules of AUTO_ADD_HOSTS are separated by a semicolon - neither a comma nor a space can be
// used because both are part of PARAMS_DELIMITER and would split the qmaster_params value itself
#define AUTO_ADD_HOSTS_RULE_DELIMITER ";"

/** @brief Parses one rule of the AUTO_ADD_HOSTS parameter.
 *
 * A rule is a host matcher with an optional host group, e.g. "10.42.0.0/16:@cloud" or "gpu-*".
 * A valid rule is appended to the rules vector, an invalid one is reported in the answer list.
 *
 * @param rule_str the rule to parse
 * @param rules vector the parsed rule is appended to
 * @param answer_list answer list to report errors, may be nullptr
 * @return true if the rule is valid, false otherwise
 */
static bool
auto_add_hosts_parse_rule(const char *rule_str, std::vector<ocs::AutoAddHosts::Rule> &rules,
                          lList **answer_list) {
   DENTER(TOP_LAYER);

   ocs::AutoAddHosts::Rule rule{};
   std::string matcher{rule_str};

   // an IPv6 network has more than one colon - the communication library is IPv4 only, therefore
   // such a rule can never match and is rejected instead of being silently ignored
   if (matcher.find(':') != matcher.rfind(':')) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_AUTOADDHOSTS_IPV6_S, rule_str);
      DRETURN(false);
   }

   // a host group may follow the matcher, separated by a colon
   const size_t colon = matcher.find(':');
   if (colon != std::string::npos) {
      rule.hostgroup = matcher.substr(colon + 1);
      matcher.erase(colon);

      if (!ocs::is_hgroup_name(rule.hostgroup.c_str())) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_AUTOADDHOSTS_HOSTGROUP_S, rule_str);
         DRETURN(false);
      }
      if (!hgroup_check_name(answer_list, rule.hostgroup.c_str())) {
         DRETURN(false);
      }
   }

   if (matcher.empty()) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_AUTOADDHOSTS_NO_MATCHER_S, rule_str);
      DRETURN(false);
   }

   const size_t slash = matcher.find('/');
   if (slash == std::string::npos) {
      // everything which is not a network is a host name pattern
      rule.pattern = matcher;
   } else {
      const std::string address_str = matcher.substr(0, slash);
      const std::string prefix_str = matcher.substr(slash + 1);

      char *end = nullptr;
      errno = 0;
      const long prefix = strtol(prefix_str.c_str(), &end, 10);
      if (prefix_str.empty() || end == nullptr || *end != '\0' || errno != 0 ||
          prefix < 0 || prefix > 32) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_AUTOADDHOSTS_PREFIX_S, matcher.c_str());
         DRETURN(false);
      }

      in_addr address{};
      if (inet_pton(AF_INET, address_str.c_str(), &address) != 1) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_AUTOADDHOSTS_NETWORK_S, matcher.c_str());
         DRETURN(false);
      }

      // a prefix length of 0 matches every address, shifting by 32 bits would be undefined
      rule.is_network = true;
      rule.netmask = prefix == 0 ? 0 : htonl(0xffffffffu << (32 - prefix));
      rule.network = address.s_addr & rule.netmask;
   }

   rules.push_back(rule);
   DRETURN(true);
}

/** @brief Parses the value of the AUTO_ADD_HOSTS parameter.
 *
 * All rules are parsed, so that a single invalid rule does not hide the errors of the following
 * ones and does not remove the valid rules from the result.
 *
 * @param value the value of the AUTO_ADD_HOSTS parameter, may be nullptr, empty or "NONE"
 * @param rules vector the parsed rules are appended to, in the order in which they are given
 * @param answer_list answer list to report errors, may be nullptr
 * @return true if all rules are valid, false otherwise
 */
bool
ocs::AutoAddHosts::parse(const char *value, std::vector<Rule> &rules, lList **answer_list) {
   DENTER(TOP_LAYER);

   if (value == nullptr || *value == '\0' || strcasecmp(value, NONE_STR) == 0) {
      DRETURN(true);
   }

   bool ret = true;
   struct saved_vars_s *context = nullptr;
   for (const char *s = sge_strtok_r(value, AUTO_ADD_HOSTS_RULE_DELIMITER, &context); s != nullptr;
        s = sge_strtok_r(nullptr, AUTO_ADD_HOSTS_RULE_DELIMITER, &context)) {
      if (!auto_add_hosts_parse_rule(s, rules, answer_list)) {
         ret = false;
      }
   }
   sge_free_saved_vars(context);

   DRETURN(ret);
}

/** @brief Checks if a host matches a pattern rule of the AUTO_ADD_HOSTS parameter.
 *
 * The address is matched as it is. A host name is matched the way host names are compared
 * everywhere else in the cluster: case-insensitive, and honouring ignore_fqdn and default_domain
 * of the bootstrap file.
 *
 * - A plain host name, a matcher without any fnmatch(3) metacharacter, is compared with
 *   sge_hostcmp(), which normalises both sides. A plain address is no host name and is matched
 *   against the address only; normalising it would cut 10.42.0.5 down to 10 if ignore_fqdn is set.
 * - A real pattern cannot be normalised, sge_hostcpy() would cut node[0.9]* down to node[0. It is
 *   matched against the normalised host name and against the host name as the communication
 *   library verified it, so that a pattern with a domain, e.g. gpu-*.example.com, still matches
 *   if ignore_fqdn is set.
 *
 * @param pattern the pattern of the rule
 * @param hostname name of the host which wants to add itself
 * @param ip address of the host in dotted decimal notation, may be nullptr or empty if unknown
 * @return true if the pattern matches the name or the address of the host, false otherwise
 */
static bool
auto_add_hosts_match_pattern(const std::string &pattern, const char *hostname, const char *ip) {
   DENTER(TOP_LAYER);

   if (ip != nullptr && *ip != '\0' && fnmatch(pattern.c_str(), ip, 0) == 0) {
      DRETURN(true);
   }

   bool matched = false;
   if (!ocs::is_pattern(pattern.c_str())) {
      in_addr address{};
      if (inet_pton(AF_INET, pattern.c_str(), &address) != 1) {
         matched = sge_hostcmp(pattern.c_str(), hostname) == 0;
      }
   } else {
      matched = sge_hostmatch_pattern(pattern.c_str(), hostname) == 0;
      if (!matched) {
         std::string lower_pattern{pattern};
         std::string lower_hostname{hostname};
         for (char &c : lower_pattern) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
         }
         for (char &c : lower_hostname) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
         }
         matched = fnmatch(lower_pattern.c_str(), lower_hostname.c_str(), 0) == 0;
      }
   }

   DRETURN(matched);
}

/** @brief Checks if a host matches one of the given AUTO_ADD_HOSTS rules.
 *
 * The rules are evaluated in the order in which they are given, the first matching rule decides.
 *
 * A network rule can only match if the address of the host is known. A pattern is matched against
 * the host name as the qmaster knows it, which is the name the communication library has verified
 * to belong to the connecting host, and against the address that name resolves to. Host names
 * are compared as everywhere else in the cluster, see auto_add_hosts_match_pattern().
 *
 * @param rules the parsed rules, see parse()
 * @param hostname name of the host which wants to add itself
 * @param ip address of the host in dotted decimal notation, may be nullptr or empty if unknown
 * @param hostgroup set to the host group of the matching rule, empty if the rule names none
 * @return true if one of the rules matches the host, false otherwise
 */
bool
ocs::AutoAddHosts::match_rules(const std::vector<Rule> &rules, const char *hostname,
                               const char *ip, std::string &hostgroup) {
   DENTER(TOP_LAYER);

   if (hostname == nullptr) {
      DRETURN(false);
   }

   bool ret = false;
   for (const Rule &rule : rules) {
      bool matched = false;

      if (rule.is_network) {
         in_addr address{};
         if (ip == nullptr || *ip == '\0') {
            DPRINTF("address of host " SFQ " is unknown, cannot match a network rule\n", hostname);
         } else if (inet_pton(AF_INET, ip, &address) == 1) {
            matched = (address.s_addr & rule.netmask) == rule.network;
         }
      } else {
         // a pattern is matched against the name of the host and against its address, so that an
         // administrator can write a name, a name pattern, an address or an address pattern
         matched = auto_add_hosts_match_pattern(rule.pattern, hostname, ip);
      }

      if (matched) {
         hostgroup = rule.hostgroup;
         ret = true;
         break;
      }
   }

   DRETURN(ret);
}

/** @brief Checks if a host may add itself to the cluster.
 *
 * Evaluates the rules of the AUTO_ADD_HOSTS parameter of the global configuration, see
 * match_rules(). Rules which do not parse are ignored here; they have been reported when the
 * configuration was modified.
 *
 * @param hostname name of the host which wants to add itself
 * @param ip address of the host in dotted decimal notation, may be nullptr or empty if unknown
 * @param hostgroup set to the host group of the matching rule, empty if the rule names none
 * @return true if one of the rules matches the host, false otherwise
 */
bool
ocs::AutoAddHosts::match_host(const char *hostname, const char *ip, std::string &hostgroup) {
   DENTER(TOP_LAYER);

   const std::string value = mconf_get_auto_add_hosts();
   std::vector<Rule> rules;
   parse(value.c_str(), rules, nullptr);

   DRETURN(match_rules(rules, hostname, ip, hostgroup));
}
