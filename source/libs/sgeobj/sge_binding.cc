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
 *   Portions of this code are Copyright 2011 Univa Inc.
 * 
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "uti/sge_binding_hlp.h"

#include "uti/sge_rmon.h"
#include "uti/oge_topology.h"

#include "sgeobj/sge_binding.h" 
#include "sgeobj/sge_answer.h"

#include "msg_common.h"

#define BINDING_LAYER TOP_LAYER

/* 
 * these sockets cores or threads are currently in use from SGE 
 * access them via getExecdTopologyInUse() because of initialization 
 */
static char* logical_used_topology = nullptr;

static int logical_used_topology_length = 0;

#if defined(OGE_HWLOC)

/* creates a string with the topology used from a single job */
static bool create_topology_used_per_job(char** accounted_topology, 
               int* accounted_topology_length, char* logical_used_topology, 
               char* used_topo_with_job, int logical_used_topology_length);

static bool get_free_sockets(const char* topology, const int topology_length, 
               int** sockets, int* sockets_size);

static int account_cores_on_socket(char** topology, const int topology_length,
               const int socket_number, const int cores_needed, int** list_of_sockets,
               int* list_of_sockets_size, int** list_of_cores, int* list_of_cores_size);

static bool get_socket_with_most_free_cores(const char* topology, const int topology_length, 
               int* socket_number);

static bool account_all_threads_after_core(char** topology, const int core_pos);

#endif

/* arch independent functions */

/****** sge_binding/get_execd_amount_of_cores() ************************************
*  NAME
*     get_execd_amount_of_threads() -- Returns the amount of hw supported threads. 
*
*  SYNOPSIS
*     int get_execd_amount_of_threads() 
*
*  FUNCTION
*     Retrieves the amount of hardware supported threads 
*     the current execution host offers.
*
*  RESULT
*     int - The amount of threads the current host has. 
*
*  NOTES
*     MT-NOTE: get_execd_amount_of_threads() is MT safe 
*
*******************************************************************************/
int get_execd_amount_of_threads() {
#if defined(OGE_HWLOC)
      return oge::topo_get_total_amount_of_threads();
#else
      return 0;
#endif  
}

/****** sge_binding/get_execd_amount_of_cores() ************************************
*  NAME
*     get_execd_amount_of_cores() -- Returns the total amount of cores the host has. 
*
*  SYNOPSIS
*     int get_execd_amount_of_cores() 
*
*  FUNCTION
*     Retrieves the total amount of cores the current host has.
*
*  RESULT
*     int - The amount of cores the current host has. 
*
*  NOTES
*     MT-NOTE: get_execd_amount_of_cores() is MT safe 
*
*******************************************************************************/
int get_execd_amount_of_cores() 
{
#if defined(OGE_HWLOC)
      return oge::topo_get_total_amount_of_cores();
#else
      return 0;
#endif
}

/****** sge_binding/get_execd_amount_of_sockets() **********************************
*  NAME
*    get_execd_amount_of_sockets() -- The total amount of sockets in the system. 
*
*  SYNOPSIS
*     int get_execd_amount_of_sockets() 
*
*  FUNCTION
*     Calculates the total amount of sockets available in the system. 
*
*  INPUTS
*
*  RESULT
*     int - The total amount of sockets available in the system.
*
*  NOTES
*     MT-NOTE: get_execd_amount_of_sockets() is MT safe 
*
*******************************************************************************/
int get_execd_amount_of_sockets()
{
#if defined(OGE_HWLOC)
   return oge::topo_get_total_amount_of_sockets();
#else
   return 0;
#endif
}


bool get_execd_topology(char** topology, int* length)
{
   bool success = false;

   /* topology must be a nullptr pointer */
   if (topology != nullptr && (*topology) == nullptr) {
#if defined(OGE_HWLOC)
      if (oge::topo_get_topology(topology, length) == true) {
         success = true;
      } else {
         success = false;
      }   
#else
      /* currently other architectures are not supported */
      success = false;
#endif
   }

  return success; 
}


/****** sge_binding/getExecdTopologyInUse() ************************************
*  NAME
*     getExecdTopologyInUse() -- Creates a string which represents the used topology. 
*
*  SYNOPSIS
*     bool getExecdTopologyInUse(char** topology) 
*
*  FUNCTION
*     
*     Checks all jobs (with going through active jobs directories) and their 
*     usage of the topology (binding). Afterwards global "logical_used_topology" 
*     string is up to date (which is also updated when a job ends and starts) and 
*     a copy is made available for the caller. 
*     
*     Note: The memory is allocated within this function and 
*           has to be freed from the caller afterwards.
*  INPUTS
*     char** topology - out: the current topology in use by jobs 
*
*  RESULT
*     bool - true if the "topology in use" string could be created 
*
*  NOTES
*     MT-NOTE: getExecdTopologyInUse() is not MT safe 
*******************************************************************************/
bool get_execd_topology_in_use(char** topology)
{
   bool retval = false;

   /* topology must be a nullptr pointer */
   if ((*topology) != nullptr) {
      return false;
   }   

   if (logical_used_topology_length == 0 || logical_used_topology == nullptr) {
#if defined(OGE_HWLOC)
      /* initialize without any usage */
      oge::topo_get_topology(&logical_used_topology,
                             &logical_used_topology_length);
#endif
   }

   if (logical_used_topology_length > 0) {
      /* copy the string */
      (*topology) = sge_strdup(nullptr, logical_used_topology);
      retval = true;
   } 
      
   return retval;   
}

