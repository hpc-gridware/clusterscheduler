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

#include "ocs_OpenSSL.h"

#include <sge_bootstrap_env.h>
#include <sge_string.h>

#if defined(OCS_WITH_OPENSSL)

namespace ocs::uti {
   // static members
   void *OpenSSL::lib_handle = nullptr;

   TLS_server_method_func_t OpenSSL::TLS_server_method_func = nullptr;
   TLS_client_method_func_t OpenSSL::TLS_client_method_func = nullptr;
   SSL_CTX_new_func_t OpenSSL::SSL_CTX_new_func = nullptr;
   ERR_get_error_func_t OpenSSL::ERR_get_error_func = nullptr;
   ERR_reason_error_string_func_t OpenSSL::ERR_reason_error_string_func = nullptr;
   SSL_CTX_use_certificate_chain_file_func_t OpenSSL::SSL_CTX_use_certificate_chain_file_func = nullptr;
   SSL_CTX_use_PrivateKey_file_func_t OpenSSL::SSL_CTX_use_PrivateKey_file_func = nullptr;
   SSL_CTX_load_verify_locations_func_t OpenSSL::SSL_CTX_load_verify_locations_func = nullptr;
   SSL_CTX_set_verify_func_t OpenSSL::SSL_CTX_set_verify_func = nullptr;
   SSL_new_func_t OpenSSL::SSL_new_func = nullptr;
   SSL_set_fd_func_t OpenSSL::SSL_set_fd_func = nullptr;
   SSL_accept_func_t OpenSSL::SSL_accept_func = nullptr;
   SSL_read_func_t OpenSSL::SSL_read_func = nullptr;
   SSL_write_func_t OpenSSL::SSL_write_func = nullptr;
   SSL_shutdown_func_t OpenSSL::SSL_shutdown_func = nullptr;
   SSL_free_func_t OpenSSL::SSL_free_func = nullptr;
   SSL_CTX_free_func_t OpenSSL::SSL_CTX_free_func = nullptr;
   SSL_set1_host_func_t OpenSSL::SSL_set1_host_func = nullptr;
   SSL_connect_func_t OpenSSL::SSL_connect_func = nullptr;
   SSL_ctrl_func_t OpenSSL::SSL_ctrl_func = nullptr;
   OPENSSL_init_ssl_func_t OpenSSL::OPENSSL_init_ssl_func = nullptr;

   EVP_PKEY_new_func_t OpenSSL::EVP_PKEY_new_func = nullptr;
   EVP_PKEY_free_func_t OpenSSL::EVP_PKEY_free_func = nullptr;
   EVP_PKEY_assign_func_t OpenSSL::EVP_PKEY_assign_func = nullptr;
   RSA_new_func_t OpenSSL::RSA_new_func = nullptr;
   RSA_free_func_t OpenSSL::RSA_free_func = nullptr;
   RSA_generate_key_ex_func_t OpenSSL::RSA_generate_key_ex_func = nullptr;
   BN_new_func_t OpenSSL::BN_new_func = nullptr;
   BN_free_func_t OpenSSL::BN_free_func = nullptr;
   BN_set_word_func_t OpenSSL::BN_set_word_func = nullptr;
   X509_new_func_t OpenSSL::X509_new_func = nullptr;
   X509_free_func_t OpenSSL::X509_free_func = nullptr;
   X509_set_version_func_t OpenSSL::X509_set_version_func = nullptr;
   ASN1_INTEGER_set_func_t OpenSSL::ASN1_INTEGER_set_func = nullptr;
   X509_gmtime_adj_func_t OpenSSL::X509_gmtime_adj_func = nullptr;
   X509_set_pubkey_func_t OpenSSL::X509_set_pubkey_func = nullptr;
   X509_get_serialNumber_func_t OpenSSL::X509_get_serialNumber_func = nullptr;
   X509_NAME_add_entry_by_txt_func_t OpenSSL::X509_NAME_add_entry_by_txt_func = nullptr;
   X509_set_issuer_name_func_t OpenSSL::X509_set_issuer_name_func = nullptr;
   X509_sign_func_t OpenSSL::X509_sign_func = nullptr;
   PEM_write_PrivateKey_func_t OpenSSL::PEM_write_PrivateKey_func = nullptr;
   PEM_write_X509_func_t OpenSSL::PEM_write_X509_func = nullptr;
   EVP_sha256_func_t OpenSSL::EVP_sha256_func = nullptr;
   X509_get_subject_name_func_t OpenSSL::X509_get_subject_name_func = nullptr;
   X509_getm_notBefore_func_t OpenSSL::X509_getm_notBefore_func = nullptr;
   X509_getm_notAfter_func_t OpenSSL::X509_getm_notAfter_func = nullptr;
   SSL_get_error_func_t OpenSSL::SSL_get_error_func = nullptr;
   ERR_clear_error_func_t OpenSSL::ERR_clear_error_func = nullptr;

