#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025-2026 HPC-Gridware GmbH
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

#if OPENSSL_VERSION_MAJOR != 3 && OPENSSL_VERSION_MAJOR != 4
#error "Only OpenSSL versions 3 and 4 are supported"
#endif

#include <filesystem>
#include <string>
#include <vector>

#include "uti/sge_dstring.h"


/** @file
 * @brief Dynamically loaded TLS, built on OpenSSL 3 or 4
 *
 * TLS is one of the security modes selectable in the `bootstrap` file — see
 * `ocs::Bootstrap::BS_SEC_MODE_TLS`. `libssl` is opened with `dlopen` at
 * runtime rather than linked, so a binary built with TLS support still starts
 * on a host without OpenSSL; @ref ocs::uti::OpenSSL::is_openssl_available
 * reports which case applies.
 *
 * Three layers, outer to inner: @ref ocs::uti::OpenSSL loads the library,
 * @ref ocs::uti::OpenSSL::OpenSSLContext holds one certificate and key pair,
 * and @ref ocs::uti::OpenSSL::OpenSSLConnection is one TLS connection on a
 * socket.
 */

namespace ocs::uti {
   /// @cond   pointer types of the libssl functions, resolved with dlsym at runtime
   // Function types for the OpenSSL interface. They are taken from the declarations in the OpenSSL
   // headers, so that the compiler checks every call against the OpenSSL version we build with.
   using ASN1_INTEGER_set_func_t = decltype(&::ASN1_INTEGER_set);
   using ASN1_STRING_get0_data_func_t = decltype(&::ASN1_STRING_get0_data);
   using ASN1_STRING_length_func_t = decltype(&::ASN1_STRING_length);
   using ASN1_TIME_diff_func_t = decltype(&::ASN1_TIME_diff);
   using BIO_free_func_t = decltype(&::BIO_free);
   using BIO_new_func_t = decltype(&::BIO_new);
   using BIO_number_written_func_t = decltype(&::BIO_number_written);
   using BIO_read_func_t = decltype(&::BIO_read);
   using BIO_s_mem_func_t = decltype(&::BIO_s_mem);
   using ERR_clear_error_func_t = decltype(&::ERR_clear_error);
   using ERR_get_error_func_t = decltype(&::ERR_get_error);
   using ERR_reason_error_string_func_t = decltype(&::ERR_reason_error_string);
   using EVP_PKEY_free_func_t = decltype(&::EVP_PKEY_free);
   using EVP_PKEY_Q_keygen_func_t = decltype(&::EVP_PKEY_Q_keygen);
   using EVP_sha256_func_t = decltype(&::EVP_sha256);
   using OPENSSL_init_ssl_func_t = decltype(&::OPENSSL_init_ssl);
   using OpenSSL_version_func_t = decltype(&::OpenSSL_version);
   using PEM_read_X509_func_t = decltype(&::PEM_read_X509);
   using PEM_read_PrivateKey_func_t = decltype(&::PEM_read_PrivateKey);
   using X509_check_private_key_func_t = decltype(&::X509_check_private_key);
   using X509_NAME_ENTRY_get_data_func_t = decltype(&::X509_NAME_ENTRY_get_data);
   using X509_NAME_get_entry_func_t = decltype(&::X509_NAME_get_entry);
   using X509_NAME_get_index_by_NID_func_t = decltype(&::X509_NAME_get_index_by_NID);
   using PEM_write_PrivateKey_func_t = decltype(&::PEM_write_PrivateKey);
   using PEM_write_X509_func_t = decltype(&::PEM_write_X509);
   using PEM_write_bio_X509_func_t = decltype(&::PEM_write_bio_X509);
   using SSL_CTX_free_func_t = decltype(&::SSL_CTX_free);
   using SSL_CTX_get0_certificate_func_t = decltype(&::SSL_CTX_get0_certificate);
   using SSL_CTX_load_verify_locations_func_t = decltype(&::SSL_CTX_load_verify_locations);
   using SSL_CTX_new_func_t = decltype(&::SSL_CTX_new);
   using SSL_CTX_set_verify_func_t = decltype(&::SSL_CTX_set_verify);
   using SSL_CTX_use_PrivateKey_file_func_t = decltype(&::SSL_CTX_use_PrivateKey_file);
   using SSL_CTX_use_PrivateKey_func_t = decltype(&::SSL_CTX_use_PrivateKey);
   using SSL_CTX_use_certificate_chain_file_func_t =
         decltype(&::SSL_CTX_use_certificate_chain_file);
   using SSL_CTX_use_certificate_func_t = decltype(&::SSL_CTX_use_certificate);
   using SSL_accept_func_t = decltype(&::SSL_accept);
   using SSL_connect_func_t = decltype(&::SSL_connect);
   using SSL_ctrl_func_t = decltype(&::SSL_ctrl);
   using SSL_free_func_t = decltype(&::SSL_free);
   using SSL_get_error_func_t = decltype(&::SSL_get_error);
   using SSL_is_init_finished_func_t = decltype(&::SSL_is_init_finished);
   using SSL_new_func_t = decltype(&::SSL_new);
   using SSL_read_func_t = decltype(&::SSL_read);
#if OPENSSL_VERSION_MAJOR >= 4
   using SSL_set1_dnsname_func_t = decltype(&::SSL_set1_dnsname);
#else
   using SSL_set1_host_func_t = decltype(&::SSL_set1_host);
#endif
   using SSL_set_fd_func_t = decltype(&::SSL_set_fd);
   using SSL_shutdown_func_t = decltype(&::SSL_shutdown);
   using SSL_write_func_t = decltype(&::SSL_write);
   using TLS_client_method_func_t = decltype(&::TLS_client_method);
   using TLS_server_method_func_t = decltype(&::TLS_server_method);
   using X509_NAME_add_entry_by_txt_func_t = decltype(&::X509_NAME_add_entry_by_txt);
   using X509_NAME_free_func_t = decltype(&::X509_NAME_free);
   using X509_NAME_new_func_t = decltype(&::X509_NAME_new);
   using X509_free_func_t = decltype(&::X509_free);
   using X509_get0_notAfter_func_t = decltype(&::X509_get0_notAfter);
   using X509_get_serialNumber_func_t = decltype(&::X509_get_serialNumber);
   using X509_get_subject_name_func_t = decltype(&::X509_get_subject_name);
   using X509_getm_notAfter_func_t = decltype(&::X509_getm_notAfter);
   using X509_getm_notBefore_func_t = decltype(&::X509_getm_notBefore);
   using X509_gmtime_adj_func_t = decltype(&::X509_gmtime_adj);
   using X509_new_func_t = decltype(&::X509_new);
   using X509_set_issuer_name_func_t = decltype(&::X509_set_issuer_name);
   using X509_set_pubkey_func_t = decltype(&::X509_set_pubkey);
   using X509_set_subject_name_func_t = decltype(&::X509_set_subject_name);
   using X509_set_version_func_t = decltype(&::X509_set_version);
   using X509_sign_func_t = decltype(&::X509_sign);
   /// @endcond