#if defined(OGE_HWLOC)
/* gets the positions in the topology string from a given <socket>,<core> pair */
static int get_position_in_topology(const int socket, const int core, const char* topology, 
   const int topology_length);

/* accounts all occupied resources given by a topology string into another one */
static bool account_job_on_topology(char** topology, const int topology_length, 
   const char* job, const int job_length);  

/* DG TODO length should be an output */
static bool is_starting_point(const char* topo, const int length, const int pos, 
   const int amount, const int stepsize, char** topo_account); 
#endif

#if defined(OGE_HWLOC)


/****** sge_binding/account_job() **********************************************
*  NAME
*     account_job() -- Accounts core binding from a job on host global topology. 
*
*  SYNOPSIS
*     bool account_job(char* job_topology) 
*
*  FUNCTION
*      Accounts core binding from a job on host global topology.
*
*  INPUTS
*     char* job_topology - Topology used from core binding. 
*
*  RESULT
*     bool - true when successful otherwise false
*
*  NOTES
*     MT-NOTE: account_job() is not MT safe 
*
*  SEE ALSO
*     ???/???
*******************************************************************************/
bool account_job(const char* job_topology)
{
   
   if (logical_used_topology_length == 0 || logical_used_topology == nullptr) {

#if defined(OGE_HWLOC)
      /* initialize without any usage */
      oge::topo_get_topology(&logical_used_topology,
                             &logical_used_topology_length);
#endif

   }

   return account_job_on_topology(&logical_used_topology, strlen(logical_used_topology), 
                           job_topology, strlen(job_topology)); 
}

/****** sge_binding/account_job_on_topology() **********************************
*  NAME
*     account_job_on_topology() -- Marks occupied resources. 
*
*  SYNOPSIS
*     static bool account_job_on_topology(char** topology, int* 
*     topology_length, const char* job, const int job_length) 
*
*  FUNCTION
*     Marks occupied resources from one topology string (job) which 
*     is usually a job on another topology string (topology) which 
*     is usually the execution daemon local topology string.
*
*  INPUTS
*     char** topology      - (in/out) topology on which the accounting is done 
*     int* topology_length - (in)  length of the topology stirng
*     const char* job      - (in) topology string from the job
*     const int job_length - (in) length of the topology string from the job
*
*  RESULT
*     static bool - true in case of success
*
*  NOTES
*     MT-NOTE: account_job_on_topology() is MT safe 
*
*  SEE ALSO
*     ???/???
*******************************************************************************/
static bool account_job_on_topology(char** topology, const int topology_length, 
   const char* job, const int job_length)
{
   int i;
   
   /* parameter validation */
   if (topology_length != job_length ||  job_length <= 0 
      || topology == nullptr || (*topology) == nullptr || job == nullptr) {
      return false;
   }

   /* go through topology and account */
   for (i = 0; i < job_length && job[i] != '\0'; i++) {
      if (job[i] == 'c') {
         (*topology)[i] = 'c';
      } else if (job[i] == 's') {
         (*topology)[i] = 's';
      } else if (job[i] == 't') {
         (*topology)[i] = 't';
      }
   }

   return true;
}



/****** sge_binding/binding_explicit_check_and_account() ***********************
*  NAME
*     binding_explicit_check_and_account() -- Checks if a job can be bound.  
*
*  SYNOPSIS
*     bool binding_explicit_check_and_account(const int* list_of_sockets, const 
*     int samount, const int** list_of_cores, const int score, char** 
*     topo_used_by_job, int* topo_used_by_job_length) 
*
*  FUNCTION
*     Checks if the job can bind to the given by the <socket>,<core> pairs. 
*     If so these cores are marked as used and true is returned. Also an 
*     topology string is returned where all cores consumed by the job are 
*     marked with smaller case letters. 
*
*  INPUTS
*     const int* list_of_sockets   - List of sockets to be used 
*     const int samount            - Size of list_of_sockets 
*     const int** list_of_cores    - List of cores (on sockets) to be used 
*     const int score              - Size of list_of_cores 
*
*  OUTPUTS
*     char** topo_used_by_job      -  Topology with resources job consumes marked.
*     int* topo_used_by_job_length -  Topology string length.
*
*  RESULT
*     bool - True if the job can be bound to the topology, false if not. 
*
*  NOTES
*     MT-NOTE: binding_explicit_check_and_account() is MT safe 
*
*  SEE ALSO
*     ???/???
*******************************************************************************/
bool binding_explicit_check_and_account(const int* list_of_sockets, const int samount, 
   const int* list_of_cores, const int score, char** topo_used_by_job, 
   int* topo_used_by_job_length)
{
   int i;

   /* position of <socket>,<core> in topology string */
   int pos;
   /* status if accounting was possible */
   bool possible = true;

   /* input parameter validation */
   if (samount != score || samount <= 0 || list_of_sockets == nullptr
         || list_of_cores == nullptr) {
      return false;
   }

   /* check if the topology which is used already is accessable */
   if (logical_used_topology == nullptr) {
      /* we have no topology string at the moment (should be initialized before) */
      if (!get_execd_topology(&logical_used_topology, &logical_used_topology_length)) {
         /* couldn't even get the topology string */
         return false;
      }
   }
   
   /* create output string */ 
   get_execd_topology(topo_used_by_job, topo_used_by_job_length);

   /* go through the <socket>,<core> pair list */
   for (i = 0; i < samount; i++) {

      /* get position in topology string */
     if ((pos = get_position_in_topology(list_of_sockets[i], list_of_cores[i], 
        logical_used_topology, logical_used_topology_length)) < 0) {
        /* the <socket>,<core> does not exist */
        possible = false;
        break;
     } 

      /* check if this core is available (DG TODO introduce threads) */
      if (logical_used_topology[pos] == 'C') {
         /* do temporarily account it */
         (*topo_used_by_job)[pos] = 'c';
         /* thread binding: account threads here */
         account_all_threads_after_core(topo_used_by_job, pos);
      } else {
         /* core not usable -> early abort */
         possible = false;
         break;
      }
   }
   
   /* do accounting if all cores can be used */
   if (possible) {
      if (account_job_on_topology(&logical_used_topology, logical_used_topology_length, 
         *topo_used_by_job, *topo_used_by_job_length) == false) {
         possible = false;
      }   
   }

   /* free memory when unsuccessful */
   if (possible == false) {
      sge_free(topo_used_by_job);
      *topo_used_by_job_length = 0;
   }

   return possible;
}

