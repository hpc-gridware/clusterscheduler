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

#include "uti/sge_rmon_macros.h"
#include "uti/sge_log.h"

#include "cull/cull.h"

#include "sgeobj/sge_job.h"
#include "sgeobj/ocs_Binding.h"

#include "sgeobj/ocs_BindingExecd2Shepherd.h"

#if defined(BINDING_SOLARIS) || defined(OCS_HWLOC)

/****** exec_job/parse_job_accounting_and_create_logical_list() ****************
*  NAME
*     parse_job_accounting_and_create_logical_list() -- Creates the core list out of accounting string.
*
*  SYNOPSIS
*     static bool parse_job_accounting_and_create_logical_list(const char*
*     binding_string, char** rankfileinput)
*
*  FUNCTION
*     Creates the input for the rankfile out of the core binding string
*     which is written in the config file in the active_jobs directory.
*
*  INPUTS
*     const char* binding_string - Pointer to the core binding string.
*
*  OUTPUTS
*     char** rankfileinput       - String with logical socket,core list.
*
*  RESULT
*     static bool - true when string with socket,core list was created
*
*  NOTES
*     MT-NOTE: parse_job_accounting_and_create_logical_list() is MT safe
*
*******************************************************************************/
bool
ocs::BindingExecd2Shepherd::parse_job_accounting_and_create_logical_list(const char *binding_string, char **rankfileinput) {
   bool retval;

   int *sockets = nullptr;
   int *cores = nullptr;
   int amount = 0;

   const char *pos;

   DENTER(TOP_LAYER);

   /* get the position of the "SCCSCc" job accounting string in string */
   pos = binding_get_topology_for_job(binding_string);

   /* convert job usage in terms of the topology string into
      a list as string */
   if (!topology_string_to_socket_core_lists(pos, &sockets, &cores, &amount)) {
      WARNING("Core binding: Couldn't parse job topology string! %s", pos);
      retval = false;
   } else if (amount > 0) {

      /* build the string */
      int i;
      dstring full = DSTRING_INIT;
      dstring pair = DSTRING_INIT;

      for (i = 0; i < (amount - 1); i++) {
         sge_dstring_sprintf(&pair, "%d,%d:", sockets[i], cores[i]);
         sge_dstring_append_dstring(&full, &pair);
         sge_dstring_clear(&pair);
      }
      /* the last pair does not have the ":" at the end */
      sge_dstring_sprintf(&pair, "%d,%d", sockets[amount - 1], cores[amount - 1]);
      sge_dstring_append_dstring(&full, &pair);

      /* allocate memory for the output variable "rankfileinput" */
      *rankfileinput = sge_strdup(nullptr, sge_dstring_get_string(&full));

      if (*rankfileinput == nullptr) {
         WARNING("Core binding: Malloc error");
         retval = false;
      } else {
         INFO("Core binding: PE rankfileinput is %s", *rankfileinput);
         retval = true;
      }

      sge_dstring_free(&pair);
      sge_dstring_free(&full);

      sge_free(&sockets);
      sge_free(&cores);

   } else {
      /* no cores used */
      INFO("Core binding: Couldn't determine any allocated cores for the job");
      *rankfileinput = sge_strdup(nullptr, "<nullptr>");
      retval = true;
   }

   DRETURN(retval);
}

#endif
/* creates binding string for config file */
#if defined(OCS_HWLOC)

