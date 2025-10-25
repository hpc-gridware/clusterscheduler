/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025 HPC-Gridware GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

#include <filesystem>
#include <string>
#include <system_error>

#include <dlfcn.h>
#include <libgen.h>

#include "uti/msg_utilib.h"
#include "uti/sge_component.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"

#include "ocs_OpenSSL.h"

#include <ocs_OpenSSL.h>
#include <sge_bootstrap_env.h>
#include <sge_string.h>

#if defined(OCS_WITH_OPENSSL)

namespace ocs::uti {
   // static members
   void *OpenSSL::libssl_handle = nullptr;

   std::vector<OpenSSL::OpenSSLContext *> OpenSSL::OpenSSLContext::contexts_to_delete;

   ASN1_INTEGER_set_func_t OpenSSL::ASN1_INTEGER_set_func = nullptr;
   ASN1_TIME_diff_func_t OpenSSL::ASN1_TIME_diff_func = nullptr;
   BIO_free_func_t OpenSSL::BIO_free_func = nullptr;
   BIO_new_func_t OpenSSL::BIO_new_func = nullptr;
   BIO_number_written_func_t OpenSSL::BIO_number_written_func = nullptr;
   BIO_read_func_t OpenSSL::BIO_read_func = nullptr;
   BIO_s_mem_func_t OpenSSL::BIO_s_mem_func = nullptr;
   BN_free_func_t OpenSSL::BN_free_func = nullptr;
   BN_new_func_t OpenSSL::BN_new_func = nullptr;
   BN_set_word_func_t OpenSSL::BN_set_word_func = nullptr;
   ERR_clear_error_func_t OpenSSL::ERR_clear_error_func = nullptr;
   ERR_get_error_func_t OpenSSL::ERR_get_error_func = nullptr;
   ERR_reason_error_string_func_t OpenSSL::ERR_reason_error_string_func = nullptr;
   EVP_PKEY_assign_func_t OpenSSL::EVP_PKEY_assign_func = nullptr;
   EVP_PKEY_free_func_t OpenSSL::EVP_PKEY_free_func = nullptr;
   EVP_PKEY_new_func_t OpenSSL::EVP_PKEY_new_func = nullptr;
   EVP_sha256_func_t OpenSSL::EVP_sha256_func = nullptr;
   OPENSSL_init_ssl_func_t OpenSSL::OPENSSL_init_ssl_func = nullptr;
   PEM_read_X509_func_t OpenSSL::PEM_read_X509_func = nullptr;
   PEM_write_PrivateKey_func_t OpenSSL::PEM_write_PrivateKey_func = nullptr;
   PEM_write_X509_func_t OpenSSL::PEM_write_X509_func = nullptr;
   PEM_write_bio_X509_func_t OpenSSL::PEM_write_bio_X509_func = nullptr;
   RSA_free_func_t OpenSSL::RSA_free_func = nullptr;
   RSA_generate_key_ex_func_t OpenSSL::RSA_generate_key_ex_func = nullptr;
   RSA_new_func_t OpenSSL::RSA_new_func = nullptr;
   SSL_CTX_free_func_t OpenSSL::SSL_CTX_free_func = nullptr;
   SSL_CTX_get0_certificate_func_t OpenSSL::SSL_CTX_get0_certificate_func = nullptr;
   SSL_CTX_load_verify_locations_func_t OpenSSL::SSL_CTX_load_verify_locations_func = nullptr;
   SSL_CTX_new_func_t OpenSSL::SSL_CTX_new_func = nullptr;
   SSL_CTX_set_verify_func_t OpenSSL::SSL_CTX_set_verify_func = nullptr;
   SSL_CTX_use_PrivateKey_file_func_t OpenSSL::SSL_CTX_use_PrivateKey_file_func = nullptr;
   SSL_CTX_use_PrivateKey_func_t OpenSSL::SSL_CTX_use_PrivateKey_func = nullptr;
   SSL_CTX_use_certificate_chain_file_func_t OpenSSL::SSL_CTX_use_certificate_chain_file_func = nullptr;
   SSL_CTX_use_certificate_func_t OpenSSL::SSL_CTX_use_certificate_func = nullptr;
   SSL_accept_func_t OpenSSL::SSL_accept_func = nullptr;
   SSL_connect_func_t OpenSSL::SSL_connect_func = nullptr;
   SSL_ctrl_func_t OpenSSL::SSL_ctrl_func = nullptr;
   SSL_free_func_t OpenSSL::SSL_free_func = nullptr;
   SSL_get_error_func_t OpenSSL::SSL_get_error_func = nullptr;
   SSL_new_func_t OpenSSL::SSL_new_func = nullptr;
   SSL_read_func_t OpenSSL::SSL_read_func = nullptr;
   SSL_set1_host_func_t OpenSSL::SSL_set1_host_func = nullptr;
   SSL_set_fd_func_t OpenSSL::SSL_set_fd_func = nullptr;
   SSL_shutdown_func_t OpenSSL::SSL_shutdown_func = nullptr;
   SSL_write_func_t OpenSSL::SSL_write_func = nullptr;
   TLS_client_method_func_t OpenSSL::TLS_client_method_func = nullptr;
   TLS_server_method_func_t OpenSSL::TLS_server_method_func = nullptr;
   X509_NAME_add_entry_by_txt_func_t OpenSSL::X509_NAME_add_entry_by_txt_func = nullptr;
   X509_free_func_t OpenSSL::X509_free_func = nullptr;
   X509_get0_notAfter_func_t OpenSSL::X509_get0_notAfter_func = nullptr;
   X509_get_serialNumber_func_t OpenSSL::X509_get_serialNumber_func = nullptr;
   X509_get_subject_name_func_t OpenSSL::X509_get_subject_name_func = nullptr;
   X509_getm_notAfter_func_t OpenSSL::X509_getm_notAfter_func = nullptr;
   X509_getm_notBefore_func_t OpenSSL::X509_getm_notBefore_func = nullptr;
   X509_gmtime_adj_func_t OpenSSL::X509_gmtime_adj_func = nullptr;
   X509_new_func_t OpenSSL::X509_new_func = nullptr;
   X509_set_issuer_name_func_t OpenSSL::X509_set_issuer_name_func = nullptr;
   X509_set_pubkey_func_t OpenSSL::X509_set_pubkey_func = nullptr;
   X509_set_version_func_t OpenSSL::X509_set_version_func = nullptr;
   X509_sign_func_t OpenSSL::X509_sign_func = nullptr;

   /**
    * @brief Initializes the OpenSSL library by loading libssl.so.3 and resolving all required function symbols.
    *
    * This function dynamically loads the OpenSSL shared library (libssl.so.3) and resolves
    * all necessary function pointers for SSL/TLS operations. It can only be called once;
    * later calls will fail with an error.
    *
    * The function loads and resolves symbols for:
    * - ASN1 operations (time, integer handling)
    * - BIO operations (memory buffers, I/O)
    * - BIGNUM operations
    * - Error handling functions
    * - EVP (envelope) operations for keys and digests
    * - PEM file I/O
    * - RSA key generation
    * - SSL/TLS context and connection operations
    * - X.509 certificate operations
    *
    * @param error_dstr Output parameter for error messages. Will be populated if initialization fails.
    *
    * @return true if initialization succeeded, false otherwise
    *
    * @note This function must be called before any other OpenSSL wrapper functions.
    * @note If initialization fails, all allocated resources are automatically cleaned up.
    * @see cleanup()
    */

   // static methods
   bool OpenSSL::initialize(dstring *error_dstr) {
      DENTER(TOP_LAYER);

      bool ret = true;

      // initialize only once
      if (ret && libssl_handle != nullptr) {
         sge_dstring_sprintf(error_dstr, SFNMAX, MSG_OPENSSL_ALREADY_INITIALIZED);
         ret = false;
      }

      // Load the shared library and the required functions
      if (ret) {
         const char *libssl = "libssl.so.3";
         libssl_handle = dlopen(libssl, RTLD_LAZY);
         if (libssl_handle == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_OPEN_LIB_SS, libssl, dlerror());
            ret = false;
         }
      }

