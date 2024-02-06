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
 *  Portions of this code are Copyright 2011 Univa Inc.
 * 
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <unistd.h>

#include "uti/sge_rmon.h"
#include "uti/sge_os.h"
#include "uti/sge_language.h"
#include "uti/sge_bootstrap.h"
#include "uti/oge_topology.h"
#include "uti/sge_arch.h"

#include "sgeobj/sge_host.h"
#include "sgeobj/sge_binding.h"

#include "basis_types.h"
#include "msg_utilbin.h"

#include <TestClass.h>

#if defined(OGE_HWLOC)
#include <sys/utsname.h>
#endif

void usage();
void print_mem_load(char *, char *, int, double, char*);
void check_core_binding();

#if defined(OGE_HWLOC)
void test_hwloc();
#endif 

#if defined(OGE_HWLOC)
void fill_socket_core_topology(dstring* msocket, dstring* mcore, dstring* mthread, dstring* mtopology);
#endif

void usage()
{
   fprintf(stderr, "%s loadcheck [-cb] | [-int] [-loadval name]\n", MSG_UTILBIN_USAGE);
   fprintf(stderr, "\t\t[-cb] \t\t\t\t Shows core binding and processor topology related information.\n");
   fprintf(stderr, "\t\t[-int]\t\t\t\t Print as integer\n");
   fprintf(stderr, "\t\t[-loadval]\t\t\t Select specific load value\n");
   exit(1);
}
   
