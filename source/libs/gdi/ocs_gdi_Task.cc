/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025-2026 HPC-Gridware GmbH
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
 * @brief One operation inside a GDI request
 */

#include <cinttypes>
#include <utility>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "gdi/ocs_gdi_Command.h"
#include "ocs_gdi_Task.h"
#include "sgeobj/sge_answer.h"

/**
 * @brief Build a task from owned parts
 *
 * A caller that keeps its own copy of a part passes a copy; ocs::gdi::Request::request() does so for
 * `do_copy`.
 *
 * @param target which object list to act on
 * @param command what to do
 * @param sub_cmd modifiers refining @p command
 * @param data the objects to send, or an empty owner
 * @param answers an answer list to start with, or an empty one
 * @param condition which objects to act on, or an empty owner
 * @param enumeration which fields to transfer, or an empty owner
 */
ocs::gdi::Task::Task(const Target target, const Command command, const SubCommand sub_cmd,
                     ocs::tom::RawListPtr data, ocs::tom::ListPtr<ocs::tom::AN> answers,
                     ocs::tom::RawWherePtr condition, ocs::tom::RawWhatPtr enumeration)
      : command(command), sub_command(sub_cmd), target(target), data_list(data.release()),
        answer_list(std::move(answers)), condition(std::move(condition)), enumeration(std::move(enumeration)),
        do_select_pack_simultaneous(false), exec_hostgroup_out_of_sync(false) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Build an empty task: no target, no command, no data
 */
ocs::gdi::Task::Task()
       : command(Command::NONE), sub_command(SubCommand::NONE), target(Target::NO_TARGET), data_list(nullptr),
         do_select_pack_simultaneous(false), exec_hostgroup_out_of_sync(false) {
   DENTER(TOP_LAYER);
   DRETURN_VOID;
}

/**
 * @brief Release the data list; the answer list, the condition and the selection free themselves
 */
ocs::gdi::Task::~Task() {
   DENTER(TOP_LAYER);
   reset_data_list();
   DRETURN_VOID;
}

/**
 * @brief Replace the data list; the one held before is freed
 *
 * Not for a #data_list that points to a master list (#do_select_pack_simultaneous): that one is set to nullptr
 * by hand, as pack_task() does.
 *
 * @param data the new data list, or an empty owner for none
 */
void
ocs::gdi::Task::reset_data_list(ocs::tom::RawListPtr data) {
   // the old list is freed when the owner taking it over leaves the scope
   const ocs::tom::RawListPtr old(std::exchange(data_list, data.release()));
}

/**
 * @brief Take the data list out of the task
 * @return the owner of the data list, empty if there was none; #data_list is nullptr afterwards
 */
ocs::tom::RawListPtr
ocs::gdi::Task::take_data_list() {
   return ocs::tom::RawListPtr(std::exchange(data_list, nullptr));
}

/**
 * @brief Log this task's command, target, data, condition and selection at debug level
 */
void
ocs::gdi::Task::debug_print() {
   DENTER(TOP_LAYER);
   DPRINTF("command = " sge_u32 "\n", static_cast<uint32_t>(command));
   DPRINTF("target = " sge_u32 "\n", static_cast<uint32_t>(target));
   DPRINTF("data_list = %p\n", data_list);
   ocs::tom::write_to(stderr, ocs::tom::ConstRawList(data_list));
   DPRINTF("answer_list = %p\n", answer_list.get().raw());
   DPRINTF("condition = %p\n", condition.get());
   DPRINTF("enumeration = %p\n", enumeration.get());
   DRETURN_VOID;
}
