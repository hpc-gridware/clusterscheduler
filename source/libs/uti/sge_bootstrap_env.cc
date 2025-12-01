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
 *  Portions of this software are Copyright (c) 2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstring>

#include <pthread.h>

#include "basis_types.h"

#include "uti/sge_bootstrap_env.h"
#include "uti/sge_dstring.h"
#include "uti/sge_hostname.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_mtutil.h"

#include "sge.h"

// thread local storage (level 1)
// initialization depends on data of level 0 (e.g. logging)
// TODO: data can partially be shared between threads. cleanup required.
typedef struct {
   // environment
   char *sge_root;
   char *sge_cell;
   u_long32 sge_qmaster_port;
   u_long32 sge_execd_port;
   bool from_services;
} sge_bootstrap_ts1_t;

static pthread_once_t bootstrap_env_once = PTHREAD_ONCE_INIT;

static pthread_key_t sge_bootstrap_env_tl1_key;

static void
bootstrap_env_thread_local_once_init();

static void
bootstrap_env_tl1_init(sge_bootstrap_ts1_t *tl);

static void
bootstrap_env_tl1_destroy(void *tl);

static void
set_sge_root(sge_bootstrap_ts1_t *tl, const char *sge_root) {
   tl->sge_root = sge_strdup(tl->sge_root, sge_root);
}

static void
set_sge_cell(sge_bootstrap_ts1_t *tl, const char *sge_cell) {
   tl->sge_cell = sge_strdup(tl->sge_cell, sge_cell);
}

static void
set_sge_qmaster_port(sge_bootstrap_ts1_t *tl, u_long32 sge_qmaster_port) {
   tl->sge_qmaster_port = sge_qmaster_port;
}

static void
set_sge_execd_port(sge_bootstrap_ts1_t *tl, u_long32 sge_execd_port) {
   tl->sge_execd_port = sge_execd_port;
}

static void
set_from_services(sge_bootstrap_ts1_t *tl, bool from_services) {
   tl->from_services = from_services;
}

static void
bootstrap_env_log_tl1_parameter(sge_bootstrap_ts1_t *tl) {
   DENTER(TOP_LAYER);

   DPRINTF("ENVIRONMENT ===\n");
   DPRINTF("   sge_root             >%s<\n", tl->sge_root ? tl->sge_root : "NA");
   DPRINTF("   sge_cell             >%s<\n", tl->sge_cell ? tl->sge_cell : "NA");
   DPRINTF("   sge_qmaster_port     >%d<\n", tl->sge_qmaster_port);
   DPRINTF("   sge_execd_port       >%d<\n", tl->sge_execd_port);
   DPRINTF("   from_services        >%s<\n", tl->from_services ? "true" : "false");

   DRETURN_VOID;
}

static void
bootstrap_env_mt_init() {
   pthread_once(&bootstrap_env_once, bootstrap_env_thread_local_once_init);
}

class BootstrapEnvThreadInit {
public:
   BootstrapEnvThreadInit() {
      bootstrap_env_mt_init();
   }
};

// although not used the constructor call has the side effect to initialize the pthread_key => do not delete
static BootstrapEnvThreadInit bootstrap_env_obj{};

static void
bootstrap_env_thread_local_once_init() {
   pthread_key_create(&sge_bootstrap_env_tl1_key, bootstrap_env_tl1_destroy);
}

static void
bootstrap_env_init_from_environment(sge_bootstrap_ts1_t *tl) {
   DENTER(TOP_LAYER);

   set_sge_root(tl, getenv("SGE_ROOT"));

   const char *sge_cell = getenv("SGE_CELL");
   set_sge_cell(tl, sge_cell != nullptr ? sge_cell : DEFAULT_CELL);

   bool from_services = false;
   set_sge_qmaster_port(tl, sge_get_qmaster_port(&from_services));
   set_from_services(tl, from_services);

   set_sge_execd_port(tl, sge_get_execd_port());

   DRETURN_VOID;
}

static void
bootstrap_env_tl1_init(sge_bootstrap_ts1_t *tl) {
   DENTER(TOP_LAYER);
   static bool already_shown = false;
   memset(tl, 0, sizeof(sge_bootstrap_ts1_t));

   bootstrap_env_init_from_environment(tl);

   if (!already_shown) {
      bootstrap_env_log_tl1_parameter(tl);
      already_shown = true;
   }
   DRETURN_VOID;
}

static void
bootstrap_env_tl1_destroy(void *tl) {
   auto _tl = (sge_bootstrap_ts1_t *) tl;

   // environment parameters
   sge_free(&(_tl->sge_root));
   sge_free(&(_tl->sge_cell));

   // wrapping structure
   sge_free(&_tl);
}

const char *
bootstrap_get_sge_root() {
   GET_SPECIFIC(sge_bootstrap_ts1_t, tl, bootstrap_env_tl1_init, sge_bootstrap_env_tl1_key);
   return tl->sge_root;
}

const char *
bootstrap_get_sge_cell() {
   GET_SPECIFIC(sge_bootstrap_ts1_t, tl, bootstrap_env_tl1_init, sge_bootstrap_env_tl1_key);
   return tl->sge_cell;
}

u_long32
bootstrap_get_sge_qmaster_port() {
   GET_SPECIFIC(sge_bootstrap_ts1_t, tl, bootstrap_env_tl1_init, sge_bootstrap_env_tl1_key);
   return tl->sge_qmaster_port;
}

u_long32
bootstrap_get_sge_execd_port() {
   GET_SPECIFIC(sge_bootstrap_ts1_t, tl, bootstrap_env_tl1_init, sge_bootstrap_env_tl1_key);
   return tl->sge_execd_port;
}

bool
bootstrap_is_from_services() {
   GET_SPECIFIC(sge_bootstrap_ts1_t, tl, bootstrap_env_tl1_init, sge_bootstrap_env_tl1_key);
   return tl->from_services;
}
