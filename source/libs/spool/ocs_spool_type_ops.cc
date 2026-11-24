/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
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
 * @brief The spooling records of every field of every object type, for rows built at run time
 *
 * A row of a typed field table (spool/flatfile/ocs_spool_fields.h) gets the #ocs_spool_field_ops record of its
 * field at compile time. The rows built at run time - from a descriptor by spool_get_fields_to_spool(), from a
 * field number for the share tree's recursion - know their object type only at run time. This file holds, per
 * object type of the model, the array of the records of its fields, generated at compile time from the type's
 * field table by the ocs_spool_make_ops() the typed tables use, and the lookups that find the array of a
 * descriptor or the record of a field number. The arrays are data only: a record points at the accessor set of
 * its field type (ocs_spool_field_info.cc), so nothing here instantiates code per field.
 */

#include <array>
#include <cstddef>
#include <iterator>
#include <tuple>
#include <utility>

#include "tom/ocs_Types.h"

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_all.h"

#include "spool/sge_spooling_utilities.h"
#include "spool/ocs_spool_field_info.h"

namespace {

   /* *INDENT-OFF* */

   /**
    * @brief The field enumeration of every object type, in the order of ocs::tom::all_type_tables
    *
    * The enumeration is what turns a field number of the table into a field of the typed object model. The order
    * and the count are checked against the generated list at compile time, so a type added to the model without an
    * entry here does not compile.
    */
   using AllFields = std::tuple<
      JB_Field, JRS_Field, QU_Field, EH_Field, AN_Field, HL_Field, HS_Field, ET_Field,
      EV_Field, EVS_Field, CE_Field, LR_Field, OR_Field, OQ_Field, US_Field, UE_Field,
      RN_Field, PN_Field, VA_Field, MR_Field, PE_Field, QR_Field, JC_Field, CONF_Field,
      CF_Field, ST_Field, STU_Field, JG_Field, SO_Field, QAJ_Field, SPA_Field, REP_Field,
      UA_Field, PR_Field, UU_Field, GRU_Field, RESL_Field, STN_Field, SC_Field, PET_Field,
      PETR_Field, FPET_Field, JR_Field, LIC_Field, JL_Field, JP_Field, JO_Field, UPU_Field,
      CK_Field, PA_Field, JRE_Field, ID_Field, MA_Field, TE_Field, CAL_Field, CA_Field,
      TMR_Field, TM_Field, RT_Field, UPP_Field, SME_Field, MES_Field, JAT_Field, CT_Field,
      SGEJ_Field, ULNG_Field, HGRP_Field, HR_Field, PERM_Field, LS_Field, RU_Field, SU_Field,
      SPC_Field, SPR_Field, SPT_Field, SPTR_Field, JJ_Field, JJAT_Field, NSV_Field, ASTR_Field,
      AULNG_Field, ABOOL_Field, ATIME_Field, AMEM_Field, AINTER_Field, ASTRING_Field, ASTRLIST_Field, AUSRLIST_Field,
      APRJLIST_Field, ACELIST_Field, ASOLIST_Field, AQTLIST_Field, CQ_Field, QIM_Field, FCAT_Field, CTI_Field,
      PARA_Field, XMLA_Field, XMLH_Field, XMLS_Field, XMLE_Field, RDE_Field, RUE_Field, QETI_Field,
      LDR_Field, QRL_Field, CCT_Field, CQU_Field, SCT_Field, REF_Field, RQS_Field, RQR_Field,
      RQRF_Field, RQRL_Field, RQL_Field, AR_Field, ARA_Field, ACK_Field, EVR_Field, JSV_Field,
      RTIC_Field, PRO_Field, GR_Field, BN_Field, TEST_Field, PACK_Field, SPP_Field, RL_Field,
      HM_Field, CX_Field>;

   /* *INDENT-ON* */

   /// the number of object types
   constexpr std::size_t n_types = std::tuple_size_v<AllFields>;