      // load the functions
      const char *func;
      if (ret) {
         func = "ASN1_INTEGER_set";
         ASN1_INTEGER_set_func = reinterpret_cast<ASN1_INTEGER_set_func_t>(dlsym(libssl_handle, func));
         if (ASN1_INTEGER_set_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "ASN1_TIME_diff";
         ASN1_TIME_diff_func = reinterpret_cast<ASN1_TIME_diff_func_t>(dlsym(libssl_handle, func));
         if (ASN1_TIME_diff_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "BIO_free";
         BIO_free_func = reinterpret_cast<BIO_free_func_t>(dlsym(libssl_handle, func));
         if (BIO_free_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "BIO_new";
         BIO_new_func = reinterpret_cast<BIO_new_func_t>(dlsym(libssl_handle, func));
         if (BIO_new_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "BIO_number_written";
         BIO_number_written_func = reinterpret_cast<BIO_number_written_func_t>(dlsym(libssl_handle, func));
         if (BIO_number_written_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "BIO_read";
         BIO_read_func = reinterpret_cast<BIO_read_func_t>(dlsym(libssl_handle, func));
         if (BIO_read_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "BIO_s_mem";
         BIO_s_mem_func = reinterpret_cast<BIO_s_mem_func_t>(dlsym(libssl_handle, func));
         if (BIO_s_mem_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "BN_free";
         BN_free_func = reinterpret_cast<BN_free_func_t>(dlsym(libssl_handle, func));
         if (BN_free_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "BN_new";
         BN_new_func = reinterpret_cast<BN_new_func_t>(dlsym(libssl_handle, func));
         if (BN_new_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "BN_set_word";
         BN_set_word_func = reinterpret_cast<BN_set_word_func_t>(dlsym(libssl_handle, func));
         if (BN_set_word_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "ERR_clear_error";
         ERR_clear_error_func = reinterpret_cast<ERR_clear_error_func_t>(dlsym(libssl_handle, func));
         if (ERR_clear_error_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "ERR_get_error";
         ERR_get_error_func = reinterpret_cast<ERR_get_error_func_t>(dlsym(libssl_handle, func));
         if (ERR_get_error_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "ERR_reason_error_string";
         ERR_reason_error_string_func = reinterpret_cast<ERR_reason_error_string_func_t>(dlsym(libssl_handle, func));
         if (ERR_reason_error_string_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "EVP_PKEY_assign";
         EVP_PKEY_assign_func = reinterpret_cast<EVP_PKEY_assign_func_t>(dlsym(libssl_handle, func));
         if (EVP_PKEY_assign_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "EVP_PKEY_free";
         EVP_PKEY_free_func = reinterpret_cast<EVP_PKEY_free_func_t>(dlsym(libssl_handle, func));
         if (EVP_PKEY_free_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "EVP_PKEY_new";
         EVP_PKEY_new_func = reinterpret_cast<EVP_PKEY_new_func_t>(dlsym(libssl_handle, func));
         if (EVP_PKEY_new_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "EVP_sha256";
         EVP_sha256_func = reinterpret_cast<EVP_sha256_func_t>(dlsym(libssl_handle, func));
         if (EVP_sha256_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "OPENSSL_init_ssl";
         OPENSSL_init_ssl_func = reinterpret_cast<OPENSSL_init_ssl_func_t>(dlsym(libssl_handle, func));
         if (OPENSSL_init_ssl_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "PEM_read_X509";
         PEM_read_X509_func = reinterpret_cast<PEM_read_X509_func_t>(dlsym(libssl_handle, func));
         if (PEM_read_X509_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "PEM_write_PrivateKey";
         PEM_write_PrivateKey_func = reinterpret_cast<PEM_write_PrivateKey_func_t>(dlsym(libssl_handle, func));
         if (PEM_write_PrivateKey_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "PEM_write_X509";
         PEM_write_X509_func = reinterpret_cast<PEM_write_X509_func_t>(dlsym(libssl_handle, func));
         if (PEM_write_X509_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "PEM_write_bio_X509";
         PEM_write_bio_X509_func = reinterpret_cast<PEM_write_bio_X509_func_t>(dlsym(libssl_handle, func));
         if (PEM_write_bio_X509_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "RSA_free";
         RSA_free_func = reinterpret_cast<RSA_free_func_t>(dlsym(libssl_handle, func));
         if (RSA_free_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "RSA_generate_key_ex";
         RSA_generate_key_ex_func = reinterpret_cast<RSA_generate_key_ex_func_t>(dlsym(libssl_handle, func));
         if (RSA_generate_key_ex_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "RSA_new";
         RSA_new_func = reinterpret_cast<RSA_new_func_t>(dlsym(libssl_handle, func));
         if (RSA_new_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_CTX_free";
         SSL_CTX_free_func = reinterpret_cast<SSL_CTX_free_func_t>(dlsym(libssl_handle, func));
         if (SSL_CTX_free_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_CTX_get0_certificate";
         SSL_CTX_get0_certificate_func = reinterpret_cast<SSL_CTX_get0_certificate_func_t>(dlsym(libssl_handle, func));
         if (SSL_CTX_get0_certificate_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_CTX_load_verify_locations";
         SSL_CTX_load_verify_locations_func = reinterpret_cast<SSL_CTX_load_verify_locations_func_t>(dlsym(libssl_handle, func));
         if (SSL_CTX_load_verify_locations_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_CTX_new";
         SSL_CTX_new_func = reinterpret_cast<SSL_CTX_new_func_t>(dlsym(libssl_handle, func));
         if (SSL_CTX_new_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_CTX_set_verify";
         SSL_CTX_set_verify_func = reinterpret_cast<SSL_CTX_set_verify_func_t>(dlsym(libssl_handle, func));
         if (SSL_CTX_set_verify_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_CTX_use_certificate";
         SSL_CTX_use_certificate_func = reinterpret_cast<SSL_CTX_use_certificate_func_t>(dlsym(libssl_handle, func));
         if (SSL_CTX_use_certificate_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_CTX_use_certificate_chain_file";
         SSL_CTX_use_certificate_chain_file_func = reinterpret_cast<SSL_CTX_use_certificate_chain_file_func_t>(dlsym(libssl_handle, func));
         if (SSL_CTX_use_certificate_chain_file_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_CTX_use_PrivateKey";
         SSL_CTX_use_PrivateKey_func = reinterpret_cast<SSL_CTX_use_PrivateKey_func_t>(dlsym(libssl_handle, func));
         if (SSL_CTX_use_PrivateKey_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_CTX_use_PrivateKey_file";
         SSL_CTX_use_PrivateKey_file_func = reinterpret_cast<SSL_CTX_use_PrivateKey_file_func_t>(dlsym(libssl_handle, func));
         if (SSL_CTX_use_PrivateKey_file_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_accept";
         SSL_accept_func = reinterpret_cast<SSL_accept_func_t>(dlsym(libssl_handle, func));
         if (SSL_accept_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_connect";
         SSL_connect_func = reinterpret_cast<SSL_connect_func_t>(dlsym(libssl_handle, func));
         if (SSL_connect_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_ctrl";
         SSL_ctrl_func = reinterpret_cast<SSL_ctrl_func_t>(dlsym(libssl_handle, func));
         if (SSL_ctrl_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_free";
         SSL_free_func = reinterpret_cast<SSL_free_func_t>(dlsym(libssl_handle, func));
         if (SSL_free_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_get_error";
         SSL_get_error_func = reinterpret_cast<SSL_get_error_func_t>(dlsym(libssl_handle, func));
         if (SSL_get_error_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_new";
         SSL_new_func = reinterpret_cast<SSL_new_func_t>(dlsym(libssl_handle, func));
         if (SSL_new_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_read";
         SSL_read_func = reinterpret_cast<SSL_read_func_t>(dlsym(libssl_handle, func));
         if (SSL_read_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_set1_host";
         SSL_set1_host_func = reinterpret_cast<SSL_set1_host_func_t>(dlsym(libssl_handle, func));
         if (SSL_set1_host_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_set_fd";
         SSL_set_fd_func = reinterpret_cast<SSL_set_fd_func_t>(dlsym(libssl_handle, func));
         if (SSL_set_fd_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_shutdown";
         SSL_shutdown_func = reinterpret_cast<SSL_shutdown_func_t>(dlsym(libssl_handle, func));
         if (SSL_shutdown_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_write";
         SSL_write_func = reinterpret_cast<SSL_write_func_t>(dlsym(libssl_handle, func));
         if (SSL_write_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "TLS_client_method";
         TLS_client_method_func = reinterpret_cast<TLS_client_method_func_t>(dlsym(libssl_handle, func));
         if (TLS_client_method_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "TLS_server_method";
         TLS_server_method_func = reinterpret_cast<TLS_server_method_func_t>(dlsym(libssl_handle, func));
         if (TLS_server_method_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "X509_free";
         X509_free_func = reinterpret_cast<X509_free_func_t>(dlsym(libssl_handle, func));
         if (X509_free_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "X509_get0_notAfter";
         X509_get0_notAfter_func = reinterpret_cast<X509_get0_notAfter_func_t>(dlsym(libssl_handle, func));
         if (X509_get0_notAfter_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "X509_get_serialNumber";
         X509_get_serialNumber_func = reinterpret_cast<X509_get_serialNumber_func_t>(dlsym(libssl_handle, func));
         if (X509_get_serialNumber_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "X509_get_subject_name";
         X509_get_subject_name_func = reinterpret_cast<X509_get_subject_name_func_t>(dlsym(libssl_handle, func));
         if (X509_get_subject_name_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "X509_getm_notAfter";
         X509_getm_notAfter_func = reinterpret_cast<X509_getm_notAfter_func_t>(dlsym(libssl_handle, func));
         if (X509_getm_notAfter_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "X509_getm_notBefore";
         X509_getm_notBefore_func = reinterpret_cast<X509_getm_notBefore_func_t>(dlsym(libssl_handle, func));
         if (X509_getm_notBefore_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "X509_gmtime_adj";
         X509_gmtime_adj_func = reinterpret_cast<X509_gmtime_adj_func_t>(dlsym(libssl_handle, func));
         if (X509_gmtime_adj_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "X509_NAME_add_entry_by_txt";
         X509_NAME_add_entry_by_txt_func = reinterpret_cast<X509_NAME_add_entry_by_txt_func_t>(dlsym(libssl_handle, func));
         if (X509_NAME_add_entry_by_txt_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "X509_new";
         X509_new_func = reinterpret_cast<X509_new_func_t>(dlsym(libssl_handle, func));
         if (X509_new_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "X509_set_issuer_name";
         X509_set_issuer_name_func = reinterpret_cast<X509_set_issuer_name_func_t>(dlsym(libssl_handle, func));
         if (X509_set_issuer_name_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "X509_set_pubkey";
         X509_set_pubkey_func = reinterpret_cast<X509_set_pubkey_func_t>(dlsym(libssl_handle, func));
         if (X509_set_pubkey_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "X509_set_version";
         X509_set_version_func = reinterpret_cast<X509_set_version_func_t>(dlsym(libssl_handle, func));
         if (X509_set_version_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "X509_sign";
         X509_sign_func = reinterpret_cast<X509_sign_func_t>(dlsym(libssl_handle, func));
         if (X509_sign_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }

      if (ret) {
         // this should actually not be necessary, unless we want to change the default settings
         if (!OPENSSL_init_ssl_func(0, nullptr)) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_INIT_FAILED);
            ret = false;
         }
      }

      // if the initialization failed, free everything again
      if (!ret) {
         cleanup();
      }

      DRETURN(ret);
   }

   /**
    * @brief Cleans up and unloads the OpenSSL library.
    *
    * This function closes the dynamically loaded libssl shared library handle
    * and sets it to nullptr. After calling this function, no OpenSSL operations
    * can be performed until initialize() is called again.
    *
    * @see initialize()
    */
   void OpenSSL::cleanup() {
      DENTER(TOP_LAYER);
      // close the library
      if (libssl_handle != nullptr) {
         dlclose(libssl_handle);
         libssl_handle = nullptr;
      }
      DRETURN_VOID;
   }

   /**
    * @brief Constructs the filesystem path for storing an SSL certificate.
    *
    * This function builds the path where an SSL certificate should be stored based on
    * whether it's for a daemon or a user process. The path format depends on the home_dir parameter:
    *
    * - For daemons (home_dir == nullptr):
    *   $SGE_ROOT/$SGE_CELL/common/certs/component_hostname.pem
    *
    * - For user processes we currently generate certificates and key on the fly and only in memory.
    *   Once we implement CS-1576 cache the per user certificate used by qrsh for the IJS connection
    *   we will store certificates and keys in the user's homedirectory, in directories below a
    *   `.ocs` directory, e.g., $HOME/.ocs/certs/component_hostname.pem
    *
    * @param cert_path Output parameter that will contain the constructed path
    * @param home_dir Home directory for user certificates, or nullptr for daemon certificates
    * @param hostname The hostname to include in the certificate filename
    * @param comp_name The component name (e.g., "qmaster", "execd") to include in the filename
    *
    * @return true if the path was successfully constructed, false if required parameters are nullptr
    *
    * @note The actual directory may not exist yet; this function only constructs the path string.
    * @see build_key_path()
    */
#define PER_USER_AND_HOST_CERTS
   bool OpenSSL::build_cert_path(std::string &cert_path, const char *home_dir, const char *hostname, const char *comp_name) {
      DENTER(TOP_LAYER);
      bool ret = true;
      // need info
      // -> daemon or user certificate?
      //    -> daemon: $SGE_ROOT/$SGE_CELL/common/certs/hostname.pem
      //    -> with CS-1576: user: $HOME/.ocs/certs/hostname.pem OR $HOME/.ocs/certs/cert.pem
      if (hostname == nullptr || comp_name == nullptr) {
         // @todo use error_dstr
         ret = false;
      } else {
         if (home_dir == nullptr) {
            cert_path = std::string(bootstrap_get_sge_root()) + "/" + std::string(bootstrap_get_sge_cell()) +
               std::string("/common/certs/") + std::string(comp_name) + std::string("_") + std::string(hostname) + std::string(".pem");
         } else {
#if defined(PER_USER_AND_HOST_CERTS)
            cert_path = std::string(home_dir) + std::string("/.ocs/certs/") + std::string(comp_name) + std::string("_") +
               std::string(hostname) + std::string(".pem");
#else
            cert_path = std::string(home_dir) + std::string("/.ocs/certs/") + std::string(comp_name) + std::string(".pem");
#endif
         }
      }
      DRETURN(ret);
   }
   /**
    * @brief Constructs the filesystem path for storing an SSL private key.
    *
    * This function builds the path where an SSL private key should be stored based on
    * whether it's for a daemon or a user process. The path format depends on the home_dir parameter:
    *
    * - For daemons (home_dir == nullptr):
    *   /var/lib/ocs/<port>/private/component_hostname.pem
    *   If port is 0, it's omitted from the path.
    *
    * - For user processes we currently generate certificates and key on the fly and only in memory.
    *   Once we implement CS-1576 cache the per user certificate used by qrsh for the IJS connection
    *   we will store certificates and keys in the user's homedirectory, in directories below a
    *   `.ocs` directory, e.g., $HOME/.ocs/private/component_hostname.pem
    *
    * @param key_path Output parameter that will contain the constructed path
    * @param home_dir Home directory for user keys, or nullptr for daemon keys
    * @param hostname The hostname to include in the key filename
    * @param port Port number to include in the path (daemon only), or 0 to omit
    * @param comp_name The component name (e.g., "qmaster", "execd") to include in the filename
    *
    * @return true if path was successfully constructed, false if required parameters are nullptr
    *
    * @note Private keys are stored in directories with restricted permissions (700).
    * @see build_cert_path()
    */
   bool OpenSSL::build_key_path(std::string &key_path, const char *home_dir, const char *hostname, u_long32 port, const char *comp_name) {
      DENTER(TOP_LAYER);
      bool ret = true;
      // -> daemon or user key?
      //    -> daemon: /var/lib/ocs/<port>/private/component_hostname.pem
      //    -> with CS-1576: user: $HOME/.ocs/private/hostname.pem OR $HOME/.ocs/private/key.pem
      if (hostname == nullptr || comp_name == nullptr) {
         // @todo use error_dstr
         ret = false;
      } else {
         if (home_dir == nullptr) {
            key_path = std::string("/var/lib/ocs/");
            if (port != 0) {
               key_path += std::to_string(port);
            }
            key_path += std::string("/private/") + std::string(comp_name) + std::string("_") + std::string(hostname) + std::string(".pem");
         } else {
#if defined(PER_USER_AND_HOST_CERTS)
            key_path = std::string(home_dir) + std::string("/.ocs/private/") + std::string(comp_name) + std::string("_") + std::string(hostname) + std::string(".pem");
#else
            key_path = std::string(home_dir) + std::string("/.ocs/private/") + std::string(comp_name) + std::string(".pem");
#endif
         }
      }
      DRETURN(ret);
   }

   /**
    * @brief Verifies that certificate and key directories exist, creating them if necessary.
    *
    * This function checks for the existence of directories needed for storing certificates
    * and private keys. If they don't exist, it creates them with appropriate permissions:
    * - Certificate directory: mode 755 (rwxr-xr-x)
    * - Private key directory: mode 700 (rwx------)
    *
    * The function handles user switching appropriately:
    * - For daemon processes, switches between root and admin user as needed
    * - Certificate directories may require admin user permissions (SGE_ROOT might be on NFS)
    * - Key directories require root permissions (/var/lib/ocs)
    *
    * @param switch_user If true, perform user switching between root and admin user
    * @param called_as_root True if the calling process is running as root
    * @param error_dstr Output parameter for error messages
    * @param created_dirs Output parameter, set to true if any directories were created
    *
    * @return true if directories exist or were successfully created, false on error
    */
   bool OpenSSL::OpenSSLContext::verify_create_directories(bool switch_user, bool called_as_root, dstring *error_dstr, bool &created_dirs) {
      DENTER(TOP_LAYER);
      bool ret = true;

      if (ret) {
         // @todo CS-1530 can we prevent multiple processes from creating the directories?
         //       probably not, but we shouldn't fail on EEXISTS
         std::filesystem::path cert_dir = cert_path.parent_path();
         if (!std::filesystem::exists(cert_dir)) {
            // need to switch to admin user if we are root
            // SGE_ROOT might be on a filesystem where root has no write permissions
            if (switch_user && called_as_root) {
               sge_switch2admin_user();
            }
            std::error_code ec;
            if (!std::filesystem::create_directories(cert_dir, ec)) {
               sge_dstring_sprintf(error_dstr, MSG_OPENSSL_CANNOT_CREATE_CERT_DIR_SS, cert_dir.c_str(), ec.message().c_str());
               ret = false;
            } else {
               // chmod 755
               std::filesystem::permissions(cert_dir, std::filesystem::perms::owner_all |
                                            std::filesystem::perms::group_read | std::filesystem::perms::group_exec |
                                            std::filesystem::perms::others_read | std::filesystem::perms::others_exec,
                                            ec);
               if (ec) {
                  sge_dstring_sprintf(error_dstr, MSG_OPENSSL_CANNOT_PERM_CERT_DIR_SS, cert_dir.c_str(), ec.message().c_str());
                  ret = false;
               } else {
                  created_dirs = true;
               }
            }
            if (switch_user && called_as_root) {
               sge_switch2start_user();
            }
         }
      }
      if (ret) {
         // for the key file we need to be root
         // if we are admin user right now, switch to start user (root)
         if (switch_user && !called_as_root) {
            sge_switch2start_user();
         }
         std::filesystem::path key_dir = key_path.parent_path();
         if (!std::filesystem::exists(key_dir)) {
            std::error_code ec;
            if (!std::filesystem::create_directories(key_dir, ec)) {
               sge_dstring_sprintf(error_dstr, MSG_OPENSSL_CANNOT_CREATE_KEY_DIR_SS, key_dir.c_str(), ec.message().c_str());
               ret = false;
            } else {
               // chmod 700
               std::filesystem::permissions(key_dir, std::filesystem::perms::owner_all, ec);
               if (ec) {
                  sge_dstring_sprintf(error_dstr, MSG_OPENSSL_CANNOT_PERM_KEY_DIR_SS, key_dir.c_str(), ec.message().c_str());
                  ret = false;
               } else {
                  created_dirs = true;
               }
            }
         }
         if (switch_user && !called_as_root) {
            sge_switch2admin_user();
         }
      }

      DRETURN(ret);
   }

   /**
    * @brief Checks if certificate recreation is required based on the stored renewal time.
    *
    * This is a lightweight check that compares the stored renewal_time against the
    * current time. The renewal_time is typically set when a certificate is created
    * or read, indicating when the certificate should be renewed (usually when 75%
    * of its lifetime has passed).
    *
    * @return true if the certificate has expired or should be renewed, false otherwise
    *
    * @note This version does not read the certificate file; it only checks cached renewal time.
    * @see certificate_recreate_required(dstring*)
    */
   bool OpenSSL::OpenSSLContext::certificate_recreate_required() {
      DENTER(TOP_LAYER);

      bool ret = false;

      u_long64 now = sge_get_gmt64();
      if (renewal_time <= now) {
         // The certificate has expired.
         DPRINTF("the certificate has expired / is about to expire\n");
         ret = true;
      }

      DRETURN(ret);
   }

   /**
    * @brief Checks if certificate recreation is required by reading and analyzing the certificate file.
    *
    * This function performs a thorough check by:
    * 1. Opening and reading the certificate file from disk
    * 2. Parsing the X.509 certificate
    * 3. Calculating the time remaining until expiration
    * 4. Determining if renewal is needed (when less than 25% of lifetime remains)
    *
    * If the certificate file cannot be read or parsed, or if calculations fail,
    * the function returns true (indicating recreation is needed) and populates
    * the error_dstr with diagnostic information.
    *
    * @param error_dstr Output parameter for error/diagnostic messages
    *
    * @return true if the certificate should be recreated, false if it's still valid
    *
    * @note Sets the renewal_time member variable when a valid certificate is found.
    * @note Returns true (needs recreation) on any error reading or parsing the certificate.
    * @see certificate_recreate_required()
    */
   bool OpenSSL::OpenSSLContext::certificate_recreate_required(dstring *error_dstr) {
      DENTER(TOP_LAYER);
      bool ret = false;
      bool ok = true;
      X509 *cert = nullptr;

      // Try to open the certificate file.
      FILE *fp = fopen(cert_path.c_str(), "r");
      if (fp == nullptr) {
         sge_dstring_sprintf(error_dstr, MSG_OPENSSL_CANNOT_OPEN_CERT_FILE_SS, cert_path.c_str(), strerror(errno));
         ok = false;
         ret = true; // We cannot read the cert file, so try to recreate it.
      }

      if (ok) {
         // Read the certificate into memory.
         cert = PEM_read_X509_func(fp, nullptr, nullptr, nullptr);
         fclose(fp);
         if (cert == nullptr) {
            // @todo Do this and the following functions set some error code? No info in the man page!
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_CANNOT_READ_CERT_FILE_S, cert_path.c_str());
            // We could not read the certificate file - nothing else to do here and try to re-create it.
            ok = false;
            ret = true;
         }
      }

      // Calculate if the certificate has already expired or will expire soon.
      int days_left, secs_left;
      if (ok) {
         // Get the expiration date/time.
         const ASN1_TIME *notAfter = X509_get0_notAfter_func(cert);

         // Diff between notAfter and current time.
         // ==> If from or to is nullptr the current time is used.
         if (ASN1_TIME_diff_func(&days_left, &secs_left, nullptr, notAfter) != 1) {
            sge_dstring_sprintf(error_dstr, SFNMAX, MSG_OPENSSL_CANNOT_CALC_DIFF_TIME);
            // We cannot calculate the diff?
            // Nothing else to be done here, re-create the certificate.
            ok = false;
            ret = true;
         }
      }

      if (ok) {
         if (days_left < 0 || secs_left < 0) {
            // The certificate has already expired - re-create it.
            ret = true;
         } else {
            int sec_total = days_left * 86400 + secs_left;
            int certificate_lifetime = bootstrap_get_cert_lifetime();
            u_long64 now = sge_get_gmt64();

            // Renew when only 25% of the lifetime is left.
            renewal_time = sge_get_gmt64() + sge_gmt32_to_gmt64(sec_total - certificate_lifetime / 4);
            if (renewal_time <= now) {
               ret = true;
            }
         }
      }

      // Clean up.
      if (cert != nullptr) {
         X509_free_func(cert);
         cert = nullptr;
      }

      DRETURN(ret);
   }

   /**
    * @brief Configures an SSL_CTX for server-side operation with certificate and private key.
    *
    * This function handles the complete setup of an SSL server context:
    * 1. Creates the necessary directories for certificates and keys (if file-based)
    * 2. Checks if existing certificates need renewal
    * 3. Generates a new RSA key pair and self-signed X.509 certificate if needed
    * 4. Writes certificate and key to files (if file-based) or uses them from memory
    * 5. Configures the SSL_CTX with the certificate and private key
    *
    * Certificate generation details:
    * - 2048-bit RSA key with exponent RSA_F4
    * - X.509 v3 certificate
    * - Self-signed with SHA-256 signature
    * - Common Name (CN) set to qualified hostname
    * - Lifetime configurable via bootstrap_get_cert_lifetime()
    * - Renewal scheduled at 75% of lifetime
    *
    * User switching for daemons:
    * - Switches to admin user for writing certificates (may be on NFS)
    * - Switches to root for writing private keys (/var/lib/ocs)
    *
    * @param error_dstr Output parameter for error messages
    *
    * @return true if server context was successfully configured, false on error
    *
    * @note The EVP_PKEY and X509 objects are safely freed after being added to SSL_CTX
    *       (SSL_CTX internally increments their reference counts).
    * @note For user processes (qrsh), certificates are not stored on disk.
    */
   bool OpenSSL::OpenSSLContext::configure_server_context(dstring *error_dstr) {
      DENTER(TOP_LAYER);

      bool ret = true;

      // When we are starting as root and creating a daemon certificate
      // in $SGE_ROOT/$SGE_CELL/common/certs and /var/lib/ocs/private
      // we need to be root to write the key.
      // But as root we might not be able to create directories or files in $SGE_ROOT,
      // so we need to switch to admin user.
      // When we are renewing certificates during qmaster/execd run time, we are admin user,
      // so we need to switch to the start user (root) for writing the key and back to admin user
      // to write the cert file.
      // OTOH when running qrsh as root, we write the certificate and key in $HOME/.ocs
      // and need to be root to write there.
      int component = component_get_component_id();
      bool switch_user = (component == QMASTER || component == EXECD);
      bool called_as_root = geteuid() == SGE_SUPERUSER_UID;

      // Do we store certificate and key on file? For daemons: yes, for qrsh: no.
      bool file_based = !cert_path.empty() && !key_path.empty();
      // Do we have to initialize the SSL_CTX from files, or can we initialize it from memory?
      bool file_read_required = file_based;

      // If it is not file-based, we need to create the certificate in any case.
      bool create_certificate_and_key = !file_based;

      if (file_based) {
         // If the certificate or the key directory not yet exist, create them.
         // In this case we have to create the certificate and key.
         bool created_dirs = false;
         ret = verify_create_directories(switch_user, called_as_root, error_dstr, created_dirs);
         if (ret && created_dirs) {
            create_certificate_and_key = true;
         }
      }

      // We already have the directories and possibly the certificate + key,
      // check if they really exist and have not yet expired or are about to expire.
      if (ret && !create_certificate_and_key) {
         if (certificate_recreate_required(error_dstr)) {
            if (sge_dstring_strlen(error_dstr) > 0) {
               // When there were errors we renew the certificate
               // no way to report this except some debug output.
               DPRINTF("checking certificate lifetime had errors: %s\n", sge_dstring_get_string(error_dstr));
            }
            create_certificate_and_key = true;
         }
      }

      // So far all was OK, but we have to create the certificates.
      if (ret && create_certificate_and_key) {
         int certificate_lifetime = bootstrap_get_cert_lifetime();
         DPRINTF("creating certificate with lifetime %d\n", certificate_lifetime);

         // @todo can we create the certificate from some template?
         // @todo additional error handling?
         EVP_PKEY *pkey = EVP_PKEY_new_func();
         RSA *rsa = RSA_new_func();
         BIGNUM *bn = BN_new_func();
         X509 *x509 = X509_new_func();

         BN_set_word_func(bn, RSA_F4);
         RSA_generate_key_ex_func(rsa, 2048, bn, nullptr);
         EVP_PKEY_assign_func(pkey, EVP_PKEY_RSA, rsa);

         X509_set_version_func(x509, 2);
         ASN1_INTEGER_set_func(X509_get_serialNumber_func(x509), 1);
         X509_gmtime_adj_func(X509_getm_notBefore_func(x509), 0); // @todo could/should we give a negative value here to avoid validity problems with notBefore?
         X509_gmtime_adj_func(X509_getm_notAfter_func(x509), certificate_lifetime);
         X509_set_pubkey_func(x509, pkey);

         X509_NAME *name = X509_get_subject_name_func(x509);
         X509_NAME_add_entry_by_txt_func(name, "CN", MBSTRING_ASC, (unsigned char *)component_get_qualified_hostname(), -1, -1, 0);
         X509_set_issuer_name_func(x509, name);

         X509_sign_func(x509, pkey, EVP_sha256_func());

         if (file_based) {
            // we need to be root to be able to write in the key directory
            DPRINTF("=====> switch_user: %d, called_as_root: %d\n", switch_user, called_as_root);
            if (switch_user && !called_as_root) {
               DPRINTF("  --> switching to start user\n");
               sge_switch2start_user();
            }
            // @todo CS-1530 need to lock the directory? Otherwise multiple processes might try to create the certificate at the same time
            DPRINTF("writing key to file %s\n", key_path.c_str());
            FILE *f = fopen(key_path.c_str(), "wb");
            if (f == nullptr) {
               sge_dstring_sprintf(error_dstr, MSG_OPENSSL_CANNOT_OPEN_KEY_FILE_SS, key_path.c_str(), strerror(errno));
               DPRINTF(SFNMAX "\n", sge_dstring_get_string(error_dstr));
               ret = false;
            } else {
               PEM_write_PrivateKey_func(f, pkey, nullptr, nullptr, 0, nullptr, nullptr);
               fclose(f);
               // @todo in theory we have a very short time window here where the file is created but not yet protected
               //       OTOH the key directory is only readable/writable by root or the user himself
               chmod(key_path.c_str(), S_IRUSR | S_IWUSR); // key file should be only readable/writable by owner
            }
            if (switch_user && !called_as_root) {
               DPRINTF("  --> switching to admin user\n");
               sge_switch2admin_user();
            }

            // need to switch to admin user if we are root
            // SGE_ROOT might be on a filesystem where root has no write permissions
            // only write the certificate if we could successfully write the key file
            if (ret) {
               if (switch_user && called_as_root) {
                  DPRINTF("  --> switching to admin user\n");
                  sge_switch2admin_user();
               }
               // @todo error handling!!
               DPRINTF("writing certificate to file %s\n", cert_path.c_str());
               f = fopen(cert_path.c_str(), "wb");
               if (f == nullptr) {
                  sge_dstring_sprintf(error_dstr, MSG_OPENSSL_CANNOT_OPEN_CERT_FILE_SS, cert_path.c_str(), strerror(errno));
                  DPRINTF(SFNMAX "\n", sge_dstring_get_string(error_dstr));
                  ret = false;
               } else {
                  PEM_write_X509_func(f, x509);
                  fclose(f);
                  chmod(cert_path.c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); // cert file should be readable by everyone
               }
               if (switch_user && called_as_root) {
                  DPRINTF("  --> switching to start user\n");
                  sge_switch2start_user();
               }
            }
         }

         // We have the certificate in memory, initialize ssl_ctx from memory.
         file_read_required = false;

         // clear previously occurred but not yet fetched errors
         ERR_clear_error_func();

         if (ret) {
            if (SSL_CTX_use_certificate_func(ssl_ctx, x509) <= 0) {
               sge_dstring_sprintf(error_dstr, MSG_OPENSSL_CANNOT_USE_CERT_X509_S, ERR_reason_error_string_func(ERR_get_error_func()));
               ret = false;
            }
         }
         if (ret) {
            if (SSL_CTX_use_PrivateKey_func(ssl_ctx, pkey) <= 0) {
               sge_dstring_sprintf(error_dstr, MSG_OPENSSL_CANNOT_USE_KEY_PKEY_S, ERR_reason_error_string_func(ERR_get_error_func()));
               ret = false;
            }
         }

         if (ret) {
            // We renew when more than 75 percent of the lifetime have passed.
            renewal_time = sge_get_gmt64() + sge_gmt32_to_gmt64(certificate_lifetime - certificate_lifetime / 4);
         }

         X509_free_func(x509);
         EVP_PKEY_free_func(pkey);
         // rsa is freed with pkey
         BN_free_func(bn);
      }

      // If we have the certificates in files.
      // If we didn't pass them from memory above (after creating a new cert), pass them to the SSL_CTX via paths.
      if (ret && file_based && file_read_required) {
         // clear previously occurred but not yet fetched errors
         ERR_clear_error_func();

         if (ret) {
            // We can read this file, as admin user and as root.
            if (SSL_CTX_use_certificate_chain_file_func(ssl_ctx, cert_path.c_str()) <= 0) {
               sge_dstring_sprintf(error_dstr, MSG_OPENSSL_CANNOT_USE_CERT_FILE_SS, cert_path.c_str(),
                                   ERR_reason_error_string_func(ERR_get_error_func()));
               ret = false;
            }
         }

         if (ret) {
            // We need to be root to read this file!
            if (switch_user && !called_as_root) {
               DPRINTF("  --> switching to start user\n");
               sge_switch2start_user();
            }
            if (SSL_CTX_use_PrivateKey_file_func(ssl_ctx, key_path.c_str(), SSL_FILETYPE_PEM) <= 0) {
               sge_dstring_sprintf(error_dstr, MSG_OPENSSL_CANNOT_USE_KEY_FILE_SS, key_path.c_str(),
                                   ERR_reason_error_string_func(ERR_get_error_func()));
               ret = false;
            }
            if (switch_user && !called_as_root) {
               DPRINTF("  --> switching to admin user\n");
               sge_switch2admin_user();
            }
         }
      }

      DRETURN(ret);
   }

   /**
    * @brief Configures an SSL_CTX for client-side operation with certificate verification.
    *
    * This function sets up an SSL client context for secure connections:
    * - If cert_path is empty: Disables certificate verification (SSL_VERIFY_NONE) (an option but not used)
    * - If cert_path is provided: Enables peer verification (SSL_VERIFY_PEER) and
    *   loads the trusted certificate from the specified location
    *
    * When verification is enabled, the client will:
    * - Abort the handshake if the server's certificate cannot be verified
    * - Use the specified certificate as the trust anchor (typically a self-signed cert)
    *
    * @param error_dstr Output parameter for error messages
    *
    * @return true if client context was successfully configured, false on error
    */
   bool OpenSSL::OpenSSLContext::configure_client_context(dstring *error_dstr) {
      DENTER(TOP_LAYER);
      bool ret = true;

      if (cert_path.empty()) {
         sge_dstring_sprintf(error_dstr, SFNMAX, MSG_OPENSSL_EMPTY_CERT_PATH);
         ret = false;
         // We could instead disable certificate verification by setting verify func SSL_VERIFY_NONE.
         // SSL_CTX_set_verify_func(ssl_ctx, SSL_VERIFY_NONE, nullptr);
      } else {
         // clear previously occurred but not yet fetched errors
         ERR_clear_error_func();

         /*
          * Configure the client to abort the handshake if certificate verification
          * fails
          */
         SSL_CTX_set_verify_func(ssl_ctx, SSL_VERIFY_PEER, nullptr);
         /*
          * In a real application you would probably just use the default system certificate trust store and call:
          *     SSL_CTX_set_default_verify_paths(ctx);
          * In this demo though we are using a self-signed certificate, so the client must trust it directly.
          */
         if (!SSL_CTX_load_verify_locations_func(ssl_ctx, cert_path.c_str(), nullptr)) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_CANNOT_USE_CERT_FILE_SS, cert_path.c_str(),
                                ERR_reason_error_string_func(ERR_get_error_func()));
            ret = false;
         }
      }

      DRETURN(ret);
   }

   /**
    * @brief Destructor for OpenSSLContext. Frees the SSL_CTX if no connections reference it.
    *
    * The destructor performs safe cleanup:
    * - If connection_count is 0: Frees the SSL_CTX immediately
    * - If connection_count > 0: Keeps the SSL_CTX allocated to prevent crashes
    *   (results in a small memory leak, but prevents use-after-free)
    *
    * Contexts with active connections should be marked for deletion using
    * mark_context_for_deletion(), which will defer actual deletion until all
    * connections are closed.
    *
    * @note This defensive approach prevents crashes at the cost of occasional small memory leaks.
    * @see mark_context_for_deletion()
    * @see delete_no_longer_used_contexts()
    */
   OpenSSL::OpenSSLContext::~OpenSSLContext() {
      DENTER(TOP_LAYER);
      // If there are still connections referencing this object, we do not free the ssl_ctx.
      // Better have a small (seldom) leak than a crash.
      if (connection_count == 0) {
         // free the SSL context
         if (ssl_ctx != nullptr) {
            SSL_CTX_free_func(ssl_ctx);
            ssl_ctx = nullptr;
         }
      } else {
         DPRINTF("===> Destructor for an OpenSSLContext which is still referenced %d times\n", connection_count);
      }
      DRETURN_VOID;
   }

   /**
    * @brief Marks an OpenSSLContext for deferred deletion when it's safe to do so.
    *
    * This function handles safe deletion of contexts that may still have active connections:
    * - If connection_count is 0: Deletes the context immediately
    * - If connection_count > 0: Adds the context to a deletion queue for later cleanup
    *
    * Deferred deletion prevents crashes when contexts need to be replaced (e.g., during
    * certificate renewal) while connections are still using them. The actual deletion
    * occurs when delete_no_longer_used_contexts() is called and all connections are closed.
    *
    * @param context Pointer to the OpenSSLContext to be deleted (must not be nullptr)
    *
    * @note This is the preferred way to delete contexts that may have active connections.
    * @see delete_no_longer_used_contexts()
    */
   void OpenSSL::OpenSSLContext::mark_context_for_deletion(OpenSSLContext *context) {
      DENTER(TOP_LAYER);
      if (context->connection_count == 0) {
         delete context;
      } else {
         contexts_to_delete.push_back(context);
      }
      DRETURN_VOID;
   }

   /**
    * @brief Deletes contexts marked for deletion that no longer have active connections.
    *
    * This function iterates through the contexts_to_delete vector and deletes
    * any contexts whose connection_count has reached zero. Contexts with active
    * connections remain in the vector for future cleanup attempts.
    *
    * This function should be called periodically (e.g., during event processing)
    * to clean up contexts that were marked for deletion but couldn't be deleted
    * immediately due to active connections.
    *
    * @note This function is safe to call at any time, even if no contexts are pending deletion.
    * @see mark_context_for_deletion()
    */
   void OpenSSL::OpenSSLContext::delete_no_longer_used_contexts() {
      DENTER(TOP_LAYER);
      // Remove and delete contexts with no active connections.
      // Use erase-remove idiom to safely delete elements while iterating.
      auto it = contexts_to_delete.begin();
      while (it != contexts_to_delete.end()) {
         OpenSSLContext *context = *it;
         if (context->connection_count == 0) {
            DPRINTF("Deleting unused OpenSSLContext\n");
            it = contexts_to_delete.erase(it);
            delete context;
         } else {
            ++it;
         }
      }
      DRETURN_VOID;
   }

      /**
       * @brief Creates an OpenSSLContext for server mode without file-based certificate storage.
       *
       * This is a convenience factory method that creates a server context with certificates
       * stored only in memory (not written to files). This is typically used for temporary
       * client connections like qrsh where persistent certificate storage is not needed.
       *
       * @param error_dstr Output parameter for error messages
       *
       * @return Pointer to newly created OpenSSLContext, or nullptr on error
       *
       * @note Caller is responsible for deleting the returned context.
       * @note Certificates are generated in memory and not persisted to disk.
       * @see create(bool, std::string&, std::string&, dstring*)
       */
      // Not really required but explicitly marks the case that certificate and key are not stored in files
      OpenSSL::OpenSSLContext * OpenSSL::OpenSSLContext::create(dstring *error_dstr) {
         DENTER(TOP_LAYER);
         OpenSSLContext *ret{nullptr};

         std::string cert_path{};
         std::string key_path{};
         ret = create(true, cert_path, key_path, error_dstr);

         DRETURN(ret);
      }

      /**
       * @brief Creates a new OpenSSLContext with the same configuration as an existing one.
       *
       * This factory method creates a new context that uses the same certificate and key paths
       * as the source context. This is useful when refreshing certificates - the new context
       * will check for updated certificates and create new ones if needed.
       *
       * @param source Pointer to the existing OpenSSLContext to copy configuration from
       * @param error_dstr Output parameter for error messages
       *
       * @return Pointer to newly created OpenSSLContext, or nullptr on error
       *
       * @note Only the configuration (paths, server/client mode) is copied, not the SSL_CTX itself.
       * @note Caller is responsible for deleting the returned context.
       * @see create(bool, std::string&, std::string&, dstring*)
       */
      OpenSSL::OpenSSLContext * OpenSSL::OpenSSLContext::create(const OpenSSLContext *source, dstring *error_dstr) {
         DENTER(TOP_LAYER);
         OpenSSLContext *ret{nullptr};

         std::string cert_path{source->cert_path};
         std::string key_path{source->key_path};
         ret = create(source->is_server, cert_path, key_path, error_dstr);

         DRETURN(ret);
      }

      /**
       * @brief Creates and fully configures an OpenSSLContext for server or client operation.
       *
       * This is the main factory method for creating SSL contexts. It:
       * 1. Creates an SSL_CTX using the appropriate method (server or client)
       * 2. Constructs an OpenSSLContext wrapper object
       * 3. Configures the context (generates/loads certificates for server, sets up verification for client)
       *
       * For server contexts:
       * - If paths are empty: Creates in-memory certificates
       * - If paths are provided: Creates/loads certificates from files
       * - Automatically generates new certificates if needed
       *
       * For client contexts:
       * - If cert_path is empty: Disables certificate verification
       * - If cert_path is provided: Enables peer verification with the specified CA cert
       *
       * @param is_server true for server context, false for client context
       * @param cert_path Path to certificate file (may be empty for in-memory certs)
       * @param key_path Path to private key file (may be empty for in-memory keys)
       * @param error_dstr Output parameter for error messages
       *
       * @return Pointer to newly created and configured OpenSSLContext, or nullptr on error
       *
       * @note This is the most flexible create method - others delegate to this one.
       * @note Caller is responsible for deleting the returned context.
       */
      OpenSSL::OpenSSLContext * OpenSSL::OpenSSLContext::create(bool is_server, std::string &cert_path, std::string &key_path, dstring *error_dstr) {
      DENTER(TOP_LAYER);
      OpenSSLContext *ret{nullptr};

      bool ok = true;

      // clear previously occurred but not yet fetched errors
      ERR_clear_error_func();

      // prepare object parameters
      SSL_CTX *ssl_ctx;
      if (ok) {
         const SSL_METHOD *method;
         if (is_server) {
            method = TLS_server_method_func();
         } else {
            method = TLS_client_method_func();
         }
         ssl_ctx = SSL_CTX_new_func(method);
         if (ssl_ctx == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_CANNOT_CREATE_SSL_CONTEXT_S, ERR_reason_error_string_func(ERR_get_error_func()));
            ok = false;
         }
      }

      // create object
      if (ok) {
         ret = new OpenSSLContext(is_server, ssl_ctx, cert_path, key_path);
      }

      // configure the SSL context
      if (ok) {
         if (is_server) {
            ok = ret->configure_server_context(error_dstr);
         } else {
            ok = ret->configure_client_context(error_dstr);
         }
      }

      // if something failed, free the object again and return nullptr
      if (!ok && ret != nullptr) {
         delete ret;
         ret = nullptr;
      }

      DRETURN(ret);
   }

   /**
    * @brief Retrieves the certificate from the SSL context in PEM format.
    *
    * This function extracts the certificate from the SSL_CTX and converts it to
    * PEM-encoded format as a null-terminated string. The certificate can be sent
    * to clients or used for verification purposes.
    *
    * The function:
    * 1. Gets the X.509 certificate from the SSL_CTX
    * 2. Creates a memory BIO
    * 3. Writes the certificate to the BIO in PEM format
    * 4. Reads the PEM data into a newly allocated string
    *
    * @return Newly allocated string containing the PEM-encoded certificate,
    *         or nullptr if no certificate is available or on error
    *
    * @note Caller is responsible for freeing the returned string using sge_free().
    * @note The returned string is null-terminated and suitable for transmission or display.
    */
   const char *OpenSSL::OpenSSLContext::get_cert() {
      DENTER(TOP_LAYER);
      char *ret = nullptr;

      X509 *cert = SSL_CTX_get0_certificate_func(ssl_ctx);
      if (cert != nullptr) {
         BIO *bio = BIO_new_func(BIO_s_mem_func());
         if (bio != nullptr) {
            if (PEM_write_bio_X509_func(bio, cert) > 0) {
               size_t size = BIO_number_written_func(bio);
               ret = sge_malloc(size + 1);
               if (ret != nullptr) {
                  memset(ret, 0, size + 1);
                  BIO_read_func(bio, ret, size);
               }
            }
            BIO_free_func(bio);
         }
      }

      DRETURN(ret);
   }

   /**
    * @brief Creates a new SSL connection object associated with the given context.
    *
    * This factory method creates an OpenSSLConnection wrapper around an SSL object.
    * The connection inherits its configuration (server/client mode, certificates) from
    * the provided context.
    *
    * The function:
    * 1. Creates a new SSL object from the context's SSL_CTX
    * 2. Configures SSL_MODE_AUTO_RETRY (auto-restart interrupted handshakes on blocking sockets)
    * 3. Configures SSL_MODE_ENABLE_PARTIAL_WRITE (allow partial writes)
    * 4. Wraps the SSL object in an OpenSSLConnection
    * 5. Increments the context's connection reference count
    *
    * @param context Pointer to the OpenSSLContext to create the connection from
    * @param error_dstr Output parameter for error messages
    *
    * @return Pointer to newly created OpenSSLConnection, or nullptr on error
    *
    * @note Caller is responsible for deleting the returned connection.
    * @note The connection maintains a reference to the context, preventing premature deletion.
    * @note SSL_MODE_ENABLE_PARTIAL_WRITE allows writing data in chunks for better performance.
    */
   OpenSSL::OpenSSLConnection *OpenSSL::OpenSSLConnection::create(OpenSSLContext *context, dstring *error_dstr) {
      DENTER(TOP_LAYER);
      OpenSSLConnection *ret{nullptr};

      bool ok = true;
      bool is_server = context->get_is_server();

      // clear previously occurred but not yet fetched errors
      ERR_clear_error_func();

      // initialize the SSL connection
      SSL *ssl;
      if (ok) {
         ssl = OpenSSL::SSL_new_func(context->get_SSL_CTX());
         if (ssl == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_CANNOT_CREATE_SSL_S, OpenSSL::ERR_reason_error_string_func(OpenSSL::ERR_get_error_func()));
            ok = false;
         }
      }
      if (ok) {
         // Make sure that protocol handshakes are automatically restarted when interrupted.
         // This does *not* mean that they are *always* restarted. On a non-blocking file handle we still see
         // the error codes SSL_ERROR_WANT_*
         // and have to restart the operations ourselves.
         // Macro: SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
         SSL_ctrl_func(ssl,SSL_CTRL_MODE, SSL_MODE_AUTO_RETRY,nullptr);
      }
      if (ok) {
         // Allow partial write operations:
         // SSL_MODE_ENABLE_PARTIAL_WRITE
         //   Allow SSL_write_ex(...,  n,  &r)  to  return with 0 < r < n (i.e., report success when just a single record has been
         //   written). This works in a similar way for SSL_write(). When not set (the default), SSL_write_ex() or SSL_write() will
         //   only report success once the complete chunk was written. Once SSL_write_ex() or  SSL_write() returns successful, r
         //   bytes have been written and the next call to SSL_write_ex() or SSL_write() must only send the n-r bytes left,
         //   imitating the behaviour of write().
         // Macro: SSL_set_mode(ssl_ctx, SSL_MODE_ENABLE_PARTIAL_WRITE);
         SSL_ctrl_func(ssl, SSL_CTRL_MODE, SSL_MODE_ENABLE_PARTIAL_WRITE, nullptr);
      }

      if (ok) {
         ret = new OpenSSLConnection(context, is_server, ssl);
      } else {
         SSL_free_func(ssl);
         ssl = nullptr;
      }

      DRETURN(ret);
   }

   /**
    * @brief Waits for the underlying socket to become ready for the specified operation.
    *
    * This function uses select() to wait for the socket to become ready for reading
    * or writing, depending on the reason parameter. This is necessary when SSL operations
    * return SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE on non-blocking sockets.
    *
    * The function handles different SSL error reasons:
    * - SSL_ERROR_WANT_ACCEPT: Waits for both read and write readiness
    * - SSL_ERROR_WANT_CONNECT: Waits for both read and write readiness
    * - SSL_ERROR_WANT_READ: Waits for read readiness
    * - SSL_ERROR_WANT_WRITE: Waits for write readiness
    *
    * @param reason The SSL error code indicating what to wait for (SSL_ERROR_WANT_*)
    * @param error_dstr Output parameter for error messages
    *
    * @return true if socket became ready or timeout occurred, false on select() error
    *
    * @note Timeout is currently hardcoded to 1 second.
    * @note Timeout is not treated as an error (returns true).
    * @note EWOULDBLOCK, EAGAIN, and EINTR from select() are not treated as errors.
    * @todo CS-1559: Consider moving this functionality into commlib for better integration.
    *                Which is tricky, e.g., when waiting for accept or connect to continue, we may not
    *               repeat the TCP accept() or connect() operation, just take up the interrupted connection again.
    */
   bool OpenSSL::OpenSSLConnection::wait_for_socket_ready(int reason, dstring *error_dstr) {
      // wait until the socket is ready for read or write
      DENTER(TOP_LAYER);

      DPRINTF("waiting for socket %d to become ready for %s\n", fd, reason ? "read" : "write");

      bool ret = true;
      if (ret) {
         fd_set fds;
         FD_ZERO(&fds);
         FD_SET(fd, &fds);
         fd_set *read_fds = nullptr;
         fd_set *write_fds = nullptr;
         switch (reason) {
            case SSL_ERROR_WANT_ACCEPT:
               read_fds = &fds;
               write_fds = &fds;
               break;
            case SSL_ERROR_WANT_CONNECT:
               read_fds = &fds;
               write_fds = &fds;
               break;
            case SSL_ERROR_WANT_READ:
               read_fds = &fds;
               break;
            case SSL_ERROR_WANT_WRITE:
               write_fds = &fds;
               break;
            default:
               ret = false;
               break;
         }
         if (ret) {
            // @todo CS-1579 make timeout configurable
            struct timeval tv;
            tv.tv_sec = 1;
            tv.tv_usec = 0;
            int select_ret = select(fd + 1, read_fds, write_fds, nullptr, &tv);
            if (select_ret < 0) {
               if (errno != EWOULDBLOCK && errno != EAGAIN && errno != EINTR) {
                  sge_dstring_sprintf(error_dstr, MSG_SSL_SELECT_FAILED_DS, errno, strerror(errno));
                  ret = false;
               }
            } else if (select_ret == 0) {
               // select timeout is not an error
               //sge_dstring_sprintf(error_dstr, MSG_SSL_SELECT_TIMEOUT);
               //ret = false;
            }
         }
      }

      if (!ret) {
         DPRINTF("  --> error: %s\n", sge_dstring_get_string(error_dstr));
      }

      DRETURN(ret);
   }

   /**
    * @brief Destructor for OpenSSLConnection. Shuts down SSL connection and frees resources.
    *
    * The destructor performs proper cleanup:
    * 1. Performs SSL shutdown handshake if ssl is not nullptr
    * 2. Frees the SSL object
    * 3. Decrements the associated context's connection reference count
    *
    * Decrementing the context's connection count may trigger deferred deletion
    * of the context if it was marked for deletion and this was the last connection.
    *
    * @note SSL_shutdown may send a close_notify alert to the peer.
    * @note The underlying socket file descriptor is not closed by this destructor.
    * @see OpenSSLContext::mark_context_for_deletion()
    */

   OpenSSL::OpenSSLConnection::~OpenSSLConnection() {
      DENTER(TOP_LAYER);
      if (ssl != nullptr) {
         SSL_shutdown_func(ssl);
         SSL_free_func(ssl);
      }
      if (context != nullptr) {
         context->dec_connection_count();
      }
      DRETURN_VOID;
   }

   /**
    * @brief Associates the SSL connection with a file descriptor (socket).
    *
    * This function binds the SSL connection to an already-established socket file descriptor.
    * After calling this function, SSL operations (read, write, accept, connect) will use
    * this socket for network I/O.
    *
    * @param new_fd The socket file descriptor to associate with this connection
    * @param error_dstr Output parameter for error messages
    *
    * @return true if the fd was successfully set, false on error
    *
    * @note This must be called before performing any SSL handshake or I/O operations.
    * @note The socket should already be connected (for client) or accepted (for server).
    * @note The SSL connection does not take ownership of the file descriptor.
    */
   bool OpenSSL::OpenSSLConnection::set_fd(int new_fd, dstring *error_dstr) {
      DENTER(TOP_LAYER);
      // clear previously occurred but not yet fetched errors
      ERR_clear_error_func();

      bool ret = ssl != nullptr;
      if (ret) {
         fd = new_fd;
      }
      if (ret) {
         if (!SSL_set_fd_func(ssl, fd)) {
            sge_dstring_sprintf(error_dstr, MSG_CANNOT_SET_FD_S, ERR_reason_error_string_func(ERR_get_error_func()));
            ret = false;
         }
      }
      DRETURN(ret);
   }

   /**
    * @brief Reads application data from the SSL connection.
    *
    * This function reads decrypted application data from the SSL connection into
    * the provided buffer. It handles SSL protocol operations transparently.
    *
    * Return value semantics:
    * - Positive value: Number of bytes successfully read
    * - 0: No application data was read (maybe due to SSL protocol data being processed)
    * - -1: Error occurred (error_dstr is populated)
    *
    * When SSL_read returns SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE:
    * - The socket became ready due to SSL protocol data (not application data)
    * - The function returns 0 (no application data available yet)
    * - Caller should try reading again when the socket becomes ready
    *
    * @param buffer Buffer to store read data
    * @param max_len Maximum number of bytes to read
    * @param error_dstr Output parameter for error messages
    *
    * @return Number of bytes read (> 0), 0 if no data available, or -1 on error
    *
    * @note On non-blocking sockets, returning 0 is normal and not an error.
    * @note SSL protocol data is handled transparently (does not appear in the buffer).
    */
   int OpenSSL::OpenSSLConnection::read(char *buffer, size_t max_len, dstring *error_dstr) {
      DENTER(TOP_LAYER);

      DPRINTF("OpenSSLConnection::read()\n");
      int ret;

      // clear previously occurred but not yet fetched errors
      ERR_clear_error_func();

      int read_ret = SSL_read_func(ssl, buffer, static_cast<int>(max_len));
      if (read_ret > 0) {
         // we actually read some data without errors
         ret = read_ret;
      } else {
         int err = SSL_get_error_func(ssl, read_ret);
         if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            DPRINTF("  --> got %s\n", err == SSL_ERROR_WANT_READ ? "SSL_ERROR_WANT_READ" : "SSL_ERROR_WANT_WRITE");
            // We return 0, meaning we didn't read any application data.
            // The socked became ready for reading due to SSL protocol data,
            // this has been parsed, either there will be more protocol data (when the socked becomes ready to
            // read again in commlib, or there is no more data.
            // The fact that SSL_read() returns SSL_ERROR_WANT_READ is somewhat misleading in this case.
            ret = 0;
         } else {
            sge_dstring_sprintf(error_dstr, MSG_CANNOT_READ_DS, err,
                                ERR_reason_error_string_func(ERR_get_error_func()));
            ret = -1;
         }
      }

      DRETURN(ret);
   }

   /**
    * @brief Writes application data to the SSL connection.
    *
    * This function encrypts and writes application data to the SSL connection.
    * With SSL_MODE_ENABLE_PARTIAL_WRITE enabled, it may write fewer bytes than requested.
    *
    * Return value semantics:
    * - Positive value: Number of bytes successfully written
    * - 0: No data was written (operation needs to be retried)
    * - -1: Error occurred (error_dstr is populated)
    *
    * When SSL_write returns SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE:
    * - Sets the repeat_write flag to true
    * - Returns 0 to indicate retry is needed
    * - Caller must call write() again with the same parameters when socket is ready
    *
    * @param buffer Buffer containing data to write
    * @param len Number of bytes to write
    * @param error_dstr Output parameter for error messages
    *
    * @return Number of bytes written (> 0), 0 if retry needed, or -1 on error
    *
    * @note With partial writes enabled, may write less than len bytes (but never 0).
    * @note When repeat_write is true, the same write must be retried (SSL requirement).
    * @note The repeat_write flag can be checked via needs_repeat_write().
    */
   int OpenSSL::OpenSSLConnection::write(char *buffer, size_t len, dstring *error_dstr) {
      DENTER(TOP_LAYER);

      int ret;
      DPRINTF("OpenSSLConnection::write()\n");

      // clear previously occurred but not yet fetched errors
      ERR_clear_error_func();

      // clear repeat_write - we expect the write operation to finish
      repeat_write = false;
      int write_ret = SSL_write_func(ssl, buffer, static_cast<int>(len));
      if (write_ret > 0) {
         // we actually wrote some data without errors
         ret = write_ret;
      } else {
         int err = SSL_get_error_func(ssl, write_ret);
         if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            DPRINTF("  --> got %s\n", err == SSL_ERROR_WANT_READ ? "SSL_ERROR_WANT_READ" : "SSL_ERROR_WANT_WRITE");
            // We need to repeat the write() call, even if our file descriptor becomes ready to read.
            repeat_write = true;
            ret = 0;
         } else {
            sge_dstring_sprintf(error_dstr, MSG_CANNOT_WRITE_DS, err,
                                ERR_reason_error_string_func(ERR_get_error_func()));
            ret = -1;
         }
      }

      // if we get here, an error occurred
      DRETURN(ret);
   }

   /**
    * @brief Performs SSL handshake for server-side connection acceptance.
    *
    * This function performs the SSL/TLS handshake from the server's perspective.
    * It repeatedly calls SSL_accept() until the handshake completes or an error occurs.
    *
    * The function handles SSL_ERROR_WANT_* conditions by:
    * 1. Waiting for the socket to become ready (using select)
    * 2. Retrying the SSL_accept() operation
    * 3. Timing out after SGE_OPENSSL_RETRY_TIMEOUT_SERVER (1 second)
    *
    * This function must be called after:
    * - TCP accept() has completed
    * - set_fd() has been called to associate the socket
    *
    * @param error_dstr Output parameter for error messages
    *
    * @return true if handshake completed successfully, false on error or timeout
    *
    * @note Can only be called on server-side connections (is_server must be true).
    * @note May block for up to 1 second waiting for handshake completion.
    * @note On non-blocking sockets, this function handles retries internally.
    * @todo CS-1679 Make timeout configurable.
    */
#define SGE_OPENSSL_RETRY_TIMEOUT_SERVER 1 * 1000000 // 1 second
#define SGE_OPENSSL_RETRY_TIMEOUT_CLIENT 10 * 1000000 // 10 seconds

   bool OpenSSL::OpenSSLConnection::accept(dstring *error_dstr) {
      DENTER(TOP_LAYER);

      bool ret = ssl != nullptr;

      if (ret) {
         if (!is_server) {
            sge_dstring_copy_string(error_dstr, MSG_SSL_ACCEPT_CALLED_ON_CLIENT);
            ret = false;
         }
      }
      if (ret) {
         // clear previously occurred but not yet fetched errors
         ERR_clear_error_func();

         bool done = false;
         u_long64 timeout = sge_get_gmt64() + SGE_OPENSSL_RETRY_TIMEOUT_SERVER;
         int repetitions = 0;
         do {
            // expect one call to be enough
            done = true;
            int accept_ret = SSL_accept_func(ssl);
            if (accept_ret <= 0) {
               int err = SSL_get_error_func(ssl, accept_ret);
               if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_ACCEPT) {
                  DPRINTF("  --> got connect SSL_ERROR_WANT_* %d\n", err);
                  if (wait_for_socket_ready(err, error_dstr)) {
                     if (sge_get_gmt64() >= timeout) {
                        sge_dstring_sprintf(error_dstr, MSG_OPENSSL_TIMEOUT_IN_ACCEPT_II,
                                            SGE_OPENSSL_RETRY_TIMEOUT_SERVER / 1000000, repetitions);
                        DPRINTF("  --> %s\n", sge_dstring_get_string(error_dstr));
                        ret = false;
                     } else {
                        done = false; // try again
                        DPRINTF("  --> repeat accept\n");
                     }
                  }
               } else {
                  sge_dstring_sprintf(error_dstr, MSG_CANNOT_ACCEPT_DS, err,
                                      ERR_reason_error_string_func(ERR_get_error_func()));
                  DPRINTF("  --> got error: %d: %s\n", err, sge_dstring_get_string(error_dstr));
                  ret = false;
               }
            }
            repetitions++;
         } while (!done);
      }

      DRETURN(ret);
   }

   /**
    * @brief Configures Server Name Indication (SNI) for client-side connection.
    *
    * This function sets the server hostname for SNI extension in the TLS ClientHello.
    * SNI allows the client to indicate which hostname it's attempting to connect to,
    * enabling the server to present the appropriate certificate.
    *
    * The function:
    * 1. Sets the TLS extension hostname (for SNI)
    * 2. Configures hostname verification (for certificate validation)
    *
    * This function must be called before connect() to take effect.
    *
    * @param server_name The hostname of the server to connect to (for SNI and verification)
    * @param error_dstr Output parameter for error messages
    *
    * @return true if SNI was successfully configured, false on error
    *
    * @note Can only be called on client-side connections (is_server must be false).
    * @note Must be called before connect() to be included in the handshake.
    * @note The hostname is used both for SNI and for certificate hostname verification.
    */
   bool OpenSSL::OpenSSLConnection::set_server_name_for_sni(const char *server_name, dstring *error_dstr) {
      DENTER(TOP_LAYER);
      bool ret = ssl != nullptr;

      // we do this only on the client side
      if (ret && !is_server) { // @todo: ERROR when is_server is true
         // clear previously occurred but not yet fetched errors
         ERR_clear_error_func();

         // Set hostname for SNI
         // Macro: SSL_set_tlsext_host_name(s, name)
         SSL_ctrl_func(ssl, SSL_CTRL_SET_TLSEXT_HOSTNAME, TLSEXT_NAMETYPE_host_name, (void *)server_name);
         /* Configure server hostname check */
         if (!SSL_set1_host_func(ssl, server_name)) {
            sge_dstring_sprintf(error_dstr, MSG_CANNOT_SET_SERVERNAME_S, ERR_reason_error_string_func(ERR_get_error_func()));
            ret = false;
         }
      }

      DRETURN(ret);
   }

   /**
    * @brief Performs SSL handshake for client-side connection establishment.
    *
    * This function performs the SSL/TLS handshake from the client's perspective.
    * It repeatedly calls SSL_connect() until the handshake completes or an error occurs.
    *
    * The function handles SSL_ERROR_WANT_* conditions by:
    * 1. Waiting for the socket to become ready (using select)
    * 2. Retrying the SSL_connect() operation
    * 3. Timing out after SGE_OPENSSL_RETRY_TIMEOUT_CLIENT (10 seconds)
    *
    * This function must be called after:
    * - TCP connect() has completed
    * - set_fd() has been called to associate the socket
    * - (Optional) set_server_name_for_sni() has been called for SNI support
    *
    * @param error_dstr Output parameter for error messages
    *
    * @return true if handshake completed successfully, false on error or timeout
    *
    * @note Can only be called on client-side connections (is_server must be false).
    * @note May block for up to 10 seconds waiting for handshake completion.
    * @note On non-blocking sockets, this function handles retries internally.
    * @note Client timeout (10s) is longer than server timeout (1s) to accommodate network delays.
    * @todo CS-1679 Make timeout configurable.
    */
   bool OpenSSL::OpenSSLConnection::connect(dstring *error_dstr) {
      DENTER(TOP_LAYER);

      bool ret = ssl != nullptr;

      // we do this only on the client side
      if (ret) {
         if (is_server) {
            sge_dstring_copy_string(error_dstr, MSG_SSL_CONNECT_CALLED_ON_SERVER);
            ret = false;
         }
      }

      if (ret) {
         // clear previously occurred but not yet fetched errors
         ERR_clear_error_func();

         bool done = false;
         u_long64 timeout = sge_get_gmt64() + SGE_OPENSSL_RETRY_TIMEOUT_CLIENT;
         int repetitions = 0;
         do {
            // expect one call to be enough
            done = true;

            int connect_ret = SSL_connect_func(ssl);
            if (connect_ret <= 0) {
               int err = SSL_get_error_func(ssl, connect_ret);
               if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_CONNECT) {
                  DPRINTF("  --> got connect SSL_ERROR_WANT_* %d\n", err);
                  if (wait_for_socket_ready(err, error_dstr)) {
                     if (sge_get_gmt64() >= timeout) {
                        sge_dstring_sprintf(error_dstr, MSG_OPENSSL_TIMEOUT_IN_CONNECT_II,
                                            SGE_OPENSSL_RETRY_TIMEOUT_CLIENT, repetitions);
                        DPRINTF("  --> %s\n", sge_dstring_get_string(error_dstr));
                        ret = false;
                     } else {
                        done = false; // try again
                        DPRINTF("  --> repeat connect\n");
                     }
                  }
               } else {
                  sge_dstring_sprintf(error_dstr, MSG_CANNOT_CONNECT_DS, err,
                                      ERR_reason_error_string_func(ERR_get_error_func()));
                  DPRINTF("  --> got error: %d: %s\n", err, sge_dstring_get_string(error_dstr));
                  ret = false;
               }
            }
            repetitions++;
         } while (!done);
      }

      DRETURN(ret);
   }
} // namespace ocs::uti

#endif
