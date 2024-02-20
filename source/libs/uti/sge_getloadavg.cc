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
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cerrno>
#include <fcntl.h>
#include <cstdio>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>

#include "uti/sge_getloadavg.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

#include "sge.h"

#if defined(SOLARIS)
#  include <nlist.h> 
#  include <sys/cpuvar.h>
#  include <kstat.h> 
#  include <sys/loadavg.h> 
#elif defined(LINUX)

#  include <cctype>

#elif defined(DARWIN)
# include <mach/host_info.h>
# include <mach/mach_host.h>
# include <mach/mach_init.h>
# include <mach/machine.h>
#elif defined(FREEBSD)
#  if defined(__FreeBSD_version) && __FreeBSD_version < 500101
#     include <sys/dkstat.h>
#  endif
#  include <sys/resource.h>
#  include <fcntl.h>
#  include <kvm.h>
#elif defined(NETBSD)
#  include <sys/sched.h>
#  include <sys/param.h>
#  include <sys/sysctl.h>
#endif

#define KERNEL_TO_USER_AVG(x) ((double)x/SGE_FSCALE)

#if defined(SOLARIS)
#  define CPUSTATES 5
#  define CPUSTATE_IOWAIT 3
#  define CPUSTATE_SWAP 4    
#  if SOLARIS64
#     define KERNEL_AVG_TYPE long
#     define KERNEL_AVG_NAME "avenrun"
#  else
#     define SGE_FSCALE FSCALE 
#     define KERNEL_AVG_TYPE long
#     define KERNEL_AVG_NAME "avenrun"
#  endif
#elif defined(LINUX)
#  define LINUX_LOAD_SOURCE "/proc/loadavg"
#  define CPUSTATES 4
#  define PROCFS "/proc"
#endif

#if defined(FREEBSD)
typedef kvm_t* kernel_fd_type;
#else
typedef int kernel_fd_type;
#endif

#ifdef SGE_LOADCPU

static long percentages(int cnt, double *out, long *new_value, long *old_value, long *diffs);

#endif

#if defined(LINUX) || defined(DARWIN)

#ifndef DARWIN

static int get_load_avg(double loadv[], int nelem);

#endif

static double get_cpu_load(void);

#endif

#if defined(LINUX)

static char *skip_token(char *p);

#endif

#if defined(FREEBSD)

static int sge_get_kernel_fd(kernel_fd_type *kernel_fd);

static int sge_get_kernel_address(char *name, long *address);
 
static int getkval(unsigned long offset, int *ptr, int size, char *refstr);

#endif

#if !defined(LINUX) && !defined(SOLARIS)
static kernel_fd_type kernel_fd;
#endif

/* MT-NOTE: code basing on kernel_initialized global variable needs not to be MT safe */
static int kernel_initialized = 0;

#if defined(FREEBSD)

static int sge_get_kernel_address(
char *name,
long *address 
) {
   int ret = 0;
   struct nlist kernel_nlist[2];

   DENTER(TOP_LAYER);

   kernel_nlist[0].n_name = name;
   kernel_nlist[1].n_name = nullptr;
   if (kernel_initialized && (kvm_nlist(kernel_fd, kernel_nlist) >= 0))
   {
      *address = kernel_nlist[0].n_value;
      ret = 1;
   } else {
      DPRINTF(("nlist(%s) failed: %s\n", name, strerror(errno)));
      *address = 0;
      ret = 0;
   }
   DRETURN(ret);
}    


static int sge_get_kernel_fd(
kernel_fd_type *fd 
) {
   char prefix[256] = "my_error:";

   DENTER(TOP_LAYER);

   if (!kernel_initialized) {
      kernel_fd = kvm_open(nullptr, nullptr, nullptr, O_RDONLY, prefix);
      if (kernel_fd != nullptr)
      {
         kernel_initialized = 1;
      } else {
         DPRINTF(("kvm_open() failed: %s\n", strerror(errno)));
         kernel_initialized = 0;
      }
   } 
   *fd = kernel_fd; 
   DRETURN(kernel_initialized);
}

