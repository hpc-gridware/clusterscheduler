/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024 HPC-Gridware GmbH
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

#include <ranges>
#include <string>

#include "ocs_HostTopology.h"

ocs::HostTopology::HostTopology(std::string &topo_mask) : topo_mask(topo_mask) {
   parseTopoMask(topo_mask);
   numberTopoNodes(sockets);
   sortNodes(sockets);
}

/** @brief Parses the topology mask and builds the topology tree.
 *
 * EBNF format of a well-formed topology mask:
 *    topo_mask := socket_mask+
 *    socket_mask := socket_char core_mask+
 *    core_mask := core_char thread_mask+
 *    thread_mask := thread_char
 *    socket_char := 'S' | 's'
 *    core_char := 'C' | 'c' | 'E' | 'e'
 *    thread_char := 'T' | 't'
 *
 * @param topo_mask
 */
void
ocs::HostTopology::parseTopoMask(std::string &topo_mask){
   std::vector<Node> cores;
   std::vector<Node> threads;

   // We reverse the topo_mask to simplify the parsing!
   for (char &it : std::ranges::reverse_view(topo_mask)) {
      if (it == 'S' || it == 's') {
         // put collected cores into a socket
         std::reverse(cores.begin(), cores.end());
         Node socket(it, std::move(cores));
         cores = std::vector<Node>();

         // collect all sockets
         this->sockets.push_back(std::move(socket));
      } else if (it == 'C' || it == 'c' || it == 'E' || it == 'e') {
         // put collected threads into a core
         std::reverse(threads.begin(), threads.end());
         Node core(it, std::move(threads));
         threads = std::vector<Node>();

         // collect all cores
         cores.push_back(std::move(core));
      } else if (it == 'T' || it == 't') {
         Node thread(it);

         // collect all threads
         threads.push_back(std::move(thread));
      }
   }
   std::reverse(sockets.begin(), sockets.end());

   // We did not strictly parse the EBNF syntax of the topology mask. But need to ensure that it is really well-formed.
   // Therefor, we compare the parsed topology mask with the original one.
   if (topo_mask != getTopoMask()) {
      throw std::runtime_error("Invalid topology mask");
   }
}

void
ocs::HostTopology::buildTopoMask(const Node& node, std::ostringstream& oss) const {
   oss << node.c;
   for (const auto& child : node.nodes) {
      buildTopoMask(child, oss);
   }
}

void
ocs::HostTopology::buildFullTopoMask(const Node& node, std::ostringstream& oss) const {
   oss << node.c << node.id;
   for (const auto& child : node.nodes) {
      buildFullTopoMask(child, oss);
   }
}

std::string
ocs::HostTopology::getTopoMask() const {
   std::ostringstream oss;
   for (const auto& socket : sockets) {
      buildTopoMask(socket, oss);
   }
   return oss.str();
}

std::string
ocs::HostTopology::getFullTopoMask() const {
   std::ostringstream oss;
   for (const auto& socket : sockets) {
      buildFullTopoMask(socket, oss);
   }
   return oss.str();
}

void
ocs::HostTopology::numberTopoNodes(std::vector<Node> &nodes) {
   size_t id = 0;
   for (auto &node : nodes) {
      node.id = id++;
      numberTopoNodes(node.nodes);
   }
}

void
ocs::HostTopology::sortNodes(std::vector<Node>& nodes) {
   std::sort(nodes.begin(), nodes.end(), [](const Node& a, const Node& b) {
      return a.c < b.c;
   });
   for (auto& node : nodes) {
      sortNodes(node.nodes);
   }
}