   static_assert(n_types == std::size(ocs::tom::all_type_tables),
                 "AllFields names one field enumeration per object type of the model");

   /**
    * @brief The records of every field of one object type, in the order of its field table
    * @tparam E the field enumeration of the type
    * @tparam I the indices 0 .. number of fields - 1
    * @return one record per field, made by ocs_spool_make_ops() as the field's #ocs_spool_ops is
    */
   template <typename E, std::size_t... I>
   consteval std::array<ocs_spool_field_ops, sizeof...(I)> make_type_ops(std::index_sequence<I...>) {
      using T = typename ocs::tom::TypeOf<E>::type;
      return {ocs_spool_make_ops<static_cast<E>(T::fields[I].nm)>()...};
   }

   /**
    * @brief The records of every field of one object type, one constant array per type
    * @tparam E the field enumeration of the type
    */
   template <typename E>
   constexpr auto type_ops =
         make_type_ops<E>(std::make_index_sequence<std::size(ocs::tom::TypeOf<E>::type::fields)>{});

   /**
    * @brief The entry of one object type in #all_type_ops, checked against the generated list of the model
    * @tparam I the index of the type in ocs::tom::all_type_tables
    * @return the entry
    */
   template <std::size_t I>
   consteval ocs_spool_type_ops make_type_entry() {
      using E = std::tuple_element_t<I, AllFields>;
      using T = typename ocs::tom::TypeOf<E>::type;
      static_assert(ocs::tom::all_type_tables[I].descr == T::descriptor.e,
                    "AllFields is in the order of ocs::tom::all_type_tables");
      return {T::fields[0].nm, std::size(T::fields), type_ops<E>.data()};
   }

   /**
    * @brief Build #all_type_ops
    * @tparam I the indices 0 .. n_types - 1
    * @return one entry per object type, in the order of ocs::tom::all_type_tables
    */
   template <std::size_t... I>
   consteval std::array<ocs_spool_type_ops, sizeof...(I)> make_all_type_ops(std::index_sequence<I...>) {
      return {make_type_entry<I>()...};
   }

   /// the records of every field of every object type
   constexpr std::array<ocs_spool_type_ops, n_types> all_type_ops =
         make_all_type_ops(std::make_index_sequence<n_types>{});

   /**
    * @brief The entry of the object type a field number belongs to
    * @param nm the field number
    * @return the entry, nullptr if no type of the model has the field
    */
   const ocs_spool_type_ops *
   type_ops_of_field(const int nm) {
      for (const ocs_spool_type_ops &entry : all_type_ops) {
         if (nm >= entry.base && static_cast<std::size_t>(nm - entry.base) < entry.size) {
            return &entry;
         }
      }
      return nullptr;
   }

} // namespace

/**
 * @brief The spooling records of the fields of the object type a descriptor describes
 *
 * The type is found by the descriptor's first field, which a reduced descriptor keeps as well. No DENTER, as the
 * walkers' per field helpers have none.
 *
 * @param descr the descriptor, full or reduced
 * @return the records of the type's fields, nullptr if the descriptor is nullptr or of no type of the model
 */
const ocs_spool_type_ops *
ocs_spool_type_ops_of(const ocs::tom::FieldDescr *descr) {
   if (descr == nullptr || descr[0].nm == NoName) {
      return nullptr;
   }
   const ocs_spool_type_ops *entry = type_ops_of_field(descr[0].nm);
   if (entry == nullptr || entry->base != descr[0].nm) {
      return nullptr;
   }
   return entry;
}

/**
 * @brief The spooling record of a field given by its number
 *
 * No DENTER, as the walkers' per field helpers have none.
 *
 * @param nm the field number
 * @return the record, equal to the #ocs_spool_ops a typed field table points at; nullptr if no type of the model
 *         has the field
 */
const ocs_spool_field_ops *
ocs_spool_field_ops_of(const int nm) {
   const ocs_spool_type_ops *entry = type_ops_of_field(nm);
   return entry != nullptr ? &entry->ops[nm - entry->base] : nullptr;
}