static int getkval(
unsigned long offset, 
int *ptr, 
int size, 
char *refstr 
) {
   kernel_fd_type kernel_fd;

#if defined(FREEBSD)
   if (sge_get_kernel_fd(&kernel_fd)
       && kvm_read(kernel_fd, offset, (char *)ptr, size) != size) {
      if (*refstr == '!') {
         return 0;
      } else {
         return -1;
      }
   }
#else
   if (sge_get_kernel_fd(&kernel_fd)) {
      if (lseek(kernel_fd, (long)offset, 0) == -1) {
         if (*refstr == '!') {
            refstr++;
         }
         return -1;
      }
      if (read(kernel_fd, (char *)ptr, size) == -1) {
         if (*refstr == '!') {
            return 0;
         } else {
            return -1;
         }
      }
   }
#endif
   return 0;
}

#endif

#if defined(SOLARIS)
/* MT-NOTE kstat is not thread save */
int get_freemem(
long *freememp 
) {
   kstat_ctl_t   *kc;  
   kstat_t       *ksp;  
   kstat_named_t *knp;
   kc = kstat_open();  
   ksp = kstat_lookup(kc, "unix", 0, "system_pages");
   if (kstat_read(kc, ksp, nullptr) == -1) {
      kstat_close(kc);
      return -1;
   } 
   knp = (kstat_named_t *)kstat_data_lookup(ksp, "freemem");
   *freememp = (long)knp -> value.ul;
   kstat_close(kc);
   return 0;
}


#endif


#if defined(SOLARIS)

static kstat_ctl_t *kc = nullptr;
static kstat_t **cpu_ks = nullptr;
static cpu_stat_t *cpu_stat = nullptr;

#define UPDKCID(nk,ok) \
if (nk == -1) { \
  perror("kstat_read "); \
  return -1; \
} \
if (nk != ok)\
  goto kcid_changed;

int kupdate(int avenrun[3])
{
   kstat_t *ks;
   kid_t nkcid;
   int i;
   int changed = 0;
   static int ncpu = 0;
   static kid_t kcid = 0;
   kstat_named_t *kn;

   /*
   * 0. kstat_open
   */
   if (!kc) {
      kc = kstat_open();
      if (!kc) {
         perror("kstat_open ");
         return -1;
      }
      changed = 1;
      kcid = kc->kc_chain_id;
   }


   /* keep doing it until no more changes */
   kcid_changed:

   /*
   * 1.  kstat_chain_update
   */
   nkcid = kstat_chain_update(kc);
   if (nkcid) {
      /* UPDKCID will abort if nkcid is -1, so no need to check */
      changed = 1;
      kcid = nkcid;
   }
   UPDKCID(nkcid,0);

   ks = kstat_lookup(kc, "unix", 0, "system_misc");
   if (kstat_read(kc, ks, 0) == -1) {
      perror("kstat_read");
      return -1;
   }

#if 0
   /* load average */
   kn = kstat_data_lookup(ks, "avenrun_1min");
   if (kn)
      avenrun[0] = kn->value.ui32;
   kn = kstat_data_lookup(ks, "avenrun_5min");
   if (kn)
      avenrun[1] = kn->value.ui32;
   kn = kstat_data_lookup(ks, "avenrun_15min");
   if (kn)
      avenrun[2] = kn->value.ui32;

   /* nproc */
   kn = kstat_data_lookup(ks, "nproc");
   if (kn) {
      nproc = kn->value.ui32;
#ifdef NO_NPROC
      if (nproc > maxprocs)
      reallocproc(2 * nproc);
#endif
   }
#endif

   if (changed) {
      int ncpus = 0;

      /*
      * 2. get data addresses
      */

      ncpu = 0;

      kn = (kstat_named_t *)kstat_data_lookup(ks, "ncpus");
      if (kn && kn->value.ui32 > ncpus) {
         ncpus = kn->value.ui32;
         cpu_ks = (kstat_t **)sge_realloc(cpu_ks, ncpus * sizeof(kstat_t *), 1);
         cpu_stat = (cpu_stat_t *)sge_realloc(cpu_stat, ncpus * sizeof(cpu_stat_t), 1);
      }

      for (ks = kc->kc_chain; ks; ks = ks->ks_next) {
         if (strncmp(ks->ks_name, "cpu_stat", 8) == 0) {
            nkcid = kstat_read(kc, ks, nullptr);
            /* if kcid changed, pointer might be invalid */
            UPDKCID(nkcid, kcid);

            cpu_ks[ncpu] = ks;
            ncpu++;
            if (ncpu >= ncpus) {
               break;
            }
         }
      }
      /* note that ncpu could be less than ncpus, but that's okay */
      changed = 0;
   }


   /*
   * 3. get data
   */

   for (i = 0; i < ncpu; i++) {
      nkcid = kstat_read(kc, cpu_ks[i], &cpu_stat[i]);
      /* if kcid changed, pointer might be invalid */
      UPDKCID(nkcid, kcid);
   }

   /* return the number of cpus found */
   return(ncpu);
}

