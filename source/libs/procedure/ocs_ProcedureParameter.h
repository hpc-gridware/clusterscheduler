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

/** @file
 * @brief Base of the parameter objects: a command's arguments, format and context
 */

#include <string>
#include <utility>

#include "tom/ocs_Types.h"
#include "gdi/ocs_gdi_Packet.h"
#include "tom/ocs_FreeAccess.h"
#include "tom/ocs_List.h"
#include "uti/sge_log.h"
#include "sgeobj/tom/ocs_tom_SPP.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_ULNG.h"
#include "sgeobj/tom/ocs_tom_VA.h"

namespace ocs {
   /** @brief Base class for stored procedure arguments.
    *
    * Holds all information needed to describe and execute one procedure invocation:
    * the command name, optional sub-command, requested output format, execution
    * context, and the environment variables the procedure depends on.
    *
    * Parameters are marshaled to a CULL SPP list (`get_bundle()`) for
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
         CLIENT,   ///< The client fetches the data over GDI and renders it itself
         SERVER    ///< The model runs inside qmaster, against the in-process master lists
      };
#pragma endregion

#pragma region Data

   protected:
      std::string procedure_name_;     ///< Primary procedure name (e.g. "qstat")
      std::string sub_procedure_name_; ///< Sub-procedure, if the command supports one (e.g. "qselect", "cq-view")
      OutputFormat output_format_ = OutputFormat::PLAIN; ///< Requested output format
      ExecContext exec_context_ = ExecContext::SERVER;   ///< Execution context
      ocs::tom::ListPtr<ocs::tom::VA> env_variable_list_; ///< Environment variables required by the procedure
      gdi::Packet *packet_ = nullptr;      ///< Originating client packet; valid only in SERVER context

   public:
      /** @brief The requested output format
       * @return plain text, XML or JSON
       */
      [[nodiscard]] OutputFormat get_output_format() const { return output_format_; }

      /** @brief Whether the model runs in the client or inside qmaster
       * @return the execution context
       */
      [[nodiscard]] ExecContext get_exec_context() const { return exec_context_; }

      /** @brief The command being run
       * @return the primary procedure name, e.g. `"qstat"`
       */
      [[nodiscard]] const std::string &get_procedure_name() const { return procedure_name_; }

      /** @brief The sub-command, for commands that have one
       * @return the sub-procedure name, empty when there is none
       */
      [[nodiscard]] const std::string &get_sub_procedure_name() const { return sub_procedure_name_; }

      /** @brief Set the sub-command
       * @param procedure_name the sub-procedure name
       */
      void set_sub_procedure_name(const std::string &procedure_name) { sub_procedure_name_ = procedure_name; }

      void add_variable(const char *name, const char *value);

      const char *get_variable(const char *name) const;

      /** @brief Return the originating client packet.
       *
       * Only valid when `get_exec_context() == ExecContext::SERVER`.
       * Provides user name, host, and permission flags of the requesting client.
       *
       * @return the packet, or nullptr in CLIENT context
       */
      [[nodiscard]] gdi::Packet *get_packet() const { return packet_; }

#pragma endregion

#pragma region Constants

   public:
      /** @name Names of the entries in the marshalled parameter bundle
       *
       * A bundle is a CULL list keyed by these strings, so both sides have to
       * agree on them - they are the wire format of a procedure call.
       * @{
       */
      static constexpr auto NAME_VALUE_LIST = "name_value_list";   ///< The generic name/value entries
      static constexpr auto PROCEDURE = "procedure";               ///< The command being run
      static constexpr auto SUB_PROCEDURE = "sub_procedure";       ///< Its sub-command, if any
      static constexpr auto RESPONSE = "response";                 ///< The rendered response, on the way back
      static constexpr auto REDUCTION = "reduction";               ///< What RBAC removed from that response

