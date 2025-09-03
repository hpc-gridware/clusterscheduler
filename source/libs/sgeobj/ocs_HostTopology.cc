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

#include <cstring>

#include "uti/sge_rmon_macros.h"

#include "cull/cull.h"

#include "ocs_HostTopology.h"

#include <cctype>

#include "ocs_TopologyString.h"
#include "cull/cull_list.h"
#include "uti/sge_log.h"
#include "uti/sge_string.h"

#if 0

/** @brief Add or remove used threads in a topology string
 *
 * This function modifies the topology string by either adding or removing used threads
 * based on the provided `topology_in_use_dstr`. If `do_add` is true, it adds threads,
 * otherwise it removes them.
 *
 * The function assumes that both strings have the same length and that they represent
 * the same topology structure.
 *
 * @param topology_dstr The topology string to modify.
 * @param topology_in_use_dstr The string representing the threads in use.
 * @param do_add If true, add used threads; if false, remove them.
 */
void
ocs::HostTopology::add_or_remove_used_threads(dstring *topology_dstr, const dstring *topology_in_use_dstr, const bool do_add) {
   DENTER(TOP_LAYER);

   // nothing to do
   if (topology_dstr == nullptr || topology_in_use_dstr == nullptr) {
      DRETURN_VOID;
   }

   // @todo: CS-731 why are they sometimes different in execd
   DPRINTF("topology_dstr: %s\n", sge_dstring_get_string(topology_dstr));
   DPRINTF("topology_in_use_dstr: %s\n", sge_dstring_get_string(topology_in_use_dstr));

   // strings need to have same length
   const char *topology_in_use = sge_dstring_get_string(topology_in_use_dstr);
   char *topology = sge_dstring_get_string_rw(topology_dstr);

   bool equal = strlen(sge_dstring_get_string(topology_dstr)) == strlen(topology_in_use);
   if (!equal) {
      // @todo: CS-731 why are they sometimes different in execd
      // If the lengths are not equal, we cannot proceed
      // This should not happen in a well-formed topology string
      ERROR("Topology strings have different lengths: %zu vs %zu", strlen(sge_dstring_get_string(topology_dstr)), strlen(topology_in_use));
      DRETURN_VOID;
   }

   for (size_t i = 0; topology_in_use[i] != '\0'; i++) {
      char new_thread_letter = 'T';
      if (do_add) {
         new_thread_letter = 't';
      }

      // Topology string needs to be the same (we just check for 'T' or 't') here)
      SGE_ASSERT(toupper(topology[i]) == toupper(topology_in_use[i]));

      if (topology_in_use[i] == 't') {
         topology[i] = new_thread_letter;
      }
   }
   DRETURN_VOID;
}

/** @brief Add used threads to a topology string
 *
 * This function adds the used threads from `topology_in_use_dstr` to `topology_dstr`.
 * It modifies the topology string in place and ensures that the topology string is
 * correctly formatted after the operation.
 *
 * @param topology_dstr The topology string to modify.
 * @param topology_in_use_dstr The string representing the threads in use.
 */
void
ocs::HostTopology::add_used_threads(dstring *topology_dstr, const dstring *topology_in_use_dstr) {
   add_or_remove_used_threads(topology_dstr, topology_in_use_dstr, true);
   correct_topology_upper_lower(topology_dstr);
}

/** @brief Add used threads to a topology string stored in a CULL element
 *
 * This function retrieves the topology string from the CULL element, adds the used threads
 * from `topology_in_use_dstr`, and updates the CULL element with the modified topology string.
 *
 * @param elem The CULL element containing the topology string.
 * @param topology_nm The nm of the topology string in the CULL element.
 * @param topology_in_use_dstr The string representing the threads in use.
 */
void
ocs::HostTopology::add_used_threads(lListElem *elem, const int topology_nm, dstring *topology_in_use_dstr) {
   DENTER(TOP_LAYER);

   DSTRING_STATIC(topology_dstr, ocs::TopologyString::MAX_LENGTH);
   const char *topology_str = lGetString(elem, topology_nm);
   if (topology_str != nullptr) {
      sge_dstring_copy_string(&topology_dstr, topology_str);
      add_used_threads(&topology_dstr, topology_in_use_dstr);
   } else {
      sge_dstring_copy_dstring(&topology_dstr, topology_in_use_dstr);
   }

   DRETURN_VOID;
}

/** OR's the used cores/threads of two topology masks together and stores the result in a CULL element.
 *
 * @param elem The CULL element to store the result in.
 * @param nm The CULL element's nm to store the result in.
 * @param binding_now The current binding mask, which is OR'ed with binding_to_use.
 * @param binding_to_use The binding mask to OR with binding_now.
 */