/****** exec_job/create_binding_strategy_string_linux() ************************
*  NAME
*     create_binding_strategy_string_linux() -- Creates the core binding strategy string.
*
*  SYNOPSIS
*     static bool create_binding_strategy_string_linux(dstring* result,
*     lListElem *jep, char** rankfileinput)
*
*  FUNCTION
*     Creates the core binding strategy string depending on the given request in
*     the CULL list. This string is written in the config file in order to
*     tell the shepherd which binding has to be performed.
*
*
*  INPUTS
*     lListElem *jep       - CULL list with the core binding request
*
*  OUTPUTS
*     dstring* result      - Contains the string which is written in config file.
*     char** rankfileinput - String which is written in the pe_hostfile when requested.
*
*  RESULT
*     static bool - returns true in case of success otherwise false
*
*  NOTES
*     MT-NOTE: create_binding_strategy_string_linux() is not MT safe
*
*******************************************************************************/
bool
ocs::BindingExecd2Shepherd::create_binding_strategy_string_linux(dstring *result, lListElem *jep, char **rankfileinput) {
   DENTER(TOP_LAYER);

   /* temporary result string with or without "env:" prefix (when environment
      variable for binding should be set or not) */
   dstring tmp_result = DSTRING_INIT;
   bool retval;

   /* binding strategy */
   const lListElem *binding_elem = lGetObject(jep, JB_new_binding);
   if (binding_elem != nullptr) {

      /* re-create the binding string (<strategy>:<parameter>:<parameter>) */

      /* check if a leading "env_" or "pe_" is needed */
      if (lGetUlong(binding_elem, BN_type) == BINDING_TYPE_ENV) {
         /* we have just to set the environment variable SGE_BINDING for the
            job */
         sge_dstring_append(result, "env_");

      } else if (lGetUlong(binding_elem, BN_type) == BINDING_TYPE_PE) {
         /* we have to attach settings to the pe_hostfile */
         sge_dstring_append(result, "pe_");
      }

      if (strcmp(lGetString(binding_elem, BN_strategy), "linear") == 0) {

         retval = linear_linux(&tmp_result, binding_elem, false);

      } else if (strcmp(lGetString(binding_elem, BN_strategy), "linear_automatic") == 0) {

         retval = linear_linux(&tmp_result, binding_elem, true);

      } else if (strcmp(lGetString(binding_elem, BN_strategy), "striding") == 0) {

         retval = striding_linux(&tmp_result, binding_elem, false);

      } else if (strcmp(lGetString(binding_elem, BN_strategy), "striding_automatic") == 0) {

         retval = striding_linux(&tmp_result, binding_elem, true);

      } else if (strcmp(lGetString(binding_elem, BN_strategy), "explicit") == 0) {

         retval = explicit_linux(&tmp_result, binding_elem);

      } else {

         /* BN_strategy does not contain anything usefull */
         retval = false;
      }

     if (retval != false) {
        /* parse the topology used by the job out of the string (it is at the
           end) and convert it to "<socket>,<core>:<socket>,<core>:..." but just
           when config binding element has prefix "pe_" */
        if (lGetUlong(binding_elem, BN_type) == BINDING_TYPE_PE) {
           /* generate pe_hostfile input */
           if (!parse_job_accounting_and_create_logical_list(
                  sge_dstring_get_string(&tmp_result), rankfileinput)) {
              WARNING("Core binding: Couldn't create input for pe_hostfile");
              retval = false;
           }
        }
        /* append result to the prefix */
        sge_dstring_append_dstring(result, &tmp_result);
     }
   } else {
      INFO("Core binding: No CULL sublist for binding found!");
      retval = false;
   }

   if (!retval) {
      sge_dstring_clear(result);
      sge_dstring_append(result, "nullptr");
   }

   sge_dstring_free(&tmp_result);

   DRETURN(retval);
}