/****** sge_binding/free_topology() ********************************************
*  NAME
*     free_topology() -- Free cores used by a job on module global accounting string. 
*
*  SYNOPSIS
*     bool free_topology(const char* topology, const int topology_length) 
*
*  FUNCTION
*     Frees global resources (cores, sockets, or threads) which are marked as 
*     beeing used (lower case letter, like 'c' 's' 't') in the given 
*     topology string. 
*
*  INPUTS
*     const char* topology      - Topology string with the occupied resources. 
*     const int topology_length - Length of the topology string 
*
*  RESULT
*     bool - true in case of success; false in case of a topology mismatch 
*
*  NOTES
*     MT-NOTE: free_topology() is MT safe 
*
*  SEE ALSO
*     ???/???
*******************************************************************************/
bool free_topology(const char* topology, const int topology_length) 
{
   /* free cores, sockets and threads in global accounting */
   int i;
   int size = topology_length;

   if (topology_length < 0) {
      /* size not known but we stop at \0 */
      size = 1000000;
   }
   
   for (i = 0; i < size && i < logical_used_topology_length && 
      topology[i] != '\0' && logical_used_topology[i] != '\0'; i++) {
      
      if (topology[i] == 'c') {
         if (logical_used_topology[i] != 'c' && logical_used_topology[i] != 'C') {
            /* topology type mismatch: input parameter is not like local topology */
            return false;
         } else {
            logical_used_topology[i] = 'C';
         }
      } else if (topology[i] == 't') {
         if (logical_used_topology[i] != 't' && logical_used_topology[i] != 'T') {
            /* topology type mismatch: input parameter is not like local topology */
            return false;
         } else {
            logical_used_topology[i] = 'T';
         }
      } else if (topology[i] == 's') {
         if (logical_used_topology[i] != 's' && logical_used_topology[i] != 'S') {
            /* topology type mismatch: input parameter is not like local topology */
            return false;
         } else {
            logical_used_topology[i] = 'S';
         }
      }

   }

   return true;
}

#endif

/* ---------------------------------------------------------------------------*/
/* ---------------------------------------------------------------------------*/
/*                    Beginning of LINUX related functions                    */
/* ---------------------------------------------------------------------------*/
/* ---------------------------------------------------------------------------*/

/* ---------------------------------------------------------------------------*/
/* ---------------------------------------------------------------------------*/
/*                    Ending of LINUX related functions                       */
/* ---------------------------------------------------------------------------*/
/* ---------------------------------------------------------------------------*/

/* ---------------------------------------------------------------------------*/
/*                   Bookkeeping of cores in use by SGE                       */ 
/* ---------------------------------------------------------------------------*/
#if defined(OGE_HWLOC)

