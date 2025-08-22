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

ocs::TopologyString::TopologyString() = default;

ocs::TopologyString::TopologyString(std::string  topology) : topology_(std::move(topology)) {
}

void ocs::TopologyString::remove_attributes() {
   // Remove all text between square brackets including the brackets
   std::string result;
   bool inside_square_brackets = false;
   for (char c : topology_) {
      if (c == '[') {
         inside_square_brackets = true;
         continue;
      }

      if (c == ']' && inside_square_brackets) {
         inside_square_brackets = false;
         continue;
      }

      if (!inside_square_brackets) {
         result.push_back(c);
      }
   }
   topology_ = result;
}

void ocs::TopologyString::remove_structure() {
   // Remove all bracket characters from topology_ that show the structure
   topology_.erase(std::remove_if(topology_.begin(), topology_.end(),
                  [&](char c) {
                     return STRUCTURE_CHARACTERS.find(c) != std::string::npos;
                  }),
                  topology_.end());

}

void ocs::TopologyString::remove_memory_info() {
   // Remove all bracket characters from topology_ that show the structure
   topology_.erase(std::remove_if(topology_.begin(), topology_.end(),
                  [&](char c) {
                     return DATA_NODE_CHARACTERS.find(c) != std::string::npos;
                  }),
                  topology_.end());

}

void ocs::TopologyString::remove_single_threads() {
   std::string result;
   size_t i = 0;

   while (i < topology_.length()) {
      // If current character is a thread
      if (topology_[i] == 'T' || topology_[i] == 't') {
         // Check if it's part of a sequence (has adjacent thread before or after)
         bool has_prev_thread = (i > 0 && (topology_[i - 1] == 'T' || topology_[i - 1] == 't'));
         bool has_next_thread = (i + 1 < topology_.length() && (topology_[i + 1] == 'T' || topology_[i + 1] == 't'));

         // Only keep it if it's part of a sequence
         if (has_prev_thread || has_next_thread) {
            result.push_back(topology_[i]);
         }
         // Skip it if isolated
      } else {
         // Non-thread character, keep it
         result.push_back(topology_[i]);
      }
      i++;
   }

   topology_ = result;
}

std::string
ocs::TopologyString::to_string() const {
   return topology_;
}

std::string
ocs::TopologyString::from_tree_to_string(bool with_data_nodes, bool with_structure,
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
             if (!with_data_nodes && DATA_NODE_CHARACTERS.find(node.c) != std::string::npos) {
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

void ocs::TopologyString::print(bool with_data_nodes, bool with_structure,
                                bool with_characteristics, bool with_internal_characteristics, bool with_tree_format) const {
   DENTER(TOP_LAYER);
   if (nodes.empty()) {
      return;
   }

      std::cout << from_tree_to_string(with_data_nodes, with_structure, with_characteristics, with_internal_characteristics, with_tree_format);
}

void ocs::TopologyString::parse_to_tree() {
   nodes.clear();

   if (topology_.empty()) {
      return;
   }

   // First, scan the topology to identify all unique node types
   std::set<char> all_node_types;
   for (size_t i = 0; i < topology_.size(); i++) {
      if (i > 0 && topology_[i-1] == '(' && topology_[i] != '(' && topology_[i] != ')' && topology_[i] != '[') {
         all_node_types.insert(std::tolower(topology_[i]));
      }
   }

   size_t pos = 0;
   int id_counter = 1;  // Start numbering from 1

   // Helper function to parse characteristics within square brackets
   std::function<std::unordered_map<std::string, std::string>(void)> parse_characteristics = [&]() -> std::unordered_map<std::string, std::string> {
      std::unordered_map<std::string, std::string> result;

      std::string chars_str;
      while (pos < topology_.size() && topology_[pos] != ']') {
         chars_str += topology_[pos++];
      }

      if (pos < topology_.size() && topology_[pos] == ']') {
         pos++; // Skip the closing bracket
      }

      // Parse the comma-separated list of name=value pairs
      std::istringstream iss(chars_str);
      std::string pair;
      while (std::getline(iss, pair, ',')) {
         // Trim whitespace
         pair.erase(0, pair.find_first_not_of(" \t"));
         pair.erase(pair.find_last_not_of(" \t") + 1);

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

      while (pos < topology_.size()) {
         char c = topology_[pos++];

         if (c == '(') {
            // Start of a new node
            if (pos < topology_.size()) {
               Node node;
               node.c = topology_[pos++]; // Read the node type

               // Check if the next character is '[' (start of characteristics)
               if (pos < topology_.size() && topology_[pos] == '[') {
                  pos++; // Skip the opening bracket
                  node.characteristics = parse_characteristics();
               } else {
                  node.characteristics = std::unordered_map<std::string, std::string>();
               }

               // Add the sequential ID as a characteristic
               node.characteristics[ID_PREFIX] = std::to_string(id_counter++);

               // Add the "bound" characteristic if the letter is lowercase
               bool is_bound = std::islower(node.c);
               if (is_bound) {
                  node.characteristics[BOUND_PREFIX] = "1";
               }

               node.nodes = parse_nodes(); // Parse child nodes recursively

               // Count bound children
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

               // Initialize counters for all identified node types to 0
               for (char type : all_node_types) {
                  bound_type_counts[type] = 0;
                  free_type_counts[type] = 0;
               }

               // If this node is bound/free, count it for its type
               char lowercase_type = std::tolower(node.c);
               if (is_bound) {
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
   std::function<void(std::vector<Node>&)> sort_nodes;
   sort_nodes = [&](std::vector<Node>& current_nodes) {
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
