#pragma once
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

#if defined(OCS_WITH_OPENSSL)
#include <openssl/ssl.h>
#include <openssl/err.h>

#if OPENSSL_VERSION_MAJOR != 3
#error "Only OpenSSL version 3 is supported"
#endif

#include <filesystem>
#include <string>
#include "uti/sge_dstring.h"


namespace ocs::uti {
   // function types for the OpenSSL interface
   // @todo sort the types, the function pointers, the initializations
   using TLS_server_method_func_t = const SSL_METHOD *(*)();
   using TLS_client_method_func_t = const SSL_METHOD *(*)();
   using SSL_CTX_new_func_t = SSL_CTX *(*)(const SSL_METHOD *method);
   using ERR_get_error_func_t = unsigned long (*)();
   using ERR_reason_error_string_func_t = const char *(*)(unsigned long e);
   using SSL_CTX_use_certificate_chain_file_func_t = int (*)(SSL_CTX *ctx, const char *file);
   using SSL_CTX_use_PrivateKey_file_func_t = int (*)(SSL_CTX *ctx, const char *file, int type);
   using SSL_CTX_load_verify_locations_func_t = int (*)(SSL_CTX *ctx, const char *CAfile, const char *CApath);
   using SSL_CTX_set_verify_func_t = void (*)(SSL_CTX *ctx, int mode, int (*callback)(int, X509_STORE_CTX *));
   using SSL_new_func_t = SSL *(*)(SSL_CTX *ctx);
   using SSL_set_fd_func_t = int (*)(SSL *ssl, int fd);
   using SSL_accept_func_t = int (*)(SSL *ssl);
   using SSL_read_func_t = int (*)(SSL *ssl, char *buf, int len);
   using SSL_write_func_t = int (*)(SSL *ssl, char *buf, int len);
   using SSL_shutdown_func_t = int (*)(SSL *ssl);
   using SSL_free_func_t = void (*)(SSL *ssl);
   using SSL_CTX_free_func_t = void (*)(SSL_CTX *ctx);
   using SSL_set1_host_func_t = int (*)(SSL *ssl, const char *name);
   using SSL_connect_func_t = int (*)(SSL *ssl);
   using SSL_ctrl_func_t = long (*)(SSL *ssl, int cmd, long larg, void *parg);
   using OPENSSL_init_ssl_func_t = int (*)(uint64_t opts, const void *settings);

   using EVP_PKEY_new_func_t = EVP_PKEY *(*)();
   using EVP_PKEY_free_func_t = void (*)(EVP_PKEY *pkey);
   using EVP_PKEY_assign_func_t = int (*)(EVP_PKEY *pkey, int type, void *key);
   using RSA_new_func_t = RSA *(*)();
   using RSA_free_func_t = void (*)(RSA *r);
   using RSA_generate_key_ex_func_t = int (*)(RSA *rsa, int bits, BIGNUM *e, BN_GENCB *cb);
   using BN_new_func_t = BIGNUM *(*)();
   using BN_free_func_t = void (*)(BIGNUM *a);
   using BN_set_word_func_t = int (*)(BIGNUM *a, unsigned long w);
   using X509_new_func_t = X509 *(*)();
   using X509_free_func_t = void (*)(X509 *a);
   using X509_set_version_func_t = int (*)(X509 *x, long version);
   using ASN1_INTEGER_set_func_t = int (*)(ASN1_INTEGER *a, long v);
   using X509_gmtime_adj_func_t = ASN1_TIME *(*)(ASN1_TIME *s, long adj);
   using X509_set_pubkey_func_t = int (*)(X509 *x, EVP_PKEY *pkey);
   using X509_get_serialNumber_func_t = ASN1_INTEGER *(*)(X509 *x);
   using X509_NAME_add_entry_by_txt_func_t = int (*)(X509_NAME *name, const char *field, int type, const unsigned char *bytes, int len, int loc, int set);
   using X509_set_issuer_name_func_t = int (*)(X509 *x, X509_NAME *name);
   using X509_sign_func_t = int (*)(X509 *x, EVP_PKEY *pkey, const EVP_MD *md);
   using PEM_write_PrivateKey_func_t = int (*)(FILE *fp, EVP_PKEY *x, const EVP_CIPHER *enc, unsigned char *kstr, int klen, pem_password_cb *cb, void *u);
   using PEM_write_X509_func_t = int (*)(FILE *fp, X509 *x);
   using EVP_sha256_func_t = const EVP_MD *(*)();
   using X509_get_subject_name_func_t = X509_NAME *(*)(X509 *x);
   using X509_getm_notBefore_func_t = ASN1_TIME *(*)(X509 *x);
   using X509_getm_notAfter_func_t = ASN1_TIME *(*)(X509 *x);
   using SSL_get_error_func_t = int (*)(SSL *ssl, int ret);
   using ERR_clear_error_func_t = void (*)();
   using PEM_read_X509_func_t = X509 *(*)(FILE *fp, X509 **x, pem_password_cb *cb, void *u);
   using X509_get0_notAfter_func_t = const ASN1_TIME *(*)(const X509 *x);
   using ASN1_TIME_diff_func_t = int (*)(int *pday, int *psec, const ASN1_TIME *from, const ASN1_TIME *to);

