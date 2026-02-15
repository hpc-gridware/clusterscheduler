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

#include <algorithm>
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

#include "uti/ocs_Topo.h"

#include "cull/cull.h"

#include "sgeobj/ocs_BindingUnit.h"
#include "sgeobj/ocs_BindingStart.h"
#include "sgeobj/ocs_BindingStop.h"

#define DATA_NODE_CHARACTERS_DEFINE "NABUVWXYZ"
#define HARDWARE_NODE_CHARACTERS_DEFINE "SCEFGHT"
#define NO_POS_DEFINE (-1)

static constexpr unsigned char ascii_to_upper(const unsigned char ch) {
   return (ch >= 'a' && ch <= 'z') ? (ch - 'a' + 'A') : ch;
}

static constexpr unsigned char ascii_to_lower(const unsigned char ch) {
   return (ch >= 'A' && ch <= 'Z') ? (ch - 'A' + 'a') : ch;
}

static constexpr std::array<int, 256>
make_char_to_index() {
   std::array<int, 256> arr{};

   // initialize the array
   arr.fill(NO_POS_DEFINE);

   // set the index for each letter that we support in TopologyStrings
   int idx = 0;
   for (constexpr std::string_view chars = HARDWARE_NODE_CHARACTERS_DEFINE DATA_NODE_CHARACTERS_DEFINE;
        const unsigned char ch : chars) {
      const auto upper = ascii_to_upper(ch);
      const auto lower = ascii_to_lower(ch);
      if (arr[upper] == NO_POS_DEFINE) {
         arr[upper] = idx++;
      }
      if (arr[lower] == NO_POS_DEFINE) {
         arr[lower] = idx++;
      }
                                              }
   return arr;
}
constexpr auto char_to_index = make_char_to_index();

static constexpr int
get_char_to_index_size() {
   return *std::ranges::max_element(char_to_index) + 1;
}

constexpr auto char_to_index_size = get_char_to_index_size();

namespace ocs {
   // Product internal topology string representation
   // e.g., "(N[size=4096](S(X[size=512](Y(C(T)(T)))(Y(C(T)(T)))(Y(E(T))(E(T))(E(T))(E(T))))))"
   class TopologyString {


      class Node {
      public:
         unsigned char c; //< Character representing the node type
         int id; //< Unique ID of the node
         std::vector<Node> nodes; //< Child nodes
         std::array<int, char_to_index_size> char_to_count; //< Count of child nodes by character (both upper and lower case)
#ifdef WITH_BINDING_NODES_CHARACTERISTICS
         std::unordered_map<std::string, long> characteristics; //< Characteristics of the node, e.g., size
#endif
      };

      // Tree structure representing the topology
      std::vector<Node> nodes;

      // Some internal service methods
      void parse_to_tree(const std::string &topology);
      void sort_tree_nodes(char node_type, char sort_characteristic, bool ascending = true);

   public:
      // Maximum length of a topology string
      static constexpr size_t MAX_LENGTH = 2560;
      static constexpr int NO_POS = NO_POS_DEFINE;

      // Static constants for node characters
      static const std::string DATA_NODE_CHARACTERS;
      static const std::string HARDWARE_NODE_CHARACTERS;
      static const std::string STRUCTURE_CHARACTERS;

      static std::string id_tuple2string(const std::vector<std::pair<int, int>> &ids);

      // Check if a string contains valid node characters
      static bool contains_valid_node_names(const std::string& sequence);
      static bool has_contradicting_sort_orders(const std::string& sort_criterias);
      static bool is_same_topology(const TopologyString& topo1, const TopologyString& topo2);

      static void elem_mark_nodes_as_used_or_unused(lListElem *elem, int nm, TopologyString &binding_now, const TopologyString &binding_to_use, bool mark_used);

      // Constructors
      TopologyString() = default;
      explicit TopologyString(const std::string &internal_topology);

      // String to tree and tree to string conversion
      [[nodiscard]] std::string to_string(bool with_data_nodes = false, bool with_structure = false,
                                          bool with_characteristics = false, bool with_internal_characteristics = false,
                                          bool with_tree_format = false, bool show_as_unused = false,
                                          bool show_single_threads = false, bool show_2nd_level_with_one_node = true) const;
      [[nodiscard]] std::string to_product_topology_string() const;
      [[nodiscard]] std::string to_unused_internal_topology_string() const;

      // Sorting
      void sort_tree(const std::string& sort_criteria, char sort_characteristic);

      // Service functions
      void correct_topology_upper_lower();

      bool find_first_unused_thread(int *pos, int *socket, int *core, int *thread) const;
      [[nodiscard]] int find_first_core() const;

      void mark_node_as_used_or_unused(int id, bool mark_used);
      void reset_topology(const std::string &topology);

      void mark_nodes_as_used_or_unused(const TopologyString &topo, bool mark_used);

      [[nodiscard]] std::vector<int> find_n_packed_units(unsigned bamount, BindingUnit::Unit bunit, BindingStart::Start bstart, BindingStop::Stop stop) const;
      void mark_units_as_used_or_unused(std::vector<int> &ids, BindingUnit::Unit unit, bool mark_used, bool handle_characteristics = true);
      void invert_binding();
      [[nodiscard]] std::vector<std::pair<int, int>> get_socket_and_cores_or_thread_tuples(bool collect_cores) const;
      bool is_empty() const;

      BindingUnit::Unit adapt_binding_unit(BindingUnit::Unit unit) const;
   };
} // namespace ocs