int main(int argc, char *argv[])
{
   double avg[3];
   int loads;
   char *name;
   oge::TestClass tst("troete");

   tst.method("rhabarberkuchen");

#if defined(OGE_HWLOC)
   dstring msocket   = DSTRING_INIT;
   dstring mcore     = DSTRING_INIT;
   dstring mthread   = DSTRING_INIT;
   dstring mtopology = DSTRING_INIT;
#endif

#ifdef SGE_LOADMEM
   sge_mem_info_t mem_info;
#endif

#ifdef SGE_LOADCPU
	double total = 0.0;	
#endif

   int pos = 0, print_as_int = 0, precision, core_binding = 0;
   char *m;

   DENTER_MAIN(TOP_LAYER, "loadcheck");

#ifdef __SGE_COMPILE_WITH_GETTEXT__   
   /* init language output for gettext() , it will use the right language */
   sge_init_language_func((gettext_func_type)        gettext,
                         (setlocale_func_type)      setlocale,
                         (bindtextdomain_func_type) bindtextdomain,
                         (textdomain_func_type)     textdomain);
   sge_init_language(nullptr,nullptr);
#endif /* __SGE_COMPILE_WITH_GETTEXT__  */
   if (argc == 2 && !strcmp(argv[1], "-cb")) {
      core_binding = 1;
   } else {
      for (int i = 1; i < argc;) {
         if (!strcmp(argv[i], "-int")) {
            print_as_int = 1;
         } else if (!strcmp(argv[i], "-loadval")) {
            if (i + 1 < argc) {
               pos = i + 1;
            } else {
               usage();
            }
            i++;
         } else {
            usage();
         }
         i++;
      }
   }   
   
   if (core_binding) {
      check_core_binding();
      DRETURN(1);
   } else if (print_as_int) {
      m = "";
      precision = 0;
   } else {
      m = "M";
      precision = 6;
   }   

   if ((pos && !strcmp("arch", argv[pos])) || !pos) {
      const char *arch = sge_get_arch();
      printf("arch            %s\n", arch);
   }
      
   if ((pos && !strcmp("num_proc", argv[pos])) || !pos) {
      int nprocs = sge_nprocs();
      printf("num_proc        %d\n", nprocs);
   }

#if defined(OGE_HWLOC)
   fill_socket_core_topology(&msocket, &mcore, &mthread, &mtopology);
   if ((pos && !strcmp("m_socket", argv[pos])) || !pos) {
      printf("m_socket        %s\n", sge_dstring_get_string(&msocket));
   }
   if ((pos && !strcmp("m_core", argv[pos])) || !pos) {
      printf("m_core          %s\n", sge_dstring_get_string(&mcore));
   }
   if ((pos && !strcmp("m_thread", argv[pos])) || !pos) {
      printf("m_thread        %s\n", sge_dstring_get_string(&mthread));
   }
   if ((pos && !strcmp("m_topology", argv[pos])) || !pos) {
      printf("m_topology      %s\n", sge_dstring_get_string(&mtopology));
   }   
#else 
   if ((pos && !strcmp("m_socket", argv[pos])) || !pos) {
      printf("m_socket        -\n");
   }
   if ((pos && !strcmp("m_core", argv[pos])) || !pos) {
      printf("m_core          -\n");
   }
   if ((pos && !strcmp("m_thread", argv[pos])) || !pos) {
      printf("m_thread        -\n");
   }
   if ((pos && !strcmp("m_topology", argv[pos])) || !pos) {
      printf("m_topology      -\n");
   }   
#endif 

	loads = sge_getloadavg(avg, 3);

   if (loads>0 && ((pos && !strcmp("load_short", argv[pos])) || !pos)) {
      printf("load_short      %.2f\n", avg[0]);
   }
   if (loads>1 && ((pos && !strcmp("load_medium", argv[pos])) || !pos)) {
      printf("load_medium     %.2f\n", avg[1]);
   }
   if (loads>2 && ((pos && !strcmp("load_long", argv[pos])) || !pos)) {
      printf("load_long       %.2f\n", avg[2]);
   }
      
   if (pos) {
      name = argv[pos];
   } else {
      name = nullptr;
   }

#ifdef SGE_LOADMEM
   /* memory load report */
   memset(&mem_info, 0, sizeof(sge_mem_info_t));
   if (sge_loadmem(&mem_info)) {
      fprintf(stderr, "%s\n", MSG_SYSTEM_RETMEMORYINDICESFAILED);
#if defined(OGE_HWLOC)
      sge_dstring_free(&mcore);
      sge_dstring_free(&msocket);
      sge_dstring_free(&mthread);
      sge_dstring_free(&mtopology);
#endif
      DRETURN(1);
   }

   print_mem_load(LOAD_ATTR_MEM_FREE, name, precision, mem_info.mem_free, m); 
   print_mem_load(LOAD_ATTR_SWAP_FREE, name, precision, mem_info.swap_free, m); 
   print_mem_load(LOAD_ATTR_VIRTUAL_FREE, name, precision, mem_info.mem_free  + mem_info.swap_free, m); 

   print_mem_load(LOAD_ATTR_MEM_TOTAL, name, precision, mem_info.mem_total, m); 
   print_mem_load(LOAD_ATTR_SWAP_TOTAL, name, precision, mem_info.swap_total, m); 
   print_mem_load(LOAD_ATTR_VIRTUAL_TOTAL, name, precision, mem_info.mem_total + mem_info.swap_total, m);

   print_mem_load(LOAD_ATTR_MEM_USED, name, precision, mem_info.mem_total - mem_info.mem_free, m); 
   print_mem_load(LOAD_ATTR_SWAP_USED, name, precision, mem_info.swap_total - mem_info.swap_free, m); 
   print_mem_load(LOAD_ATTR_VIRTUAL_USED, name, precision,(mem_info.mem_total + mem_info.swap_total) - 
                                          (mem_info.mem_free  + mem_info.swap_free), m); 
#endif /* SGE_LOADMEM */

#ifdef SGE_LOADCPU
   loads = sge_getcpuload(&total);
   sleep(1);
   loads = sge_getcpuload(&total);

   if (loads != -1) {
      print_mem_load("cpu", name,  1, total, "%");
   }
#endif /* SGE_LOADCPU */

#if defined(OGE_HWLOC)
   sge_dstring_free(&mcore);
   sge_dstring_free(&msocket);
   sge_dstring_free(&mthread);
   sge_dstring_free(&mtopology);
#endif
	return 0;
}

void print_mem_load(char *name, char *thisone, int precision, double value, char *m) {
   if ((thisone && !strcmp(name, thisone)) || !thisone)
      printf("%-15s %.*f%s\n", name, precision, value, m);
}

