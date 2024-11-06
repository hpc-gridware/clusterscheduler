#pragma once

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

#include <string>
#include <sstream>
#include <utility>
#include <vector>

namespace ocs {
   class HostTopology {
      class Node {
      public:
         char c;
         size_t id;
         std::vector<Node> nodes;

         explicit Node(char c, std::vector<Node> nodes) : c(c), id(0), nodes(std::move(nodes)) {}
         explicit Node(char c) : c(c), id(0), nodes() {}
         ~Node() = default;
      };

      std::string topo_mask;
      std::vector<Node> sockets;

      void parseTopoMask(std::string &topo_mask);
      void buildTopoMask(const Node& node, std::ostringstream& oss) const;
      void buildFullTopoMask(const Node& node, std::ostringstream& oss) const;
      void numberTopoNodes(std::vector<Node> &nodes);
      void sortNodes(std::vector<Node>& nodes);

   public:
      HostTopology(std::string &topo_mask);

      std::string getTopoMask() const;
      std::string getFullTopoMask() const;
   };
};