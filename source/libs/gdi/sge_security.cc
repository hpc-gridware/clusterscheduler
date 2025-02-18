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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <cstring>
#include <pwd.h>
#include <pthread.h>

#include "comm/cl_commlib.h"

#include "cull/cull.h"

#include "uti/sge_afsutil.h"
#include "uti/sge_arch.h"
#include "uti/sge_bootstrap.h"
#include "uti/sge_hostname.h"
#include "uti/sge_io.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_uidgid.h"

#include "sgeobj/sge_feature.h"
#include "sgeobj/sge_var.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_answer.h"

#include "gdi/sge_security.h"
#include "gdi/msg_gdilib.h"

#include "execution_states.h"

#include "msg_common.h"

#ifdef CRYPTO
#include <openssl/evp.h>
#endif

#define ENCODE_TO_STRING   1
#define DECODE_FROM_STRING 0

#ifdef SECURE

const char* sge_dummy_sec_string = "AIMK_SECURE_OPTION_ENABLED";

static pthread_mutex_t sec_ssl_setup_config_mutex = PTHREAD_MUTEX_INITIALIZER;
static cl_ssl_setup_t* sec_ssl_setup_config       = nullptr;
#define SEC_LOCK_SSL_SETUP()      sge_mutex_lock("ssl_setup_mutex", __func__, __LINE__, &sec_ssl_setup_config_mutex)
#define SEC_UNLOCK_SSL_SETUP()    sge_mutex_unlock("ssl_setup_mutex", __func__, __LINE__, &sec_ssl_setup_config_mutex)

static bool ssl_cert_verify_func(cl_ssl_verify_mode_t mode, bool service_mode, const char* value);
static bool is_daemon(const char* progname);
static bool is_master(const char* progname);

#endif



static bool is_daemon(const char* progname) {
   if (progname != nullptr) {
      if ( !strcmp(prognames[QMASTER], progname) ||
           !strcmp(prognames[EXECD]  , progname) ||
           !strcmp(prognames[SCHEDD] , progname)) {
         return true;
      }
   }
   return false;
}

#ifdef SECURE

static bool is_master(const char* progname) {
   if (progname != nullptr) {
      if ( !strcmp(prognames[QMASTER],progname)) {
         return true;
      }
   }
   return false;
}

