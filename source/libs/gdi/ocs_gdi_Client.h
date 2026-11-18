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
 *  Portions of this software are Copyright (c) 2024-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The client side entry points of the GDI
 */

#include <tuple>

#include "cull/cull.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Select.h"
#include "tom/ocs_Transitional.h"
#include "sgeobj/tom/ocs_tom_CONF.h"

#include "gdi/ocs_gdi_Command.h"
#include "gdi/ocs_gdi_ClientBase.h"
#include "gdi/ocs_gdi_Target.h"
#include "sgeobj/sge_answer.h"

/* from gdi_tsm.h */
#define MASTER_KILL       (1<<0) ///< `gdi_kill()` target: shut the qmaster down
#define SCHEDD_KILL       (1<<1) ///< `gdi_kill()` target: shut the scheduler down
#define EXECD_KILL        (1<<2) ///< `gdi_kill()` target: shut execution daemons down
#define JOB_KILL          (1<<3) ///< `gdi_kill()` target: kill jobs
#define EVENTCLIENT_KILL  (1<<4) ///< `gdi_kill()` target: shut event clients down
#define THREAD_START      (1<<5) ///< `gdi_kill()` target: start a qmaster thread instead of killing

namespace ocs::gdi {
   /**
    * @brief The GDI calls an ordinary client makes
    *
    * Everything here goes to qmaster and waits for the answer.
    */
   class Client : public ClientBase {
   public:
      OCS_TRANSITIONAL static ocs::tom::ListPtr<ocs::tom::AN> sge_gdi(Target target, Command cmd, SubCommand,
                                                                      lList **lpp, lCondition *cp,
                                                               lEnumeration *enp);

      template <ocs::tom::ObjectType T>
      static ocs::tom::ListPtr<ocs::tom::AN> sge_gdi(Target target, Command cmd, SubCommand sub_cmd,
                                                     ocs::tom::ListPtr<T> &data,
                                              const ocs::tom::Where<T> &where, const ocs::tom::What<T> &what);

      template <ocs::tom::ObjectType T>
      static ocs::tom::ListPtr<ocs::tom::AN> sge_gdi(Target target, Command cmd, SubCommand sub_cmd,
                                                     ocs::tom::ListPtr<T> &data);

      static ocs::tom::ListPtr<ocs::tom::AN> gdi_tsm();
      static ocs::tom::ListPtr<ocs::tom::AN> gdi_kill(lList *id_list, uint32_t action_flag);
      static bool sge_gdi_get_permission(ocs::tom::ListPtr<ocs::tom::AN> *alpp, bool *is_manager, bool *is_operator,
                                         bool *is_admin_host, bool *is_submit_host);
      static std::tuple<int, ocs::tom::ElementPtr<ocs::tom::CONF>, ocs::tom::ElementPtr<ocs::tom::CONF>>
      gdi_get_configuration(const char *config_name, bool with_local);
   };

   /**
    * @brief Send a single-task GDI request and wait for the answer, with typed data, condition and selection
    *
    * The request list stays with @p data and is copied for sending. When the
    * command answers with objects (see #get_response_from_master()), @p data
    * holds the answer afterwards and the request list is freed; otherwise it is
    * left as it was.
    *
    * @tparam T the object type of the target
    * @param target which object list to act on
    * @param cmd what to do
    * @param sub_cmd modifiers refining @p cmd
    * @param data the objects to send, an empty handle for none; receives the ones read back
    * @param where which objects to act on, an empty condition for all
    * @param what which fields to transfer, an empty selection for all
    * @return the answer list; the caller owns it
    */
   template <ocs::tom::ObjectType T>
   ocs::tom::ListPtr<ocs::tom::AN> Client::sge_gdi(const Target target, const Command cmd, const SubCommand sub_cmd,
                                            ocs::tom::ListPtr<T> &data, const ocs::tom::Where<T> &where,
                                            const ocs::tom::What<T> &what) {
      lList *const request = data.get();
      lList *lp = request;
      // the request copies the condition and the selection, it does not change them
      auto alp = sge_gdi(target, cmd, sub_cmd, &lp, const_cast<lCondition *>(where.get()),
                           const_cast<lEnumeration *>(what.get()));
      if (lp != request) {
         // the answer of a task is a list of the target's type (reduced by the selection) that qmaster packed;
         // it is unpacked with the descriptor it travelled with, so it is taken over unchecked
         data = ocs::tom::ListPtr<T>::from_typed(lp);
      }
      return alp;
   }

   /**
    * @brief Send a single-task GDI request without condition and selection and wait for the answer
    *
    * @tparam T the object type of the target
    * @param target which object list to act on
    * @param cmd what to do
    * @param sub_cmd modifiers refining @p cmd
    * @param data the objects to send, an empty handle for none; receives the ones read back, see the form with
    *        a condition
    * @return the answer list; the caller owns it
    */
   template <ocs::tom::ObjectType T>
   ocs::tom::ListPtr<ocs::tom::AN> Client::sge_gdi(const Target target, const Command cmd, const SubCommand sub_cmd,
                                            ocs::tom::ListPtr<T> &data) {
      return sge_gdi(target, cmd, sub_cmd, data, ocs::tom::Where<T>(), ocs::tom::What<T>());
   }
}
