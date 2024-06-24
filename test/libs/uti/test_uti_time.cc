/*___INFO__MARK_BEGIN_NEW__*/
/*___INFO__MARK_END_NEW__*/

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <iostream>
#include <chrono>

#include "sge_time.h"

void
basic_functions() {
   // expect time_t to be 8 bytes
   size_t time_t_size = sizeof(time_t);
   std::cout << "size of time_t: " << time_t_size;
   if (time_t_size >= 8) {
      std::cout << " ok" << std::endl;
   } else {
      std::cout << " we cannot rely on it being 8 bytes" << std::endl;
   }

   // c way
   time_t time_t_now = time(nullptr);

   // c++ way
   const auto now = std::chrono::system_clock::now(); // timepoint
   const auto epoch = now.time_since_epoch();
   const auto s = duration_cast<std::chrono::seconds>(epoch);
   const auto ms = duration_cast<std::chrono::milliseconds>(epoch);
   const auto mus = duration_cast<std::chrono::microseconds>(epoch);
   const auto tt = std::chrono::system_clock::to_time_t(now);

   std::cout << "time(0): " << time_t_now << ", chrono: " << s.count() << ", in ms: " << ms.count() << ", in µs: " << mus.count() << std::endl;
   std::cout << "sizes: now: " << sizeof(now) << ", epoch: " << sizeof(epoch) << ", s: " << sizeof(s) << ", ms: " << sizeof(ms) << std::endl;
   // cannot use std::localtime, it is not thread safe!
   struct tm tm;
   localtime_r(&tt, &tm);
   std::cout << asctime(&tm);

   // having a time stamp in ms as u_long64
   u_long64 cull_time_stamp = 1717508158535;

   // make it a time_t as we want to call localtime_r for output
   std::chrono::milliseconds xms{cull_time_stamp};
   // make it a time_point - for whatever purpose
   std::chrono::system_clock::time_point timePoint{xms};
   // convert to seconds which we need for time output
   std::chrono::seconds xs = duration_cast<std::chrono::seconds>(xms);
   const time_t xt{(time_t)xs.count()};
   localtime_r(&xt, &tm);
   std::cout << asctime(&tm);

#if not defined(TARGET_32BIT)
   // can it handle time stamps after 2038?
   // probably not on 32bit as time_t is just a long
   const time_t xxt{U_LONG32_MAX};
   localtime_r(&xxt, &tm);
   std::cout << asctime(&tm); // should print Sun Feb  7 07:28:15 2106
#endif
}

bool test_64bit() {
   bool ret = true;
   u_long64 time64 = sge_get_gmt64();
   std::cout << "sge_get_gmt64(): " << time64 << std::endl;

   time_t timestamp = time(nullptr);
   u_long32 time32 = (u_long32)timestamp;
   std::cout << "time(nullptr):   " << timestamp << " = " << time32 << std::endl;

   if (labs(sge_gmt64_to_gmt32(time64) - timestamp) > 1) {
      std::cerr << "64bit timestamp as seconds (" << sge_gmt64_to_gmt32(time64) <<
                ") should be about the same as the time_t timestamp (" << timestamp << ")" << std::endl;
      ret = false;
   }

   if (labs(sge_gmt64_to_gmt32(time64) - time32) > 1) {
      std::cerr << "64bit timestamp as seconds (" << sge_gmt64_to_gmt32(time64) <<
                   ") should be about the same as the 32bit timestamp (" << time32 << ")" << std::endl;
      ret = false;
   }

   DSTRING_STATIC(dstr, 100);
   std::cout << append_time(time64, &dstr, false) << std::endl;
   sge_dstring_clear(&dstr);
   std::cout << append_time((time_t)time32, &dstr, false) << std::endl;
   sge_dstring_clear(&dstr);
   std::cout << append_time(time64, &dstr, true) << std::endl;
   sge_dstring_clear(&dstr);
   std::cout << append_time((time_t)time32, &dstr, true) << std::endl;

   std::cout << sge_ctime64(time64, &dstr, false, true) << std::endl;
   std::cout << sge_ctime64(time64, &dstr, true, true) << std::endl;
   std::cout << sge_ctime64(time64, &dstr) << std::endl;
   std::cout << sge_ctime64_short(time64, &dstr) << std::endl;
   std::cout << sge_ctime64_xml(time64, &dstr) << std::endl;

   return ret;
}

int main(int argc, char *argv[]) {
   bool ret = true;

   basic_functions();
   ret = test_64bit();

   return ret ? EXIT_SUCCESS : EXIT_FAILURE;
}