/****** exec_job/linear_linux() ************************************************
*  NAME
*     linear_linux() -- Creates a binding request string from request (CULL list).
*
*  SYNOPSIS
*     static bool linear_linux(dstring* result, lListElem* binding_elem, const
*     bool automatic)
*
*  FUNCTION
*     Tries to allocate processor cores according the request in the binding_elem.
*     If this is possible the cores are listed in the result string and true
*     is returned.
*
*     Linear means that the job is tried to be accomodated on a single socket,
*     if this is not possible than the remaining cores are taken from another
*     free socket (or afterwards from that socket with the most free cores).
*
*     Linear with automatic=false means that the same algorithms as for striding
*     with automatic=false is applied. Both have a core requested to be the
*     first core. This core is taken and his successors if this is possible
*     otherwise no binding is done at all.
*
*     In case of success the cores were marked internally as beeing bound.
*
*  INPUTS
*     lListElem* binding_elem - List containing the binding request.
*     const bool automatic    - If the start core to allocate is given or not.
*
*  OUTPUTS
*     dstring* result         - String containing the requested cores if possible.
*
*  RESULT
*     static bool - True in case core binding was possible.
*
*  NOTES
*     MT-NOTE: linear_linux() is not MT safe
*
*******************************************************************************/
bool
ocs::BindingExecd2Shepherd::linear_linux(dstring *result, const lListElem *binding_elem, const bool automatic) {
   DENTER(TOP_LAYER);

   int first_socket = 0;
   int first_core = 0;
   int used_first_socket = 0;
   int used_first_core = 0;
   char *topo_job = nullptr;
   int topo_job_length = 0;
   bool retval;

   int amount = (int) lGetUlong(binding_elem, BN_parameter_n);

   /* check if first socket and first core have to be determined by execd or
      not */
   if (!automatic) {
      /* we have to retrieve socket,core to begin with when explicitly given */
      first_socket = (int) lGetUlong(binding_elem, BN_parameter_socket_offset);
      first_core = (int) lGetUlong(binding_elem, BN_parameter_core_offset);
   }

   /* check if the resources are free and binding could be performed from
      shephered */

   if (automatic) {
      DPRINTF("automatic\n");

      /* user has not specified where to begin, this has now being figured out automatically */
      int *list_of_sockets = nullptr;
      int samount = 0;
      int *list_of_cores = nullptr;
      int camount = 0;

      if (get_linear_automatic_socket_core_list_and_account(amount,
                                                            &list_of_sockets, &samount, &list_of_cores, &camount,
                                                            &topo_job, &topo_job_length)) {

         int cn;
         /* could get the socket,core pairs to bind to   */
         /* tell it to shepherd like an explicit request */
         sge_dstring_sprintf(result, "%s:", "explicit");
         /* add the list of socket,core pairs */
         for (cn = 0; cn < camount; cn++) {
            dstring pair = DSTRING_INIT;
            sge_dstring_sprintf(&pair, "%d,%d:", list_of_sockets[cn], list_of_cores[cn]);
            sge_dstring_append_dstring(result, &pair);
            sge_dstring_free(&pair);
         }

         /* finally add the topology */
         sge_dstring_append(result, topo_job);

         /* free lists */
         sge_free(&list_of_sockets);
         sge_free(&list_of_cores);

         retval = true;

      } else {
         /* there was a problem allocating the cores */
         DPRINTF("ERROR: Couldn't allocate cores with respect to binding request!");
         sge_dstring_append(result, "nullptr");
         retval = false;
      }

   } else {
      DPRINTF("not automatic\n");

      /* we have already a socket,core tuple to start with, therefore we
         use this one if possible or do not do any binding */
      if (get_striding_first_socket_first_core_and_account(amount, 1, first_socket, first_core,
                                                           automatic, &used_first_socket, &used_first_core,
                                                           &topo_job, &topo_job_length)) {

         /* only "linear" is allowed in config file, because execd has to figure
            out first <socket,core> to bind to (not shepherd - because of race
            conditions) */
         sge_dstring_sprintf(result, "%s:%d:%d,%d:%s", "linear", amount, first_socket, first_core, topo_job);

         retval = true;
      } else {
         /* couldn't allocate cores */
         DPRINTF("ERROR: Couldn't allocate cores with respect to binding request!");
         sge_dstring_append(result, "nullptr");
         retval = false;
      }
   }

   /* free topology string */
   sge_free(&topo_job);

   DRETURN(retval);
}