bool get_linear_automatic_socket_core_list_and_account(const int amount, 
      int** list_of_sockets, int* samount, int** list_of_cores, int* camount, 
      char** topo_by_job, int* topo_by_job_length)
{
   /* return value: if it is possible to fit the request on the host  */
   bool possible       = true;   
   
   /* temp topology string where accounting is done on     */
   char* tmp_topo_busy = nullptr;

   /* amount of cores we could account already             */
   int used_cores      = 0;

   /* the numbers of the sockets which are completely free */
   int* sockets        = nullptr;
   int sockets_size    = 0;

   /* tmp counter */
   int i;

   /* get the topology which could be used by the job */
   tmp_topo_busy = (char *) calloc(logical_used_topology_length, sizeof(char));
   memcpy(tmp_topo_busy, logical_used_topology, logical_used_topology_length*sizeof(char));

   /* 1. Find all free sockets and try to fit the request on them     */
   if (get_free_sockets(tmp_topo_busy, logical_used_topology_length, &sockets, 
         &sockets_size) == true) {
      
      /* there are free sockets: use them */
      for (i = 0; i < sockets_size && used_cores < amount; i++) {
         int needed_cores = amount - used_cores;
         used_cores += account_cores_on_socket(&tmp_topo_busy, logical_used_topology_length, 
                           sockets[i], needed_cores, list_of_sockets, samount, 
                           list_of_cores, camount);
      }

      sge_free(&sockets);
   }

   /* 2. If not all cores fit there - fill up the rest of the sockets */
   if (used_cores < amount) {
      
      /* the socket which offers some cores */
      int socket_free = 0;
      /* the amount of cores we still need */
      int needed_cores = amount - used_cores;

      while (needed_cores > 0) {
         /* get the socket with the most free cores */
         if (get_socket_with_most_free_cores(tmp_topo_busy, logical_used_topology_length,
               &socket_free) == true) {
            
            int accounted_cores = account_cores_on_socket(&tmp_topo_busy, 
                                    logical_used_topology_length, socket_free, 
                                    needed_cores, list_of_sockets, samount, 
                                    list_of_cores, camount);

            if (accounted_cores < 1) {
               /* there must be a bug in one of the last two functions! */
               possible = false;
               break;
            }

            needed_cores -= accounted_cores;
            
          } else {
            /* we don't have free cores anymore */
            possible = false;
            break;
          }
       }   

   }

   if (possible == true) {
      /* calculate the topology used by the job out of */ 
      create_topology_used_per_job(topo_by_job, topo_by_job_length, 
         logical_used_topology, tmp_topo_busy, logical_used_topology_length);

      /* make the temporary accounting permanent */
      memcpy(logical_used_topology, tmp_topo_busy, logical_used_topology_length*sizeof(char));
   } 
     
   sge_free(&tmp_topo_busy);

   return possible;
}

static bool get_socket_with_most_free_cores(const char* topology, const int topology_length, 
               int* socket_number) 
{
   /* get the socket which offers most free cores */
   int highest_amount_of_cores = 0;
   *socket_number              = 0;
   int current_socket          = -1;
   int i;
   /* number of unbound cores on the current socket */
   int current_free_cores      = 0;

   /* go through the topology, remember the socket with the highest amount 
      of free cores so far and update it when it is neccessary */
   for (i = 0; i < topology_length && topology[i] != '\0'; i++) {
      
      if (topology[i] == 'S' || topology[i] == 's') {
         /* we are on a new socket */
         current_socket++;
         /* reset core counter */
         current_free_cores = 0;
      } else if (topology[i] == 'C') {
         current_free_cores++;
         
         /* remember if the socket offers more free cores */
         if (current_free_cores > highest_amount_of_cores) {
            highest_amount_of_cores = current_free_cores;
            *socket_number          = current_socket;
         }

      }

   }

   if (highest_amount_of_cores <= 0) {
      /* there is no core free */
      return false;
   } else {
      /* we've found the socket which offers most free cores (socket_number) */
      return true;
   }
}

static bool account_all_threads_after_core(char** topology, const int core_pos)
{
   /* we need the position after the C in the topology string (example: "SCTTSCTT"
      or "SCCSCC") */
   size_t next_pos = core_pos + 1;

   /* check correctness of input values */
   if (topology == nullptr || (*topology) == nullptr || core_pos < 0 || strlen(*topology) <= (size_t)core_pos) {
      return false;
   }
   
   /* check if we are at the last core of the string without T's at the end */
   if (next_pos >= strlen(*topology)) {
      /* there is no thread on the last core to account: thats a success anyway */
      return true;
   } else {
      /* set all T's at the current position */
      while ((*topology)[next_pos] == 'T' || (*topology)[next_pos] == 't') {
         /* account the thread */
         (*topology)[next_pos] = 't';
         next_pos++;
      } 
   }

   return true;
}


