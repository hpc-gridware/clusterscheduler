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

/** @file
 * @brief See the header (CS-2816)
 */

#include <cstdarg>

#include "uti/sge_dstring.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "basis_types.h"

#include "../msg_qconf.h"
#include "ocs_QConfHGroupWhyView.h"

/** @brief Write a catalogue message to a stream
 *
 * The message macros expand to a catalogue lookup rather than to a string
 * literal, so they cannot be streamed directly and the newline cannot be
 * concatenated onto them. The client solves this with qconf_msg(); this is the
 * same thing for the library.
 *
 * @param os  the stream to write to
 * @param fmt the message, a printf format
 */
static void
hgroup_msg(std::ostream &os, const char *fmt, ...) {
   va_list ap;
   va_start(ap, fmt);
   dstring buffer = DSTRING_INIT;
   sge_dstring_vsprintf(&buffer, fmt, ap);
   va_end(ap);
   const char *text = sge_dstring_get_string(&buffer);
   if (text != nullptr) {
      os << text;
   }
   sge_dstring_free(&buffer);
}

/** @brief `qconf -shgrp_why`: word the route the model found
 *
 * @param os  the stream to write to
 * @param why what the model decided
 */
void
ocs::QConfHGroupWhyView::show_why(std::ostream &os, const QConfModelBase::HGroupWhy &why) {
   DENTER(TOP_LAYER);

   const char *host = why.host.c_str();
   const char *in = why.found_in.empty() ? nullptr : why.found_in.c_str();

   switch (why.route) {
      case Matcher::Route::LITERAL:
         hgroup_msg(os, MSG_QCONF_WHY_LITERAL_S, host);
         break;
      case Matcher::Route::GROUP_REFERENCE:
         hgroup_msg(os, MSG_QCONF_WHY_GROUP_SS, host, in != nullptr ? in : "");
         break;
      case Matcher::Route::MATCHER:
         if (in != nullptr) {
            hgroup_msg(os, MSG_QCONF_WHY_MATCHER_GROUP_SSS, host, why.matcher.c_str(), in);
         } else {
            hgroup_msg(os, MSG_QCONF_WHY_MATCHER_SS, host, why.matcher.c_str());
         }
         break;
      case Matcher::Route::NOT_A_MEMBER:
         hgroup_msg(os, MSG_QCONF_WHY_NOT_S, host);
         break;
   }
   os << "\n";

   DRETURN_VOID;
}
