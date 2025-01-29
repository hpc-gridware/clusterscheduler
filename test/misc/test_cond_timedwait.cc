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

#include <iostream>
#include <chrono>

#include <time.h>
#include <pthread.h>

long get_milli() {
   const auto now = std::chrono::system_clock::now();
   const auto epoch = now.time_since_epoch();
   const auto us = duration_cast<std::chrono::milliseconds>(epoch);
   return us.count();
}

int main(int argc, const char *argv[]) {
#if defined(SOLARIS)
   const int num_clocks = 3;
   int clock_ids[num_clocks] = {CLOCK_VIRTUAL, CLOCK_REALTIME, CLOCK_MONOTONIC};
   std::string clock_names[num_clocks] = {"CLOCK_VIRTUAL", "CLOCK_REALTIME", "CLOCK_MONOTONIC"};
#elif defined(FREEBSD)
   const int num_clocks = 4;
   int clock_ids[num_clocks] = {CLOCK_REALTIME, CLOCK_MONOTONIC, CLOCK_MONOTONIC_PRECISE, CLOCK_MONOTONIC_COARSE};
   std::string clock_names[num_clocks] = {"CLOCK_REALTIME", "CLOCK_MONOTONIC", "CLOCK_MONOTONIC_PRECISE", "CLOCK_MONOTONIC_COARSE"};
#else
   // LINUX
   // we don't have CLOCK_TAI on older Linuxes
#if defined CLOCK_TAI
   const int num_clocks = 5;
   int clock_ids[num_clocks] = {CLOCK_REALTIME, CLOCK_MONOTONIC, CLOCK_MONOTONIC_RAW, CLOCK_MONOTONIC_COARSE, CLOCK_TAI};
   std::string clock_names[num_clocks] = {"CLOCK_REALTIME", "CLOCK_MONOTONIC", "CLOCK_MONOTONIC_RAW", "CLOCK_MONOTONIC_COARSE", "CLOCK_TAI"};
#else
   const int num_clocks = 4;
   int clock_ids[num_clocks] = {CLOCK_REALTIME, CLOCK_MONOTONIC, CLOCK_MONOTONIC_RAW, CLOCK_MONOTONIC_COARSE};
   std::string clock_names[num_clocks] = {"CLOCK_REALTIME", "CLOCK_MONOTONIC", "CLOCK_MONOTONIC_RAW", "CLOCK_MONOTONIC_COARSE"};
#endif
#endif

    for (int i = 0; i < num_clocks; i++) {
        struct timespec ts;
        if (clock_getres(clock_ids[i], &ts) == 0) {
            std::cout << "Clock " << clock_names[i] << " has resolution: " << ts.tv_sec << "s " << ts.tv_nsec << "ns" << std::endl;
        } else {
            std::cerr << "Error getting resolution for clock " << clock_names[i] << std::endl;
        }
    }

    std::cout << std::endl;

    for (int i = 0; i < num_clocks; i++) {
       std::cout << "Clock " << clock_names[i] << " sleeping for 500 ms" << std::endl;

       pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

       pthread_condattr_t attr;
       pthread_condattr_init(&attr);
       pthread_condattr_setclock(&attr, clock_ids[i]);

       pthread_cond_t condition;
       pthread_cond_init(&condition, &attr);

       pthread_mutex_lock(&mutex);
       auto start = get_milli();
       timespec ts{};
       clock_gettime(clock_ids[i], &ts);
       std::cout << "   > timespec before: " << ts.tv_sec << "s " << ts.tv_nsec << "ns" << std::endl;
       ts.tv_nsec += 500000000;
       if (ts.tv_nsec > 1000000000) {
          ts.tv_sec++;
          ts.tv_nsec %= 1000000000;
       }
       pthread_cond_timedwait(&condition, &mutex, &ts);
       auto end = get_milli();

       clock_gettime(clock_ids[i], &ts);
       std::cout << "   > timespec after:  " << ts.tv_sec << "s " << ts.tv_nsec << "ns" << std::endl;
       std::cout << "actually slept for " << end - start << " ms\n" << std::endl;

       pthread_mutex_unlock(&mutex);

       pthread_cond_destroy(&condition);
       pthread_mutex_destroy(&mutex);
    }

    return 0;
}