void
ocs::HostTopology::elem_add_binding(lListElem *elem, const int nm, const char *binding_now, const char *binding_to_use) {
   DENTER(TOP_LAYER);

#if 0
   DPRINTF("utilization_add_binding: binding_now=%s, binding_to_use=%s\n",
           binding_now != nullptr ? binding_now : "nullptr",
           binding_to_use != nullptr ? binding_to_use : "nullptr");
#endif
   DSTRING_STATIC(topology_dstr, ocs::TopologyString::MAX_LENGTH);
   if (binding_now != nullptr) {
      sge_dstring_copy_string(&topology_dstr, binding_now);
      DSTRING_STATIC(binding_to_use_dstr, ocs::TopologyString::MAX_LENGTH);
      sge_dstring_copy_string(&binding_to_use_dstr, binding_to_use);
      add_used_threads(&topology_dstr, &binding_to_use_dstr);
   } else {
      sge_dstring_copy_string(&topology_dstr, binding_to_use);
   }
   lSetString(elem, nm, sge_dstring_get_string(&topology_dstr));
#if 0
   DPRINTF("utilization_add_binding %s\n", sge_dstring_get_string(&topology_dstr));
#endif
   DRETURN_VOID;
}

/** @brief Removes the used cores/threads of a topology mask from topology mask and stores the result in a CULL element.
 *
 * If the binding_now is nullptr, then binding_to_use is removed from itself which is equivalent to removing
 * all used cores/threads which results in a topology string without set cores/threads.
 *
 * @param elem The CULL element to modify.
 * @param nm The CULL element's nm to modify.
 * @param binding_now The current binding mask, which is modified.
 * @param binding_to_use The binding mask to remove from binding_now.
 */
void
ocs::HostTopology::elem_remove_binding(lListElem *elem, const int nm, const char *binding_now, const char *binding_to_use) {
   DENTER(TOP_LAYER);

   DSTRING_STATIC(topology_dstr, ocs::TopologyString::MAX_LENGTH);
   sge_dstring_copy_string(&topology_dstr, binding_now != nullptr ? binding_now : binding_to_use);
   DSTRING_STATIC(binding_to_use_dstr, ocs::TopologyString::MAX_LENGTH);
   sge_dstring_copy_string(&binding_to_use_dstr, binding_to_use);
   remove_used_threads(&topology_dstr, &binding_to_use_dstr);
   lSetString(elem, nm, sge_dstring_get_string(&topology_dstr));
#if 0
   DPRINTF("utilization_remove_binding %s\n", sge_dstring_get_string(&topology_dstr));
#endif
   DRETURN_VOID;
}

/** @brief Remove used threads from a topology string
 *
 * This function modifies the topology string by removing the used threads specified in `topology_in_use_dstr`.
 * It ensures that the topology string is correctly formatted after the operation.
 *
 * @param topology The topology string to modify.
 * @param topology_in_use The string representing the threads in use.
 */
void
ocs::HostTopology::remove_used_threads(dstring *topology, const dstring *topology_in_use) {
   add_or_remove_used_threads(topology, topology_in_use, false);
   correct_topology_upper_lower(topology);
}

/** @brief Add a used thread to a topology string at a specific position
 *
 * This function modifies the topology string by adding a used thread (lowercase 't') at the specified position.
 * It also ensures that the topology string is correctly formatted after the operation.
 *
 * @param topology_dstr The topology string to modify.
 * @param pos The position where the used thread should be added.
 */
void
ocs::HostTopology::add_used_thread(dstring *topology_dstr, const int pos) {
   char *topology = sge_dstring_get_string_rw(topology_dstr);
   topology[pos] = tolower(topology[pos]);
   correct_topology_upper_lower(topology_dstr);
}

/** @brief Remove a used thread from a topology string at a specific position
 *
 * This function modifies the topology string by removing a used thread (lowercase 't') at the specified position.
 * It also ensures that the topology string is correctly formatted after the operation.
 *
 * @param topology_dstr The topology string to modify.
 * @param pos The position where the used thread should be removed.
 */
void
ocs::HostTopology::remove_used_thread(dstring *topology_dstr, const int pos) {
   char *topology = sge_dstring_get_string_rw(topology_dstr);
   topology[pos] = std::toupper(topology[pos]);
   correct_topology_upper_lower(topology_dstr);
}

