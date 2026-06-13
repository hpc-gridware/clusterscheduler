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

#include <cstddef>
#include <deque>
#include <functional>
#include <unordered_set>
#include <utility>

namespace ocs {
   /** Linked-hash FIFO: a queue that ignores duplicate pushes while preserving
    *  insertion order for the unique entries. Combines a std::deque (for fair
    *  drain in arrival order) with an std::unordered_set (for O(1) dedup on
    *  push); the two backing containers are kept in sync internally and
    *  cannot be desynced from outside.
    *
    *  Useful when a producer can mark the same key dirty many times before a
    *  consumer drains it, and the consumer wants each key processed exactly
    *  once in arrival order - for example the per-user / per-project spool
    *  fan-in in ocs::SharetreeUsage.
    *
    *  Not thread-safe. Callers serialise access through whatever lock already
    *  protects the surrounding state (LOCK_GLOBAL in the SharetreeUsage case).
    */
   template <typename T, typename Hash = std::hash<T>, typename Equal = std::equal_to<T>>
   class UniqueFifo {
   public:
      /** Enqueue value at the tail if it is not already present. A duplicate
       *  push is silently dropped. Returns true if value was newly added,
       *  false if it was already in the queue.
       */
      bool push(T value) {
         if (!seen_.insert(value).second) {
            return false;
         }
         order_.push_back(std::move(value));
         return true;
      }

      /** Remove and return the front element. Caller must ensure !empty(). */
      T pop_front() {
         T value = std::move(order_.front());
         order_.pop_front();
         seen_.erase(value);
         return value;
      }

      bool empty() const noexcept {
         return order_.empty();
      }

      std::size_t size() const noexcept {
         return order_.size();
      }

      void clear() noexcept {
         order_.clear();
         seen_.clear();
      }

   private:
      std::deque<T> order_;
      std::unordered_set<T, Hash, Equal> seen_;
   };
}