/****** exec_job/striding_linux() **********************************************
*  NAME
*     striding_linux() -- Creates a binding request string from request (CULL list).
*
*  SYNOPSIS
*     static bool striding_linux(dstring* result, lListElem* binding_elem,
*     const bool automatic)
*
*  FUNCTION
*     Tries to allocate processor cores according the request in the binding_elem.
*     If this is possible the cores are listed in the result string and true
*     is returned.
*     Striding means that cores with a specific distance (the step size) are
*     tried to be used. In case of automatic=false the first core to allocate
*     is given in the CULL list (binding_elem). If the request could not be
*     fulfilled the function returns false.
*
*     In case of success the cores were marked internally as beeing bound.
*
*  INPUTS
*     lListElem* binding_elem - The CULL list with the request.
*     const bool automatic    - True when the first core have to be searched.
*
*  OUTPUTS
*     dstring* result         - Contains the requested cores is case of success.
*
*  RESULT
*     static bool - true in case of success otherwise false
*
*  NOTES
*     MT-NOTE: striding_linux() is not MT safe
*
*******************************************************************************/
bool
ocs::BindingExecd2Shepherd::striding_linux(dstring *result, const lListElem *binding_elem, const bool automatic) {
   int first_socket = 0;
   int first_core = 0;
   int used_first_socket = 0;
   int used_first_core = 0;
   char *topo_job = nullptr;
   int topo_job_length = 0;
   bool retval;

   /* get mandatory parameters of -binding striding */
   int amount = (int) lGetUlong(binding_elem, BN_parameter_n);
   int step_size = (int) lGetUlong(binding_elem, BN_parameter_striding_step_size);

   DENTER(TOP_LAYER);

   if (!automatic) {
      /* We have to determine the first socket and core to use for core binding
         automatically. The rest of the cores are then implicitly given by the
         strategy. */

      /* get the start socket and start core which was a submission parameter */
      first_socket = (int) lGetUlong(binding_elem, BN_parameter_socket_offset);
      first_core = (int) lGetUlong(binding_elem, BN_parameter_core_offset);
      DPRINTF("Got starting point for binding (socket, core) %d %d", first_socket, first_core);
   }

   /* try to allocate first core and first socket */
   if (get_striding_first_socket_first_core_and_account(amount, step_size, first_socket,
                                                        first_core, automatic, &used_first_socket, &used_first_core,
                                                        &topo_job, &topo_job_length)) {
      DPRINTF("Found following starting point (socket, core) %d %d\n", used_first_socket, used_first_core);

      /* found first socket and first core ! */
      sge_dstring_sprintf(result, "%s:%d:%d:%d,%d:%s",
                          "striding",
                          amount,
                          step_size,
                          automatic ? used_first_socket : first_socket,
                          automatic ? used_first_core : first_core,
                          topo_job);
      DPRINTF("Found following binding %s\n", sge_dstring_get_string(result));
      retval = true;

   } else {
      /* it was not possible to fit the binding strategy on host
         because it is occupied already or any other reason */
      DPRINTF("ERROR: couldn't allocate cores with respect to binding request");
      sge_dstring_append(result, "nullptr");
      retval = false;
   }

   /* free topology string */
   if (topo_job != nullptr) {
      sge_free(&topo_job);
   }

   /* return core binding string */
   DRETURN(retval);
}


/****** exec_job/explicit_linux() **********************************************
*  NAME
*     explicit_linux() -- Creates a binding request string from request (CULL list).
*
*  SYNOPSIS
*     static bool explicit_linux(dstring* result, lListElem* binding_elem)
*
*  FUNCTION
*     Tries to allocate processor cores according the request in the binding_elem.
*     If this is possible the cores are listed in the result string and true
*     is returned.
*
*     Explicit means that specific cores (as socket, core list) are requested
*     on submission time. If one of these cores can not be allocated (because
*     it is not available on the exution host or it is currently bound) than
*     no binding will be done.
*
*     In case of success the cores were marked internally as beeing bound.
*
*  INPUTS
*     lListElem* binding_elem - List containing the binding request.
*
*  OUTPUTS
*     dstring* result         - String containing the requested cores if possible.
*
*  RESULT
*     static bool - true in case of success otherwise false
*
*  NOTES
*     MT-NOTE: explicit_linux() is not MT safe
*
*******************************************************************************/
bool
ocs::BindingExecd2Shepherd::explicit_linux(dstring *result, const lListElem *binding_elem) {
   /* pointer to string which contains the <socket>,<core> pairs */
   const char *request = nullptr;

   /* the topology used by the job */
   char *topo_by_job = nullptr;
   int topo_by_job_length;

   /* the from the request extracted sockets and cores (to bind to) */
   int *socket_list = nullptr;
   int *core_list = nullptr;
   int socket_list_length, core_list_length;
   bool retval;

   DENTER(TOP_LAYER);

   request = (char *) lGetString(binding_elem, BN_parameter_explicit);

   /* get the socket and core number lists */
   if (!binding_explicit_extract_sockets_cores(request, &socket_list,
      &socket_list_length, &core_list, &core_list_length)) {
      /* problems while parsing the binding request */
      INFO("Couldn't extract socket and core lists out of string");
      sge_dstring_append(result, "nullptr");
      retval = false;
   } else {

      /* check if cores are free */
      if (binding_explicit_check_and_account(socket_list, socket_list_length,
                                             core_list, core_list_length, &topo_by_job, &topo_by_job_length)) {

         /* was able to account core usage from job */
         sge_dstring_sprintf(result, "%s:%s", request, topo_by_job);
         retval = true;
      } else {

         /* couldn't find an appropriate binding because topology doesn't offer
            it or some cores are already occupied */
         INFO("ERROR: Couldn't determine appropriate core binding %s %d %d %d %d", request, socket_list_length, socket_list[0], core_list_length, core_list[0]);
         sge_dstring_append(result, "nullptr");
         retval = false;
      }
   }

   /* free resources */
   sge_free(&topo_by_job);
   sge_free(&socket_list);
   sge_free(&core_list);

   DRETURN(retval);
}

