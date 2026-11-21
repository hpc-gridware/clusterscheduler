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
 * @brief Unit tests for flatfile in `libs/spool`
 */

#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/wait.h>

#include "uti/ocs_Bootstrap.h"
#include "uti/sge.h"
#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_unistd.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/tom/ocs_tom_ARA.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_MR.h"
#include "sgeobj/tom/ocs_tom_ABOOL.h"
#include "sgeobj/tom/ocs_tom_ACELIST.h"
#include "sgeobj/tom/ocs_tom_AINTER.h"
#include "sgeobj/tom/ocs_tom_AMEM.h"
#include "sgeobj/tom/ocs_tom_APRJLIST.h"
#include "sgeobj/tom/ocs_tom_AQTLIST.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_ASOLIST.h"
#include "sgeobj/tom/ocs_tom_ASTR.h"
#include "sgeobj/tom/ocs_tom_ASTRLIST.h"
#include "sgeobj/tom/ocs_tom_ATIME.h"
#include "sgeobj/tom/ocs_tom_AULNG.h"
#include "sgeobj/tom/ocs_tom_AUSRLIST.h"
#include "sgeobj/tom/ocs_tom_CAL.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CF.h"
#include "sgeobj/tom/ocs_tom_CK.h"
#include "sgeobj/tom/ocs_tom_CONF.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_HL.h"
#include "sgeobj/tom/ocs_tom_HS.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/tom/ocs_tom_RQR.h"
#include "sgeobj/tom/ocs_tom_RQRF.h"
#include "sgeobj/tom/ocs_tom_RQRL.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "sgeobj/tom/ocs_tom_RU.h"
#include "sgeobj/tom/ocs_tom_RUE.h"
#include "sgeobj/tom/ocs_tom_SC.h"
#include "sgeobj/tom/ocs_tom_SO.h"
#include "sgeobj/tom/ocs_tom_SPC.h"
#include "sgeobj/tom/ocs_tom_SPR.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_STN.h"
#include "sgeobj/tom/ocs_tom_STU.h"
#include "sgeobj/tom/ocs_tom_UA.h"
#include "sgeobj/tom/ocs_tom_UE.h"
#include "sgeobj/tom/ocs_tom_UPP.h"
#include "sgeobj/tom/ocs_tom_UPU.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_UU.h"
#include "tom/ocs_Element.h"
#include "spool/sge_spooling_utilities.h"
#include "spool/flatfile/sge_flatfile.h"
#include "spool/flatfile/sge_flatfile_obj.h"
#include "spool/flatfile/sge_spooling_flatfile.h"
#include "spool/flatfile/sge_spooling_flatfile_scanner.h"

#include <sge_log.h>

/** @brief Let the test rewrite an object's delete time
 *
 * Not something the product allows; the test needs it to reach the code that
 * runs when a spooled object has already expired.
 */
#define allow_delete_time_modification

static int s_fail = 0;

/** @def CHECK
 * @brief Assert one condition and record the result
 *
 * Prints `PASS`/`FAIL` with the test's id and label and counts the failure, so
 * a run reports every problem rather than stopping at the first.
 *
 * @param id the test number, printed as `[Tnn]`
 * @param label what the check is about, printed on failure
 * @param expr the condition that must hold
 */
#define CHECK(id, label, expr) \
   do { \
      if (!(expr)) { \
         printf("FAIL  [T%02d] %s\n", (id), (label)); \
         ++s_fail; \
      } else { \
         printf("ok    [T%02d] %s\n", (id), (label)); \
      } \
   } while (0)

static int diff(const char *file1, const char *file2);
static int PE_test();
static int CAL_test();
static int CK_test();
static int STN_test();
static int CE_test();
static int CEL_test(); 
static int UU_test();
static int PR_test();
static int US_test();
static int EH_test();
static int CQ_test();
static int SC_test();
static int QU_test();
static int HGRP_test();
static int CONF_test();
static int RQS_test();
static int AR_test();
static int PE_minimal_test();
static int CE_minimal_test();
static int PE_zero_slots_test();
static int PE_max_slots_test();
static int CE_zero_values_test();
static int CE_max_values_test();
static int PE_spaces_test();
static int CE_special_test();
static int PE_empty_lists_test();
static int PE_large_list_test();
static int SPOOL_perm_test();
static int SPOOL_dir_perm_test();
static int SPOOL_restrict_existing_test();
static int SPOOL_key_safe_test();
static int SPOOL_classic_params_test();

/** @brief One test case: takes nothing, returns non-zero on failure */
typedef int (*func)();

int main(int argc, char **argv) {
   DENTER_MAIN(TOP_LAYER, "test_ff_cl");

   if (argc > 1) {
      int ret;
      while ((ret = spool_lex())) {
         printf("line %3d: token %3d: %s\n", spool_line, ret, spool_text);
      }
      DRETURN(0);
   }

   // Resolve the bootstrap configuration while log output still reaches stderr.
   // Several of the checks below build CULL objects with hashed host fields, and
   // hashing a host key goes through sge_hostcpy(), which needs the ignore_fqdn
   // bootstrap setting. That lookup aborts the process when SGE_ROOT is unset,
   // and once we are daemonized sge_log() no longer writes to stderr - the test
   // would then die without any indication why. Touching it here keeps the
   // critical error visible while the checks themselves stay free of log noise.
   ocs::Bootstrap::get_ignore_fqdn();

   component_set_daemonized(true);
   ocs::tom::install_model(ocs::tom::gcs_model());

   int id = 1;
   CHECK(id, "PE: write-read-write round-trip produces identical output",   PE_test()   == 0); id++;
   CHECK(id, "CAL: write-read-write round-trip produces identical output",  CAL_test()  == 0); id++;
   CHECK(id, "CK: write-read-write round-trip produces identical output",   CK_test()   == 0); id++;
   CHECK(id, "STN: write-read-write round-trip produces identical output",  STN_test()  == 0); id++;
   CHECK(id, "CE: write-read-write round-trip produces identical output",   CE_test()   == 0); id++;
   CHECK(id, "CEL: write-read-write round-trip produces identical output",  CEL_test()  == 0); id++;
   CHECK(id, "UU: write-read-write round-trip produces identical output",   UU_test()   == 0); id++;
   CHECK(id, "PR: write-read-write round-trip produces identical output",   PR_test()   == 0); id++;
   CHECK(id, "EH: write-read-write round-trip produces identical output",   EH_test()   == 0); id++;
   CHECK(id, "US: write-read-write round-trip produces identical output",   US_test()   == 0); id++;
   CHECK(id, "CQ: write-read-write round-trip produces identical output",   CQ_test()   == 0); id++;
   CHECK(id, "SC: write-read-write round-trip produces identical output",   SC_test()   == 0); id++;
   CHECK(id, "QU: write-read-write round-trip produces identical output",   QU_test()   == 0); id++;
   CHECK(id, "HGRP: write-read-write round-trip produces identical output", HGRP_test() == 0); id++;
   CHECK(id, "CONF: write-read-write round-trip produces identical output", CONF_test() == 0); id++;
   CHECK(id, "AR: write-read-write round-trip produces identical output",   AR_test()   == 0); id++;
   CHECK(id, "RQS: write-read-write round-trip produces identical output",  RQS_test()  == 0); id++;
   CHECK(id, "PE minimal: only name set, all other fields at default",           PE_minimal_test()      == 0); id++;
   CHECK(id, "CE minimal: name, shortcut, and lowest valid enum values",          CE_minimal_test()      == 0); id++;
   CHECK(id, "PE boundary: slots=0 round-trips correctly",                       PE_zero_slots_test()   == 0); id++;
   CHECK(id, "PE boundary: slots=4294967295 (UINT32 max) round-trips correctly", PE_max_slots_test()    == 0); id++;
   CHECK(id, "CE boundary: consumable=NO(0) and string fields at \"0\" round-trip", CE_zero_values_test()  == 0); id++;
   CHECK(id, "CE boundary: large urgency_weight string round-trips correctly",   CE_max_values_test()   == 0); id++;
   CHECK(id, "PE special chars: spaces in proc args round-trip correctly",       PE_spaces_test()       == 0); id++;
   CHECK(id, "CE special chars: decimal and scientific notation in fields",      CE_special_test()      == 0); id++;
   CHECK(id, "PE empty lists: explicitly empty user_list and xuser_list",        PE_empty_lists_test()  == 0); id++;
   CHECK(id, "PE large list: 10-element user_list round-trips correctly",        PE_large_list_test()   == 0); id++;
   CHECK(id, "SPOOL perm: SP_DEST_SPOOL file created 0600 (not world/group accessible)", SPOOL_perm_test() == 0); id++;
   CHECK(id, "SPOOL dir perm: classic spool directory created 0700 (not world/group traversable)", SPOOL_dir_perm_test() == 0); id++;
   CHECK(id, "SPOOL restrict: startup restricts an existing spool tree to the owner", SPOOL_restrict_existing_test() == 0); id++;
   CHECK(id, "SPOOL key: spool_flatfile_key_is_safe accepts relative multi-component keys, rejects traversal", SPOOL_key_safe_test() == 0); id++;
   CHECK(id, "SPOOL params: classic create_context rejects legacy 2-arg and non-absolute spooling_params", SPOOL_classic_params_test() == 0); id++;

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}

/**
 * @brief Write a parallel environment with resource utilization and user lists with classic spooling, read it back
 *        and write it again
 * @return 0 if the file written first and the file written after the read are equal, non-zero otherwise
 */
static int PE_test() {
   int ret = 0;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   auto ep = ocs::tom::ElementPtr<ocs::tom::PE>::create();
   const char *file1 = nullptr;
   const char *file2 = nullptr;
   
   /* Build a PE structure */
   
   ep->set<PE_name>("Test_Name");
   ep->set<PE_slots>(7);
   ep->set<PE_start_proc_args>("start_args");
   ep->set<PE_stop_proc_args>("stop_args");
   ep->set<PE_allocation_rule>("allocation_rule");
   ep->set<PE_control_slaves>(true);
   ep->set<PE_job_is_first_task>(true);
   ep->set<PE_urgency_slots>("urgency_slots");

   auto lp = ocs::tom::ListPtr<ocs::tom::RUE>::create("Resource Utilization List");
   
   auto ep2 = ocs::tom::ElementPtr<ocs::tom::RUE>::create();
   ep2->set<RUE_name>("Test_Name2");
   ep2->set<RUE_utilized_now>(12.345);
   lp->push_back(std::move(ep2));
   
   auto ep2_rue = ocs::tom::ElementPtr<ocs::tom::RUE>::create();
   ep2_rue->set<RUE_name>("Test_Name3");
   ep2_rue->set<RUE_utilized_now>(678.9);
   lp->push_back(std::move(ep2_rue));
   
   ep->set<PE_resource_utilization>(std::move(lp));
   
   auto lp_us = ocs::tom::ListPtr<ocs::tom::US>::create("User_List");

   auto ep2_us = ocs::tom::ElementPtr<ocs::tom::US>::create();   
   ep2_us->set<US_name>("First_User_List");
   ep2_us->set<US_type>(101);
   ep2_us->set<US_fshare>(303);
   ep2_us->set<US_oticket>(505);
   ep2_us->set<US_job_cnt>(707);
   ep2_us->set<US_pending_job_cnt>(909);
   
   auto lp2 = ocs::tom::ListPtr<ocs::tom::UE>::create("User_Entry_List");
   
   auto ep3 = ocs::tom::ElementPtr<ocs::tom::UE>::create();
   ep3->set<UE_name>("First_User");
   lp2->push_back(std::move(ep3));
   
   auto ep3_ue = ocs::tom::ElementPtr<ocs::tom::UE>::create();
   ep3_ue->set<UE_name>("Second_User");
   lp2->push_back(std::move(ep3_ue));

   ep2_us->set<US_entries>(std::move(lp2));
   
   lp_us->push_back(std::move(ep2_us));
   
   auto ep2_us_2 = ocs::tom::ElementPtr<ocs::tom::US>::create();   
   ep2_us_2->set<US_name>("Second_User_List");
   ep2_us_2->set<US_type>(202);
   ep2_us_2->set<US_fshare>(404);
   ep2_us_2->set<US_oticket>(606);
   ep2_us_2->set<US_job_cnt>(808);
   ep2_us_2->set<US_pending_job_cnt>(0);
   
   auto lp2_ue = ocs::tom::ListPtr<ocs::tom::UE>::create("User_Entry_List");
   
   auto ep3_ue_2 = ocs::tom::ElementPtr<ocs::tom::UE>::create();
   ep3_ue_2->set<UE_name>("Third_User");
   lp2_ue->push_back(std::move(ep3_ue_2));
   
   auto ep3_ue_3 = ocs::tom::ElementPtr<ocs::tom::UE>::create();
   ep3_ue_3->set<UE_name>("Fourth_User");
   lp2_ue->push_back(std::move(ep3_ue_3));

   ep2_us_2->set<US_entries>(std::move(lp2_ue));
   
   lp_us->push_back(std::move(ep2_us_2));
   
   ep->set<PE_user_list>(std::move(lp_us));
   
   auto lp_us_2 = ocs::tom::ListPtr<ocs::tom::US>::create("XUser_List");
   
   auto ep2_us_3 = ocs::tom::ElementPtr<ocs::tom::US>::create();   
   ep2_us_3->set<US_name>("First_XUser_List");
   ep2_us_3->set<US_type>(101);
   ep2_us_3->set<US_fshare>(303);
   ep2_us_3->set<US_oticket>(505);
   ep2_us_3->set<US_job_cnt>(707);
   ep2_us_3->set<US_pending_job_cnt>(909);
   
   auto lp2_ue_2 = ocs::tom::ListPtr<ocs::tom::UE>::create("XUser_Entry_List");
   
   auto ep3_ue_4 = ocs::tom::ElementPtr<ocs::tom::UE>::create();
   ep3_ue_4->set<UE_name>("First_XUser");
   lp2_ue_2->push_back(std::move(ep3_ue_4));
   
   auto ep3_ue_5 = ocs::tom::ElementPtr<ocs::tom::UE>::create();
   ep3_ue_5->set<UE_name>("Second_XUser");
   lp2_ue_2->push_back(std::move(ep3_ue_5));

   ep2_us_3->set<US_entries>(std::move(lp2_ue_2));
   
   lp_us_2->push_back(std::move(ep2_us_3));
   
   auto ep2_us_4 = ocs::tom::ElementPtr<ocs::tom::US>::create();   
   ep2_us_4->set<US_name>("Second_XUser_List");
   ep2_us_4->set<US_type>(202);
   ep2_us_4->set<US_fshare>(404);
   ep2_us_4->set<US_oticket>(606);
   ep2_us_4->set<US_job_cnt>(808);
   ep2_us_4->set<US_pending_job_cnt>(0);
   
   auto lp2_ue_3 = ocs::tom::ListPtr<ocs::tom::UE>::create("XUser_Entry_List");
   
   auto ep3_ue_6 = ocs::tom::ElementPtr<ocs::tom::UE>::create();
   ep3_ue_6->set<UE_name>("Third_XUser");
   lp2_ue_3->push_back(std::move(ep3_ue_6));
   
   auto ep3_ue_7 = ocs::tom::ElementPtr<ocs::tom::UE>::create();
   ep3_ue_7->set<UE_name>("Fourth_XUser");
   lp2_ue_3->push_back(std::move(ep3_ue_7));

   ep2_us_4->set<US_entries>(std::move(lp2_ue_3));
   
   lp_us_2->push_back(std::move(ep2_us_4));
   
   ep->set<PE_xuser_list>(std::move(lp_us_2));

   /* Write a PE file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, *ep, false,
                                       PE_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, false);

   /* Read a PE file using flatfile spooling */
   ep.reset();
   ep = ocs::tom::ElementPtr<ocs::tom::PE>(spool_flatfile_read_object(&alp, ocs::tom::PE::descriptor.e, nullptr,
                                   PE_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1));

   /* Write a PE file using flatfile spooling */
   file2 = spool_flatfile_write_object(&alp, *ep, false,
                                       PE_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file2, false);

   ep.reset();
   ret = diff(file1, file2);
   
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);

   answer_list_output(&alp);   

   return ret;
}

/**
 * @brief Write a calendar with classic spooling, read it back and write it again
 * @return 0 if the file written first and the file written after the read are equal, non-zero otherwise
 */
static int CAL_test() {
   int ret = 0;
   ocs::tom::ElementPtr<ocs::tom::CAL> ep;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   const char *file1 = nullptr, *file2 = nullptr;
   
   auto ep_cal = ocs::tom::ElementPtr<ocs::tom::CAL>::create();   
   ep_cal->set<CAL_name>("Test_Name");
   ep_cal->set<CAL_year_calendar>("2004");
   ep_cal->set<CAL_week_calendar>("KW 05");

   /* Write a CAL file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, *ep_cal, false,
                                       CAL_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, false);
   
   /* Read a CAL file using flatfile spooling */
   ep_cal.reset();
   ep = ocs::tom::ElementPtr<ocs::tom::CAL>(spool_flatfile_read_object(&alp, ocs::tom::CAL::descriptor.e, nullptr,
                                   CAL_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1));
   
   /* Write a CAL file using flatfile spooling */
   file2 = spool_flatfile_write_object(&alp, *ep, false,
                                       CAL_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file2, false);
   
   ep.reset();
   
   ret = diff(file1, file2);
   
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   
   answer_list_output(&alp);   
   alp.reset();
   
   return ret;
}

/**
 * @brief Write a checkpointing environment with classic spooling, read it back and write it again
 * @return 0 if the file written first and the file written after the read are equal, non-zero otherwise
 */
