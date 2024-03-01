/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 * 
 *  The Contents of thiz file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of thiz file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use thiz file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 * 
 *  Software provided under thiz License is provided on an "AS IS" basis,
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

#include <netdb.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <pwd.h>
#include <cerrno>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "sge.h"
#include "uti/sge_bootstrap.h"
#include "uti/sge_bootstrap_env.h"
#include "uti/sge_csp_path.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_unistd.h"

#include "sgeobj/sge_answer.h"

#include "gdi/msg_gdilib.h"


#define SGE_COMMD_SERVICE "sge_qmaster"
#define CA_DIR          "common/sgeCA"
#define CA_LOCAL_ROOTUSER_DIR    "/var/sgeCA"
#define CA_LOCAL_NORMALUSER_DIR  "/tmp/sgeCA"
#define CaKey           "cakey.pem"
#define CaCert          "cacert.pem"
#define SGESecPath      ".sge"
#define UserKey         "key.pem"
#define RandFile        "rand.seed"
#define UserCert        "cert.pem"
#define CrlFile         "ca-crl.pem"
#define ReconnectFile   "private/reconnect.dat"
#define VALID_MINUTES    7          /* expiry of connection        */

typedef struct {
   char *ca_root;                /* path of ca_root directory */
   char *ca_local_root;          /* path of ca_local_root directory */
   char *CA_cert_file;           /* CA certificate file */
   char *CA_key_file;            /* CA's private key file */
   char *cert_file;              /* user certificate file */
   char *key_file;               /* user's key file */
   char *rand_file;              /* rand file */
   char *reconnect_file;         /* reconnect data file (not used) */
   char *crl_file;               /* CRL file */
   unsigned long refresh_time;   /* connection refresh time (not used) */
   char *password;               /* password for encrypted keyfiles (not used) */
   cl_ssl_verify_func_t verify_func; /* cert verify function */
} sge_csp_path_t;

static bool sge_csp_path_setup(sge_csp_path_class_t *thiz, sge_error_class_t *eh);

static void sge_csp_path_destroy(void *theState);

static void sge_csp_path_dprintf(sge_csp_path_class_t *thiz);

static const char *get_ca_root(sge_csp_path_class_t *thiz);

static const char *get_ca_local_root(sge_csp_path_class_t *thiz);

static const char *get_CA_cert_file(sge_csp_path_class_t *thiz);

static const char *get_CA_key_file(sge_csp_path_class_t *thiz);

static const char *get_cert_file(sge_csp_path_class_t *thiz);

static const char *get_key_file(sge_csp_path_class_t *thiz);

static const char *get_rand_file(sge_csp_path_class_t *thiz);

static const char *get_reconnect_file(sge_csp_path_class_t *thiz);

static const char *get_crl_file(sge_csp_path_class_t *thiz);

static const char *get_password(sge_csp_path_class_t *thiz);

static int get_refresh_time(sge_csp_path_class_t *thiz);

static cl_ssl_verify_func_t get_verify_func(sge_csp_path_class_t *thiz);

static void set_ca_root(sge_csp_path_class_t *thiz, const char *ca_root);

static void set_ca_local_root(sge_csp_path_class_t *thiz, const char *CA_cert_file);

static void set_CA_cert_file(sge_csp_path_class_t *thiz, const char *CA_cert_file);

static void set_CA_key_file(sge_csp_path_class_t *thiz, const char *CA_key_file);

static void set_cert_file(sge_csp_path_class_t *thiz, const char *cert_file);

static void set_key_file(sge_csp_path_class_t *thiz, const char *key_file);

static void set_rand_file(sge_csp_path_class_t *thiz, const char *rand_file);

static void set_reconnect_file(sge_csp_path_class_t *thiz, const char *reconnect_file);

static void set_crl_file(sge_csp_path_class_t *thiz, const char *crl_file);

static void set_refresh_time(sge_csp_path_class_t *thiz, u_long32 refresh_time);

static void set_password(sge_csp_path_class_t *thiz, const char *password);

static void set_verify_func(sge_csp_path_class_t *thiz, cl_ssl_verify_func_t func);

static bool ssl_cert_verify_func(cl_ssl_verify_mode_t mode, bool service_mode, const char *value);