static int account_cores_on_socket(char** topology, const int topology_length,
               const int socket_number, const int cores_needed, int** list_of_sockets,
               int* list_of_sockets_size, int** list_of_cores, int* list_of_cores_size)
{
   int i;
   /* socket number we are at the moment */
   int current_socket_number = -1;
   /* return value */
   int retval;

   /* try to use as many cores as possible on a specific socket 
      but not more */
   
   /* jump to the specific socket given by the "socket_number" */
   for (i = 0; i < topology_length && (*topology)[i] != '\0'; i++) {
      if ((*topology)[i] == 'S' || (*topology)[i] == 's') {
         current_socket_number++;
         if (current_socket_number >= socket_number) {
            /* we are at the beginning of socket #"socket_number" */
            break;
         }   
      }
   }

   /* check if we reached that socket or if it was out of range */
   if (socket_number != current_socket_number) {

      /* early abort because we couldn't find the socket we were 
         searching for */ 
      retval = 0;

   } else {
      
      /* we are at a 'S' or 's' and going to the next 'S' or 's' 
         and collecting all cores in between */
      
      int core_counter = 0;   /* current core number on the socket */
      i++;                    /* just forward to the first core on the socket */  
      retval  = 0;            /* need to initialize the amount of cores we found */

      for (; i < topology_length && (*topology)[i] != '\0'; i++) {
         if ((*topology)[i] == 'C') {
            /* take this core */
            (*list_of_sockets_size)++;    /* the socket list is growing */
            (*list_of_cores_size)++;      /* the core list is growing */
            *list_of_sockets = (int *) realloc(*list_of_sockets, (*list_of_sockets_size) 
                                          * sizeof(int));
            *list_of_cores   = (int *) realloc(*list_of_cores, (*list_of_cores_size)  
                                          * sizeof(int));
            /* store the logical <socket,core> tuple inside the lists */
            (*list_of_sockets)[(*list_of_sockets_size) - 1]   = socket_number;
            (*list_of_cores)[(*list_of_cores_size) - 1]       = core_counter;
            /* increase the amount of cores we've collected so far */
            retval++;
            /* move forward to the next core */
            core_counter++;
            /* do accounting */
            (*topology)[i] = 'c';
            /* thread binding: accounting is done here */
            account_all_threads_after_core(topology, i);

         } else if ((*topology)[i] == 'c') {
            /* this core is already in use */
            /* move forward to the next core */
            core_counter++;
         } else if ((*topology)[i] == 'S' || (*topology)[i] == 's') {
            /* we are already on another socket which we can not use */
            break;
         }

         if (retval >= cores_needed) {
            /* we have already collected as many cores we need to collect */
            break;
         }
      }
      
   }

   return retval;
}


static bool get_free_sockets(const char* topology, const int topology_length, 
               int** sockets, int* sockets_size)
{
   /* temporary counter */
   int i, j;
   /* this amount of sockets we discovered already */ 
   int socket_number  = 0;

   (*sockets) = nullptr;
   (*sockets_size) = 0;

   /* go through the whole topology and check if there are some sockets
      completely unbound */
   for (i = 0; i < topology_length && topology[i] != '\0'; i++) {

      if (topology[i] == 'S' || topology[i] == 's') {

         /* we're on a new socket: check all cores (and skip threads) after it */
         bool free = true;

         /* check the topology till the next socket (or end) */
         for (j = i + 1; j < topology_length && topology[j] != '\0'; j++) {
            if (topology[j] == 'c') {
               /* this socket has at least one core in use */
               free = false;
            } else if (topology[j] == 'S' || topology[j] == 's') {
               break;
            }
         }

         /* fast forward */
         i = j;

         /* check if this socket had a core in use */ 
         if (free == true) {
            /* this socket can be used completely */ 
            (*sockets) = (int *) realloc(*sockets, ((*sockets_size)+1)*sizeof(int));
            (*sockets)[(*sockets_size)] = socket_number;
            (*sockets_size)++;
         }
         
         /* increment the amount of sockets we discovered so far */
         socket_number++;

      } /* end if this is a socket */
      
   }

   /* it was successful when we found at least one socket not used by any job */
   if ((*sockets_size) > 0) {
      /* we also have to free the list outside afterwards */
      return true;
   } else {
      return false;
   }
}