static int CK_test() {
   int ret = 0;
   ocs::tom::ElementPtr<ocs::tom::CK> ep;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   const char *file1 = nullptr, *file2 = nullptr;
   
   auto ep_ck = ocs::tom::ElementPtr<ocs::tom::CK>::create();
   ep_ck->set<CK_name>("Test_Name");
   ep_ck->set<CK_interface>("interface");
   ep_ck->set<CK_ckpt_command>("ckpt_command");
   ep_ck->set<CK_migr_command>("migr_command");
   ep_ck->set<CK_rest_command>("rest_command");
   ep_ck->set<CK_ckpt_dir>("ckpt_dir");
   ep_ck->set<CK_when>("when");
   ep_ck->set<CK_signal>("signal");
   ep_ck->set<CK_job_pid>(1313);
   ep_ck->set<CK_clean_command>("clean_command");
   
   /* Write a CK file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, *ep_ck, false,
                                       CK_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, false);
   
   /* Read a CK file using flatfile spooling */
   ep_ck.reset();
   ep = ocs::tom::ElementPtr<ocs::tom::CK>(spool_flatfile_read_object(&alp, ocs::tom::CK::descriptor.e, nullptr,
                                   CK_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1));
      
   /* Write a CK file using flatfile spooling */
   file2 = spool_flatfile_write_object(&alp, *ep, false,
                                       CK_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file2, false);
   
   ep.reset();
   
   ret = diff(file1, file2);
   
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   
   answer_list_output(&alp);   

   return ret;
}

/**
 * @brief Write a share tree node in the name=value format with classic spooling, read it back and write it again
 * @return 0 if the file written first and the file written after the read are equal, non-zero otherwise
 */
static int STN_test() {
   int ret = 0;
   ocs::tom::ElementPtr<ocs::tom::STN> ep;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   spooling_field *fields = nullptr;
   const char *file1 = nullptr;
   const char *file2 = nullptr;
   
   auto ep_stn = ocs::tom::ElementPtr<ocs::tom::STN>::create();
   ep_stn->set<STN_name>("Test_Name");
   ep_stn->set<STN_type>(1);
   ep_stn->set<STN_id>(0);
   ep_stn->set<STN_shares>(99);
   ep_stn->set<STN_version>(1);
   
   auto lp = ocs::tom::ListPtr<ocs::tom::STN>::create("Sharetree List");
   
   auto ep2 = ocs::tom::ElementPtr<ocs::tom::STN>::create();
   ep2->set<STN_name>("Test_Name2");
   ep2->set<STN_type>(1);
   ep2->set<STN_id>(1);
   ep2->set<STN_shares>(99);
   
   auto lp2 = ocs::tom::ListPtr<ocs::tom::STN>::create("Sharetree List");
   
   auto ep3 = ocs::tom::ElementPtr<ocs::tom::STN>::create();
   ep3->set<STN_name>("Test_Name3");
   ep3->set<STN_type>(1);
   ep3->set<STN_id>(2);
   ep3->set<STN_shares>(99);
   ep3->set<STN_children>(nullptr);
   lp2->push_back(std::move(ep3));
   
   auto ep3_stn = ocs::tom::ElementPtr<ocs::tom::STN>::create();
   ep3_stn->set<STN_name>("Test_Name4");
   ep3_stn->set<STN_type>(1);
   ep3_stn->set<STN_id>(4);
   ep3_stn->set<STN_shares>(99);
   ep3_stn->set<STN_children>(nullptr);
   lp2->push_back(std::move(ep3_stn));
   
   ep2->set<STN_children>(std::move(lp2));
   lp->push_back(std::move(ep2));
   
   auto ep2_stn = ocs::tom::ElementPtr<ocs::tom::STN>::create();
   ep2_stn->set<STN_name>("Test_Name5");
   ep2_stn->set<STN_type>(1);
   ep2_stn->set<STN_id>(5);
   ep2_stn->set<STN_shares>(99);
   
   auto lp2_stn = ocs::tom::ListPtr<ocs::tom::STN>::create("Sharetree List");
   
   auto ep3_stn_2 = ocs::tom::ElementPtr<ocs::tom::STN>::create();
   ep3_stn_2->set<STN_name>("Test_Name6");
   ep3_stn_2->set<STN_type>(1);
   ep3_stn_2->set<STN_id>(7);
   ep3_stn_2->set<STN_shares>(99);
   ep3_stn_2->set<STN_children>(nullptr);
   lp2_stn->push_back(std::move(ep3_stn_2));
   
   ep2_stn->set<STN_children>(std::move(lp2_stn));
   lp->push_back(std::move(ep2_stn));
   
   ep_stn->set<STN_children>(std::move(lp));

   fields = sge_build_STN_field_list(true, true);
   
   /* Write a STN file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, *ep_stn, false,
                                       fields,
                                       &qconf_name_value_list_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, false);

   /* Read a STN file using flatfile spooling */
   ep_stn.reset();
   ep = ocs::tom::ElementPtr<ocs::tom::STN>(spool_flatfile_read_object(&alp, ocs::tom::STN::descriptor.e, nullptr,
                                   fields, nullptr, true, &qconf_name_value_list_sfi,
                                   SP_FORM_ASCII, nullptr, file1));
   
   /* Write a STN file using flatfile spooling */
   file2 = spool_flatfile_write_object(&alp, *ep, false,
                                       fields,
                                       &qconf_name_value_list_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file2, false);
   ep.reset();

   
   ret = diff(file1, file2);
   
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   sge_free(&fields);

   answer_list_output(&alp);   
   
   return ret;
}

/**
 * @brief Write a complex entry with classic spooling, read it back and write it again
 * @return 0 if the file written first and the file written after the read are equal, non-zero otherwise
 */
static int CE_test() {
   int ret = 0;
   ocs::tom::ElementPtr<ocs::tom::CE> ep;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   const char *file1 = nullptr, *file2 = nullptr;
   
   auto ep_ce = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ep_ce->set<CE_name>("Test_Name");
   ep_ce->set<CE_shortcut>("shortcut");
   ep_ce->set<CE_valtype>(1);
   ep_ce->set<CE_relop>(5);
   ep_ce->set<CE_consumable>(1);
   ep_ce->set<CE_defaultval>("10");
   ep_ce->set<CE_requestable>(REQU_NO);
   ep_ce->set<CE_urgency_weight>("20");
   
   /* Write a CE file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, *ep_ce, false,
                                       CE_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, false);
   
   /* Read a CE file using flatfile spooling */
   ep_ce.reset();
   ep = ocs::tom::ElementPtr<ocs::tom::CE>(spool_flatfile_read_object(&alp, ocs::tom::CE::descriptor.e, nullptr,
                                   CE_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1));
         
   /* Write a CE file using flatfile spooling */
   file2 = spool_flatfile_write_object(&alp, *ep, false,
                                       CE_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file2, false);

   ep.reset();
   
   ret = diff(file1, file2);
   
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   
   answer_list_output(&alp);   
   
   return ret;
}

/**
 * @brief Write a list of two complex entries with classic spooling, read it back and write it again
 * @return 0 if the file written first and the file written after the read are equal, non-zero otherwise
 */
static int CEL_test() {
   int ret = 0;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   const char *file1 = nullptr;
   const char *file2 = nullptr;
   
   auto lp = ocs::tom::ListPtr<ocs::tom::CE>::create("Complex List");
   
   auto ep = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ep->set<CE_name>("Test_Name1");
   ep->set<CE_shortcut>("tn1");
   ep->set<CE_valtype>(1);
   ep->set<CE_relop>(5);
   ep->set<CE_consumable>(2);
   ep->set<CE_defaultval>("15");
   ep->set<CE_requestable>(REQU_NO);
   ep->set<CE_urgency_weight>("25");
   lp->push_back(std::move(ep));
   
   auto ep_ce = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ep_ce->set<CE_name>("Test_Name2");
   ep_ce->set<CE_shortcut>("tn2");
   ep_ce->set<CE_valtype>(1);
   ep_ce->set<CE_relop>(5);
   ep_ce->set<CE_consumable>(0);
   ep_ce->set<CE_defaultval>("15");
   ep_ce->set<CE_requestable>(REQU_YES);
   ep_ce->set<CE_urgency_weight>("25");
   lp->push_back(std::move(ep_ce));
   
   spool_flatfile_align_list(&alp, *lp, CE_fields, 0);
   
   /* Write a CEL file using classic spooling */
   file1 = spool_flatfile_write_list(&alp, *lp,
                                     CE_fields,
                                     &qconf_ce_list_sfi,
                                     SP_DEST_TMP, 
                                     SP_FORM_ASCII, 
                                     file1, false);
   
   /* Read a CE file using flatfile spooling */
   lp.reset();
   lp = ocs::tom::ListPtr<ocs::tom::CE>(spool_flatfile_read_list(&alp, ocs::tom::CE::descriptor.e,
                                   CE_fields, nullptr, true, &qconf_ce_list_sfi,
                                   SP_FORM_ASCII, nullptr, file1));
   
   /* Write a CEL file using flatfile spooling */
   file2 = spool_flatfile_write_list(&alp, *lp,
                                     CE_fields,
                                     &qconf_ce_list_sfi,
                                     SP_DEST_TMP, 
                                     SP_FORM_ASCII, 
                                     nullptr, false);
   lp.reset();

   
   ret = diff(file1, file2);
   
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   
   answer_list_output(&alp);   
   
   return ret;
}

static int diff(const char *file1, const char *file2) {
   int ret = 1;
   char **argv =(char **)sge_malloc(sizeof(char *) * 4);
   SGE_ASSERT(argv != nullptr);
   const char *path = "/usr/bin/diff";

   if (file1 == nullptr || file2 == nullptr) {
      printf("file pointer is <nullptr>\n");
      return 1;
   }
   
   if(!fork()) {
      argv[0] = (char *)path;
      argv[1] = (char *)file1;
      argv[2] = (char *)file2;
      argv[3] = nullptr;
      
      execv(path, argv);
      /* if execv return an error occurred */
   }
   else {
      int stat_loc = 0;
      
      wait(&stat_loc);
      if (WIFEXITED(stat_loc)) {
         ret = WEXITSTATUS(stat_loc);
      }
   }
   
   sge_free(&argv);
   return ret;
}

/**
 * @brief A usage list of three entries
 * @return the list
 */
static ocs::tom::ListPtr<ocs::tom::UA> buildUsageList() {   

   auto lp = ocs::tom::ListPtr<ocs::tom::UA>::create("Usage List");
   
   auto ep2 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ep2->set<UA_name>("Test_Name2");
   ep2->set<UA_value>(11.22);
   lp->push_back(std::move(ep2));
   
   auto ep2_ua = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ep2_ua->set<UA_name>("Test_Name3");
   ep2_ua->set<UA_value>(33.44);
   lp->push_back(std::move(ep2_ua));
   
   auto ep2_ua_2 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ep2_ua_2->set<UA_name>("Test_Name4");
   ep2_ua_2->set<UA_value>(55.66);
   lp->push_back(std::move(ep2_ua_2));

   return lp;
}

/**
 * @brief A long term usage list of two entries
 * @return the list
 */
static ocs::tom::ListPtr<ocs::tom::UA> buildLTUsageList() {

   auto lp = ocs::tom::ListPtr<ocs::tom::UA>::create("LT Usage List");
   
   auto ep2 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ep2->set<UA_name>("Test_Name5");
   ep2->set<UA_value>(11.22);
   lp->push_back(std::move(ep2));
   
   auto ep2_ua = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ep2_ua->set<UA_name>("Test_Name6");
   ep2_ua->set<UA_value>(33.44);
   lp->push_back(std::move(ep2_ua));
   
   auto ep2_ua_2 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ep2_ua_2->set<UA_name>("Test_Name7");
   ep2_ua_2->set<UA_value>(55.66);
   lp->push_back(std::move(ep2_ua_2));
 
   return lp;
}      

/**
 * @brief A list of two projects with their usage
 * @return the list
 */
static ocs::tom::ListPtr<ocs::tom::UPP> buildProjectList() {

   auto lp = ocs::tom::ListPtr<ocs::tom::UPP>::create("Project List");
   
   auto ep2 = ocs::tom::ElementPtr<ocs::tom::UPP>::create();
   ep2->set<UPP_name>("Test_Name8");
   
   auto lp2 = ocs::tom::ListPtr<ocs::tom::UA>::create("Usage List");
   
   auto ep3 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ep3->set<UA_name>("Test_Name9");
   ep3->set<UA_value>(11.22);
   lp2->push_back(std::move(ep3));
   
   auto ep3_ua = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ep3_ua->set<UA_name>("Test_Name10");
   ep3_ua->set<UA_value>(33.44);
   lp2->push_back(std::move(ep3_ua));
   
   auto ep3_ua_2 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ep3_ua_2->set<UA_name>("Test_Name11");
   ep3_ua_2->set<UA_value>(55.66);
   lp2->push_back(std::move(ep3_ua_2));
      
   ep2->set<UPP_usage>(std::move(lp2));
   
   auto lp2_ua = ocs::tom::ListPtr<ocs::tom::UA>::create("LT Usage List");
   
   auto ep3_ua_3 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ep3_ua_3->set<UA_name>("Test_Name12");
   ep3_ua_3->set<UA_value>(11.22);
   lp2_ua->push_back(std::move(ep3_ua_3));
   
   auto ep3_ua_4 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ep3_ua_4->set<UA_name>("Test_Name13");
   ep3_ua_4->set<UA_value>(33.44);
   lp2_ua->push_back(std::move(ep3_ua_4));
   
   auto ep3_ua_5 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ep3_ua_5->set<UA_name>("Test_Name14");
   ep3_ua_5->set<UA_value>(55.66);
   lp2_ua->push_back(std::move(ep3_ua_5));
      
   ep2->set<UPP_long_term_usage>(std::move(lp2_ua));
   lp->push_back(std::move(ep2));
   
   auto ep2_upp = ocs::tom::ElementPtr<ocs::tom::UPP>::create();
   ep2_upp->set<UPP_name>("Test_Name15");
   
   auto lp2_ua_2 = ocs::tom::ListPtr<ocs::tom::UA>::create("Usage List");
   
   auto ep3_ua_6 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ep3_ua_6->set<UA_name>("Test_Name16");
   ep3_ua_6->set<UA_value>(11.22);
   lp2_ua_2->push_back(std::move(ep3_ua_6));
   
   auto ep3_ua_7 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ep3_ua_7->set<UA_name>("Test_Name17");
   ep3_ua_7->set<UA_value>(33.44);
   lp2_ua_2->push_back(std::move(ep3_ua_7));
   
   auto ep3_ua_8 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ep3_ua_8->set<UA_name>("Test_Name18");
   ep3_ua_8->set<UA_value>(55.66);
   lp2_ua_2->push_back(std::move(ep3_ua_8));
      
   ep2_upp->set<UPP_usage>(std::move(lp2_ua_2));
   
   auto lp2_ua_3 = ocs::tom::ListPtr<ocs::tom::UA>::create("LT Usage List");
   
   auto ep3_ua_9 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ep3_ua_9->set<UA_name>("Test_Name19");
   ep3_ua_9->set<UA_value>(11.22);
   lp2_ua_3->push_back(std::move(ep3_ua_9));
   
   auto ep3_ua_10 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ep3_ua_10->set<UA_name>("Test_Name20");
   ep3_ua_10->set<UA_value>(33.44);
   lp2_ua_3->push_back(std::move(ep3_ua_10));
   
   auto ep3_ua_11 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ep3_ua_11->set<UA_name>("Test_Name21");
   ep3_ua_11->set<UA_value>(55.66);
   lp2_ua_3->push_back(std::move(ep3_ua_11));
      
   ep2_upp->set<UPP_long_term_usage>(std::move(lp2_ua_3));
   lp->push_back(std::move(ep2_upp));

   return lp;
}

/**
 * @brief A list of the usage debited to two jobs
 * @return the list
 */
static ocs::tom::ListPtr<ocs::tom::UPU> buildDebitedUsageList() {

   auto lp = ocs::tom::ListPtr<ocs::tom::UPU>::create("Debited Usage List");
   
   auto ep = ocs::tom::ElementPtr<ocs::tom::UPU>::create();
   ep->set<UPU_job_number>(13579);
   
   auto lp2 = ocs::tom::ListPtr<ocs::tom::UA>::create("Old Usage List");
   
   auto ep1 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ep1->set<UA_name>("Test_Name26");
   ep1->set<UA_value>(11.22);
   lp2->push_back(std::move(ep1));
   
   auto ep1_ua = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ep1_ua->set<UA_name>("Test_Name27");
   ep1_ua->set<UA_value>(33.44);
   lp2->push_back(std::move(ep1_ua));
   
   auto ep1_ua_2 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ep1_ua_2->set<UA_name>("Test_Name28");
   ep1_ua_2->set<UA_value>(55.66);
   lp2->push_back(std::move(ep1_ua_2));
      
   ep->set<UPU_old_usage_list>(std::move(lp2));
   lp->push_back(std::move(ep));
   
   auto ep_upu = ocs::tom::ElementPtr<ocs::tom::UPU>::create();
   ep_upu->set<UPU_job_number>(97531);
   
   auto lp2_ua = ocs::tom::ListPtr<ocs::tom::UA>::create("Old Usage List");
   
   auto ep1_ua_3 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ep1_ua_3->set<UA_name>("Test_Name29");
   ep1_ua_3->set<UA_value>(11.22);
   lp2_ua->push_back(std::move(ep1_ua_3));
   
   auto ep1_ua_4 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ep1_ua_4->set<UA_name>("Test_Name30");
   ep1_ua_4->set<UA_value>(33.44);
   lp2_ua->push_back(std::move(ep1_ua_4));
   
   auto ep1_ua_5 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ep1_ua_5->set<UA_name>("Test_Name31");
   ep1_ua_5->set<UA_value>(55.66);
   lp2_ua->push_back(std::move(ep1_ua_5));
      
   ep_upu->set<UPU_old_usage_list>(std::move(lp2_ua));
   lp->push_back(std::move(ep_upu));

   return lp;
}   

/**
 * @brief A list of two usersets for the access list of a project
 * @return the list
 */
static ocs::tom::ListPtr<ocs::tom::US> buildACLList() {

   auto lp = ocs::tom::ListPtr<ocs::tom::US>::create("ACL List");
   
   auto ep2 = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ep2->set<US_name>("Test_Name22");
   lp->push_back(std::move(ep2));
   
   auto ep2_us = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ep2_us->set<US_name>("Test_Name23");
   lp->push_back(std::move(ep2_us));

   return lp;
}   
   
/**
 * @brief A list of two usersets for the denied access list of a project
 * @return the list
 */
