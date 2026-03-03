#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
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

#include <cstring>
#include <string>
#include <ostream>

namespace ocs {
   class EscapedString {
      std::string escaped_string_;
   public:
      EscapedString(const char *string) : escaped_string_(string) {}

      friend std::ostream &operator<<(std::ostream &os, const EscapedString &es) {
         const size_t len = strlen(es.escaped_string_.c_str());
         for (size_t i = 0; i < len; i++){
            switch(es.escaped_string_[i]){
               case '<' :
                  os << "&lt;";
                  break;
               case '>' :
                  os << "&gt;";
                  break;
               case '&' :
                  os << "&amp;";
                  break;
               case '\'' :
                  os << "&apos;";
                  break;
               case '\"' :
                  os << "&quot;";
                  break;
               default :
                  os << es.escaped_string_[i];
                  break;
            }
         }
         return os;
      }
   };
}
