#pragma once
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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Where the CSP security mode keeps its keys and certificates
 *
 * CSP is the certificate based security mode selected by `security_mode csp`
 * in the `bootstrap` file — see `ocs::Bootstrap::BS_SEC_MODE_CSP`. This module
 * only works out the file names; it neither reads nor validates them.
 *
 * The paths are derived once, at #sge_csp_path_class_create, from `$SGE_ROOT`,
 * `$SGE_CELL`, the qmaster port and the calling user. Daemons read the shared
 * CA directory, ordinary users a per-user directory under their home. Several
 * individual files can be overridden by the environment — see
 * #sge_csp_path_class_create.
 *
 * Written in the same C "class" idiom as `sge_error_class.h`: an opaque
 * handle plus a table of function pointers, called as `pc->get_cert_file(pc)`.
 */

#include "sge_error_class.h"
#include "comm/cl_data_types.h"

/// The CSP path set; see @ref sge_csp_path_class_str
typedef struct sge_csp_path_class_str sge_csp_path_class_t;

/**
 * @brief The file names CSP needs, with accessors for each
 *
 * The setters exist so a caller can override a single path after creation;
 * they replace the stored copy and do not touch the file system.
 */
struct sge_csp_path_class_str {
   void *sge_csp_path_handle; ///< opaque path storage, owned by the object

   /// Log every path in this object at debug level
   void (*dprintf)(sge_csp_path_class_t *thiz);

   /// Directory of the cluster-wide CA, `$SGE_ROOT/$SGE_CELL/common/sgeCA`
   const char *(*get_ca_root)(sge_csp_path_class_t *thiz);

   /// Directory of the local CA holding private keys, under `/var/sgeCA` or `/tmp/sgeCA`
   const char *(*get_ca_local_root)(sge_csp_path_class_t *thiz);

   /// The CA's own certificate
   const char *(*get_CA_cert_file)(sge_csp_path_class_t *thiz);

   /// The CA's private key; overridable with `$SGE_CAKEYFILE`
   const char *(*get_CA_key_file)(sge_csp_path_class_t *thiz);

   /// This user's or daemon's certificate; overridable with `$SGE_CERTFILE`
   const char *(*get_cert_file)(sge_csp_path_class_t *thiz);

   /// This user's or daemon's private key; overridable with `$SGE_KEYFILE`
   const char *(*get_key_file)(sge_csp_path_class_t *thiz);

   /// Seed file for the random number generator
   const char *(*get_rand_file)(sge_csp_path_class_t *thiz);

   /// Reconnect data file; computed but not used by any current code
   const char *(*get_reconnect_file)(sge_csp_path_class_t *thiz);

   /// The certificate revocation list
   const char *(*get_crl_file)(sge_csp_path_class_t *thiz);

   /// Password for encrypted key files; never set by any current code
   const char *(*get_password)(sge_csp_path_class_t *thiz);

   /// Connection refresh time; never set by any current code, so always 0
   int (*get_refresh_time)(sge_csp_path_class_t *thiz);

   /// Callback the commlib uses to verify a peer certificate, or nullptr
   cl_ssl_verify_func_t (*get_verify_func)(sge_csp_path_class_t *thiz);

   /// Override the CA certificate path
   void (*set_CA_cert_file)(sge_csp_path_class_t *thiz, const char *CA_cert_file);

   /// Override the CA key path
   void (*set_CA_key_file)(sge_csp_path_class_t *thiz, const char *CA_key_file);

   /// Override the certificate path
   void (*set_cert_file)(sge_csp_path_class_t *thiz, const char *cert_file);

   /// Override the key path
   void (*set_key_file)(sge_csp_path_class_t *thiz, const char *key_file);

   /// Override the random seed path
   void (*set_rand_file)(sge_csp_path_class_t *thiz, const char *rand_file);

   /// Override the reconnect data path
   void (*set_reconnect_file)(sge_csp_path_class_t *thiz, const char *reconnect_file);

   /// Override the revocation list path
   void (*set_crl_file)(sge_csp_path_class_t *thiz, const char *crl_file);

   /// Set the password for encrypted key files
   void (*set_password)(sge_csp_path_class_t *thiz, const char *password);

   /// Set the connection refresh time
   void (*set_refresh_time)(sge_csp_path_class_t *thiz, uint32_t refresh_time);

   /// Install the peer certificate verification callback
   void (*set_verify_func)(sge_csp_path_class_t *thiz, cl_ssl_verify_func_t verify_func);
};

sge_csp_path_class_t *
sge_csp_path_class_create(sge_error_class_t *eh);

void sge_csp_path_class_destroy(sge_csp_path_class_t **pst);