#endif

#if defined(BINDING_SOLARIS)
/****** exec_job/create_binding_strategy_string_solaris() **********************
*  NAME
*     create_binding_strategy_string_solaris() --  Creates a binding request string from request (CULL list).
*
*  SYNOPSIS
*     static bool create_binding_strategy_string_solaris(dstring* result,
*     lListElem *jep, char* err_str, int err_length, char** env, char**
*     rankfileinput)
*
*  FUNCTION
*     Tries to allocate processor cores according the request in the binding_elem.
*     If this is possible the cores are listed in the result string and true
*     is returned.
*
*     This function dispatches the task to the appropriate helper functions.
*
*     TODO DG: eliminate err_str and err_length.
*
*  INPUTS
*     lListElem *jep       -  The CULL list with the request.
*
*  OUTPUTS
*     dstring* result      - Contains the requested cores is case of success.
*     char* err_str        - Contains error messages in case of errors.
*     int err_length       - Length of the error messages
*     char** env           - Contains the SGE_BINDING content in case of 'env'.
*     char** rankfileinput - Contains the selected cores for pe_hostfile in case of 'pe'.
*
*  RESULT
*     static bool - true in case of success otherwise false.
*
*  NOTES
*     MT-NOTE: create_binding_strategy_string_solaris() is not MT safe
*
*******************************************************************************/
bool
ocs::BindingExecd2Shepherd::create_binding_strategy_string_solaris(dstring* result, lListElem *jep, char* err_str, int err_length, char** env, char** rankfileinput)
{
   DENTER(TOP_LAYER);

   /* 1. check cull list and check which binding strategy was requested */
   bool retval;
   /* binding strategy */
   const lListElem *binding_elem = lGetObject(jep, JB_new_binding);
   if (binding_elem != nullptr)) {

      if (strcmp(lGetString(binding_elem, BN_strategy), "striding_automatic") == 0) {

         /* try to allocate processor set according the settings and account it on
            execution host level */
         retval = striding_solaris(result, binding_elem, true, false, err_str,
                                    err_length, env);

      } else if (strcmp(lGetString(binding_elem, BN_strategy), "striding") == 0) {

         retval = striding_solaris(result, binding_elem, false, false, err_str,
                                    err_length, env);

      } else if(strcmp(lGetString(binding_elem, BN_strategy), "linear_automatic") == 0) {

         retval = linear_automatic_solaris(result, binding_elem, env);

      } else if (strcmp(lGetString(binding_elem, BN_strategy), "linear") == 0) {

         /* use same algorithm than striding with stepsize 1 */
         retval = striding_solaris(result, binding_elem, false, true,
                                    err_str, err_length, env);

      } else if (strcmp(lGetString(binding_elem, BN_strategy), "explicit") == 0) {

         retval = explicit_solaris(result, binding_elem, err_str, err_length, env);

      } else {
         /* no valid binding strategy selected */
         INFO("ERROR: No valid binding strategy in CULL BN_strategy");
         retval = false;
      }

   } else {
      INFO("No CULL JB_new_binding sublist found");
      retval = false;
   }

   /* in case no core binding is selected or any other error occurred */
   if (!retval) {
      sge_dstring_append(result, "nullptr");
   } else {
      /* in case of -binding PE the string with the socket,core pairs
         must be returned */

      if (result != nullptr && sge_dstring_get_string(result) != nullptr &&
            strstr(sge_dstring_get_string(result), "pe_") != nullptr) {
         retval = parse_job_accounting_and_create_logical_list(
               sge_dstring_get_string(result), rankfileinput);
      }
   }

   /* 7. shepherd have to append current process on processor set */
   DRETURN(retval);
}


