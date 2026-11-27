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
 *  Portions of this software are Copyright (c) 2024-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Paths of the files in the cell's `common` directory
 *
 * Every path is derived once per thread from `$SGE_ROOT` and `$SGE_CELL` (see
 * #bootstrap_get_sge_root and #bootstrap_get_sge_cell) and cached in
 * thread-local storage, so callers can treat the getters as cheap.
 *
 * If `$SGE_ROOT` cannot be stat()ed, is not a directory, or the cell or its
 * `common` directory is missing, a `CRITICAL` message is logged and the paths
 * are left unset — **the getters then return nullptr** rather than aborting.
 * Callers that cannot proceed without a path must check, as
 * `ocs::Bootstrap::init_from_file()` does for the bootstrap file.
 */

#include <cstring>
#include <optional>
#include <string>

#include <pthread.h>

#include <cinttypes>

#include "uti/ocs_Bootstrap.h"
#include "uti/sge_bootstrap_env.h"
#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_spool.h"
#include "uti/sge_string.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_unistd.h"

#include "uti/msg_utilib.h"

#include "sge.h"

/**
 * @brief Thread local storage (level 1) holding the cell's file paths
 *
 * Level 1 because its initialisation depends on level 0 data such as logging.
 * Every member is owned by the struct, which `bootstrap_files_tl1_destroy()`
 * destroys; all of them are std::nullopt (the getters return nullptr) when
 * `bootstrap_init_paths()` failed.
 *
 * @todo data can partially be shared between threads. cleanup required.
 */
typedef struct {
   // files and paths
   std::optional<std::string> cell_root;           ///< `$SGE_ROOT/$SGE_CELL`
   std::optional<std::string> bootstrap_file;      ///< `<cell_root>/common/`#BOOTSTRAP_FILE
   std::optional<std::string> act_qmaster_file;    ///< `<cell_root>/common/act_qmaster`, naming the active qmaster host
   std::optional<std::string> acct_file;           ///< `<cell_root>/common/`#ACCT_FILE
   std::optional<std::string> reporting_file;      ///< `<cell_root>/common/`#REPORTING_FILE
   std::optional<std::string> shadow_masters_file; ///< `<cell_root>/common/`#SHADOW_MASTERS_FILE
   std::optional<std::string> alias_file;          ///< `<cell_root>/common/host_aliases`, the host alias file
} sge_bootstrap_files_tl1_t;

static pthread_once_t bootstrap_files_once = PTHREAD_ONCE_INIT;

static pthread_key_t sge_bootstrap_files_tl1_key;

static void
bootstrap_files_thread_local_once_init();

static void
bootstrap_files_tl1_init(sge_bootstrap_files_tl1_t *tl);

static void
bootstrap_files_tl1_destroy(void *tl);

/**
 * @brief The value of an optional path as a C string
 *
 * @param path the path
 * @return the path, or nullptr if it is not set
 */
static const char *
opt_c_str(const std::optional<std::string> &path) {
   return path ? path->c_str() : nullptr;
}

/**
 * @brief Log the file paths of the thread local data at debug level
 *
 * @param tl the thread local data
 */
static void
bootstrap_files_log_tl1_parameter(sge_bootstrap_files_tl1_t *tl) {
   DENTER(TOP_LAYER);

   DPRINTF("FILES AND PATHS ===\n");
   DPRINTF("   cell_root                 >%s<\n", opt_c_str(tl->cell_root));
   DPRINTF("   bootstrap_file            >%s<\n", opt_c_str(tl->bootstrap_file));
   DPRINTF("   act_qmaster_file          >%s<\n", opt_c_str(tl->act_qmaster_file));
   DPRINTF("   acct_file                 >%s<\n", opt_c_str(tl->acct_file));
   DPRINTF("   reporting_file            >%s<\n", opt_c_str(tl->reporting_file));
   DPRINTF("   shadow_masters_file       >%s<\n", opt_c_str(tl->shadow_masters_file));
   DPRINTF("   alias_file                >%s<\n", opt_c_str(tl->alias_file));

   DRETURN_VOID;
}

static void
bootstrap_files_mt_init() {
   pthread_once(&bootstrap_files_once, bootstrap_files_thread_local_once_init);
}

/**
 * @brief Creates the thread-local key before `main()` runs
 *
 * Exists only for the side effect of its constructor. A single static instance
 * is defined below; do not remove it, and do not instantiate it anywhere else.
 */
class BootstrapFilesThreadInit {
public:
   /// Runs the one-time `pthread_key_create()` for this module
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

/**
 * @brief Set the file paths of the cell from `$SGE_ROOT` and `$SGE_CELL`
 *
 * Logs a `CRITICAL` message and leaves the remaining paths unset if `$SGE_ROOT`, the cell directory or its
 * `common` directory is missing.
 *
 * @param tl the thread local data to fill
 */
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

   tl->cell_root = sge_dstring_get_string(&bw);
   const char *cell_root = tl->cell_root->c_str();

   /* common dir */
   sge_dstring_sprintf(&bw, "%s" PATH_SEPARATOR "%s", cell_root, COMMON_DIR);
   if (SGE_STAT(buffer, &sbuf)) {
      CRITICAL(MSG_UTI_DIRECTORYNOTEXIST_S, buffer);
      DRETURN_VOID;
   }

   sge_dstring_sprintf(&bw, "%s" PATH_SEPARATOR "%s" PATH_SEPARATOR "%s", cell_root, COMMON_DIR, BOOTSTRAP_FILE);
   tl->bootstrap_file = sge_dstring_get_string(&bw);

   sge_dstring_sprintf(&bw, "%s" PATH_SEPARATOR "%s" PATH_SEPARATOR "%s", cell_root, COMMON_DIR, ACT_QMASTER_FILE);
   tl->act_qmaster_file = sge_dstring_get_string(&bw);

   sge_dstring_sprintf(&bw, "%s" PATH_SEPARATOR "%s" PATH_SEPARATOR "%s", cell_root, COMMON_DIR, ACCT_FILE);
   tl->acct_file = sge_dstring_get_string(&bw);

   sge_dstring_sprintf(&bw, "%s" PATH_SEPARATOR "%s" PATH_SEPARATOR "%s", cell_root, COMMON_DIR, REPORTING_FILE);
   tl->reporting_file = sge_dstring_get_string(&bw);

   sge_dstring_sprintf(&bw, "%s" PATH_SEPARATOR "%s" PATH_SEPARATOR "%s", cell_root, COMMON_DIR, SHADOW_MASTERS_FILE);
   tl->shadow_masters_file = sge_dstring_get_string(&bw);

   sge_dstring_sprintf(&bw, "%s" PATH_SEPARATOR "%s" PATH_SEPARATOR "%s", cell_root, COMMON_DIR, ALIAS_FILE);
   tl->alias_file = sge_dstring_get_string(&bw);

   DRETURN_VOID;
}

/**
 * @brief Initialise the thread local file paths of the calling thread
 *
 * The structure comes zeroed from ocs::uti::create() in GET_SPECIFIC, so every path is nullptr until
 * bootstrap_init_paths() sets it. The first call in the process logs the paths.
 *
 * @param tl the new thread local data
 */
static void
bootstrap_files_tl1_init(sge_bootstrap_files_tl1_t *tl) {
   DENTER(TOP_LAYER);
   static bool already_shown = false;

   bootstrap_init_paths(tl);

   if (!already_shown) {
      bootstrap_files_log_tl1_parameter(tl);
      already_shown = true;
   }
   DRETURN_VOID;
}

/**
 * @brief Release the thread local file paths of an exiting thread
 *
 * Registered with `pthread_key_create()`. The structure was made with ocs::uti::create() by GET_SPECIFIC
 * and owns its paths.
 *
 * @param tl the thread's sge_bootstrap_files_tl1_t
 */
static void
bootstrap_files_tl1_destroy(void *tl) {
   auto *_tl = static_cast<sge_bootstrap_files_tl1_t *>(tl);

   ocs::uti::destroy(_tl);
}

/**
 * @brief The cell directory, `$SGE_ROOT/$SGE_CELL`
 *
 * @return the path, or nullptr if the paths could not be initialised; owned by
 *         thread-local storage, do not free
 */
const char *
bootstrap_get_cell_root() {
   GET_SPECIFIC(sge_bootstrap_files_tl1_t, tl, bootstrap_files_tl1_init, sge_bootstrap_files_tl1_key);
   return opt_c_str(tl->cell_root);
}

/**
 * @brief The cell's `bootstrap` file, the source of @ref ocs::Bootstrap
 *
 * @return the path, or nullptr if the paths could not be initialised; owned by
 *         thread-local storage, do not free
 */
const char *
bootstrap_get_bootstrap_file() {
   GET_SPECIFIC(sge_bootstrap_files_tl1_t, tl, bootstrap_files_tl1_init, sge_bootstrap_files_tl1_key);
   return opt_c_str(tl->bootstrap_file);
}

/**
 * @brief The file naming the host the qmaster currently runs on
 *
 * @return the path, or nullptr if the paths could not be initialised; owned by
 *         thread-local storage, do not free
 */
const char *
bootstrap_get_act_qmaster_file() {
   GET_SPECIFIC(sge_bootstrap_files_tl1_t, tl, bootstrap_files_tl1_init, sge_bootstrap_files_tl1_key);
   return opt_c_str(tl->act_qmaster_file);
}

/**
 * @brief The accounting file qmaster appends finished jobs to
 *
 * @return the path, or nullptr if the paths could not be initialised; owned by
 *         thread-local storage, do not free
 */
const char *
bootstrap_get_acct_file() {
   GET_SPECIFIC(sge_bootstrap_files_tl1_t, tl, bootstrap_files_tl1_init, sge_bootstrap_files_tl1_key);
   return opt_c_str(tl->acct_file);
}

/**
 * @brief The reporting file qmaster writes its reporting records to
 *
 * @return the path, or nullptr if the paths could not be initialised; owned by
 *         thread-local storage, do not free
 */
const char *
bootstrap_get_reporting_file() {
   GET_SPECIFIC(sge_bootstrap_files_tl1_t, tl, bootstrap_files_tl1_init, sge_bootstrap_files_tl1_key);
   return opt_c_str(tl->reporting_file);
}

/**
 * @brief The file listing the hosts allowed to take over as qmaster
 *
 * @return the path, or nullptr if the paths could not be initialised; owned by
 *         thread-local storage, do not free
 */
const char *
bootstrap_get_shadow_masters_file() {
   GET_SPECIFIC(sge_bootstrap_files_tl1_t, tl, bootstrap_files_tl1_init, sge_bootstrap_files_tl1_key);
   return opt_c_str(tl->shadow_masters_file);
}

/**
 * @brief The host alias file
 *
 * The same path #sge_get_alias_path builds, but taken from thread-local
 * storage instead of allocated on every call — prefer this one.
 *
 * @return the path, or nullptr if the paths could not be initialised; owned by
 *         thread-local storage, do not free
 */
const char *
bootstrap_get_alias_file() {
   GET_SPECIFIC(sge_bootstrap_files_tl1_t, tl, bootstrap_files_tl1_init, sge_bootstrap_files_tl1_key);
   return opt_c_str(tl->alias_file);
}