static ocs::tom::ListPtr<ocs::tom::US> buildXACLList() {

   auto lp = ocs::tom::ListPtr<ocs::tom::US>::create("XACL List");
   
   auto ep2 = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ep2->set<US_name>("Test_Name24");
   lp->push_back(std::move(ep2));
   
   auto ep2_us = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ep2_us->set<US_name>("Test_Name25");
   lp->push_back(std::move(ep2_us));

   return lp;
}   

/**
 * @brief Write a user with classic spooling, read it back and write it again
 * @return 0 if the file written first and the file written after the read are equal, non-zero otherwise
 */
static int UU_test() {
   int ret = 0;
   ocs::tom::ElementPtr<ocs::tom::UU> user;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   spooling_field *fields = nullptr;
   const char *file1 = nullptr;
   const char *file2 = nullptr;
   
   auto user_uu = ocs::tom::ElementPtr<ocs::tom::UU>::create();
   user_uu->set<UU_name>("Test_Name");
   user_uu->set<UU_oticket>(100);
   user_uu->set<UU_fshare>(50);
   user_uu->set<UU_delete_time>((uint64_t)123456789000000);
   user_uu->set<UU_default_project>("default_project");

   user_uu->set<UU_usage_time_stamp>(987654321);

   user_uu->set<UU_usage>(buildUsageList());
   
   user_uu->set<UU_long_term_usage>(buildLTUsageList());
   
   user_uu->set<UU_project>(buildProjectList());
   
   user_uu->set<UU_debited_job_usage>(buildDebitedUsageList());


   fields = sge_build_UU_field_list(true);

   /* Write a UU file using flatfile spooling */
   file1 = spool_flatfile_write_object(&alp, *user_uu, false,
                                     fields,
                                     &qconf_sfi,
                                     SP_DEST_TMP,
                                     SP_FORM_ASCII, 
                                     file1, false);
   user_uu.reset();
   user = ocs::tom::ElementPtr<ocs::tom::UU>(spool_flatfile_read_object(&alp, ocs::tom::UU::descriptor.e, nullptr,
                                   fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1));
   file2 = spool_flatfile_write_object(&alp, *user, false,
                                     fields,
                                     &qconf_sfi,
                                     SP_DEST_TMP,
                                     SP_FORM_ASCII, 
                                     file2, false);
   user.reset();
   ret = diff(file1, file2);
   
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   sge_free(&fields);
   
   answer_list_output(&alp);   
   
   return ret;
}

/**
 * @brief Write a project with classic spooling, read it back and write it again
 * @return 0 if the file written first and the file written after the read are equal, non-zero otherwise
 */
static int PR_test() {
   int ret = 0;
   ocs::tom::ElementPtr<ocs::tom::PR> prj;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   spooling_field *fields = nullptr;
   const char *file1 = nullptr;
   const char *file2 = nullptr;
   
   auto prj_pr = ocs::tom::ElementPtr<ocs::tom::PR>::create();
   prj_pr->set<PR_name>("Test_Name");
   prj_pr->set<PR_oticket>(100);
   prj_pr->set<PR_fshare>(50);
   prj_pr->set<PR_usage_time_stamp>(987654321);

   prj_pr->set<PR_usage>(buildUsageList());
   
   prj_pr->set<PR_long_term_usage>(buildLTUsageList());
   
   prj_pr->set<PR_project>(buildProjectList());
   
   prj_pr->set<PR_acl>(buildACLList());
   
   prj_pr->set<PR_xacl>(buildXACLList());
   
   prj_pr->set<PR_debited_job_usage>(buildDebitedUsageList());

   fields = sge_build_PR_field_list(true);

   /* Write a PR file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, *prj_pr, false,
                                     fields,
                                     &qconf_sfi,
                                     SP_DEST_TMP,
                                     SP_FORM_ASCII, 
                                     file1, false);
   
   /* Read a PR file using flatfile spooling */
   prj_pr.reset();
   prj = ocs::tom::ElementPtr<ocs::tom::PR>(spool_flatfile_read_object(&alp, ocs::tom::PR::descriptor.e, nullptr,
                                   fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1));
         
   /* Write a PR file using flatfile spooling */
   file2 = spool_flatfile_write_object(&alp, *prj, false,
                                     fields,
                                     &qconf_sfi,
                                     SP_DEST_TMP,
                                     SP_FORM_ASCII, 
                                     file2, false);

   prj.reset();
   answer_list_output(&alp);   
   
   ret = diff(file1, file2);
   
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   sge_free(&fields);
   
   answer_list_output(&alp);   
   
   return ret;
}

/**
 * @brief Write a userset with classic spooling, read it back and write it again
 * @return 0 if the file written first and the file written after the read are equal, non-zero otherwise
 */
static int US_test() {
   int ret = 0;
   ocs::tom::ElementPtr<ocs::tom::US> ep;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   const char *file1 = nullptr;
   const char *file2 = nullptr;
   
   auto ep_us = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ep_us->set<US_name>("Test_Name");
   ep_us->set<US_type>(1);
   ep_us->set<US_fshare>(10);
   ep_us->set<US_oticket>(100);
   
   auto lp = ocs::tom::ListPtr<ocs::tom::UE>::create("Entry List");
   
   auto ep2 = ocs::tom::ElementPtr<ocs::tom::UE>::create();
   ep2->set<UE_name>("Test_Name2");
   lp->push_back(std::move(ep2));
   
   auto ep2_ue = ocs::tom::ElementPtr<ocs::tom::UE>::create();
   ep2_ue->set<UE_name>("Test_Name3");
   lp->push_back(std::move(ep2_ue));
   
   auto ep2_ue_2 = ocs::tom::ElementPtr<ocs::tom::UE>::create();
   ep2_ue_2->set<UE_name>("Test_Name4");
   lp->push_back(std::move(ep2_ue_2));
   
   ep_us->set<US_entries>(std::move(lp));
   
   /* Write a US file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, *ep_us, false,
                                       US_fields,
                                       &qconf_param_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, false);

   /* Read a US file using flatfile spooling */
   ep_us.reset();
   ep = ocs::tom::ElementPtr<ocs::tom::US>(spool_flatfile_read_object(&alp, ocs::tom::US::descriptor.e, nullptr,
                                   US_fields, nullptr, true, &qconf_param_sfi,
                                   SP_FORM_ASCII, nullptr, file1));
      
   /* Write a US file using flatfile spooling */
   file2 = spool_flatfile_write_object(&alp, *ep, false,
                                       US_fields,
                                       &qconf_param_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file2, false);
   
   
   ret = diff(file1, file2);
   
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   
   ep.reset();
   
   answer_list_output(&alp);   
   
   return ret;
}

/**
 * @brief Write an execution host with classic spooling, read it back and write it again
 * @return 0 if the file written first and the file written after the read are equal, non-zero otherwise
 */
static int EH_test() {
   int ret = 0;
   ocs::tom::ElementPtr<ocs::tom::EH> ep;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   spooling_field *fields = nullptr;
   const char *file1 = nullptr;
   const char *file2 = nullptr;
   
   auto ep_eh = ocs::tom::ElementPtr<ocs::tom::EH>::create();
   ep_eh->set<EH_name>(ocs::tom::Host("Test_Name"));
   
   auto lp_hs = ocs::tom::ListPtr<ocs::tom::HS>::create("Load Scaling List");
   
   auto ep2 = ocs::tom::ElementPtr<ocs::tom::HS>::create();
   ep2->set<HS_name>("Test_Name2");
   ep2->set<HS_value>(1234.567);
   lp_hs->push_back(std::move(ep2));
   
   auto ep2_hs = ocs::tom::ElementPtr<ocs::tom::HS>::create();
   ep2_hs->set<HS_name>("Test_Name3");
   ep2_hs->set<HS_value>(6.7);
   lp_hs->push_back(std::move(ep2_hs));
   
   ep_eh->set<EH_scaling_list>(std::move(lp_hs));
   
   auto lp_ce = ocs::tom::ListPtr<ocs::tom::CE>::create("Consumable Config List");
   
   auto ep2_ce = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ep2_ce->set<CE_name>("Test_Name4");
   ep2_ce->set<CE_shortcut>("tn4");
   ep2_ce->set<CE_valtype>(1);
   ep2_ce->set<CE_relop>(5);
   ep2_ce->set<CE_consumable>(1);
   ep2_ce->set<CE_defaultval>("15");
   ep2_ce->set<CE_requestable>(REQU_NO);
   ep2_ce->set<CE_urgency_weight>("25");
   ep2_ce->set<CE_stringval>("stringval");
   lp_ce->push_back(std::move(ep2_ce));
   
   auto ep2_ce_2 = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ep2_ce_2->set<CE_name>("Test_Name5");
   ep2_ce_2->set<CE_shortcut>("tn5");
   ep2_ce_2->set<CE_valtype>(1);
   ep2_ce_2->set<CE_relop>(5);
   ep2_ce_2->set<CE_consumable>(0);
   ep2_ce_2->set<CE_defaultval>("15");
   ep2_ce_2->set<CE_requestable>(REQU_YES);
   ep2_ce_2->set<CE_urgency_weight>("25");
   ep2_ce_2->set<CE_doubleval>(6969.9696);
   lp_ce->push_back(std::move(ep2_ce_2));
   
   ep_eh->set<EH_consumable_config_list>(std::move(lp_ce));
   
   auto lp = ocs::tom::ListPtr<ocs::tom::CE>::create("Actual Complex Values List");

   auto ep2_ce_3 = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ep2_ce_3->set<CE_name>("Test_Name6");
   ep2_ce_3->set<CE_shortcut>("tn6");
   ep2_ce_3->set<CE_valtype>(1);
   ep2_ce_3->set<CE_relop>(5);
   ep2_ce_3->set<CE_consumable>(1);
   ep2_ce_3->set<CE_defaultval>("15");
   ep2_ce_3->set<CE_requestable>(REQU_NO);
   ep2_ce_3->set<CE_urgency_weight>("25");
   ep2_ce_3->set<CE_stringval>("stringval");
   lp->push_back(std::move(ep2_ce_3));
   
   auto ep2_ce_4 = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ep2_ce_4->set<CE_name>("Test_Name7");
   ep2_ce_4->set<CE_shortcut>("tn7");
   ep2_ce_4->set<CE_valtype>(1);
   ep2_ce_4->set<CE_relop>(5);
   ep2_ce_4->set<CE_consumable>(0);
   ep2_ce_4->set<CE_defaultval>("15");
   ep2_ce_4->set<CE_requestable>(REQU_YES);
   ep2_ce_4->set<CE_urgency_weight>("25");
   ep2_ce_4->set<CE_doubleval>(6969.9696);
   lp->push_back(std::move(ep2_ce_4));

   ocs::tom::set<EH_resource_utilization>(ep_eh.get().raw(), lp.release());
   
   auto lp_hl = ocs::tom::ListPtr<ocs::tom::HL>::create("Load Values List");
   
   auto ep2_hl = ocs::tom::ElementPtr<ocs::tom::HL>::create();
   ep2_hl->set<HL_name>("Test_Name8");
   ep2_hl->set<HL_value>("1234.567");
   lp_hl->push_back(std::move(ep2_hl));
   
   auto ep2_hl_2 = ocs::tom::ElementPtr<ocs::tom::HL>::create();
   ep2_hl_2->set<HL_name>("Test_Name9");
   ep2_hl_2->set<HL_value>("6.7");
   lp_hl->push_back(std::move(ep2_hl_2));
   
   ep_eh->set<EH_load_list>(std::move(lp_hl));

   ep_eh->set<EH_processors>(64);
   
   auto lp_ru = ocs::tom::ListPtr<ocs::tom::RU>::create("Resched Unknown List");
   
   auto ep2_ru = ocs::tom::ElementPtr<ocs::tom::RU>::create();
   ep2_ru->set<RU_job_number>(1);
   ep2_ru->set<RU_task_number>(1);
   ep2_ru->set<RU_state>(0);
   lp_ru->push_back(std::move(ep2_ru));
   
   auto ep2_ru_2 = ocs::tom::ElementPtr<ocs::tom::RU>::create();
   ep2_ru_2->set<RU_job_number>(1);
   ep2_ru_2->set<RU_task_number>(2);
   ep2_ru_2->set<RU_state>(1);
   lp_ru->push_back(std::move(ep2_ru_2));
   
   auto ep2_ru_3 = ocs::tom::ElementPtr<ocs::tom::RU>::create();
   ep2_ru_3->set<RU_job_number>(2);
   ep2_ru_3->set<RU_task_number>(1);
   ep2_ru_3->set<RU_state>(2);
   lp_ru->push_back(std::move(ep2_ru_3));

   ep_eh->set<EH_reschedule_unknown_list>(std::move(lp_ru));
   
   auto lp_us = ocs::tom::ListPtr<ocs::tom::US>::create("User List");
   
   auto ep2_us = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ep2_us->set<US_name>("Test_Name9");
   lp_us->push_back(std::move(ep2_us));
   
   auto ep2_us_2 = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ep2_us_2->set<US_name>("Test_Name10");
   lp_us->push_back(std::move(ep2_us_2));
   
   ep_eh->set<EH_acl>(std::move(lp_us));
   
   auto lp_us_2 = ocs::tom::ListPtr<ocs::tom::US>::create("XUser List");
   
   auto ep2_us_3 = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ep2_us_3->set<US_name>("Test_Name11");
   lp_us_2->push_back(std::move(ep2_us_3));
   
   auto ep2_us_4 = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ep2_us_4->set<US_name>("Test_Name12");
   lp_us_2->push_back(std::move(ep2_us_4));
   
   ep_eh->set<EH_xacl>(std::move(lp_us_2));
   
   auto lp_pr = ocs::tom::ListPtr<ocs::tom::PR>::create("Projects List");
   
   auto ep2_pr = ocs::tom::ElementPtr<ocs::tom::PR>::create();
   ep2_pr->set<PR_name>("Test_Name13");
   lp_pr->push_back(std::move(ep2_pr));
   
   auto ep2_pr_2 = ocs::tom::ElementPtr<ocs::tom::PR>::create();
   ep2_pr_2->set<PR_name>("Test_Name14");
   lp_pr->push_back(std::move(ep2_pr_2));
   
   ep_eh->set<EH_prj>(std::move(lp_pr));
   
   auto lp_pr_2 = ocs::tom::ListPtr<ocs::tom::PR>::create("XProjects List");
   
   auto ep2_pr_3 = ocs::tom::ElementPtr<ocs::tom::PR>::create();
   ep2_pr_3->set<PR_name>("Test_Name15");
   lp_pr_2->push_back(std::move(ep2_pr_3));
   
   auto ep2_pr_4 = ocs::tom::ElementPtr<ocs::tom::PR>::create();
   ep2_pr_4->set<PR_name>("Test_Name16");
   lp_pr_2->push_back(std::move(ep2_pr_4));
   
   ep_eh->set<EH_xprj>(std::move(lp_pr_2));
   
   auto lp_hs_2 = ocs::tom::ListPtr<ocs::tom::HS>::create("Usage Scaling List");
   
   auto ep2_hs_2 = ocs::tom::ElementPtr<ocs::tom::HS>::create();
   ep2_hs_2->set<HS_name>("Test_Name17");
   ep2_hs_2->set<HS_value>(1234.567);
   lp_hs_2->push_back(std::move(ep2_hs_2));
   
   auto ep2_hs_3 = ocs::tom::ElementPtr<ocs::tom::HS>::create();
   ep2_hs_3->set<HS_name>("Test_Name18");
   ep2_hs_3->set<HS_value>(6.7);
   lp_hs_2->push_back(std::move(ep2_hs_3));
   
   ep_eh->set<EH_usage_scaling_list>(std::move(lp_hs_2));
   
   auto lp_stu = ocs::tom::ListPtr<ocs::tom::STU>::create("Report Vars List");
   
   auto ep2_stu = ocs::tom::ElementPtr<ocs::tom::STU>::create();
   ep2_stu->set<STU_name>("Test_Name19");
   lp_stu->push_back(std::move(ep2_stu));
   
   auto ep2_stu_2 = ocs::tom::ElementPtr<ocs::tom::STU>::create();
   ep2_stu_2->set<STU_name>("Test_Name20");
   lp_stu->push_back(std::move(ep2_stu_2));
   
   ep_eh->set<EH_report_variables>(std::move(lp_stu));

   /* the time at which the host added itself. The spool file carries the raw value, so that it
    * can be read back - the formatted date and time which qconf -se shows contains a blank,
    * which is the delimiter between the name of an attribute and its value in this format */
   ep_eh->set<EH_auto_added_time>(1792143190772380);

   fields = sge_build_EH_field_list(true, false, false, false);

   /* Write a EH file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, *ep_eh, false,
                                       fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, false);
   
   /* Read a EH file using flatfile spooling */
   ep_eh.reset();
   ep = ocs::tom::ElementPtr<ocs::tom::EH>(spool_flatfile_read_object(&alp, ocs::tom::EH::descriptor.e, nullptr,
                                   fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1));
      
   /* Write a EH file using flatfile spooling */
   file2 = spool_flatfile_write_object(&alp, *ep, false,
                                       fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file2, false);
   
   ret = diff(file1, file2);
   
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   sge_free(&fields);

   ep.reset();

   if (ret != 0) {
      return ret;
   }
   
   answer_list_output(&alp);   
   
   return ret;
}

/**
 * @brief Write a cluster queue with classic spooling, read it back and write it again
 * @return 0 if the file written first and the file written after the read are equal, non-zero otherwise
 */