double get_cpu_load(void) {
   int cpus_found, i, j;
   double cpu_load = -1.0;
   static long cpu_time[CPUSTATES] = { 0L, 0L, 0L, 0L, 0L};
   static long cpu_old[CPUSTATES]  = { 0L, 0L, 0L, 0L, 0L};
   static long cpu_diff[CPUSTATES] = { 0L, 0L, 0L, 0L, 0L}; 
   double cpu_states[CPUSTATES];
   int avenrun[3];

   DENTER(CULL_LAYER);

   /* use kstat to update all processor information */
   if ((cpus_found = kupdate(avenrun)) < 0 ) {
      DRETURN(cpu_load);
   }
   for (i=0; i<CPUSTATES; i++)
      cpu_time[i] = 0;

   for (i = 0; i < cpus_found; i++) {
      /* sum counters up to, but not including, wait state counter */
      for (j = 0; j < CPU_WAIT; j++) {
         cpu_time[j] += (long) cpu_stat[i].cpu_sysinfo.cpu[j];
         DPRINTF(("cpu_time[%d] = %ld (+ %ld)\n", j,  cpu_time[j],
               (long) cpu_stat[i].cpu_sysinfo.cpu[j]));
      }
      /* add in wait state breakdown counters */
      cpu_time[CPUSTATE_IOWAIT] += (long) cpu_stat[i].cpu_sysinfo.wait[W_IO] +
                                   (long) cpu_stat[i].cpu_sysinfo.wait[W_PIO];
      DPRINTF(("cpu_time[%d] = %ld (+ %ld)\n", CPUSTATE_IOWAIT,  cpu_time[CPUSTATE_IOWAIT],
                                (long) cpu_stat[i].cpu_sysinfo.wait[W_IO] +
                                (long) cpu_stat[i].cpu_sysinfo.wait[W_PIO]));


      cpu_time[CPUSTATE_SWAP] += (long) cpu_stat[i].cpu_sysinfo.wait[W_SWAP];
      DPRINTF(("cpu_time[%d] = %ld (+ %ld)\n", CPUSTATE_SWAP,  cpu_time[CPUSTATE_SWAP],
         (long) cpu_stat[i].cpu_sysinfo.wait[W_SWAP]));
   }
   percentages(CPUSTATES, cpu_states, cpu_time, cpu_old, cpu_diff);
   cpu_load = cpu_states[1] + cpu_states[2] + cpu_states[3] + cpu_states[4];
   DPRINTF(("cpu_load %f ( %f %f %f %f )\n", cpu_load,
         cpu_states[1], cpu_states[2], cpu_states[3], cpu_states[4]));
#if 0
#if defined(SOLARIS) && !defined(SOLARIS64)
   DPRINTF(("avenrun(%d %d %d) -> (%f %f %f)\n", avenrun[0], avenrun[1], avenrun[2],
      KERNEL_TO_USER_AVG(avenrun[0]), KERNEL_TO_USER_AVG(avenrun[1]), KERNEL_TO_USER_AVG(avenrun[2])));
#endif
#endif

   DRETURN(cpu_load);
}
#elif defined(LINUX)

static char *skip_token(char *p) {
   while (isspace(*p)) {
      p++;
   }
   while (*p && !isspace(*p)) {
      p++;
   }
   return p;
}

