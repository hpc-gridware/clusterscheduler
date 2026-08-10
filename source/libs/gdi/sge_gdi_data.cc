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
 * @brief Per-thread data of the GDI layer
 */

#include <cinttypes>
#include "uti/sge_csp_path.h"
#include "uti/sge_error_class.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_stdlib.h"

#include "sge_gdi_data.h"

// thread local data
/// GDI state that is private to one thread
typedef struct {
   uint32_t request_id;              ///< incremented per request, so an answer can be matched to it
   bool is_setup;                    ///< has this thread completed the GDI setup?
   sge_error_class_t *error_handle;  ///< collector the GDI reports its errors into
   int last_commlib_error;           ///< the commlib error the last call ran into
} sge_gdi_tl_t;

// data shared between threads
/**
 * @brief GDI state shared by every thread of the process
 *
 * @warning #mutex exists to guard these fields but is **never taken** — it is
 *          only initialised and destroyed. Every field here is therefore read
 *          and written without synchronisation.
 */
typedef struct {
   pthread_mutex_t mutex;            ///< intended to guard this struct; currently never locked

   char *master_host;                ///< host qmaster was last known to run on
   uint64_t timestamp_qmaster_file;  ///< mtime of `act_qmaster` when it was last read
   // True when the qmaster client certificate could not be loaded during
   // prepare_enroll() (typically because act_qmaster pointed to a host whose
   // certificate file does not exist on disk). gdi_get_act_master_host() will
   // retry the client TLS configuration on each re-read of act_qmaster as long
   // as this flag is set.
   bool tls_client_cert_pending;     ///< true while the client TLS certificate still has to be loaded
   char *ssl_private_key;            ///< path of the client's private key
   char *ssl_certificate;            ///< path of the client's certificate
#ifdef SECURE
   sge_csp_path_class_t *csp_path_obj; ///< the CSP file locations, when that security mode is used
#endif
} sge_gdi_ts_t;

// once initializer
static pthread_once_t gdi_data_once = PTHREAD_ONCE_INIT;

// key to get thread local storage
static pthread_key_t gdi_data_tl_key;

// shared storage
static sge_gdi_ts_t ts;

/**
 * @brief Record the host qmaster is running on
 *
 * @param master_host the host name, which is copied
 */
void
gdi_set_master_host(const char *master_host) {
   ts.master_host = sge_strdup(ts.master_host, master_host);
}

static void
gdi_data_log_tl_parameter(sge_gdi_tl_t *tl) {
   DENTER(TOP_LAYER);
   DPRINTF("GDI LOCAL ===\n");
   DPRINTF("   request_id                >%d<\n", tl->request_id);
   DPRINTF("   is_setup                  >%s<\n", tl->is_setup ? "true" : "false");
   DPRINTF("   error_handle              >%p<\n", tl->error_handle);
   DPRINTF("   last_commlib_error        >%d<\n", tl->last_commlib_error);
   DRETURN_VOID;
}

static void
gdi_data_log_ts_parameter() {
   DENTER(TOP_LAYER);
   DPRINTF("GDI SHARED ===\n");
   DPRINTF("   master_host               >%s<\n", ts.master_host ? ts.master_host : "NA");
   DPRINTF("   timestamp_qmaster_file    >" sge_u64 "<\n", ts.timestamp_qmaster_file);
   DPRINTF("   tls_client_cert_pending   >%s<\n", ts.tls_client_cert_pending ? "true" : "false");
   DRETURN_VOID;
}

static void
gdi_data_tl_init(sge_gdi_tl_t *tl) {
   DENTER(TOP_LAYER);
   memset(tl, 0, sizeof(sge_gdi_tl_t));
   tl->is_setup = false;
   tl->error_handle = sge_error_class_create();
   gdi_data_log_tl_parameter(tl);
   DRETURN_VOID;
}

static void
gdi_data_tl_destroy(void *tl) {
   auto _tl = (sge_gdi_tl_t *) tl;

   sge_error_class_destroy(&_tl->error_handle);
   sge_free(&_tl);
}

static void
gdi_data_ts_init() {
   DENTER(TOP_LAYER);
   memset(&ts, 0, sizeof(sge_gdi_ts_t));
   pthread_mutex_init(&ts.mutex, nullptr);
   gdi_data_log_ts_parameter();
   DRETURN_VOID;
}

static void
gdi_data_ts_destroy() {
   DENTER(TOP_LAYER);

#ifdef SECURE
   sge_csp_path_class_destroy(&ts.csp_path_obj);
#endif

   pthread_mutex_destroy(&ts.mutex);

   /* @todo CS-591 don't we have to free all the other attributes as well?
    * char *master_host;
    * uint64_t timestamp_qmaster_file;
    * char *ssl_private_key;
    * char *ssl_certificate;
    * sge_csp_path_class_t *csp_path_obj;
    */
   DRETURN_VOID;
}

static void
gdi_data_once_init() {
   // init key that will provide access to local storage.
   pthread_key_create(&gdi_data_tl_key, gdi_data_tl_destroy);

   // init shared storage
   gdi_data_ts_init();
}

static void
gdi_data_mt_init() {
   pthread_once(&gdi_data_once, gdi_data_once_init);
}

/**
 * @brief Creates the thread-local key before `main()` runs
 *
 * Exists only for the side effect of its constructor. A single static instance
 * is defined below; do not remove it, and do not instantiate it anywhere else.
 */
class GdiThreadInit {
public:
   /// Runs the one-time initialisation for this module
   GdiThreadInit() {
      gdi_data_mt_init();
   }
};

// although not used the constructor call has the side effect to initialize the pthread_key => do not delete
static GdiThreadInit gdi_obj{};

