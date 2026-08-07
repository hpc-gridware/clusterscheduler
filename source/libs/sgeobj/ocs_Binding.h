#pragma once
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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Binding a job to specific CPU hardware
 */

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <cinttypes>

#include "cull/cull_list.h"

#include "uti/sge_dstring.h"
#include "uti/sge_string.h"
#include "uti/sge_spool.h"

#include "sgeobj/sge_object.h"
#include "sgeobj/cull/sge_binding_BN_L.h"

#include "err_trace.h"

#if defined(LINUX)
#  include <dlfcn.h>
#endif

/* functions related to get load values for execd (see load_avg.c) */

/* get the amount of cores available on the execution host */ 
int get_execd_amount_of_cores();

/* get the amount of sockets of the execution host */
int get_execd_amount_of_sockets();

/* get the amount of hardware supported threads for the specific exec host */
int get_execd_amount_of_threads();

/// Fetch the topology string listing every core installed on this host
bool get_execd_topology(char** topology, int* length);

/* get the topology string where all cores currently in use are marked */
bool get_execd_topology_in_use(char** topology);

#if defined(OCS_HWLOC) || defined(BINDING_SOLARIS)
bool account_job(const char* job_topology);
 
/**
 * @brief Bind to cores in a striding pattern
 * @param first_socket socket to start at
 * @param first_core core within that socket to start at
 * @param amount_of_cores how many cores to take
 * @param offset cores to skip before the first one is taken
 * @param stepsize distance between two taken cores
 * @param[out] reason receives a message when the binding is not possible
 * @return true when the binding was applied
 */
bool binding_set_striding(int first_socket, int first_core, int amount_of_cores,
      int offset, int stepsize, char** reason);

/**
 * @brief Bind to one core on each of several sockets
 * @param first_socket socket to start at
 * @param amount_of_sockets how many sockets to use
 * @param n index of the core to take on each socket
 * @return true when the binding was applied
 */
bool binding_one_per_socket(int first_socket, int amount_of_sockets, int n);

/**
 * @brief Bind to several cores on each of several sockets
 * @param first_socket socket to start at
 * @param amount_of_sockets how many sockets to use
 * @param n how many cores to take on each socket
 * @return true when the binding was applied
 */
bool binding_n_per_socket(int first_socket, int amount_of_sockets, int n);

/**
 * @brief Split an explicit binding request into its socket and core numbers
 * @param parameter the request as the user wrote it, e.g. `0,0:0,1`
 * @param[out] list_of_sockets receives the socket number of each pair
 * @param[out] samount receives the length of `list_of_sockets`
 * @param[out] list_of_cores receives the core number of each pair
 * @param[out] camount receives the length of `list_of_cores`
 * @return true when the request could be parsed
 */
bool binding_explicit_exctract_sockets_cores(const char* parameter, int** list_of_sockets, 
   int* samount, int** list_of_cores, int* camount);

bool binding_explicit_check_and_account(const int* list_of_sockets, const int samount, 
   const int* list_of_cores, const int score, char** topo_used_by_job, 
   int* topo_used_by_job_length);

/// Pick `amount` free cores in linear order, account them and report which ones were taken
bool get_linear_automatic_socket_core_list_and_account(const int amount, 
      int** list_of_sockets, int* samount, int** list_of_cores, int* camount, 
      char** topo_by_job, int* topo_by_job_length);

/* functions related to get load values for execd (see load_avg.c) */
/**
 * @brief Number of cores available on this execution host, reported as a load value
 * @return the core count, or 0 when the topology is unknown
 */
int getExecdAmountOfCores();

/**
 * @brief Number of sockets of this execution host, reported as a load value
 * @return the socket count, or 0 when the topology is unknown
 */
int getExecdAmountOfSockets();

/* function for determining the binding */
bool get_striding_first_socket_first_core_and_account(const int amount, const int stepsize,
   const int start_at_socket, const int start_at_core, const bool automatic,
   int* first_socket, int* first_core,
   char** accounted_topology, int* accounted_topology_length);

/// Read the host's topology once at execd startup, so accounting has something to mark
bool initialize_topology();

/**
 * @brief Is the given core already accounted to a job?
 * @param socket the socket the core sits on
 * @param core the core within that socket
 * @return true when a job holds it
 */
bool topology_is_in_use(const int socket, const int core);

/* free cores on execution host which were used by a job */
bool free_topology(const char* topology, const int topology_length);

#endif

#if defined(BINDING_SOLARIS)

int create_processor_set_striding_solaris(const int first_socket,
   const int first_core, const int amount, const int step_size, 
   const binding_type_t type, char** env);

int create_processor_set_explicit_solaris(const int* list_of_sockets,
   const int samount, const int* list_of_cores, const int camount,
   const binding_type_t type, char** env);

/* matrix represents internal kstat structure */
bool generate_chipID_coreID_matrix(int*** matrix, int* length);

/* frees the memory allocated by the topology matrix */
void free_matrix(int** matrix, const int length);
#endif