static double get_cpu_load() {
   int fd = -1;
   int len, i;
   char buffer[4096];
   char filename[4096];
   char *p;
   double cpu_load;
   static long cpu_new[CPUSTATES];
   static long cpu_old[CPUSTATES];
   static long cpu_diff[CPUSTATES];
   static double cpu_states[CPUSTATES];

   sprintf(filename, "%s/stat", PROCFS);
   fd = open(filename, O_RDONLY);
   if (fd == -1) {
      return -1;
   }
   len = read(fd, buffer, sizeof(buffer) - 1);
   close(fd);
   buffer[len] = '\0';
   p = skip_token(buffer);
   for (i = 0; i < CPUSTATES; i++) {
      cpu_new[i] = strtoul(p, &p, 10);
   }
   percentages(CPUSTATES, cpu_states, cpu_new, cpu_old, cpu_diff);

   cpu_load = cpu_states[0] + cpu_states[1] + cpu_states[2];

   if (cpu_load < 0.0) {
      cpu_load = -1.0;
   }
   return cpu_load;
}

#elif defined(FREEBSD)

static double get_cpu_load()
{
   kernel_fd_type kernel_fd;
   long address = 0;
   static long cpu_time[CPUSTATES];
   static long cpu_old[CPUSTATES];
   static long cpu_diff[CPUSTATES];
   double cpu_states[CPUSTATES];
   double cpu_load;

   if (sge_get_kernel_fd(&kernel_fd)
       && sge_get_kernel_address("cp_time", &address)) {
      getkval(address, (int *)&cpu_time, sizeof(cpu_time), "cp_time");
      percentages(CPUSTATES, cpu_states, cpu_time, cpu_old, cpu_diff);
      cpu_load = cpu_states[0] + cpu_states[1] + cpu_states[2];
      if (cpu_load < 0.0) {
         cpu_load = -1.0;
      }
   } else {
      cpu_load = -1.0;
   }
   return cpu_load;
}    

#elif defined(DARWIN)

double get_cpu_load(void)
{
   static long cpu_new[CPU_STATE_MAX];
   static long cpu_old[CPU_STATE_MAX];
   static long cpu_diff[CPU_STATE_MAX];
   double cpu_states[CPU_STATE_MAX];
   double cpu_load;
   int i;

   kern_return_t error;
   struct host_cpu_load_info cpu_load_data;
   mach_msg_type_number_t host_count = sizeof(cpu_load_data)/sizeof(integer_t);
   mach_port_t host_priv_port = mach_host_self();

   error = host_statistics(host_priv_port, HOST_CPU_LOAD_INFO,
        (host_info_t)&cpu_load_data, &host_count);

   if (error != KERN_SUCCESS) {
      return -1.0;
   }

   for (i = 0; i < CPU_STATE_MAX; i++) {
      cpu_new[i] = cpu_load_data.cpu_ticks[i];
   }

   percentages (CPU_STATE_MAX, cpu_states, cpu_new, cpu_old, cpu_diff);

   cpu_load = cpu_states[CPU_STATE_USER] + cpu_states[CPU_STATE_SYSTEM] + cpu_states[CPU_STATE_NICE];

   if (cpu_load < 0.0) {
      cpu_load = -1.0;
   }
   return cpu_load;
}

#elif defined(NETBSD)

double get_cpu_load()
{
  int mib[2];
  static long cpu_time[CPUSTATES];
  static long cpu_old[CPUSTATES];
  static long cpu_diff[CPUSTATES];
  double cpu_states[CPUSTATES];
  double cpu_load;
  size_t size;

  mib[0] = CTL_KERN;
  mib[1] = KERN_CP_TIME;

  size = sizeof(cpu_time);

  sysctl(mib, sizeof(mib)/sizeof(int), &cpu_time, &size, nullptr, 0);
  percentages(CPUSTATES, cpu_states, cpu_time, cpu_old, cpu_diff);
  cpu_load = cpu_states[0] + cpu_states[1] + cpu_states[2];

  if (cpu_load < 0.0) {
    cpu_load = -1.0;
  }

  return cpu_load;

}
#endif

#if defined(LINUX)