   /**
    * @brief Entry point to the OpenSSL library and the TLS objects built on it
    *
    * Static only: the library is opened once per process by #initialize and
    * the resolved function pointers are shared. Only OpenSSL 3 and 4 are
    * supported; the header refuses to compile against any other major version.
    */
   class OpenSSL {
      // static data
      // handle and function pointers of the libssl.so
      static void *libssl_handle;
      // name of the loaded libssl and the OpenSSL version it reports
      static std::string libssl_name;
      static std::string libssl_version;

      static ASN1_INTEGER_set_func_t ASN1_INTEGER_set_func;
      static ASN1_STRING_get0_data_func_t ASN1_STRING_get0_data_func;
      static ASN1_STRING_length_func_t ASN1_STRING_length_func;
      static ASN1_TIME_diff_func_t ASN1_TIME_diff_func;
      static BIO_free_func_t BIO_free_func;
      static BIO_new_func_t BIO_new_func;
      static BIO_number_written_func_t BIO_number_written_func;
      static BIO_read_func_t BIO_read_func;
      static BIO_s_mem_func_t BIO_s_mem_func;
      static ERR_clear_error_func_t ERR_clear_error_func;
      static ERR_get_error_func_t ERR_get_error_func;
      static ERR_reason_error_string_func_t ERR_reason_error_string_func;
      static EVP_PKEY_free_func_t EVP_PKEY_free_func;
      static EVP_PKEY_Q_keygen_func_t EVP_PKEY_Q_keygen_func;
      static EVP_sha256_func_t EVP_sha256_func;
      static OPENSSL_init_ssl_func_t OPENSSL_init_ssl_func;
      static OpenSSL_version_func_t OpenSSL_version_func;
      static PEM_read_X509_func_t PEM_read_X509_func;
      static PEM_read_PrivateKey_func_t PEM_read_PrivateKey_func;
      static X509_check_private_key_func_t X509_check_private_key_func;
      static X509_NAME_ENTRY_get_data_func_t X509_NAME_ENTRY_get_data_func;
      static X509_NAME_get_entry_func_t X509_NAME_get_entry_func;
      static X509_NAME_get_index_by_NID_func_t X509_NAME_get_index_by_NID_func;
      static PEM_write_PrivateKey_func_t PEM_write_PrivateKey_func;
      static PEM_write_X509_func_t PEM_write_X509_func;
      static PEM_write_bio_X509_func_t PEM_write_bio_X509_func;
      static SSL_CTX_free_func_t SSL_CTX_free_func;
      static SSL_CTX_get0_certificate_func_t SSL_CTX_get0_certificate_func;
      static SSL_CTX_load_verify_locations_func_t SSL_CTX_load_verify_locations_func;
      static SSL_CTX_new_func_t SSL_CTX_new_func;
      static SSL_CTX_set_verify_func_t SSL_CTX_set_verify_func;
      static SSL_CTX_use_PrivateKey_file_func_t SSL_CTX_use_PrivateKey_file_func;
      static SSL_CTX_use_PrivateKey_func_t SSL_CTX_use_PrivateKey_func;
      static SSL_CTX_use_certificate_chain_file_func_t SSL_CTX_use_certificate_chain_file_func;
      static SSL_CTX_use_certificate_func_t SSL_CTX_use_certificate_func;
      static SSL_accept_func_t SSL_accept_func;
      static SSL_connect_func_t SSL_connect_func;
      static SSL_ctrl_func_t SSL_ctrl_func;
      static SSL_free_func_t SSL_free_func;
      static SSL_get_error_func_t SSL_get_error_func;
      static SSL_is_init_finished_func_t SSL_is_init_finished_func;
      static SSL_new_func_t SSL_new_func;
      static SSL_read_func_t SSL_read_func;
#if OPENSSL_VERSION_MAJOR >= 4
      static SSL_set1_dnsname_func_t SSL_set1_dnsname_func;
#else
      static SSL_set1_host_func_t SSL_set1_host_func;
#endif
      static SSL_set_fd_func_t SSL_set_fd_func;
      static SSL_shutdown_func_t SSL_shutdown_func;
      static SSL_write_func_t SSL_write_func;
      static TLS_client_method_func_t TLS_client_method_func;
      static TLS_server_method_func_t TLS_server_method_func;
      static X509_NAME_add_entry_by_txt_func_t X509_NAME_add_entry_by_txt_func;
      static X509_NAME_free_func_t X509_NAME_free_func;
      static X509_NAME_new_func_t X509_NAME_new_func;
      static X509_free_func_t X509_free_func;
      static X509_get0_notAfter_func_t X509_get0_notAfter_func;
      static X509_get_serialNumber_func_t X509_get_serialNumber_func;
      static X509_get_subject_name_func_t X509_get_subject_name_func;
      static X509_getm_notAfter_func_t X509_getm_notAfter_func;
      static X509_getm_notBefore_func_t X509_getm_notBefore_func;
      static X509_gmtime_adj_func_t X509_gmtime_adj_func;
      static X509_new_func_t X509_new_func;
      static X509_set_issuer_name_func_t X509_set_issuer_name_func;
      static X509_set_pubkey_func_t X509_set_pubkey_func;
      static X509_set_subject_name_func_t X509_set_subject_name_func;
      static X509_set_version_func_t X509_set_version_func;
      static X509_sign_func_t X509_sign_func;