static bool is_daemon(void) {
   const char *progname = component_get_component_name();
   if (progname != nullptr) {
      if (!strcmp(prognames[QMASTER], progname) || !strcmp(prognames[EXECD], progname) ||
          !strcmp(prognames[SCHEDD], progname)) {
         return true;
      }
   }
   return false;
}

static bool ssl_cert_verify_func(cl_ssl_verify_mode_t mode, bool service_mode, const char *value) {

   /*
    *   CR:
    *
    * - This callback function can be used to make additonal security checks 
    * 
    * - this callback is not called from commlib with a value == nullptr
    * 
    * - NOTE: This callback is called from the commlib. If the commlib is initalized with
    *   thread support (see cl_com_setup_commlib() ) this may be a problem because the thread has
    *   no application specific context initalization. So never call functions within this callback 
    *   which need thread specific setup.
    */
   DENTER(TOP_LAYER);

   DPRINTF(("ssl_cert_verify_func()\n"));

   if (value == nullptr) {
      /* This should never happen */
      CRITICAL(SFNMAX, MSG_SEC_CERT_VERIFY_FUNC_NO_VAL);
      DRETURN(false);
   }

   if (service_mode == true) {
      switch (mode) {
         case CL_SSL_PEER_NAME: {
            DPRINTF(("local service got certificate from peer \"%s\"\n", value));
#if 0
            if (strcmp(value,"SGE admin user") != 0) {
               DRETURN(false);
            }
#endif
            break;
         }
         case CL_SSL_USER_NAME: {
            DPRINTF(("local service got certificate from user \"%s\"\n", value));
#if 0
            if (strcmp(value,"") != 0) {
               DRETURN(false);
            }
#endif
            break;
         }
      }
   } else {
      switch (mode) {
         case CL_SSL_PEER_NAME: {
            DPRINTF(("local client got certificate from peer \"%s\"\n", value));
#if 0
            if (strcmp(value,"SGE admin user") != 0) {
               DRETURN(false);
            }
#endif
            break;
         }
         case CL_SSL_USER_NAME: {
            DPRINTF(("local client got certificate from user \"%s\"\n", value));
#if 0
            if (strcmp(value,"") != 0) {
               DRETURN(false);
            }
#endif
            break;
         }
      }
   }
   DRETURN(true);
}


sge_csp_path_class_t *
sge_csp_path_class_create(sge_error_class_t *eh)
{
   sge_csp_path_class_t *ret = nullptr;

   DENTER(TOP_LAYER);

   ret = (sge_csp_path_class_t *) sge_malloc(sizeof(sge_csp_path_class_t));
   if (!ret) {
      eh->error(eh, STATUS_EMALLOC, ANSWER_QUALITY_ERROR, MSG_MEMORY_MALLOCFAILED);
      DRETURN(nullptr);
   }

   ret->dprintf = sge_csp_path_dprintf;

   ret->get_ca_root = get_ca_root;
   ret->get_ca_local_root = get_ca_local_root;
   ret->get_CA_cert_file = get_CA_cert_file;
   ret->get_CA_key_file = get_CA_key_file;
   ret->get_cert_file = get_cert_file;
   ret->get_key_file = get_key_file;
   ret->get_rand_file = get_rand_file;
   ret->get_reconnect_file = get_reconnect_file;
   ret->get_crl_file = get_crl_file;
   ret->get_refresh_time = get_refresh_time;
   ret->get_password = get_password;
   ret->get_verify_func = get_verify_func;

   ret->set_CA_cert_file = set_CA_cert_file;
   ret->set_CA_key_file = set_CA_key_file;
   ret->set_cert_file = set_cert_file;
   ret->set_key_file = set_key_file;
   ret->set_rand_file = set_rand_file;
   ret->set_reconnect_file = set_reconnect_file;
   ret->set_crl_file = set_crl_file;
   ret->set_refresh_time = set_refresh_time;
   ret->set_password = set_password;
   ret->set_verify_func = set_verify_func;

   ret->sge_csp_path_handle = (sge_csp_path_t *) sge_malloc(sizeof(sge_csp_path_t));
   if (ret->sge_csp_path_handle == nullptr) {
      eh->error(eh, STATUS_EMALLOC, ANSWER_QUALITY_ERROR, MSG_MEMORY_MALLOCFAILED);
      sge_csp_path_class_destroy(&ret);
      DRETURN(nullptr);
   }
   memset(ret->sge_csp_path_handle, 0, sizeof(sge_csp_path_t));

   if (!sge_csp_path_setup(ret, eh)) {
      sge_csp_path_class_destroy(&ret);
      DRETURN(nullptr);
   }

   DRETURN(ret);
}