      /** @name What a response had removed from it by RBAC (CS-2821)
       *
       * A bit mask, zero when nothing was removed. The two bits are the two
       * mechanisms the specification names, and they are deliberately separate
       * although both mean the same thing to the write guard today: more may
       * follow, and a mask leaves room for it without touching the wire format.
       *
       * The distinction is not cosmetic. Masking keeps the structure and
       * substitutes placeholder values, so an object that looks complete is not;
       * writing it back would store the placeholder. Hiding leaves things out, so
       * the structure itself is short. Either way the response is not a faithful
       * copy of what qmaster holds, which is why the editor may not run on it -
       * see CS-2823.
       *
       * @{
       */
      static constexpr uint32_t REDUCTION_NONE = 0;        ///< nothing was removed
      static constexpr uint32_t REDUCTION_MASKED = 1;      ///< placeholder values, structure intact
      static constexpr uint32_t REDUCTION_HIDDEN = 2;      ///< data left out, structure shortened
      /** @} */

      /** @brief Environment variable that makes a response claim it was reduced
       *
       * Nothing produces a reduction yet - RBAC redaction is still ahead - so the
       * one behaviour CS-2823 adds, refusing to offer a reduced object to the
       * editor, cannot be reached and therefore cannot be tested. A client that
       * finds this variable set ships its value with the request and qmaster
       * answers with the mask it names.
       *
       * It grants nothing: the mask only ever makes the caller's own client
       * refuse more than it otherwise would, and qmaster acts on it nowhere else.
       *
       * Values are `masked`, `hidden`, or both separated by a comma.
       */
      static constexpr auto FORCE_REDUCTION_VARIABLE = "SGE_PROCEDURE_FORCE_REDUCTION";
      static constexpr auto OUTPUT_FORMAT = "output_format";       ///< Plain, XML or JSON
      static constexpr auto ENVIRONMENT = "environment";           ///< The environment variables

      /** @name What a write did, on the way back (CS-2822)
       *
       * Two counts beside the rendered response, for the same reason the
       * reduction mask is beside it: they are a verdict on the call, not part of
       * its output, and the caller has to act on them.
       *
       * A set write reports per item through the answer list, and the caller can
       * count the failures there - but it cannot tell *which phase* they come
       * from, and `-strict` turns exactly on that: one item that could not be
       * **read** rejects the batch, while one that could not be **applied**
       * does not. Counting error entries cannot separate the two, so the side
       * that knows says so.
       * @{
       */
      static constexpr auto WRITE_APPLIED = "write_applied";       ///< How many items a write applied
      static constexpr auto WRITE_PARSE_FAILED = "write_parse_failed"; ///< How many could not be read
      static constexpr auto WRITE_ELEMENTS = "write_elements";     ///< How many objects that was -
                                                                   ///< more than one per item for a
                                                                   ///< type whose text holds a list
      /** @} */

      /** @name What an export procedure produced (CS-2826)
       *
       * One entry per exported object, as a name/value pair: the object's name
       * and the text of the file it becomes. A structured entry rather than the
       * one RESPONSE text, because the caller writes one file per object and
       * has to know which name each text belongs to - splitting a concatenated
       * text on a separator would mean a second parser for a format the writer
       * already knows the boundaries of.
       *
       * Sent only by a read that was asked for the export form, so an ordinary
       * read's response is unchanged.
       * @{
       */
      static constexpr auto EXPORT_OBJECTS = "export_objects";     ///< name -> file text, in fetch order
      /** @} */
      /** @} */
#pragma endregion

#pragma region Marshaling

   protected:

      /** @brief Populate this object from a received parameter bundle
       * @param bundle the received bundle; value lists the object keeps are moved out of it
       * @return none
       */
      virtual void set_bundle(ocs::tom::List<ocs::tom::SPP> bundle);

      /** @brief The text of a bundle value, with the wire format's empty string rule applied
       *
       * `ocs::tom::get_str()` maps an empty string to `nullptr` on purpose
       * (`libs/tom/ocs_PackCore.cc`), so a value that was empty in the client arrives here
       * as a null pointer. Building a `std::string` from it throws
       * `std::logic_error`, which nothing catches and `TerminationManager` turns
       * into `abort()` - one `qconf -scat ''` took the whole qmaster down
       * (CS-2840). Every bundle value that becomes a string goes through here.
       *
       * @param text the value as CULL returns it, possibly nullptr
       * @return the text, or an empty string when it was empty on the wire
       */
      [[nodiscard]] static std::string bundle_string(const char *text) {
         return text != nullptr ? std::string{text} : std::string{};
      }

