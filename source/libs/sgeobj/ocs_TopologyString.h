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

#include <string>
#include <unordered_map>
#include <vector>

#include "uti/sge_dstring.h"

#include "sgeobj/ocs_TopologyNode.h"

namespace ocs {
   class TopologyString {
      std::string topology_;
      std::vector<TopologyNode> sockets_;

      [[nodiscard]] std::string from_tree_to_string() const;

      void initialize_logical_numbering();
   public:
      // Constructors
      TopologyString();
      explicit TopologyString(std::string topology);

      // Utility methods
      static void assign_prop_if_not_exists(std::vector<TopologyNode> &nodes, const std::string &name, int value);
      static void print(const std::vector<TopologyNode> &nodes, int indent);

      void parse();

      void print() const;
      void sort_by_characteristic(const std::string& characteristic, bool ascending);


      static constexpr size_t MAX_LENGTH = 2560;


      void correct_missing_threads();
      void correct_upper_lower();
      void correct();

      // Accessors
      [[nodiscard]] std::string to_string() const;


      std::string
      get_packed_binding(int units, std::string &unit, std::string &strategy, std::string &filter, std::string &sort, std::string &start, std::string &stop);
   };
} // namespace ocs