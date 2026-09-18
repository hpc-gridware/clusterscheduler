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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Security layer setup and user authentication helpers
 */

#include <cstdio>
#include <cstring>
#include <pwd.h>
#include <pthread.h>

#include "comm/cl_commlib.h"

#include "sge_hostname.h"
#include "sge_log.h"
#include "sge_rmon_macros.h"
#include "sge_uidgid.h"

#include "sge_security.h"

#include "ocs_Bootstrap.h"

#ifdef CRYPTO
#include <openssl/evp.h>
#endif




static bool is_daemon(const char* progname) {
   if (progname != nullptr) {
      if ( !strcmp(to_cstr(QMASTER), progname) ||
           !strcmp(to_cstr(EXECD)  , progname) ||
           !strcmp(to_cstr(SCHEDD) , progname)) {
         return true;
      }
   }
   return false;
}



/* MT-NOTE: sge_security_verify_user() is MT safe (assumptions) */
bool
/** @brief Check that a request really comes from the user it claims
 *
 * @param host host the request arrived from
 * @param commproc name of the sending component
 * @param id commlib id of the sender
 * @param gdi_user user the request claims to be from
 * @return true when the claim holds
 */
sge_security_verify_user(const char *host, const char *commproc, uint32_t id, const char *gdi_user) {
   DENTER(TOP_LAYER);

   if (gdi_user == nullptr || host == nullptr || commproc == nullptr) {
      DRETURN(false);
   }

   const char *admin_user = ocs::Bootstrap::get_admin_user();
   if (is_daemon(commproc) && strcmp(gdi_user, admin_user) != 0 && !sge_is_user_superuser(gdi_user)) {
      DRETURN(false);
   }



   DRETURN(true);
}
