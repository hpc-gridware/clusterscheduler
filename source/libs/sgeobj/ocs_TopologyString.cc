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
#include <iostream>
#include <ranges>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "uti/sge_string.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/ocs_TopologyString.h"

ocs::TopologyString::TopologyString() = default;

ocs::TopologyString::TopologyString(std::string  topology) : topology_(std::move(topology)) {
   correct();
}

void
ocs::TopologyString::initialize_logical_numbering() {
   int topology_id = 0;
   int socket_num = 0;

   for (auto& socket : sockets_) {
      socket.assign_prop_if_not_exists(TopologyNode::TOPOLOGY_ID, topology_id++);
      socket.assign_prop_if_not_exists(TopologyNode::LOGICAL_SOCKET, socket_num);

      int core_num = 0;
      for (auto& core : socket.nodes) {
         core.assign_prop_if_not_exists(TopologyNode::TOPOLOGY_ID, topology_id++);
         core.assign_prop_if_not_exists(TopologyNode::LOGICAL_SOCKET, socket_num);
         core.assign_prop_if_not_exists(TopologyNode::LOGICAL_CORE, core_num);

         int thread_num = 0;
         for (auto& thread : core.nodes) {
            thread.assign_prop_if_not_exists(TopologyNode::TOPOLOGY_ID, topology_id++);
            thread.assign_prop_if_not_exists(TopologyNode::LOGICAL_SOCKET, socket_num);
            thread.assign_prop_if_not_exists(TopologyNode::LOGICAL_CORE, core_num);
            thread.assign_prop_if_not_exists(TopologyNode::LOGICAL_THREAD, thread_num);

            thread_num++;
         }
         core_num++;
      }
      socket_num++;
   }
}


void
ocs::TopologyString::parse() {
   DENTER(TOP_LAYER);
   std::vector<TopologyNode> cores;
   std::vector<TopologyNode> threads;
   TopologyNode::Status core_status = TopologyNode::Status::UNINITIALIZED;
   TopologyNode::Status socket_status = TopologyNode::Status::UNINITIALIZED;
   int threads_free = 0;
   int threads_used = 0;
   int c_threads_on_socket_free = 0;
   int c_threads_on_socket_used = 0;
   int e_threads_on_socket_free = 0;
   int e_threads_on_socket_used = 0;

   std::unordered_map<std::string, int> properties;

   // We reverse the topo_mask to simplify the parsing!
   for (char &it : std::ranges::reverse_view(topology_)) {
      switch (it) {
         case 'T':
         case 't': {
            bool is_free = (it == 'T');

            if (is_free) {
               threads_free++;
            } else {
               threads_used++;
            }

            TopologyNode thread(it, is_free ? TopologyNode::Status::FREE : TopologyNode::Status::UNAVAILABLE);
            threads.push_back(std::move(thread));
            core_status = TopologyNode::get_combined_status(core_status, is_free);
            break;
         }
         case 'C':
         case 'c':
         case 'E':
         case 'e': {
            bool is_power_core = (it == 'C' || it == 'c');
            bool is_free = (it == 'C' || it == 'E');

            // If we have threads, reverse them to maintain the order
            std::ranges::reverse(threads);

            // Create a core node with the found threads
            TopologyNode core(it, core_status, std::move(threads));

            // Assign properties for the number of free and used threads on the core
            core.assign_prop_if_not_exists(is_power_core ? TopologyNode::NUMBER_OF_FREE_C_THREADS : TopologyNode::NUMBER_OF_FREE_E_THREADS, threads_free);
            core.assign_prop_if_not_exists(is_power_core ? TopologyNode::NUMBER_OF_USED_C_THREADS: TopologyNode::NUMBER_OF_USED_E_THREADS, threads_used);

            // Add threads to the core counters and reset the thread counters
            if (is_power_core) {
               c_threads_on_socket_free += threads_free;
               c_threads_on_socket_used += threads_used;
            } else {
               e_threads_on_socket_free += threads_free;
               e_threads_on_socket_used += threads_used;
            }
            threads_free = 0;
            threads_used = 0;

            // Reset the threads vector for the next core
            threads = std::vector<TopologyNode>();
            core_status = TopologyNode::Status::UNINITIALIZED;

            // Add the core to the cores vector
            cores.push_back(std::move(core));

            // Update the socket status based on the core status for cores found so far
            socket_status = TopologyNode::get_combined_status(socket_status, is_free);
            break;
         }
         case 'S':
         case 's': {
            // Reverse the cores vector to maintain the order of cores
            std::ranges::reverse(cores);

            // Create a socket node with the found cores
            TopologyNode socket(it, socket_status, std::move(cores));

            // Assign properties for the number of free and used threads on the socket
            socket.assign_prop_if_not_exists(TopologyNode::NUMBER_OF_FREE_C_THREADS, c_threads_on_socket_free);
            socket.assign_prop_if_not_exists(TopologyNode::NUMBER_OF_FREE_E_THREADS, e_threads_on_socket_free);
            socket.assign_prop_if_not_exists(TopologyNode::NUMBER_OF_USED_C_THREADS, c_threads_on_socket_used);
            socket.assign_prop_if_not_exists(TopologyNode::NUMBER_OF_USED_E_THREADS, e_threads_on_socket_used);
            c_threads_on_socket_free = 0;
            c_threads_on_socket_used = 0;
            e_threads_on_socket_free = 0;
            e_threads_on_socket_used = 0;

            // Reset the cores and threads vectors for the next socket
            cores = std::vector<TopologyNode>();

            // Add the socket to the list of sockets
            this->sockets_.push_back(std::move(socket));

            // Reset the core status for the next socket
            socket_status = TopologyNode::Status::UNINITIALIZED;
            break;
         }
         case 'N':
         case 'X':
         case 'Y': {
            std::string key = std::string(1, it);
            if (!properties.contains(key)) {
               properties[key] = std::ranges::count(topology_, it) - 1;
            } else {
               properties[key]--;
            }
            if (it == 'N' || it == 'X') {
               assign_prop_if_not_exists(sockets_, key, properties[key]);
            }
            assign_prop_if_not_exists(cores, key, properties[key]);
            assign_prop_if_not_exists(threads, key, properties[key]);
            break;
         }
         default:
            break;
      }
   }
   std::ranges::reverse(sockets_);
   initialize_logical_numbering();

   // @todo: enable this when fully implemented
#if 0
   // We did not strictly parse the EBNF syntax of the topology mask. But need to ensure that it is really well-formed.
   // Therefor, we compare the reconstructed topology mask from the tree with the original one.
   if (topology_ != from_tree_to_string()) {
      throw std::runtime_error("Invalid topology mask");
   }
#endif

   DRETURN_VOID;
}

