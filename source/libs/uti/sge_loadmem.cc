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

#include <cstring>

#ifdef TEST
#include <unistd.h>
#include <cerrno>
#endif

#include "uti/msg_utilib.h"
#include "uti/sge_loadmem.h"
#include "uti/sge_log.h"
#include "uti/sge_os.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"

#if !defined(LINUX) && !defined(DARWIN) && !defined(FREEBSD) && !defined(NETBSD)

#include <unistd.h>

int (*p_page2M)(int size);
void init_pageshift(void);
static int page2M_none(int size);
static int page2M_left(int size);
static int page2M_right(int size);

#define page2M(size) ((*p_page2M)(size))

/* MT-NOTE: only execd and utilities use code that depends on the modules global variables */

static int pageshift;

static int page2M_none(
int size 
) {
   return (size);
}
 
static int page2M_left(
int size 
) {
   return (size << pageshift); 
}   
 
static int page2M_right(
int size 
) {
   return (size >> pageshift);
}   

void init_pageshift()
{
   int i;
   i = sysconf(_SC_PAGESIZE);
   pageshift = 0;
   while ((i >>= 1) > 0) {
      pageshift++;
   }

   pageshift -= 20; /* adjust for MB */


   /* now determine which pageshift function is appropriate for the
      result (have to because x << y is undefined for y < 0) */
   if (pageshift > 0)
      p_page2M = page2M_left;
   else if (pageshift == 0) 
      p_page2M = page2M_none;
   else {
      p_page2M = page2M_right;
      pageshift = -pageshift;   
   }
}

#endif

#if TEST

int main(
int argc,
char *argv[]  
) {
   sge_mem_info_t mem_info;

   memset(&mem_info, 0, sizeof(sge_mem_info_t));
   if (sge_loadmem(&mem_info)) {
      fprintf(stderr, "error: failed retrieving memory info\n");
      return 1;
   }
      
   printf("mem_free      %fM\n", mem_info.mem_free);
   printf("mem_total     %fM\n", mem_info.mem_total);
   printf("swap_total    %fM\n", mem_info.swap_total);
   printf("swap_free     %fM\n", mem_info.swap_free);
   printf("virtual_total %fM\n", mem_info.mem_total + mem_info.swap_total);
   printf("virtual_free  %fM\n", mem_info.mem_free  + mem_info.swap_free);

   return 0;
}
#endif /* TEST */


/*--------------------------------------------------------------------------*/
#if defined(SOLARIS)
#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/swap.h>
#include <nlist.h>
#include <sys/types.h>
#include <fcntl.h>

int sge_loadmem(sge_mem_info_t *mem_info) 
{
   long total, fr;
   long cnt, i;
   long t, f, l;
   struct swaptable *swt;
   struct swapent *ste;
   static char path[256];
   int sz;

   long freemem;

   DENTER(TOP_LAYER);

   init_pageshift();
   
   /* get total number of swap entries */
   if ((cnt = swapctl(SC_GETNSWP, 0))<0) {
      DRETURN(-1);
   }

   /* allocate enough space to hold count + n swapents */
   sz =  sizeof(long) + cnt * sizeof(struct swapent);
   swt = (struct swaptable *) calloc(1, sz);

   if (swt == nullptr) {
      total = 0;
      fr = 0;
      DRETURN(-1);
   }
   swt->swt_n = cnt;

   /* fill in ste_path pointers: we don't care about the paths, so we point
      them all to the same buffer */
   ste = &(swt->swt_ent[0]);
   i = cnt;
   while (--i >= 0) {
      ste++->ste_path = path;
   }

   /* grab all swap info */
   if (swapctl(SC_LIST, swt) != cnt) {
      DRETURN(-1);
   }

   /* walk thru the structs and sum up the fields */
   t = f = l = 0;
   ste = &(swt->swt_ent[0]);

   i = cnt;
   while (--i >= 0) {
      /* dont count slots being deleted */
      if (!(ste->ste_flags & ST_INDEL) &&
          !(ste->ste_flags & ST_DOINGDEL)) {
      /* DPRINTF(("%s pages: %ld free: %ld length %ld\n", 
            ste->ste_path,
            ste->ste_pages,
            ste->ste_free,
            ste->ste_length)); */
         t += ste->ste_pages;
         f += ste->ste_free;
         l += ste->ste_length;      
      }
      ste++;
   }

   /* fill in the results */
   total = t;
   fr = f;
   sge_free(&swt);
   mem_info->swap_total = page2M(total);
   mem_info->swap_free = page2M(fr);

   if (get_freemem(&freemem)) {
      DRETURN(-1);
   }

   mem_info->mem_free = page2M(freemem);
   mem_info->mem_total = page2M(sysconf(_SC_PHYS_PAGES));

   return 0;
}
#endif /* SOLARIS */


/*--------------------------------------------------------------------------*/
#if defined(LINUX)

#include <cstdio>
#include <cstring>

#define PROC_MEMINFO "/proc/meminfo"

#define KEY_MEMTOTAL  "MemTotal"
#define KEY_MEMFREE   "MemFree"
#define KEY_SWAPTOTAL "SwapTotal"
#define KEY_SWAPFREE  "SwapFree"

#define KEY_BUFFERS   "Buffers"
#define KEY_CACHED    "Cached"