static int CQ_test() {
   int ret = 0;
   ocs::tom::ElementPtr<ocs::tom::CQ> ep;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   const char *file1 = nullptr, *file2 = nullptr;
   
   auto ep_cq = ocs::tom::ElementPtr<ocs::tom::CQ>::create();
   
   ep_cq->set<CQ_name>("Test_Name");
   
   ep_cq->emplace_back_in<CQ_hostlist, HR_name>("Test_Name2");
   ep_cq->emplace_back_in<CQ_hostlist, HR_name>("Test_Name3");
   ep_cq->emplace_back_in<CQ_hostlist, HR_name>("Test_Name4");
   ep_cq->emplace_back_in<CQ_hostlist, HR_name>("Test_Name5");
   ep_cq->emplace_back_in<CQ_hostlist, HR_name>("Test_Name6");
   ep_cq->emplace_back_in<CQ_hostlist, HR_name>("Test_Name7");

   auto lp = ocs::tom::ListPtr<ocs::tom::AULNG>::create("Seq No. List");
   
   auto ep2 = ocs::tom::ElementPtr<ocs::tom::AULNG>::create();
   ep2->set<AULNG_href>(ocs::tom::Host("Test_Name4"));
   ep2->set<AULNG_value>(12);
   lp->push_back(std::move(ep2));
   
   auto ep2_aulng = ocs::tom::ElementPtr<ocs::tom::AULNG>::create();
   ep2_aulng->set<AULNG_href>(ocs::tom::Host("Test_Name5"));
   ep2_aulng->set<AULNG_value>(16);
   lp->push_back(std::move(ep2_aulng));

   ep_cq->set<CQ_seq_no>(std::move(lp));

   auto lp_acelist = ocs::tom::ListPtr<ocs::tom::ACELIST>::create("Load Thresholds");
   
   auto ep2_acelist = ocs::tom::ElementPtr<ocs::tom::ACELIST>::create();
   ep2_acelist->set<ACELIST_href>(ocs::tom::Host("Test_Name6"));

   auto lp2 = ocs::tom::ListPtr<ocs::tom::CE>::create("Complex List");
   
   auto ep3 = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ep3->set<CE_name>("Test_Name7");
   ep3->set<CE_stringval>("stringval");
   lp2->push_back(std::move(ep3));
   
   ep2_acelist->set<ACELIST_value>(std::move(lp2));
   
   lp_acelist->push_back(std::move(ep2_acelist));

   ep_cq->set<CQ_load_thresholds>(std::move(lp_acelist));
   
   auto lp_acelist_2 = ocs::tom::ListPtr<ocs::tom::ACELIST>::create("Suspend Thresholds");

   auto ep2_acelist_2 = ocs::tom::ElementPtr<ocs::tom::ACELIST>::create();
   ep2_acelist_2->set<ACELIST_href>(ocs::tom::Host("Test_Name15"));

   auto lp2_ce = ocs::tom::ListPtr<ocs::tom::CE>::create("Complex_List2");
   
   auto ep3_ce = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ep3_ce->set<CE_name>("Test_Name16");
   ep3_ce->set<CE_stringval>("stringval");
   lp2_ce->push_back(std::move(ep3_ce));

   ep2_acelist_2->set<ACELIST_value>(std::move(lp2_ce));
   
   lp_acelist_2->push_back(std::move(ep2_acelist_2));

   ep_cq->set<CQ_suspend_thresholds>(std::move(lp_acelist_2));
   
   auto lp_aulng = ocs::tom::ListPtr<ocs::tom::AULNG>::create("NSuspend List");
   
   auto ep2_aulng_2 = ocs::tom::ElementPtr<ocs::tom::AULNG>::create();
   ep2_aulng_2->set<AULNG_href>(ocs::tom::Host("Test_Name18"));
   ep2_aulng_2->set<AULNG_value>(12);
   lp_aulng->push_back(std::move(ep2_aulng_2));
   
   auto ep2_aulng_3 = ocs::tom::ElementPtr<ocs::tom::AULNG>::create();
   ep2_aulng_3->set<AULNG_href>(ocs::tom::Host("Test_Name19"));
   ep2_aulng_3->set<AULNG_value>(16);
   lp_aulng->push_back(std::move(ep2_aulng_3));

   ep_cq->set<CQ_nsuspend>(std::move(lp_aulng));

   auto lp_ainter = ocs::tom::ListPtr<ocs::tom::AINTER>::create("Suspend Intervals");
   
   auto ep2_ainter = ocs::tom::ElementPtr<ocs::tom::AINTER>::create();
   ep2_ainter->set<AINTER_href>(ocs::tom::Host("Test_Name20"));
   ep2_ainter->set<AINTER_value>("1");
   lp_ainter->push_back(std::move(ep2_ainter));
   
   auto ep2_ainter_2 = ocs::tom::ElementPtr<ocs::tom::AINTER>::create();
   ep2_ainter_2->set<AINTER_href>(ocs::tom::Host("Test_Name21"));
   ep2_ainter_2->set<AINTER_value>("2");
   lp_ainter->push_back(std::move(ep2_ainter_2));

   ep_cq->set<CQ_suspend_interval>(std::move(lp_ainter));

   auto lp_astr = ocs::tom::ListPtr<ocs::tom::ASTR>::create("Priority List");
   
   auto ep2_astr = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ep2_astr->set<ASTR_href>(ocs::tom::Host("Test_Name22"));
   ep2_astr->set<ASTR_value>("3");
   lp_astr->push_back(std::move(ep2_astr));
   
   auto ep2_astr_2 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ep2_astr_2->set<ASTR_href>(ocs::tom::Host("Test_Name23"));
   ep2_astr_2->set<ASTR_value>("4");
   lp_astr->push_back(std::move(ep2_astr_2));

   ep_cq->set<CQ_priority>(std::move(lp_astr));

   auto lp_ainter_2 = ocs::tom::ListPtr<ocs::tom::AINTER>::create("Min CPU Intervals");
   
   auto ep2_ainter_3 = ocs::tom::ElementPtr<ocs::tom::AINTER>::create();
   ep2_ainter_3->set<AINTER_href>(ocs::tom::Host("Test_Name24"));
   ep2_ainter_3->set<AINTER_value>("5");
   lp_ainter_2->push_back(std::move(ep2_ainter_3));
   
   auto ep2_ainter_4 = ocs::tom::ElementPtr<ocs::tom::AINTER>::create();
   ep2_ainter_4->set<AINTER_href>(ocs::tom::Host("Test_Name25"));
   ep2_ainter_4->set<AINTER_value>("6");
   lp_ainter_2->push_back(std::move(ep2_ainter_4));

   ep_cq->set<CQ_min_cpu_interval>(std::move(lp_ainter_2));

   auto lp_astr_2 = ocs::tom::ListPtr<ocs::tom::ASTR>::create("Processor List");
   
   auto ep2_astr_3 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ep2_astr_3->set<ASTR_href>(ocs::tom::Host("Test_Name26"));
   ep2_astr_3->set<ASTR_value>("7");
   lp_astr_2->push_back(std::move(ep2_astr_3));
   
   auto ep2_astr_4 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ep2_astr_4->set<ASTR_href>(ocs::tom::Host("Test_Name27"));
   ep2_astr_4->set<ASTR_value>("8");
   lp_astr_2->push_back(std::move(ep2_astr_4));

   ep_cq->set<CQ_processors>(std::move(lp_astr_2));

   auto lp_aqtlist = ocs::tom::ListPtr<ocs::tom::AQTLIST>::create("Q-Type List");
   
   auto ep2_aqtlist = ocs::tom::ElementPtr<ocs::tom::AQTLIST>::create();
   ep2_aqtlist->set<AQTLIST_href>(ocs::tom::Host("Test_Name28"));
   ep2_aqtlist->set<AQTLIST_value>(12);
   lp_aqtlist->push_back(std::move(ep2_aqtlist));
   
   auto ep2_aqtlist_2 = ocs::tom::ElementPtr<ocs::tom::AQTLIST>::create();
   ep2_aqtlist_2->set<AQTLIST_href>(ocs::tom::Host("Test_Name29"));
   ep2_aqtlist_2->set<AQTLIST_value>(16);
   lp_aqtlist->push_back(std::move(ep2_aqtlist_2));

   ep_cq->set<CQ_qtype>(std::move(lp_aqtlist));

   auto lp_astrlist = ocs::tom::ListPtr<ocs::tom::ASTRLIST>::create("Checkpoint List");
   
   auto ep2_astrlist = ocs::tom::ElementPtr<ocs::tom::ASTRLIST>::create();
   ep2_astrlist->set<ASTRLIST_href>(ocs::tom::Host("Test_Name30"));
   
   auto lp2_st = ocs::tom::ListPtr<ocs::tom::ST>::create("String List");
   
   auto ep3_st = ocs::tom::ElementPtr<ocs::tom::ST>::create();
   ep3_st->set<ST_name>("Test_Name31");
   lp2_st->push_back(std::move(ep3_st));
   
   auto ep3_st_2 = ocs::tom::ElementPtr<ocs::tom::ST>::create();
   ep3_st_2->set<ST_name>("Test_Name32");
   lp2_st->push_back(std::move(ep3_st_2));
   
   ep2_astrlist->set<ASTRLIST_value>(std::move(lp2_st));
   lp_astrlist->push_back(std::move(ep2_astrlist));

   ep_cq->set<CQ_ckpt_list>(std::move(lp_astrlist));

   auto lp_astrlist_2 = ocs::tom::ListPtr<ocs::tom::ASTRLIST>::create("PE List");
   
   auto ep2_astrlist_2 = ocs::tom::ElementPtr<ocs::tom::ASTRLIST>::create();
   ep2_astrlist_2->set<ASTRLIST_href>(ocs::tom::Host("Test_Name36"));
   
   auto lp2_st_2 = ocs::tom::ListPtr<ocs::tom::ST>::create("String List");
   
   auto ep3_st_3 = ocs::tom::ElementPtr<ocs::tom::ST>::create();
   ep3_st_3->set<ST_name>("Test_Name37");
   lp2_st_2->push_back(std::move(ep3_st_3));

   auto ep3_st_4 = ocs::tom::ElementPtr<ocs::tom::ST>::create();
   ep3_st_4->set<ST_name>("Test_Name38");
   lp2_st_2->push_back(std::move(ep3_st_4));
   
   ep2_astrlist_2->set<ASTRLIST_value>(std::move(lp2_st_2));
   lp_astrlist_2->push_back(std::move(ep2_astrlist_2));
   
   ep_cq->set<CQ_pe_list>(std::move(lp_astrlist_2));

   auto lp_abool = ocs::tom::ListPtr<ocs::tom::ABOOL>::create("Rerun List");
   
   auto ep2_abool = ocs::tom::ElementPtr<ocs::tom::ABOOL>::create();
   ep2_abool->set<ABOOL_href>(ocs::tom::Host("Test_Name42"));
   ep2_abool->set<ABOOL_value>(true);
   lp_abool->push_back(std::move(ep2_abool));
   
   auto ep2_abool_2 = ocs::tom::ElementPtr<ocs::tom::ABOOL>::create();
   ep2_abool_2->set<ABOOL_href>(ocs::tom::Host("Test_Name43"));
   ep2_abool_2->set<ABOOL_value>(false);
   lp_abool->push_back(std::move(ep2_abool_2));

   ep_cq->set<CQ_rerun>(std::move(lp_abool));

   auto lp_aulng_2 = ocs::tom::ListPtr<ocs::tom::AULNG>::create("Job Slots");
   
   auto ep2_aulng_4 = ocs::tom::ElementPtr<ocs::tom::AULNG>::create();
   ep2_aulng_4->set<AULNG_href>(ocs::tom::Host("Test_Name44"));
   ep2_aulng_4->set<AULNG_value>(12);
   lp_aulng_2->push_back(std::move(ep2_aulng_4));
   
   auto ep2_aulng_5 = ocs::tom::ElementPtr<ocs::tom::AULNG>::create();
   ep2_aulng_5->set<AULNG_href>(ocs::tom::Host("Test_Name45"));
   ep2_aulng_5->set<AULNG_value>(16);
   lp_aulng_2->push_back(std::move(ep2_aulng_5));

   auto ep2_aulng_6 = ocs::tom::ElementPtr<ocs::tom::AULNG>::create();
   ep2_aulng_6->set<AULNG_href>(ocs::tom::Host("Test_Name45_1"));
   ep2_aulng_6->set<AULNG_value>(16);
   lp_aulng_2->push_back(std::move(ep2_aulng_6));

   auto ep2_aulng_7 = ocs::tom::ElementPtr<ocs::tom::AULNG>::create();
   ep2_aulng_7->set<AULNG_href>(ocs::tom::Host("Test_Name45_2"));
   ep2_aulng_7->set<AULNG_value>(16);
   lp_aulng_2->push_back(std::move(ep2_aulng_7));

   auto ep2_aulng_8 = ocs::tom::ElementPtr<ocs::tom::AULNG>::create();
   ep2_aulng_8->set<AULNG_href>(ocs::tom::Host("Test_Name45_3"));
   ep2_aulng_8->set<AULNG_value>(16);
   lp_aulng_2->push_back(std::move(ep2_aulng_8));

   ep_cq->set<CQ_job_slots>(std::move(lp_aulng_2));

   auto lp_astr_3 = ocs::tom::ListPtr<ocs::tom::ASTR>::create("Tmp Dir List");
   
   auto ep2_astr_5 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ep2_astr_5->set<ASTR_href>(ocs::tom::Host("Test_Name46"));
   ep2_astr_5->set<ASTR_value>("value");
   lp_astr_3->push_back(std::move(ep2_astr_5));
   
   auto ep2_astr_6 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ep2_astr_6->set<ASTR_href>(ocs::tom::Host("Test_Name47"));
   ep2_astr_6->set<ASTR_value>("value");
   lp_astr_3->push_back(std::move(ep2_astr_6));

   ep_cq->set<CQ_tmpdir>(std::move(lp_astr_3));

   auto lp_astr_4 = ocs::tom::ListPtr<ocs::tom::ASTR>::create("Shell List");
   
   auto ep2_astr_7 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ep2_astr_7->set<ASTR_href>(ocs::tom::Host("Test_Name48"));
   ep2_astr_7->set<ASTR_value>("value");
   lp_astr_4->push_back(std::move(ep2_astr_7));
   
   auto ep2_astr_8 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ep2_astr_8->set<ASTR_href>(ocs::tom::Host("Test_Name49"));
   ep2_astr_8->set<ASTR_value>("value");
   lp_astr_4->push_back(std::move(ep2_astr_8));

   ep_cq->set<CQ_shell>(std::move(lp_astr_4));

   auto lp_astr_5 = ocs::tom::ListPtr<ocs::tom::ASTR>::create("Prolog List");
   
   auto ep2_astr_9 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ep2_astr_9->set<ASTR_href>(ocs::tom::Host("Test_Name50"));
   ep2_astr_9->set<ASTR_value>("value");
   lp_astr_5->push_back(std::move(ep2_astr_9));
   
   auto ep2_astr_10 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ep2_astr_10->set<ASTR_href>(ocs::tom::Host("Test_Name51"));
   ep2_astr_10->set<ASTR_value>("value");
   lp_astr_5->push_back(std::move(ep2_astr_10));

   ep_cq->set<CQ_prolog>(std::move(lp_astr_5));

   auto lp_astr_6 = ocs::tom::ListPtr<ocs::tom::ASTR>::create("Epilog List");
   
   auto ep2_astr_11 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ep2_astr_11->set<ASTR_href>(ocs::tom::Host("Test_Name52"));
   ep2_astr_11->set<ASTR_value>("value");
   lp_astr_6->push_back(std::move(ep2_astr_11));
   
   auto ep2_astr_12 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ep2_astr_12->set<ASTR_href>(ocs::tom::Host("Test_Name53"));
   ep2_astr_12->set<ASTR_value>("value");
   lp_astr_6->push_back(std::move(ep2_astr_12));

   ep_cq->set<CQ_epilog>(std::move(lp_astr_6));

   auto lp_astr_7 = ocs::tom::ListPtr<ocs::tom::ASTR>::create("Shell Start Modes");
   
   auto ep2_astr_13 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ep2_astr_13->set<ASTR_href>(ocs::tom::Host("Test_Name54"));
   ep2_astr_13->set<ASTR_value>("value");
   lp_astr_7->push_back(std::move(ep2_astr_13));
   
   auto ep2_astr_14 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ep2_astr_14->set<ASTR_href>(ocs::tom::Host("Test_Name55"));
   ep2_astr_14->set<ASTR_value>("value");
   lp_astr_7->push_back(std::move(ep2_astr_14));

   ep_cq->set<CQ_shell_start_mode>(std::move(lp_astr_7));

   auto lp_astr_8 = ocs::tom::ListPtr<ocs::tom::ASTR>::create("Shell Start Methods");
   
   auto ep2_astr_15 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ep2_astr_15->set<ASTR_href>(ocs::tom::Host("Test_Name56"));
   ep2_astr_15->set<ASTR_value>("value");
   lp_astr_8->push_back(std::move(ep2_astr_15));
   
   auto ep2_astr_16 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ep2_astr_16->set<ASTR_href>(ocs::tom::Host("Test_Name57"));
   ep2_astr_16->set<ASTR_value>("value");
   lp_astr_8->push_back(std::move(ep2_astr_16));

   ep_cq->set<CQ_starter_method>(std::move(lp_astr_8));

   auto lp_astr_9 = ocs::tom::ListPtr<ocs::tom::ASTR>::create("Shell Suspend Methods");
   
   auto ep2_astr_17 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ep2_astr_17->set<ASTR_href>(ocs::tom::Host("Test_Name58"));
   ep2_astr_17->set<ASTR_value>("value");
   lp_astr_9->push_back(std::move(ep2_astr_17));
   
   auto ep2_astr_18 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ep2_astr_18->set<ASTR_href>(ocs::tom::Host("Test_Name59"));
   ep2_astr_18->set<ASTR_value>("value");
   lp_astr_9->push_back(std::move(ep2_astr_18));

   ep_cq->set<CQ_suspend_method>(std::move(lp_astr_9));

   auto lp_astr_10 = ocs::tom::ListPtr<ocs::tom::ASTR>::create("Shell Resume Methods");
   
   auto ep2_astr_19 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ep2_astr_19->set<ASTR_href>(ocs::tom::Host("Test_Name60"));
   ep2_astr_19->set<ASTR_value>("value");
   lp_astr_10->push_back(std::move(ep2_astr_19));
   
   auto ep2_astr_20 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ep2_astr_20->set<ASTR_href>(ocs::tom::Host("Test_Name61"));
   ep2_astr_20->set<ASTR_value>("value");
   lp_astr_10->push_back(std::move(ep2_astr_20));

   ep_cq->set<CQ_resume_method>(std::move(lp_astr_10));

   auto lp_astr_11 = ocs::tom::ListPtr<ocs::tom::ASTR>::create("Shell Terminate Methods");
   
   auto ep2_astr_21 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ep2_astr_21->set<ASTR_href>(ocs::tom::Host("Test_Name62"));
   ep2_astr_21->set<ASTR_value>("value");
   lp_astr_11->push_back(std::move(ep2_astr_21));
   
   auto ep2_astr_22 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ep2_astr_22->set<ASTR_href>(ocs::tom::Host("Test_Name63"));
   ep2_astr_22->set<ASTR_value>("value");
   lp_astr_11->push_back(std::move(ep2_astr_22));

   ep_cq->set<CQ_terminate_method>(std::move(lp_astr_11));

   auto lp_ainter_3 = ocs::tom::ListPtr<ocs::tom::AINTER>::create("Notify List");
   
   auto ep2_ainter_5 = ocs::tom::ElementPtr<ocs::tom::AINTER>::create();
   ep2_ainter_5->set<AINTER_href>(ocs::tom::Host("Test_Name64"));
   ep2_ainter_5->set<AINTER_value>("46");
   lp_ainter_3->push_back(std::move(ep2_ainter_5));
   
   auto ep2_ainter_6 = ocs::tom::ElementPtr<ocs::tom::AINTER>::create();
   ep2_ainter_6->set<AINTER_href>(ocs::tom::Host("Test_Name65"));
   ep2_ainter_6->set<AINTER_value>("77");
   lp_ainter_3->push_back(std::move(ep2_ainter_6));

   ep_cq->set<CQ_notify>(std::move(lp_ainter_3));

   auto lp_ausrlist = ocs::tom::ListPtr<ocs::tom::AUSRLIST>::create("Owner List");
   
   auto ep2_ausrlist = ocs::tom::ElementPtr<ocs::tom::AUSRLIST>::create();
   ep2_ausrlist->set<AUSRLIST_href>(ocs::tom::Host("Test_Name66"));
   
   auto lp2_us = ocs::tom::ListPtr<ocs::tom::US>::create("Userset List");
   
   auto ep3_us = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ep3_us->set<US_name>("Test_Name67");
   lp2_us->push_back(std::move(ep3_us));
   
   auto ep3_us_2 = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ep3_us_2->set<US_name>("Test_Name68");
   lp2_us->push_back(std::move(ep3_us_2));
   
   ep2_ausrlist->set<AUSRLIST_value>(std::move(lp2_us));
   lp_ausrlist->push_back(std::move(ep2_ausrlist));
 
   ep_cq->set<CQ_owner_list>(std::move(lp_ausrlist));

   auto lp_ausrlist_2 = ocs::tom::ListPtr<ocs::tom::AUSRLIST>::create("ACL List");
   
   auto ep2_ausrlist_2 = ocs::tom::ElementPtr<ocs::tom::AUSRLIST>::create();
   ep2_ausrlist_2->set<AUSRLIST_href>(ocs::tom::Host("Test_Name72"));
   
   auto lp2_us_2 = ocs::tom::ListPtr<ocs::tom::US>::create("Userset List");
   
   auto ep3_us_3 = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ep3_us_3->set<US_name>("Test_Name73");
   lp2_us_2->push_back(std::move(ep3_us_3));
   
   auto ep3_us_4 = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ep3_us_4->set<US_name>("Test_Name74");
   lp2_us_2->push_back(std::move(ep3_us_4));
   
   ep2_ausrlist_2->set<AUSRLIST_value>(std::move(lp2_us_2));
   lp_ausrlist_2->push_back(std::move(ep2_ausrlist_2));

   ep_cq->set<CQ_acl>(std::move(lp_ausrlist_2));

   auto lp_ausrlist_3 = ocs::tom::ListPtr<ocs::tom::AUSRLIST>::create("XUser List");
   
   auto ep2_ausrlist_3 = ocs::tom::ElementPtr<ocs::tom::AUSRLIST>::create();
   ep2_ausrlist_3->set<AUSRLIST_href>(ocs::tom::Host("Test_Name78"));
   
   auto lp2_us_3 = ocs::tom::ListPtr<ocs::tom::US>::create("Userset List");
   
   auto ep3_us_5 = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ep3_us_5->set<US_name>("Test_Name79");
   lp2_us_3->push_back(std::move(ep3_us_5));
   
   auto ep3_us_6 = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ep3_us_6->set<US_name>("Test_Name80");
   lp2_us_3->push_back(std::move(ep3_us_6));
   
   ep2_ausrlist_3->set<AUSRLIST_value>(std::move(lp2_us_3));
   lp_ausrlist_3->push_back(std::move(ep2_ausrlist_3));

   ep_cq->set<CQ_xacl>(std::move(lp_ausrlist_3));

   auto lp_asolist = ocs::tom::ListPtr<ocs::tom::ASOLIST>::create("Subordinate List");
   
   auto ep2_asolist = ocs::tom::ElementPtr<ocs::tom::ASOLIST>::create();
   ep2_asolist->set<ASOLIST_href>(ocs::tom::Host("Test_Name84"));

   auto lp2_so = ocs::tom::ListPtr<ocs::tom::SO>::create("Subordinate List");
   
   auto ep3_so = ocs::tom::ElementPtr<ocs::tom::SO>::create();
   ep3_so->set<SO_name>("Test_Name85");
   ep3_so->set<SO_threshold>(13);
   lp2_so->push_back(std::move(ep3_so));
   
   auto ep3_so_2 = ocs::tom::ElementPtr<ocs::tom::SO>::create();
   ep3_so_2->set<SO_name>("Test_Name86");
   ep3_so_2->set<SO_threshold>(16);
   lp2_so->push_back(std::move(ep3_so_2));

   ep2_asolist->set<ASOLIST_value>(std::move(lp2_so));
   
   lp_asolist->push_back(std::move(ep2_asolist));
   
   ep_cq->set<CQ_subordinate_list>(std::move(lp_asolist));
  
   auto lp_acelist_3 = ocs::tom::ListPtr<ocs::tom::ACELIST>::create("Consumable Configs List");
   
   auto ep2_acelist_3 = ocs::tom::ElementPtr<ocs::tom::ACELIST>::create();
   ep2_acelist_3->set<ACELIST_href>(ocs::tom::Host("Test_Name93"));

   auto lp2_ce_2 = ocs::tom::ListPtr<ocs::tom::CE>::create("Complex List");
   
   auto ep3_ce_2 = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ep3_ce_2->set<CE_name>("Test_Name94");
   ep3_ce_2->set<CE_stringval>("stringval");
   lp2_ce_2->push_back(std::move(ep3_ce_2));

   ep2_acelist_3->set<ACELIST_value>(std::move(lp2_ce_2));
   
   lp_acelist_3->push_back(std::move(ep2_acelist_3));

   ep_cq->set<CQ_consumable_config_list>(std::move(lp_acelist_3));
   
   auto lp_aprjlist = ocs::tom::ListPtr<ocs::tom::APRJLIST>::create("Projects List");
   
   auto ep2_aprjlist = ocs::tom::ElementPtr<ocs::tom::APRJLIST>::create();
   ep2_aprjlist->set<APRJLIST_href>(ocs::tom::Host("Test_Name96"));
   
   auto lp2_pr = ocs::tom::ListPtr<ocs::tom::PR>::create("Project List");
   
   auto ep3_pr = ocs::tom::ElementPtr<ocs::tom::PR>::create();
   ep3_pr->set<PR_name>("Test_Name97");
   lp2_pr->push_back(std::move(ep3_pr));
   
   auto ep3_pr_2 = ocs::tom::ElementPtr<ocs::tom::PR>::create();
   ep3_pr_2->set<PR_name>("Test_Name98");
   lp2_pr->push_back(std::move(ep3_pr_2));
   
   ep2_aprjlist->set<APRJLIST_value>(std::move(lp2_pr));
   lp_aprjlist->push_back(std::move(ep2_aprjlist));

   ep_cq->set<CQ_projects>(std::move(lp_aprjlist));

   auto lp_aprjlist_2 = ocs::tom::ListPtr<ocs::tom::APRJLIST>::create("XProjects List");
   
   auto ep2_aprjlist_2 = ocs::tom::ElementPtr<ocs::tom::APRJLIST>::create();
   ep2_aprjlist_2->set<APRJLIST_href>(ocs::tom::Host("Test_Name102"));
   
   auto lp2_pr_2 = ocs::tom::ListPtr<ocs::tom::PR>::create("Project List");
   
   auto ep3_pr_3 = ocs::tom::ElementPtr<ocs::tom::PR>::create();
   ep3_pr_3->set<PR_name>("Test_Name103");
   lp2_pr_2->push_back(std::move(ep3_pr_3));
   
   auto ep3_pr_4 = ocs::tom::ElementPtr<ocs::tom::PR>::create();
   ep3_pr_4->set<PR_name>("Test_Name104");
   lp2_pr_2->push_back(std::move(ep3_pr_4));
   
   ep2_aprjlist_2->set<APRJLIST_value>(std::move(lp2_pr_2));
   lp_aprjlist_2->push_back(std::move(ep2_aprjlist_2));

   ep_cq->set<CQ_xprojects>(std::move(lp_aprjlist_2));

   auto lp_astr_12 = ocs::tom::ListPtr<ocs::tom::ASTR>::create("Calendar List");
   
   auto ep2_astr_23 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ep2_astr_23->set<ASTR_href>(ocs::tom::Host("Test_Name108"));
   ep2_astr_23->set<ASTR_value>("value");
   lp_astr_12->push_back(std::move(ep2_astr_23));
   
   auto ep2_astr_24 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ep2_astr_24->set<ASTR_href>(ocs::tom::Host("Test_Name109"));
   ep2_astr_24->set<ASTR_value>("value");
   lp_astr_12->push_back(std::move(ep2_astr_24));

   ep_cq->set<CQ_calendar>(std::move(lp_astr_12));

   auto lp_astr_13 = ocs::tom::ListPtr<ocs::tom::ASTR>::create("Initial State List");
   
   auto ep2_astr_25 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ep2_astr_25->set<ASTR_href>(ocs::tom::Host("Test_Name110"));
   ep2_astr_25->set<ASTR_value>("value");
   lp_astr_13->push_back(std::move(ep2_astr_25));
   
   auto ep2_astr_26 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ep2_astr_26->set<ASTR_href>(ocs::tom::Host("Test_Name111"));
   ep2_astr_26->set<ASTR_value>("value");
   lp_astr_13->push_back(std::move(ep2_astr_26));

   ep_cq->set<CQ_initial_state>(std::move(lp_astr_13));

   auto lp_atime = ocs::tom::ListPtr<ocs::tom::ATIME>::create("s_rt List");
   
   auto ep2_atime = ocs::tom::ElementPtr<ocs::tom::ATIME>::create();
   ep2_atime->set<ATIME_href>(ocs::tom::Host("Test_Name112"));
   ep2_atime->set<ATIME_value>("00:00:00");
   lp_atime->push_back(std::move(ep2_atime));
   
   auto ep2_atime_2 = ocs::tom::ElementPtr<ocs::tom::ATIME>::create();
   ep2_atime_2->set<ATIME_href>(ocs::tom::Host("Test_Name113"));
   ep2_atime_2->set<ATIME_value>("23:59:59");
   lp_atime->push_back(std::move(ep2_atime_2));

   ep_cq->set<CQ_s_rt>(std::move(lp_atime));

   auto lp_atime_2 = ocs::tom::ListPtr<ocs::tom::ATIME>::create("h_rt List");
   
   auto ep2_atime_3 = ocs::tom::ElementPtr<ocs::tom::ATIME>::create();
   ep2_atime_3->set<ATIME_href>(ocs::tom::Host("Test_Name114"));
   ep2_atime_3->set<ATIME_value>("00:00:00");
   lp_atime_2->push_back(std::move(ep2_atime_3));
   
   auto ep2_atime_4 = ocs::tom::ElementPtr<ocs::tom::ATIME>::create();
   ep2_atime_4->set<ATIME_href>(ocs::tom::Host("Test_Name115"));
   ep2_atime_4->set<ATIME_value>("23:59:59");
   lp_atime_2->push_back(std::move(ep2_atime_4));

   ep_cq->set<CQ_h_rt>(std::move(lp_atime_2));

   auto lp_atime_3 = ocs::tom::ListPtr<ocs::tom::ATIME>::create("s_cpu List");
   
   auto ep2_atime_5 = ocs::tom::ElementPtr<ocs::tom::ATIME>::create();
   ep2_atime_5->set<ATIME_href>(ocs::tom::Host("Test_Name116"));
   ep2_atime_5->set<ATIME_value>("00:00:00");
   lp_atime_3->push_back(std::move(ep2_atime_5));
   
   auto ep2_atime_6 = ocs::tom::ElementPtr<ocs::tom::ATIME>::create();
   ep2_atime_6->set<ATIME_href>(ocs::tom::Host("Test_Name117"));
   ep2_atime_6->set<ATIME_value>("23:59:59");
   lp_atime_3->push_back(std::move(ep2_atime_6));

   ep_cq->set<CQ_s_cpu>(std::move(lp_atime_3));

   auto lp_atime_4 = ocs::tom::ListPtr<ocs::tom::ATIME>::create("h_cpu List");
   
   auto ep2_atime_7 = ocs::tom::ElementPtr<ocs::tom::ATIME>::create();
   ep2_atime_7->set<ATIME_href>(ocs::tom::Host("Test_Name118"));
   ep2_atime_7->set<ATIME_value>("00:00:00");
   lp_atime_4->push_back(std::move(ep2_atime_7));
   
   auto ep2_atime_8 = ocs::tom::ElementPtr<ocs::tom::ATIME>::create();
   ep2_atime_8->set<ATIME_href>(ocs::tom::Host("Test_Name119"));
   ep2_atime_8->set<ATIME_value>("23:59:59");
   lp_atime_4->push_back(std::move(ep2_atime_8));

   ep_cq->set<CQ_h_cpu>(std::move(lp_atime_4));

   auto lp_amem = ocs::tom::ListPtr<ocs::tom::AMEM>::create("s_fsize List");
   
   auto ep2_amem = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ep2_amem->set<AMEM_href>(ocs::tom::Host("Test_Name120"));
   ep2_amem->set<AMEM_value>("1048576");
   lp_amem->push_back(std::move(ep2_amem));
   
   auto ep2_amem_2 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ep2_amem_2->set<AMEM_href>(ocs::tom::Host("Test_Name121"));
   ep2_amem_2->set<AMEM_value>("1024");
   lp_amem->push_back(std::move(ep2_amem_2));

   ep_cq->set<CQ_s_fsize>(std::move(lp_amem));

   auto lp_amem_2 = ocs::tom::ListPtr<ocs::tom::AMEM>::create("h_fsize List");
   
   auto ep2_amem_3 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ep2_amem_3->set<AMEM_href>(ocs::tom::Host("Test_Name122"));
   ep2_amem_3->set<AMEM_value>("1048576");
   lp_amem_2->push_back(std::move(ep2_amem_3));
   
   auto ep2_amem_4 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ep2_amem_4->set<AMEM_href>(ocs::tom::Host("Test_Name123"));
   ep2_amem_4->set<AMEM_value>("1024");
   lp_amem_2->push_back(std::move(ep2_amem_4));

   ep_cq->set<CQ_h_fsize>(std::move(lp_amem_2));

   auto lp_amem_3 = ocs::tom::ListPtr<ocs::tom::AMEM>::create("s_data List");
   
   auto ep2_amem_5 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ep2_amem_5->set<AMEM_href>(ocs::tom::Host("Test_Name124"));
   ep2_amem_5->set<AMEM_value>("1048576");
   lp_amem_3->push_back(std::move(ep2_amem_5));
   
   auto ep2_amem_6 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ep2_amem_6->set<AMEM_href>(ocs::tom::Host("Test_Name125"));
   ep2_amem_6->set<AMEM_value>("1024");
   lp_amem_3->push_back(std::move(ep2_amem_6));

   ep_cq->set<CQ_s_data>(std::move(lp_amem_3));

   auto lp_amem_4 = ocs::tom::ListPtr<ocs::tom::AMEM>::create("h_data List");
   
   auto ep2_amem_7 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ep2_amem_7->set<AMEM_href>(ocs::tom::Host("Test_Name126"));
   ep2_amem_7->set<AMEM_value>("1048576");
   lp_amem_4->push_back(std::move(ep2_amem_7));
   
   auto ep2_amem_8 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ep2_amem_8->set<AMEM_href>(ocs::tom::Host("Test_Name127"));
   ep2_amem_8->set<AMEM_value>("1024");
   lp_amem_4->push_back(std::move(ep2_amem_8));

   ep_cq->set<CQ_h_data>(std::move(lp_amem_4));

   auto lp_amem_5 = ocs::tom::ListPtr<ocs::tom::AMEM>::create("s_stack List");
   
   auto ep2_amem_9 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ep2_amem_9->set<AMEM_href>(ocs::tom::Host("Test_Name128"));
   ep2_amem_9->set<AMEM_value>("1048576");
   lp_amem_5->push_back(std::move(ep2_amem_9));
   
   auto ep2_amem_10 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ep2_amem_10->set<AMEM_href>(ocs::tom::Host("Test_Name129"));
   ep2_amem_10->set<AMEM_value>("1024");
   lp_amem_5->push_back(std::move(ep2_amem_10));

   ep_cq->set<CQ_s_stack>(std::move(lp_amem_5));

   auto lp_amem_6 = ocs::tom::ListPtr<ocs::tom::AMEM>::create("h_stack List");
   
   auto ep2_amem_11 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ep2_amem_11->set<AMEM_href>(ocs::tom::Host("Test_Name130"));
   ep2_amem_11->set<AMEM_value>("1048576");
   lp_amem_6->push_back(std::move(ep2_amem_11));
   
   auto ep2_amem_12 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ep2_amem_12->set<AMEM_href>(ocs::tom::Host("Test_Name131"));
   ep2_amem_12->set<AMEM_value>("1024");
   lp_amem_6->push_back(std::move(ep2_amem_12));

   ep_cq->set<CQ_h_stack>(std::move(lp_amem_6));

   auto lp_amem_7 = ocs::tom::ListPtr<ocs::tom::AMEM>::create("s_core List");
   
   auto ep2_amem_13 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ep2_amem_13->set<AMEM_href>(ocs::tom::Host("Test_Name132"));
   ep2_amem_13->set<AMEM_value>("1048576");
   lp_amem_7->push_back(std::move(ep2_amem_13));
   
   auto ep2_amem_14 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ep2_amem_14->set<AMEM_href>(ocs::tom::Host("Test_Name133"));
   ep2_amem_14->set<AMEM_value>("1024");
   lp_amem_7->push_back(std::move(ep2_amem_14));

   ep_cq->set<CQ_s_core>(std::move(lp_amem_7));

   auto lp_amem_8 = ocs::tom::ListPtr<ocs::tom::AMEM>::create("h_core List");
   
   auto ep2_amem_15 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ep2_amem_15->set<AMEM_href>(ocs::tom::Host("Test_Name134"));
   ep2_amem_15->set<AMEM_value>("1048576");
   lp_amem_8->push_back(std::move(ep2_amem_15));
   
   auto ep2_amem_16 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ep2_amem_16->set<AMEM_href>(ocs::tom::Host("Test_Name135"));
   ep2_amem_16->set<AMEM_value>("1024");
   lp_amem_8->push_back(std::move(ep2_amem_16));

   ep_cq->set<CQ_h_core>(std::move(lp_amem_8));

   auto lp_amem_9 = ocs::tom::ListPtr<ocs::tom::AMEM>::create("s_rss List");
   
   auto ep2_amem_17 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ep2_amem_17->set<AMEM_href>(ocs::tom::Host("Test_Name136"));
   ep2_amem_17->set<AMEM_value>("1048576");
   lp_amem_9->push_back(std::move(ep2_amem_17));
   
   auto ep2_amem_18 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ep2_amem_18->set<AMEM_href>(ocs::tom::Host("Test_Name137"));
   ep2_amem_18->set<AMEM_value>("1024");
   lp_amem_9->push_back(std::move(ep2_amem_18));

   ep_cq->set<CQ_s_rss>(std::move(lp_amem_9));

   auto lp_amem_10 = ocs::tom::ListPtr<ocs::tom::AMEM>::create("h_rss List");
   
   auto ep2_amem_19 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ep2_amem_19->set<AMEM_href>(ocs::tom::Host("Test_Name138"));
   ep2_amem_19->set<AMEM_value>("1048576");
   lp_amem_10->push_back(std::move(ep2_amem_19));
   
   auto ep2_amem_20 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ep2_amem_20->set<AMEM_href>(ocs::tom::Host("Test_Name139"));
   ep2_amem_20->set<AMEM_value>("1024");
   lp_amem_10->push_back(std::move(ep2_amem_20));

   ep_cq->set<CQ_h_rss>(std::move(lp_amem_10));

   auto lp_amem_11 = ocs::tom::ListPtr<ocs::tom::AMEM>::create("s_vmem List");
   
   auto ep2_amem_21 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ep2_amem_21->set<AMEM_href>(ocs::tom::Host("Test_Name140"));
   ep2_amem_21->set<AMEM_value>("1048576");
   lp_amem_11->push_back(std::move(ep2_amem_21));
   
   auto ep2_amem_22 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ep2_amem_22->set<AMEM_href>(ocs::tom::Host("Test_Name141"));
   ep2_amem_22->set<AMEM_value>("1024");
   lp_amem_11->push_back(std::move(ep2_amem_22));

   ep_cq->set<CQ_s_vmem>(std::move(lp_amem_11));

   auto lp_amem_12 = ocs::tom::ListPtr<ocs::tom::AMEM>::create("h_vmem List");
   
   auto ep2_amem_23 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ep2_amem_23->set<AMEM_href>(ocs::tom::Host("Test_Name142"));
   ep2_amem_23->set<AMEM_value>("1048576");
   lp_amem_12->push_back(std::move(ep2_amem_23));
   
   auto ep2_amem_24 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ep2_amem_24->set<AMEM_href>(ocs::tom::Host("Test_Name143"));
   ep2_amem_24->set<AMEM_value>("1024");
   lp_amem_12->push_back(std::move(ep2_amem_24));

   ep_cq->set<CQ_h_vmem>(std::move(lp_amem_12));

   /* Write a CQ file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, *ep_cq, false,
                                       CQ_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, false);
   
   /* Read a CQ file using flatfile spooling */
   ep_cq.reset();
   ep = ocs::tom::ElementPtr<ocs::tom::CQ>(spool_flatfile_read_object(&alp, ocs::tom::CQ::descriptor.e, nullptr,
                                   CQ_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1));
   
   /* Write a CQ file using flatfile spooling */
   file2 = spool_flatfile_write_object(&alp, *ep, false,
                                       CQ_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file2, false);
   
   ep.reset();
   
   ret = diff(file1, file2);
   
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   
   answer_list_output(&alp);   
   
   return ret;
}

