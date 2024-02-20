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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "basis_types.h"
#include "sge_dstring.h"
#include "uti/sge_component.h"

#define PATH_SEPARATOR "/"
#define COMMON_DIR "common"
#define BOOTSTRAP_FILE "bootstrap"
#define CONF_FILE "configuration"
#define SCHED_CONF_FILE "sched_configuration"
#define ACCT_FILE "accounting"
#define REPORTING_FILE "reporting"
#define LOCAL_CONF_DIR "local_conf"
#define SHADOW_MASTERS_FILE "shadow_masters"

#define PATH_SEPARATOR "/"
#define PATH_SEPARATOR_CHAR '/'

const char *
bootstrap_get_cell_root();

const char *
bootstrap_get_bootstrap_file();

const char *
bootstrap_get_conf_file();

const char *
bootstrap_get_sched_conf_file();

const char *
bootstrap_get_act_qmaster_file();

const char *
bootstrap_get_acct_file();

const char *
bootstrap_get_reporting_file();

const char *
bootstrap_get_local_conf_dir();

const char *
bootstrap_get_shadow_masters_file();

const char *
bootstrap_get_alias_file();
