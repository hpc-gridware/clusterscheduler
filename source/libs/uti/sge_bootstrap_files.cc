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

#include <pthread.h>

#include "basis_types.h"

#include "uti/sge_bootstrap.h"
#include "uti/sge_bootstrap_env.h"
#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_spool.h"
#include "uti/sge_string.h"
#include "uti/sge_uidgid.h"

#include "uti/msg_utilib.h"

#include "sge.h"

// thread local storage (level 1)
// initialization depends on data of level 0 (e.g. logging)
// TODO: data can partially be shared between threads. cleanup required.
typedef struct {
   // files and paths
   char *cell_root;
   char *bootstrap_file;
   char *conf_file;
   char *sched_conf_file;
   char *act_qmaster_file;
   char *acct_file;
   char *reporting_file;
   char *local_conf_dir;
   char *shadow_masters_file;
   char *alias_file;
} sge_bootstrap_files_tl1_t;

static pthread_once_t bootstrap_files_once = PTHREAD_ONCE_INIT;

static pthread_key_t sge_bootstrap_files_tl1_key;

static void
bootstrap_files_thread_local_once_init();

static void
bootstrap_files_tl1_init(sge_bootstrap_files_tl1_t *tl);

static void
bootstrap_files_tl1_destroy(void *tl);

static void
set_cell_root(sge_bootstrap_files_tl1_t *tl, const char *cell_root) {
   tl->cell_root = sge_strdup(tl->cell_root, cell_root);
}

static void
set_conf_file(sge_bootstrap_files_tl1_t *tl, const char *conf_file) {
   tl->conf_file = sge_strdup(tl->conf_file, conf_file);
}

static void
set_bootstrap_file(sge_bootstrap_files_tl1_t *tl, const char *bootstrap_file) {
   tl->bootstrap_file = sge_strdup(tl->bootstrap_file, bootstrap_file);
}

static void
set_act_qmaster_file(sge_bootstrap_files_tl1_t *tl, const char *act_qmaster_file) {
   tl->act_qmaster_file = sge_strdup(tl->act_qmaster_file, act_qmaster_file);
}

static void
set_acct_file(sge_bootstrap_files_tl1_t *tl, const char *acct_file) {
   tl->acct_file = sge_strdup(tl->acct_file, acct_file);
}

static void
set_reporting_file(sge_bootstrap_files_tl1_t *tl, const char *reporting_file) {
   tl->reporting_file = sge_strdup(tl->reporting_file, reporting_file);
}

static void
set_local_conf_dir(sge_bootstrap_files_tl1_t *tl, const char *local_conf_dir) {
   tl->local_conf_dir = sge_strdup(tl->local_conf_dir, local_conf_dir);
}

static void
set_shadow_masters_file(sge_bootstrap_files_tl1_t *tl, const char *shadow_masters_file) {
   tl->shadow_masters_file = sge_strdup(tl->shadow_masters_file, shadow_masters_file);
}

static void
set_sched_conf_file(sge_bootstrap_files_tl1_t *tl, const char *sched_conf_file) {
   tl->sched_conf_file = sge_strdup(tl->sched_conf_file, sched_conf_file);
}

static void
set_alias_file(sge_bootstrap_files_tl1_t *tl, const char *alias_file) {
   tl->alias_file = sge_strdup(tl->alias_file, alias_file);
}

static void
bootstrap_files_log_tl1_parameter(sge_bootstrap_files_tl1_t *tl) {
   DENTER(TOP_LAYER);

   DPRINTF(("FILES AND PATHS ===\n"));
   DPRINTF(("   cell_root            >%s<\n", tl->cell_root));
   DPRINTF(("   conf_file            >%s<\n", tl->bootstrap_file));
   DPRINTF(("   bootstrap_file       >%s<\n", tl->conf_file));
   DPRINTF(("   act_qmaster_file     >%s<\n", tl->act_qmaster_file));
   DPRINTF(("   acct_file            >%s<\n", tl->acct_file));
   DPRINTF(("   reporting_file       >%s<\n", tl->reporting_file));
   DPRINTF(("   local_conf_dir       >%s<\n", tl->local_conf_dir));
   DPRINTF(("   shadow_masters_file  >%s<\n", tl->shadow_masters_file));
   DPRINTF(("   alias_file           >%s<\n", tl->alias_file));

   DRETURN_VOID;
}