/****** sge_binding/get_striding_first_socket_first_core_and_account() ********
*  NAME
*     get_striding_first_socket_first_core_and_account() -- Checks if and where 
*                                                           striding would fit.
*
*  SYNOPSIS
*     bool getStridingFirstSocketFirstCore(const int amount, const int 
*     stepsize, int* first_socket, int* first_core) 
*
*  FUNCTION
*     This operating system independent function checks (depending on 
*     the underlaying topology string and the topology string which 
*     reflects already execution units in use) if it is possible to 
*     bind the job in a striding manner to cores on the host. 
*     
*     This function requires the topology string and the string with the 
*     topology currently in use. 
*
*  INPUTS
*     const int amount    - Amount of cores to allocate. 
*     const int stepsize  - Distance of the cores to allocate.
*     const int start_at_socket - First socket to begin the search with (usually at 0).
*     const int start_at_core   - First core to begin the search with (usually at 0). 
*     int* first_socket   - out: First socket when striding is possible (return value).
*     int* first_core     - out: First core when striding is possible (return value).
*
*  RESULT
*     bool - if true striding is possible at <first_socket, first_core> 
*
*  NOTES
*     MT-NOTE: getStridingFirstSocketFirstCore() is not MT safe 
*
*  SEE ALSO
*     ???/???
*******************************************************************************/
bool get_striding_first_socket_first_core_and_account(const int amount, const int stepsize,
   const int start_at_socket, const int start_at_core, const bool automatic,  
   int* first_socket, int* first_core, char** accounted_topology, 
   int* accounted_topology_length) 
{
   /* return value: if it is possible to fit the request on the host */
   bool possible   = false;   
   
   /* position in topology string */ 
   int i = 0;

   /* socket and core counter in order to find the first core and socket */
   int sc = -1; 
   int cc = -1;
   
   /* these core and socket counters are added later on .. */
   int found_cores   = 0;
   int found_sockets = 0; /* first socket is given implicitely */
   
   /* temp topology string where accounting is done on */
   char* tmp_topo_busy;

   /* initialize socket and core where the striding will fit */
   *first_socket   = 0;
   *first_core     = 0;

   if (start_at_socket < 0 || start_at_core < 0) {
      /* wrong input parameter */
      return false;
   }

   if (logical_used_topology == nullptr) {
      /* we have no topology string at the moment (should be initialized before) */
      if (!get_execd_topology(&logical_used_topology, &logical_used_topology_length)) {
         /* couldn't even get the topology string */
         return false;
      }
   }
   /* temporary accounting string -> account on this and 
      when eventually successful then copy this string back 
      to global topo_busy string */
   tmp_topo_busy = (char *) calloc(logical_used_topology_length + 1, sizeof(char));
   memcpy(tmp_topo_busy, logical_used_topology, logical_used_topology_length*sizeof(char));

   /* we have to go to the first position given by the arguments 
      (start_at_socket and start_at_core) */
   for (i = 0; i < logical_used_topology_length; i++) {

      if (logical_used_topology[i] == 'C' || logical_used_topology[i] == 'c') {
         /* found core   -> update core counter   */
         cc++;
      } else if (logical_used_topology[i] == 'S' || logical_used_topology[i] == 's') {
         /* found socket -> update socket counter */
         sc++;
         /* we're changing socket -> no core found on this one yet */
         cc = -1;
      } else if (logical_used_topology[i] == '\0') {
         /* we couldn't find start socket start string */
         possible = false;
         sge_free(&tmp_topo_busy);
         return possible;
      }
      
      if (sc == start_at_socket && cc == start_at_core) {
         /* we found our starting point (we remember 'i' for next loop!) */
         break;
      }
   }
   
   /* check if we found the socket and core we want to start searching */
   if (sc != start_at_socket || cc != start_at_core) {
      /* could't find the start socket and start core */
      sge_free(&tmp_topo_busy);
      return false;
   }

   /* check each position of the topology string */
   /* we reuse 'i' from last loop -> this is the position where we begin */
   for (; i < logical_used_topology_length && logical_used_topology[i] != '\0'; i++) {
      
      /* this could be optimized (with increasing i in case if it is not
         possible) */  
      if (is_starting_point(logical_used_topology, logical_used_topology_length, i, amount, stepsize, 
            &tmp_topo_busy)) {
         /* we can do striding with this as starting point */
         possible = true;
         /* update place where we can begin */
         *first_socket = start_at_socket + found_sockets;
         *first_core   = start_at_core + found_cores;
         /* return the accounted topology */ 
         create_topology_used_per_job(accounted_topology, accounted_topology_length, 
            logical_used_topology, tmp_topo_busy, logical_used_topology_length);
         /* finally do execution host wide accounting */
         /* DG TODO mutex */ 
         memcpy(logical_used_topology, tmp_topo_busy, logical_used_topology_length*sizeof(char));

         break;
      } else { 

         /* else retry and update socket and core number to start with */

         if (logical_used_topology[i] == 'C' || logical_used_topology[i] == 'c') {
            /* jumping over a core */
            found_cores++;
            /* a core is a valid starting point for binding in non-automatic case */ 
            /* if we have a fixed start socket and a start core we do not retry 
               it with the next core available (when introducing T's this have to 
               be added there too) */
            if (automatic == false) {
               possible = false;
               break;
            }

         } else if (logical_used_topology[i] == 'S' || logical_used_topology[i] == 's') {
            /* jumping over a socket */
            found_sockets++;
            /* we are at core 0 on the new socket */
            found_cores = 0;
         }
         /* at the moment we are not interested in threads or anything else */
         
      }
   
   } /* end go through the whole topology string */
   
   sge_free(&tmp_topo_busy);
   return possible;
}


static bool create_topology_used_per_job(char** accounted_topology, int* accounted_topology_length, 
            char* logical_used_topology, char* used_topo_with_job, int logical_used_topology_length)
{        
   /* tmp counter */
   int i;

   /* length of output string remains the same */
   (*accounted_topology_length) = logical_used_topology_length;
   
   /* copy string of current topology in use */
   (*accounted_topology) = (char *)calloc(logical_used_topology_length+1, sizeof(char));
   if ((*accounted_topology) == nullptr) {
      /* out of memory */
      return false;
   }

   memcpy((*accounted_topology), logical_used_topology, sizeof(char)*logical_used_topology_length);
   
   /* revert all accounting from other jobs */ 
   for (i = 0; i < logical_used_topology_length; i++) {
      if ((*accounted_topology)[i] == 'c') {
         (*accounted_topology)[i] = 'C';
      } else if ((*accounted_topology)[i] == 's') {
         (*accounted_topology)[i] = 'S';
      } else if ((*accounted_topology)[i] == 't') {
         (*accounted_topology)[i] = 'T';
      }
   }

   /* account all the resources the job consumes: these are all occupied 
      resources in used_topo_with_job String that are not occupied in 
      logical_used_topology String */
   for (i = 0; i < logical_used_topology_length; i++) {

      if (used_topo_with_job[i] == 'c' && logical_used_topology[i] == 'C') {
         /* this resource is from job exclusively used */
         (*accounted_topology)[i] = 'c';
      }

      if (used_topo_with_job[i] == 't' && logical_used_topology[i] == 'T') {
         /* this resource is from job exclusively used */
         (*accounted_topology)[i] = 't';
      }

      if (used_topo_with_job[i] == 's' && logical_used_topology[i] == 'S') {
         /* this resource is from job exclusively used */
         (*accounted_topology)[i] = 's';
      }
      
   }

   return true;
}

