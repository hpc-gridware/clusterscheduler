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
#include <functional>
#include <iostream>
#include <ranges>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <unordered_map>
#include <set>

#include "uti/sge_string.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_log.h"

#include "sgeobj/ocs_TopologyString.h"

const std::string ocs::TopologyString::DATA_NODE_CHARACTERS = "NABUVWXYZ";
const std::string ocs::TopologyString::HARDWARE_NODE_CHARACTERS = "SCEFGHT";
const std::string ocs::TopologyString::STRUCTURE_CHARACTERS = "()";
const std::string ocs::TopologyString::PREFIX = "#";
const std::string ocs::TopologyString::FREE_PREFIX = PREFIX + "f";
const std::string ocs::TopologyString::BOUND_PREFIX = PREFIX + "b";
const std::string ocs::TopologyString::ID_PREFIX = PREFIX + "i";

// e.g., "(N[size=4096](S(X[size=512](Y(C(T)(T)))(Y(C(T)(T)))(Y(E(T))(E(T))(E(T))(E(T))))))"
ocs::TopologyString::TopologyString(const std::string& topology) {
   parse_to_tree(topology);
}

bool
ocs::TopologyString::contains_valid_node_names(std::string& sequence) {
   for (char c : sequence) {
      if (DATA_NODE_CHARACTERS.find(c) == std::string::npos &&
          HARDWARE_NODE_CHARACTERS.find(c) == std::string::npos) {
         return false;
          }
   }
   return true;
}

std::string
ocs::TopologyString::to_string(bool with_data_nodes, bool with_structure,
                               bool with_characteristics, bool with_internal_characteristics, bool with_tree_format) const {
   std::ostringstream oss;

   if (with_tree_format) {
      with_structure = false;
   }

   // Helper function to convert nodes to string recursively
   std::function<void(const std::vector<Node>&, int)> nodes_to_string =
       [&](const std::vector<Node>& current_nodes, int indent) {
          bool show_data_nodes = true;

          for (const auto& node : current_nodes) {
             // Skip data nodes if requested
             if (!with_data_nodes && DATA_NODE_CHARACTERS.find(std::toupper(node.c)) != std::string::npos) {
                show_data_nodes = false;
             }

             // Print with indentation if requested
             if (with_tree_format && show_data_nodes) {
                oss << std::string(indent * 3, ' ');
             }

             if (with_structure && show_data_nodes) {
                oss << "(";
             }

             // Print the node character
             if (show_data_nodes) {
                // Print structure if requested

                oss << node.c;

                // Print characteristics if requested
                if (with_characteristics && !node.characteristics.empty()) {
                   std::ostringstream oss_characteristics;

                   bool first = true;
                   for (const auto& [name, value] : node.characteristics) {
                      // Skip internal characteristics
                      if (!with_internal_characteristics && name[0] == PREFIX[0]) {
                         continue;
                      }
                      if (!first) {
                         oss_characteristics << ",";
                      }
                      first = false;
                      oss_characteristics << name << "=" << value;
                   }
                   if (!first) {
                      oss << "[";
                      oss << oss_characteristics.str();
                      oss << "]";
                   }
                }
             }


             // Add a newline after each node
             if (with_tree_format && show_data_nodes) {
                oss << std::endl;
             }

             // Recursively process child nodes
             if (!node.nodes.empty()) {
                int delta_indent = 1;
                if (!show_data_nodes) {
                   delta_indent = 0;
                }

                nodes_to_string(node.nodes, indent + delta_indent);
             }

             // Close structure if requested
             if (with_structure && show_data_nodes) {
                oss << ")";
             }

          }
   };

   nodes_to_string(nodes, 0);
   return oss.str();
}

std::string ocs::TopologyString::to_product_topology_string() const {
#ifdef WITH_EXTENSIONS
   return to_string(true, false, false, false, false);
#else
   return to_string(false, false, false, false, false);
#endif
}