/**
 * @brief Write a scheduler configuration with classic spooling, read it back and write it again
 * @return 0 if the file written first and the file written after the read are equal, non-zero otherwise
 */
static int SC_test() {
   int ret = 0;
   ocs::tom::ElementPtr<ocs::tom::SC> ep;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   const char *file1 = nullptr, *file2 = nullptr;
   
   auto ep_sc = ocs::tom::ElementPtr<ocs::tom::SC>::create();
   ep_sc->set<SC_algorithm>("algorithm");
   ep_sc->set<SC_schedule_interval>("30");
   ep_sc->set<SC_maxujobs>(1024);
   ep_sc->set<SC_queue_sort_method>(1);

   auto lp = ocs::tom::ListPtr<ocs::tom::CE>::create("Job Load Adjustments");

   auto ep2 = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ep2->set<CE_name>("Test_Name1");
   ep2->set<CE_shortcut>("tn1");
   ep2->set<CE_valtype>(1);
   ep2->set<CE_relop>(5);
   ep2->set<CE_consumable>(1);
   ep2->set<CE_defaultval>("15");
   ep2->set<CE_requestable>(REQU_NO);
   ep2->set<CE_urgency_weight>("25");
   ep2->set<CE_stringval>("stringval");
   lp->push_back(std::move(ep2));
   
   auto ep2_ce = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ep2_ce->set<CE_name>("Test_Name2");
   ep2_ce->set<CE_shortcut>("tn2");
   ep2_ce->set<CE_valtype>(1);
   ep2_ce->set<CE_relop>(5);
   ep2_ce->set<CE_consumable>(0);
   ep2_ce->set<CE_defaultval>("15");
   ep2_ce->set<CE_requestable>(REQU_YES);
   ep2_ce->set<CE_urgency_weight>("25");
   ep2_ce->set<CE_stringval>("stringval");
   lp->push_back(std::move(ep2_ce));

   ep_sc->set<SC_job_load_adjustments>(std::move(lp));
   ep_sc->set<SC_load_adjustment_decay_time>("45");
   ep_sc->set<SC_load_formula>("load_formula");
   ep_sc->set<SC_schedd_job_info>("schedd_job_info");
   ep_sc->set<SC_flush_submit_sec>(128);
   ep_sc->set<SC_flush_finish_sec>(512);
   ep_sc->set<SC_params>("params");
   ep_sc->set<SC_reprioritize_interval>("15");
   ep_sc->set<SC_halftime>(32);
   
   auto lp_ua = ocs::tom::ListPtr<ocs::tom::UA>::create("Usage Weight List");
   
   auto ep2_ua = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ep2_ua->set<UA_name>("Test_Name3");
   ep2_ua->set<UA_value>(11.22);
   lp_ua->push_back(std::move(ep2_ua));
   
   auto ep2_ua_2 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ep2_ua_2->set<UA_name>("Test_Name4");
   ep2_ua_2->set<UA_value>(33.44);
   lp_ua->push_back(std::move(ep2_ua_2));
      
   ep_sc->set<SC_usage_weight_list>(std::move(lp_ua));
   
   ep_sc->set<SC_compensation_factor>(11.22);
   ep_sc->set<SC_weight_user>(11.22);
   ep_sc->set<SC_weight_project>(33.44);
   ep_sc->set<SC_weight_department>(66.77);
   ep_sc->set<SC_weight_job>(88.99);
   ep_sc->set<SC_weight_tickets_functional>(36);
   ep_sc->set<SC_weight_tickets_share>(18);
   ep_sc->set<SC_share_override_tickets>(true);
   ep_sc->set<SC_share_functional_shares>(true);
   ep_sc->set<SC_max_functional_jobs_to_schedule>(2048);
   ep_sc->set<SC_report_pjob_tickets>(true);
   ep_sc->set<SC_max_pending_tasks_per_job>(256);
   ep_sc->set<SC_halflife_decay_list>("halflife_decay_list");
   ep_sc->set<SC_policy_hierarchy>("policy_hierarchy");
   ep_sc->set<SC_weight_ticket>(11.99);
   ep_sc->set<SC_weight_waiting_time>(22.88);
   ep_sc->set<SC_weight_deadline>(33.77);
   ep_sc->set<SC_weight_urgency>(44.66);
   ep_sc->set<SC_weight_priority>(55.99);
   ep_sc->set<SC_max_reservation>(33.00);
   ep_sc->set<SC_default_duration>("default_duration");

   /* Write a SC file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, *ep_sc, false,
                                       SC_fields,
                                       &qconf_comma_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, false);
   
   /* Read a SC file using flatfile spooling */
   ep_sc.reset();
   ep = ocs::tom::ElementPtr<ocs::tom::SC>(spool_flatfile_read_object(&alp, ocs::tom::SC::descriptor.e, nullptr,
                                   SC_fields, nullptr, true, &qconf_comma_sfi,
                                   SP_FORM_ASCII, nullptr, file1));
   
   /* Write a SC file using flatfile spooling */
   file2 = spool_flatfile_write_object(&alp, *ep, false,
                                       SC_fields,
                                       &qconf_comma_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file2, false);
   
   ep.reset();
   
   ret = diff(file1, file2);
   
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   
   answer_list_output(&alp);   
   
   return ret;
}