/****** sge_binding/is_starting_point() ****************************************
*  NAME
*     is_starting_point() -- Checks if 'pos' is a valid first core for striding.
*
*  SYNOPSIS
*     bool is_starting_point(const char* topo, const int length, const int pos,
*     const int amount, const int stepsize) 
*
*  FUNCTION
*     Checks if 'pos' is a starting point for binding the 'amount' of cores
*     in a striding manner on the host. The topo string contains 'C's for unused
*     cores and 'c's for cores in use.
*
*  INPUTS
*     const char* topo   - String representing the topology currently in use.
*     const int length   - Length of topology string.
*     const int pos      - Position within the topology string.
*     const int amount   - Amount of cores to bind to.
*     const int stepsize - Step size when binding in a striding manner.
*
*  OUTPUTS
*     char* topo_account - Here the accounting is done on.
*
*  RESULT
*     bool - true if striding with the given parameters is possible.
*
*  NOTES
*     MT-NOTE: is_starting_point() is not MT safe
*
*  SEE ALSO
*     ???/???
*******************************************************************************/
static bool is_starting_point(const char* topo, const int length, const int pos, 
   const int amount, const int stepsize, char** topo_account) {
   
   /* go through the topology (in use) string with the beginning at pos 
      and try to fit all cores in there */ 
   int i;   
   /* core counter in order to fulfill the stepsize property */
   int found_cores = 1;
   /* so many cores we have collected so far */
   int accounted_cores = 0;
   /* return value */ 
   bool is_possible = false;

   /* stepsize must be 1 or greater */
   if (stepsize < 1) {
      return false;
   }
   /* position in string must be smaller than string length */
   if (pos >= length) {
      return false;
   }
   /* topology string must not be nullptr */
   if (topo == nullptr) {
      return false;
   }
   /* amount must be 1 or greater */
   if (amount < 1) {
      return false;
   }

   /* fist check if this is a valid core */ 
   if (topo[pos] != 'C' || topo[pos] == '\0') {
      /* not possible this is not a valid free core (could be a socket,
         thread, or core in use) */
      return false;
   }

   /* we count this core */ 
   accounted_cores++;
   /* this core is used */
   (*topo_account)[pos] = 'c';
   /* thread binding: account following threads */
   account_all_threads_after_core(topo_account, pos); 

   if (accounted_cores == amount) {
      /* we have all cores and we are still within the string */
      is_possible = true;
      return is_possible;
   }

   /* go to the remaining topology which is in use */ 
   for (i = pos + 1; i < length && topo[i] != '\0'; i++) {
   
      if (topo[i] == 'C') {
         /* we found an unused core */
         if (found_cores >= stepsize) {
            /* this core we need and it is free - good */
            found_cores = 1;
            /* increase the core counter */
            accounted_cores++;
            /* this core is used */
            (*topo_account)[i] = 'c';
            /* thread binding: bind following threads */
            account_all_threads_after_core(topo_account, i); 

         } else if (found_cores < stepsize) {
            /* this core we don't need */
            found_cores++;
         }
      } else if (topo[i] == 'c') {
         /* this is a core in use */
         if (found_cores >= stepsize) {
            /* this core we DO NEED but it is busy */
            return false;
         } else if (found_cores < stepsize) {
            /* this core we don't need */
            found_cores++;
         }
      } 
      
      /* accounted cores */ 
      if (accounted_cores == amount) {
         /* we have all cores and we are still within the string */
         is_possible = true;
         break;
      }
   }
   
   /* using this core as first core is possible */
   return is_possible;
}   

static int get_position_in_topology(const int socket, const int core, 
   const char* topology, const int topology_length)
{
   
   int i;
   /* position of <socket>,<core> in the topology string */
   int retval = -1;

   /* current position */
   int s = -1;
   int c = -1;

   if (topology_length <= 0 || socket < 0 || core < 0 || topology == nullptr) {
      return false;
   }
   
   for (i = 0; i < topology_length && topology[i] != '\0'; i++) {
      if (topology[i] == 'S') {
         /* we've got a new socket */
         s++;
         /* invalidate core counter */
         c = -1;
      } else if (topology[i] == 'C') {
         /* we've got a new core */
         c++;
         /* invalidate thread counter */
      } else if (topology[i] == 'T') {
         /* we've got a new thread */
      }
      /* check if we are at the position seeking for */
      if (socket == s && core == c) {
         retval = i;
         break;
      }   
   }

   return retval;
}

bool initialize_topology() {
   
   /* this is done when execution daemon starts        */
   
   if (logical_used_topology == nullptr) {
      if (get_execd_topology(&logical_used_topology, &logical_used_topology_length)) {
         return true;
      }
   }

   return false;
}

#endif

/* ---------------------------------------------------------------------------*/
/*               End of bookkeeping of cores in use by GE                     */
/* ---------------------------------------------------------------------------*/