/****** exec_job/linear_automatic_solaris() ************************************
*  NAME
*     linear_automatic_solaris() -- Creates core binding string.
*
*  SYNOPSIS
*     static bool linear_automatic_solaris(dstring* result, lListElem*
*     binding_elem, char** env)
*
*  FUNCTION
*    Tries to allocate cores in a "linear" (successive) manner. It beginns with
*    the first free socket on the system. If there are still cores left
*    to be allocated or there is no free socket on the system, the socket
*    with the most free cores is taken. And so on.
*
*  INPUTS
*     lListElem* binding_elem - Cull list containing the core binding request.
*
*  OUTPUTS
*     dstring* result         - String with the allocated cores.
*     char** env              - String with the SGE_BINDING content if neccessary.
*
*  RESULT
*     static bool - true in case of success otherwise false
*
*******************************************************************************/
bool
ocs::BindingExecd2Shepherd::linear_automatic_solaris(dstring* result, const lListElem* binding_elem, char** env)
{
   int amount;  /* amount of cores to bind to       */
   binding_type_t type;    /* type of binding (set|env|pe)     */

   /* the <socket, core> tuples on which the job have to be bound to */
   int* list_of_sockets    = nullptr;
   int* list_of_cores      = nullptr;
   int samount             = 0;
   int camount             = 0;

   /* topology used by job */
   char* topo_by_job       = nullptr;
   int topo_by_job_length  = 0;

   /* return value */
   bool retval = false;

   DENTER(TOP_LAYER);

   /* get mandatory parameters of -binding linear */
   amount = (int) lGetUlong(binding_elem, BN_parameter_n);
   type   = (binding_type_t) lGetUlong(binding_elem, BN_type);

   /* get <socket,core> tuples */
   if (get_linear_automatic_socket_core_list_and_account(amount,
         &list_of_sockets, &samount, &list_of_cores, &camount,
         &topo_by_job, &topo_by_job_length)) {

      int processor_set = 0;

      /* 4. create processor set */
      sge_switch2start_user();
      /* create the processor set with the given list of socket and cores */
      processor_set = create_processor_set_explicit_solaris(list_of_sockets,
                         samount, list_of_cores, camount, type, env);
      sge_switch2admin_user();

      /* 5. delete accounting when creating of processor set was not successful */
      if (processor_set < 0) {

         /* free the cores occupied by this job because we couldn't generate processor set */
         free_topology(topo_by_job, topo_by_job_length);

         DPRINTF("Couldn't create processor set");

         retval = false;
      } else {
         /* 6. write processor set id into "binding" in config file */

         /* record processor set id and the topology which the job consumes */
         if (type == BINDING_TYPE_ENV) {
            sge_dstring_sprintf(result, "env_psrset:%d:%s", -1, topo_by_job);
         } else if (type == BINDING_TYPE_PE) {
            sge_dstring_sprintf(result, "pe_psrset:%d:%s", -1, topo_by_job);
         } else {
            sge_dstring_sprintf(result, "psrset:%d:%s", processor_set, topo_by_job);
         }

         retval = true;
      }

   }

   sge_free(&list_of_cores);
   sge_free(&list_of_sockets);
   sge_free(&topo_by_job);

   DRETURN(retval);
}