/* int 0 on success, -1 on failure */
int sge_ssl_setup_security_path(const char *progname, const char *user) {

   int return_value = 0;
   int commlib_error = 0;
   SGE_STRUCT_STAT sbuf;
   dstring userdir = DSTRING_INIT;
   dstring user_local_dir = DSTRING_INIT;
	dstring ca_root = DSTRING_INIT;
	dstring ca_local_root = DSTRING_INIT;
   char *sge_cakeyfile = nullptr;
   char *sge_keyfile = nullptr;
   char *sge_certfile = nullptr;

#define SGE_COMMD_SERVICE "sge_qmaster"
#define CA_DIR          "common/sgeCA"
#define CA_LOCAL_DIR    "/var/sgeCA"
#define CaKey           "cakey.pem"
#define CaCert          "cacert.pem"
#define SGESecPath      ".sge"
#define UserKey         "key.pem"
#define RandFile        "rand.seed"
#define UserCert        "cert.pem"
#define CrlFile         "ca-crl.pem"
#define ReconnectFile   "private/reconnect.dat"
#define VALID_MINUTES    7          /* expiry of connection        */

   /* former global values */
   dstring ca_key_file    = DSTRING_INIT;   
   dstring ca_cert_file   = DSTRING_INIT;
   dstring key_file       = DSTRING_INIT;
   dstring rand_file      = DSTRING_INIT;
   dstring cert_file      = DSTRING_INIT; 
   dstring reconnect_file = DSTRING_INIT;
   dstring crl_file       = DSTRING_INIT;
   bool from_services   = false;
   int  qmaster_port    = -1;
   char *user_name = sge_strdup(nullptr, user);

   DENTER(TOP_LAYER);

   if (progname == nullptr) {
      CRITICAL(SFNMAX, MSG_GDI_NO_VALID_PROGRAMM_NAME);
      sge_free(&user_name);
      DRETURN(-1);
   }

   SEC_LOCK_SSL_SETUP();

   qmaster_port = sge_get_qmaster_port(&from_services);
   
   /*
   ** malloc ca_root string and check if directory has been created during
   ** install otherwise exit
   */
   sge_dstring_sprintf(&ca_root, "%s/%s/%s", sge_get_root_dir(1, nullptr, 0, 1), sge_get_default_cell(), CA_DIR);
   if (SGE_STAT(sge_dstring_get_string(&ca_root), &sbuf)) { 
      CRITICAL(MSG_SEC_CAROOTNOTFOUND_S, sge_dstring_get_string(&ca_root));
      sge_free(&user_name);
      sge_dstring_free(&userdir);
      sge_dstring_free(&user_local_dir);
      sge_dstring_free(&ca_root);
      sge_dstring_free(&ca_local_root);
      sge_dstring_free(&ca_key_file);   
      sge_dstring_free(&ca_cert_file);
      sge_dstring_free(&key_file);
      sge_dstring_free(&rand_file);
      sge_dstring_free(&cert_file); 
      sge_dstring_free(&reconnect_file);
      sge_dstring_free(&crl_file);
      DRETURN(-1);
   }

   /*
   ** check that ca local root string directory has been created during
   ** install otherwise exit
   */
   if ((sge_cakeyfile=getenv("SGE_CAKEYFILE"))) {
      sge_dstring_copy_string(&ca_key_file, sge_cakeyfile);
   } else {
      if (getenv("SGE_NO_CA_LOCAL_ROOT")) {
         sge_dstring_copy_dstring(&ca_local_root, &ca_root);
      } else {
         char *ca_local_dir = nullptr;
         /* If the user is root, use /var/sgeCA.  Otherwise, use /tmp/sgeCA */
#if 0
         if (geteuid () == SGE_SUPERUSER_ID) {
            ca_local_dir = CA_LOCAL_DIR;
         }
         else {
            ca_local_dir = USER_CA_LOCAL_DIR;
         }
#endif
         ca_local_dir = CA_LOCAL_DIR; 
         if (!from_services) {
            sge_dstring_sprintf(&ca_local_root, "%s/port%d/%s", ca_local_dir, qmaster_port, sge_get_default_cell());
         } else {
            sge_dstring_sprintf(&ca_local_root, "%s/%s/%s", ca_local_dir, SGE_COMMD_SERVICE, sge_get_default_cell());
         }
      }   
      if (is_daemon(progname) && SGE_STAT(sge_dstring_get_string(&ca_local_root), &sbuf)) { 
         CRITICAL(MSG_SEC_CALOCALROOTNOTFOUND_S, sge_dstring_get_string(&ca_local_root));
         sge_free(&user_name);
         sge_dstring_free(&userdir);
         sge_dstring_free(&user_local_dir);
         sge_dstring_free(&ca_root);
         sge_dstring_free(&ca_local_root);
         sge_dstring_free(&ca_key_file);   
         sge_dstring_free(&ca_cert_file);
         sge_dstring_free(&key_file);
         sge_dstring_free(&rand_file);
         sge_dstring_free(&cert_file); 
         sge_dstring_free(&reconnect_file);
         sge_dstring_free(&crl_file);
         DRETURN(-1);
      }
      sge_dstring_sprintf(&ca_key_file, "%s/private/%s", sge_dstring_get_string(&ca_local_root), CaKey);
   }

   if (is_master(progname) && SGE_STAT(sge_dstring_get_string(&ca_key_file), &sbuf)) { 
      CRITICAL(MSG_SEC_CAKEYFILENOTFOUND_S, sge_dstring_get_string(&ca_key_file));
      sge_free(&user_name);
      sge_dstring_free(&user_local_dir);
      sge_dstring_free(&ca_root);
      sge_dstring_free(&ca_local_root);
      sge_dstring_free(&ca_key_file);   
      sge_dstring_free(&ca_cert_file);
      sge_dstring_free(&key_file);
      sge_dstring_free(&rand_file);
      sge_dstring_free(&cert_file); 
      sge_dstring_free(&reconnect_file);
      sge_dstring_free(&crl_file);
      DRETURN(-1);
   }
   DPRINTF("ca_key_file: %s\n", sge_dstring_get_string(&ca_key_file));

	sge_dstring_sprintf(&ca_cert_file, "%s/%s", sge_dstring_get_string(&ca_root), CaCert);

   if (SGE_STAT(sge_dstring_get_string(&ca_cert_file), &sbuf)) { 
      CRITICAL(MSG_SEC_CACERTFILENOTFOUND_S, sge_dstring_get_string(&ca_cert_file));
      sge_free(&user_name);
      sge_dstring_free(&user_local_dir);
      sge_dstring_free(&ca_root);
      sge_dstring_free(&ca_local_root);
      sge_dstring_free(&ca_key_file);   
      sge_dstring_free(&ca_cert_file);
      sge_dstring_free(&key_file);
      sge_dstring_free(&rand_file);
      sge_dstring_free(&cert_file); 
      sge_dstring_free(&reconnect_file);
      sge_dstring_free(&crl_file);
      DRETURN(-1);
   }
   DPRINTF("ca_cert_file: %s\n", sge_dstring_get_string(&ca_cert_file));

	sge_dstring_sprintf(&crl_file, "%s/%s", sge_dstring_get_string(&ca_root), CrlFile);
   DPRINTF("crl_file: %s\n", sge_dstring_get_string(&crl_file));

   /*
   ** determine user directory: 
   ** - ca_root, ca_local_root for daemons 
   ** - $HOME/.sge/{port$COMMD_PORT|SGE_COMMD_SERVICE}/$SGE_CELL
   **   and as fallback
   **   /var/sgeCA/{port$COMMD_PORT|SGE_COMMD_SERVICE}/$SGE_CELL/userkeys/$USER/{cert.pem,key.pem}
   */

   if (is_daemon(progname)){
      sge_dstring_copy_dstring(&userdir, &ca_root);
      sge_dstring_copy_dstring(&user_local_dir, &ca_local_root);
   } else {
      struct passwd *pw;
      struct passwd pw_struct;
      char *buffer;
      int size;

      size = get_pw_buffer_size();
      buffer = sge_malloc(size);
      SGE_ASSERT(buffer != nullptr);
      pw = sge_getpwnam_r(user_name, &pw_struct, buffer, size);
      if (!pw) {
         CRITICAL(MSG_SEC_USERNOTFOUND_S, user_name);
         sge_free(&user_name);
         sge_free(&buffer);
         sge_dstring_free(&user_local_dir);
         sge_dstring_free(&ca_root);
         sge_dstring_free(&ca_local_root);
         sge_dstring_free(&ca_key_file);   
         sge_dstring_free(&ca_cert_file);
         sge_dstring_free(&key_file);
         sge_dstring_free(&rand_file);
         sge_dstring_free(&cert_file); 
         sge_dstring_free(&reconnect_file);
         sge_dstring_free(&crl_file);
         DRETURN(-1);
      }
      if (!from_services) {
         sge_dstring_sprintf(&userdir, "%s/%s/port%d/%s", pw->pw_dir, SGESecPath, qmaster_port, sge_get_default_cell());
      } else {
         sge_dstring_sprintf(&userdir, "%s/%s/%s/%s", pw->pw_dir, SGESecPath, SGE_COMMD_SERVICE, sge_get_default_cell());
      }
      sge_dstring_copy_dstring(&user_local_dir, &userdir);
      sge_free(&buffer);
   }

   if ((sge_keyfile = getenv("SGE_KEYFILE"))) {
      sge_dstring_copy_string(&key_file, sge_keyfile);
   } else {   
      sge_dstring_sprintf(&key_file, "%s/private/%s", sge_dstring_get_string(&user_local_dir), UserKey);
   }   

   if (SGE_STAT(sge_dstring_get_string(&key_file), &sbuf)) { 
      sge_dstring_sprintf(&key_file, "%s/userkeys/%s/%s", sge_dstring_get_string(&ca_local_root), user_name, UserKey);
   }   
   sge_dstring_sprintf(&rand_file, "%s/private/%s", sge_dstring_get_string(&user_local_dir), RandFile);

   if (SGE_STAT(sge_dstring_get_string(&rand_file), &sbuf)) { 
      sge_dstring_sprintf(&rand_file, "%s/userkeys/%s/%s", sge_dstring_get_string(&ca_local_root), user_name, RandFile);
   }   

   if (SGE_STAT(sge_dstring_get_string(&key_file), &sbuf)) { 
      CRITICAL(MSG_SEC_KEYFILENOTFOUND_S, sge_dstring_get_string(&key_file));
      sge_free(&user_name);
      sge_dstring_free(&user_local_dir);
      sge_dstring_free(&ca_root);
      sge_dstring_free(&ca_local_root);
      sge_dstring_free(&ca_key_file);   
      sge_dstring_free(&ca_cert_file);
      sge_dstring_free(&key_file);
      sge_dstring_free(&rand_file);
      sge_dstring_free(&cert_file); 
      sge_dstring_free(&reconnect_file);
      sge_dstring_free(&crl_file);
      DRETURN(-1);
   }
   DPRINTF("key_file: %s\n", sge_dstring_get_string(&key_file));

   if (SGE_STAT(sge_dstring_get_string(&rand_file), &sbuf)) { 
      WARNING(MSG_SEC_RANDFILENOTFOUND_S, sge_dstring_get_string(&rand_file));
   } else {
      DPRINTF("rand_file: %s\n", sge_dstring_get_string(&rand_file));
   }   

   if ((sge_certfile = getenv("SGE_CERTFILE"))) {
      sge_dstring_copy_string(&cert_file, sge_certfile);
   } else {   
      sge_dstring_sprintf(&cert_file, "%s/certs/%s", sge_dstring_get_string(&userdir), UserCert);
   }

   if (SGE_STAT(sge_dstring_get_string(&cert_file), &sbuf)) {
      sge_dstring_sprintf(&cert_file, "%s/userkeys/%s/%s", sge_dstring_get_string(&ca_local_root), user_name, UserCert);
   }   

   if (SGE_STAT(sge_dstring_get_string(&cert_file), &sbuf)) { 
      CRITICAL(MSG_SEC_CERTFILENOTFOUND_S, sge_dstring_get_string(&cert_file));
      sge_free(&user_name);
      sge_dstring_free(&user_local_dir);
      sge_dstring_free(&ca_root);
      sge_dstring_free(&ca_local_root);
      sge_dstring_free(&ca_key_file);   
      sge_dstring_free(&ca_cert_file);
      sge_dstring_free(&key_file);
      sge_dstring_free(&rand_file);
      sge_dstring_free(&cert_file); 
      sge_dstring_free(&reconnect_file);
      sge_dstring_free(&crl_file);
      DRETURN(-1);
   }
   DPRINTF("cert_file: %s\n", sge_dstring_get_string(&cert_file));

   sge_dstring_sprintf(&reconnect_file, "%s/%s", sge_dstring_get_string(&userdir), ReconnectFile);
   DPRINTF("reconnect_file: %s\n", sge_dstring_get_string(&reconnect_file));
    
   sge_dstring_free(&userdir);
   sge_dstring_free(&user_local_dir);

   sge_dstring_free(&ca_root);
   sge_dstring_free(&ca_local_root);

   if (sec_ssl_setup_config != nullptr) {
      DPRINTF("deleting old ssl configuration setup ...\n");
      cl_com_free_ssl_setup(&sec_ssl_setup_config);
   }

   DPRINTF("creating ssl configuration setup ...\n");
   commlib_error = cl_com_create_ssl_setup(&sec_ssl_setup_config,
                                           CL_SSL_PEM_FILE,                         /* ssl_cert_mode        */
                                           CL_SSL_v23,                              /* ssl_method           */
                                           sge_dstring_get_string(&ca_cert_file),   /* ssl_CA_cert_pem_file */
                                           sge_dstring_get_string(&ca_key_file),    /* ssl_CA_key_pem_file  */
                                           sge_dstring_get_string(&cert_file),      /* ssl_cert_pem_file    */
                                           sge_dstring_get_string(&key_file),       /* ssl_key_pem_file     */
                                           sge_dstring_get_string(&rand_file),      /* ssl_rand_file        */
                                           sge_dstring_get_string(&reconnect_file), /* ssl_reconnect_file   */
                                           sge_dstring_get_string(&crl_file),       /* ssl_crl_file         */
                                           60 * VALID_MINUTES,                      /* ssl_refresh_time     */
                                           nullptr,                                    /* ssl_password         */
                                           ssl_cert_verify_func);                   /* ssl_verify_func (cl_ssl_verify_func_t)  */
   if ( commlib_error != CL_RETVAL_OK) {
      return_value = -1;
      DPRINTF("return value of cl_com_create_ssl_setup(): %s\n", cl_get_error_text(commlib_error));
   }


   commlib_error = cl_com_specify_ssl_configuration(sec_ssl_setup_config);
   if ( commlib_error != CL_RETVAL_OK) {
      return_value = -1;
      DPRINTF("return value of cl_com_specify_ssl_configuration(): %s\n", cl_get_error_text(commlib_error));
   }

   SEC_UNLOCK_SSL_SETUP();

   sge_dstring_free(&ca_key_file);   
   sge_dstring_free(&ca_cert_file);
   sge_dstring_free(&key_file);
   sge_dstring_free(&rand_file);
   sge_dstring_free(&cert_file); 
   sge_dstring_free(&reconnect_file);
   sge_dstring_free(&crl_file);
   sge_free(&user_name);

   DRETURN(return_value);
}