/**
 * @brief Write a queue instance with classic spooling, read it back and write it again
 * @return 0 if the file written first and the file written after the read are equal, non-zero otherwise
 */
static int QU_test() {
   int ret = 0;
   ocs::tom::ElementPtr<ocs::tom::QU> ep;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   spooling_field *fields = nullptr;
   const char *file1 = nullptr, *file2 = nullptr;
   
   auto ep_qu = ocs::tom::ElementPtr<ocs::tom::QU>::create();
   ep_qu->set<QU_qhostname>(ocs::tom::Host("Test_Name"));
   ep_qu->set<QU_qname>("Test_Name2");
   ep_qu->set<QU_state>(1);
   ep_qu->set<QU_pending_signal>(2);
   ep_qu->set<QU_pending_signal_delivery_time>(3000000);
   ep_qu->set<QU_version>(4);


   fields = sge_build_QU_field_list(false, true);
   
   /* Write a QU file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, *ep_qu, false,
                                       fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, false);
   
   /* Read a QU file using flatfile spooling */
   ep_qu.reset();
   ep = ocs::tom::ElementPtr<ocs::tom::QU>(spool_flatfile_read_object(&alp, ocs::tom::QU::descriptor.e, nullptr,
                                   fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1));
   
   /* Write a QU file using flatfile spooling */
   file2 = spool_flatfile_write_object(&alp, *ep, false,
                                       fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file2, false);
   
   ep.reset();
   
   ret = diff(file1, file2);
   
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   sge_free(&fields);
   
   answer_list_output(&alp);   
   
   return ret;
}

/**
 * @brief Write a host group with classic spooling, read it back and write it again
 * @return 0 if the file written first and the file written after the read are equal, non-zero otherwise
 */
static int HGRP_test() {
   int ret = 0;
   ocs::tom::ElementPtr<ocs::tom::HGRP> ep;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   const char *file1 = nullptr, *file2 = nullptr;
   
   auto ep_hgrp = ocs::tom::ElementPtr<ocs::tom::HGRP>::create();
   ep_hgrp->set<HGRP_name>(ocs::tom::Host("Test_Name"));
  
   ep_hgrp->emplace_back_in<HGRP_host_list, HR_name>("Test_Name2");
   ep_hgrp->emplace_back_in<HGRP_host_list, HR_name>("Test_Name3");
   ep_hgrp->emplace_back_in<HGRP_host_list, HR_name>("Test_Name4");
   
   
   /* Write a HGRP file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, *ep_hgrp, false,
                                       HGRP_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, false);
   
   /* Read a HGRP file using flatfile spooling */
   ep_hgrp.reset();
   ep = ocs::tom::ElementPtr<ocs::tom::HGRP>(spool_flatfile_read_object(&alp, ocs::tom::HGRP::descriptor.e, nullptr,
                                   HGRP_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1));
   
   /* Write a HGRP file using flatfile spooling */
   file2 = spool_flatfile_write_object(&alp, *ep, false,
                                       HGRP_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file2, false);
   ep.reset();
   
   ret = diff(file1, file2);
   
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   
   answer_list_output(&alp);   
   
   return ret;
}

/**
 * @brief Write a configuration with classic spooling, read it back and write it again
 * @return 0 if the file written first and the file written after the read are equal, non-zero otherwise
 */
static int CONF_test() {
   int ret = 0;
   ocs::tom::ElementPtr<ocs::tom::CONF> ep;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   spooling_field *fields;
   const char *file1 = nullptr;
   const char *file2 = nullptr;
   
   auto ep_conf = ocs::tom::ElementPtr<ocs::tom::CONF>::create();
   ep_conf->set<CONF_version>(101);
   ep_conf->set<CONF_name>(ocs::tom::Host("host1"));
   
   auto lp = ocs::tom::ListPtr<ocs::tom::CF>::create("Config List");
   
   auto ep2 = ocs::tom::ElementPtr<ocs::tom::CF>::create();
   ep2->set<CF_name>("gid_range");
   ep2->set<CF_value>("1000-1100");
   lp->push_back(std::move(ep2));
   
   auto ep2_cf = ocs::tom::ElementPtr<ocs::tom::CF>::create();
   ep2_cf->set<CF_name>("max_aj_tasks");
   ep2_cf->set<CF_value>("1001");
   lp->push_back(std::move(ep2_cf));
   
   auto ep2_cf_2 = ocs::tom::ElementPtr<ocs::tom::CF>::create();
   ep2_cf_2->set<CF_name>("admin_user");
   ep2_cf_2->set<CF_value>("testuser");
   lp->push_back(std::move(ep2_cf_2));
   
   auto ep2_cf_3 = ocs::tom::ElementPtr<ocs::tom::CF>::create();
   ep2_cf_3->set<CF_name>("user_lists");
   ep2_cf_3->set<CF_value>("me you someone_else");
   lp->push_back(std::move(ep2_cf_3));
   
   auto ep2_cf_4 = ocs::tom::ElementPtr<ocs::tom::CF>::create();
   ep2_cf_4->set<CF_name>("login_shells");
   ep2_cf_4->set<CF_value>("sh,ksh,csh,tcsh");
   lp->push_back(std::move(ep2_cf_4));

   auto ep2_cf_5 = ocs::tom::ElementPtr<ocs::tom::CF>::create();
   ep2_cf_5->set<CF_name>("execd_params");
   ep2_cf_5->set<CF_value>("PTF_MIN_PRIORITY=20,PTF_MAX_PRIORITY=0,SET_LIB_PATH=true");
   lp->push_back(std::move(ep2_cf_5));

   auto ep2_cf_6 = ocs::tom::ElementPtr<ocs::tom::CF>::create();
   ep2_cf_6->set<CF_name>("reporting_params");
   ep2_cf_6->set<CF_value>("accounting=true reporting=false flush_time=00:00:05 joblog=true sharelog=00:10:00=true");
   lp->push_back(std::move(ep2_cf_6));
   
   ep_conf->set<CONF_entries>(std::move(lp));
   

   fields = sge_build_CONF_field_list(true);
   
   /* Write a CONF file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, *ep_conf, false,
                                       fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, true);

   
   /* Read a CONF file using flatfile spooling */
   ep_conf.reset();
   ep = ocs::tom::ElementPtr<ocs::tom::CONF>(spool_flatfile_read_object(&alp, ocs::tom::CONF::descriptor.e, nullptr,
                                   fields, nullptr, false, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1));

   /* Write a CONF file using flatfile spooling */
   file2 = spool_flatfile_write_object(&alp, *ep, false,
                                       fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file2, true);

   ep.reset();
   
   ret = diff(file1, file2);
   
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   sge_free(&fields);
   
   answer_list_output(&alp);   
   
   return ret;
}

/**
 * @brief Write a list of two resource quota sets with classic spooling, read it back and write it again
 * @return 0 if the file written first and the file written after the read are equal, non-zero otherwise
 */
