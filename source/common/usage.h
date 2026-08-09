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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Interface of the shared usage/`-help` output
 */

#include <cstdlib>

#include <cinttypes>

/* 
   for use in mark_argument_syntax() calls:

   these values correspond to an array of 
   argument syntax description texts in usage.c

   note: both are sorted alphabetically (array and enum)

*/
enum {
   OA_ACCOUNT_STRING,   ///< `account_string - account_name`
   OA_ALLOCATION_RULE,   ///< `allocation_rule - <int> | $pe_slots | $fill_up | $round_robin`
   OA_CATEGORY_ID,   ///< `cat_id - category ID`
   OA_COMPLEX_LIST,   ///< `complex_list - complex[,complex,...]`
   OA_CONTEXT_LIST,   ///< `context_list - variable[=value][,variable[=value],...]`
   OA_CKPT_SEL,   ///< `ckpt_selector - 'n' 's' 'm' 'x' <interval>`
   OA_DATE_TIME,   ///< `date_time - [[CC]YY]MMDDhhmm[.SS]`
   OA_DESTIN_ID_LIST,   ///< `destin_id_list - queue[,queue,...]`
   OA_HOLD_LIST,   ///< `hold_list - 'n' 'u' 's' 'o' 'U' 'S' 'O'`
   OA_HOST_ID_LIST,   ///< `host_id_list - host[ host ...]`
   OA_HOSTNAME_LIST,   ///< `hostname_list - hostname[,hostname,...]`
   OA_JOB_ID_LIST,   ///< `job_id_list - job_id[,job_id,...]`
   OA_JOB_IDENTIFIER_LIST,   ///< `job_identifier_list - {job_id|job_name|reg_exp}[,{job_id|job_name|reg_exp},...]`
   OA_JOB_QUEUE_DEST,   ///< `job_queue_list - {job|queue}[{,| }{job|queue}{,| }...]`
   OA_JSV_URL,   ///< `jsv_url - [script:][username@]path`
   OA_LISTNAME_LIST,   ///< `listname_list - listname[,listname,...]`
   OA_RQS_LIST,   ///< `rqs_list - rqs_name[,rqs_name,...]`
   OA_MAIL_ADDRESS,   ///< `mail_address - username[@host]`
   OA_MAIL_LIST,   ///< `mail_list - mail_address[,mail_address,...]`
   OA_MAIL_OPTIONS,   ///< `mail_options - 'e' 'b' 'a' 'n' 's'`
   OA_MAIL_OPTIONS_AR,   ///< `mail_options - 'e' 'b' 'a' 'n' (end, begin, error, no)`
   OA_NODE_LIST,   ///< `node_list - node_path[,node_path,...]`
   OA_NODE_PATH,   ///< `node_path - [/]node_name[[/.]node_name...]`
   OA_NODE_SHARES_LIST,   ///< `node_shares_list - node_path=shares[,node_path=shares,...]`
   OA_PATH,   ///< `working_directory - path`
   OA_PATH_LIST,   ///< `path_list - [host:]path[,[host:]path,...]`
   OA_FILE_LIST,   ///< `file_list - [host:]file[,[host:]file,...]`
   OA_PRIORITY,   ///< `priority - -1023 - 1024`
   OA_RESOURCE_LIST,   ///< `resource_list - resource[=value][,resource[=value],...]`
   OA_SCOPE_NAME,   ///< `scope_name - 'global' 'master' 'slave'`
   OA_SERVER,   ///< `server - hostname`
   OA_SERVER_LIST,   ///< `server_list - server[,server,...]`
   OA_SIGNAL,   ///< `signal - -int_val, symbolic names`
   OA_SIMPLE_CONTEXT_LIST,   ///< `simple_context_list - variable[,variable,...]`
   OA_SLOT_RANGE,   ///< `slot_range - [n[-m]|[-]m] - n,m > 0`
   OA_STATES,   ///< `states - 'e' 'q' 'r' 't' 'h' 'w' 'm' 's'`
   OA_JOB_TASK_LIST,   ///< `job_task_list - job_tasks[,job_tasks,...]`
   OA_JOB_TASKS,   ///< `job_tasks - [job_id['.'task_id_range]|job_name|pattern][' -t 'task_id_range]`
   OA_TASK_ID_RANGE,   ///< `task_id_range - task_id['-'task_id[':'step]]`
   OA_USER_LIST,   ///< `user_list - user[,user,...]`
   OA_VARIABLE_LIST,   ///< `variable_list - variable[=value][,variable[=value],...]`
   OA_OBJECT_NAME,   ///< `obj_nm - \`
   OA_ATTRIBUTE_NAME,   ///< `attr_nm - (see man pages)`
   OA_OBJECT_ID_LIST,   ///< `obj_id_list - objectname [ objectname ...]`
   OA_PROJECT_LIST,   ///< `project_list - project[,project,...]`
   OA_EVENTCLIENT_LIST,   ///< `evid_list - all | evid[,evid,...]`
   OA_HOST_LIST,   ///< `host_list - all | hostname[,hostname,...]`
   OA_WC_CQUEUE,   ///< `wc_cqueue - wildcard expression matching a cluster queue`
   OA_WC_HOST,   ///< `wc_host - wildcard expression matching a host`
   OA_WC_HOSTGROUP,   ///< `wc_hostgroup - wildcard expression matching a hostgroup`
   OA_WC_QINSTANCE,   ///< `wc_qinstance - wc_cqueue@wc_host`
   OA_WC_QDOMAIN,   ///< `wc_qdomain - wc_cqueue@wc_hostgroup`
   OA_WC_QUEUE,   ///< `wc_queue - wc_cqueue|wc_qdomain|wc_qinstance`
   OA_WC_QUEUE_LIST,   ///< `wc_queue_list - wc_queue[,wc_queue,...]`
   OA_OBJECT_NAME2,   ///< `obj_nm2 - \`
   OA_OBJECT_NAME3,   ///< `obj_nm3 - \`
   OA_TIME,   ///< `time - hours:minutes:seconds | seconds`
   OA_AR_ID,   ///< `ar_id - advance reservation id`
   OA_AR_ID_LIST,   ///< `ar_id_list - ar_id[,ar_id,...]`
   OA_WC_AR_LIST,   ///< `wc_ar_list - wc_ar[,wc_ar,...]`
   OA_WC_AR,   ///< `wc_ar - ar_id|ar_name|pattern`
   OA_THREAD_NAME,   ///< `thread_name - \`
   OA_TASK_CONCURRENCY,   ///< `max_running_tasks - maximum number of simultaneously running tasks`
   OA_BINDING_FILTER,   ///< `topology_string - topology string where lower case letters show masked units`

   OA__END   ///< not a placeholder; the number of them, used to size `marker[]`
};

/** @brief Note that this client uses a given argument placeholder
 *
 * The usage output prints the syntax of each placeholder only once, at the end,
 * and only for the placeholders this client actually uses. Every option printed
 * therefore marks the placeholders it names.
 *
 * @param argument_number one of the `OA_*` values above
 */
void mark_argument_syntax(int argument_number);
/** @brief Print the usage text of a client
 *
 * @param prog_number the client to print the usage of
 * @param fp where to print it - `stdout` for `-help`, `stderr` for a usage error
 */
void sge_usage(ProgName prog_number, FILE *fp);