void sge_csp_path_class_destroy(sge_csp_path_class_t **pst) {
   DENTER(TOP_LAYER);

   if (!pst || !*pst) {
      DRETURN_VOID;
   }
   sge_csp_path_destroy((*pst)->sge_csp_path_handle);
   sge_free(pst);
   DRETURN_VOID;
}

static bool 
sge_csp_path_setup(sge_csp_path_class_t *thiz, sge_error_class_t *eh)
{
   char buffer[2*1024];
   dstring bw;
   const char *sge_root = nullptr;
   const char *sge_cell = nullptr;
   const char *user_dir = nullptr;
   const char *user_local_dir = nullptr;
   const char *username = nullptr;
   const char *sge_cakeyfile = nullptr;
   const char *sge_certfile = nullptr;
   const char *sge_keyfile = nullptr;
   int sge_qmaster_port = 0;
   bool is_from_services = false;
   SGE_STRUCT_STAT sbuf;
/*    bool sge_no_ca_local_root = false;  */
   char ca_local_dir[SGE_PATH_MAX];

   DENTER(TOP_LAYER);

   /* get the necessary info to build the paths */
   sge_root = bootstrap_get_sge_root();
   sge_cell = bootstrap_get_sge_cell();
   sge_qmaster_port = bootstrap_get_sge_qmaster_port();
   is_from_services = bootstrap_is_from_services();
   username = component_get_username();

   DTRACE;

#if 0
   TODO: currently not supported by sge_ca script
   if (getenv("SGE_NO_CA_LOCAL_ROOT")) {
      sge_no_ca_local_root = true;
   }
#endif

#ifdef NORMALUSER_FIX
   if (sge_is_start_user_superuser()) {
      strncpy(ca_local_dir, CA_LOCAL_ROOTUSER_DIR, SGE_PATH_MAX);
   } else {
      strncpy(ca_local_dir, CA_LOCAL_NORMALUSER_DIR, SGE_PATH_MAX);
   }
#else
   strncpy(ca_local_dir, CA_LOCAL_ROOTUSER_DIR, SGE_PATH_MAX);
#endif

   /*
   ** TODO: certificate handling does not work since usually the servlet runs as
   **       a specific user and has no access to the users key
   **       a different mechanism must be delivered to get access to the users key
   **       setuid wrapper for the file access or different storage for the key ???
   */
   sge_dstring_init(&bw, buffer, sizeof(buffer));

   sge_dstring_sprintf(&bw, "%s/%s/%s", sge_root, sge_cell, CA_DIR);
   set_ca_root(thiz, sge_dstring_get_string(&bw));

   if (!is_from_services) {
      sge_dstring_sprintf(&bw, "%s/port%d/%s", ca_local_dir, sge_qmaster_port, sge_cell);
   } else {
      sge_dstring_sprintf(&bw, "%s/%s/%s", ca_local_dir, SGE_COMMD_SERVICE, sge_cell);
   }
   set_ca_local_root(thiz, sge_dstring_get_string(&bw));

   /*
   ** determine user_dir: 
   ** - ca_root, ca_local_root for daemons 
   ** - $HOME/.sge/{port$COMMD_PORT|SGE_COMMD_SERVICE}/$SGE_CELL
   **   and as fallback
   **   /var/sgeCA/{port$COMMD_PORT|SGE_COMMD_SERVICE}/$SGE_CELL/userkeys/$USER/{cert.pem,key.pem}
   */
   if (is_daemon()) {
      user_dir = strdup(get_ca_root(thiz));
      user_local_dir = strdup(get_ca_local_root(thiz));
   } else {
      struct passwd *pw;
      struct passwd pw_struct;
      char *buffer;
      int size;

      size = get_pw_buffer_size();
      buffer = sge_malloc(size);
      pw = sge_getpwnam_r(username, &pw_struct, buffer, size);

      if (!pw) {
         eh->error(eh, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_SEC_USERNOTFOUND_S, username);
         sge_free(&buffer);
         DRETURN(false);
      }
      if (!is_from_services) {
         sge_dstring_sprintf(&bw, "%s/%s/port%d/%s", pw->pw_dir, SGESecPath, sge_qmaster_port, sge_cell);
      } else {
         sge_dstring_sprintf(&bw, "%s/%s/%s/%s", pw->pw_dir, SGESecPath, SGE_COMMD_SERVICE, sge_cell);
      }
      user_dir = strdup(sge_dstring_get_string(&bw));
      user_local_dir = strdup(user_dir);
      sge_free(&buffer);
   }

   sge_dstring_sprintf(&bw, "%s/%s", get_ca_root(thiz), CaCert);
   thiz->set_CA_cert_file(thiz, sge_dstring_get_string(&bw));

   if ((sge_cakeyfile = getenv("SGE_CAKEYFILE"))) {
      thiz->set_CA_key_file(thiz, sge_cakeyfile);
   } else {
      sge_dstring_sprintf(&bw, "%s/private/%s", get_ca_local_root(thiz), CaKey);
      thiz->set_CA_key_file(thiz, sge_dstring_get_string(&bw));
   }

   if ((sge_certfile = getenv("SGE_CERTFILE"))) {
      thiz->set_cert_file(thiz, sge_certfile);
   } else {   
      if (is_daemon()) {
         sge_dstring_sprintf(&bw, "%s/certs/%s", user_dir, UserCert);
      } else {
         sge_dstring_sprintf(&bw, "%s/userkeys/%s/%s", get_ca_local_root(thiz), username, UserCert);
      }
      thiz->set_cert_file(thiz, sge_dstring_get_string(&bw));
   }

   if ((sge_keyfile = getenv("SGE_KEYFILE"))) {
      thiz->set_key_file(thiz, sge_keyfile); 
   } else {   
      if (is_daemon()) {
         sge_dstring_sprintf(&bw, "%s/private/%s", user_local_dir, UserKey);   
      } else {
         sge_dstring_sprintf(&bw, "%s/userkeys/%s/%s", get_ca_local_root(thiz), username, UserKey);
      }
      thiz->set_key_file(thiz, sge_dstring_get_string(&bw));
   }

   sge_dstring_sprintf(&bw, "%s/%s", user_dir, RandFile);
   thiz->set_rand_file(thiz, sge_dstring_get_string(&bw));
   if (SGE_STAT(thiz->get_rand_file(thiz), &sbuf)) { 
      if (is_daemon()) {
         sge_dstring_sprintf(&bw, "%s/private/%s", user_local_dir, RandFile);   
      } else {
         sge_dstring_sprintf(&bw, "%s/userkeys/%s/%s", get_ca_local_root(thiz), username, RandFile);
      }
      thiz->set_rand_file(thiz, sge_dstring_get_string(&bw));
   }

   sge_dstring_sprintf(&bw, "%s/%s", user_dir, ReconnectFile);
   thiz->set_reconnect_file(thiz, sge_dstring_get_string(&bw));

   sge_dstring_sprintf(&bw, "%s/%s", get_ca_root(thiz), CrlFile);
   thiz->set_crl_file(thiz, sge_dstring_get_string(&bw));

   thiz->set_password(thiz, nullptr);
   thiz->set_refresh_time(thiz, 60 * VALID_MINUTES);

   thiz->set_verify_func(thiz, ssl_cert_verify_func);

   sge_free(&user_dir);
   sge_free(&user_local_dir);

   DRETURN(true);
}