static int RQS_test() {
   int ret = 0;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   const char *file1 = nullptr;
   const char *file2 = nullptr;
   auto rqs_list = ocs::tom::ListPtr<ocs::tom::RQS>::create("test");

   auto ep = ocs::tom::ElementPtr<ocs::tom::RQS>::create();
   ep->set<RQS_name>("Test_Name1");
   ep->set<RQS_description>("Test Description");
   ep->set<RQS_enabled>(false);
   auto lp1 = ocs::tom::ListPtr<ocs::tom::RQR>::create("Rule_List");
   /* rule 1 */
   auto ep2 = ocs::tom::ElementPtr<ocs::tom::RQR>::create();
      auto ep3 = ocs::tom::ElementPtr<ocs::tom::RQRF>::create();
      ep3->set<RQRF_expand>(true);
      ep3->emplace_back_in<RQRF_scope, ST_name>("Test_User1");

      ep3->emplace_back_in<RQRF_xscope, ST_name>("Test_User2");
      ep3->emplace_back_in<RQRF_xscope, ST_name>("Test_User3");
      ep2->set<RQR_filter_users>(std::move(ep3));
      
      auto ep3_rqrf = ocs::tom::ElementPtr<ocs::tom::RQRF>::create();
      ep3_rqrf->set<RQRF_expand>(false);
      ep3_rqrf->emplace_back_in<RQRF_scope, ST_name>("Test_Host1");
      ep3_rqrf->emplace_back_in<RQRF_xscope, ST_name>("Test_Host2");
      ep3_rqrf->emplace_back_in<RQRF_xscope, ST_name>("Test_Host3");
      ep2->set<RQR_filter_hosts>(std::move(ep3_rqrf));

      auto ep3_rqrf_2 = ocs::tom::ElementPtr<ocs::tom::RQRF>::create();
      ep3_rqrf_2->set<RQRF_expand>(false);
      ep3_rqrf_2->emplace_back_in<RQRF_scope, ST_name>("Test_PE1");
      ep3_rqrf_2->emplace_back_in<RQRF_xscope, ST_name>("Test_PE2");
      ep3_rqrf_2->emplace_back_in<RQRF_xscope, ST_name>("Test_PE3");
      ep2->set<RQR_filter_pes>(std::move(ep3_rqrf_2));


      auto limit_list = ocs::tom::ListPtr<ocs::tom::RQRL>::create("Limit_List");
      auto limit = ocs::tom::ElementPtr<ocs::tom::RQRL>::create();
      limit->set<RQRL_name>("slots");
      limit->set<RQRL_value>("2*$num_proc");
      limit_list->push_back(std::move(limit));
      ep2->set<RQR_limit>(std::move(limit_list));
   lp1->push_back(std::move(ep2));
   /* rule 2 */
   auto ep2_rqr = ocs::tom::ElementPtr<ocs::tom::RQR>::create();
      auto ep3_rqrf_3 = ocs::tom::ElementPtr<ocs::tom::RQRF>::create();
      ep3_rqrf_3->set<RQRF_expand>(true);
      ep3_rqrf_3->emplace_back_in<RQRF_scope, ST_name>("Test_Queue1");

      ep3_rqrf_3->emplace_back_in<RQRF_xscope, ST_name>("Test_Queue2");
      ep2_rqr->set<RQR_filter_queues>(std::move(ep3_rqrf_3));

      auto limit_list_rqrl = ocs::tom::ListPtr<ocs::tom::RQRL>::create("Limit_List");
      auto limit_rqrl = ocs::tom::ElementPtr<ocs::tom::RQRL>::create();
      limit_rqrl->set<RQRL_name>("arch");
      limit_rqrl->set<RQRL_value>("lx-amd64");
      limit_list_rqrl->push_back(std::move(limit_rqrl));
      ep2_rqr->set<RQR_limit>(std::move(limit_list_rqrl));
   lp1->push_back(std::move(ep2_rqr));
   /* rule 3 */
   auto ep2_rqr_2 = ocs::tom::ElementPtr<ocs::tom::RQR>::create();
   ep2_rqr_2->set<RQR_name>("rule3");
      auto ep3_rqrf_4 = ocs::tom::ElementPtr<ocs::tom::RQRF>::create();
      ep3_rqrf_4->set<RQRF_expand>(true);
      ep3_rqrf_4->emplace_back_in<RQRF_scope, ST_name>("Test_Pe1");

      ep3_rqrf_4->emplace_back_in<RQRF_xscope, ST_name>("Test_Pe2");
      ep2_rqr_2->set<RQR_filter_pes>(std::move(ep3_rqrf_4));

      auto limit_list_rqrl_2 = ocs::tom::ListPtr<ocs::tom::RQRL>::create("Limit_List");
      auto limit_rqrl_2 = ocs::tom::ElementPtr<ocs::tom::RQRL>::create();
      limit_rqrl_2->set<RQRL_name>("mem");
      limit_rqrl_2->set<RQRL_value>("1G");
      limit_list_rqrl_2->push_back(std::move(limit_rqrl_2));
      ep2_rqr_2->set<RQR_limit>(std::move(limit_list_rqrl_2));
   lp1->push_back(std::move(ep2_rqr_2));
   ep->set<RQS_rule>(std::move(lp1));
   rqs_list->push_back(std::move(ep));

   /* rule 4 */
   auto ep_rqs = ocs::tom::ElementPtr<ocs::tom::RQS>::create();
   ep_rqs->set<RQS_name>("Test_Name2");
   ep_rqs->set<RQS_description>("Test Description");
   ep_rqs->set<RQS_enabled>(true);
   auto lp1_rqr = ocs::tom::ListPtr<ocs::tom::RQR>::create("Rule_List");
   auto ep2_rqr_3 = ocs::tom::ElementPtr<ocs::tom::RQR>::create();
      auto ep3_rqrf_5 = ocs::tom::ElementPtr<ocs::tom::RQRF>::create();
      ep3_rqrf_5->set<RQRF_expand>(false);
      ep3_rqrf_5->emplace_back_in<RQRF_scope, ST_name>("roland");

      ep3_rqrf_5->emplace_back_in<RQRF_xscope, ST_name>("andre");
      ep2_rqr_3->set<RQR_filter_users>(std::move(ep3_rqrf_5));

      auto limit_list_rqrl_3 = ocs::tom::ListPtr<ocs::tom::RQRL>::create("Limit_List");
      /* first limit */
      auto limit_rqrl_3 = ocs::tom::ElementPtr<ocs::tom::RQRL>::create();
      limit_rqrl_3->set<RQRL_name>("mem");
      limit_rqrl_3->set<RQRL_value>("10G");
      limit_list_rqrl_3->push_back(std::move(limit_rqrl_3));
      /* second limit */
      auto limit_rqrl_4 = ocs::tom::ElementPtr<ocs::tom::RQRL>::create();
      limit_rqrl_4->set<RQRL_name>("arch");
      limit_rqrl_4->set<RQRL_value>("sol-sparc64");
      limit_list_rqrl_3->push_back(std::move(limit_rqrl_4));

      ep2_rqr_3->set<RQR_limit>(std::move(limit_list_rqrl_3));
   lp1_rqr->push_back(std::move(ep2_rqr_3));
   ep_rqs->set<RQS_rule>(std::move(lp1_rqr));
   rqs_list->push_back(std::move(ep_rqs));


   /* Write a RQS file using classic spooling */
   file1 = spool_flatfile_write_list(&alp, *rqs_list, RQS_fields, &qconf_rqs_sfi, SP_DEST_TMP, SP_FORM_ASCII,
                                     file1, false);

   /* Read a RQS file using flatfile spooling */
   rqs_list.reset();
   rqs_list = ocs::tom::ListPtr<ocs::tom::RQS>(spool_flatfile_read_list(&alp, ocs::tom::RQS::descriptor.e, RQS_fields,
                                                                        nullptr, true, &qconf_rqs_sfi, SP_FORM_ASCII,
                                                                        nullptr, file1));

   /* Write a RQS file using flatfile spooling */
   file2 = spool_flatfile_write_list(&alp, *rqs_list, RQS_fields, &qconf_rqs_sfi, SP_DEST_TMP, SP_FORM_ASCII,
                                     file2, false);
   rqs_list.reset();

   ret = diff(file1, file2);

   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);

   answer_list_output(&alp);

   return ret;
}

/**
 * @brief Write an advance reservation with classic spooling, read it back and write it again
 * @return 0 if the file written first and the file written after the read are equal, non-zero otherwise
 */
static int AR_test() {
   int ret = 0;
   ocs::tom::ElementPtr<ocs::tom::AR> ep;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   const char *file1 = nullptr, *file2 = nullptr;
   
   auto ep_ar = ocs::tom::ElementPtr<ocs::tom::AR>::create();
   ep_ar->set<AR_id>(1);
   ep_ar->set<AR_name>("Test_Name");

   auto lp = ocs::tom::ListPtr<ocs::tom::RN>::create("range_list");
   auto ep1 = ocs::tom::ElementPtr<ocs::tom::RN>::create();
   ep1->set<RN_min>(1);
   ep1->set<RN_max>(2);
   ep1->set<RN_step>(3);
   lp->push_back(std::move(ep1));
   auto ep1_rn = ocs::tom::ElementPtr<ocs::tom::RN>::create();
   ep1_rn->set<RN_min>(4);
   ep1_rn->set<RN_max>(5);
   ep1_rn->set<RN_step>(6);
   lp->push_back(std::move(ep1_rn));
   ep_ar->set<AR_pe_range>(std::move(lp));

   auto lp_ce = ocs::tom::ListPtr<ocs::tom::CE>::create("resource_list");
   auto ep1_ce = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ep1_ce->set<CE_name>("ce1");
   ep1_ce->set<CE_stringval>("101");
   lp_ce->push_back(std::move(ep1_ce));
   auto ep1_ce_2 = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ep1_ce_2->set<CE_name>("ce2");
   ep1_ce_2->set<CE_doubleval>(102);
   lp_ce->push_back(std::move(ep1_ce_2));
   ep_ar->set<AR_resource_list>(std::move(lp_ce));

   ep_ar->emplace_back_in<AR_queue_list, QR_name>("queue1.q");
   ep_ar->emplace_back_in<AR_queue_list, QR_name>("queue2.q");
   ep_ar->emplace_back_in<AR_queue_list, QR_name>("queue3.q");
   ep_ar->emplace_back_in<AR_queue_list, QR_name>("queue4.q");

   ocs::tom::Element<ocs::tom::JG> ep1_jg = ep_ar->emplace_back_in<AR_granted_slots, JG_qname>("all.q@bla");
   ep1_jg.set<JG_slots>(5);
   ocs::tom::Element<ocs::tom::JG> ep1_jg_2 = ep_ar->emplace_back_in<AR_granted_slots, JG_qname>("my.q@bla");
   ep1_jg_2.set<JG_slots>(10);

   ep_ar->emplace_back_in<AR_mail_list, MR_user>("user1");
   ocs::tom::Element<ocs::tom::MR> ep1_mr = ep_ar->emplace_back_in<AR_mail_list, MR_user>("user2");
   ep1_mr.set<MR_host>("sun.com");

   ep_ar->emplace_back_in<AR_master_queue_list, QR_name>("queue1.q");
   ep_ar->emplace_back_in<AR_master_queue_list, QR_name>("queue2.q");
   ep_ar->emplace_back_in<AR_master_queue_list, QR_name>("queue3.q");
   ep_ar->emplace_back_in<AR_master_queue_list, QR_name>("queue4.q");

   ocs::tom::Element<ocs::tom::ARA> ep1_ara = ep_ar->emplace_back_in<AR_acl_list, ARA_name>("user1");
   ep1_ara.set<ARA_group>("group1");
   ocs::tom::Element<ocs::tom::ARA> ep1_ara_2 = ep_ar->emplace_back_in<AR_acl_list, ARA_name>("user2");
   ep1_ara_2.set<ARA_group>("group2");
   ep_ar->emplace_back_in<AR_acl_list, ARA_name>("@userset1");
   ep_ar->emplace_back_in<AR_acl_list, ARA_name>("@userset2");

  
   
   /* Write a AR file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, *ep_ar, false,
                                       AR_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, true);

   /* Read a AR file using flatfile spooling */
   ep_ar.reset();
   ep = ocs::tom::ElementPtr<ocs::tom::AR>(spool_flatfile_read_object(&alp, ocs::tom::AR::descriptor.e, nullptr,
                                   AR_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1));
   
   /* Write a AR file using flatfile spooling */
   file2 = spool_flatfile_write_object(&alp, *ep, false,
                                       AR_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file2, true);
   ep.reset();

   ret = diff(file1, file2);
  
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   
   answer_list_output(&alp);
   return ret;
}

/**
 * @brief Write a parallel environment holding only its name with classic spooling, read it back and write it again
 * @return 0 if the file written first and the file written after the read are equal, non-zero otherwise
 */
static int PE_minimal_test() {
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   auto ep = ocs::tom::ElementPtr<ocs::tom::PE>::create();
   const char *file1 = nullptr;
   const char *file2 = nullptr;

   // only name set — all other fields remain at CULL defaults
   ep->set<PE_name>("minimal_pe");

   file1 = spool_flatfile_write_object(&alp, *ep, false,
                                       PE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file1, false);
   ep.reset();
   ep = ocs::tom::ElementPtr<ocs::tom::PE>(spool_flatfile_read_object(&alp, ocs::tom::PE::descriptor.e, nullptr,
                                   PE_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1));
   file2 = spool_flatfile_write_object(&alp, *ep, false,
                                       PE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file2, false);
   ep.reset();
   int ret = diff(file1, file2);
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   answer_list_output(&alp);
   return ret;
}

/**
 * @brief SECURITY REGRESSION (CS-2352, MEDIUM-SPOOL-001, CWE-732/CWE-276)
 *
 * spool_flatfile_open_file() used to create SP_DEST_SPOOL files with mode 0666 (world-/group-writable,
 * world-readable), relying solely on the daemon umask. Spool files hold authoritative cluster state and must be
 * owner-only (0600). Write a minimal object via SP_DEST_SPOOL under umask(0) so the on-disk mode reflects exactly
 * the requested create mode, then assert it is 0600 with no group/other bits. Pre-fix the file is 0666, so this
 * returns non-zero.
 * @return 0 if the file is created with mode 0600, 1 otherwise
 */
static int SPOOL_perm_test() {
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   auto ep = ocs::tom::ElementPtr<ocs::tom::PE>::create();
   ep->set<PE_name>("perm_pe");

   char path[1024];
   snprintf(path, sizeof(path), "/tmp/test_spool_perm.%ld", (long)getpid());
   unlink(path);

   // Neutralise the ambient umask so the resulting mode is exactly what the
   // code requested (otherwise it would be mode & ~umask).
   mode_t old_umask = umask(0);
   const char *result = spool_flatfile_write_object(&alp, *ep, false,
                                                    PE_fields, &qconf_sfi,
                                                    SP_DEST_SPOOL, SP_FORM_ASCII,
                                                    path, false);
   umask(old_umask);
   ep.reset();

   int ret = 1;
   struct stat sb{};
   if (result != nullptr && stat(path, &sb) == 0) {
      const mode_t perm = sb.st_mode & 07777;
      if (perm == (S_IRUSR | S_IWUSR)) {
         ret = 0;
      } else {
         printf("   spool file mode = 0%03o (expected 0600)\n", (unsigned)perm);
      }
   } else {
      printf("   could not write/stat spool file via SP_DEST_SPOOL\n");
   }

   unlink(path);
   sge_free(&result);
   answer_list_output(&alp);
   return ret;
}

/**
 * @brief SECURITY REGRESSION (CS-2352 directory hardening, CWE-732 / CWE-276)
 *
 * CS-2352 hardened the classic-spool *file* mode to 0600, but the directories that contain those files were created
 * 0755 - world-readable and traversable - so their entry names (job IDs, queue-instance and object names, per-host
 * config) still leaked to any local user able to reach the spool tree. The classic-spool startup function now
 * creates its directories owner-only (0700). This exercises the real path via spool_classic_default_startup_func(),
 * which - among the object directories - creates the "configs" subdirectory holding the global and per-host
 * configurations (which now live in the spool dir, not the common dir), and asserts the resulting mode is exactly
 * 0700. Red->green: pre-fix the directory is 0755.
 * @return 0 if the directory is created with mode 0700, 1 otherwise
 */
static int SPOOL_dir_perm_test() {
   ocs::tom::ListPtr<ocs::tom::AN> alp;

   char base[1024];
   snprintf(base, sizeof(base), "/tmp/test_spool_dirperm.%ld", (long)getpid());

   // spool_classic_default_startup_func() chdirs into the spool dir; remember the
   // current working directory so we can restore it afterwards (and not strand
   // the process in a directory we are about to delete).
   char cwd[4096];
   if (getcwd(cwd, sizeof(cwd)) == nullptr) {
      printf("   getcwd failed\n");
      return 1;
   }

   // clean any leftover from a previous run, then create the spool dir
   char cmd[1200];
   snprintf(cmd, sizeof(cmd), "rm -rf %s", base);
   if (system(cmd) != 0) { /* best effort cleanup */ }
   if (mkdir(base, 0755) != 0) {
      printf("   could not create test spool dir %s\n", base);
      return 1;
   }

   auto rule = ocs::tom::ElementPtr<ocs::tom::SPR>::create();
   rule->set<SPR_url>(base);

   // Neutralise the ambient umask so the resulting mode is exactly what the
   // code requested (otherwise it would be mode & ~umask).
   mode_t old_umask = umask(0);
   bool ok = spool_classic_default_startup_func(&alp, *rule, false);
   umask(old_umask);
   rule.reset();

   // restore the working directory changed by the startup function
   if (chdir(cwd) != 0) { /* best effort */ }

   // "configs" mirrors CONFIG_DIR (uti/sge.h); kept as a literal here so the test
   // needs no extra include.
   char sub[1152];
   snprintf(sub, sizeof(sub), "%s/%s", base, "configs");

   int ret = 1;
   struct stat sb{};
   if (ok && stat(sub, &sb) == 0 && S_ISDIR(sb.st_mode)) {
      const mode_t perm = sb.st_mode & 07777;
      if (perm == (S_IRUSR | S_IWUSR | S_IXUSR)) {   // 0700
         ret = 0;
      } else {
         printf("   configs dir mode = 0%03o (expected 0700)\n", (unsigned)perm);
      }
   } else {
      printf("   could not create/stat configs dir via default_startup_func\n");
   }

   snprintf(cmd, sizeof(cmd), "rm -rf %s", base);
   if (system(cmd) != 0) { /* best effort cleanup */ }
   answer_list_output(&alp);
   return ret;
}

/**
 * @brief Security regression test for CS-2384: qmaster restricts an existing spool tree.
 *
 * A patch update replaces only the binaries, so the spool tree of an older
 * version keeps its 0755 directories and 0644 files. The classic startup
 * function restricts them: the test prepares such a tree, runs
 * spool_classic_default_startup_func() under umask(0) and expects every
 * object directory 0700 and the root level spool files (scheduler
 * configuration, share tree) 0600.
 *
 * @return 0 if all modes are owner-only afterwards, 1 otherwise
 */
