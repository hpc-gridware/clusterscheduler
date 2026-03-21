/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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

#include <sgeobj/ocs_EscapedString.h>

#include "ocs_QStatSelectViewXML.h"

ocs::QStatSelectViewXML::QStatSelectViewXML(QStatParameter &parameter) : QStatSelectViewBase() {
}

void
ocs::QStatSelectViewXML::report_started(std::ostream &os) {
   os << "<qselect>" << std::endl;
}

void
ocs::QStatSelectViewXML::report_finished(std::ostream &os) {
   os << "</qselect>" << std::endl;
}

void
ocs::QStatSelectViewXML::report_queue(std::ostream &os, const char* qname) {
   os << "   " << "<queue>" << EscapedString(qname) << "</queue>" << std::endl;
}