static void
bootstrap_files_mt_init() {
   pthread_once(&bootstrap_files_once, bootstrap_files_thread_local_once_init);
}

class BootstrapFilesThreadInit {
public:
   BootstrapFilesThreadInit() {
      bootstrap_files_mt_init();
   }
};

// although not used the constructor call has the side effect to initialize the pthread_key => do not delete
static BootstrapFilesThreadInit bootstrap_files_obj{};

static void
bootstrap_files_thread_local_once_init() {
   pthread_key_create(&sge_bootstrap_files_tl1_key, bootstrap_files_tl1_destroy);
}

static void
bootstrap_init_paths(sge_bootstrap_files_tl1_t *tl) {
   DENTER(TOP_LAYER);
   const char *sge_root = bootstrap_get_sge_root();
   const char *sge_cell = bootstrap_get_sge_cell();
   char buffer[2 * 1024];
   dstring bw;

   sge_dstring_init(&bw, buffer, sizeof(buffer));

   SGE_STRUCT_STAT sbuf{};
   if (SGE_STAT(sge_root, &sbuf)) {
      CRITICAL(MSG_SGETEXT_SGEROOTNOTFOUND_S, sge_root);
      DRETURN_VOID;
   }

   if (!S_ISDIR(sbuf.st_mode)) {
      CRITICAL(MSG_UTI_SGEROOTNOTADIRECTORY_S, sge_root);
      DRETURN_VOID;
   }

   /* cell_root */
   sge_dstring_sprintf(&bw, "%s" PATH_SEPARATOR "%s", sge_root, sge_cell);

   if (SGE_STAT(sge_dstring_get_string(&bw), &sbuf)) {
      CRITICAL(MSG_SGETEXT_NOSGECELL_S, sge_dstring_get_string(&bw));
      DRETURN_VOID;
   }

   set_cell_root(tl, sge_dstring_get_string(&bw));
   const char *cell_root = tl->cell_root;

   /* common dir */
   sge_dstring_sprintf(&bw, "%s" PATH_SEPARATOR "%s", cell_root, COMMON_DIR);
   if (SGE_STAT(buffer, &sbuf)) {
      CRITICAL(MSG_UTI_DIRECTORYNOTEXIST_S, buffer);
      DRETURN_VOID;
   }

   sge_dstring_sprintf(&bw, "%s" PATH_SEPARATOR "%s" PATH_SEPARATOR "%s", cell_root, COMMON_DIR, BOOTSTRAP_FILE);
   set_bootstrap_file(tl, sge_dstring_get_string(&bw));

   sge_dstring_sprintf(&bw, "%s" PATH_SEPARATOR "%s" PATH_SEPARATOR "%s", cell_root, COMMON_DIR, CONF_FILE);
   set_conf_file(tl, sge_dstring_get_string(&bw));

   sge_dstring_sprintf(&bw, "%s" PATH_SEPARATOR "%s" PATH_SEPARATOR "%s", cell_root, COMMON_DIR, SCHED_CONF_FILE);
   set_sched_conf_file(tl, sge_dstring_get_string(&bw));

   sge_dstring_sprintf(&bw, "%s" PATH_SEPARATOR "%s" PATH_SEPARATOR "%s", cell_root, COMMON_DIR, ACT_QMASTER_FILE);
   set_act_qmaster_file(tl, sge_dstring_get_string(&bw));

   sge_dstring_sprintf(&bw, "%s" PATH_SEPARATOR "%s" PATH_SEPARATOR "%s", cell_root, COMMON_DIR, ACCT_FILE);
   set_acct_file(tl, sge_dstring_get_string(&bw));

   sge_dstring_sprintf(&bw, "%s" PATH_SEPARATOR "%s" PATH_SEPARATOR "%s", cell_root, COMMON_DIR, REPORTING_FILE);
   set_reporting_file(tl, sge_dstring_get_string(&bw));

   sge_dstring_sprintf(&bw, "%s" PATH_SEPARATOR "%s" PATH_SEPARATOR "%s", cell_root, COMMON_DIR, LOCAL_CONF_DIR);
   set_local_conf_dir(tl, sge_dstring_get_string(&bw));

   sge_dstring_sprintf(&bw, "%s" PATH_SEPARATOR "%s" PATH_SEPARATOR "%s", cell_root, COMMON_DIR, SHADOW_MASTERS_FILE);
   set_shadow_masters_file(tl, sge_dstring_get_string(&bw));

   sge_dstring_sprintf(&bw, "%s" PATH_SEPARATOR "%s" PATH_SEPARATOR "%s", cell_root, COMMON_DIR, ALIAS_FILE);
   set_alias_file(tl, sge_dstring_get_string(&bw));

   DRETURN_VOID;
}