static int SPOOL_restrict_existing_test() {
   ocs::tom::ListPtr<ocs::tom::AN> alp;

   char base[1024];
   snprintf(base, sizeof(base), "/tmp/test_spool_restrict.%ld", (long)getpid());

   // the startup function changes the working directory
   char cwd[4096];
   if (getcwd(cwd, sizeof(cwd)) == nullptr) {
      printf("   getcwd failed\n");
      return 1;
   }

   char cmd[1200];
   snprintf(cmd, sizeof(cmd), "rm -rf %s", base);
   if (system(cmd) != 0) { /* best effort cleanup */ }
   if (mkdir(base, 0755) != 0) {
      printf("   could not create test spool dir %s\n", base);
      return 1;
   }

   // what an older version left behind
   const mode_t old_umask = umask(0);
   char path[1152];
   for (const char *d : {JOB_DIR, CONFIG_DIR, ROLE_DIR}) {
      snprintf(path, sizeof(path), "%s/%s", base, d);
      mkdir(path, 0755);
   }
   const char *files[] = {SCHED_CONF_FILE, SHARETREE_FILE};
   for (const char *f : files) {
      snprintf(path, sizeof(path), "%s/%s", base, f);
      FILE *fp = fopen(path, "w");
      if (fp != nullptr) {
         fprintf(fp, "# old\n");
         fclose(fp);
      }
      chmod(path, 0644);
   }

   auto rule = ocs::tom::ElementPtr<ocs::tom::SPR>::create();
   rule->set<SPR_url>(base);
   const bool ok = spool_classic_default_startup_func(&alp, *rule, false);
   rule.reset();
   umask(old_umask);
   if (chdir(cwd) != 0) { /* best effort */ }

   int ret = ok ? 0 : 1;
   const char *dirs[] = {JOB_DIR, CQUEUE_DIR, QINSTANCES_DIR, EXECHOST_DIR, CENTRY_DIR, EXEC_DIR, PE_DIR,
                         CKPTOBJ_DIR, USERSET_DIR, CAL_DIR, HGROUP_DIR, USER_DIR, PROJECT_DIR,
                         RESOURCEQUOTAS_DIR, AR_DIR, ROLE_DIR, CONFIG_DIR};
   struct stat sb{};
   for (const char *d : dirs) {
      snprintf(path, sizeof(path), "%s/%s", base, d);
      if (stat(path, &sb) != 0 || (sb.st_mode & 07777) != S_IRWXU) {
         printf("   %s: mode 0%03o, expected 0700\n", d, (unsigned)(sb.st_mode & 07777));
         ret = 1;
      }
   }
   for (const char *f : files) {
      snprintf(path, sizeof(path), "%s/%s", base, f);
      if (stat(path, &sb) != 0 || (sb.st_mode & 07777) != (S_IRUSR | S_IWUSR)) {
         printf("   %s: mode 0%03o, expected 0600\n", f, (unsigned)(sb.st_mode & 07777));
         ret = 1;
      }
   }

   snprintf(cmd, sizeof(cmd), "rm -rf %s", base);
   if (system(cmd) != 0) { /* best effort cleanup */ }
   answer_list_output(&alp);
   return ret;
}

/**
 * @brief Regression for the classic spooling parameter change (RBAC config-in-spool-dir work)
 *
 * the configuration objects moved into the spool directory, so the classic spooling parameter is now a single
 * absolute spool directory path. The obsolete two-argument "<common_dir>;<spool_dir>" form must be rejected (hard
 * error, not silently accepted), as must a null or non-absolute path. The accept path (a valid single directory)
 * allocates a full spooling context and is exercised by qmaster startup and the testsuite, so it is intentionally
 * not re-created here; this test focuses on the rejection contract, which returns nullptr without allocating.
 * @return 0 if every argument is rejected, 1 otherwise
 */
static int SPOOL_classic_params_test() {
   struct { const char *args; const char *desc; } rejected[] = {
      {nullptr,          "null args"},
      {"relative/path",  "non-absolute path"},
      {"/common;/spool", "legacy <common_dir>;<spool_dir> two-argument form"},
      {"/a;/b;/c",       "malformed multi-token form"},
   };

   int ret = 0;
   for (const auto &c : rejected) {
      ocs::tom::ListPtr<ocs::tom::AN> alp;
      ocs::tom::ElementPtr<ocs::tom::SPC> ctx(spool_classic_create_context(&alp, c.args));
      if (ctx) {
         printf("   spool_classic_create_context accepted %s (expected reject)\n", c.desc);
         ctx.reset();
         ret = 1;
      }
      alp.reset();
   }
   return ret;
}

// SECURITY REGRESSION (CS-2364, LOW-SPOOL-002, CWE-22):
// spool_flatfile_key_is_safe() is the defence-in-depth guard at the spool sink.
// An object key is used to build a spool path, so it must not escape the spool
// directory. A key may legitimately be a multi-component relative path - a queue
// instance is spooled as "<cqueue>/<host>" and a job script key embeds a path -
// so '/' is allowed as a separator. The guard validates each component instead:
// the key must be relative (no leading '/') and no component may be empty, "."
// or ".." or start with '.' (".hidden"). Ordinary keys (including FQDNs with '-'
// and '.') and well-formed multi-component keys are accepted; traversal is not.
static int SPOOL_key_safe_test() {
   struct { const char *key; bool expect_safe; } cases[] = {
      {"validkey",                    true},
      {"host-01.example.com",         true},   // '-' and '.' are fine when not leading
      {"12345.7",                     true},   // job-style key
      {"cqueue/host",                 true},   // queue-instance key: '/' is a valid separator
      {"q_issue1760/v01701.fritz.box",true},   // realistic queue-instance key (CS-2364 regression)
      {"JOBSCRIPT:123.job_scripts/123", true}, // job script key as jobscript_get_key() builds it
      {"123.1 1.host",                true},   // PE task key
      {"../../tmp/x",                 false},
      {"/etc/passwd",                 false},   // absolute path
      {"..",                          false},
      {".",                           false},
      {".hidden",                     false},
      {"a/../b",                      false},   // traversal in a later component
      {"a/./b",                       false},   // "." component
      {"a//b",                        false},   // empty component
      {"a/",                          false},   // trailing slash -> empty last component
      {"a/.hidden",                   false},   // leading '.' in a later component
      {nullptr,                       false},
   };
   int ret = 0;
   for (const auto &c : cases) {
      if (spool_flatfile_key_is_safe(c.key) != c.expect_safe) {
         printf("   spool_flatfile_key_is_safe(%s) = %s, expected %s\n",
                c.key ? c.key : "nullptr",
                spool_flatfile_key_is_safe(c.key) ? "true" : "false",
                c.expect_safe ? "true" : "false");
         ret = 1;
      }
   }
   return ret;
}

/**
 * @brief Write a complex entry holding only the mandatory fields with classic spooling, read it back and write it
 *        again
 * @return 0 if the file written first and the file written after the read are equal, non-zero otherwise
 */
static int CE_minimal_test() {
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   auto ep = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   const char *file1 = nullptr;
   const char *file2 = nullptr;

   // minimum valid CE: name + shortcut + lowest valid enum values; no defaultval or urgency_weight
   ep->set<CE_name>("minimal_ce");
   ep->set<CE_shortcut>("mc");
   ep->set<CE_valtype>(1);            // INT = first valid type (0 is not valid)
   ep->set<CE_relop>(CMPLXEQ_OP);    // == = first valid relop
   ep->set<CE_requestable>(REQU_NO);  // NO = first valid requestable

   file1 = spool_flatfile_write_object(&alp, *ep, false,
                                       CE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file1, false);
   ep.reset();
   ep = ocs::tom::ElementPtr<ocs::tom::CE>(spool_flatfile_read_object(&alp, ocs::tom::CE::descriptor.e, nullptr,
                                   CE_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1));
   file2 = spool_flatfile_write_object(&alp, *ep, false,
                                       CE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file2, false);
   ep.reset();
   int ret = diff(file1, file2);
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   answer_list_output(&alp);
   return ret;
}

/**
 * @brief Write a parallel environment with zero slots with classic spooling, read it back and write it again
 * @return 0 if the file written first and the file written after the read are equal, non-zero otherwise
 */
static int PE_zero_slots_test() {
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   auto ep = ocs::tom::ElementPtr<ocs::tom::PE>::create();
   const char *file1 = nullptr;
   const char *file2 = nullptr;

   ep->set<PE_name>("zero_slots_pe");
   ep->set<PE_slots>(0);

   file1 = spool_flatfile_write_object(&alp, *ep, false,
                                       PE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file1, false);
   ep.reset();
   ep = ocs::tom::ElementPtr<ocs::tom::PE>(spool_flatfile_read_object(&alp, ocs::tom::PE::descriptor.e, nullptr,
                                   PE_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1));
   file2 = spool_flatfile_write_object(&alp, *ep, false,
                                       PE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file2, false);
   ep.reset();
   int ret = diff(file1, file2);
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   answer_list_output(&alp);
   return ret;
}

/**
 * @brief Write a parallel environment with the maximum number of slots with classic spooling, read it back and write
 *        it again
 * @return 0 if the file written first and the file written after the read are equal, non-zero otherwise
 */
static int PE_max_slots_test() {
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   auto ep = ocs::tom::ElementPtr<ocs::tom::PE>::create();
   const char *file1 = nullptr;
   const char *file2 = nullptr;

   ep->set<PE_name>("max_slots_pe");
   ep->set<PE_slots>(4294967295U);   ///< UINT32_MAX — stress-tests ulong serialization

   file1 = spool_flatfile_write_object(&alp, *ep, false,
                                       PE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file1, false);
   ep.reset();
   ep = ocs::tom::ElementPtr<ocs::tom::PE>(spool_flatfile_read_object(&alp, ocs::tom::PE::descriptor.e, nullptr,
                                   PE_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1));
   file2 = spool_flatfile_write_object(&alp, *ep, false,
                                       PE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file2, false);
   ep.reset();
   int ret = diff(file1, file2);
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   answer_list_output(&alp);
   return ret;
}

/**
 * @brief Write a complex entry with zero values with classic spooling, read it back and write it again
 * @return 0 if the file written first and the file written after the read are equal, non-zero otherwise
 */
static int CE_zero_values_test() {
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   auto ep = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   const char *file1 = nullptr;
   const char *file2 = nullptr;

   // enum fields at minimum valid values; consumable=0 (CONSUMABLE_NO is genuinely 0);
   // string fields explicitly set to "0" — tests zero-valued numeric strings round-trip
   ep->set<CE_name>("zero_ce");
   ep->set<CE_shortcut>("zc");
   ep->set<CE_valtype>(1);            // INT (0 is not a valid type)
   ep->set<CE_relop>(CMPLXEQ_OP);    // == (0 is not a valid relop)
   ep->set<CE_consumable>(CONSUMABLE_NO);  // 0 = "NO" (valid)
   ep->set<CE_defaultval>("0");
   ep->set<CE_requestable>(REQU_NO);  // 1 (0 is not a valid requestable)
   ep->set<CE_urgency_weight>("0");

   file1 = spool_flatfile_write_object(&alp, *ep, false,
                                       CE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file1, false);
   ep.reset();
   ep = ocs::tom::ElementPtr<ocs::tom::CE>(spool_flatfile_read_object(&alp, ocs::tom::CE::descriptor.e, nullptr,
                                   CE_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1));
   file2 = spool_flatfile_write_object(&alp, *ep, false,
                                       CE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file2, false);
   ep.reset();
   int ret = diff(file1, file2);
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   answer_list_output(&alp);
   return ret;
}

/**
 * @brief Write a complex entry with maximum values with classic spooling, read it back and write it again
 * @return 0 if the file written first and the file written after the read are equal, non-zero otherwise
 */
static int CE_max_values_test() {
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   auto ep = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   const char *file1 = nullptr;
   const char *file2 = nullptr;

   // large numeric string in urgency_weight — tests wide-value serialization
   ep->set<CE_name>("max_ce");
   ep->set<CE_shortcut>("m");
   ep->set<CE_valtype>(1);
   ep->set<CE_relop>(5);
   ep->set<CE_consumable>(1);
   ep->set<CE_defaultval>("1234567890");
   ep->set<CE_requestable>(REQU_NO);
   ep->set<CE_urgency_weight>("1234567890");

   file1 = spool_flatfile_write_object(&alp, *ep, false,
                                       CE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file1, false);
   ep.reset();
   ep = ocs::tom::ElementPtr<ocs::tom::CE>(spool_flatfile_read_object(&alp, ocs::tom::CE::descriptor.e, nullptr,
                                   CE_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1));
   file2 = spool_flatfile_write_object(&alp, *ep, false,
                                       CE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file2, false);
   ep.reset();
   int ret = diff(file1, file2);
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   answer_list_output(&alp);
   return ret;
}

/**
 * @brief Write a parallel environment with blanks in its procedure arguments with classic spooling, read it back and
 *        write it again
 * @return 0 if the file written first and the file written after the read are equal, non-zero otherwise
 */
static int PE_spaces_test() {
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   auto ep = ocs::tom::ElementPtr<ocs::tom::PE>::create();
   const char *file1 = nullptr;
   const char *file2 = nullptr;

   // embedded spaces in proc args test token preservation across write/read
   ep->set<PE_name>("spaces_pe");
   ep->set<PE_slots>(4);
   ep->set<PE_start_proc_args>("/bin/sh -c start_script arg1 arg2");
   ep->set<PE_stop_proc_args>("/bin/sh -c stop_script arg1 arg2");
   ep->set<PE_allocation_rule>("$pe_slots");
   ep->set<PE_control_slaves>(true);
   ep->set<PE_job_is_first_task>(false);
   ep->set<PE_urgency_slots>("min");

   file1 = spool_flatfile_write_object(&alp, *ep, false,
                                       PE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file1, false);
   ep.reset();
   ep = ocs::tom::ElementPtr<ocs::tom::PE>(spool_flatfile_read_object(&alp, ocs::tom::PE::descriptor.e, nullptr,
                                   PE_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1));
   file2 = spool_flatfile_write_object(&alp, *ep, false,
                                       PE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file2, false);
   ep.reset();
   int ret = diff(file1, file2);
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   answer_list_output(&alp);
   return ret;
}

/**
 * @brief Write a complex entry with values in decimal and scientific notation with classic spooling, read it back
 *        and write it again
 * @return 0 if the file written first and the file written after the read are equal, non-zero otherwise
 */
static int CE_special_test() {
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   auto ep = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   const char *file1 = nullptr;
   const char *file2 = nullptr;

   // decimal and scientific notation strings test parser tolerance for unusual number formats
   ep->set<CE_name>("special_ce");
   ep->set<CE_shortcut>("sc");
   ep->set<CE_valtype>(1);
   ep->set<CE_relop>(5);
   ep->set<CE_consumable>(0);
   ep->set<CE_defaultval>("0.001");
   ep->set<CE_requestable>(REQU_YES);
   ep->set<CE_urgency_weight>("1.5e2");

   file1 = spool_flatfile_write_object(&alp, *ep, false,
                                       CE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file1, false);
   ep.reset();
   ep = ocs::tom::ElementPtr<ocs::tom::CE>(spool_flatfile_read_object(&alp, ocs::tom::CE::descriptor.e, nullptr,
                                   CE_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1));
   file2 = spool_flatfile_write_object(&alp, *ep, false,
                                       CE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file2, false);
   ep.reset();
   int ret = diff(file1, file2);
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   answer_list_output(&alp);
   return ret;
}

/**
 * @brief Write a parallel environment with explicitly empty user lists with classic spooling, read it back and write
 *        it again
 * @return 0 if the file written first and the file written after the read are equal, non-zero otherwise
 */
static int PE_empty_lists_test() {
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   auto ep = ocs::tom::ElementPtr<ocs::tom::PE>::create();
   const char *file1 = nullptr;
   const char *file2 = nullptr;

   // explicitly empty lists (not null) — verify they serialize identically to null lists
   ep->set<PE_name>("empty_lists_pe");
   ep->set<PE_slots>(1);
   ep->set<PE_user_list>(ocs::tom::ListPtr<ocs::tom::US>::create(""));
   ep->set<PE_xuser_list>(ocs::tom::ListPtr<ocs::tom::US>::create(""));

   file1 = spool_flatfile_write_object(&alp, *ep, false,
                                       PE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file1, false);
   ep.reset();
   ep = ocs::tom::ElementPtr<ocs::tom::PE>(spool_flatfile_read_object(&alp, ocs::tom::PE::descriptor.e, nullptr,
                                   PE_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1));
   file2 = spool_flatfile_write_object(&alp, *ep, false,
                                       PE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file2, false);
   ep.reset();
   int ret = diff(file1, file2);
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   answer_list_output(&alp);
   return ret;
}

/**
 * @brief Write a parallel environment with a user list of ten entries with classic spooling, read it back and write
 *        it again
 * @return 0 if the file written first and the file written after the read are equal, non-zero otherwise
 */
static int PE_large_list_test() {
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   auto ep = ocs::tom::ElementPtr<ocs::tom::PE>::create();
   const char *file1 = nullptr;
   const char *file2 = nullptr;

   ep->set<PE_name>("large_list_pe");
   ep->set<PE_slots>(10);

   // 10-element user_list — verifies that list ordering and content survive round-trip
   auto lp = ocs::tom::ListPtr<ocs::tom::US>::create("User_List");
   for (int i = 0; i < 10; i++) {
      char name[32];
      snprintf(name, sizeof(name), "user%02d", i);
      auto ep2 = ocs::tom::ElementPtr<ocs::tom::US>::create();
      ep2->set<US_name>(name);
      ep2->set<US_fshare>(i * 100);
      lp->push_back(std::move(ep2));
   }
   ep->set<PE_user_list>(std::move(lp));

   file1 = spool_flatfile_write_object(&alp, *ep, false,
                                       PE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file1, false);
   ep.reset();
   ep = ocs::tom::ElementPtr<ocs::tom::PE>(spool_flatfile_read_object(&alp, ocs::tom::PE::descriptor.e, nullptr,
                                   PE_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1));
   file2 = spool_flatfile_write_object(&alp, *ep, false,
                                       PE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file2, false);
   ep.reset();
   int ret = diff(file1, file2);
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   answer_list_output(&alp);
   return ret;
}
