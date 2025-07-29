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

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <unordered_map>

#include "uti/sge_string.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/ocs_TopologyNode.h"

ocs::TopologyNode::TopologyNode(const char c, const Status status) : c(c) {
   assign_prop_if_not_exists(STATUS, status);
}

ocs::TopologyNode::TopologyNode(const char c, const Status status, std::vector<TopologyNode> nodes) : TopologyNode(c, status) {
   this->nodes = std::move(nodes);
}

std::string
ocs::TopologyNode::status_to_string(const Status status) {
   switch (status) {
      case FREE:
          return "FREE";
      case USED:
          return "USED";
      case UNAVAILABLE:
          return "UNAVAILABLE";
      case UNINITIALIZED:
          return "UNINITIALIZED";
      default:
         return "UNKNOWN";
   }
}

// Only assign the value if the property doesn't already exist
void
ocs::TopologyNode::assign_prop_if_not_exists(const std::string &name, const int value) {
   if (!properties.contains(name)) {
      properties[name] = value;
   }
}

ocs::TopologyNode::Status
ocs::TopologyNode::get_combined_status(Status status, const bool is_free) {
   if (is_free) {
      switch (status) {
         case UNINITIALIZED:
            status = FREE;
            break;
         case UNAVAILABLE:
            status = USED;
            break;
         case FREE:
         case USED:
         default:
            break;
      }
   } else {
      switch (status) {
         case UNINITIALIZED:
            status = UNAVAILABLE;
            break;
         case FREE:
         case USED:
            status = USED;
            break;
         case UNAVAILABLE:
         default:
            break;
      }
   }
   return status;
}

void
ocs::TopologyNode::to_string(std::ostringstream& oss) const {
   oss << c;
   for (const auto& child : nodes) {
      child.to_string(oss);
   }
}

