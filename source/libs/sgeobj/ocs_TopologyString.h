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

#include "cull/cull.h"

#include "sgeobj/ocs_BindingUnit.h"
#include "sgeobj/ocs_BindingStart.h"
#include "sgeobj/ocs_BindingEnd.h"

namespace ocs {
   // Product internal topology string representation
   // e.g., "(N[size=4096](S(X[size=512](Y(C(T)(T)))(Y(C(T)(T)))(Y(E(T))(E(T))(E(T))(E(T))))))"
   class TopologyString {
      struct Node {
         char c; //< Character representing the node type
         std::vector<Node> nodes; //< Child nodes
         std::unordered_map<std::string, std::string> characteristics; //< Characteristics of the node, e.g., size, status
      };

      // Prefixes for characteristics
      static const std::string PREFIX;
      static const std::string FREE_PREFIX;
      static const std::string BOUND_PREFIX;
      static const std::string ID_PREFIX;

      // Tree structure representing the topology
      std::vector<Node> nodes;

      // Some internal service methods
      void parse_to_tree(const std::string &topology);
      void sort_tree_nodes(char node_type, char sort_characteristic, bool ascending = true);
   public:
      // Maximum length of a topology string
      static constexpr size_t MAX_LENGTH = 2560;
      static constexpr int NO_POS = -1;

      // Static constants for node characters
      static const std::string DATA_NODE_CHARACTERS;
      static const std::string HARDWARE_NODE_CHARACTERS;
      static const std::string STRUCTURE_CHARACTERS;

      // Check if a string contains valid node characters
      static bool contains_valid_node_names(std::string& sequence);

      static void elem_mark_nodes_as_used_or_unused(lListElem *elem, int nm, TopologyString &binding_now, const TopologyString &binding_to_use, bool mark_used);

      // Constructors
      TopologyString() = default;
      explicit TopologyString(const std::string &internal_topology);

      // String to tree and tree to string conversion
      [[nodiscard]] std::string to_string(bool with_data_nodes = false, bool with_structure = false,
                                          bool with_characteristics = false, bool with_internal_characteristics = false,
                                          bool with_tree_format = false, bool show_as_unused = false) const;
      [[nodiscard]] std::string to_product_topology_string() const;
      [[nodiscard]] std::string to_unused_internal_topology_string() const;

      // Sorting
      void sort_tree(const std::string& sort_criteria, char sort_characteristic);

      // Service functions
      void correct_topology_upper_lower();

      bool find_first_unused_thread(int *pos, int *socket, int *core, int *thread) const;
      [[nodiscard]] int find_first_unused_thread() const;
      void mark_node_as_used_or_unused(int id, bool mark_used);
      void reset_topology(const std::string &topology);

      void mark_nodes_as_used_or_unused(const TopologyString &topo, bool mark_used);

      [[nodiscard]] std::vector<int> find_n_packed_units(int bamount, BindingUnit::Unit bunit, BindingStart::Start bstart, BindingEnd::End end) const;
      void mark_units_as_used_or_unused(std::vector<int> &ids, BindingUnit::Unit unit, bool mark_used);
   };
} // namespace ocs