   public:
      // static methods
      static bool initialize(dstring *error_dstr);
      static void cleanup();

      /**
       * @brief Is libssl loaded and usable?
       *
       * @return true once #initialize has succeeded; false when the library is
       *         absent, which is not in itself an error
       */
      static bool is_openssl_available() { return libssl_handle != nullptr; }

      /// Log the name of the loaded libssl and the OpenSSL version it reports
      static void log_library_info();

      static bool build_cert_path(std::string &cert_path, const char *home_dir, const char *hostname, const char *comp_name);
      static bool build_key_path(std::string &key_path, const char *home_dir, const char *hostname, uint32_t port, const char *comp_name);
      static const char *get_error_message();

      // sub-classes
      /**
       * @brief One certificate and key pair, as an `SSL_CTX`
       *
       * Created through one of the #create overloads; the constructor is
       * private. A context is shared by every connection made from it and
       * counts them, so it is destroyed through
       * #mark_context_for_deletion rather than `delete` — the actual
       * destruction is deferred until the last connection has gone.
       *
       * Certificates expire, so a context also knows when it needs replacing:
       * see #certificate_recreate_required and #is_cert_file_updated.
       */
      class OpenSSLContext {
         static std::vector<OpenSSLContext *> contexts_to_delete;
         bool is_server;
         std::filesystem::file_time_type client_certificate_time;
         uint64_t renewal_time;
         int connection_count;
         SSL_CTX *ssl_ctx;
         std::filesystem::path cert_path;
         std::filesystem::path key_path;

         // private constructor, use the create() method
         OpenSSLContext(bool is_server, SSL_CTX *ssl_ctx, std::filesystem::path(cert_path), std::filesystem::path(key_path))
         : is_server(is_server), renewal_time(0), connection_count(0), ssl_ctx(ssl_ctx), cert_path{cert_path}, key_path {key_path} {}