static void sge_csp_path_destroy(void *theState) {
   sge_csp_path_t *s = (sge_csp_path_t *) theState;

   DENTER(TOP_LAYER);

   sge_free(&(s->ca_root));
   sge_free(&(s->ca_local_root));
   sge_free(&(s->CA_cert_file));
   sge_free(&(s->CA_key_file));
   sge_free(&(s->cert_file));
   sge_free(&(s->key_file));
   sge_free(&(s->rand_file));
   sge_free(&(s->reconnect_file));
   sge_free(&(s->crl_file));
   sge_free(&(s->password));
   sge_free(&s);

   DRETURN_VOID;
}

static void sge_csp_path_dprintf(sge_csp_path_class_t *thiz) {
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;

   DENTER(TOP_LAYER);

   DPRINTF(("ca_root             >%s<\n", es->ca_root ? es->ca_root : "NA"));
   DPRINTF(("ca_local_root       >%s<\n", es->ca_local_root ? es->ca_local_root : "NA"));
   DPRINTF(("CA_cert_file        >%s<\n", es->CA_cert_file ? es->CA_cert_file : "NA"));
   DPRINTF(("CA_key_file         >%s<\n", es->CA_key_file ? es->CA_key_file : "NA"));
   DPRINTF(("cert_file           >%s<\n", es->cert_file ? es->cert_file : "NA"));
   DPRINTF(("key_file            >%s<\n", es->key_file ? es->key_file : "NA"));
   DPRINTF(("rand_file           >%s<\n", es->rand_file ? es->rand_file : "NA"));
   DPRINTF(("reconnect_file      >%s<\n", es->reconnect_file ? es->reconnect_file : "NA"));
   DPRINTF(("CRL file            >%s<\n", es->crl_file ? es->crl_file : "NA"));
   DPRINTF(("refresh_time        >%d<\n", es->refresh_time));
   DPRINTF(("password            >%s<\n", es->password ? es->password : "NA"));

   DRETURN_VOID;
}