void ocs::TopologyString::sort_by_characteristic(const std::string& characteristic, bool ascending) {
    // Sort comparator using the specified characteristic
    auto comparator = [&characteristic, ascending](const TopologyNode& a, const TopologyNode& b) {
        // Get property values with 0 as default if not found
        auto a_it = a.properties.find(characteristic);
        auto b_it = b.properties.find(characteristic);

        int value_a = (a_it != a.properties.end()) ? a_it->second : 0;
        int value_b = (b_it != b.properties.end()) ? b_it->second : 0;

        return ascending ? value_a < value_b : value_a > value_b;
    };

    // Sort sockets
    std::sort(sockets_.begin(), sockets_.end(), comparator);

    // Sort cores within each socket
    for (auto& socket : sockets_) {
        std::sort(socket.nodes.begin(), socket.nodes.end(), comparator);

        // Sort threads within each core
        for (auto& core : socket.nodes) {
            std::sort(core.nodes.begin(), core.nodes.end(), comparator);
        }
    }

    // Renumber logical IDs after sorting
    initialize_logical_numbering();
}

void
ocs::TopologyString::assign_prop_if_not_exists(std::vector<TopologyNode> &nodes, const std::string &name, const int value) {
   for (auto &node : nodes) {
      node.assign_prop_if_not_exists(name, value);

      if (!node.nodes.empty()) {
         assign_prop_if_not_exists(node.nodes, name, value);
      }
   }
}

void ocs::TopologyString::print(const std::vector<TopologyNode> &nodes, const int indent) {
   DENTER(TOP_LAYER);
   for (const auto& node : nodes) {
      std::cout << std::string(indent * 2, ' ') << node.c << " ";

      std::cout << std::string((3 - indent) * 2, ' ') << "[";
      bool first_sproperty = true;
      for (const auto & property : node.properties) {
         if (first_sproperty) {
            first_sproperty = false;
         } else {
            std::cout << ", ";
         }
         std::cout << property.first << "=" << property.second;
      }
      std::cout << "]" << std::endl;

      if (!node.nodes.empty()) {
         print(node.nodes, indent + 1);
      }
   }

   DRETURN_VOID;
}

void ocs::TopologyString::print() const {
   print(sockets_, 0);
}


std::string
ocs::TopologyString::from_tree_to_string() const {
   std::ostringstream oss;
   for (const auto& socket : sockets_) {
      socket.to_string(oss);
   }
   return oss.str();
}

// Utility methods
void
ocs::TopologyString::correct_missing_threads() {
    DENTER(TOP_LAYER);
    char new_topo[MAX_LENGTH];

    // Correct the topology string by adding missing (single) threads
    const char *topology = topology_.c_str();
    int threads = 0;
    int pos = 0;
    for (int i = static_cast<int>(strlen(topology)) - 1; i >= 0; i--, pos++) {
        switch (topology[i]) {
            case 't':
            case 'T':
                threads++;
                break;
            case 'C':
            case 'c':
            case 'E':
            case 'e':
                // No threads after upper case core, then we add a 'T' or 't' for the thread
                // depending on the case of the core letter
                if (threads == 0) {
                    new_topo[pos++] = isupper(topology[i]) ? 'T' : 't';
                } else {
                    threads = 0;
                }
                break;
            default:
                break;
        }
        new_topo[pos] = topology[i];
    }

    new_topo[pos] = '\0';

    // Reverse the new topology string to maintain the original order
    sge_str_reverse(new_topo);
    topology_ = new_topo;

    DRETURN_VOID;
}

