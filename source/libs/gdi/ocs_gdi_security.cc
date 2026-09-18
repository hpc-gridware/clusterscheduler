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
 *  Portions of this software are Copyright (c) 2025-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Authenticating a GDI request and setting its user
 */

#include <cstdio>
#include <cstring>
#include <pwd.h>
#include <pthread.h>

#include "comm/cl_commlib.h"

#include "../cull/cull.h"

#include "uti/ocs_Bootstrap.h"
#include "uti/sge_afsutil.h"
#include "uti/sge_arch.h"
#include "uti/sge_hostname.h"
#include "uti/sge_io.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_uidgid.h"

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

#define ENCODE_TO_STRING   1 ///< direction flag: encode a credential into its string form
#define DECODE_FROM_STRING 0 ///< direction flag: decode a credential from its string form


/**
 * @brief Get credit for security system
 *
 * In AFS mode this runs get_token_cmd and stores the resulting token in the
 * job, so that the execution side can hand it to the shepherd.
 * If an error occurs the return value is unequal 0
 *
 * @param sge_root the installation root
 * @param mastername host qmaster runs on, used when acquiring the credential
 * @param job the job structure the credential is stored in
 * @param[out] alpp receives the reason on failure
 *
 * @return 0 in case of success, something different otherwise
 *
 * @note MT-NOTE: set_sec_cred() is MT safe (major assumptions!)
 */
int set_sec_cred(const char *sge_root, const char *mastername, lListElem *job, lList **alpp) {
   DENTER(TOP_LAYER);

   pid_t command_pid;
   FILE *fp_in, *fp_out, *fp_err;
   char *str;
   int ret = 0;
   char binary[1024];

   if (ocs::Bootstrap::has_security_mode(ocs::Bootstrap::BS_SEC_MODE_AFS)) {
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
      
   DRETURN(ret);
}