/****** loadcheck/check_core_binding() *****************************************
*  NAME
*     check_core_binding() -- Checks core binding functionality on current host. 
*
*  SYNOPSIS
*     void check_core_binding() 
*
*  FUNCTION
*     Checks core binding functionality on current host. 
*
*  INPUTS
*
*  RESULT
*     void - No result
*
*******************************************************************************/
void check_core_binding()
{
   /* try if it is possible to use hwloc in case of Linux */
#if defined(OGE_HWLOC)
      printf("Your OGE version has built-in core binding functionality!\n");
      test_hwloc();
#else
      printf("Your OGE does currently not support core binding on this platform!\n");
#endif
}

#if defined(OGE_HWLOC)
void test_hwloc()
{
   char* topology = nullptr;
   int length     = 0;
   int s, c;
   struct utsname name;

   if (uname(&name) != -1) {
      printf("Your " SFN  " kernel version is: %s\n", name.sysname, name.release);
   }

   if (!oge::topo_has_core_binding()) {
      printf("Your Linux kernel seems not to offer core binding capabilities for HWLOC!\n");
   }

   if (!oge::topo_has_topology_information()) {
      printf("No topology information could by retrieved by HWLOC!\n");
   } else {
      /* get amount of sockets */
      printf("Amount of sockets:\t\t%d\n", oge::topo_get_total_amount_of_sockets());
      /* get amount of cores   */
      printf("Amount of cores:\t\t%d\n", oge::topo_get_total_amount_of_cores());
      /* the amount of threads must be shown as well */
      printf("Amount of threads:\t\t%d\n", oge::topo_get_total_amount_of_threads());
      /* get topology */
      oge::topo_get_topology(&topology, &length);
      printf("Topology:\t\t\t%s\n", topology);
      sge_free(&topology); 
      printf("Mapping of logical socket and core numbers to internal\n");

      /* for each socket,core pair get the internal processor number */
      /* try multi-mapping */
      for (s = 0; s < oge::topo_get_total_amount_of_sockets(); s++) {
         for (c = 0; c < oge::topo_get_amount_of_cores_for_socket(s); c++) {
            int* proc_ids  = nullptr;
            int amount     = 0;
            if (oge::topo_get_processor_ids(s, c, &proc_ids, &amount)) {
               printf("Internal processor ids for socket %5d core %5d: ", s , c);
               for (int i = 0; i < amount; i++) {
                  printf(" %5d", proc_ids[i]);
               }
               printf("\n");
               sge_free(&proc_ids);
            } else {
               printf("Couldn't get processor ids for socket %5d core %5d\n", s, c);
            }
         }
      }
   }   
}
#endif

#if defined(OGE_HWLOC)
/****** loadcheck/fill_socket_core_topology() **********************************
*  NAME
*     fill_socket_core_topology() -- Get load values regarding processor topology. 
*
*  SYNOPSIS
*     void fill_socket_core_topology(dstring* msocket, dstring* mcore, dstring* 
*     mtopology) 
*
*  FUNCTION
*     Gets the values regarding processor topology. 
*
*  OUTPUTS 
*     dstring* msocket   - The amount of sockets the host have. 
*     dstring* mcore     - The amount of cores the host have.
*     dstring* mtopology - The topology the host have. 
*
*  RESULT
*     void - nothing 
*
*******************************************************************************/
void fill_socket_core_topology(dstring* msocket, dstring* mcore, dstring* mthread, dstring* mtopology)
{
   int ms, mc, mt;
   char* topo = nullptr;
   int length = 0;

   ms = get_execd_amount_of_sockets();
   mc = get_execd_amount_of_cores();
   mt = get_execd_amount_of_threads();
   if (!get_execd_topology(&topo, &length) || topo == nullptr) {
      topo = sge_strdup(nullptr, "-");
   }
   sge_dstring_sprintf(msocket, "%d", ms);
   sge_dstring_sprintf(mcore, "%d", mc);
   sge_dstring_sprintf(mthread, "%d", mt);
   sge_dstring_append(mtopology, topo);
   sge_free(&topo);
}

#endif
