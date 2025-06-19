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

#include "cull/cull_list.h"
#include "uti/sge_log.h"
#include "uti/sge_string.h"

/** @brief Find the first unused thread in a topology string
 *
 * This function searches for the first occurrence of an unused thread (uppercase 'T') in the
 * given topology string. It also returns the position of the character in the string
 * as well as the logical socket/core/thread ID.
 *
 * Passed topology strings can be asymmetric or symmetric, and they may contain
 * other characters representing different hardware components. Letters for threads must be present even
 * if the cores do not support hyperthreading. (e.g. "SCTSCT" instead of "SCSC").
 *
 * Numbering of logical sockets, cores, and threads starts at 0.
 *
 * If the topology string is malformed or does not contain a thread, the function
 * returns false and sets the position, socket, core, and thread to -1.
 *
 * @param topology_dstr The topology string to search in.
 * @param pos Pointer to store the position of the found thread.
 * @param socket Pointer to store the socket number of the found thread.
 * @param core Pointer to store the core number of the found thread.
 * @param thread Pointer to store the thread number of the found thread.
 * @return true if a thread was found, false otherwise.
 */
bool
ocs::HostTopology::find_first_unused_thread(const dstring *topology_dstr, int *pos, int *socket, int *core, int *thread) {
   DENTER(TOP_LAYER);
   constexpr int no_pos = -1;

   // nothing to search and find
   if (topology_dstr == nullptr || pos == nullptr || socket == nullptr || core == nullptr || thread == nullptr) {
      DRETURN(false);
   }

   const char *topology = sge_dstring_get_string(topology_dstr);
   int s = no_pos;
   int c = no_pos;
   int t = no_pos;
   for (size_t i = 0; topology[i] != '\0'; i++) {
      switch (topology[i]) {
         case 'S':
         case 's':
            s++;
            c = no_pos; // reset core when a new socket is found
            t = no_pos; // reset thread when a new socket is found
            break;
         case 'C':
         case 'c':
         case 'E':
         case 'e':
            c++;
            t = no_pos; // reset thread when a new core is found
            break;
         case 't':
            t++;
            break;
         case 'T':
            // found it
            t++;
            *pos = static_cast<int>(i);
            *socket = s;
            *core = c;
            *thread = t;

            // 't' in front of socket or core should not happen
            SGE_ASSERT(*socket != no_pos && *core != no_pos);

            DRETURN(true);
         default:
            // ignore other characters (like N for NUMA nodes or letters for caches)
            break;
      }
   }

   // no 't' found in topology string
   *pos = *socket = *core = *thread = -1;
   DRETURN(false);
}

void
ocs::HostTopology::add_or_remove_used_threads(dstring *topology_dstr, const dstring *topology_in_use_dstr, const bool do_add) {
   DENTER(TOP_LAYER);

   // strings need to have same length
   const char *topology_in_use = sge_dstring_get_string(topology_in_use_dstr);
   char *topology = sge_dstring_get_string_rw(topology_dstr);
   SGE_ASSERT(strlen(sge_dstring_get_string(topology_dstr)) == strlen(topology_in_use));

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

void
ocs::HostTopology::add_used_threads(dstring *topology_dstr, const dstring *topology_in_use_dstr) {
   add_or_remove_used_threads(topology_dstr, topology_in_use_dstr, true);
   correct_topology_upper_lower(topology_dstr);
}

void
ocs::HostTopology::add_used_threads(lListElem *elem, const int topology_nm, dstring *topology_in_use_dstr) {
   DENTER(TOP_LAYER);

   DSTRING_STATIC(topology_dstr, ocs::HostTopology::MAX_TOPOLOGY_LENGTH);
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
   DSTRING_STATIC(topology_dstr, ocs::HostTopology::MAX_TOPOLOGY_LENGTH);
   if (binding_now != nullptr) {
      sge_dstring_copy_string(&topology_dstr, binding_now);
      DSTRING_STATIC(binding_to_use_dstr, ocs::HostTopology::MAX_TOPOLOGY_LENGTH);
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

   DSTRING_STATIC(topology_dstr, ocs::HostTopology::MAX_TOPOLOGY_LENGTH);
   sge_dstring_copy_string(&topology_dstr, binding_now != nullptr ? binding_now : binding_to_use);
   DSTRING_STATIC(binding_to_use_dstr, ocs::HostTopology::MAX_TOPOLOGY_LENGTH);
   sge_dstring_copy_string(&binding_to_use_dstr, binding_to_use);
   ocs::HostTopology::remove_used_threads(&topology_dstr, &binding_to_use_dstr);
   lSetString(elem, nm, sge_dstring_get_string(&topology_dstr));
#if 0
   DPRINTF("utilization_remove_binding %s\n", sge_dstring_get_string(&topology_dstr));
#endif
   DRETURN_VOID;
}

void
ocs::HostTopology::remove_used_threads(dstring *topology, const dstring *topology_in_use) {
   add_or_remove_used_threads(topology, topology_in_use, false);
   correct_topology_upper_lower(topology);
}

void
ocs::HostTopology::add_used_thread(dstring *topology_dstr, const int pos) {
   char *topology = sge_dstring_get_string_rw(topology_dstr);
   topology[pos] = tolower(topology[pos]);
   correct_topology_upper_lower(topology_dstr);
}


void
ocs::HostTopology::remove_used_thread(dstring *topology_dstr, const int pos) {
   char *topology = sge_dstring_get_string_rw(topology_dstr);
   topology[pos] = toupper(topology[pos]);
   correct_topology_upper_lower(topology_dstr);
}

void
ocs::HostTopology::correct_topology_missing_threads(dstring *topology_dstr) {
   DENTER(TOP_LAYER);
   char new_topo[MAX_TOPOLOGY_LENGTH];

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

void
ocs::HostTopology::correct_topology_upper_lower(dstring *topology_dstr) {
   DENTER(TOP_LAYER);

   // Correct the topology string by replacing 't' with 'T'
   char *topology = sge_dstring_get_string_rw(topology_dstr);
   int threads_used = 0;
   int threads_unused = 0;
   int cores_used = 0;
   int cores_unused = 0;
   int socket_used = 0;
   int socket_unused = 0;
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
               socket_used++;
            } else {
               topology[i] = toupper(topology[i]);
               socket_unused++;
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
} /* ocs::HostTopology::correct_topology_string */

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

