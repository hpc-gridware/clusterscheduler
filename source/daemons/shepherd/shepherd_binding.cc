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
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this code are Copyright 2011 Univa Inc.
 * 
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "basis_types.h"
#include "uti/config_file.h"


#include "uti/sge_binding_hlp.h"
#include "uti/sge_dstring.h"
#include "uti/sge_string.h"
#include "uti/oge_topology.h"

#include "shepherd_binding.h"
#include "err_trace.h"

#if defined(BINDING_SOLARIS)
#  include "sge_uidgid.h"
#  include <sys/pset.h>
#endif

namespace oge {
#if defined(OGE_HWLOC)

   static bool binding_set_linear_linux(int first_socket, int first_core,
                                        int amount_of_cores, int offset, const binding_type_t type);

   static bool binding_set_striding_linux(int first_socket, int first_core,
                                          int amount_of_cores, int offset, int n, const binding_type_t type);

   static bool bind_process_to_mask(hwloc_const_bitmap_t cpuset);

   static bool binding_explicit(const int *list_of_sockets, const int samount,
                                const int *list_of_cores, const int camount, const binding_type_t type);

   static bool create_binding_env_linux(hwloc_const_bitmap_t cpuset);
#endif

#if defined(BINDING_SOLARIS)
   static bool bind_shepherd_to_pset(int pset_id);
#endif

#if defined(OGE_HWLOC) && !defined(SOLARIS)

/****** shepherd_binding/do_core_binding() *************************************
*  NAME
*     do_core_binding() -- Performs the core binding task for the Linux OS. 
*
*  SYNOPSIS
*     int do_core_binding(void) 
*
*  FUNCTION
*     Performs core binding on shepherd side. All information required for  
*     the binding is communicated from execd to shepherd in the config 
*     file value "binding". If there is "nullptr" no core binding is done.
* 
*     This function is Linux specific.
*
*     If there is any instruction the bookkeeping for these cores is already 
*     done. In case of Solaris the processor set is already created by 
*     execution daemon. Hence shepherd has just to add itself to it.
*     In case of Linux the whole binding is done by shepherd. In each case 
*     the binding is inherited from shepherd to the job it starts.
*
*     DG TODO change return value to bool
* 
*  RESULT
*     int - Returns 0 in case of success and a negative value in case of problems. 
*
*  NOTES
*     MT-NOTE: do_core_binding() is not MT safe 
*
*******************************************************************************/
   int do_core_binding(void) {
      /* Check if "binding" parameter in 'config' file
       * is available and not set to "binding=no_job_binding".
       * If so, we do an early abortion.
       */
      char *binding = get_conf_val("binding");
      binding_type_t type;

      if (binding == nullptr || strcasecmp("nullptr", binding) == 0) {
         shepherd_trace("do_core_binding: \"binding\" parameter not found in config file");
         return -1;
      }

      if (strcasecmp("no_job_binding", binding) == 0) {
         shepherd_trace("do_core_binding: skip binding - no core binding configured");
         return -1;
      }

      /* get the binding type (set = 0 | env = 1 | pe = 2) where default is 0 */
      type = binding_parse_type(binding);

      /* do a binding accorting the strategy */
      if (strstr(binding, "linear") != nullptr) {
         /* do a linear binding */
         int amount;
         int socket;
         int core;

         shepherd_trace("do_core_binding: do linear");

         /* get the amount of cores to bind on */
         if ((amount = binding_linear_parse_amount(binding)) < 0) {
            shepherd_trace("do_core_binding: couldn't parse the amount of cores from config file");
            return -1;
         }

         /* get the socket to begin binding with (choosen by execution daemon) */
         if ((socket = binding_linear_parse_socket_offset(binding)) < 0) {
            shepherd_trace("do_core_binding: couldn't get the socket number from config file");
            return -1;
         }

         /* get the core to begin binding with (choosen by execution daemon)   */
         if ((core = binding_linear_parse_core_offset(binding)) < 0) {
            shepherd_trace("do_core_binding: couldn't get the core number from config file");
            return -1;
         }

         /* perform core binding on current process */
         if (binding_set_linear_linux(socket, core, amount, 1, type) == false) {
            /* core binding was not successful */
            if (type == BINDING_TYPE_SET) {
               shepherd_trace("do_core_binding: linear binding was not successful");
            } else if (type == BINDING_TYPE_ENV) {
               shepherd_trace("do_core_binding: couldn't set SGE_BINDING environment variable");
            } else if (type == BINDING_TYPE_PE) {
               shepherd_trace("do_core_binding: couldn't produce rankfile");
            }
         } else {
            if (type == BINDING_TYPE_SET) {
               shepherd_trace("do_core_binding: job successfully bound");
            } else if (type == BINDING_TYPE_ENV) {
               shepherd_trace("do_core_binding: SGE_BINDING environment variable created");
            } else if (type == BINDING_TYPE_PE) {
               shepherd_trace("do_core_binding: rankefile produced");
            }
         }

      } else if (strstr(binding, "striding") != nullptr) {
         int amount = binding_striding_parse_amount(binding);
         int stepsize = binding_striding_parse_step_size(binding);

         /* these are the real start parameters */
         int first_socket = 0, first_core = 0;

         shepherd_trace("do_core_binding: striding");

         if (amount <= 0) {
            shepherd_trace("do_core_binding: error parsing <amount>");
            return -1;
         }

         if (stepsize < 0) {
            shepherd_trace("do_core_binding: error parsing <stepsize>");
            return -1;
         }

         first_socket = binding_striding_parse_first_socket(binding);
         if (first_socket < 0) {
            shepherd_trace("do_core_binding: error parsing <socket>");
            return -1;
         }

         first_core = binding_striding_parse_first_core(binding);
         if (first_core < 0) {
            shepherd_trace("do_core_binding: error parsing <core>");
            return -1;
         }

         /* last core has to be incremented because core 0 is first core to be used */
         if (stepsize == 0) {
            /* stepsize must be >= 1 */
            stepsize = 1;
         }

         shepherd_trace("do_core_binding: striding set binding: first_core: %d first_socket %d amount %d stepsize %d",
                        first_core, first_socket, amount, stepsize);

         /* get the first core and first socket which is available for striding    */

         /* perform core binding on current process                */

         if (binding_set_striding_linux(first_socket, first_core, amount, 0, stepsize, type)) {
            shepherd_trace("do_core_binding: striding: binding done");
         } else {
            shepherd_trace("do_core_binding: striding: binding not done");
         }

      } else if (strstr(binding, "explicit") != nullptr) {

         /* list with the sockets (first part of the <socket>,<core> tuples) */
         int *sockets = nullptr;
         /* length of sockets list */
         int nr_of_sockets = 0;
         /* list with the cores to be bound on the sockets */
         int *cores = nullptr;
         /* length of cores list */
         int nr_of_cores = 0;

         shepherd_trace("do_core_binding: explicit");

         /* get <socket>,<core> pairs out of binding string */
         if (binding_explicit_extract_sockets_cores(binding, &sockets, &nr_of_sockets,
                                                    &cores, &nr_of_cores) == true) {

            if (nr_of_sockets == 0 && nr_of_cores == 0) {
               /* no cores and no sockets are found */
               shepherd_trace("do_core_binding: explicit: no socket or no core was specified");
            } else if (nr_of_sockets != nr_of_cores) {
               shepherd_trace("do_core_binding: explicit: unequal amount of specified sockets and cores");
            } else {
               /* do core binding according the <socket>,<core> tuples */
               if (binding_explicit(sockets, nr_of_sockets, cores, nr_of_cores, type) == true) {
                  shepherd_trace("do_core_binding: explicit: binding done");
               } else {
                  shepherd_trace("do_core_binding: explicit: no core binding done");
               }
            }

            sge_free(&sockets);
            sge_free(&cores);

         } else {
            sge_free(&sockets);
            sge_free(&cores);
            shepherd_trace("do_core_binding: explicit: couldn't extract <socket>,<core> pair");
         }

      } else {

         if (binding != nullptr) {
            shepherd_trace("do_core_binding: WARNING: unknown \"binding\" parameter: %s",
                           binding);
         } else {
            shepherd_trace("do_core_binding: WARNING: binding was null!");
         }

      }

      shepherd_trace("do_core_binding: finishing");

      return 0;
   }

#endif

#if defined(BINDING_SOLARIS)
   /****** shepherd_binding/do_core_binding() ******************
   *******************
   *  NAME
   *     do_core_binding() -- Performs the core binding task for the Solaris OS.
   *
   *  SYNOPSIS
   *     int do_core_binding(void)
   *
   *  FUNCTION
   *     Performs core binding on shepherd side. All information required for
   *     the binding is communicated from execd to shepherd in the config
   *     file value "binding". If there is "nullptr" no core binding is done.
   *
   *     This function is Solaris specific.
   *
   *     DG TODO change return value to bool
   *
   *  RESULT
   *     int - Returns 0 in case of success and a negative value in case of problems.
   *
   *  NOTES
   *     MT-NOTE: do_core_binding() is not MT safe
   *
   *******************************************************************************/
   int do_core_binding(void)
   {
      int retval = 0;

      /* just read out what is in "config" file and attach to the given psrset if
         it is specified */
      char *binding = get_conf_val("binding");

      if (binding == nullptr) {
         shepherd_trace("do_core_binding: \"binding\" parameter not found in config file");
         retval = -1;
      } else if (strcasecmp("no_job_binding", binding) == 0 || strcasecmp("nullptr", binding) == 0) {
         shepherd_trace("do_core_binding: skip binding - no core binding configured");
         retval = -1;
      }

      if (retval == 0 && strstr(binding, "psrset:") != nullptr) {
         int processor_set_id = 0;
         shepherd_trace("do_core_binding: psrset found - attaching to it!");

         /* parse the psrset number right after "psrset:" */
         if (sge_strtok(binding, ":") != nullptr) {
            /* parse the rest of the line */
            char* pset_id;
            if ((pset_id = sge_strtok(nullptr, ":")) != nullptr) {
               /* finally get the processor set id */
               processor_set_id = atoi(pset_id);
            } else {
               shepherd_trace("do_core_binding: couldn't find the psrset id after \"psrset:\" in config file (binding)");
               retval = -1;
            }
         } else {
            shepherd_trace("do_core_binding: found string \"psrset:\" but no \":\" - almost impossible");
            retval = -1;
         }

         if (retval == 0) {
            if (processor_set_id == -1) {
               /* prcoessor_set_id == -1: Check here for a special processor_set_id (negative; 0)
                  which does show that no binding is needed since this processor set
                  would require (exactly) all of the remaining cores. Creating
                  such a processor set is not possible because one processor must
                  left for the OS. But the job is implicitly bound to the processors
                  since it can not use any onther processor from the other processor
                  sets. */
               shepherd_trace("do_core_binding: psrset not created since all remaining processors would be used");
               shepherd_trace("do_core_binding: binding is done implicitly");
            } else {
               /* start user rights (root) are required for creating processor sets */
               sge_switch2start_user();

               if (bind_shepherd_to_pset(processor_set_id) == false) {
                  shepherd_trace("do_core_binding: couldn't bind to existing processor set!");
               } else {
                  shepherd_trace("do_core_binding: successfully bound to existing processor set!");
               }

               /* switch back to admin user */
               sge_switch2admin_user();
            }
         }

      } else {  /* "psrset" is not in config file defined */
         shepherd_trace("do_core_binding: no processor set found in config file! do nothing");
         retval = -1;
      }

      shepherd_trace("do_core_binding: finishing");

      return retval;
   }