/****** exec_job/striding_solaris() ********************************************
*  NAME
*     striding_solaris() -- Creates binding request string.
*
*  SYNOPSIS
*     static bool striding_solaris(dstring* result, lListElem* binding_elem,
*     const bool automatic, const bool do_linear, char* err_str, int
*     err_length, char** env)
*
*  FUNCTION
*     Tries to allocate processor cores according the request in the binding_elem.
*     If this is possible the cores are listed in the result string and true
*     is returned.
*     Striding means that cores with a specific distance (the step size) are
*     tried to be used. In case of automatic=false the first core to allocate
*     is given in the CULL list (binding_elem). If the request could not be
*     fulfilled the function returns false.
*
*  INPUTS
*     lListElem* binding_elem - CULL list containing the request.
*     const bool automatic    - Finds first core automatically.
*     const bool do_linear    - In case of linear request.
*
*  OUTPUTS
*     dstring* result         - String with core binding request.
*     char* err_str           - String containing errors in case of.
*     int err_length          - Length of the error string.
*     char** env              - Content of SGE_BINDING env var when requested.
*
*  RESULT
*     static bool - true in case of success false otherwise
*
*  NOTES
*     MT-NOTE: striding_solaris() is not MT safe
*
*******************************************************************************/
bool
ocs::BindingExecd2Shepherd::striding_solaris(dstring* result, const lListElem* binding_elem, const bool automatic, const bool do_linear, char* err_str, int err_length, char** env)
{
   /* 2. check if a starting point exist */
   int first_socket = 0;
   int first_core = 0;
   int used_first_socket = 0;
   int used_first_core = 0;
   int step_size;
   int amount;
   binding_type_t type;

   /* topology consumed by job */
   char* topo_by_job       = nullptr;
   int topo_by_job_length  = 0;

   /* return value */
   bool retval = false;

   DENTER(TOP_LAYER);

   /* get mandatory parameters of -binding striding */
   amount = (int) lGetUlong(binding_elem, BN_parameter_n);
   type   = (binding_type_t) lGetUlong(binding_elem, BN_type);

   if (!do_linear) {
      step_size = (int) lGetUlong(binding_elem, BN_parameter_striding_step_size);
   } else {
      /* in case of "linear" binding the stepsize is one */
      step_size = 1;
   }

   /* in automatic mode the socket,core pair which is bound first is determined
      automatically */
   if (!automatic) {
      /* get the start socket and start core which was a submission parameter */
      DPRINTF("Get user defined starting point for binding (socket, core)");
      first_socket = (int) lGetUlong(binding_elem, BN_parameter_socket_offset);
      first_core = (int) lGetUlong(binding_elem, BN_parameter_core_offset);
   } else {
      DPRINTF("Do determine starting point automatically");
   }

   /* try to allocate first core and first socket */
   if (get_striding_first_socket_first_core_and_account(amount, step_size,
         first_socket, first_core, automatic, &used_first_socket, &used_first_core,
         &topo_by_job, &topo_by_job_length)) {

      int processor_set = 0;

      /* check against errors: in automatic case the used first socket (and core)
         must be the same than the first socket (and core) from user parameters */
      if (!automatic && (first_socket != used_first_socket
            || first_core != used_first_core)) {
         /* we've a bug */
         DPRINTF("The starting point for binding is not like the user specified!");
      }

      /* we found a socket and a core we can use as start point */
      DPRINTF("Found a socket and a core as starting point for binding");

      /* 4. create processor set */

      sge_switch2start_user();

      processor_set = create_processor_set_striding_solaris(used_first_socket,
         used_first_core, amount, step_size, type, env);

      sge_switch2admin_user();

      /* 5. delete accounting when creating of processor set was not successful */
      if (processor_set < 0) {

         snprintf(err_str, err_length, "binding: couldn't create processor set");

         /* free the cores occupied by this job because we couldn't generate processor set */
         free_topology(topo_by_job, topo_by_job_length);

         DPRINTF("Couldn't create processor set");

         retval = false;
      } else {
         /* 6. write processor set id into "binding" in config file */

         snprintf(err_str, err_length, "binding: created processor set");

         retval = true;
         /* record processor set id and the topology which the job consumes */
         if (type == BINDING_TYPE_ENV) {
            sge_dstring_sprintf(result, "env_psrset:%d:%s", -1, topo_by_job);
         } else if (type == BINDING_TYPE_PE) {
            sge_dstring_sprintf(result, "pe_psrset:%d:%s", -1, topo_by_job);
         } else {
            sge_dstring_sprintf(result, "psrset:%d:%s", processor_set, topo_by_job);
         }

      }

   } else {

      snprintf(err_str, err_length, "binding: strategy does not fit on execution host");

      DPRINTF("Didn't find socket,core to start binding with");
      retval = false;
   }

   sge_free(&topo_by_job);

   DRETURN(retval);
}

