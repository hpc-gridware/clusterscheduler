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
 * @brief The wire format of a GDI task: the bytes ocs::gdi::Packet writes and reads
 *
 * The task is packed by Packet::pack_task() and compared byte for byte with the
 * bytes the primitive functions write for the same parts in the order of the
 * protocol: three integers, cull_pack_list() of the data list and of the answer
 * list, cull_pack_cond(), cull_pack_enum() and the has-next integer. A GET of an
 * external client selects straight into the buffer; its bytes are compared with
 * lSelectHashPack() into a buffer. Packet::unpack() reads them back, and packing
 * what it read gives the same bytes again.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <utility>

#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

#include "cull/cull_test_api.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_Raw.h"

#include "gdi/ocs_gdi_Packet.h"
#include "gdi/ocs_gdi_Task.h"

static int s_fail = 0;

/** @def CHECK
 * @brief Assert one condition and record the result
 * @param id the test number, printed as `[Tnn]`
 * @param label what the check is about
 * @param expr the condition that must hold
 */
#define CHECK(id, label, expr) \
   do { \
      if (!(expr)) { \
         printf("FAIL  [T%02d] %s\n", (id), (label)); \
         ++s_fail; \
      } else { \
         printf("ok    [T%02d] %s\n", (id), (label)); \
      } \
   } while (0)

/**
 * @brief Whether two pack buffers hold the same bytes
 * @param a the first buffer
 * @param b the second buffer
 * @return true if both have the same number of bytes and the same bytes
 */
static bool
same_bytes(const sge_pack_buffer *a, const sge_pack_buffer *b) {
   return a->bytes_used == b->bytes_used && memcmp(a->head_ptr, b->head_ptr, a->bytes_used) == 0;
}

/**
 * @brief A list of ST with three names
 * @return the owner of the list
 */
static ocs::tom::RawListPtr
make_data() {
   auto list = ocs::tom::ListPtr<ocs::tom::ST>::create("test data");
   list->emplace_back<ST_name>("alpha");
   list->emplace_back<ST_name>("beta");
   list->emplace_back<ST_name>("gamma");
   return ocs::tom::RawListPtr(std::move(list));
}

/**
 * @brief A task with data, an answer, a condition and a selection
 * @return the task, owned by the caller
 */
static ocs::gdi::Task *
make_full_task() {
   ocs::tom::ListPtr<ocs::tom::AN> answers;
   answer_list_add(&answers, "first answer", STATUS_OK, ANSWER_QUALITY_INFO);
   return new ocs::gdi::Task(ocs::gdi::Target::US_LIST, ocs::gdi::Command::MOD,
                             ocs::gdi::SubCommand::RETURN_NEW_VERSION, make_data(), std::move(answers),
                             ocs::tom::RawWherePtr(lWhere("%T(%I==%s)", ocs::tom::ST::descriptor.e, ST_name, "beta")),
                             ocs::tom::RawWhatPtr(lWhat("%T(%I)", ocs::tom::ST::descriptor.e, ST_name)));
}

/**
 * @brief Pack a task part by part with the primitive functions, in the order of the protocol
 * @param pb the buffer to append to
 * @param task the task
 * @param has_next whether another task follows
 * @return true if every primitive succeeded
 */
static bool
reference_pack_task(sge_pack_buffer *pb, const ocs::gdi::Task *task, const bool has_next) {
   return packint(pb, static_cast<uint32_t>(task->command)) == PACK_SUCCESS &&
          packint(pb, static_cast<uint32_t>(task->sub_command)) == PACK_SUCCESS &&
          packint(pb, static_cast<uint32_t>(task->target)) == PACK_SUCCESS &&
          cull_pack_list(pb, task->data_list) == PACK_SUCCESS &&
          cull_pack_list(pb, task->answer_list.get().raw()) == PACK_SUCCESS &&
          cull_pack_cond(pb, task->condition.get()) == PACK_SUCCESS &&
          cull_pack_enum(pb, task->enumeration.get()) == PACK_SUCCESS &&
          packint(pb, has_next ? 1 : 0) == PACK_SUCCESS;
}

/** @brief A full task and an empty one: Packet::pack_task() writes the bytes of the primitives [T01–T02] */
static void
test_pack_task_bytes() {
   printf("\n--- pack_task against the primitives ---\n");

   ocs::gdi::Packet packet;
   packet.append_task(make_full_task());
   packet.append_task(new ocs::gdi::Task());

   sge_pack_buffer pb;
   sge_pack_buffer ref;
   init_packbuffer(&pb, 0, false, false);
   init_packbuffer(&ref, 0, false, false);

   ocs::tom::ListPtr<ocs::tom::AN> alp;
   const bool packed = packet.pack_task(packet.tasks[0], &alp, &pb, true);
   const bool ref_packed = reference_pack_task(&ref, packet.tasks[0], true);
   // T01: data list, answer list, condition and selection
   CHECK(1, "full task: the same bytes as the primitives", packed && ref_packed && same_bytes(&pb, &ref));

   const bool packed_empty = packet.pack_task(packet.tasks[1], &alp, &pb, false);
   const bool ref_packed_empty = reference_pack_task(&ref, packet.tasks[1], false);
   // T02: no data list, no answer list, no condition, no selection
   CHECK(2, "empty task after it: the same bytes as the primitives",
         packed_empty && ref_packed_empty && same_bytes(&pb, &ref));

   clear_packbuffer(&pb);
   clear_packbuffer(&ref);
}

