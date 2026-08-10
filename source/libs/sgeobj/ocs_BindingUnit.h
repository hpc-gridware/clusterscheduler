#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2025 HPC-Gridware GmbH
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

/** @file
 * @brief The hardware unit a binding counts in: threads, cores, caches, NUMA nodes
 */

#include <string>

namespace ocs {
   /**
    * @brief The hardware unit a binding request counts in
    *
    * On hybrid CPUs each unit comes in two flavours: a `C…` performance
    * variant and an `E…` efficiency one. They map to the letters of the
    * topology string — `CCORE` is written `C`, `ECORE` is written `E`, and so
    * on — so a request and a host's reported topology speak the same alphabet.
    *
    * @see #is_power_unit, `ocs::Topo::CpuKind` in libuti
    */
   class BindingUnit {
      BindingUnit() = default; // prevent instantiation
   public:
      /// The hardware unit counted, in its performance and efficiency variants
      enum Unit {
         UNINITIALIZED = 0,  ///< not set; the request has not been parsed yet
         NONE,               ///< no unit; no binding requested
         CTHREAD,            ///< hardware thread of a performance core, written `T`
         ETHREAD,            ///< hardware thread of an efficiency core, written `ET`
         CCORE,              ///< performance core, written `C`
         ECORE,              ///< efficiency core, written `E`
         CSOCKET,            ///< socket holding performance cores, written `S`
         ESOCKET,            ///< socket holding efficiency cores, written `ES`
         CCACHE2,            ///< level 2 cache of performance cores, written `Y`
         ECACHE2,            ///< level 2 cache of efficiency cores, written `EY`
         CCACHE3,            ///< level 3 cache of performance cores, written `X`
         ECACHE3,            ///< level 3 cache of efficiency cores, written `EX`
         CNUMA,              ///< NUMA node of performance cores, written `N`
         ENUMA,              ///< NUMA node of efficiency cores, written `EN`
      };

      static std::string to_string(Unit mode);
      static Unit from_string(const std::string& mode);
      static bool is_power_unit(Unit unit);
   };
}