static bool ssl_cert_verify_func(cl_ssl_verify_mode_t mode, bool service_mode, const char* value) {

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

   DPRINTF("ssl_cert_verify_func()\n");

   if (value == nullptr) {
      /* This should never happen */
      CRITICAL(SFNMAX, MSG_SEC_CERT_VERIFY_FUNC_NO_VAL);
      DRETURN(false);
   }

   if (service_mode) {
      switch(mode) {
         case CL_SSL_PEER_NAME: {
            DPRINTF("local service got certificate from peer \"%s\"\n", value);
#if 0
            if (strcmp(value,"SGE admin user") != 0) {
               DRETURN(false);
            }
#endif
            break;
         }
         case CL_SSL_USER_NAME: {
            DPRINTF("local service got certificate from user \"%s\"\n", value);
#if 0
            if (strcmp(value,"") != 0) {
               DRETURN(false);
            }
#endif
            break;
         }
      }
   } else {
      switch(mode) {
         case CL_SSL_PEER_NAME: {
            DPRINTF("local client got certificate from peer \"%s\"\n", value);
#if 0
            if (strcmp(value,"SGE admin user") != 0) {
               DRETURN(false);
            }
#endif
            break;
         }
         case CL_SSL_USER_NAME: {
            DPRINTF("local client got certificate from user \"%s\"\n", value);
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

#endif


/****** gdi/security/sge_security_initialize() ********************************
*  NAME
*     sge_security_initialize -- initialize sge security
*
*  SYNOPSIS
*     int sge_security_initialize(char *name);
*
*  FUNCTION
*     Initialize sge security by initializing the underlying security
*     mechanism and setup the corresponding data structures
*
*  INPUTS
*     name - name of enrolling program
*
*  RETURN
*     0  in case of success, something different otherwise 
*
*  NOTES
*     MT-NOTE: sge_security_initialize() is MT safe (assumptions)
******************************************************************************/

int sge_security_initialize(const char *progname, const char *username)
{
   DENTER(TOP_LAYER);

#ifdef SECURE
   {

     /*
      * The dummy_string is only neccessary to be able to check with
      * strings command in installation scripts whether the SECURE
      * compile option was used at compile time.
      * 
      */
      static const char* dummy_string = nullptr;

      dummy_string = sge_dummy_sec_string;
      if (dummy_string != nullptr) {
         DPRINTF("secure dummy string: %s\n", dummy_string);
      } else {
         DPRINTF("secure dummy string not available\n");
      }

      if (feature_is_enabled(FEATURE_CSP_SECURITY)) {
         if (sge_ssl_setup_security_path(progname, username)) {
            DRETURN(-1);
         }
      }
   }
#endif

#ifdef KERBEROS
   if (krb_init(name)) {
      DRETURN(-1);
   }
#endif   

   DRETURN(0);
}

/****** gdi/security/sge_security_exit() **************************************
*  NAME
*     sge_security_exit -- exit sge security
*
*  SYNOPSIS
*     void sge_security_exit(int status);
*
*  FUNCTION
*     Execute any routines that the security mechanism needs to do when
*     the program
*
*  INPUTS
*     status - exit status value
*
*  NOTES
*     MT-NOTE: sge_security_exit() is MT safe
******************************************************************************/
void sge_security_exit(int i)
{
   DENTER(TOP_LAYER);

#ifdef SECURE
   if (feature_is_enabled(FEATURE_CSP_SECURITY)) {
      SEC_LOCK_SSL_SETUP();
      cl_com_free_ssl_setup(&sec_ssl_setup_config);
      SEC_UNLOCK_SSL_SETUP();
   } 
#endif

   DRETURN_VOID;
}


/****** gdi/security/set_sec_cred() *******************************************
*  NAME
*     set_sec_cred -- get credit for security system
*
*  SYNOPSIS
*     int set_sec_cred(lListElem *job);
*
*  FUNCTION
*     Tries to get credit for a security system (DCE or KERBEROS),
*     sets the accordant information in the job structure
*     If an error occurs the return value is unequal 0
*
*  INPUTS
*     job - the job structure
*
*  RETURN
*     0  in case of success, something different otherwise 
*
*  EXAMPLE
*
*  NOTES
*     Hope, the above description is correct - don't know the 
*     DCE/KERBEROS code.
* 
*  NOTES
*     MT-NOTE: set_sec_cred() is MT safe (major assumptions!)
******************************************************************************/
int set_sec_cred(const char *sge_root, const char *mastername, lListElem *job, lList **alpp)
{

   pid_t command_pid;
   FILE *fp_in, *fp_out, *fp_err;
   char *str;
   int ret = 0;
   char binary[1024];
   char cmd[2048];
   char line[1024];

   DENTER(TOP_LAYER);
   
   if (feature_is_enabled(FEATURE_AFS_SECURITY)) {
      snprintf(binary, sizeof(binary), "%s/util/get_token_cmd", sge_root);

      if (sge_get_token_cmd(binary, nullptr, 0) != 0) {
         answer_list_add(alpp, MSG_QSH_QSUBFAILED, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(-1);
      }   
      
      command_pid = sge_peopen("/bin/sh", 0, binary, nullptr, nullptr, &fp_in, &fp_out, &fp_err, false);

      if (command_pid == -1) {
         answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, 
                                 MSG_QSUB_CANTSTARTCOMMANDXTOGETTOKENQSUBFAILED_S, binary);
         DRETURN(-1);
      }

      str = sge_bin2string(fp_out, 0);
      
      ret = sge_peclose(command_pid, fp_in, fp_out, fp_err, nullptr);
      
      lSetString(job, JB_tgt, str);
   }
      
   /*
    * DCE / KERBEROS security stuff
    *
    *  This same basic code is in qsh.c and qmon_submit.c
    *  It should really be moved to a common place. It would
    *  be nice if there was a generic job submittal function.
    */

   if (feature_is_enabled(FEATURE_DCE_SECURITY) ||
       feature_is_enabled(FEATURE_KERBEROS_SECURITY)) {
      snprintf(binary, sizeof(binary), "%s/utilbin/%s/get_cred", sge_root, sge_get_arch());

      if (sge_get_token_cmd(binary, nullptr, 0) != 0) {
         answer_list_add(alpp, MSG_QSH_QSUBFAILED, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(-1);
      }   

      snprintf(cmd, sizeof(cmd), "%s %s%s%s", binary, "sge", "@", mastername);
      
      command_pid = sge_peopen("/bin/sh", 0, cmd, nullptr, nullptr, &fp_in, &fp_out, &fp_err, false);

      if (command_pid == -1) {
         answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, 
                                 MSG_QSUB_CANTSTARTCOMMANDXTOGETTOKENQSUBFAILED_S, binary);
         DRETURN(-1);
      }

      str = sge_bin2string(fp_out, 0);

      while (!feof(fp_err)) {
         if (fgets(line, sizeof(line), fp_err)) {
            answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, 
                                    "getcred stderr: %s", line);
         }
      }

      ret = sge_peclose(command_pid, fp_in, fp_out, fp_err, nullptr);

      if (ret) {
         answer_list_add(alpp, MSG_QSH_CANTGETCREDENTIALS, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR); 
      }
      
      lSetString(job, JB_cred, str);
   }
   DRETURN(ret);
} 

/****** sge_security/cache_sec_cred() ******************************************
*  NAME
*     cache_sec_cred() -- ??? 
*
*  SYNOPSIS
*     bool cache_sec_cred(lListElem *jep, const char *rhost) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     lListElem *jep    - ??? 
*     const char *rhost - ??? 
*
*  RESULT
*     bool - true, if jep got modified
*
*  EXAMPLE
*     ??? 
*
*  NOTES
*     MT-NOTE:  cache_sec_cred() is MT safe (assumptions)
*
*  BUGS
*     ??? 
*
*  SEE ALSO
*     ???/???
*******************************************************************************/
bool cache_sec_cred(const char* sge_root, lListElem *jep, const char *rhost)
{
   bool ret_value = true;

   DENTER(TOP_LAYER);

   /* 
    * Execute command to get DCE or Kerberos credentials.
    * 
    * This needs to be made asynchronous.
    *
    */

   if (feature_is_enabled(FEATURE_DCE_SECURITY) ||
       feature_is_enabled(FEATURE_KERBEROS_SECURITY)) {

      pid_t command_pid=-1;
      FILE *fp_in, *fp_out, *fp_err;
      char *str;
      char binary[1024], cmd[2048], ccname[64];
      int ret;
      char *env[2];

      /* set up credentials cache for this job */
      snprintf(ccname, sizeof(ccname), "KRB5CCNAME=FILE:/tmp/krb5cc_qmaster_" sge_u32,
              lGetUlong(jep, JB_job_number));
      env[0] = ccname;
      env[1] = nullptr;

      snprintf(binary, sizeof(binary), "%s/utilbin/%s/get_cred", sge_root, sge_get_arch());

      if (sge_get_token_cmd(binary, nullptr, 0) == 0) {
         char line[1024];

         snprintf(cmd, sizeof(cmd), "%s %s%s%s", binary, "sge", "@", rhost);

         command_pid = sge_peopen("/bin/sh", 0, cmd, nullptr, env, &fp_in, &fp_out, &fp_err, false);

         if (command_pid == -1) {
            ERROR(MSG_SEC_NOSTARTCMD4GETCRED_SU, binary, lGetUlong(jep, JB_job_number));
         }

         str = sge_bin2string(fp_out, 0);

         while (!feof(fp_err)) {
            if (fgets(line, sizeof(line), fp_err))
               ERROR(MSG_QSH_GET_CREDSTDERR_S, line);
         }

         ret = sge_peclose(command_pid, fp_in, fp_out, fp_err, nullptr);

         lSetString(jep, JB_cred, str);

         if (ret) {
            ERROR(MSG_SEC_NOCRED_USSI, lGetUlong(jep, JB_job_number), rhost, binary, ret);
         }
      } else {
         ERROR(MSG_SEC_NOCREDNOBIN_US,  lGetUlong(jep, JB_job_number), binary);
         ret_value = false;       
      }
   }
   else {
      ret_value = false;
   }
   DRETURN(ret_value);
}   

/*
 * 
 *  NOTES
 *     MT-NOTE: delete_credentials() is MT safe (major assumptions!)
 * 
 */
void delete_credentials(const char *sge_root, lListElem *jep)
{

   DENTER(TOP_LAYER);

   /* 
    * Execute command to delete the client's DCE or Kerberos credentials.
    */
   if ((feature_is_enabled(FEATURE_DCE_SECURITY) ||
        feature_is_enabled(FEATURE_KERBEROS_SECURITY)) &&
        lGetString(jep, JB_cred)) {

      pid_t command_pid=-1;
      FILE *fp_in, *fp_out, *fp_err;
      char binary[1024], cmd[2048], ccname[128], ccfile[32], ccenv[64];
      int ret=0;
      char *env[2];
      char tmpstr[1024];

      /* set up credentials cache for this job */
      snprintf(ccfile, sizeof(ccfile), "/tmp/krb5cc_qmaster_" sge_u32,
               lGetUlong(jep, JB_job_number));
      snprintf(ccenv, sizeof(ccenv), "FILE:%s", ccfile);
      snprintf(ccname, sizeof(ccname), "KRB5CCNAME=%s", ccenv);
      env[0] = ccname;
      env[1] = nullptr;

      snprintf(binary, sizeof(binary), "%s/utilbin/%s/delete_cred", sge_root, sge_get_arch());

      if (sge_get_token_cmd(binary, nullptr, 0) == 0) {
         char line[1024];

         snprintf(cmd, sizeof(cmd), "%s -s %s", binary, "sge");

         command_pid = sge_peopen("/bin/sh", 0, cmd, nullptr, env, &fp_in, &fp_out, &fp_err, false);

         if (command_pid == -1) {
            strcpy(tmpstr, SGE_EVENT);
            ERROR(MSG_SEC_STARTDELCREDCMD_SU, binary, lGetUlong(jep, JB_job_number));
            strcpy(SGE_EVENT, tmpstr);
         }

         while (!feof(fp_err)) {
            if (fgets(line, sizeof(line), fp_err)) {
               strcpy(tmpstr, SGE_EVENT);
               ERROR(MSG_SEC_DELCREDSTDERR_S, line);
               strcpy(SGE_EVENT, tmpstr);
            }
         }

         ret = sge_peclose(command_pid, fp_in, fp_out, fp_err, nullptr);

         if (ret != 0) {
            strcpy(tmpstr, SGE_EVENT);
            ERROR(MSG_SEC_DELCREDRETCODE_USI, lGetUlong(jep, JB_job_number), binary, ret);
            strcpy(SGE_EVENT, tmpstr);
         }

      } else {
         strcpy(tmpstr, SGE_EVENT);
         ERROR(MSG_SEC_DELCREDNOBIN_US,  lGetUlong(jep, JB_job_number), binary);
         strcpy(SGE_EVENT, tmpstr);
      }
   }

   DRETURN_VOID;
}



/* 
 * Execute command to store the client's DCE or Kerberos credentials.
 * This also creates a forwardable credential for the user.
 *
 *  NOTES
 *     MT-NOTE: store_sec_cred() is MT safe (assumptions)
 */
int store_sec_cred(const char* sge_root, lListElem *jep, int do_authentication, lList** alpp)
{

   DENTER(TOP_LAYER);

   if ((feature_is_enabled(FEATURE_DCE_SECURITY) ||
        feature_is_enabled(FEATURE_KERBEROS_SECURITY)) &&
       (do_authentication || lGetString(jep, JB_cred))) {

      pid_t command_pid;
      FILE *fp_in, *fp_out, *fp_err;
      char line[1024], binary[1024], cmd[2048], ccname[64];
      int ret;
      char *env[2];

      if (do_authentication && lGetString(jep, JB_cred) == nullptr) {
         ERROR(MSG_SEC_NOAUTH_U, lGetUlong(jep, JB_job_number));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(-1);
      }

      /* set up credentials cache for this job */
      snprintf(ccname, sizeof(ccname), "KRB5CCNAME=FILE:/tmp/krb5cc_qmaster_" sge_u32,
              lGetUlong(jep, JB_job_number));
      env[0] = ccname;
      env[1] = nullptr;

      snprintf(binary, sizeof(binary), "%s/utilbin/%s/put_cred", sge_root, sge_get_arch());

      if (sge_get_token_cmd(binary, nullptr, 0) == 0) {
         snprintf(cmd, sizeof(cmd), "%s -s %s -u %s", binary, "sge", lGetString(jep, JB_owner));

         command_pid = sge_peopen("/bin/sh", 0, cmd, nullptr, env, &fp_in, &fp_out, &fp_err, false);

         if (command_pid == -1) {
            ERROR(MSG_SEC_NOSTARTCMD4GETCRED_SU, binary, lGetUlong(jep, JB_job_number));
         }

         sge_string2bin(fp_in, lGetString(jep, JB_cred));

         while (!feof(fp_err)) {
            if (fgets(line, sizeof(line), fp_err))
               ERROR(MSG_SEC_PUTCREDSTDERR_S, line);
         }

         ret = sge_peclose(command_pid, fp_in, fp_out, fp_err, nullptr);

         if (ret) {
            ERROR(MSG_SEC_NOSTORECRED_USI, lGetUlong(jep, JB_job_number), binary, ret);
         }

         /*
          * handle authentication failure
          */

         if (do_authentication && (ret != 0)) {
            ERROR(MSG_SEC_NOAUTH_U, lGetUlong(jep, JB_job_number));
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(-1);
         }

      } else {
         ERROR(MSG_SEC_NOSTORECREDNOBIN_US, lGetUlong(jep, JB_job_number), binary);
      }
   }
#ifdef KERBEROS

   /* get client TGT and store in job entry */

   {
      krb5_error_code rc;
      krb5_creds ** tgt_creds = nullptr;
      krb5_data outbuf;

      outbuf.length = 0;

      if (krb_get_tgt(request->host, request->commproc, request->id,
		      request->request_id, &tgt_creds) == 0) {
      
	 if ((rc = krb_encrypt_tgt_creds(tgt_creds, &outbuf))) {
	    ERROR(MSG_SEC_KRBENCRYPTTGT_SSIS, request->host, request->commproc, request->id, error_message(rc));
	 }

	 if (rc == 0)
	    lSetString(jep, JB_tgt,
                       krb_bin2str(outbuf.data, outbuf.length, nullptr));

	 if (outbuf.length)
	    krb5_free_data(outbuf.data);

         /* get rid of the TGT credentials */
         krb_put_tgt(request->host, request->commproc, request->id,
		     request->request_id, nullptr);

      }
   }

#endif

   return 0;
}   




/*
 *
 *  NOTES
 *     MT-NOTE: store_sec_cred2() is MT safe (assumptions)
 */
int store_sec_cred2(const char* sge_root, const char* unqualified_hostname, lListElem *jelem, int do_authentication, int *general, dstring *err_str)
{
   int ret = 0;
   const char *cred;
   
   DENTER(TOP_LAYER);

   if ((feature_is_enabled(FEATURE_DCE_SECURITY) ||
        feature_is_enabled(FEATURE_KERBEROS_SECURITY)) &&
       (cred = lGetString(jelem, JB_cred)) && cred[0]) {

      pid_t command_pid;
      FILE *fp_in, *fp_out, *fp_err;
      char binary[1024], cmd[2048], ccname[128], ccfile[32], ccenv[64],
           jobstr[32];
      int ret;
      char *env[3];
      lListElem *vep;

      /* set up credentials cache for this job */
      snprintf(ccfile, sizeof(ccfile), "/tmp/krb5cc_%s_" sge_u32, "sge", lGetUlong(jelem, JB_job_number));
      snprintf(ccenv, sizeof(ccenv), "FILE:%s", ccfile);
      snprintf(ccname, sizeof(ccname), "KRB5CCNAME=%s", ccenv);
      snprintf(jobstr, sizeof(jobstr), "JOB_ID=" sge_u32, lGetUlong(jelem, JB_job_number));
      env[0] = ccname;
      env[1] = jobstr;
      env[2] = nullptr;
      vep = lAddSubStr(jelem, VA_variable, "KRB5CCNAME", JB_env_list, VA_Type);
      lSetString(vep, VA_value, ccenv);

      snprintf(binary, sizeof(binary), "%s/utilbin/%s/put_cred", sge_root, sge_get_arch());

      if (sge_get_token_cmd(binary, nullptr, 0) == 0) {
         char line[1024];

         snprintf(cmd, sizeof(cmd), "%s -s %s -u %s -b %s", binary, "sge",
                 lGetString(jelem, JB_owner), lGetString(jelem, JB_owner));

         command_pid = sge_peopen("/bin/sh", 0, cmd, nullptr, env, &fp_in, &fp_out, &fp_err, false);

         if (command_pid == -1) {
            ERROR(MSG_SEC_NOSTARTCMD4GETCRED_SU, binary, lGetUlong(jelem, JB_job_number));
         }

         sge_string2bin(fp_in, lGetString(jelem, JB_cred));

         while (!feof(fp_err)) {
            if (fgets(line, sizeof(line), fp_err))
               ERROR(MSG_SEC_PUTCREDSTDERR_S, line);
         }

         ret = sge_peclose(command_pid, fp_in, fp_out, fp_err, nullptr);

         if (ret) {
            ERROR(MSG_SEC_NOSTORECRED_USI, lGetUlong(jelem, JB_job_number), binary, ret);
         }

         /*
          * handle authentication failure
          */                                                  
                                                              
         if (do_authentication && (ret != 0)) {               
            ERROR(MSG_SEC_KRBAUTHFAILURE, lGetUlong(jelem, JB_job_number));
            sge_dstring_sprintf(err_str, MSG_SEC_KRBAUTHFAILUREONHOST,
                    lGetUlong(jelem, JB_job_number),
                    unqualified_hostname);                 
            *general = GFSTATE_JOB;                            
         }                                                    
      } else {
         ERROR(MSG_SEC_NOSTORECREDNOBIN_US, lGetUlong(jelem, JB_job_number), binary);
      }
   }
   DRETURN(ret);
}

#ifdef KERBEROS
/*
 *
 *  NOTES
 *     MT-NOTE: kerb_job() is not MT safe
 */
int kerb_job(
lListElem *jelem,
struct dispatch_entry *de 
) {
   /* get TGT and store in job entry and in user's credentials cache */
   krb5_error_code rc;
   krb5_creds ** tgt_creds = nullptr;
   krb5_data outbuf;

   DENTER(TOP_LAYER);

   outbuf.length = 0;

   if (krb_get_tgt(de->host, de->commproc, de->id, lGetUlong(jelem, JB_job_number), &tgt_creds) == 0) {
      struct passwd *pw;
      struct passwd pw_struct;
      char *pw_buffer;
      size_t pw_buffer_size;

      if ((rc = krb_encrypt_tgt_creds(tgt_creds, &outbuf))) {
         ERROR(MSG_SEC_KRBENCRYPTTGTUSER_SUS, lGetString(jelem, JB_owner), lGetUlong(jelem, JB_job_number), error_message(rc));
      }

      if (rc == 0)
         lSetString(jelem, JB_tgt, krb_bin2str(outbuf.data, outbuf.length, nullptr));

      if (outbuf.length)
         krb5_xfree(outbuf.data);

      pw_buffer_size = get_pw_buffer_size();
      pw_buffer = sge_malloc(pw_buffer_size);
      SGE_ASSERT(pw_buffer != nullptr);
      pw = sge_getpwnam_r(lGetString(jelem, JB_owner), &pw_struct, pw_buffer, pw_buffer_size);

      if (pw) {
         if (krb_store_forwarded_tgt(pw->pw_uid,
               lGetUlong(jelem, JB_job_number),
               tgt_creds) == 0) {
            char ccname[40];
            lListElem *vep;

            krb_get_ccname(lGetUlong(jelem, JB_job_number), ccname);
            vep = lAddSubStr(jelem, VA_variable, "KRB5CCNAME", JB_env_list, VA_Type);
            lSetString(vep, VA_value, ccname);
         }

      } else {
         ERROR(MSG_SEC_NOUID_SU, lGetString(jelem, JB_owner), lGetUlong(jelem, JB_job_number));
      }

      /* clear TGT out of client entry (this frees the TGT credentials) */
      krb_put_tgt(de->host, de->commproc, de->id, lGetUlong(jelem, JB_job_number), nullptr);

      sge_free(&pw_buffer);
   }

   DRETURN(0);
}
#endif


/* 
 *  FUNCTION
 *     get TGT from job entry and store in client connection 
 *
 *  NOTES
 *     MT-NOTE: tgt2cc() is not MT safe (assumptions)
 */
void tgt2cc(lListElem *jep, const char *rhost)
{

#ifdef KERBEROS
   krb5_error_code rc;
   krb5_creds ** tgt_creds = nullptr;
   krb5_data inbuf;
   char *tgtstr = nullptr;
   u_long32 jid = 0;
   
   DENTER(TOP_LAYER);
   inbuf.length = 0;
   jid = lGetUlong(jep, JB_job_number);
   
   if ((tgtstr = lGetString(jep, JB_tgt))) { 
      inbuf.data = krb_str2bin(tgtstr, nullptr, &inbuf.length);
      if (inbuf.length) {
         if ((rc = krb_decrypt_tgt_creds(&inbuf, &tgt_creds))) {
            ERROR(MSG_SEC_KRBDECRYPTTGT_US, jid, error_message(rc));
         }
      }
      if (rc == 0)
         if (krb_put_tgt(rhost, prognames[EXECD], 0, jid, tgt_creds) == 0) {
            krb_set_tgt_id(jid);
 
            tgt_creds = nullptr;
         }

      if (inbuf.length)
         krb5_xfree(inbuf.data);

      if (tgt_creds)
         krb5_free_creds(krb_context(), *tgt_creds);
   }

   DRETURN_VOID;
#endif

}


/*
 *
 *  NOTES
 *     MT-NOTE: tgtcclr() is MT safe (assumptions)
 */
void tgtcclr(lListElem *jep, const char *rhost)
{
#ifdef KERBEROS

   /* clear client TGT */
   krb_put_tgt(rhost, prognames[EXECD], 0, lGetUlong(jep, JB_job_number), nullptr);
   krb_set_tgt_id(0);

#endif
}



#ifndef CRYPTO
/*
** standard encrypt/decrypt functions
**
** MT-NOTE: sge_encrypt() is MT safe
*/
bool
sge_encrypt(const char *intext, char *outbuf, int outsize) {
   int len;

   DENTER(TOP_LAYER);

/*    DPRINTF("======== intext:\n" SFN "\n=========\n", intext); */

   len = strlen(intext);
   if (!change_encoding(outbuf, &outsize, (unsigned char*) intext, &len, ENCODE_TO_STRING)) {
      DRETURN(false);
   }   

/*    DPRINTF("======== outbuf:\n" SFN "\n=========\n", outbuf); */

   DRETURN(true);
}

/*
** MT-NOTE: standard sge_decrypt() is MT safe
*/
bool sge_decrypt(char *intext, int inlen, char *out_buffer, int* outsize)
{
   DENTER(TOP_LAYER);

   // decode
   unsigned char buffer[2 * SGE_SEC_BUFSIZE];
   int buffer_len = sizeof(buffer);
   if (!change_encoding(intext, &inlen, buffer, &buffer_len, DECODE_FROM_STRING)) {
      DRETURN(false);
   }

   // return the result
   buffer[buffer_len] = '\0';
   strcpy(out_buffer, (char*)buffer);
   DRETURN(true);
}

#else

/*
** MT-NOTE: EVP based sge_encrypt() is not MT safe
*/
static bool sge_encrypt(char *intext, int inlen, char *outbuf, int outsize)
{

   int enclen, tmplen;
   unsigned char encbuf[2*SGE_SEC_BUFSIZE];

   unsigned char key[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
   unsigned char iv[] = {1,2,3,4,5,6,7,8};
   EVP_CIPHER_CTX ctx;

   DENTER(TOP_LAYER);

/*    DPRINTF("======== intext:\n" SFN "\n=========\n", intext); */

   if (!EVP_EncryptInit(&ctx, /*EVP_enc_null() EVP_bf_cbc()*/EVP_cast5_ofb(), key, iv)) {
      printf("EVP_EncryptInit failure !!!!!!!\n");
      DRETURN(false);
   }   

   if (!EVP_EncryptUpdate(&ctx, encbuf, &enclen, (unsigned char*) intext, inlen)) {
      DRETURN(false);
   }

   if (!EVP_EncryptFinal(&ctx, encbuf + enclen, &tmplen)) {
      DRETURN(false);
   }
   enclen += tmplen;
   EVP_CIPHER_CTX_cleanup(&ctx);

   if (!change_encoding(outbuf, &outsize, encbuf, &enclen, ENCODE_TO_STRING)) {
      DRETURN(false);
   }   

/*    DPRINTF("======== outbuf:\n" SFN "\n=========\n", outbuf); */

   DRETURN(true);
}

static bool sge_decrypt(char *intext, int inlen, char *outbuf, int* outsize)
{

   int outlen, tmplen;
   unsigned char decbuf[2*SGE_SEC_BUFSIZE];
   int declen = sizeof(decbuf);

   unsigned char key[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
   unsigned char iv[] = {1,2,3,4,5,6,7,8};
   EVP_CIPHER_CTX ctx;

   DENTER(TOP_LAYER);

   if (!change_encoding(intext, &inlen, decbuf, &declen, DECODE_FROM_STRING)) {
      DRETURN(false);
   }   

   if (!EVP_DecryptInit(&ctx, /* EVP_enc_null() EVP_bf_cbc()*/EVP_cast5_ofb(), key, iv)) {
      DRETURN(false);
   }
   
   if (!EVP_DecryptUpdate(&ctx, (unsigned char*)outbuf, &outlen, decbuf, declen)) {
      DRETURN(false);
   }

   if (!EVP_DecryptFinal(&ctx, (unsigned char*)outbuf + outlen, &tmplen)) {
      DRETURN(false);
   }
   EVP_CIPHER_CTX_cleanup(&ctx);

   *outsize = outlen+tmplen;

/*    DPRINTF("======== outbuf:\n" SFN "\n=========\n", outbuf); */

   DRETURN(true);
}

#endif


#define LOQUAD(i) (((i)&0x0F))
#define HIQUAD(i) (((i)&0xF0)>>4)
#define SETBYTE(hi, lo)  ((((hi)<<4)&0xF0) | (0x0F & (lo)))

/*
 *
 * NOTES
 *    MT-NOTE: change_encoding() is MT safe
 *
 */
bool change_encoding(char *cbuf, int* csize, unsigned char* ubuf, int* usize, int mode)
{
   static const char alphabet[17] = {"*b~de,gh&jklrn=p"};

   DENTER(TOP_LAYER);

   if (mode == ENCODE_TO_STRING) {
      /*
      ** encode to string
      */
      int i, j;
      int enclen = *usize;
      if ((*csize) < (2*enclen+1)) {
         DRETURN(false);
      }

      for (i=0,j=0; i<enclen; i++) {
         cbuf[j++] = alphabet[HIQUAD(ubuf[i])];
         cbuf[j++] = alphabet[LOQUAD(ubuf[i])];
      }
      cbuf[j] = '\0';
   }

   if (mode == DECODE_FROM_STRING) {
      /*
      ** decode from string
      */
      char *p;
      int declen;
      if ((*usize) < (*csize)) {
         DRETURN(false);
      }
      for (p=cbuf, declen=0; *p; p++, declen++) {
         int hi, lo, j;
         for (j=0; j<16; j++) {
            if (*p == alphabet[j]) 
               break;
         }
         hi = j;
         p++;
         for (j=0; j<16; j++) {
            if (*p == alphabet[j]) 
               break;
         }
         lo = j;
         ubuf[declen] = (unsigned char) SETBYTE(hi, lo);
      }   
      *usize = declen;
   }
      
   DRETURN(true);
}

/* MT-NOTE: sge_security_verify_user() is MT safe (assumptions) */
bool
sge_security_verify_user(const char *host, const char *commproc, u_long32 id, const char *gdi_user)
{
   DENTER(TOP_LAYER);

   if (gdi_user == nullptr || host == nullptr || commproc == nullptr) {
      DRETURN(false);
   }

   const char *admin_user = bootstrap_get_admin_user();
   if (is_daemon(commproc) && strcmp(gdi_user, admin_user) != 0 && !sge_is_user_superuser(gdi_user)) {
      DRETURN(false);
   }

   const char *component_name = component_get_component_name();
   bool check_admin_user = is_daemon(commproc);
   const char *user = check_admin_user ? admin_user : gdi_user;
   if (!sge_security_verify_unique_identifier(check_admin_user, user, component_name, 0, host, commproc, id)) {
      DRETURN(false);
   }

#ifdef KERBEROS

   if (krb_verify_user(host, commproc, id, user) < 0) {
      DRETURN(false);
   }

#endif /* KERBEROS */

   DRETURN(true);
}

bool sge_security_verify_unique_identifier(bool check_admin_user, const char* user, const char* progname,
        unsigned long progid, const char* hostname, const char* commproc, unsigned long commid) {

   DENTER(TOP_LAYER);

#ifdef SECURE

   if (user == nullptr || progname == nullptr || hostname == nullptr || commproc == nullptr) {
      DRETURN(false);
   }

   if (feature_is_enabled(FEATURE_CSP_SECURITY)) {
      int ret = CL_RETVAL_OK;
      cl_com_handle_t* handle = nullptr;
      char* unique_identifier = nullptr;

      DPRINTF("sge_security_verify_unique_identifier: progname, progid = %s, %d\n", progname, (int)progid);
      handle = cl_com_get_handle(progname, progid);
      DPRINTF("sge_security_verify_unique_identifier: hostname, commproc, commid = %s, %s, %d\n", hostname, commproc, (int)commid);
      ret = cl_com_ssl_get_unique_id(handle, (char*)hostname, (char*)commproc, commid, &unique_identifier);
      if (ret == CL_RETVAL_OK) {
         DPRINTF("unique identifier = " SFQ "\n", unique_identifier );
         DPRINTF("user = " SFQ "\n", user);
      } else {
         DPRINTF("-------> CL_RETVAL: %s\n", cl_get_error_text(ret));
      }

      if ( unique_identifier == nullptr ) {
         DPRINTF("unique_identifier is nullptr\n");
         DRETURN(false);
      }

      if (check_admin_user) {
         if (strcmp(unique_identifier, user) != 0 
            && !sge_is_user_superuser(unique_identifier)) { 
            DPRINTF(MSG_ADMIN_REQUEST_DENIED_FOR_USER_S, user ? user: "nullptr");
            WARNING(MSG_ADMIN_REQUEST_DENIED_FOR_USER_S, user ? user: "nullptr");
            sge_free(&unique_identifier);
            DRETURN(false);
         }     
      } else {
         if (strcmp(unique_identifier, user) != 0) {
            DPRINTF(MSG_REQUEST_DENIED_FOR_USER_S, user ? user: "nullptr");
            WARNING(MSG_REQUEST_DENIED_FOR_USER_S, user ? user: "nullptr");
            sge_free(&unique_identifier);
            DRETURN(false);
         }
      }
      
      sge_free(&unique_identifier);
   }
#endif
   DRETURN(true);
}

/* MT-NOTE: sge_security_ck_to_do() is MT safe (assumptions) */
void sge_security_event_handler(te_event_t anEvent, monitoring_t *monitor)
{
   DENTER(TOP_LAYER);  
#ifdef KERBEROS
   krb_check_for_idle_clients();
#endif
   DRETURN_VOID;
}

