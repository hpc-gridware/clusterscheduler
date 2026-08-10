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
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2024-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Starting the job itself: the environment, the shell, and the exec
 *
 * The last thing the shepherd does before the job replaces it. Everything the
 * job will see - its environment, its shell, its arguments, its standard
 * streams - is set up here, in the child process, after the limits are applied
 * and the privileges dropped.
 *
 * The environment is not simply inherited. The execution daemon writes an
 * `environment` file, and the shepherd builds the job's environment from that,
 * either on top of its own or from nothing, depending on the configuration.
 */

void son(const char *childname, char *script_file, int truncate_stderr_out, bool is_interactive_job);
int sge_set_environment();
char** sge_get_environment();
int sge_set_env_value(const char *name, const char* value);
const char *sge_get_env_value(const char *name);
void start_command(const char *childname, char *shell_path, char *script_file, char *argv0,
                   const char *shell_start_mode, int is_interactive, int is_qlogin, int is_rsh, int is_rlogin,
                   const char *str_title, int use_starter_method);
int check_configured_method(const char *method, const char *name, char *err_str, size_t err_str_size);
char* build_path(int type);