/**
 * @brief Release the shared GDI state at shutdown
 */
void
gdi_data_mt_done() {
   gdi_data_ts_destroy();
}

/**
 * @brief The host qmaster was last known to run on
 *
 * @return the host name, or nullptr before it was determined; shared, do not free
 */
const char *
gdi_data_get_master_host() {
   return ts.master_host;
}

/**
 * @brief Has the calling thread completed the GDI setup?
 *
 * @return true when this thread may issue GDI requests
 */
bool
gdi_data_is_setup() {
   DENTER(TOP_LAYER);

   GET_SPECIFIC(sge_gdi_tl_t, tl, gdi_data_tl_init, gdi_data_tl_key);
   gdi_data_log_ts_parameter();
   DRETURN(tl->is_setup);
}

/**
 * @brief Record whether the calling thread has completed the GDI setup
 *
 * @param is_setup true once the setup succeeded
 */
void
gdi_data_set_setup(bool is_setup) {
   DENTER(TOP_LAYER);

   GET_SPECIFIC(sge_gdi_tl_t, tl, gdi_data_tl_init, gdi_data_tl_key);
   tl->is_setup = is_setup;
   DRETURN_VOID;
}

/**
 * @brief When `act_qmaster` was last read
 *
 * @return the file's mtime at the last read, or 0
 */
uint64_t
gdi_data_get_timestamp_qmaster_file() {
   return ts.timestamp_qmaster_file;
}

/**
 * @brief Record when `act_qmaster` was read
 *
 * @param timestamp_qmaster_file the file's mtime
 */
void
gdi_data_set_timestamp_qmaster_file(uint64_t timestamp_qmaster_file) {
   ts.timestamp_qmaster_file = timestamp_qmaster_file;
}

/**
 * @brief Does the qmaster client TLS certificate still have to be loaded?
 *
 * Set by `gdi_setup_tls_config()` when the certificate file derived from
 * `act_qmaster` does not exist on disk, and cleared by
 * `gdi_update_client_tls_config()` once it is in place. While it is set,
 * `gdi_get_act_master_host()` retries the client TLS configuration on every
 * re-read of `act_qmaster`.
 *
 * @return true while the certificate is still missing
 */
bool
gdi_data_get_tls_client_cert_pending() {
   return ts.tls_client_cert_pending;
}

/**
 * @brief Record whether the qmaster client TLS certificate still has to be loaded
 *
 * @param tls_client_cert_pending true while the certificate is missing
 *
 * @see #gdi_data_get_tls_client_cert_pending
 */
void
gdi_data_set_tls_client_cert_pending(bool tls_client_cert_pending) {
   ts.tls_client_cert_pending = tls_client_cert_pending;
}

/**
 * @brief The error collector of the calling thread
 *
 * @return the collector; owned by thread-local storage
 */
sge_error_class_t *
gdi_data_get_error_handle() {
   GET_SPECIFIC(sge_gdi_tl_t, tl, gdi_data_tl_init, gdi_data_tl_key);
   return tl->error_handle;
}

/**
 * @brief Replace the error collector of the calling thread
 *
 * @param error_handle the collector to use
 */
void
gdi_data_set_error_handle(sge_error_class_t *error_handle) {
   GET_SPECIFIC(sge_gdi_tl_t, tl, gdi_data_tl_init, gdi_data_tl_key);
   tl->error_handle = error_handle;
}

/**
 * @brief The commlib error the last GDI call ran into
 *
 * @return the commlib error code, or 0
 */
int
gdi_data_get_last_commlib_error() {
   GET_SPECIFIC(sge_gdi_tl_t, tl, gdi_data_tl_init, gdi_data_tl_key);
   return tl->last_commlib_error;
}

/**
 * @brief Record the commlib error of the last GDI call
 *
 * @param last_commlib_error the code to record
 */
void
gdi_data_set_last_commlib_error(int last_commlib_error) {
   GET_SPECIFIC(sge_gdi_tl_t, tl, gdi_data_tl_init, gdi_data_tl_key);
   tl->last_commlib_error = last_commlib_error;
}

#ifdef SECURE
/**
 * @brief Path of the client's TLS private key
 *
 * @return the path; shared, do not free
 */
const char *
gdi_data_get_ssl_private_key() {
   return ts.ssl_private_key;
}

/**
 * @brief Set the path of the client's TLS private key
 *
 * @param ssl_private_key the path, which is copied
 */
void
gdi_data_set_ssl_private_key(const char *ssl_private_key) {
   ts.ssl_private_key = sge_strdup(ts.ssl_private_key, ssl_private_key);
}

/**
 * @brief Path of the client's TLS certificate
 *
 * @return the path; shared, do not free
 */
const char *
gdi_data_get_ssl_certificate() {
   return ts.ssl_certificate;
}

/**
 * @brief Set the path of the client's TLS certificate
 *
 * @param ssl_certificate the path, which is copied
 */
void
gdi_data_set_ssl_certificate(const char *ssl_certificate) {
   ts.ssl_certificate = sge_strdup(ts.ssl_certificate, ssl_certificate);
}

/**
 * @brief The CSP file locations in use
 *
 * @return the path set, or nullptr when CSP is not configured
 */
sge_csp_path_class_t *
gdi_data_get_csp_path_obj() {
   return ts.csp_path_obj;
}

/**
 * @brief Set the CSP file locations
 *
 * @param csp_path_obj the path set; taken over
 */
void
gdi_data_set_csp_path_obj(sge_csp_path_class_t *csp_path_obj) {
   ts.csp_path_obj = csp_path_obj;
}
#endif