static int get_load_avg(
        double loadv[],
        int nelem
) {
   char buffer[41];
   int fd, count;

   fd = open(LINUX_LOAD_SOURCE, O_RDONLY);
   if (fd == -1) {
      return -1;
   }
   count = read(fd, buffer, 40);
   buffer[count] = '\0';
   close(fd);
   if (count <= 0) {
      return -1;
   }
   count = sscanf(buffer, "%lf %lf %lf", &(loadv[0]), &loadv[1], &loadv[2]);
   if (count < 1) {
      return -1;
   }
   return 0;
}

#endif


int get_channel_fd(void) {
   if (kernel_initialized) {
#if defined(SOLARIS) || defined(LINUX) || defined(FREEBSD)
      return -1;
#else
      return kernel_fd;
#endif
   } else {
      return -1;
   }
}

int sge_getloadavg(double loadavg[], int nelem) {
   int elem = 0;

#if defined(SOLARIS) || defined(FREEBSD) || defined(NETBSD) || defined(DARWIN)
   elem = getloadavg(loadavg, nelem); /* <== library function */
#elif defined(LINUX)
   elem = get_load_avg(loadavg, nelem);
#else
   elem = -2;
#endif
   if (elem >= 0) {
      elem = nelem;
   }
   return elem;
}

#ifdef SGE_LOADCPU

/****** uti/os/sge_getcpuload() ***********************************************
*  NAME
*     sge_getcpuload() -- Retrieve cpu utilization percentage 
*
*  SYNOPSIS
*     int sge_getcpuload(double *cpu_load) 
*
*  FUNCTION
*     Retrieve cpu utilization percentage (load value "cpu")
*
*  INPUTS
*     double *cpu_load - caller passes adr of double variable 
*                        for cpu load
*
*  RESULT
*     int - error state
*         0 - OK
*        !0 - Error 
******************************************************************************/
int sge_getcpuload(double *cpu_load) {
   double load;
   int ret;

   DENTER(TOP_LAYER);

   if ((load = get_cpu_load()) < 0.0) {
      ret = -1;
   } else {
      *cpu_load = load;
      ret = 0;
   }

   DRETURN(ret);
}

static long percentages(int cnt, double *out, long *new_value, long *old_value, long *diffs) {
   int i;
   long change;
   long total_change;
   long *dp;
   long half_total;

   DENTER(CULL_LAYER);

   /* initialization */
   total_change = 0;
   dp = diffs;
   /* calculate changes for each state and the overall change */
   for (i = 0; i < cnt; i++) {
      if ((change = *new_value - *old_value) < 0) {
         /* this only happens when the counter wraps */
         change = (int)
                 ((unsigned long) *new_value - (unsigned long) *old_value);
      }
      total_change += (*dp++ = change);
      *old_value++ = *new_value++;
   }
   /* avoid divide by zero potential */
   if (total_change == 0) {
      total_change = 1;
   }
   /* calculate percentages based on overall change, rounding up */
   half_total = total_change / 2l;
   for (i = 0; i < cnt; i++) {
      *out = ((double) ((*diffs++ * 1000 + half_total) / total_change)) / 10;
#if 0
      DPRINTF(("diffs: %lu half_total: %lu total_change: %lu -> %f",
            *diffs, half_total, total_change, *out));
#endif
      out++;
   }

   DRETURN(total_change);
}

#endif

#ifdef TEST
int main(
int argc,
char *argv[],
char *envp[] 
) {
   int naptime = -1;

   if (argc > 1)
      naptime = atoi(argv[1]);

   while (1) {
      double avg[3];
      int loads;
      double cpu_load;

      errno = 0;             
      loads = sge_getloadavg(avg, 3);
      if (loads == -1) {
         perror("Error getting load average");
         exit(1);
      }
      if (loads > 0)
         printf("load average: %.2f", avg[0]);
      if (loads > 1)
         printf(", %.2f", avg[1]);
      if (loads > 2)
         printf(", %.2f", avg[2]);
      if (loads > 0)
         putchar('\n');

      cpu_load = get_cpu_load();
      if (cpu_load >= 0.0) {
         printf("cpu load: %.2f\n", cpu_load);
      }

      if (naptime == -1) {
         break;
      }
      sleep(naptime);
   }

   exit(0);
}
#endif /* TEST */

