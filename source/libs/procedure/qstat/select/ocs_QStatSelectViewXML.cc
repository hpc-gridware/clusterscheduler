/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2026 HPC-Gridware GmbH
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

#include "uti/sge_rmon_macros.h"

#include <sgeobj/ocs_EscapedString.h>

#include "qstat/select/ocs_QStatSelectViewXML.h"

ocs::QStatSelectViewXML::QStatSelectViewXML(const QStatParameter &parameter) : QStatSelectViewBase(parameter) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

void
ocs::QStatSelectViewXML::report_started(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "<qselect>\n";
   DRETURN_VOID;
}

void
ocs::QStatSelectViewXML::report_finished(std::ostream &os) {
   DENTER(TOP_LAYER);
   os << "</qselect>\n";
   DRETURN_VOID;
}

void
ocs::QStatSelectViewXML::report_queue(std::ostream &os, const char* qname) {
   DENTER(TOP_LAYER);
   os << "   " << "<queue>" << EscapedString(qname) << "</queue>\n";
   DRETURN_VOID;
}