int sge_loadmem(sge_mem_info_t *mem_info) {
   int ret = 0;
   char dummy[512], buffer[1024];
   double kbytes;
   FILE *fp;
   double buffers = 0, cached = 0;

   if ((fp = fopen(PROC_MEMINFO, "r"))) {
      while (fgets(buffer, sizeof(buffer) - 1, fp)) {

#define READ_VALUE(key, dest)    if (!strncmp(buffer, key, sizeof(key)-1)) { \
            sscanf(buffer, "%[^0-9]%lf", dummy, &kbytes); \
            dest = kbytes/1024; \
            continue; \
         }

         READ_VALUE(KEY_MEMTOTAL, mem_info->mem_total);
         READ_VALUE(KEY_MEMFREE, mem_info->mem_free);
         READ_VALUE(KEY_SWAPTOTAL, mem_info->swap_total);
         READ_VALUE(KEY_SWAPFREE, mem_info->swap_free);
         READ_VALUE(KEY_BUFFERS, buffers);
         READ_VALUE(KEY_CACHED, cached);

      }
      FCLOSE(fp);
      mem_info->mem_free += buffers + cached;
   } else {
      ret = 1;
   }

   return ret;
   FCLOSE_ERROR:
   return 1;
}

#endif /* LINUX */


#if defined(DARWIN)
#include <mach/mach_init.h>
#include <mach/mach_host.h>
#include <mach/host_info.h>
#include <sys/sysctl.h>

int sge_loadmem(sge_mem_info_t *mem_info)
{
    uint64_t mem_total;
    size_t len = sizeof(mem_total);

    vm_statistics_data_t vm_info;
    mach_msg_type_number_t info_count = HOST_VM_INFO_COUNT;

    sysctlbyname("hw.memsize", &mem_total, &len, nullptr, 0);
    mem_info->mem_total = mem_total / (1024*1024);

    host_statistics(mach_host_self (), HOST_VM_INFO, (host_info_t)&vm_info, &info_count);
    mem_info->mem_free = ((double)vm_info.free_count)*vm_page_size / (1024*1024);

    return 0;
}

#endif /* DARWIN */

#if defined(FREEBSD)
#include <sys/types.h>
#include <sys/sysctl.h>
#include <fcntl.h>
#include <kvm.h>

int sge_loadmem(sge_mem_info_t *mem_info) 
{
   int			i, n;
   int			swap_count, usedswap_count;
   size_t		tmpsize;
   unsigned int		page_size;
   unsigned int		page_count;
   unsigned int		free_count, cache_count, inactive_count;
   kvm_t		*kd;
   struct kvm_swap	kswap[16];

   tmpsize = sizeof(page_size);
   if (sysctlbyname("vm.stats.vm.v_page_size", &page_size, &tmpsize,
      nullptr, 0) == -1)
      return -1;
   tmpsize = sizeof(page_count);
   if (sysctlbyname("vm.stats.vm.v_page_count", &page_count, &tmpsize,
      nullptr, 0) == -1)
      return -1;
   mem_info->mem_total = (page_count * page_size) / (1024.0*1024.0);

   /*
    * The concept of free memory doesn't make much sense when you're
    * talking about the FreeBSD vm, but we'll fake it as the number of
    * pages marked free, inactive, or cache.
    */
   tmpsize = sizeof(free_count);
   if (sysctlbyname("vm.stats.vm.v_free_count", &free_count, &tmpsize,
      nullptr, 0) == -1)
      return -1;
   tmpsize = sizeof(cache_count);
   if (sysctlbyname("vm.stats.vm.v_cache_count", &cache_count, &tmpsize,
      nullptr, 0) == -1)
      return -1;
   tmpsize = sizeof(inactive_count);
   if (sysctlbyname("vm.stats.vm.v_inactive_count", &inactive_count, &tmpsize,
      nullptr, 0) == -1)
      return -1;
   mem_info->mem_free =
      ((free_count + cache_count + inactive_count) * page_size) /
      (1024.0*1024.0);

   /*
    * Grovel around in kernel memory to find out how much swap we have.
    * This only works if we're in group kmem so to let other programs
    * maintain limited functionality, we'll just report zero if we can't
    * open /dev/mem.
    *
    * XXX: On 5.0+ we should really use the new sysctl interface to
    * swap stats.
    */
   swap_count = 0;
   usedswap_count = 0;
   if ((kd = kvm_open(nullptr, nullptr, nullptr, O_RDONLY, "sge_loadmem")) != nullptr) {
      n = kvm_getswapinfo(kd, kswap, sizeof(kswap)/sizeof(kswap[0]), 0);
      kvm_close(kd);
      if (n == -1)
         return -1;

      for (i = 0; i < n; i++) {
         swap_count += kswap[i].ksw_total;
         usedswap_count += kswap[i].ksw_used;
      }
   }
   mem_info->swap_total = (swap_count * page_size) / (1024.0*1024.0);
   mem_info->swap_free = ((swap_count - usedswap_count) * page_size) /
      (1024.0*1024.0);

   return 0;
}
#endif /* FREEBSD */

#if defined(NETBSD)

#include <sys/param.h>
#include <sys/sysctl.h>

int sge_loadmem(sge_mem_info_t *mem_info)
{
  int mib[2];
  size_t size;
  struct uvmexp_sysctl uvmexp;

  mib[0] = CTL_VM;
  mib[1] = VM_UVMEXP2;
  size   = sizeof(uvmexp);

  sysctl(mib, sizeof(mib)/sizeof(int), &uvmexp, &size, nullptr, 0);

  /* Memory */
  mem_info->mem_total = (uvmexp.npages * uvmexp.pagesize) / (1024 * 1024);
  mem_info->mem_free  = (uvmexp.free   * uvmexp.pagesize) / (1024 * 1024);

  /* Swap */
  mem_info->swap_total = (uvmexp.swpages * uvmexp.pagesize) / (1024 * 1024);
  mem_info->swap_free = ((uvmexp.swpages - uvmexp.swpginuse) * uvmexp.pagesize) / (1024 * 1024);

  return 0;
}
#endif /* NETBSD */