   public:
      /** @brief Serialise this object into a parameter bundle for GDI transport
       *
       * @return the bundle, which the caller owns
       */
      [[nodiscard]] virtual ocs::tom::ListPtr<ocs::tom::SPP> get_bundle();

      /** @brief Add one named entry to a bundle
       *
       * The value list of an entry is of any type, so the type of
       * the list handed in is the caller's.
       *
       * @tparam T the object type of the value list
       * @param bundle the bundle to add to
       * @param name the entry's name, one of the constants above
       * @param parameter the entry's value, taken over by the bundle; may be empty
       * @return none
       */
      template <ocs::tom::ObjectType T>
      static void add_parameter_bundle(const ocs::tom::List<ocs::tom::SPP> bundle, const std::string &name,
                                       ocs::tom::ListPtr<T> &&parameter) {
         auto value = std::move(parameter);
         SGE_ASSERT(bundle);
         const auto bundle_elem = bundle.emplace_back<SPP_name>(name.c_str());
         bundle_elem.set<SPP_value_list>(std::move(value));
      }


      /** @brief A value list holding one number, as a bundle entry carries it
       * @param value the number
       * @return the list, named as the free emplace_back() named it
       */
      [[nodiscard]] static ocs::tom::ListPtr<ocs::tom::ULNG> ulong_value_list(uint32_t value);

      /** @brief A value list holding one text, as a bundle entry carries it
       * @param value the text; nullptr gives no list, as the free emplace_back() did
       * @return the list, or an empty handle for nullptr
       */
      [[nodiscard]] static ocs::tom::ListPtr<ocs::tom::ST> string_value_list(const char *value);

      /** @brief The value list of a bundle entry, read only
       *
       * The value list of an entry is of any type: the caller names
       * the type the sender put in, and the list is wrapped without the descriptor check, so that an empty list
       * of another type from a broken client is ignored in qmaster instead of throwing (get_as() would throw).
       *
       * @tparam T the object type of the value list
       * @param entry the bundle entry, not empty
       * @return the value list, empty if the entry holds none
       */
      template <ocs::tom::ObjectType T>
      [[nodiscard]] static ocs::tom::ConstList<T> value_list(const ocs::tom::ConstElement<ocs::tom::SPP> entry) {
         return ocs::tom::ConstList<T>::from_typed(entry.get<SPP_value_list>());
      }

      /** @brief Move the value list out of a bundle entry
       *
       * The list is taken over rather than copied: it is the request's payload and
       * the bundle is freed right after the parameters are built. It is wrapped
       * without the descriptor check, as value_list() does.
       *
       * @tparam T the object type of the value list
       * @param entry the bundle entry, or an empty handle (the entry is optional), which gives an empty handle
       * @return the value list, an empty handle if the entry holds none
       */
      template <ocs::tom::ObjectType T>
      [[nodiscard]] static ocs::tom::ListPtr<T> take_value_list(const ocs::tom::Element<ocs::tom::SPP> entry) {
         if (!entry) {
            return {};
         }
         return ocs::tom::ListPtr<T>::from_typed(entry.exchange<SPP_value_list>(ocs::tom::ListPtr<T>{}).release());
      }

      static std::string get_procedure_from_bundle(ocs::tom::ConstList<ocs::tom::SPP> parameter_bundle);

      static std::string get_sub_procedure_from_bundle(ocs::tom::ConstList<ocs::tom::SPP> parameter_bundle);
#pragma endregion

#pragma region Constructor/Destructor

   public:
      /** @brief Build the parameters of one procedure call
       * @param procedure_name the command being run
       * @param packet the originating client packet; nullptr in CLIENT context
       */
      explicit ProcedureParameter(std::string procedure_name, gdi::Packet *packet) : procedure_name_(
         std::move(procedure_name)), packet_(packet) {
      };

      virtual ~ProcedureParameter() = default;
#pragma endregion
   };
}