/** @brief Correct the topology string by adding missing threads
 *
 * This function modifies the topology string by adding missing threads (lowercase 't' or uppercase 'T')
 * after cores (lowercase 'c', uppercase 'C', lowercase 'e', uppercase 'E') that do not have any threads.
 * It ensures that the topology string is correctly formatted after the operation.
 *
 * @param topology_dstr The topology string to modify.
 */
void
ocs::HostTopology::correct_topology_missing_threads(dstring *topology_dstr) {
   DENTER(TOP_LAYER);
   char new_topo[TopologyString::MAX_LENGTH];

   // Correct the topology string by adding missing (single) threads
   char *topology = sge_dstring_get_string_rw(topology_dstr);
   int threads = 0;
   int pos = 0;
   for (int i = static_cast<int>(strlen(topology)) - 1; i >= 0; i--, pos++) {
      switch (topology[i]) {
         case 't':
         case 'T':
            threads++;
            break;
         case 'C':
         case 'c':
         case 'E':
         case 'e':
            // No threads after upper case core, then we add a 'T' or 't' for the thread
            // depending on the case of the core letter
            if (threads == 0) {
               new_topo[pos++] = isupper(topology[i]) ? 'T' : 't';
            } else {
               threads = 0;
            }
            break;
         default:
            break;
      }
      new_topo[pos] = topology[i];
   }

   new_topo[pos] = '\0';

   // Reverse the new topology string to maintain the original order
   sge_str_reverse(new_topo);
   strcpy(topology, new_topo);

   DRETURN_VOID;
}

/** @brief Correct the topology string by adjusting the case of threads, cores, and sockets
 *
 * This function modifies the topology string by ensuring that:
 *
 * - Threads are represented as lowercase 't' if they are used, or uppercase 'T' if they are unused.
 * - Cores are represented as lowercase 'c' or 'e' if all threads on the core are used, or uppercase 'C' or 'E' if not.
 * - Sockets are represented as lowercase 's' if all cores on sockets are used, or uppercase 'S' if not.
 * - It also ensures that memory/cache letters remain uppercase.
 *
 * @param topology_dstr The topology string to modify.
 */
void
ocs::HostTopology::correct_topology_upper_lower(dstring *topology_dstr) {
   DENTER(TOP_LAYER);

   // Correct the topology string by replacing 't' with 'T'
   char *topology = sge_dstring_get_string_rw(topology_dstr);
   int threads_used = 0;
   int threads_unused = 0;
   int cores_used = 0;
   int cores_unused = 0;
   //int socket_used = 0;
   //int socket_unused = 0;
   for (int i = static_cast<int>(strlen(topology)) - 1; i >= 0; i--) {
      switch (topology[i]) {
         case 't':
            threads_used++;
            break;
         case 'T':
            threads_unused++;
            break;
         case 'C':
         case 'c':
         case 'E':
         case 'e':
            // All threads on the core are used
            if (threads_unused == 0 && threads_used > 0) {
               topology[i] = tolower(topology[i]);
               cores_used++;
            } else {
               topology[i] = toupper(topology[i]);
               cores_unused++;
            }
            threads_used = threads_unused = 0;
            break;
         case 'S':
         case 's':
            // All cores on the socket are used
            if (cores_unused == 0 && cores_used > 0) {
               topology[i] = tolower(topology[i]);
               //socket_used++;
            } else {
               topology[i] = toupper(topology[i]);
               //socket_unused++;
            }
            cores_used = cores_unused = 0;
            break;
         default:
            // If we reach here, we have a character that is not a thread, core, or socket.
            // We convert it to uppercase because it represents memory or cache
            topology[i] = toupper(topology[i]);
            break;
      }
   }

   DRETURN_VOID;
}

/** @brief Remove all used threads from a topology string
 *
 * This function modifies the topology string by converting all used threads (lowercase 't' or uppercase 'T')
 * to uppercase, effectively marking them as unused. It ensures that the topology string is correctly formatted
 * after the operation.
 *
 * @param topology_dstr The topology string to modify.
 */
void
ocs::HostTopology::remove_all_used_threads(dstring *topology_dstr) {
   DENTER(TOP_LAYER);
   char *topology = sge_dstring_get_string_rw(topology_dstr);
   for (int i = static_cast<int>(strlen(topology)) - 1; i >= 0; i--) {
      switch (topology[i]) {
         case 't':
         case 'T':
         case 'c':
         case 'C':
         case 'e':
         case 'E':
         case 's':
         case 'S':
            topology[i] = toupper(topology[i]);
            break;
         default:
            // Do not touch other characters (e.g. N for NUMA nodes or letters for caches)
            break;
      }
   }
   DRETURN_VOID;
}

#endif