/****** exec_job/explicit_solaris() ********************************************
*  NAME
*     explicit_solaris() -- Creates a binding request string from request (CULL list).
*
*  SYNOPSIS
*     static bool explicit_solaris(dstring* result, lListElem* binding_elem,
*     char* err_str, int err_length, char** env)
*
*  FUNCTION
*     Creates a binding request string out of the explicit core binding request.
*
*  INPUTS
*     lListElem* binding_elem - List containing the binding request.
*
*  OUTPUTS
*     dstring* result         - String containing the core bindin request.
*     char* err_str           - String containing possible errors.
*     int err_length          - Length of the error string.
*     char** env              - String containing the SGE_BINDING env var content
*                               when requested.
*
*  RESULT
*     static bool - true in case the request could be fulfilled otherwise false
*
*  NOTES
*     MT-NOTE: explicit_solaris() is not MT safe
*
*******************************************************************************/
bool ocs::BindingExecd2Shepherd::explicit_solaris(dstring* result, const lListElem* binding_elem, char* err_str, int err_length, char** env)
{
   /* pointer to string which contains the <socket>,<core> pairs */
   const char* request = nullptr;

   /* the from the request extracted sockets and cores (to bind to) */
   int* socket_list = nullptr;
   int* core_list = nullptr;
   int socket_list_length, core_list_length;

   int processor_set = 0;
   binding_type_t type;
   bool retval = false;

   DENTER(TOP_LAYER);

   /* get the socket and core numbers */
   request = (char*) lGetString(binding_elem, BN_parameter_explicit);
   type   = (binding_type_t) lGetUlong(binding_elem, BN_type);


   INFO("request: %s", request);

   if (!binding_explicit_extract_sockets_cores(request, &socket_list,
            &socket_list_length, &core_list, &core_list_length)) {
      /* problems while parsing the binding request */
      snprintf(err_str, err_length, "binding: couldn't parse explicit parameter");
      INFO("Couldn't parse binding explicit parameter");
      retval = false;
   } else {
      /* the topology used by the job */
      char* topo_by_job = nullptr;
      int topo_by_job_length;

      /* check if socket and core numbers are free */
      if (binding_explicit_check_and_account(socket_list, socket_list_length,
         core_list, core_list_length, &topo_by_job, &topo_by_job_length)) {
         /* it is possible to bind to the given cores */

         /* create the processor set as user root */
         sge_switch2start_user();

         processor_set = create_processor_set_explicit_solaris(socket_list,
            socket_list_length, core_list, core_list_length, type, env);

         sge_switch2admin_user();

         if (processor_set < 0) {
            /* creating processor set was not possible (could be also because
               these are ALL remaining cores on system [one must left])
               TODO DG check this in create_processor_set method */
            snprintf(err_str, err_length, "binding: couldn't create processor set");
            /* free the cores occupied by this job because we couldn't generate processor set */
            free_topology(topo_by_job, topo_by_job_length);
            INFO("Could't create processor set in order to bind job to.");
            retval = false;
         } else {

            /* record processor set id and the topology which the job consumes */
            if (type == BINDING_TYPE_ENV) {
               sge_dstring_sprintf(result, "env_psrset:%d:%s", -1, topo_by_job);
            } else if (type == BINDING_TYPE_PE) {
               sge_dstring_sprintf(result, "pe_psrset:%d:%s", -1, topo_by_job);
            } else {
               sge_dstring_sprintf(result, "psrset:%d:%s", processor_set, topo_by_job);
            }

            /* write processor set id into "binding" in config file */
            snprintf(err_str, err_length, "binding: created processor set");

            retval = true;
         }

      } else {
         /* "binding explicit" with the given cores is not possible */
         snprintf(err_str, err_length, "binding: strategy does not fit on execution host");
         INFO("Binding strategy does not fit on execution host");
         retval = false;
      }

      sge_free(&core_list);
      sge_free(&socket_list);
      sge_free(&topo_by_job);
   }

   DRETURN(retval);
}

#endif
