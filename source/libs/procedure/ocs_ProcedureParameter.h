#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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

#include <string>
#include <utility>

#include "cull/cull.h"
#include "gdi/ocs_gdi_Packet.h"

namespace ocs {
   /** @brief Base class for stored procedure arguments.
    *
    * Holds all information needed to describe and execute one procedure invocation:
    * the command name, optional sub-command, requested output format, execution
    * context, and the environment variables the procedure depends on.
    *
    * Parameters are marshaled to a CULL SPP_Type list (`get_bundle()`) for
    * transport in a GDI request, and reconstructed on the server side
    * (`set_bundle()`).
    *
    * @ingroup libprocedure
    */
   class ProcedureParameter {
#pragma region Data Types

   public:
      /** @brief Requested output format for the procedure result. */
      enum class OutputFormat : uint32_t {
         PLAIN, ///< Human-readable plain text
         XML,   ///< XML (compatible with legacy qstat -xml output)
         JSON   ///< JSON
      };

      /** @brief Where the procedure logic executes.
       *
       * - **CLIENT**: the client application fetches the required data from qmaster
       *   via GDI and processes and renders it locally.
       * - **SERVER**: the model runs inside qmaster and reads directly from the
       *   in-process master lists.  `packet_` carries the originating client's
       *   identity (user, host, permissions).
       */
      enum class ExecContext : uint32_t {
         CLIENT,
         SERVER
      };
#pragma endregion

#pragma region Data

   protected:
      std::string procedure_name_;     ///< Primary procedure name (e.g. "qstat")
      std::string sub_procedure_name_; ///< Sub-procedure, if the command supports one (e.g. "qselect", "cq-view")
      OutputFormat output_format_ = OutputFormat::PLAIN; ///< Requested output format
      ExecContext exec_context_ = ExecContext::SERVER;   ///< Execution context
      lList *env_variable_list_ = nullptr; ///< Environment variables required by the procedure
      gdi::Packet *packet_ = nullptr;      ///< Originating client packet; valid only in SERVER context

   public:
      [[nodiscard]] OutputFormat get_output_format() const { return output_format_; }
      [[nodiscard]] ExecContext get_exec_context() const { return exec_context_; }

      [[nodiscard]] const std::string &get_procedure_name() const { return procedure_name_; }
      [[nodiscard]] const std::string &get_sub_procedure_name() const { return sub_procedure_name_; }
      void set_sub_procedure_name(const std::string &procedure_name) { sub_procedure_name_ = procedure_name; }

      [[nodiscard]] const lList *get_env_variable_list() const { return env_variable_list_; }

      void add_variable(const char *name, const char *value);

      const char *get_variable(const char *name) const;

      /** @brief Return the originating client packet.
       *
       * Only valid when `get_exec_context() == ExecContext::SERVER`.
       * Provides user name, host, and permission flags of the requesting client.
       */
      [[nodiscard]] gdi::Packet *get_packet() const { return packet_; }

#pragma endregion

#pragma region Constants

   public:
      static constexpr auto NAME_VALUE_LIST = "name_value_list";
      static constexpr auto PROCEDURE = "procedure";
      static constexpr auto SUB_PROCEDURE = "sub_procedure";
      static constexpr auto RESPONSE = "response";
      static constexpr auto OUTPUT_FORMAT = "output_format";
      static constexpr auto ENVIRONMENT = "environment";
#pragma endregion

#pragma region Marshaling

   protected:
      /** @brief Populate this object from a received CULL SPP_Type parameter bundle.
       *
       * Called on the server side after the GDI request is received.
       */
      virtual void set_bundle(const lList *bundle);

   public:
      /** @brief Serialise this object into a CULL SPP_Type parameter bundle for GDI transport.
       *
       * The returned list is owned by the caller.
       */
      [[nodiscard]] virtual lList *get_bundle();

      static void add_parameter_bundle(lList *bundle, const std::string &name, lList *parameter);

      static std::string get_procedure_from_bundle(const lList *parameter_bundle);

      static std::string get_sub_procedure_from_bundle(const lList *parameter_bundle);
#pragma endregion

#pragma region Constructor/Destructor

   public:
      explicit ProcedureParameter(std::string procedure_name, gdi::Packet *packet) : procedure_name_(
         std::move(procedure_name)), packet_(packet) {
      };

      virtual ~ProcedureParameter() = default;
#pragma endregion
   };
}