   class OpenSSL {
      // static data
      // handle and function pointers of the libssl.so
      static void *lib_handle;
      static TLS_server_method_func_t TLS_server_method_func;
      static TLS_client_method_func_t TLS_client_method_func;
      static SSL_CTX_new_func_t SSL_CTX_new_func;
      static ERR_get_error_func_t ERR_get_error_func;
      static ERR_reason_error_string_func_t ERR_reason_error_string_func;
      static SSL_CTX_use_certificate_chain_file_func_t SSL_CTX_use_certificate_chain_file_func;
      static SSL_CTX_use_PrivateKey_file_func_t SSL_CTX_use_PrivateKey_file_func;
      static SSL_CTX_load_verify_locations_func_t SSL_CTX_load_verify_locations_func;
      static SSL_CTX_set_verify_func_t SSL_CTX_set_verify_func;
      static SSL_new_func_t SSL_new_func;
      static SSL_set_fd_func_t SSL_set_fd_func;
      static SSL_accept_func_t SSL_accept_func;
      static SSL_read_func_t SSL_read_func;
      static SSL_write_func_t SSL_write_func;
      static SSL_shutdown_func_t SSL_shutdown_func;
      static SSL_free_func_t SSL_free_func;
      static SSL_CTX_free_func_t SSL_CTX_free_func;
      static SSL_set1_host_func_t SSL_set1_host_func;
      static SSL_connect_func_t SSL_connect_func;
      static SSL_ctrl_func_t SSL_ctrl_func;
      static OPENSSL_init_ssl_func_t OPENSSL_init_ssl_func;
      static EVP_PKEY_new_func_t EVP_PKEY_new_func;
      static EVP_PKEY_free_func_t EVP_PKEY_free_func;
      static EVP_PKEY_assign_func_t EVP_PKEY_assign_func;
      static RSA_new_func_t RSA_new_func;
      static RSA_free_func_t RSA_free_func;
      static RSA_generate_key_ex_func_t RSA_generate_key_ex_func;
      static BN_new_func_t BN_new_func;
      static BN_free_func_t BN_free_func;
      static BN_set_word_func_t BN_set_word_func;
      static X509_new_func_t X509_new_func;
      static X509_free_func_t X509_free_func;
      static X509_set_version_func_t X509_set_version_func;
      static ASN1_INTEGER_set_func_t ASN1_INTEGER_set_func;
      static X509_gmtime_adj_func_t X509_gmtime_adj_func;
      static X509_set_pubkey_func_t X509_set_pubkey_func;
      static X509_get_serialNumber_func_t X509_get_serialNumber_func;
      static X509_NAME_add_entry_by_txt_func_t X509_NAME_add_entry_by_txt_func;
      static X509_set_issuer_name_func_t X509_set_issuer_name_func;
      static X509_sign_func_t X509_sign_func;
      static PEM_write_PrivateKey_func_t PEM_write_PrivateKey_func;
      static PEM_write_X509_func_t PEM_write_X509_func;
      static EVP_sha256_func_t EVP_sha256_func;
      static X509_get_subject_name_func_t X509_get_subject_name_func;
      static X509_getm_notBefore_func_t X509_getm_notBefore_func;
      static X509_getm_notAfter_func_t X509_getm_notAfter_func;
      static SSL_get_error_func_t SSL_get_error_func;
      static ERR_clear_error_func_t ERR_clear_error_func;
      static PEM_read_X509_func_t PEM_read_X509_func;
      static X509_get0_notAfter_func_t X509_get0_notAfter_func;
      static ASN1_TIME_diff_func_t ASN1_TIME_diff_func;

   public:
      // static methods
      static bool initialize(dstring *error_dstr);
      static void cleanup();
      static bool is_openssl_available() { return lib_handle != nullptr; }
      static bool build_cert_path(std::string &cert_path, const char *home_dir, const char *hostname);
      static bool build_key_path(std::string &key_path, const char *home_dir, const char *hostname);

      // sub-classes
      class OpenSSLContext {
         bool is_server;
         SSL_CTX *ssl_ctx;
         std::filesystem::path cert_path;
         std::filesystem::path key_path;

         // private constructor, use the create() method
         OpenSSLContext(bool is_server, SSL_CTX *ssl_ctx, std::filesystem::path(cert_path), std::filesystem::path(key_path))
         : is_server(is_server), ssl_ctx(ssl_ctx), cert_path{cert_path}, key_path {key_path} {}

         bool verify_create_directories(bool switch_user, dstring *error_dstr);
         bool verify_create_certificate_and_key(dstring *error_dstr);
         bool certificate_recreate_required(dstring *error_dstr);

         bool configure_server_context(dstring *error_dstr);
         bool configure_client_context(dstring *error_dstr);

      public:
         static OpenSSLContext *create(bool is_server, std::string &cert_path, std::string &key_path, dstring *error_dstr);
         ~OpenSSLContext();

         bool get_is_server() { return is_server; }
         const char *get_cert_file() { return cert_path.c_str(); }
         SSL_CTX *get_SSL_CTX() { return ssl_ctx; }
      };

      class OpenSSLConnection {
         bool is_server;
         SSL *ssl;
         int fd;

         // private constructor, use the create() method
         OpenSSLConnection(bool is_server, SSL *ssl) : is_server(is_server), ssl(ssl) {}
         bool wait_for_socket_ready(int reason, dstring *error_dstr);

      public:
         static OpenSSLConnection *create(OpenSSLContext *context, dstring *error_dstr);
         ~OpenSSLConnection();

         SSL *get_ssl() { return ssl; } // @todo remove it
         bool set_fd(int new_fd, dstring *error_dstr);
         bool accept(dstring *error_dstr); // server side
         bool set_server_name_for_sni(const char *server_name, dstring *error_dstr); // client side
         bool connect(dstring *error_dstr);
         int read(char *buffer, size_t max_len, dstring *error_dstr);
         int write(char *buffer, size_t len, dstring *error_dstr);
      };
   };
} // namespace ocs::uti
#endif