void
ocs::TopologyString::correct_upper_lower() {
    DENTER(TOP_LAYER);

    // Correct the topology string by replacing 't' with 'T'
    char *topology = topology_.data();
    int threads_used = 0;
    int threads_unused = 0;
    int cores_used = 0;
    int cores_unused = 0;
    int socket_used = 0;
    int socket_unused = 0;
    for (int i = static_cast<int>(strlen(topology)) - 1; i >= 0; i--) {
        switch (topology[i]) {
            case 't':
                threads_used++;
                break;
            case 'T':
                threads_unused++;
                break;
            case 'C':
            case 'c':
            case 'E':
            case 'e':
                // All threads on the core are used
                if (threads_unused == 0 && threads_used > 0) {
                    topology[i] = std::tolower(topology[i]);
                    cores_used++;
                } else {
                    topology[i] = std::toupper(topology[i]);
                    cores_unused++;
                }
                threads_used = threads_unused = 0;
                break;
            case 'S':
            case 's':
                // All cores on the socket are used
                if (cores_unused == 0 && cores_used > 0) {
                    topology[i] = std::tolower(topology[i]);
                    socket_used++;
                } else {
                    topology[i] = std::toupper(topology[i]);
                    socket_unused++;
                }
                cores_used = cores_unused = 0;
                break;
            default:
                // If we reach here, we have a character that is not a thread, core, or socket.
                // We convert it to uppercase because it represents memory or cache
                topology[i] = std::toupper(topology[i]);
                break;
        }
    }

    DRETURN_VOID;
}

void
ocs::TopologyString::correct() {
    correct_missing_threads();
    correct_upper_lower();
}

// Accessors
std::string
ocs::TopologyString::to_string() const {
   return topology_;
}

std::string
ocs::TopologyString::get_packed_binding(int units, std::string &unit, std::string &strategy, std::string &filter, std::string &sort, std::string &start, std::string &stop) {
   DENTER(TOP_LAYER);


   // Should we do thread
   bool do_thread_binding = (unit == "CT" || unit == "ET");

   // Should we find power units (C or CT) or efficiency units (E or ET)
   bool do_find_power_unit = (unit == "CT" || unit == "C");

   // Start binding at completely empty core unless requested otherwise
   bool start_on_empty_core = true;
   if (unit == "CT" && start == "c") {
      start_on_empty_core = false;
   }

   // Start within used sockets unless requested otherwise
   bool start_on_empty_socket = false;
   if (start == "S") {
      start_on_empty_socket = true;
   }

   // Remaining units we are looking for
   int remaining_units = units;

   std::string binding;
   for (auto &socket : sockets_) {
      bool is_socket_empty = socket.properties.at(TopologyNode::NUMBER_OF_USED_C_THREADS) == 0 &&
                             socket.properties.at(TopologyNode::NUMBER_OF_USED_E_THREADS) == 0;

      // skip this socket if we want to start on empty sockets
      if (start_on_empty_socket && !is_socket_empty) {
         continue;
      }

      for (auto &core : socket.nodes) {
         bool is_core_empty = false;

         bool is_power_core = (core.c == 'C' || core.c == 'c');

         // If we are looking for power cores, skip efficiency cores
         // If we are looking for efficiency cores, skip power cores
         if ((do_find_power_unit && !is_power_core) || (!do_find_power_unit && is_power_core)) {
            continue;
         }

         if (is_power_core) {
            is_core_empty = core.properties.at(TopologyNode::NUMBER_OF_USED_C_THREADS) == 0;
         } else if (core.c == 'E' || core.c == 'e') {
            is_core_empty = core.properties.at(TopologyNode::NUMBER_OF_USED_E_THREADS) == 0;
         }

         // skip this core if we want to start on empty cores
         if (start_on_empty_core && !is_core_empty) {
            continue;
         }

         bool found_thread = false;
         for (auto &thread : core.nodes) {
            auto thread_status = static_cast<TopologyNode::Status>(thread.properties.at(TopologyNode::STATUS));

            // Skip the thread if it is not free
            if (thread_status != TopologyNode::Status::FREE) {
               continue;
            }

            // If we are here, we have a free thread, and we can add it to the binding
            found_thread = true;
            if (do_thread_binding) {
               remaining_units--;
            }

            // Early exit if we found enough units
            if (remaining_units == 0) {
               break;
            }
         }

         // If we found a thread, we can add the core to the binding
         if (found_thread && !do_thread_binding) {
            remaining_units--;
         }

         if (remaining_units == 0) {
            break;
         }
      }

      if (remaining_units == 0) {
         break;
      }
   }

   // Not enough units found, return empty binding
   if (remaining_units > 0) {
      binding.clear();
   }

   DRETURN(binding);
}