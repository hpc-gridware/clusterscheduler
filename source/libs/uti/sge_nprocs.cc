/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 * 
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 * 
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 * 
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Number of processors available to the host
 */
#include "msg_utilib.h"

#if defined(DARWIN)
#   include <mach/host_info.h>
#   include <mach/mach_host.h>
#   include <mach/mach_init.h>
#endif

#if defined(SOLARIS) || defined(LINUX)

#   include <unistd.h>

#endif

#if defined(LINUX)
#   include <sched.h>
#   include <cstdlib>
#   include <fstream>
#   include <string>
#endif

#include <algorithm>

#if defined(FREEBSD)
#   include <sys/types.h>
#   include <sys/sysctl.h>
#endif

#if defined(NETBSD)
#   include <sys/param.h>
#   include <sys/sysctl.h>
#endif

#ifdef NPROCS_TEST
#   include <cstdio>
#   include <unistd.h>
void main(int, char**);
#endif

#ifndef NPROCS_TEST

#include "sge_os.h"

#else
int sge_nprocs ();
#endif

/**
 * @brief Number of processors in this machine
 *
 * Use this function to get the number of processors in
 * this machine
 *
 * @return number of procs
 *
 * @note MT-NOTE: sge_nprocs() is MT safe (SOLARIS, LINUX)
 */
int sge_nprocs() {
   int nprocs = 1; /* default */

#if defined(DARWIN)
   struct host_basic_info cpu_load_data;

   mach_msg_type_number_t host_count = sizeof(cpu_load_data)/sizeof(integer_t);
   mach_port_t host_priv_port = mach_host_self();

   host_info(host_priv_port, HOST_BASIC_INFO , (host_info_t)&cpu_load_data, &host_count);

   nprocs =  cpu_load_data.avail_cpus;

#endif

#if defined(SOLARIS) || defined(LINUX)
   nprocs = sysconf(_SC_NPROCESSORS_ONLN);
#endif

#if defined(FREEBSD)
   size_t nprocs_len = sizeof(nprocs);

   if (sysctlbyname("hw.ncpu", &nprocs, &nprocs_len, nullptr, 0) == -1) {
      nprocs = -1;
   }
#endif

#if defined(NETBSD)
   int mib[2];
   size_t nprocs_len;

   nprocs_len = sizeof(nprocs);
   mib[0]     = CTL_HW;
   mib[1]     = HW_NCPU;

   if (sysctl(mib, sizeof(mib)/sizeof(int), &nprocs, &nprocs_len, nullptr, 0) == -1) {
     nprocs = -1;
   }
#endif

   if (nprocs <= 0) {
      nprocs = 1;
   }

   return nprocs;
}

#if defined(LINUX)
/**
 * @brief The CPU quota the cgroup v2 hierarchy puts on the calling process
 *
 * Reads the cgroup of the process from `/proc/self/cgroup` and walks from it up to the root of the cgroup
 * file system. Every `cpu.max` on the way limits the process, so the smallest quota counts. A quota of
 * `<quota> <period>` allows quota / period CPUs, rounded up; `max` sets no limit.
 *
 * cgroup v1 has no `0::` line and is not looked at.
 *
 * @return the quota in whole CPUs, or 0 when no quota applies
 */
static int
cgroup_cpu_quota() {
   std::ifstream self("/proc/self/cgroup");
   std::string path;
   for (std::string line; std::getline(self, line);) {
      if (line.starts_with("0::")) {
         path = line.substr(3);
         break;
      }
   }
   if (path.empty() || path[0] != '/') {
      return 0;
   }

   int limit = 0;
   while (true) {
      std::ifstream cpu_max("/sys/fs/cgroup" + path + "/cpu.max");
      std::string quota;
      long long period = 0;
      if (cpu_max >> quota >> period && quota != "max" && period > 0) {
         char *end = nullptr;
         const long long q = strtoll(quota.c_str(), &end, 10);
         if (end != quota.c_str() && *end == '\0' && q > 0) {
            const int cpus = static_cast<int>((q + period - 1) / period);
            if (limit == 0 || cpus < limit) {
               limit = cpus;
            }
         }
      }
      if (path == "/") {
         break;
      }
      const auto slash = path.find_last_of('/');
      path = slash == 0 ? "/" : path.substr(0, slash);
   }
   return limit;
}
#endif

/**
 * @brief Number of CPUs the calling process may use
 *
 * Unlike sge_nprocs(), which counts the CPUs of the machine, this honours what restricts the process: on
 * Linux the CPU affinity of the calling thread and a cgroup v2 CPU quota (`cpu.max`). A process in a
 * container or in a systemd slice with a CPU limit gets that limit. Other operating systems report
 * sge_nprocs().
 *
 * @return the number of CPUs, at least 1
 */
int
ocs_available_cpus() {
   int cpus = sge_nprocs();
#if defined(LINUX)
   cpu_set_t set;
   CPU_ZERO(&set);
   if (sched_getaffinity(0, sizeof(set), &set) == 0) {
      cpus = CPU_COUNT(&set);
   }
   if (const int quota = cgroup_cpu_quota(); quota > 0 && quota < cpus) {
      cpus = quota;
   }
#endif
   return std::max(cpus, 1);
}

#ifdef NPROCS_TEST
void main(
int argc,
char **argv 
) {
   printf(MSG_INFO_NUMBOFPROCESSORS_I, sge_nprocs());
   printf("\n");
   exit(0);
}
#endif