         bool verify_create_directories(bool switch_user, bool called_as_root, dstring *error_dstr, bool &created_dirs) const;
         bool certificate_recreate_required(dstring *error_dstr);
         std::string installation_tag() const;
         /**
          * @brief Set when the certificate on disk belongs to another installation.
          *
          * Such a certificate must not be replaced -- the installation that owns
          * it may be running with it. The daemon start fails instead. CS-2487.
          */
         bool cert_of_other_installation = false;

         bool configure_server_context(dstring *error_dstr, bool is_recreate);
         bool configure_client_context(dstring *error_dstr);

         static void delete_no_longer_used_contexts();

      public:
         static OpenSSLContext *create(dstring *error_dstr);
         static OpenSSLContext *create(const OpenSSLContext *source, dstring *error_dstr, bool is_recreate);
         static OpenSSLContext *create(bool is_server, const std::string &cert_path, const std::string &key_path, dstring *error_dstr, bool is_recreate);
         static void mark_context_for_deletion(OpenSSLContext *context);

         ~OpenSSLContext();

         /**
          * @brief Was this context created for the server side of a connection?
          * @return true for a server context, false for a client one
          */
         bool get_is_server() { return is_server; }

         /// Register one more connection using this context
         void inc_connection_count() { connection_count++; }

         /// Drop one connection, and destroy any context left without users
         void dec_connection_count() { connection_count--; delete_no_longer_used_contexts(); }

         /**
          * @brief The underlying OpenSSL context
          * @return the `SSL_CTX`; owned by this object, do not free it
          */
         SSL_CTX *get_SSL_CTX() { return ssl_ctx; }
         const char *get_cert() const;

         /**
          * @brief Path of the certificate file this context was built from
          * @return the path; owned by this object, and empty for a context
          *         that keeps its certificate in memory only
          */
         const char *get_cert_file() { return cert_path.c_str(); }

         /**
          * @brief When the certificate should be replaced
          * @return a unix timestamp, or 0 when no renewal is scheduled
          */
         uint64_t get_renewal_time() { return renewal_time; }

         bool certificate_recreate_required() const;
         bool is_cert_file_updated();
      };

      /// result of one step of a non-blocking TLS handshake, see OpenSSLConnection::handshake()
      enum class HandshakeResult {
         DONE,       ///< the TLS session is established
         WANT_READ,  ///< call handshake() again when the socket is readable
         WANT_WRITE, ///< call handshake() again when the socket is writable
         FAILED      ///< the handshake failed
      };

      /**
       * @brief One TLS connection on an already connected socket
       *
       * Created with #create from a context, then given a file descriptor with
       * #set_fd. The server side calls #accept, the client side
       * #set_server_name_for_sni and #connect. Every method reports failure
       * through a `dstring` rather than a return code alone.
       */
      class OpenSSLConnection {
         OpenSSLContext *context;
         bool is_server;
         SSL *ssl;
         int fd;
         bool repeat_write;
         bool fatal_error; // SSL_ERROR_SYSCALL or SSL_ERROR_SSL occurred, no SSL_shutdown() then

         // private constructor, use the create() method
         OpenSSLConnection(OpenSSLContext *context, bool is_server, SSL *ssl)
         : context(context), is_server(is_server), ssl(ssl), fd(-1), repeat_write(false),
           fatal_error(false) {
            context->inc_connection_count();
         }
         bool wait_for_socket_ready(int reason, dstring *error_dstr) const;
         void note_error(int err);
         bool handshake_with_timeout(uint64_t timeout, dstring *error_dstr);

      public:
         static OpenSSLConnection *create(OpenSSLContext *context, dstring *error_dstr);
         ~OpenSSLConnection();

         /**
          * @brief The raw OpenSSL connection
          * @return the `SSL`; owned by this object, do not free it
          * @todo remove it — callers should go through this class
          */
         SSL *get_ssl() { return ssl; }
         bool set_fd(int new_fd, dstring *error_dstr);
         bool accept(dstring *error_dstr); // server side
         bool set_server_name_for_sni(const char *server_name, dstring *error_dstr) const; // client side
         bool connect(dstring *error_dstr);
         HandshakeResult handshake(dstring *error_dstr);
         int read(char *buffer, size_t max_len, dstring *error_dstr);
         int write(char *buffer, size_t len, dstring *error_dstr);
         /**
          * @brief Must the last #write be retried?
          *
          * OpenSSL can ask for a write to be repeated with the same buffer.
          *
          * @return true when the previous #write must be issued again
          */
         bool repeat_write_required() { return repeat_write; }
      };
   };
} // namespace ocs::uti
#endif
