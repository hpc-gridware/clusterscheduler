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
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <cstring>
#include <pwd.h>
#include <pthread.h>

#include "comm/cl_commlib.h"

#include "sge_bootstrap.h"
#include "sge_hostname.h"
#include "sge_log.h"
#include "sge_rmon_macros.h"
#include "sge_stdio.h"
#include "sge_uidgid.h"

#include "sge_security.h"

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

#if defined(SECURE)
   const char *component_name = component_get_component_name();
   bool check_admin_user = is_daemon(commproc);
   const char *user = check_admin_user ? admin_user : gdi_user;
   if (!sge_security_verify_unique_identifier(check_admin_user, user, component_name, 0, host, commproc, id)) {
      DRETURN(false);
   }
#endif

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

