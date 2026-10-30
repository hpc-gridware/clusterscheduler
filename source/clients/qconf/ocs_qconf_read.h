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

/** @file
 * @brief The read switches of `qconf`: show, list and export (CS-2972)
 *
 * The read stored procedure, the sub-procedure table and the export of
 * objects into files.
 */

#include <cstdint>
#include <ostream>

#include "cull/cull.h"
#include "procedure/qconf/ocs_QConfParameterClient.h"
#include "ocs_qconf_common.h"

/** @brief How a show of one object ended (CS-2796).
 *
 * The engine reports what happened and leaves the reaction to the call site,
 * which is also what CS-2799 needs: no exit call inside the engine. Since
 * CS-2950 every call site reacts the same way - the exit status is set and the
 * command line stops after the operation - and the callers that still tell
 * the two failures apart are the editor paths, for their messages. */
enum class QConfShowStatus {
   OK,            ///< object found and written
   GDI_FAILED,    ///< the GDI request was answered with an error
   NOT_FOUND      ///< the request succeeded but returned no object
};

/** @brief Run one read operation as a stored procedure (CS-2799).
 *
 * The one place that decides between the two execution contexts. With
 * ExecContext::SERVER the generic model ships the operation to qmaster and
 * renders the response it sends back; with CLIENT the typed model fetches over
 * GDI and the format specific view renders here. Both write into @p os.
 *
 * @param os    the stream the output is written to
 * @param param the invocation, with its operation already set
 * @param alpp  answer list, carries the reason when the call fails
 * @param reduction receives the reduction flags of the read, nullptr to ignore them
 * @return true when the operation was rendered
 */
bool
qconf_dispatch_procedure(std::ostream &os, ocs::QConfParameterClient &param, lList **alpp,
                         uint32_t *reduction = nullptr);

/** @brief Show one object of the given type on stdout.
 *
 * Fetches the object by name - or, for a type without a name field, the single
 * one that exists - and renders it in the layout of the descriptor.
 *
 * @param os    the stream the output is written to (CS-1841)
 * @param param the invocation, whose operation is set for the call
 * @param o    the object type
 * @param name the object name, ignored for a type without a name field
 * @param alpp the answer list of the operation, receives the reason a read failed
 * @param reduction where the reduction flags of the read are stored, nullptr to ignore them
 * @param report_errors false to leave a failed read silent, for the reduction
 *        probe of CS-2825: the operation it guards has its own message for an
 *        object that cannot be read, and two complaints for one cause read as
 *        two causes
 * @return what happened, see QConfShowStatus
 */
QConfShowStatus
qconf_show_object(std::ostream &os, ocs::QConfParameterClient &param, QConfObj o, const char *name,
                  lList **alpp, uint32_t *reduction = nullptr, bool report_errors = true);

/**
 * @brief Run one read operation as a stored procedure, if it is one
 *
 * The switches of groups A and B render inside qmaster from CS-2799 on. Which
 * ones is decided by the procedure itself, through
 * ocs::QConfModelBase::resolve() - not by a list kept here, because what a
 * caller may read is the server's question.
 *
 * A row marked export_only is an export, not a read to print: the export reads
 * through the same table (CS-2826), but its texts go to files. Printing them
 * here is what the installation's own configuration check once measured, so
 * such a row is handed to qconf_export_path() - or qconf_export_singleton() for
 * a singleton, which takes no directory - with the operation's argument as the
 * path (CS-2949). That is all the one block per "-S<obj>" switch used to do.
 *
 * @param os        the stream the output is written to
 * @param param     the invocation, whose operation is set to @p op for the call
 * @param op        the operation to run
 * @param alpp  the answer list of the operation, receives every message (CS-2971)
 * @return what became of the operation
 */
QConfOpResult
qconf_run_read_procedure(std::ostream &os, ocs::QConfParameterClient &param, const qconf_op &op,
                         lList **alpp);

/**
 * @brief Run one read operation from qconf_sub_procedure_table, if it is one
 *
 * Sends the operation with the row's sub-procedure name, clears the name again
 * - it belongs to this call, not to the invocation - and reports the answer
 * list the way the row says.
 *
 * @param os    the stream the output is written to
 * @param param the invocation, whose operation is set to @p op for the call
 * @param op    the operation to run
 * @param alpp  the answer list of the operation, receives every message (CS-2971)
 * @return what became of the operation
 */
QConfOpResult
qconf_run_sub_procedure(std::ostream &os, ocs::QConfParameterClient &param, const qconf_op &op,
                        lList **alpp);