/** @brief A GET of an external client selects from the master list straight into the buffer [T03–T05] */
static void
test_select_pack_bytes() {
   printf("\n--- select straight into the buffer ---\n");

   // the list the task borrows, as qmaster lends it the master list
   ocs::tom::RawListPtr source = make_data();
   ocs::tom::RawWherePtr where(lWhere("%T(%I==%s)", ocs::tom::ST::descriptor.e, ST_name, "beta"));
   ocs::tom::RawWhatPtr what(lWhat("%T(%I)", ocs::tom::ST::descriptor.e, ST_name));

   ocs::gdi::Packet packet;
   auto *task = new ocs::gdi::Task(ocs::gdi::Target::US_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE,
                                   {}, {}, ocs::tom::clone(where.get()), ocs::tom::clone(what.get()));
   task->data_list = source.get();
   task->do_select_pack_simultaneous = true;
   packet.append_task(task);

   sge_pack_buffer pb;
   sge_pack_buffer ref;
   init_packbuffer(&pb, 0, false, false);
   init_packbuffer(&ref, 0, false, false);

   ocs::tom::ListPtr<ocs::tom::AN> alp;
   const bool packed = packet.pack_task(task, &alp, &pb, false);

   // T03: the borrowed list is given back, the condition and the selection are spent
   CHECK(3, "the borrowed list is no longer held, condition and selection are freed",
         task->data_list == nullptr && !task->condition && !task->enumeration);

   // the reference: the selection packed by lSelectHashPack(), then the answer list pack_task() completed
   bool ref_packed = packint(&ref, static_cast<uint32_t>(task->command)) == PACK_SUCCESS &&
                     packint(&ref, static_cast<uint32_t>(task->sub_command)) == PACK_SUCCESS &&
                     packint(&ref, static_cast<uint32_t>(task->target)) == PACK_SUCCESS;
   lSelectHashPack("", source.get(), where.get(), what.get(), false, &ref);
   ref_packed = ref_packed && cull_pack_list(&ref, task->answer_list.get().raw()) == PACK_SUCCESS &&
                cull_pack_cond(&ref, nullptr) == PACK_SUCCESS && cull_pack_enum(&ref, nullptr) == PACK_SUCCESS &&
                packint(&ref, 0) == PACK_SUCCESS;

   // T04: the bytes of a select into the buffer
   CHECK(4, "selected task: the same bytes as lSelectHashPack() and the primitives",
         packed && ref_packed && same_bytes(&pb, &ref));

   // T05: the source list is untouched
   CHECK(5, "the borrowed list keeps its three elements", ocs::tom::size(source.get()) == 3);

   clear_packbuffer(&pb);
   clear_packbuffer(&ref);
}

/**
 * @brief A read buffer over a copy of the first bytes of a written one
 * @param from the written buffer
 * @param size how many of its bytes
 * @param to the buffer to initialise; owns the copy afterwards
 * @return true on success
 */
static bool
read_buffer_of(const sge_pack_buffer *from, const size_t size, sge_pack_buffer *to) {
   auto *bytes = static_cast<char *>(sge_malloc(size));
   memcpy(bytes, from->head_ptr, size);
   return init_packbuffer_from_buffer(to, bytes, static_cast<uint32_t>(size), false) == PACK_SUCCESS;
}

/** @brief Packet::unpack() reads a packet back, and packing it again gives the same bytes [T06–T08] */
static void
test_unpack_roundtrip() {
   printf("\n--- unpack and pack again ---\n");

   ocs::gdi::Packet packet;
   packet.append_task(make_full_task());
   packet.append_task(new ocs::gdi::Task());
   packet.append_task(make_full_task());

   sge_pack_buffer pb;
   init_packbuffer(&pb, 0, false, false);
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   const bool packed = packet.pack(&alp, &pb);

   sge_pack_buffer in;
   ocs::gdi::Packet back;
   const bool in_ok = packed && read_buffer_of(&pb, pb.bytes_used, &in);
   const bool read = in_ok && back.unpack(&alp, &in);
   if (in_ok) {
      clear_packbuffer(&in);
   }

   // T06: three tasks with their parts
   CHECK(6, "three tasks read back with data, condition and selection",
         read && back.tasks.size() == 3 && ocs::tom::size(back.tasks[0]->data_list) == 3 &&
         back.tasks[0]->condition && back.tasks[0]->enumeration && back.tasks[1]->data_list == nullptr &&
         !back.tasks[1]->condition && ocs::tom::size(back.tasks[2]->answer_list.get().raw()) == 1);

   sge_pack_buffer again;
   init_packbuffer(&again, 0, false, false);
   const bool repacked = read && back.pack(&alp, &again);

   // T07: the same bytes again
   CHECK(7, "packing what was read gives the same bytes", repacked && same_bytes(&pb, &again));

   // T08: a packet cut off inside the selection of the last task is refused; the task read in part is freed
   sge_pack_buffer cut;
   ocs::gdi::Packet partial;
   ocs::tom::ListPtr<ocs::tom::AN> cut_alp;
   const bool cut_ok = packed && read_buffer_of(&pb, pb.bytes_used - 8, &cut);
   const bool cut_read = cut_ok && partial.unpack(&cut_alp, &cut);
   if (cut_ok) {
      clear_packbuffer(&cut);
   }
   CHECK(8, "a cut-off packet is refused with an answer", cut_ok && !cut_read && cut_alp);

   clear_packbuffer(&pb);
   clear_packbuffer(&again);
}

/**
 * @brief Run the checks of this module test
 * @return 0 if every check passed, 1 otherwise
 */
int
main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_gdi_task_pack");

   // suppress sge_log stderr output emitted by functions under test
   component_set_daemonized(true);

   ocs::tom::install_model(ocs::tom::gcs_model());

   test_pack_task_bytes();
   test_select_pack_bytes();
   test_unpack_roundtrip();

   printf("\n%s: %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
