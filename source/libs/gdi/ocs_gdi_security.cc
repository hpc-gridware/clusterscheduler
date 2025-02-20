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

#include "../cull/cull.h"

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

#include "ocs_gdi_security.h"
#include "msg_gdilib.h"

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