static void
bootstrap_files_tl1_init(sge_bootstrap_files_tl1_t *tl) {
   DENTER(TOP_LAYER);
   static bool already_shown = false;
   memset(tl, 0, sizeof(sge_bootstrap_files_tl1_t));

   bootstrap_init_paths(tl);

   if (!already_shown) {
      bootstrap_files_log_tl1_parameter(tl);
      already_shown = true;
   }
   DRETURN_VOID;
}

static void
bootstrap_files_tl1_destroy(void *tl) {
   auto _tl = (sge_bootstrap_files_tl1_t *) tl;

   // files and paths
   sge_free(&(_tl->cell_root));
   sge_free(&(_tl->bootstrap_file));
   sge_free(&(_tl->conf_file));
   sge_free(&(_tl->sched_conf_file));
   sge_free(&(_tl->act_qmaster_file));
   sge_free(&(_tl->acct_file));
   sge_free(&(_tl->reporting_file));
   sge_free(&(_tl->local_conf_dir));
   sge_free(&(_tl->shadow_masters_file));
   sge_free(&(_tl->alias_file));

   // wrapping structure
   sge_free(&_tl);
}

const char *
bootstrap_get_cell_root() {
   GET_SPECIFIC(sge_bootstrap_files_tl1_t, tl, bootstrap_files_tl1_init, sge_bootstrap_files_tl1_key);
   return tl->cell_root;
}

const char *
bootstrap_get_bootstrap_file() {
   GET_SPECIFIC(sge_bootstrap_files_tl1_t, tl, bootstrap_files_tl1_init, sge_bootstrap_files_tl1_key);
   return tl->bootstrap_file;
}

const char *
bootstrap_get_conf_file() {
   GET_SPECIFIC(sge_bootstrap_files_tl1_t, tl, bootstrap_files_tl1_init, sge_bootstrap_files_tl1_key);
   return tl->conf_file;
}

const char *
bootstrap_get_sched_conf_file() {
   GET_SPECIFIC(sge_bootstrap_files_tl1_t, tl, bootstrap_files_tl1_init, sge_bootstrap_files_tl1_key);
   return tl->sched_conf_file;
}

const char *
bootstrap_get_act_qmaster_file() {
   GET_SPECIFIC(sge_bootstrap_files_tl1_t, tl, bootstrap_files_tl1_init, sge_bootstrap_files_tl1_key);
   return tl->act_qmaster_file;
}

const char *
bootstrap_get_acct_file() {
   GET_SPECIFIC(sge_bootstrap_files_tl1_t, tl, bootstrap_files_tl1_init, sge_bootstrap_files_tl1_key);
   return tl->acct_file;
}

const char *
bootstrap_get_reporting_file() {
   GET_SPECIFIC(sge_bootstrap_files_tl1_t, tl, bootstrap_files_tl1_init, sge_bootstrap_files_tl1_key);
   return tl->reporting_file;
}

const char *
bootstrap_get_local_conf_dir() {
   GET_SPECIFIC(sge_bootstrap_files_tl1_t, tl, bootstrap_files_tl1_init, sge_bootstrap_files_tl1_key);
   return tl->local_conf_dir;
}

const char *
bootstrap_get_shadow_masters_file() {
   GET_SPECIFIC(sge_bootstrap_files_tl1_t, tl, bootstrap_files_tl1_init, sge_bootstrap_files_tl1_key);
   return tl->shadow_masters_file;
}

const char *
bootstrap_get_alias_file() {
   GET_SPECIFIC(sge_bootstrap_files_tl1_t, tl, bootstrap_files_tl1_init, sge_bootstrap_files_tl1_key);
   return tl->alias_file;
}
