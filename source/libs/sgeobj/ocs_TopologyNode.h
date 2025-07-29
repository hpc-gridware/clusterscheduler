#pragma once

/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025 HPC-Gridware GmbH
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

#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace ocs {
   class TopologyNode {
   public:
      // Possible statuses for a topology nodes
      enum Status {FREE, USED, UNAVAILABLE, UNINITIALIZED};
      static std::string status_to_string(Status status);

      // Keys used for the property map
      static constexpr auto STATUS = "ST";
      static constexpr auto LOGICAL_SOCKET = "S";
      static constexpr auto LOGICAL_CORE = "C";
      static constexpr auto LOGICAL_THREAD = "T";
      static constexpr auto NUMA_NODE_ID = "N";
      static constexpr auto CACHE_3RD_ID = "X";
      static constexpr auto CACHE_2ND_ID = "Y";
      static constexpr auto TOPOLOGY_ID = "TO";
      static constexpr auto NUMBER_OF_FREE_C_THREADS = "#C";
      static constexpr auto NUMBER_OF_FREE_E_THREADS = "#E";
      static constexpr auto NUMBER_OF_USED_C_THREADS = "#c";
      static constexpr auto NUMBER_OF_USED_E_THREADS = "#e";

      // Data
      char c;
      std::vector<TopologyNode> nodes;
      std::unordered_map<std::string, int> properties;

      // Constructors
      explicit TopologyNode(char c, Status status);
      explicit TopologyNode(char c, Status status, std::vector<TopologyNode> nodes);

      // Property handling
      void assign_prop_if_not_exists(const std::string &name, int value);

      // Helper to get combined status of a CPU-threads
      static Status get_combined_status(Status status, bool is_free);

      // Convert the node to a string representation
      void to_string(std::ostringstream& oss) const;
   };
} // namespace ocs