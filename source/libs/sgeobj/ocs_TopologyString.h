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
   public:
      struct Node {
         // Character representing the node type
         char c;

         // Child nodes
         std::vector<Node> nodes;

         // Characteristics of the node, e.g., size, status
         std::unordered_map<std::string, std::string> characteristics;
      };

   private:
      // Product internal topology string representation
      // e.g., "(N[size=4096](S(X[size=512](Y(C(T)(T)))(Y(C(T)(T)))(Y(E(T))(E(T))(E(T))(E(T))))))"
      std::string topology_;

      // Tree structure representing the topology
      std::vector<Node> nodes;

      void sort_tree_nodes(char node_type, char sort_characteristic, bool ascending = true);

   public:
      static constexpr size_t MAX_LENGTH = 2560;
      static constexpr std::string DATA_NODE_CHARACTERS = "NABUVWXYZ";
      static constexpr std::string HARDWARE_NODE_CHARACTERS = "SCEFGHT";
      static constexpr std::string STRUCTURE_CHARACTERS = "()";
      static constexpr std::string PREFIX = "#";
      static constexpr std::string FREE_PREFIX = PREFIX + "f";
      static constexpr std::string BOUND_PREFIX = PREFIX + "b";
      static constexpr std::string ID_PREFIX = PREFIX + "i";

      // Constructors
      TopologyString();
      explicit TopologyString(std::string topology);

      // String to Tree and Tree to String conversion
      void parse_to_tree();
      [[nodiscard]] std::string to_string() const;
      [[nodiscard]] std::string from_tree_to_string(bool with_data_nodes, bool with_structure, bool with_characteristics,
                                                    bool with_internal_characteristics, bool with_tree_format) const;

      void sort_tree(const std::string& sort_criteria, char sort_characteristic);

      void remove_attributes();
      void remove_structure();
      void remove_memory_info();
      void remove_single_threads();

      void print(bool with_data_nodes = false, bool with_structure = false, bool with_characteristics = false,
                 bool with_internal_characteristics = false, bool with_tree_format = false) const;
   };
} // namespace ocs