   // static methods
   bool OpenSSL::initialize(dstring *error_dstr) {
      DENTER(TOP_LAYER);

      bool ret = true;

      // initialize only once
      if (ret && lib_handle != nullptr) {
         sge_dstring_sprintf(error_dstr, SFNMAX, MSG_OPENSSL_ALREADY_INITIALIZED);
         ret = false;
      }

      // Load the shared library and the required functions
      if (ret) {
         const char *libsystemd = "libssl.so.3";
         lib_handle = dlopen(libsystemd, RTLD_LAZY);
         if (lib_handle == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_OPEN_LIB_SS, libsystemd, dlerror());
            ret = false;
         }
      }

      // load the functions
      const char *func;
      if (ret) {
         func = "TLS_server_method";
         //func = "TLS_method";
         TLS_server_method_func = reinterpret_cast<TLS_server_method_func_t>(dlsym(lib_handle, func));
         if (TLS_server_method_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "TLS_client_method";
         //func = "TLS_method";
         TLS_client_method_func = reinterpret_cast<TLS_client_method_func_t>(dlsym(lib_handle, func));
         if (TLS_client_method_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_CTX_new";
         SSL_CTX_new_func = reinterpret_cast<SSL_CTX_new_func_t>(dlsym(lib_handle, func));
         if (SSL_CTX_new_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "ERR_get_error";
         ERR_get_error_func = reinterpret_cast<ERR_get_error_func_t>(dlsym(lib_handle, func));
         if (ERR_get_error_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "ERR_reason_error_string";
         ERR_reason_error_string_func = reinterpret_cast<ERR_reason_error_string_func_t>(dlsym(lib_handle, func));
         if (ERR_reason_error_string_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_CTX_use_certificate_chain_file";
         SSL_CTX_use_certificate_chain_file_func = reinterpret_cast<SSL_CTX_use_certificate_chain_file_func_t>(dlsym(lib_handle, func));
         if (SSL_CTX_use_certificate_chain_file_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_CTX_use_PrivateKey_file";
         SSL_CTX_use_PrivateKey_file_func = reinterpret_cast<SSL_CTX_use_PrivateKey_file_func_t>(dlsym(lib_handle, func));
         if (SSL_CTX_use_PrivateKey_file_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_CTX_load_verify_locations";
         SSL_CTX_load_verify_locations_func = reinterpret_cast<SSL_CTX_load_verify_locations_func_t>(dlsym(lib_handle, func));
         if (SSL_CTX_load_verify_locations_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_CTX_set_verify";
         SSL_CTX_set_verify_func = reinterpret_cast<SSL_CTX_set_verify_func_t>(dlsym(lib_handle, func));
         if (SSL_CTX_set_verify_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_new";
         SSL_new_func = reinterpret_cast<SSL_new_func_t>(dlsym(lib_handle, func));
         if (SSL_new_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_set_fd";
         SSL_set_fd_func = reinterpret_cast<SSL_set_fd_func_t>(dlsym(lib_handle, func));
         if (SSL_set_fd_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_accept";
         SSL_accept_func = reinterpret_cast<SSL_accept_func_t>(dlsym(lib_handle, func));
         if (SSL_accept_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_read";
         SSL_read_func = reinterpret_cast<SSL_read_func_t>(dlsym(lib_handle, func));
         if (SSL_read_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_write";
         SSL_write_func = reinterpret_cast<SSL_write_func_t>(dlsym(lib_handle, func));
         if (SSL_write_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_shutdown";
         SSL_shutdown_func = reinterpret_cast<SSL_shutdown_func_t>(dlsym(lib_handle, func));
         if (SSL_shutdown_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_free";
         SSL_free_func = reinterpret_cast<SSL_free_func_t>(dlsym(lib_handle, func));
         if (SSL_free_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_CTX_free";
         SSL_CTX_free_func = reinterpret_cast<SSL_CTX_free_func_t>(dlsym(lib_handle, func));
         if (SSL_CTX_free_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_set1_host";
         SSL_set1_host_func = reinterpret_cast<SSL_set1_host_func_t>(dlsym(lib_handle, func));
         if (SSL_set1_host_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_connect";
         SSL_connect_func = reinterpret_cast<SSL_connect_func_t>(dlsym(lib_handle, func));
         if (SSL_connect_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_ctrl";
         SSL_ctrl_func = reinterpret_cast<SSL_ctrl_func_t>(dlsym(lib_handle, func));
         if (SSL_ctrl_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "OPENSSL_init_ssl";
         OPENSSL_init_ssl_func = reinterpret_cast<OPENSSL_init_ssl_func_t>(dlsym(lib_handle, func));
         if (OPENSSL_init_ssl_func == nullptr) {
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
      if (ret) {
         func = "EVP_PKEY_new";
         EVP_PKEY_new_func = reinterpret_cast<EVP_PKEY_new_func_t>(dlsym(lib_handle, func));
         if (EVP_PKEY_new_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "EVP_PKEY_free";
         EVP_PKEY_free_func = reinterpret_cast<EVP_PKEY_free_func_t>(dlsym(lib_handle, func));
         if (EVP_PKEY_free_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "EVP_PKEY_assign";
         EVP_PKEY_assign_func = reinterpret_cast<EVP_PKEY_assign_func_t>(dlsym(lib_handle, func));
         if (EVP_PKEY_assign_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "RSA_new";
         RSA_new_func = reinterpret_cast<RSA_new_func_t>(dlsym(lib_handle, func));
         if (RSA_new_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "RSA_free";
         RSA_free_func = reinterpret_cast<RSA_free_func_t>(dlsym(lib_handle, func));
         if (RSA_free_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "RSA_generate_key_ex";
         RSA_generate_key_ex_func = reinterpret_cast<RSA_generate_key_ex_func_t>(dlsym(lib_handle, func));
         if (RSA_generate_key_ex_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "BN_new";
         BN_new_func = reinterpret_cast<BN_new_func_t>(dlsym(lib_handle, func));
         if (BN_new_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "BN_free";
         BN_free_func = reinterpret_cast<BN_free_func_t>(dlsym(lib_handle, func));
         if (BN_free_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "BN_set_word";
         BN_set_word_func = reinterpret_cast<BN_set_word_func_t>(dlsym(lib_handle, func));
         if (BN_set_word_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "X509_new";
         X509_new_func = reinterpret_cast<X509_new_func_t>(dlsym(lib_handle, func));
         if (X509_new_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "X509_free";
         X509_free_func = reinterpret_cast<X509_free_func_t>(dlsym(lib_handle, func));
         if (X509_free_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "X509_set_version";
         X509_set_version_func = reinterpret_cast<X509_set_version_func_t>(dlsym(lib_handle, func));
         if (X509_set_version_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "ASN1_INTEGER_set";
         ASN1_INTEGER_set_func = reinterpret_cast<ASN1_INTEGER_set_func_t>(dlsym(lib_handle, func));
         if (ASN1_INTEGER_set_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "X509_gmtime_adj";
         X509_gmtime_adj_func = reinterpret_cast<X509_gmtime_adj_func_t>(dlsym(lib_handle, func));
         if (X509_gmtime_adj_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "X509_set_pubkey";
         X509_set_pubkey_func = reinterpret_cast<X509_set_pubkey_func_t>(dlsym(lib_handle, func));
         if (X509_set_pubkey_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "X509_get_serialNumber";
         X509_get_serialNumber_func = reinterpret_cast<X509_get_serialNumber_func_t>(dlsym(lib_handle, func));
         if (X509_get_serialNumber_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "X509_NAME_add_entry_by_txt";
         X509_NAME_add_entry_by_txt_func = reinterpret_cast<X509_NAME_add_entry_by_txt_func_t>(dlsym(lib_handle, func));
         if (X509_NAME_add_entry_by_txt_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "X509_set_issuer_name";
         X509_set_issuer_name_func = reinterpret_cast<X509_set_issuer_name_func_t>(dlsym(lib_handle, func));
         if (X509_set_issuer_name_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "X509_sign";
         X509_sign_func = reinterpret_cast<X509_sign_func_t>(dlsym(lib_handle, func));
         if (X509_sign_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "PEM_write_PrivateKey";
         PEM_write_PrivateKey_func = reinterpret_cast<PEM_write_PrivateKey_func_t>(dlsym(lib_handle, func));
         if (PEM_write_PrivateKey_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "PEM_write_X509";
         PEM_write_X509_func = reinterpret_cast<PEM_write_X509_func_t>(dlsym(lib_handle, func));
         if (PEM_write_X509_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "EVP_sha256";
         EVP_sha256_func = reinterpret_cast<EVP_sha256_func_t>(dlsym(lib_handle, func));
         if (EVP_sha256_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "X509_get_subject_name";
         X509_get_subject_name_func = reinterpret_cast<X509_get_subject_name_func_t>(dlsym(lib_handle, func));
         if (X509_get_subject_name_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "X509_getm_notBefore";
         X509_getm_notBefore_func = reinterpret_cast<X509_getm_notBefore_func_t>(dlsym(lib_handle, func));
         if (X509_getm_notBefore_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "X509_getm_notAfter";
         X509_getm_notAfter_func = reinterpret_cast<X509_getm_notAfter_func_t>(dlsym(lib_handle, func));
         if (X509_getm_notAfter_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "SSL_get_error";
         SSL_get_error_func = reinterpret_cast<SSL_get_error_func_t>(dlsym(lib_handle, func));
         if (SSL_get_error_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "ERR_clear_error";
         ERR_clear_error_func = reinterpret_cast<ERR_clear_error_func_t>(dlsym(lib_handle, func));
         if (ERR_clear_error_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_OPENSSL_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }

      // if the initialization failed, free everything again
      if (!ret) {
         if (lib_handle != nullptr) {
            dlclose(lib_handle);
            lib_handle = nullptr;
         }
      }

      DRETURN(ret);
   }

   void OpenSSL::cleanup() {
      // close the library
      if (lib_handle != nullptr) {
         dlclose(lib_handle);
         lib_handle = nullptr;
      }
   }

   bool OpenSSL::build_cert_path(std::string &cert_path, const char *home_dir, const char *hostname) {
      bool ret = true;
      // need info
      // -> daemon or user certificate?
      //    -> daemon: $SGE_ROOT/$SGE_CELL/common/certs/hostname.pem
      //    -> user: $HOME/.ocs/certs/hostname.pem
      if (hostname == nullptr) {
         // @todo use error_dstr
         ret = false;
      } else {
         if (home_dir == nullptr) {
            cert_path = std::string(bootstrap_get_sge_root()) + "/" + std::string(bootstrap_get_sge_cell()) + std::string("/common/certs/") + std::string(hostname) + std::string(".pem");
         } else {
            cert_path = std::string(home_dir) + std::string("/.ocs/certs/") + std::string(hostname) + std::string(".pem");
         }
      }
      return ret;
   }
   bool OpenSSL::build_key_path(std::string &key_path, const char *home_dir, const char *hostname) {
      bool ret = true;
      // -> daemon or user key?
      //    -> daemon: /var/lib/ocs/private/hostname.pem
      //    -> user: $HOME/.ocs/private/hostname.pem
      // @todo need to lock the directory? Otherwise multiple processes might try to create the certificate at the same time
      if (hostname == nullptr) {
         // @todo use error_dstr
         ret = false;
      } else {
         if (home_dir == nullptr) {
            key_path = std::string("/var/lib/ocs/private/") + std::string(hostname) + std::string(".pem");
         } else {
            key_path = std::string(home_dir) + std::string("/.ocs/private/") + std::string(hostname) + std::string(".pem");
         }
      }
      return ret;
   }

   bool OpenSSL::OpenSSLContext::verify_create_directories(bool switch_user, dstring *error_dstr) {
      bool ret = true;

      if (ret) {
         std::filesystem::path cert_dir = cert_path.parent_path();
         if (!std::filesystem::exists(cert_dir)) {
            // need to switch to admin user if we are root
            // SGE_ROOT might be on a filesystem where root has no write permissions
            if (switch_user) {
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
               }
            }
            if (switch_user) {
               sge_switch2start_user();
            }
         }
      }
      if (ret) {
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
               }
            }
         }
      }

      return ret;
   }

   bool OpenSSL::OpenSSLContext::certificate_recreate_required() {
      return false;
   }

   bool OpenSSL::OpenSSLContext::verify_create_certificate_and_key(dstring *error_dstr) {
      bool ret = true;

      // when we are starting as root and creating a daemon certificate
      // in $SGE_ROOT/$SGE_CELL/common/certs and /var/lib/ocs/private
      // we need to be root to write the key
      // but as root we might not be able to create directories or files in $SGE_ROOT
      // OTOH when running qrsh as root, we write the certificate and key in $HOME/.ocs
      // and need to be root to write there.
      bool called_as_root = geteuid() == SGE_SUPERUSER_UID;
      int component = component_get_component_id();
      bool switch_user = called_as_root && (component == QMASTER || component == EXECD);
      bool create_certificate_and_key = false;

      if (!std::filesystem::exists(cert_path) ||
          !std::filesystem::exists(key_path)) {
         ret = verify_create_directories(switch_user, error_dstr);
         if (ret) {
            create_certificate_and_key = true;
         }
      } else {
         // @todo also check how long the certificate/key are still valid
         if (certificate_recreate_required()) {
            create_certificate_and_key = true;
         }
      }

      if (ret && create_certificate_and_key) {
         // @todo can we create the certificate from some template?
         // @todo need to lock the directory? Otherwise multiple processes might try to create the certificate at the same time
         EVP_PKEY *pkey = EVP_PKEY_new_func();
         RSA *rsa = RSA_new_func();
         BIGNUM *bn = BN_new_func();
         X509 *x509 = X509_new_func();

         BN_set_word_func(bn, RSA_F4);
         RSA_generate_key_ex_func(rsa, 2048, bn, nullptr);
         EVP_PKEY_assign_func(pkey, EVP_PKEY_RSA, rsa);

         X509_set_version_func(x509, 2);
         ASN1_INTEGER_set_func(X509_get_serialNumber_func(x509), 1);
         X509_gmtime_adj_func(X509_getm_notBefore_func(x509), 0); // @todo could we give a negative value here to avoid validity problems with notBefore?
         X509_gmtime_adj_func(X509_getm_notAfter_func(x509), 31536000L); // 1 Jahr
         X509_set_pubkey_func(x509, pkey);

         X509_NAME *name = X509_get_subject_name_func(x509);
         X509_NAME_add_entry_by_txt_func(name, "CN", MBSTRING_ASC, (unsigned char *)component_get_qualified_hostname(), -1, -1, 0);
         X509_set_issuer_name_func(x509, name);

         X509_sign_func(x509, pkey, EVP_sha256_func());

         FILE *f = fopen(key_path.c_str(), "wb");
         PEM_write_PrivateKey_func(f, pkey, nullptr, nullptr, 0, nullptr, nullptr);
         fclose(f);
         // @todo in theory we have a very short time window here where the file is created but not yet protected
         chmod(key_path.c_str(), S_IRUSR | S_IWUSR); // key file should be only readable/writable by owner

         // need to switch to admin user if we are root
         // SGE_ROOT might be on a filesystem where root has no write permissions
         if (switch_user) {
            sge_switch2admin_user();
         }
         f = fopen(cert_path.c_str(), "wb");
         PEM_write_X509_func(f, x509);
         fclose(f);
         chmod(cert_path.c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); // cert file should be readable by everyone
         if (switch_user) {
            sge_switch2start_user();
         }

         X509_free_func(x509);
         EVP_PKEY_free_func(pkey);
         // rsa is freed with pkey
         BN_free_func(bn);
      }

      return ret;
   }

   bool OpenSSL::OpenSSLContext::configure_server_context(dstring *error_dstr) {
      bool ret = true;

      // verify if certificate and key files exist, otherwise create them
      if (ret) {
         ret = verify_create_certificate_and_key(error_dstr);
      }

      /* Set the key and cert */
      if (ret) {
         if (SSL_CTX_use_certificate_chain_file_func(ssl_ctx, cert_path.c_str()) <= 0) {
            sge_dstring_sprintf(error_dstr, "Unable to read %s: %s", cert_path.c_str(), ERR_reason_error_string_func(ERR_get_error_func()));
            ret = false;
         }
      }

      if (ret) {
         if (SSL_CTX_use_PrivateKey_file_func(ssl_ctx, key_path.c_str(), SSL_FILETYPE_PEM) <= 0) {
            sge_dstring_sprintf(error_dstr, "Unable to read %s: %s", key_path.c_str(), ERR_reason_error_string_func(ERR_get_error_func()));
            ret = false;
         }
      }

      return ret;
   }

   bool OpenSSL::OpenSSLContext::configure_client_context(dstring *error_dstr) {
      bool ret = true;
      if (cert_path.empty()) {
         // We do not use this, consider handling it an error.
         SSL_CTX_set_verify_func(ssl_ctx, SSL_VERIFY_NONE, nullptr);
      } else {
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
            sge_dstring_sprintf(error_dstr, "Unable to load verify location %s: %s", cert_path.c_str(), ocs::uti::OpenSSL::ERR_reason_error_string_func(ocs::uti::OpenSSL::ERR_get_error_func()));
            ret = false;
         }
      }

      return ret;
   }

   OpenSSL::OpenSSLContext::~OpenSSLContext() {
      // free the SSL context
      if (ssl_ctx != nullptr) {
         SSL_CTX_free_func(ssl_ctx);
         ssl_ctx = nullptr;
      }
   }

   OpenSSL::OpenSSLContext * OpenSSL::OpenSSLContext::create(bool is_server, std::string &cert_path, std::string &key_path, dstring *error_dstr) {
      OpenSSLContext *ret{nullptr};

      bool ok = true;

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

      return ret;
   }

   OpenSSL::OpenSSLConnection *OpenSSL::OpenSSLConnection::create(OpenSSLContext *context, dstring *error_dstr) {
      OpenSSLConnection *ret{nullptr};

      bool ok = true;
      bool is_server = context->get_is_server();

      // initialize the SSL context
      SSL *ssl;
      if (ok) {
         ssl = OpenSSL::SSL_new_func(context->get_SSL_CTX());
         if (ssl == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_CANNOT_CREATE_SSL_S, OpenSSL::ERR_reason_error_string_func(OpenSSL::ERR_get_error_func()));
            ok = false;
         }
      }
      if (ok) {
         // make sure that protocol handshakes are automatically restarted when interrupted
         //SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
         SSL_ctrl_func(ssl,SSL_CTRL_MODE, SSL_MODE_AUTO_RETRY,nullptr);
      }

      if (ok) {
         ret = new OpenSSLConnection(is_server, ssl);
      }

      return ret;
   }

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
            struct timeval tv;
            tv.tv_sec = 1; // @todo make timeout configurable
            tv.tv_usec = 0;
            int select_ret = select(fd + 1, read_fds, write_fds, nullptr, &tv);
            if (select_ret < 0) {
               if (errno != EWOULDBLOCK && errno != EAGAIN && errno != EINTR) {
                  sge_dstring_sprintf(error_dstr, MSG_SSL_SELECT_FAILED_DS, errno, strerror(errno));
                  ret = false;
               }
            } else if (select_ret == 0) {
               // @todo really an error? Or just try again?
               sge_dstring_sprintf(error_dstr, MSG_SSL_SELECT_TIMEOUT);
               ret = false;
            }
         }
      }

      if (!ret) {
         DPRINTF("  --> error: %s\n", sge_dstring_get_string(error_dstr));
      }

      DRETURN(ret);
   }


   OpenSSL::OpenSSLConnection::~OpenSSLConnection() {
      if (ssl != nullptr) {
         SSL_shutdown_func(ssl);
         SSL_free_func(ssl);
      }
   }

   bool OpenSSL::OpenSSLConnection::set_fd(int new_fd, dstring *error_dstr) {
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
      return ret;
   }

   int OpenSSL::OpenSSLConnection::read(char *buffer, size_t max_len, dstring *error_dstr) {
      DENTER(TOP_LAYER);
      // clear previously occurred but not yet fetched errors
      ERR_clear_error_func();

      DPRINTF("OpenSSLConnection::read()\n");
      bool done;
      do {
         done = true;
         int read_ret = SSL_read_func(ssl, buffer, static_cast<int>(max_len));
         if (read_ret > 0) {
            // we actually read some data without errors
            DRETURN(read_ret);
         } else {
            int err = SSL_get_error_func(ssl, read_ret);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
               DPRINTF("  --> got %s\n", err == SSL_ERROR_WANT_READ ? "SSL_ERROR_WANT_READ" : "SSL_ERROR_WANT_WRITE");
               // @todo workaround: need to set commlib connection into some special state and re-visit
               //       ==> even if we see a socket ready for write, if we have an unfinished read, we need
               //           to try the read again
               //       ==> store the unfinished operation in the OpenSSLConnection object
               //       ==> return a boolean indicating that we need to repeat something
               //       ==> have a special state (multiple states?) in commlib connection to indicate that
               //           we need to repeat some operation (read, write, accept, connect)
               // @todo test with test_openssl binary operating on non-blocking socket
               if (wait_for_socket_ready(err, error_dstr)) {
                  done = false; // try again
                  DPRINTF("  --> repeat read\n");
               }
            } else {
               sge_dstring_sprintf(error_dstr, MSG_CANNOT_READ_DS, err,
                                   ERR_reason_error_string_func(ERR_get_error_func()));
            }
         }
      } while (!done);
      DRETURN(-1);
   }

   int OpenSSL::OpenSSLConnection::write(char *buffer, size_t len, dstring *error_dstr) {
      DENTER(TOP_LAYER);
      // clear previously occurred but not yet fetched errors
      ERR_clear_error_func();

      DPRINTF("OpenSSLConnection::write()\n");
      bool done;
      do {
         done = true;
         int write_ret = SSL_write_func(ssl, buffer, static_cast<int>(len));
         if (write_ret > 0) {
            // we actually wrote some data without errors
            DRETURN(write_ret);
         } else {
            int err = SSL_get_error_func(ssl, write_ret);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
               DPRINTF("  --> got %s\n", err == SSL_ERROR_WANT_READ ? "SSL_ERROR_WANT_READ" : "SSL_ERROR_WANT_WRITE");
               if (wait_for_socket_ready(err, error_dstr)) {
                  done = false; // try again
                  DPRINTF("  --> repeat write\n");
               }
            } else {
               sge_dstring_sprintf(error_dstr, MSG_CANNOT_WRITE_DS, err,
                                   ERR_reason_error_string_func(ERR_get_error_func()));
            }
         }
      } while (!done);
      // if we get here an error occurred
      DRETURN(-1);
   }

   bool OpenSSL::OpenSSLConnection::accept(dstring *error_dstr) {
      DENTER(TOP_LAYER);
      DPRINTF("OpenSSLConnection::accept()\n");

      // clear previously occurred but not yet fetched errors
      ERR_clear_error_func();

      bool ret = ssl != nullptr;

      if (ret) {
         if (!is_server) {
            sge_dstring_copy_string(error_dstr, MSG_SSL_ACCEPT_CALLED_ON_CLIENT);
            ret = false;
         }
      }
      if (ret) {
         // @todo there is some option for auto repeat with blocking io, and it is still blocking at the time of the accept call
         bool done;
         do {
            done = true;
            int accept_ret = SSL_accept_func(ssl);
            if (accept_ret <= 0) {
               int err = SSL_get_error_func(ssl, accept_ret);
               if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_ACCEPT) {
                  DPRINTF("  --> got %s\n", err == SSL_ERROR_WANT_READ ? "SSL_ERROR_WANT_READ" : "SSL_ERROR_WANT_WRITE");
                  if (wait_for_socket_ready(err, error_dstr)) {
                     done = false; // try again
                     DPRINTF("  --> repeat accept\n");
                  }
               } else {
                  // @todo need to call SSL_get_error_func() for all failing SSL_* functions?
                  sge_dstring_sprintf(error_dstr, MSG_CANNOT_ACCEPT_DS, err,
                                      ERR_reason_error_string_func(ERR_get_error_func()));
                  DPRINTF("  --> got error: %d: %s\n", err, sge_dstring_get_string(error_dstr));
                  ret = false;
               }
            }
         } while (!done);
      }

      DRETURN(ret);
   }

   bool OpenSSL::OpenSSLConnection::set_server_name_for_sni(const char *server_name, dstring *error_dstr) {
      // clear previously occurred but not yet fetched errors
      ERR_clear_error_func();

      bool ret = ssl != nullptr;

      // we do this only on the client side
      if (ret && !is_server) { // @todo: ERROR when is_server is true
         // Set hostname for SNI
         // SSL_set_tlsext_host_name(s, name) is a macro:
         // SSL_ctrl(s,SSL_CTRL_SET_TLSEXT_HOSTNAME,TLSEXT_NAMETYPE_host_name, (void *)name)
         SSL_ctrl_func(ssl, SSL_CTRL_SET_TLSEXT_HOSTNAME, TLSEXT_NAMETYPE_host_name, (void *)server_name);
         /* Configure server hostname check */
         if (!SSL_set1_host_func(ssl, server_name)) {
            sge_dstring_sprintf(error_dstr, MSG_CANNOT_SET_SERVERNAME_S, ERR_reason_error_string_func(ERR_get_error_func()));
            ret = false;
         }
      }

      return ret;
   }

   bool OpenSSL::OpenSSLConnection::connect(dstring *error_dstr) {
      DENTER(TOP_LAYER);
      // clear previously occurred but not yet fetched errors
      ERR_clear_error_func();

      bool ret = ssl != nullptr;

      DPRINTF("OpenSSLConnection::connect()\n");

      // we do this only on the client side
      if (ret) {
         if (is_server) {
            sge_dstring_copy_string(error_dstr, MSG_SSL_CONNECT_CALLED_ON_SERVER);
            ret = false;
         }
      }
      if (ret) {
         bool done;
         do {
            done = true;
            int connect_ret = SSL_connect_func(ssl);
            if (connect_ret <= 0) {
               // @todo need to call SSL_get_error_func() for all failing SSL_* functions?
               int err = SSL_get_error_func(ssl, connect_ret);
               if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_CONNECT) {
                  DPRINTF("  --> got connect error %d\n", err);
                  // @todo workaround: need to set commlib connection into some special state and re-visit
                  // @todo test with test_openssl binary operating on non-blocking socket
                  if (wait_for_socket_ready(err, error_dstr)) {
                     done = false; // try again
                     DPRINTF("  --> repeat connect\n");
                  }
               } else {
                  sge_dstring_sprintf(error_dstr, MSG_CANNOT_CONNECT_DS, err,
                                      ERR_reason_error_string_func(ERR_get_error_func()));
                  ret = false;
               }
            }
         } while (!done);
      }

      DPRINTF("  --> returning %d: %s\n", ret, sge_dstring_get_string(error_dstr));

      DRETURN(ret);
   }
} // namespace ocs::uti

#endif
