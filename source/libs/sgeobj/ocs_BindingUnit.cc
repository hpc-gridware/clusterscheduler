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

#include "ocs_BindingUnit.h"

/** @brief Returns the sort string form of a binding unit type
 *
 * @param mode A binding unit type
 */
std::string ocs::BindingUnit::to_string(const Unit mode) {
   switch (mode) {
      case NONE: return "NONE";
      case CTHREAD: return "T";
      case ETHREAD: return "ET";
      case CCORE: return "C";
      case ECORE: return "E";
      case CSOCKET: return "S";
      case ESOCKET: return  "ES";
      case CCACHE2: return "Y";
      case ECACHE2: return "EY";
      case CCACHE3: return "X";
      case ECACHE3: return "EX";
      case CNUMA: return "N";
      case ENUMA: return "EN";
      default: return "???";
   }
}

/** @brief Converts a binding unit string to its corresponding constant
 *
 * Units strings with an E represent a unit attached to an efficiency core. Units with the C prefix denote power units.
 * Single letter units automatically refer to power units.
 *
 * @param mode A Binding unit string.
 */
ocs::BindingUnit::Unit
ocs::BindingUnit::from_string(const std::string& mode) {
   if (mode == "NONE") {
      return NONE;
   } else if (mode == "T" || mode == "CT") {
      return CTHREAD;
   } else if (mode == "ET") {
      return ETHREAD;
   } else if (mode == "C" || mode == "CC") {
      return CCORE;
   } else if (mode == "E" || mode == "EC") {
      return ECORE;
   } else if (mode == "S" || mode == "CS") {
      return CSOCKET;
   } else if (mode == "ES") {
      return ESOCKET;
   } else if (mode == "Y" || mode == "CY") {
      return CCACHE2;
   } else if (mode == "EY") {
      return ECACHE2;
   } else if (mode == "X" || mode == "CX") {
      return CCACHE3;
   } else if (mode == "EX") {
      return ECACHE3;
   } else if (mode == "N" || mode == "CN") {
      return CNUMA;
   } else if (mode == "EN") {
      return ENUMA;
   } else {
      return UNINITIALIZED;
   }
}

/** @brief Returns true if the given unit is a power unit
 *
 * Intel differentiates between power and efficiency core. If the specified `unit` comprises a
 * power core (e.g., also memory units that are connected to power-cores), then this method
 * returns true.
 *
 * @param unit Enum value representing a binding unit.
 */
bool ocs::BindingUnit::is_power_unit(const Unit unit) {
   if (unit == CSOCKET || unit == CCORE || unit == CTHREAD || unit == CCACHE2 || unit == CCACHE3 || unit == CNUMA) {
      return true;
   }
   return false;
}