static const char *get_ca_root(sge_csp_path_class_t *thiz) {
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   return es->ca_root;
}

static const char *get_ca_local_root(sge_csp_path_class_t *thiz) {
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   return es->ca_local_root;
}

static const char *get_CA_cert_file(sge_csp_path_class_t *thiz) {
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   return es->CA_cert_file;
}

static const char *get_CA_key_file(sge_csp_path_class_t *thiz) {
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   return es->CA_key_file;
}

static const char *get_cert_file(sge_csp_path_class_t *thiz) {
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   return es->cert_file;
}

static const char *get_key_file(sge_csp_path_class_t *thiz) {
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   return es->key_file;
}

static const char *get_rand_file(sge_csp_path_class_t *thiz) {
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   return es->rand_file;
}

static const char *get_reconnect_file(sge_csp_path_class_t *thiz) {
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   return es->reconnect_file;
}

static const char *get_crl_file(sge_csp_path_class_t *thiz) {
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   return es->crl_file;
}

static const char *get_password(sge_csp_path_class_t *thiz) {
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   return es->password;
}

static int get_refresh_time(sge_csp_path_class_t *thiz) {
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   return (int) es->refresh_time;
}

static cl_ssl_verify_func_t get_verify_func(sge_csp_path_class_t *thiz) {
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   return es->verify_func;
}

static void set_ca_root(sge_csp_path_class_t *thiz, const char *ca_root) {
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   es->ca_root = sge_strdup(es->ca_root, ca_root);
}

static void set_ca_local_root(sge_csp_path_class_t *thiz, const char *ca_local_root) {
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   es->ca_local_root = sge_strdup(es->ca_local_root, ca_local_root);
}

static void set_CA_cert_file(sge_csp_path_class_t *thiz, const char *CA_cert_file) {
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   es->CA_cert_file = sge_strdup(es->CA_cert_file, CA_cert_file);
}

static void set_CA_key_file(sge_csp_path_class_t *thiz, const char *CA_key_file) {
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   es->CA_key_file = sge_strdup(es->CA_key_file, CA_key_file);
}

static void set_cert_file(sge_csp_path_class_t *thiz, const char *cert_file) {
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   es->cert_file = sge_strdup(es->cert_file, cert_file);
}

static void set_key_file(sge_csp_path_class_t *thiz, const char *key_file) {
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   es->key_file = sge_strdup(es->key_file, key_file);
}

static void set_rand_file(sge_csp_path_class_t *thiz, const char *rand_file) {
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   es->rand_file = sge_strdup(es->rand_file, rand_file);
}

static void set_reconnect_file(sge_csp_path_class_t *thiz, const char *reconnect_file) {
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   es->reconnect_file = sge_strdup(es->reconnect_file, reconnect_file);
}

static void set_crl_file(sge_csp_path_class_t *thiz, const char *crl_file) {
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   es->crl_file = sge_strdup(es->crl_file, crl_file);
}

static void set_password(sge_csp_path_class_t *thiz, const char *password) {
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   es->password = sge_strdup(es->password, password);
}

static void set_refresh_time(sge_csp_path_class_t *thiz, u_long32 refresh_time) {
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   es->refresh_time = refresh_time;
}

static void set_verify_func(sge_csp_path_class_t *thiz, cl_ssl_verify_func_t verify_func) {
   sge_csp_path_t *es = (sge_csp_path_t *) thiz->sge_csp_path_handle;
   es->verify_func = verify_func;
}

