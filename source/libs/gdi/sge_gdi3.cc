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
#include "uti/sge_string.h"
#include "uti/sge_rmon.h"
#include "uti/sge_error_class.h"
#include "uti/sge_csp_path.h"

#include "sge_gdi3.h"

// thread local data
typedef struct {
   // incremented with each GDI request to have a unique request ID for the answer
   u_long32 request_id;
   bool is_setup;
   sge_error_class_t *error_handle;
   int last_commlib_error;
} sge_gdi_tl_t;

// data shared between threads
typedef struct {
   // still unused but should be used when attributes are accessed
   pthread_mutex_t mutex;

   char *master_host;
   u_long32 timestamp_qmaster_file;
   char *ssl_private_key;
   char *ssl_certificate;
   sge_csp_path_class_t *csp_path_obj;
} sge_gdi_ts_t;

// once initializer
static pthread_once_t gdi3_once = PTHREAD_ONCE_INIT;

// key to get thread local storage
static pthread_key_t gdi3_tl_key;

// shared storage
static sge_gdi_ts_t ts;

static void
set_master_host(const char *master_host) {
   ts.master_host = sge_strdup(ts.master_host, master_host);
}

static void
gdi3_log_tl_parameter(sge_gdi_tl_t *tl) {
   DENTER(TOP_LAYER);
   DPRINTF(("request_id             >%d<\n", tl->request_id));
   DPRINTF(("is_setup                >%s<\n", tl->is_setup ? "true" : "false"));
   DPRINTF(("error_handle            >%p<\n", tl->error_handle));
   DPRINTF(("last_commlib_error      >%d<\n", tl->last_commlib_error));
   DRETURN_VOID;
}

static void
gdi3_log_ts_parameter() {
   DENTER(TOP_LAYER);
   DPRINTF(("master_host             >%s<\n", ts.master_host ? ts.master_host : "NA"));
   DPRINTF(("timestamp_qmaster_file  >%d<\n", ts.timestamp_qmaster_file));
   DRETURN_VOID;
}

static void
gdi3_tl_init(sge_gdi_tl_t *tl) {
   DENTER(TOP_LAYER);
   memset(tl, 0, sizeof(sge_gdi_tl_t));
   tl->is_setup = false;
   tl->error_handle = sge_error_class_create();
   gdi3_log_tl_parameter(tl);
   DRETURN_VOID;
}

static void
gdi3_tl_destroy(void *tl) {
   auto _tl = (sge_gdi_tl_t *) tl;

   sge_error_class_destroy(&_tl->error_handle);
   sge_free(&_tl);
}

static void
gdi3_ts_init() {
   DENTER(TOP_LAYER);
   memset(&ts, 0, sizeof(sge_gdi_ts_t));
   pthread_mutex_init(&ts.mutex, nullptr);
   gdi3_log_ts_parameter();
   DRETURN_VOID;
}

static void
gdi3_ts_destroy() {
   DENTER(TOP_LAYER);
   pthread_mutex_destroy(&ts.mutex);
   DRETURN_VOID;
}

static void
gdi3_once_init() {
   // init key that will provide access to local storage.
   pthread_key_create(&gdi3_tl_key, gdi3_tl_destroy);

   // init shared storage
   gdi3_ts_init();
}

static void
gdi3_mt_init() {
   pthread_once(&gdi3_once, gdi3_once_init);
}

class GdiThreadInit {
public:
   GdiThreadInit() {
      gdi3_mt_init();
   }
};

// although not used the constructor call has the side effect to initialize the pthread_key => do not delete
static GdiThreadInit gdi_obj{};


void
gdi3_mt_done() {
   gdi3_ts_destroy();
}

const char *
gdi3_get_master_host() {
   return ts.master_host;
}

void
gdi3_set_master_host(const char *master_host) {
   set_master_host(master_host);
}

bool
gdi3_is_setup() {
   GET_SPECIFIC(sge_gdi_tl_t, tl, gdi3_tl_init, gdi3_tl_key);
   DENTER(TOP_LAYER);
   gdi3_log_ts_parameter();
   DRETURN(tl->is_setup);
}

void
gdi3_set_setup(bool is_setup) {
   GET_SPECIFIC(sge_gdi_tl_t, tl, gdi3_tl_init, gdi3_tl_key);
   DENTER(TOP_LAYER);
   tl->is_setup = is_setup;
   DRETURN_VOID;
}

u_long32
gdi3_get_timestamp_qmaster_file() {
   return ts.timestamp_qmaster_file;
}

void
gdi3_set_timestamp_qmaster_file(u_long32 timestamp_qmaster_file) {
   ts.timestamp_qmaster_file = timestamp_qmaster_file;
}

sge_error_class_t *
gdi3_get_error_handle() {
   GET_SPECIFIC(sge_gdi_tl_t, tl, gdi3_tl_init, gdi3_tl_key);
   return tl->error_handle;
}

void
gdi3_set_error_handle(sge_error_class_t *error_handle) {
   GET_SPECIFIC(sge_gdi_tl_t, tl, gdi3_tl_init, gdi3_tl_key);
   tl->error_handle = error_handle;
}

int
gdi3_get_last_commlib_error() {
   GET_SPECIFIC(sge_gdi_tl_t, tl, gdi3_tl_init, gdi3_tl_key);
   return tl->last_commlib_error;
}

void
gdi3_set_last_commlib_error(int last_commlib_error) {
   GET_SPECIFIC(sge_gdi_tl_t, tl, gdi3_tl_init, gdi3_tl_key);
   tl->last_commlib_error = last_commlib_error;
}

const char *
gdi3_get_ssl_private_key() {
   return ts.ssl_private_key;
}

void
gdi3_set_ssl_private_key(const char *ssl_private_key) {
   ts.ssl_private_key = sge_strdup(ts.ssl_private_key, ssl_private_key);
}

const char *
gdi3_get_ssl_certificate() {
   return ts.ssl_certificate;
}

void
gdi3_set_ssl_certificate(const char *ssl_certificate) {
   ts.ssl_certificate = sge_strdup(ts.ssl_certificate, ssl_certificate);
}

sge_csp_path_class_t *
gdi3_get_csp_path_obj() {
   return ts.csp_path_obj;
}

void
gdi3_set_csp_path_obj(sge_csp_path_class_t *csp_path_obj) {
   ts.csp_path_obj = csp_path_obj;
}

u_long32
gdi3_get_next_request_id() {
   GET_SPECIFIC(sge_gdi_tl_t, tl, gdi3_tl_init, gdi3_tl_key);
   tl->request_id++;
   return tl->request_id;
}