   /****** shepherd_binding/bind_shepherd_to_pset() *******************************
   *  NAME
   *     bind_shepherd_to_pset() -- Binds the current process to a processor set.
   *
   *  SYNOPSIS
   *     static bool bind_shepherd_to_pset(int pset_id)
   *
   *  FUNCTION
   *     Binds the current shepherd process to an existing processor set.
   *
   *  INPUTS
   *     int pset_id - Existing processor set id.
   *
   *  RESULT
   *     static bool - true in case the process was bound false otherwise
   *
   *  NOTES
   *     MT-NOTE: bind_shepherd_to_pset() is MT safe
   *
   *******************************************************************************/
   static bool bind_shepherd_to_pset(int pset_id)
   {
      /* try to bind current process to processor set */
      if (pset_bind((psetid_t)pset_id, P_PID, P_MYID, nullptr) != 0) {
         /* binding was not successful */
         return false;
      }

      /* successfully bound current process to processor set */
      return true;
   }
#endif


/* helper for core_binding */

#if defined(OGE_HWLOC)

bool binding_add_core_to_cpuset(hwloc_bitmap_t cpuset, int socket, int core) {
   bool ret = false;
   hwloc_obj_t current_core = hwloc_get_obj_below_by_type(topo_get_hwloc_topology(),
                                              HWLOC_OBJ_SOCKET, socket,
                                              HWLOC_OBJ_CORE, core);
   if (current_core != nullptr) {
      hwloc_bitmap_or(cpuset, cpuset, current_core->cpuset);
      ret = true;
   }

   return ret;
}

/****** shepherd_binding/binding_set_linear_linux() ***************************************
*  NAME
*     binding_set_linear_linux() -- Bind current process linear to chunk of cores. 
*
*  SYNOPSIS
*     bool binding_set_linear(int first_socket, int first_core, int 
*     amount_of_cores, int offset) 
*
*  FUNCTION
*     Binds current process (shepherd) to a set of cores. All processes 
*     started by the current process are inheriting the core binding (Linux).
*     
*     The core binding is done in a linear manner, that means that 
*     the process is bound to 'amount_of_cores' cores using one core 
*     after another starting at socket 'first_socket' (usually 0) and 
*     core = 'first_core' (usually 0) + 'offset'. If the core number 
*     is higher than the number of cores which are provided by socket 
*     'first_socket' then the next socket is taken (the core number 
*      defines how many cores are skiped).
*
*  INPUTS
*     int first_socket    - The first socket (starting at 0) to bind to. 
*     int first_core      - The first core to bind. 
*     int amount_of_cores - The amount of cores to bind to. 
*     int offset          - The user specified core number offset. 
*     binding_type_t type - The type of binding ONLY FOR EXECD ( set | env | pe )
*                           
*  RESULT
*     bool - true if binding for current process was done, false if not
*
*  NOTES
*     MT-NOTE: binding_set_linear() is not MT safe 
*
*******************************************************************************/
   static bool binding_set_linear_linux(int first_socket, int first_core,
                                        int amount_of_cores, int offset, const binding_type_t type) {

      /* sets bitmask in a linear manner        */
      /* first core is on exclusive host 0      */
      /* first core could be set from scheduler */
      /* offset is the first core to start with (make sense only with exclusive host) */

      if (topo_has_core_binding()) {
         /* bitmask for processors to turn on and off */
         hwloc_bitmap_t cpuset = hwloc_bitmap_alloc();

         if (topo_has_topology_information()) {
            /* next socket to use */
            int next_socket = first_socket;
            /* the amount of cores of the next socket */
            int socket_amount_of_cores;
            /* next core to use */
            int next_core = first_core + offset;
            /* maximal amount of sockets on this system */
            int max_amount_of_sockets = topo_get_total_amount_of_sockets();

            /* strategy: go to the first_socket and the first_core + offset and
               fill up socket and go to the next one. */

            /* TODO maybe better to search for using a core exclusively? */

            while (topo_get_amount_of_cores_for_socket(next_socket) <= next_core) {
               /* TODO which kind of warning when first socket does not offer this? */
               /* move on to next socket - could be that we have to deal only with cores
                  instead of <socket><core> tuples */
               next_core -= topo_get_amount_of_cores_for_socket(next_socket);
               next_socket++;
               if (next_socket >= max_amount_of_sockets) {
                  /* we are out of sockets - we do nothing */
                  hwloc_bitmap_free(cpuset);
                  return false;
               }
            }
            // add current core to cpuset
            if (!binding_add_core_to_cpuset(cpuset, next_socket, next_core)) {
               hwloc_bitmap_free(cpuset);
               return false;
            }

            /* collect the other processo r ids with the strategy */
            for (int cores_set = 1; cores_set < amount_of_cores; ++cores_set) {
               next_core++;
               /* jump to next socket when it is needed */
               /* maybe the next socket could offer 0 cores (I can' see when,
                  but just to be sure) */
               while ((socket_amount_of_cores = topo_get_amount_of_cores_for_socket(next_socket))
                      <= next_core) {
                  next_socket++;
                  next_core = next_core - socket_amount_of_cores;
                  if (next_socket >= max_amount_of_sockets) {
                     /* we are out of sockets - we do nothing */
                     hwloc_bitmap_free(cpuset);
                     return false;
                  }
               }
               // add current core to cpuset
               if (!binding_add_core_to_cpuset(cpuset, next_socket, next_core)) {
                  hwloc_bitmap_free(cpuset);
                  return false;
               }
            }

            /* check what to do with the processor ids (set, env or pe) */
            if (type == BINDING_TYPE_PE) {

               /* is done outside */

            } else if (type == BINDING_TYPE_ENV) {

               /* set the environment variable                    */
               /* this does not show up in "environment" file !!! */
               if (create_binding_env_linux(cpuset)) {
                  shepherd_trace("binding_set_linear_linux: SGE_BINDING env var created");
               } else {
                  shepherd_trace("binding_set_linear_linux: problems while creating SGE_BINDING env");
               }

            } else {

               /* bind SET process to mask */
               if (!bind_process_to_mask(cpuset)) {
                  /* there was an error while binding */
                  hwloc_bitmap_free(cpuset);
                  return false;
               }
            }
         } else {

            /* TODO DG strategy without topology information but with
               working library? */
            shepherd_trace("binding_set_linear_linux: no information about topology");
            return false;
         }

         hwloc_bitmap_free(cpuset);
      } else {
         shepherd_trace("binding_set_linear_linux: HWLOC binding not supported");
      }

      return true;
   }

/****** shepherd_binding/binding_set_striding_linux() *************************************
*  NAME
*     binding_set_striding_linux() -- Binds current process to cores.  
*
*  SYNOPSIS
*     bool binding_set_striding_linux(int first_socket, int first_core, int 
*     amount_of_cores, int offset, int stepsize) 
*
*  FUNCTION
*     Performs a core binding for the calling process according to the 
*     'striding' strategy. The first core used is specified by first_socket
*     (beginning with 0) and first_core (beginning with 0). If first_core is 
*     greater than available cores on first_socket, the next socket is examined 
*     and first_core is reduced by the skipped cores. If the first_core could 
*     not be found on system (because it was to high) no binding will be done.
*     
*     If the first core was choosen the next one is defined by the step size 'n' 
*     which is incremented to the first core found. If the socket has not the 
*     core (because it was the last core of the socket for example) the next 
*     socket is examined.
*
*     If the system is out of cores and there are still some cores to select 
*     (because of the amount_of_cores parameter) no core binding will be performed.
*    
*  INPUTS
*     int first_socket    - first socket to begin with  
*     int first_core      - first core to start with  
*     int amount_of_cores - total amount of cores to be used 
*     int offset          - core offset for first core (increments first core used) 
*     int stepsize        - step size
*     int type            - type of binding (set or env or pe)
*
*  RESULT
*     bool - Returns true if the binding was performed, otherwise false.
*
*  NOTES
*     MT-NOTE: binding_set_striding() is MT safe 
*
*******************************************************************************/
   bool binding_set_striding_linux(int first_socket, int first_core, int amount_of_cores,
                                   int offset, int stepsize, const binding_type_t type) {
      /* n := take every n-th core */
      bool bound = false;

      if (topo_has_core_binding() == true) {
         /* bitmask for processors to turn on and off */
         hwloc_bitmap_t cpuset = hwloc_bitmap_alloc();

         /* when library offers architecture: 
            - get virtual processor ids in the following manner:
              * on socket "first_socket" choose core number "first_core + offset"
              * then add n: if core is not available go to next socket
              * ...
         */
         if (topo_has_topology_information()) {
            /* amount of cores set in processor binding mask */
            int cores_set = 0;
            /* next socket to use */
            int next_socket = first_socket;
            /* next core to use */
            int next_core = first_core + offset;
            /* maximal amount of sockets on this system */
            int max_amount_of_sockets = topo_get_total_amount_of_sockets();

            /* check if we are already out of range */
            if (next_socket >= max_amount_of_sockets) {
               shepherd_trace("binding_set_striding_linux: already out of sockets");
               hwloc_bitmap_free(cpuset);
               return false;
            }

            while (topo_get_amount_of_cores_for_socket(next_socket) <= next_core) {
               /* move on to next socket - could be that we have to deal only with cores 
                  instead of <socket><core> tuples */
               next_core -= topo_get_amount_of_cores_for_socket(next_socket);
               next_socket++;
               if (next_socket >= max_amount_of_sockets) {
                  /* we are out of sockets - we do nothing */
                  shepherd_trace("binding_set_striding_linux: first core: out of sockets");
                  hwloc_bitmap_free(cpuset);
                  return false;
               }
            }

            // add current core to cpuset
            if (!binding_add_core_to_cpuset(cpuset, next_socket, next_core)) {
               hwloc_bitmap_free(cpuset);
               return false;
            }

            /* collect the rest of the processor ids */
            for (cores_set = 1; cores_set < amount_of_cores; cores_set++) {
               /* calculate next_core number */
               next_core += stepsize;

               /* check if we are already out of range */
               if (next_socket >= max_amount_of_sockets) {
                  shepherd_trace("binding_set_striding_linux: out of sockets");
                  hwloc_bitmap_free(cpuset);
                  return false;
               }

               while (topo_get_amount_of_cores_for_socket(next_socket) <= next_core) {
                  /* move on to next socket - could be that we have to deal only with cores 
                     instead of <socket><core> tuples */
                  next_core -= topo_get_amount_of_cores_for_socket(next_socket);
                  next_socket++;
                  if (next_socket >= max_amount_of_sockets) {
                     /* we are out of sockets - we do nothing */
                     shepherd_trace("binding_set_striding_linux: out of sockets!");
                     hwloc_bitmap_free(cpuset);
                     return false;
                  }
               }

               // add current core to cpuset
               if (!binding_add_core_to_cpuset(cpuset, next_socket, next_core)) {
                  hwloc_bitmap_free(cpuset);
                  return false;
               }
            } /* collecting processor ids */

            if (type == BINDING_TYPE_PE) {

               /* rankfile is created: do nothing */

            } else if (type == BINDING_TYPE_ENV) {
               /* set the environment variable */
               if (create_binding_env_linux(cpuset)) {
                  shepherd_trace("binding_set_striding_linux: SGE_BINDING env var created");
               } else {
                  shepherd_trace("binding_set_striding_linux: problems while creating SGE_BINDING env");
               }
            } else {
               /* bind process to mask */
               if (bind_process_to_mask(cpuset)) {
                  /* there was an error while binding */
                  bound = true;
               }
            }
         } else {
            /* setting bitmask without topology information which could 
               not be right? */
            shepherd_trace("binding_set_striding_linux: bitmask without topology information");
            return false;
         }
         hwloc_bitmap_free(cpuset);
      } else {
         /* has no core binding feature */
         return false;
      }


      return bound;
   }

/****** shepherd_binding/bind_process_to_mask() *************************************
*  NAME
*     bind_process_to_mask() -- Binds a process to a given cpuset (mask). 
*
*  SYNOPSIS
*     static bool bind_process_to_mask(const hwloc_bitmap_t  cpuset)
*
*  FUNCTION
*     Binds a process to a given cpuset. 
*
*  INPUTS
*     const plpa_cpu_set_t cpuset - Processors to bind processes to
*
*  RESULT
*     static bool - true if successful, false otherwise
*
*  NOTES
*     MT-NOTE: bind_process_to_mask() is not MT safe 
*
*******************************************************************************/
   static bool bind_process_to_mask(hwloc_const_bitmap_t cpuset) {
      bool ret = false;

      if (topo_has_core_binding()) {
         // we only need core binding capabilites, no topology is required
         hwloc_topology_t topology = topo_get_hwloc_topology();
         if (hwloc_set_cpubind(topology, cpuset, HWLOC_CPUBIND_STRICT) == 0) {
            shepherd_trace("strict cpu binding succeeded");
            ret = true;
         } else {
            shepherd_trace("strict cpu binding is not available");
            if (hwloc_set_cpubind(topology, cpuset, 0) == 0) {
               shepherd_trace("non strict cpu binding succeeded");
               return true;
            } else {
               // @todo do we somehow get an error description?
               shepherd_trace("cpu binding failed");
            }
         }
      }

      return ret;
   }

/****** shepherd_binding/binding_explicit() *****************************************
*  NAME
*     binding_explicit() -- Binds current process to specified CPU cores. 
*
*  SYNOPSIS
*     bool binding_explicit(int* list_of_cores, int camount, int* 
*     list_of_sockets, int samount) 
*
*  FUNCTION
*     Binds the current process to the cores specified by a <socket>,<core>
*     tuple. The tuple is given by a list of sockets and a list of cores. 
*     The elements on the same position of these lists are reflecting 
*     a tuple. Therefore the length of the lists must be the same.
*
*     Binding is currently done on Linux hosts only where the machine topology 
*     can be retrieved with HWLOC library. It also does require this library.
*
*  INPUTS
*     int* list_of_sockets - List of sockets in the same order as list of cores. 
*     int samount          - Length of the list of sockets. 
*     int* list_of_cores   - List of cores in the same order as list of sockets. 
*     int camount          - Length of the list of cores. 
*     int type             - Type of binding ( set | env | pe ).
*
*  RESULT
*     bool - true when the current process was bound like specified with the 
*            input parameter
*
*  NOTES
*     MT-NOTE: binding_explicit() is not MT safe 
*
*******************************************************************************/
   static bool binding_explicit(const int *list_of_sockets, const int samount,
                                const int *list_of_cores, const int camount, const binding_type_t type) {
      /* return value: successful bound or not */
      bool bound = false;

      /* check if we have exactly the same amount of sockets as cores */
      if (camount != samount) {
         shepherd_trace("binding_explicit: bug: amount of sockets != amount of cores");
         return false;
      }

      if (list_of_sockets == nullptr || list_of_cores == nullptr) {
         shepherd_trace("binding_explicit: wrong input values");
      }

      /* do only on linux when we have core binding feature in kernel */
      if (topo_has_core_binding() == true) {

         if (topo_has_topology_information()) {
            /* bitmask for processors to turn on and off */
            hwloc_bitmap_t cpuset = hwloc_bitmap_alloc();
            /* processor id counter */
            int pr_id_ctr;

            /* Fetch for each socket,core tuple the processor id.
               If this is not possible for one do nothing and return false. */

            /* go through all socket,core tuples and get the processor id */
            for (pr_id_ctr = 0; pr_id_ctr < camount; pr_id_ctr++) {
               /* get the processor id */
               /* get the OS internal processor ids */
               // add current core to cpuset
               if (!binding_add_core_to_cpuset(cpuset, list_of_sockets[pr_id_ctr], list_of_cores[pr_id_ctr])) {
                  hwloc_bitmap_free(cpuset);
                  return false;
               }
            }

            if (type == BINDING_TYPE_PE) {

               /* rankfile is created */

            } else if (type == BINDING_TYPE_ENV) {
               /* set the environment variable */
               if (create_binding_env_linux(cpuset)) {
                  shepherd_trace("binding_explicit: SGE_BINDING env var created");
               } else {
                  shepherd_trace("binding_explicit: problems while creating SGE_BINDING env");
               }
            } else {
               /* do the core binding for the current process with the mask */
               if (bind_process_to_mask(cpuset)) {
                  bound = true;
               } else {
                  /* couldn't be bound return false */
                  shepherd_trace("binding_explicit: bind_process_to_mask was not successful");
               }
            }

            hwloc_bitmap_free(cpuset);
         } else {
            /* has no topology information */
            shepherd_trace("binding_explicit: Linux does not offer topology information");
         }

      } else {
         /* has no core binding ability */
         shepherd_trace("binding_explicit: host does not support core binding");
      }

      return bound;
   }

/****** shepherd_binding/create_binding_env_linux() ****************************
*  NAME
*     create_binding_env_linux() -- Creates SGE_BINDING env variable. 
*
*  SYNOPSIS
*     bool create_binding_env_linux(const int* proc_id, const int amount) 
*
*  FUNCTION
*     Creates the SGE_BINDING environment variable on Linux operating system. 
*     This environment variable contains a space separated list of Linux 
*     internal processor ids given as input parameter.
*
*  INPUTS
*     const int* proc_id - List of processor ids. 
*     const int amount   - Length of processor id list. 
*
*  RESULT
*     bool - true when SGE_BINDING env var could be generated false if not
*
*  NOTES
*     MT-NOTE: create_binding_env_linux() is MT safe 
*
*******************************************************************************/
   bool create_binding_env_linux(hwloc_const_bitmap_t cpuset) {
      bool ret = true;
      std::string str_binding;
      unsigned i;
      bool first = true;

      hwloc_bitmap_foreach_begin(i, cpuset)
         if (first) {
           first = false;
           str_binding += std::to_string(i);
         } else {
            str_binding += " ";
            str_binding += std::to_string(i);
         }
      hwloc_bitmap_foreach_end();

      shepherd_trace("setting SGE_BINDING to %s", str_binding.c_str());
      if (sge_setenv("SGE_BINDING", str_binding.c_str()) != 1) {
         /* setting env var was not successful */
         ret = false;
         shepherd_trace("create_binding_env_linux: Couldn't set environment variable!");
      }

      return ret;
   }

#endif
}