void ocs::TopologyString::parse_to_tree(const std::string& topology) {
   nodes.clear();

   if (topology.empty()) {
      return;
   }

   // First, scan the topology to identify all unique node types
   std::set<char> all_node_types;
   for (size_t i = 0; i < topology.size(); i++) {
      if (i > 0 && topology[i-1] == '(' && topology[i] != '(' && topology[i] != ')' && topology[i] != '[') {
         all_node_types.insert(std::tolower(topology[i]));
      }
   }

   size_t pos = 0;
   int id_counter = 1;  // Start numbering from 1

   // Helper function to parse characteristics within square brackets
   std::function<std::unordered_map<std::string, std::string>(void)> parse_characteristics = [&]() -> std::unordered_map<std::string, std::string> {
      std::unordered_map<std::string, std::string> result;

      std::string chars_str;
      while (pos < topology.size() && topology[pos] != ']') {
         chars_str += topology[pos++];
      }

      if (pos < topology.size() && topology[pos] == ']') {
         pos++; // Skip the closing bracket
      }

      // Parse the comma-separated list of name=value pairs
      std::istringstream iss(chars_str);
      std::string pair;
      while (std::getline(iss, pair, ',')) {
         // Trim whitespace
         pair.erase(0, pair.find_first_not_of(" \t"));
         auto last_pos = pair.find_last_not_of(" \t");
         if (last_pos != std::string::npos) {
            pair.erase(last_pos + 1);
         }

         // Find the equals sign
         size_t equals_pos = pair.find('=');
         if (equals_pos != std::string::npos) {
            std::string name = pair.substr(0, equals_pos);
            std::string value = pair.substr(equals_pos + 1);

            // Trim whitespace
            name.erase(0, name.find_first_not_of(" \t"));
            name.erase(name.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            result[name] = value;
         }
      }

      return result;
   };

// Define a recursive lambda function to parse nodes
   std::function<std::vector<Node>(void)> parse_nodes = [&]() -> std::vector<Node> {
      std::vector<Node> result;

      while (pos < topology.size()) {
         char c = topology[pos++];

         if (c == '(') {
            // Start of a new node
            if (pos < topology.size()) {
               Node node;
               node.c = topology[pos++]; // Read the node type

               // Check if the next character is '[' which indicates the start of characteristics
               if (pos < topology.size() && topology[pos] == '[') {
                  pos++; // Skip the opening bracket
                  node.characteristics = parse_characteristics();
               } else {
                  node.characteristics = std::unordered_map<std::string, std::string>();
               }

               // Add the sequential ID as a characteristic
               node.characteristics[ID_PREFIX] = std::to_string(id_counter++);

               // Parse all child nodes
               node.nodes = parse_nodes();

               // Count bound children and store as characteristic #b
               int bound_children = 0;
               for (const auto& child : node.nodes) {
                  if (std::islower(child.c)) {
                     bound_children++;
                  }
               }
               node.characteristics[BOUND_PREFIX] = std::to_string(bound_children);

               // Initialize counters for ALL node types in the tree
               std::unordered_map<char, int> bound_type_counts;
               std::unordered_map<char, int> free_type_counts;
               for (char type : all_node_types) {
                  bound_type_counts[type] = 0;
                  free_type_counts[type] = 0;
               }

               // If this node is bound/free, count it for its type
               char lowercase_type = std::tolower(node.c);
               if (std::islower(node.c)) {
                  bound_type_counts[lowercase_type] = 1;
               } else {
                  free_type_counts[lowercase_type] = 1;
               }

               // Add counts from child nodes for all node types
               for (const auto& child : node.nodes) {
                  // Process bound node counts
                  for (const auto& [key, value] : child.characteristics) {
                     static size_t bound_prefix_length = BOUND_PREFIX.size();
                     static size_t free_prefix_length = FREE_PREFIX.size();

                     if (key.size() > bound_prefix_length && key.substr(0, bound_prefix_length) == BOUND_PREFIX) {
                        char type_char = key[bound_prefix_length];
                        bound_type_counts[type_char] += std::stoi(value);
                     }
                     else if (key.size() > free_prefix_length && key.substr(0, free_prefix_length) == FREE_PREFIX) {
                        char type_char = key[free_prefix_length];
                        free_type_counts[type_char] += std::stoi(value);
                     }
                  }
               }

               // Store all accumulated bound and free type counts for ALL node types
               for (char type : all_node_types) {
                  node.characteristics[BOUND_PREFIX + std::string(1, type)] = std::to_string(bound_type_counts[type]);
                  node.characteristics[FREE_PREFIX + std::string(1, type)] = std::to_string(free_type_counts[type]);
               }

               result.push_back(node);
            }
         } else if (c == ')') {
            // End of current level
            return result;
         }
         // Ignore other characters
      }

      return result;
   };

   nodes = parse_nodes();
}

void ocs::TopologyString::sort_tree_nodes(const char node_type, const char sort_characteristic, bool ascending) {
   if (nodes.empty()) {
      return;
   }

   // Convert to lowercase for consistent comparison
   char node_type_lower = std::tolower(node_type);
   char sort_type_lower = std::tolower(sort_characteristic);

   // Create the characteristic name for sorting
   std::string sort_key = FREE_PREFIX + std::string(1, sort_type_lower);

   // Define a recursive function to sort nodes at each level
   std::function<void(std::vector<Node>&)> sort_nodes = [&](std::vector<Node>& current_nodes) {
      // Check if any child nodes match the target type
      bool has_matching_nodes = false;
      for (const auto& node : current_nodes) {
         if (std::tolower(node.c) == node_type_lower) {
            has_matching_nodes = true;
            break;
         }
      }

      // If we have nodes of the target type, sort them
      if (has_matching_nodes) {
         std::stable_sort(current_nodes.begin(), current_nodes.end(),
            [node_type_lower, sort_key, ascending](const Node& a, const Node& b) {
               // Only compare nodes of the target type
               bool a_matches = std::tolower(a.c) == node_type_lower;
               bool b_matches = std::tolower(b.c) == node_type_lower;

               if (a_matches && b_matches) {
                  // Get the values from characteristics
                  int value_a = 0;
                  int value_b = 0;

                  auto it_a = a.characteristics.find(sort_key);
                  if (it_a != a.characteristics.end()) {
                     value_a = std::stoi(it_a->second);
                  }

                  auto it_b = b.characteristics.find(sort_key);
                  if (it_b != b.characteristics.end()) {
                     value_b = std::stoi(it_b->second);
                  }

                  // Sort in ascending order by the free characteristic
                  if (ascending) {
                     return value_a < value_b;
                  } else {
                     return value_a > value_b;
                  }
               }

               // If one matches and the other doesn't, put matching nodes first
               if (a_matches != b_matches) {
                  return a_matches;
               }

               // If neither matches, maintain current order
               return false;
            });
      }

      // Recursively sort children of each node
      for (auto& node : current_nodes) {
         if (!node.nodes.empty()) {
            sort_nodes(node.nodes);
         }
      }
   };

   // Start sorting from the top level
   sort_nodes(nodes);
}

void ocs::TopologyString::sort_tree(const std::string& node_types, char sort_characteristic) {
   // If there is nothing to sort then return
   if (nodes.empty()) {
      return;
   }

   // Sort each node type in the order specified by captalisation of the characters
   // Lowercase means ascending order, which means less utilized nodes first
   for (char node_type : node_types) {
      bool ascending = std::islower(node_type);
      sort_tree_nodes(node_type, sort_characteristic, ascending);
   }
}

/** @brief Find the first unused thread in a topology string
 *
 * This function searches for the first occurrence of an unused thread (uppercase 'T') in the
 * given topology string. It also returns the position of the character in the string
 * as well as the logical socket/core/thread ID.
 *
 * Passed topology strings can be asymmetric or symmetric, and they may contain
 * other characters representing different hardware components. Letters for threads must be present even
 * if the cores do not support hyperthreading. (e.g. "SCTSCT" instead of "SCSC").
 *
 * Numbering of logical sockets, cores, and threads starts at 0.
 *
 * If the topology string is malformed or does not contain a thread, the function
 * returns false and sets the position, socket, core, and thread to -1.
 *
 * @param topology_dstr The topology string to search in.
 * @param pos Pointer to store the position of the found thread.
 * @param socket Pointer to store the socket number of the found thread.
 * @param core Pointer to store the core number of the found thread.
 * @param thread Pointer to store the thread number of the found thread.
 * @return true if a thread was found, false otherwise.
 */
bool
ocs::TopologyString::find_first_unused_thread(int *pos, int *socket, int *core, int *thread) const {
   DENTER(TOP_LAYER);

   // nothing to search and find
   if (pos == nullptr || socket == nullptr || core == nullptr || thread == nullptr) {
      DRETURN(false);
   }

   // Initialize counters
   int s = NO_POS;
   int c = NO_POS;
   int t = NO_POS;

   // Define a recursive function to traverse the tree
   std::function<bool(const std::vector<Node>&)> find_thread;
   find_thread = [&](const std::vector<Node>& current_nodes) -> bool {
      for (const auto& node : current_nodes) {
         // Check node type and update counters
         char upper_c = std::toupper(node.c);
         if (upper_c == 'S') {
            s++;
            c = NO_POS; // reset core when a new socket is found
            t = NO_POS; // reset thread when a new socket is found
         } else if (upper_c == 'C' || upper_c == 'E') {
            c++;
            t = NO_POS; // reset thread when a new core is found
         } else if (upper_c == 'T') {
            t++;
            if (node.c == 'T') { // Uppercase T means unused thread
               // Found an unused thread
               *socket = s;
               *core = c;
               *thread = t;

               // For position, use the node ID
               auto it = node.characteristics.find(ID_PREFIX);
               if (it != node.characteristics.end()) {
                  *pos = std::stoi(it->second);
               } else {
                  *pos = NO_POS;
               }

               return true; // Thread found
            }
         }

         // Continue search in child nodes
         if (!node.nodes.empty()) {
            if (find_thread(node.nodes)) {
               return true; // Thread found in child nodes
            }
         }
      }

      return false; // No thread found in this subtree
   };

   // Start the search from the root
   bool found = find_thread(nodes);

   if (!found) {
      // No thread found
      *pos = *socket = *core = *thread = -1;
   }

   DRETURN(found);
}

/** @brief Correct the topology string by adjusting the case of threads, cores, and sockets
 *
 * This function modifies the topology string by ensuring that:
 *
 * - Threads are represented as lowercase 't' if they are used, or uppercase 'T' if they are unused.
 * - Cores are represented as lowercase 'c' or 'e' if all threads on the core are used, or uppercase 'C' or 'E' if not.
 * - Sockets are represented as lowercase 's' if all cores on sockets are used, or uppercase 'S' if not.
 */
void ocs::TopologyString::correct_topology_upper_lower() {
    DENTER(TOP_LAYER);

    // Define a recursive function to update node cases
    // Returns true if all children are "used" (lowercase)
    std::function<bool(Node&)> update_node_case = [&](Node& node) -> bool {
       // We keep leaf nodes as they are (threads)
       if (node.nodes.empty()) {
          return std::islower(node.c);
       }

       // Process children first (bottom-up approach)
       bool all_children_used = true;
       for (auto& child_node : node.nodes) {
          bool child_used = update_node_case(child_node);
          all_children_used = all_children_used && child_used;
       }

       // Update current node case based on children
       if (all_children_used) {
          node.c = std::tolower(node.c);
       } else {
          node.c = std::toupper(node.c);
       }

       // Return whether all children are used
       return all_children_used;
    };

    // Process each root node
    for (auto& root_node : nodes) {
        update_node_case(root_node);
    }

    DRETURN_VOID;
}