/****** sge_binding/binding_print_to_string() **********************************
*  NAME
*     binding_print_to_string() -- Prints the content of a binding list to a string
*
*  SYNOPSIS
*     bool binding_print_to_string(const lListElem *this_elem, dstring *string)
*
*  FUNCTION
*     Prints the binding type and binding strategy of a binding list element 
*     into a string.
*
*  INPUTS
*     const lListElem* this_elem - Binding list element
*
*  OUTPUTS
*     const dstring *string      - Output string which must be initialized.
*
*  RESULT
*     bool - true in all cases
*
*  NOTES
*     MT-NOTE: is_starting_point() is MT safe
*
*******************************************************************************/
bool
binding_print_to_string(const lListElem *this_elem, dstring *string) {
   bool ret = true;

   DENTER(BINDING_LAYER);
   if (this_elem != nullptr && string != nullptr) {
      const char *const strategy = lGetString(this_elem, BN_strategy);
      binding_type_t type = (binding_type_t)lGetUlong(this_elem, BN_type);

      switch (type) {
         case BINDING_TYPE_SET:
            sge_dstring_append(string, "set ");
            break;
         case BINDING_TYPE_PE:
            sge_dstring_append(string, "pe ");
            break;
         case BINDING_TYPE_ENV:
            sge_dstring_append(string, "env ");
            break;
         case BINDING_TYPE_NONE:
            sge_dstring_append(string, "NONE");
      }

      if (strcmp(strategy, "linear_automatic") == 0) {
         sge_dstring_sprintf_append(string, "%s:"sge_U32CFormat,
            "linear", sge_u32c(lGetUlong(this_elem, BN_parameter_n)));
      } else if (strcmp(strategy, "linear") == 0) {
         sge_dstring_sprintf_append(string, "%s:"sge_U32CFormat":"sge_U32CFormat","sge_U32CFormat,
            "linear", sge_u32c(lGetUlong(this_elem, BN_parameter_n)),
            sge_u32c(lGetUlong(this_elem, BN_parameter_socket_offset)),
            sge_u32c(lGetUlong(this_elem, BN_parameter_core_offset)));
      } else if (strcmp(strategy, "striding_automatic") == 0) {
         sge_dstring_sprintf_append(string, "%s:"sge_U32CFormat":"sge_U32CFormat,
            "striding", sge_u32c(lGetUlong(this_elem, BN_parameter_n)),
            sge_u32c(lGetUlong(this_elem, BN_parameter_striding_step_size)));
      } else if (strcmp(strategy, "striding") == 0) {
         sge_dstring_sprintf_append(string, "%s:"sge_U32CFormat":"sge_U32CFormat":"sge_U32CFormat","sge_U32CFormat,
            "striding", sge_u32c(lGetUlong(this_elem, BN_parameter_n)),
            sge_u32c(lGetUlong(this_elem, BN_parameter_striding_step_size)),
            sge_u32c(lGetUlong(this_elem, BN_parameter_socket_offset)),
            sge_u32c(lGetUlong(this_elem, BN_parameter_core_offset)));
      } else if (strcmp(strategy, "explicit") == 0) {
         sge_dstring_sprintf_append(string, "%s", lGetString(this_elem, BN_parameter_explicit));
      }
   }
   DRETURN(ret);
}

bool
binding_parse_from_string(lListElem *this_elem, lList **answer_list, dstring *string) 
{
   bool ret = true;

   DENTER(BINDING_LAYER);

   if (this_elem != nullptr && string != nullptr) {
      int amount = 0;
      int stepsize = 0;
      int firstsocket = 0;
      int firstcore = 0;
      binding_type_t type = BINDING_TYPE_NONE; 
      dstring strategy = DSTRING_INIT;
      dstring socketcorelist = DSTRING_INIT;
      dstring error = DSTRING_INIT;

      if (parse_binding_parameter_string(sge_dstring_get_string(string), 
               &type, &strategy, &amount, &stepsize, &firstsocket, &firstcore, 
               &socketcorelist, &error) != true) {
         dstring parse_binding_error = DSTRING_INIT;

         sge_dstring_append_dstring(&parse_binding_error, &error);

         answer_list_add_sprintf(answer_list, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                 MSG_PARSE_XOPTIONWRONGARGUMENT_SS, "-binding",  
                                 sge_dstring_get_string(&parse_binding_error));

         sge_dstring_free(&parse_binding_error);
         ret = false;
      } else {
         lSetString(this_elem, BN_strategy, sge_dstring_get_string(&strategy));
         
         lSetUlong(this_elem, BN_type, type);
         lSetUlong(this_elem, BN_parameter_socket_offset, (firstsocket >= 0) ? firstsocket : 0);
         lSetUlong(this_elem, BN_parameter_core_offset, (firstcore >= 0) ? firstcore : 0);
         lSetUlong(this_elem, BN_parameter_n, (amount >= 0) ? amount : 0);
         lSetUlong(this_elem, BN_parameter_striding_step_size, (stepsize >= 0) ? stepsize : 0);
         
         if (strstr(sge_dstring_get_string(&strategy), "explicit") != nullptr) {
            lSetString(this_elem, BN_parameter_explicit, sge_dstring_get_string(&socketcorelist));
         }
      }

      sge_dstring_free(&strategy);
      sge_dstring_free(&socketcorelist);
      sge_dstring_free(&error);
   }

   DRETURN(ret);
}

