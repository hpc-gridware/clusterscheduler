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
#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_unistd.h"
#include "uti/sge_stdlib.h"

#include "cull/cull_multitype.h"
#include "cull/cull_list.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/cull/sge_all_listsL.h"
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
   lInit(nmv);

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
   CHECK(id, "SPOOL key: spool_flatfile_key_is_safe accepts relative multi-component keys, rejects traversal", SPOOL_key_safe_test() == 0); id++;
   CHECK(id, "SPOOL params: classic create_context rejects legacy 2-arg and non-absolute spooling_params", SPOOL_classic_params_test() == 0); id++;

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}

static int PE_test() {
   int ret = 0;
   lList *alp = nullptr;
   lListElem *ep = lCreateElem(PE_Type);
   const char *file1 = nullptr;
   const char *file2 = nullptr;
   
   /* Build a PE structure */
   
   ocs::tom::set<PE_name>(ep, "Test_Name");
   ocs::tom::set<PE_slots>(ep, 7);
   ocs::tom::set<PE_start_proc_args>(ep, "start_args");
   ocs::tom::set<PE_stop_proc_args>(ep, "stop_args");
   ocs::tom::set<PE_allocation_rule>(ep, "allocation_rule");
   ocs::tom::set<PE_control_slaves>(ep, true);
   ocs::tom::set<PE_job_is_first_task>(ep, true);
   ocs::tom::set<PE_urgency_slots>(ep, "urgency_slots");

   auto lp = ocs::tom::ListPtr<ocs::tom::RUE>::create("Resource Utilization List");
   
   auto ep2 = ocs::tom::ElementPtr<ocs::tom::RUE>::create();
   ocs::tom::set<RUE_name>(ep2, "Test_Name2");
   ocs::tom::set<RUE_utilized_now>(ep2, 12.345);
   lAppendElem(*lp, std::move(ep2));
   
   auto ep2_rue = ocs::tom::ElementPtr<ocs::tom::RUE>::create();
   ocs::tom::set<RUE_name>(ep2_rue, "Test_Name3");
   ocs::tom::set<RUE_utilized_now>(ep2_rue, 678.9);
   lAppendElem(*lp, std::move(ep2_rue));
   
   ocs::tom::set<PE_resource_utilization>(ep, std::move(lp));
   
   auto lp_us = ocs::tom::ListPtr<ocs::tom::US>::create("User_List");

   auto ep2_us = ocs::tom::ElementPtr<ocs::tom::US>::create();   
   ocs::tom::set<US_name>(ep2_us, "First_User_List");
   ocs::tom::set<US_type>(ep2_us, 101);
   ocs::tom::set<US_fshare>(ep2_us, 303);
   ocs::tom::set<US_oticket>(ep2_us, 505);
   ocs::tom::set<US_job_cnt>(ep2_us, 707);
   ocs::tom::set<US_pending_job_cnt>(ep2_us, 909);
   
   auto lp2 = ocs::tom::ListPtr<ocs::tom::UE>::create("User_Entry_List");
   
   auto ep3 = ocs::tom::ElementPtr<ocs::tom::UE>::create();
   ocs::tom::set<UE_name>(ep3, "First_User");
   lAppendElem(*lp2, std::move(ep3));
   
   auto ep3_ue = ocs::tom::ElementPtr<ocs::tom::UE>::create();
   ocs::tom::set<UE_name>(ep3_ue, "Second_User");
   lAppendElem(*lp2, std::move(ep3_ue));

   ocs::tom::set<US_entries>(ep2_us, std::move(lp2));
   
   lAppendElem(*lp_us, std::move(ep2_us));
   
   auto ep2_us_2 = ocs::tom::ElementPtr<ocs::tom::US>::create();   
   ocs::tom::set<US_name>(ep2_us_2, "Second_User_List");
   ocs::tom::set<US_type>(ep2_us_2, 202);
   ocs::tom::set<US_fshare>(ep2_us_2, 404);
   ocs::tom::set<US_oticket>(ep2_us_2, 606);
   ocs::tom::set<US_job_cnt>(ep2_us_2, 808);
   ocs::tom::set<US_pending_job_cnt>(ep2_us_2, 0);
   
   auto lp2_ue = ocs::tom::ListPtr<ocs::tom::UE>::create("User_Entry_List");
   
   auto ep3_ue_2 = ocs::tom::ElementPtr<ocs::tom::UE>::create();
   ocs::tom::set<UE_name>(ep3_ue_2, "Third_User");
   lAppendElem(*lp2_ue, std::move(ep3_ue_2));
   
   auto ep3_ue_3 = ocs::tom::ElementPtr<ocs::tom::UE>::create();
   ocs::tom::set<UE_name>(ep3_ue_3, "Fourth_User");
   lAppendElem(*lp2_ue, std::move(ep3_ue_3));

   ocs::tom::set<US_entries>(ep2_us_2, std::move(lp2_ue));
   
   lAppendElem(*lp_us, std::move(ep2_us_2));
   
   ocs::tom::set<PE_user_list>(ep, std::move(lp_us));
   
   auto lp_us_2 = ocs::tom::ListPtr<ocs::tom::US>::create("XUser_List");
   
   auto ep2_us_3 = ocs::tom::ElementPtr<ocs::tom::US>::create();   
   ocs::tom::set<US_name>(ep2_us_3, "First_XUser_List");
   ocs::tom::set<US_type>(ep2_us_3, 101);
   ocs::tom::set<US_fshare>(ep2_us_3, 303);
   ocs::tom::set<US_oticket>(ep2_us_3, 505);
   ocs::tom::set<US_job_cnt>(ep2_us_3, 707);
   ocs::tom::set<US_pending_job_cnt>(ep2_us_3, 909);
   
   auto lp2_ue_2 = ocs::tom::ListPtr<ocs::tom::UE>::create("XUser_Entry_List");
   
   auto ep3_ue_4 = ocs::tom::ElementPtr<ocs::tom::UE>::create();
   ocs::tom::set<UE_name>(ep3_ue_4, "First_XUser");
   lAppendElem(*lp2_ue_2, std::move(ep3_ue_4));
   
   auto ep3_ue_5 = ocs::tom::ElementPtr<ocs::tom::UE>::create();
   ocs::tom::set<UE_name>(ep3_ue_5, "Second_XUser");
   lAppendElem(*lp2_ue_2, std::move(ep3_ue_5));

   ocs::tom::set<US_entries>(ep2_us_3, std::move(lp2_ue_2));
   
   lAppendElem(*lp_us_2, std::move(ep2_us_3));
   
   auto ep2_us_4 = ocs::tom::ElementPtr<ocs::tom::US>::create();   
   ocs::tom::set<US_name>(ep2_us_4, "Second_XUser_List");
   ocs::tom::set<US_type>(ep2_us_4, 202);
   ocs::tom::set<US_fshare>(ep2_us_4, 404);
   ocs::tom::set<US_oticket>(ep2_us_4, 606);
   ocs::tom::set<US_job_cnt>(ep2_us_4, 808);
   ocs::tom::set<US_pending_job_cnt>(ep2_us_4, 0);
   
   auto lp2_ue_3 = ocs::tom::ListPtr<ocs::tom::UE>::create("XUser_Entry_List");
   
   auto ep3_ue_6 = ocs::tom::ElementPtr<ocs::tom::UE>::create();
   ocs::tom::set<UE_name>(ep3_ue_6, "Third_XUser");
   lAppendElem(*lp2_ue_3, std::move(ep3_ue_6));
   
   auto ep3_ue_7 = ocs::tom::ElementPtr<ocs::tom::UE>::create();
   ocs::tom::set<UE_name>(ep3_ue_7, "Fourth_XUser");
   lAppendElem(*lp2_ue_3, std::move(ep3_ue_7));

   ocs::tom::set<US_entries>(ep2_us_4, std::move(lp2_ue_3));
   
   lAppendElem(*lp_us_2, std::move(ep2_us_4));
   
   ocs::tom::set<PE_xuser_list>(ep, std::move(lp_us_2));

   /* Write a PE file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, ep, false,
                                       PE_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, false);

   /* Read a PE file using flatfile spooling */
   lFreeElem(&ep);
   ep = spool_flatfile_read_object(&alp, PE_Type, nullptr,
                                   PE_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1);

   /* Write a PE file using flatfile spooling */
   file2 = spool_flatfile_write_object(&alp, ep, false,
                                       PE_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file2, false);

   lFreeElem(&ep);
   ret = diff(file1, file2);
   
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);

   answer_list_output(&alp);   

   return ret;
}

static int CAL_test() {
   int ret = 0;
   lListElem *ep = nullptr;
   lList *alp = nullptr;
   const char *file1 = nullptr, *file2 = nullptr;
   
   auto ep_cal = ocs::tom::ElementPtr<ocs::tom::CAL>::create();   
   ocs::tom::set<CAL_name>(ep_cal, "Test_Name");
   ocs::tom::set<CAL_year_calendar>(ep_cal, "2004");
   ocs::tom::set<CAL_week_calendar>(ep_cal, "KW 05");

   /* Write a CAL file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, *ep_cal, false,
                                       CAL_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, false);
   
   /* Read a CAL file using flatfile spooling */
   ep_cal.reset();
   ep = spool_flatfile_read_object(&alp, CAL_Type, nullptr,
                                   CAL_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1);
   
   /* Write a CAL file using flatfile spooling */
   file2 = spool_flatfile_write_object(&alp, ep, false,
                                       CAL_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file2, false);
   
   lFreeElem(&ep);
   
   ret = diff(file1, file2);
   
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   
   answer_list_output(&alp);   
   lFreeList(&alp);
   
   return ret;
}

static int CK_test() {
   int ret = 0;
   lListElem *ep = nullptr;
   lList *alp = nullptr;
   const char *file1 = nullptr, *file2 = nullptr;
   
   auto ep_ck = ocs::tom::ElementPtr<ocs::tom::CK>::create();
   ocs::tom::set<CK_name>(ep_ck, "Test_Name");
   ocs::tom::set<CK_interface>(ep_ck, "interface");
   ocs::tom::set<CK_ckpt_command>(ep_ck, "ckpt_command");
   ocs::tom::set<CK_migr_command>(ep_ck, "migr_command");
   ocs::tom::set<CK_rest_command>(ep_ck, "rest_command");
   ocs::tom::set<CK_ckpt_dir>(ep_ck, "ckpt_dir");
   ocs::tom::set<CK_when>(ep_ck, "when");
   ocs::tom::set<CK_signal>(ep_ck, "signal");
   ocs::tom::set<CK_job_pid>(ep_ck, 1313);
   ocs::tom::set<CK_clean_command>(ep_ck, "clean_command");
   
   /* Write a CK file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, *ep_ck, false,
                                       CK_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, false);
   
   /* Read a CK file using flatfile spooling */
   ep_ck.reset();
   ep = spool_flatfile_read_object(&alp, CK_Type, nullptr,
                                   CK_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1);
      
   /* Write a CK file using flatfile spooling */
   file2 = spool_flatfile_write_object(&alp, ep, false,
                                       CK_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file2, false);
   
   lFreeElem(&ep);
   
   ret = diff(file1, file2);
   
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   
   answer_list_output(&alp);   

   return ret;
}

static int STN_test() {
   int ret = 0;
   lListElem *ep = nullptr;
   lList *alp = nullptr;
   spooling_field *fields = nullptr;
   const char *file1 = nullptr;
   const char *file2 = nullptr;
   
   auto ep_stn = ocs::tom::ElementPtr<ocs::tom::STN>::create();
   ocs::tom::set<STN_name>(ep_stn, "Test_Name");
   ocs::tom::set<STN_type>(ep_stn, 1);
   ocs::tom::set<STN_id>(ep_stn, 0);
   ocs::tom::set<STN_shares>(ep_stn, 99);
   ocs::tom::set<STN_version>(ep_stn, 1);
   
   auto lp = ocs::tom::ListPtr<ocs::tom::STN>::create("Sharetree List");
   
   auto ep2 = ocs::tom::ElementPtr<ocs::tom::STN>::create();
   ocs::tom::set<STN_name>(ep2, "Test_Name2");
   ocs::tom::set<STN_type>(ep2, 1);
   ocs::tom::set<STN_id>(ep2, 1);
   ocs::tom::set<STN_shares>(ep2, 99);
   
   auto lp2 = ocs::tom::ListPtr<ocs::tom::STN>::create("Sharetree List");
   
   auto ep3 = ocs::tom::ElementPtr<ocs::tom::STN>::create();
   ocs::tom::set<STN_name>(ep3, "Test_Name3");
   ocs::tom::set<STN_type>(ep3, 1);
   ocs::tom::set<STN_id>(ep3, 2);
   ocs::tom::set<STN_shares>(ep3, 99);
   ocs::tom::set<STN_children>(ep3, nullptr);
   lAppendElem(*lp2, std::move(ep3));
   
   auto ep3_stn = ocs::tom::ElementPtr<ocs::tom::STN>::create();
   ocs::tom::set<STN_name>(ep3_stn, "Test_Name4");
   ocs::tom::set<STN_type>(ep3_stn, 1);
   ocs::tom::set<STN_id>(ep3_stn, 4);
   ocs::tom::set<STN_shares>(ep3_stn, 99);
   ocs::tom::set<STN_children>(ep3_stn, nullptr);
   lAppendElem(*lp2, std::move(ep3_stn));
   
   ocs::tom::set<STN_children>(ep2, std::move(lp2));
   lAppendElem(*lp, std::move(ep2));
   
   auto ep2_stn = ocs::tom::ElementPtr<ocs::tom::STN>::create();
   ocs::tom::set<STN_name>(ep2_stn, "Test_Name5");
   ocs::tom::set<STN_type>(ep2_stn, 1);
   ocs::tom::set<STN_id>(ep2_stn, 5);
   ocs::tom::set<STN_shares>(ep2_stn, 99);
   
   auto lp2_stn = ocs::tom::ListPtr<ocs::tom::STN>::create("Sharetree List");
   
   auto ep3_stn_2 = ocs::tom::ElementPtr<ocs::tom::STN>::create();
   ocs::tom::set<STN_name>(ep3_stn_2, "Test_Name6");
   ocs::tom::set<STN_type>(ep3_stn_2, 1);
   ocs::tom::set<STN_id>(ep3_stn_2, 7);
   ocs::tom::set<STN_shares>(ep3_stn_2, 99);
   ocs::tom::set<STN_children>(ep3_stn_2, nullptr);
   lAppendElem(*lp2_stn, std::move(ep3_stn_2));
   
   ocs::tom::set<STN_children>(ep2_stn, std::move(lp2_stn));
   lAppendElem(*lp, std::move(ep2_stn));
   
   ocs::tom::set<STN_children>(ep_stn, std::move(lp));

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
   ep = spool_flatfile_read_object(&alp, STN_Type, nullptr,
                                   fields, nullptr, true, &qconf_name_value_list_sfi,
                                   SP_FORM_ASCII, nullptr, file1);
   
   /* Write a STN file using flatfile spooling */
   file2 = spool_flatfile_write_object(&alp, ep, false,
                                       fields,
                                       &qconf_name_value_list_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file2, false);
   lFreeElem(&ep);

   
   ret = diff(file1, file2);
   
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   sge_free(&fields);

   answer_list_output(&alp);   
   
   return ret;
}

static int CE_test() {
   int ret = 0;
   lListElem *ep = nullptr;
   lList *alp = nullptr;
   const char *file1 = nullptr, *file2 = nullptr;
   
   auto ep_ce = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ocs::tom::set<CE_name>(ep_ce, "Test_Name");
   ocs::tom::set<CE_shortcut>(ep_ce, "shortcut");
   ocs::tom::set<CE_valtype>(ep_ce, 1);
   ocs::tom::set<CE_relop>(ep_ce, 5);
   ocs::tom::set<CE_consumable>(ep_ce, 1);
   ocs::tom::set<CE_defaultval>(ep_ce, "10");
   ocs::tom::set<CE_requestable>(ep_ce, REQU_NO);
   ocs::tom::set<CE_urgency_weight>(ep_ce, "20");
   
   /* Write a CE file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, *ep_ce, false,
                                       CE_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, false);
   
   /* Read a CE file using flatfile spooling */
   ep_ce.reset();
   ep = spool_flatfile_read_object(&alp, CE_Type, nullptr,
                                   CE_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1);
         
   /* Write a CE file using flatfile spooling */
   file2 = spool_flatfile_write_object(&alp, ep, false,
                                       CE_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file2, false);

   lFreeElem(&ep);
   
   ret = diff(file1, file2);
   
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   
   answer_list_output(&alp);   
   
   return ret;
}

static int CEL_test() {
   int ret = 0;
   lList *lp = nullptr;
   lList *alp = nullptr;
   const char *file1 = nullptr;
   const char *file2 = nullptr;
   
   lp = lCreateList("Complex List", CE_Type);
   
   auto ep = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ocs::tom::set<CE_name>(ep, "Test_Name1");
   ocs::tom::set<CE_shortcut>(ep, "tn1");
   ocs::tom::set<CE_valtype>(ep, 1);
   ocs::tom::set<CE_relop>(ep, 5);
   ocs::tom::set<CE_consumable>(ep, 2);
   ocs::tom::set<CE_defaultval>(ep, "15");
   ocs::tom::set<CE_requestable>(ep, REQU_NO);
   ocs::tom::set<CE_urgency_weight>(ep, "25");
   lAppendElem(lp, std::move(ep));
   
   auto ep_ce = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ocs::tom::set<CE_name>(ep_ce, "Test_Name2");
   ocs::tom::set<CE_shortcut>(ep_ce, "tn2");
   ocs::tom::set<CE_valtype>(ep_ce, 1);
   ocs::tom::set<CE_relop>(ep_ce, 5);
   ocs::tom::set<CE_consumable>(ep_ce, 0);
   ocs::tom::set<CE_defaultval>(ep_ce, "15");
   ocs::tom::set<CE_requestable>(ep_ce, REQU_YES);
   ocs::tom::set<CE_urgency_weight>(ep_ce, "25");
   lAppendElem(lp, std::move(ep_ce));
   
   spool_flatfile_align_list(&alp,(const lList *)lp, CE_fields, 0);
   
   /* Write a CEL file using classic spooling */
   file1 = spool_flatfile_write_list(&alp, lp,
                                     CE_fields,
                                     &qconf_ce_list_sfi,
                                     SP_DEST_TMP, 
                                     SP_FORM_ASCII, 
                                     file1, false);
   
   /* Read a CE file using flatfile spooling */
   lFreeList(&lp);
   lp = spool_flatfile_read_list(&alp, CE_Type,
                                   CE_fields, nullptr, true, &qconf_ce_list_sfi,
                                   SP_FORM_ASCII, nullptr, file1);
   
   /* Write a CEL file using flatfile spooling */
   file2 = spool_flatfile_write_list(&alp, lp,
                                     CE_fields,
                                     &qconf_ce_list_sfi,
                                     SP_DEST_TMP, 
                                     SP_FORM_ASCII, 
                                     nullptr, false);
   lFreeList(&lp);

   
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

static lList *buildUsageList() {   

   auto lp = ocs::tom::ListPtr<ocs::tom::UA>::create("Usage List");
   
   auto ep2 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ocs::tom::set<UA_name>(ep2, "Test_Name2");
   ocs::tom::set<UA_value>(ep2, 11.22);
   lAppendElem(*lp, std::move(ep2));
   
   auto ep2_ua = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ocs::tom::set<UA_name>(ep2_ua, "Test_Name3");
   ocs::tom::set<UA_value>(ep2_ua, 33.44);
   lAppendElem(*lp, std::move(ep2_ua));
   
   auto ep2_ua_2 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ocs::tom::set<UA_name>(ep2_ua_2, "Test_Name4");
   ocs::tom::set<UA_value>(ep2_ua_2, 55.66);
   lAppendElem(*lp, std::move(ep2_ua_2));

   return lp.release();
}

static lList *buildLTUsageList() {

   auto lp = ocs::tom::ListPtr<ocs::tom::UA>::create("LT Usage List");
   
   auto ep2 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ocs::tom::set<UA_name>(ep2, "Test_Name5");
   ocs::tom::set<UA_value>(ep2, 11.22);
   lAppendElem(*lp, std::move(ep2));
   
   auto ep2_ua = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ocs::tom::set<UA_name>(ep2_ua, "Test_Name6");
   ocs::tom::set<UA_value>(ep2_ua, 33.44);
   lAppendElem(*lp, std::move(ep2_ua));
   
   auto ep2_ua_2 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ocs::tom::set<UA_name>(ep2_ua_2, "Test_Name7");
   ocs::tom::set<UA_value>(ep2_ua_2, 55.66);
   lAppendElem(*lp, std::move(ep2_ua_2));
 
   return lp.release();
}      

static lList *buildProjectList() {

   auto lp = ocs::tom::ListPtr<ocs::tom::UPP>::create("Project List");
   
   auto ep2 = ocs::tom::ElementPtr<ocs::tom::UPP>::create();
   ocs::tom::set<UPP_name>(ep2, "Test_Name8");
   
   auto lp2 = ocs::tom::ListPtr<ocs::tom::UA>::create("Usage List");
   
   auto ep3 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ocs::tom::set<UA_name>(ep3, "Test_Name9");
   ocs::tom::set<UA_value>(ep3, 11.22);
   lAppendElem(*lp2, std::move(ep3));
   
   auto ep3_ua = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ocs::tom::set<UA_name>(ep3_ua, "Test_Name10");
   ocs::tom::set<UA_value>(ep3_ua, 33.44);
   lAppendElem(*lp2, std::move(ep3_ua));
   
   auto ep3_ua_2 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ocs::tom::set<UA_name>(ep3_ua_2, "Test_Name11");
   ocs::tom::set<UA_value>(ep3_ua_2, 55.66);
   lAppendElem(*lp2, std::move(ep3_ua_2));
      
   ocs::tom::set<UPP_usage>(ep2, std::move(lp2));
   
   auto lp2_ua = ocs::tom::ListPtr<ocs::tom::UA>::create("LT Usage List");
   
   auto ep3_ua_3 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ocs::tom::set<UA_name>(ep3_ua_3, "Test_Name12");
   ocs::tom::set<UA_value>(ep3_ua_3, 11.22);
   lAppendElem(*lp2_ua, std::move(ep3_ua_3));
   
   auto ep3_ua_4 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ocs::tom::set<UA_name>(ep3_ua_4, "Test_Name13");
   ocs::tom::set<UA_value>(ep3_ua_4, 33.44);
   lAppendElem(*lp2_ua, std::move(ep3_ua_4));
   
   auto ep3_ua_5 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ocs::tom::set<UA_name>(ep3_ua_5, "Test_Name14");
   ocs::tom::set<UA_value>(ep3_ua_5, 55.66);
   lAppendElem(*lp2_ua, std::move(ep3_ua_5));
      
   ocs::tom::set<UPP_long_term_usage>(ep2, std::move(lp2_ua));
   lAppendElem(*lp, std::move(ep2));
   
   auto ep2_upp = ocs::tom::ElementPtr<ocs::tom::UPP>::create();
   ocs::tom::set<UPP_name>(ep2_upp, "Test_Name15");
   
   auto lp2_ua_2 = ocs::tom::ListPtr<ocs::tom::UA>::create("Usage List");
   
   auto ep3_ua_6 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ocs::tom::set<UA_name>(ep3_ua_6, "Test_Name16");
   ocs::tom::set<UA_value>(ep3_ua_6, 11.22);
   lAppendElem(*lp2_ua_2, std::move(ep3_ua_6));
   
   auto ep3_ua_7 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ocs::tom::set<UA_name>(ep3_ua_7, "Test_Name17");
   ocs::tom::set<UA_value>(ep3_ua_7, 33.44);
   lAppendElem(*lp2_ua_2, std::move(ep3_ua_7));
   
   auto ep3_ua_8 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ocs::tom::set<UA_name>(ep3_ua_8, "Test_Name18");
   ocs::tom::set<UA_value>(ep3_ua_8, 55.66);
   lAppendElem(*lp2_ua_2, std::move(ep3_ua_8));
      
   ocs::tom::set<UPP_usage>(ep2_upp, std::move(lp2_ua_2));
   
   auto lp2_ua_3 = ocs::tom::ListPtr<ocs::tom::UA>::create("LT Usage List");
   
   auto ep3_ua_9 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ocs::tom::set<UA_name>(ep3_ua_9, "Test_Name19");
   ocs::tom::set<UA_value>(ep3_ua_9, 11.22);
   lAppendElem(*lp2_ua_3, std::move(ep3_ua_9));
   
   auto ep3_ua_10 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ocs::tom::set<UA_name>(ep3_ua_10, "Test_Name20");
   ocs::tom::set<UA_value>(ep3_ua_10, 33.44);
   lAppendElem(*lp2_ua_3, std::move(ep3_ua_10));
   
   auto ep3_ua_11 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ocs::tom::set<UA_name>(ep3_ua_11, "Test_Name21");
   ocs::tom::set<UA_value>(ep3_ua_11, 55.66);
   lAppendElem(*lp2_ua_3, std::move(ep3_ua_11));
      
   ocs::tom::set<UPP_long_term_usage>(ep2_upp, std::move(lp2_ua_3));
   lAppendElem(*lp, std::move(ep2_upp));

   return lp.release();
}

static lList *buildDebitedUsageList() {

   auto lp = ocs::tom::ListPtr<ocs::tom::UPU>::create("Debited Usage List");
   
   auto ep = ocs::tom::ElementPtr<ocs::tom::UPU>::create();
   ocs::tom::set<UPU_job_number>(ep, 13579);
   
   auto lp2 = ocs::tom::ListPtr<ocs::tom::UA>::create("Old Usage List");
   
   auto ep1 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ocs::tom::set<UA_name>(ep1, "Test_Name26");
   ocs::tom::set<UA_value>(ep1, 11.22);
   lAppendElem(*lp2, std::move(ep1));
   
   auto ep1_ua = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ocs::tom::set<UA_name>(ep1_ua, "Test_Name27");
   ocs::tom::set<UA_value>(ep1_ua, 33.44);
   lAppendElem(*lp2, std::move(ep1_ua));
   
   auto ep1_ua_2 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ocs::tom::set<UA_name>(ep1_ua_2, "Test_Name28");
   ocs::tom::set<UA_value>(ep1_ua_2, 55.66);
   lAppendElem(*lp2, std::move(ep1_ua_2));
      
   ocs::tom::set<UPU_old_usage_list>(ep, std::move(lp2));
   lAppendElem(*lp, std::move(ep));
   
   auto ep_upu = ocs::tom::ElementPtr<ocs::tom::UPU>::create();
   ocs::tom::set<UPU_job_number>(ep_upu, 97531);
   
   auto lp2_ua = ocs::tom::ListPtr<ocs::tom::UA>::create("Old Usage List");
   
   auto ep1_ua_3 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ocs::tom::set<UA_name>(ep1_ua_3, "Test_Name29");
   ocs::tom::set<UA_value>(ep1_ua_3, 11.22);
   lAppendElem(*lp2_ua, std::move(ep1_ua_3));
   
   auto ep1_ua_4 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ocs::tom::set<UA_name>(ep1_ua_4, "Test_Name30");
   ocs::tom::set<UA_value>(ep1_ua_4, 33.44);
   lAppendElem(*lp2_ua, std::move(ep1_ua_4));
   
   auto ep1_ua_5 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ocs::tom::set<UA_name>(ep1_ua_5, "Test_Name31");
   ocs::tom::set<UA_value>(ep1_ua_5, 55.66);
   lAppendElem(*lp2_ua, std::move(ep1_ua_5));
      
   ocs::tom::set<UPU_old_usage_list>(ep_upu, std::move(lp2_ua));
   lAppendElem(*lp, std::move(ep_upu));

   return lp.release();
}   

static lList *buildACLList() {

   auto lp = ocs::tom::ListPtr<ocs::tom::US>::create("ACL List");
   
   auto ep2 = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ocs::tom::set<US_name>(ep2, "Test_Name22");
   lAppendElem(*lp, std::move(ep2));
   
   auto ep2_us = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ocs::tom::set<US_name>(ep2_us, "Test_Name23");
   lAppendElem(*lp, std::move(ep2_us));

   return lp.release();
}   
   
static lList *buildXACLList() {

   auto lp = ocs::tom::ListPtr<ocs::tom::US>::create("XACL List");
   
   auto ep2 = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ocs::tom::set<US_name>(ep2, "Test_Name24");
   lAppendElem(*lp, std::move(ep2));
   
   auto ep2_us = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ocs::tom::set<US_name>(ep2_us, "Test_Name25");
   lAppendElem(*lp, std::move(ep2_us));

   return lp.release();
}   

static int UU_test() {
   int ret = 0;
   lListElem *user = nullptr;
   lList *lp = nullptr;
   lList *alp = nullptr;
   spooling_field *fields = nullptr;
   const char *file1 = nullptr;
   const char *file2 = nullptr;
   
   auto user_uu = ocs::tom::ElementPtr<ocs::tom::UU>::create();
   ocs::tom::set<UU_name>(user_uu, "Test_Name");
   ocs::tom::set<UU_oticket>(user_uu, 100);
   ocs::tom::set<UU_fshare>(user_uu, 50);
   ocs::tom::set<UU_delete_time>(user_uu, (uint64_t)123456789000000);
   ocs::tom::set<UU_default_project>(user_uu, "default_project");

   ocs::tom::set<UU_usage_time_stamp>(user_uu, 987654321);

   lp = buildUsageList();
   ocs::tom::set<UU_usage>(user_uu, lp);
   
   lp = buildLTUsageList();
   ocs::tom::set<UU_long_term_usage>(user_uu, lp);
   
   lp = buildProjectList();
   ocs::tom::set<UU_project>(user_uu, lp);
   
   lp = buildDebitedUsageList();
   ocs::tom::set<UU_debited_job_usage>(user_uu, lp);


   fields = sge_build_UU_field_list(true);

   /* Write a UU file using flatfile spooling */
   file1 = spool_flatfile_write_object(&alp, *user_uu, false,
                                     fields,
                                     &qconf_sfi,
                                     SP_DEST_TMP,
                                     SP_FORM_ASCII, 
                                     file1, false);
   user_uu.reset();
   user = spool_flatfile_read_object(&alp, UU_Type, nullptr,
                                   fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1);
   file2 = spool_flatfile_write_object(&alp, user, false,
                                     fields,
                                     &qconf_sfi,
                                     SP_DEST_TMP,
                                     SP_FORM_ASCII, 
                                     file2, false);
   lFreeElem(&user);
   ret = diff(file1, file2);
   
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   sge_free(&fields);
   
   answer_list_output(&alp);   
   
   return ret;
}

static int PR_test() {
   int ret = 0;
   lListElem *prj = nullptr;
   lList *lp = nullptr;
   lList *alp = nullptr;
   spooling_field *fields = nullptr;
   const char *file1 = nullptr;
   const char *file2 = nullptr;
   
   auto prj_pr = ocs::tom::ElementPtr<ocs::tom::PR>::create();
   ocs::tom::set<PR_name>(prj_pr, "Test_Name");
   ocs::tom::set<PR_oticket>(prj_pr, 100);
   ocs::tom::set<PR_fshare>(prj_pr, 50);
   ocs::tom::set<PR_usage_time_stamp>(prj_pr, 987654321);

   lp = buildUsageList();
   ocs::tom::set<PR_usage>(prj_pr, lp);
   
   lp = buildLTUsageList();
   ocs::tom::set<PR_long_term_usage>(prj_pr, lp);
   
   lp = buildProjectList();
   ocs::tom::set<PR_project>(prj_pr, lp);
   
   lp = buildACLList();
   ocs::tom::set<PR_acl>(prj_pr, lp);
   
   lp = buildXACLList();
   ocs::tom::set<PR_xacl>(prj_pr, lp);
   
   lp = buildDebitedUsageList();
   ocs::tom::set<PR_debited_job_usage>(prj_pr, lp);

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
   prj = spool_flatfile_read_object(&alp, PR_Type, nullptr,
                                   fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1);
         
   /* Write a PR file using flatfile spooling */
   file2 = spool_flatfile_write_object(&alp, prj, false,
                                     fields,
                                     &qconf_sfi,
                                     SP_DEST_TMP,
                                     SP_FORM_ASCII, 
                                     file2, false);

   lFreeElem(&prj);
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

static int US_test() {
   int ret = 0;
   lListElem *ep = nullptr;
   lList *alp = nullptr;
   const char *file1 = nullptr;
   const char *file2 = nullptr;
   
   auto ep_us = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ocs::tom::set<US_name>(ep_us, "Test_Name");
   ocs::tom::set<US_type>(ep_us, 1);
   ocs::tom::set<US_fshare>(ep_us, 10);
   ocs::tom::set<US_oticket>(ep_us, 100);
   
   auto lp = ocs::tom::ListPtr<ocs::tom::UE>::create("Entry List");
   
   auto ep2 = ocs::tom::ElementPtr<ocs::tom::UE>::create();
   ocs::tom::set<UE_name>(ep2, "Test_Name2");
   lAppendElem(*lp, std::move(ep2));
   
   auto ep2_ue = ocs::tom::ElementPtr<ocs::tom::UE>::create();
   ocs::tom::set<UE_name>(ep2_ue, "Test_Name3");
   lAppendElem(*lp, std::move(ep2_ue));
   
   auto ep2_ue_2 = ocs::tom::ElementPtr<ocs::tom::UE>::create();
   ocs::tom::set<UE_name>(ep2_ue_2, "Test_Name4");
   lAppendElem(*lp, std::move(ep2_ue_2));
   
   ocs::tom::set<US_entries>(ep_us, std::move(lp));
   
   /* Write a US file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, *ep_us, false,
                                       US_fields,
                                       &qconf_param_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, false);

   /* Read a US file using flatfile spooling */
   ep_us.reset();
   ep = spool_flatfile_read_object(&alp, US_Type, nullptr,
                                   US_fields, nullptr, true, &qconf_param_sfi,
                                   SP_FORM_ASCII, nullptr, file1);
      
   /* Write a US file using flatfile spooling */
   file2 = spool_flatfile_write_object(&alp, ep, false,
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
   
   lFreeElem(&ep);
   
   answer_list_output(&alp);   
   
   return ret;
}

static int EH_test() {
   int ret = 0;
   lListElem *ep = nullptr;
   lList *lp = nullptr;
   lList *alp = nullptr;
   spooling_field *fields = nullptr;
   const char *file1 = nullptr;
   const char *file2 = nullptr;
   
   auto ep_eh = ocs::tom::ElementPtr<ocs::tom::EH>::create();
   ocs::tom::set<EH_name>(ep_eh, "Test_Name");
   
   auto lp_hs = ocs::tom::ListPtr<ocs::tom::HS>::create("Load Scaling List");
   
   auto ep2 = ocs::tom::ElementPtr<ocs::tom::HS>::create();
   ocs::tom::set<HS_name>(ep2, "Test_Name2");
   ocs::tom::set<HS_value>(ep2, 1234.567);
   lAppendElem(*lp_hs, std::move(ep2));
   
   auto ep2_hs = ocs::tom::ElementPtr<ocs::tom::HS>::create();
   ocs::tom::set<HS_name>(ep2_hs, "Test_Name3");
   ocs::tom::set<HS_value>(ep2_hs, 6.7);
   lAppendElem(*lp_hs, std::move(ep2_hs));
   
   ocs::tom::set<EH_scaling_list>(ep_eh, std::move(lp_hs));
   
   auto lp_ce = ocs::tom::ListPtr<ocs::tom::CE>::create("Consumable Config List");
   
   auto ep2_ce = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ocs::tom::set<CE_name>(ep2_ce, "Test_Name4");
   ocs::tom::set<CE_shortcut>(ep2_ce, "tn4");
   ocs::tom::set<CE_valtype>(ep2_ce, 1);
   ocs::tom::set<CE_relop>(ep2_ce, 5);
   ocs::tom::set<CE_consumable>(ep2_ce, 1);
   ocs::tom::set<CE_defaultval>(ep2_ce, "15");
   ocs::tom::set<CE_requestable>(ep2_ce, REQU_NO);
   ocs::tom::set<CE_urgency_weight>(ep2_ce, "25");
   ocs::tom::set<CE_stringval>(ep2_ce, "stringval");
   lAppendElem(*lp_ce, std::move(ep2_ce));
   
   auto ep2_ce_2 = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ocs::tom::set<CE_name>(ep2_ce_2, "Test_Name5");
   ocs::tom::set<CE_shortcut>(ep2_ce_2, "tn5");
   ocs::tom::set<CE_valtype>(ep2_ce_2, 1);
   ocs::tom::set<CE_relop>(ep2_ce_2, 5);
   ocs::tom::set<CE_consumable>(ep2_ce_2, 0);
   ocs::tom::set<CE_defaultval>(ep2_ce_2, "15");
   ocs::tom::set<CE_requestable>(ep2_ce_2, REQU_YES);
   ocs::tom::set<CE_urgency_weight>(ep2_ce_2, "25");
   ocs::tom::set<CE_doubleval>(ep2_ce_2, 6969.9696);
   lAppendElem(*lp_ce, std::move(ep2_ce_2));
   
   ocs::tom::set<EH_consumable_config_list>(ep_eh, std::move(lp_ce));
   
   lp = lCreateList("Actual Complex Values List", CE_Type);

   auto ep2_ce_3 = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ocs::tom::set<CE_name>(ep2_ce_3, "Test_Name6");
   ocs::tom::set<CE_shortcut>(ep2_ce_3, "tn6");
   ocs::tom::set<CE_valtype>(ep2_ce_3, 1);
   ocs::tom::set<CE_relop>(ep2_ce_3, 5);
   ocs::tom::set<CE_consumable>(ep2_ce_3, 1);
   ocs::tom::set<CE_defaultval>(ep2_ce_3, "15");
   ocs::tom::set<CE_requestable>(ep2_ce_3, REQU_NO);
   ocs::tom::set<CE_urgency_weight>(ep2_ce_3, "25");
   ocs::tom::set<CE_stringval>(ep2_ce_3, "stringval");
   lAppendElem(lp, std::move(ep2_ce_3));
   
   auto ep2_ce_4 = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ocs::tom::set<CE_name>(ep2_ce_4, "Test_Name7");
   ocs::tom::set<CE_shortcut>(ep2_ce_4, "tn7");
   ocs::tom::set<CE_valtype>(ep2_ce_4, 1);
   ocs::tom::set<CE_relop>(ep2_ce_4, 5);
   ocs::tom::set<CE_consumable>(ep2_ce_4, 0);
   ocs::tom::set<CE_defaultval>(ep2_ce_4, "15");
   ocs::tom::set<CE_requestable>(ep2_ce_4, REQU_YES);
   ocs::tom::set<CE_urgency_weight>(ep2_ce_4, "25");
   ocs::tom::set<CE_doubleval>(ep2_ce_4, 6969.9696);
   lAppendElem(lp, std::move(ep2_ce_4));

   ocs::tom::set<EH_resource_utilization>(ep_eh, lp);
   
   auto lp_hl = ocs::tom::ListPtr<ocs::tom::HL>::create("Load Values List");
   
   auto ep2_hl = ocs::tom::ElementPtr<ocs::tom::HL>::create();
   ocs::tom::set<HL_name>(ep2_hl, "Test_Name8");
   ocs::tom::set<HL_value>(ep2_hl, "1234.567");
   lAppendElem(*lp_hl, std::move(ep2_hl));
   
   auto ep2_hl_2 = ocs::tom::ElementPtr<ocs::tom::HL>::create();
   ocs::tom::set<HL_name>(ep2_hl_2, "Test_Name9");
   ocs::tom::set<HL_value>(ep2_hl_2, "6.7");
   lAppendElem(*lp_hl, std::move(ep2_hl_2));
   
   ocs::tom::set<EH_load_list>(ep_eh, std::move(lp_hl));

   ocs::tom::set<EH_processors>(ep_eh, 64);
   
   auto lp_ru = ocs::tom::ListPtr<ocs::tom::RU>::create("Resched Unknown List");
   
   auto ep2_ru = ocs::tom::ElementPtr<ocs::tom::RU>::create();
   ocs::tom::set<RU_job_number>(ep2_ru, 1);
   ocs::tom::set<RU_task_number>(ep2_ru, 1);
   ocs::tom::set<RU_state>(ep2_ru, 0);
   lAppendElem(*lp_ru, std::move(ep2_ru));
   
   auto ep2_ru_2 = ocs::tom::ElementPtr<ocs::tom::RU>::create();
   ocs::tom::set<RU_job_number>(ep2_ru_2, 1);
   ocs::tom::set<RU_task_number>(ep2_ru_2, 2);
   ocs::tom::set<RU_state>(ep2_ru_2, 1);
   lAppendElem(*lp_ru, std::move(ep2_ru_2));
   
   auto ep2_ru_3 = ocs::tom::ElementPtr<ocs::tom::RU>::create();
   ocs::tom::set<RU_job_number>(ep2_ru_3, 2);
   ocs::tom::set<RU_task_number>(ep2_ru_3, 1);
   ocs::tom::set<RU_state>(ep2_ru_3, 2);
   lAppendElem(*lp_ru, std::move(ep2_ru_3));

   ocs::tom::set<EH_reschedule_unknown_list>(ep_eh, std::move(lp_ru));
   
   auto lp_us = ocs::tom::ListPtr<ocs::tom::US>::create("User List");
   
   auto ep2_us = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ocs::tom::set<US_name>(ep2_us, "Test_Name9");
   lAppendElem(*lp_us, std::move(ep2_us));
   
   auto ep2_us_2 = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ocs::tom::set<US_name>(ep2_us_2, "Test_Name10");
   lAppendElem(*lp_us, std::move(ep2_us_2));
   
   ocs::tom::set<EH_acl>(ep_eh, std::move(lp_us));
   
   auto lp_us_2 = ocs::tom::ListPtr<ocs::tom::US>::create("XUser List");
   
   auto ep2_us_3 = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ocs::tom::set<US_name>(ep2_us_3, "Test_Name11");
   lAppendElem(*lp_us_2, std::move(ep2_us_3));
   
   auto ep2_us_4 = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ocs::tom::set<US_name>(ep2_us_4, "Test_Name12");
   lAppendElem(*lp_us_2, std::move(ep2_us_4));
   
   ocs::tom::set<EH_xacl>(ep_eh, std::move(lp_us_2));
   
   auto lp_pr = ocs::tom::ListPtr<ocs::tom::PR>::create("Projects List");
   
   auto ep2_pr = ocs::tom::ElementPtr<ocs::tom::PR>::create();
   ocs::tom::set<PR_name>(ep2_pr, "Test_Name13");
   lAppendElem(*lp_pr, std::move(ep2_pr));
   
   auto ep2_pr_2 = ocs::tom::ElementPtr<ocs::tom::PR>::create();
   ocs::tom::set<PR_name>(ep2_pr_2, "Test_Name14");
   lAppendElem(*lp_pr, std::move(ep2_pr_2));
   
   ocs::tom::set<EH_prj>(ep_eh, std::move(lp_pr));
   
   auto lp_pr_2 = ocs::tom::ListPtr<ocs::tom::PR>::create("XProjects List");
   
   auto ep2_pr_3 = ocs::tom::ElementPtr<ocs::tom::PR>::create();
   ocs::tom::set<PR_name>(ep2_pr_3, "Test_Name15");
   lAppendElem(*lp_pr_2, std::move(ep2_pr_3));
   
   auto ep2_pr_4 = ocs::tom::ElementPtr<ocs::tom::PR>::create();
   ocs::tom::set<PR_name>(ep2_pr_4, "Test_Name16");
   lAppendElem(*lp_pr_2, std::move(ep2_pr_4));
   
   ocs::tom::set<EH_xprj>(ep_eh, std::move(lp_pr_2));
   
   auto lp_hs_2 = ocs::tom::ListPtr<ocs::tom::HS>::create("Usage Scaling List");
   
   auto ep2_hs_2 = ocs::tom::ElementPtr<ocs::tom::HS>::create();
   ocs::tom::set<HS_name>(ep2_hs_2, "Test_Name17");
   ocs::tom::set<HS_value>(ep2_hs_2, 1234.567);
   lAppendElem(*lp_hs_2, std::move(ep2_hs_2));
   
   auto ep2_hs_3 = ocs::tom::ElementPtr<ocs::tom::HS>::create();
   ocs::tom::set<HS_name>(ep2_hs_3, "Test_Name18");
   ocs::tom::set<HS_value>(ep2_hs_3, 6.7);
   lAppendElem(*lp_hs_2, std::move(ep2_hs_3));
   
   ocs::tom::set<EH_usage_scaling_list>(ep_eh, std::move(lp_hs_2));
   
   auto lp_stu = ocs::tom::ListPtr<ocs::tom::STU>::create("Report Vars List");
   
   auto ep2_stu = ocs::tom::ElementPtr<ocs::tom::STU>::create();
   ocs::tom::set<STU_name>(ep2_stu, "Test_Name19");
   lAppendElem(*lp_stu, std::move(ep2_stu));
   
   auto ep2_stu_2 = ocs::tom::ElementPtr<ocs::tom::STU>::create();
   ocs::tom::set<STU_name>(ep2_stu_2, "Test_Name20");
   lAppendElem(*lp_stu, std::move(ep2_stu_2));
   
   ocs::tom::set<EH_report_variables>(ep_eh, std::move(lp_stu));

   /* the time at which the host added itself. The spool file carries the raw value, so that it
    * can be read back - the formatted date and time which qconf -se shows contains a blank,
    * which is the delimiter between the name of an attribute and its value in this format */
   ocs::tom::set<EH_auto_added_time>(ep_eh, 1792143190772380);

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
   ep = spool_flatfile_read_object(&alp, EH_Type, nullptr,
                                   fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1);
      
   /* Write a EH file using flatfile spooling */
   file2 = spool_flatfile_write_object(&alp, ep, false,
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

   lFreeElem(&ep);

   if (ret != 0) {
      return ret;
   }
   
   answer_list_output(&alp);   
   
   return ret;
}

static int CQ_test() {
   int ret = 0;
   lListElem *ep = nullptr;
   lList *alp = nullptr;
   const char *file1 = nullptr, *file2 = nullptr;
   
   auto ep_cq = ocs::tom::ElementPtr<ocs::tom::CQ>::create();
   
   ocs::tom::set<CQ_name>(ep_cq, "Test_Name");
   
   ocs::tom::emplace_back_in<CQ_hostlist, HR_name>(ep_cq, "Test_Name2");
   ocs::tom::emplace_back_in<CQ_hostlist, HR_name>(ep_cq, "Test_Name3");
   ocs::tom::emplace_back_in<CQ_hostlist, HR_name>(ep_cq, "Test_Name4");
   ocs::tom::emplace_back_in<CQ_hostlist, HR_name>(ep_cq, "Test_Name5");
   ocs::tom::emplace_back_in<CQ_hostlist, HR_name>(ep_cq, "Test_Name6");
   ocs::tom::emplace_back_in<CQ_hostlist, HR_name>(ep_cq, "Test_Name7");

   auto lp = ocs::tom::ListPtr<ocs::tom::AULNG>::create("Seq No. List");
   
   auto ep2 = ocs::tom::ElementPtr<ocs::tom::AULNG>::create();
   ocs::tom::set<AULNG_href>(ep2, "Test_Name4");
   ocs::tom::set<AULNG_value>(ep2, 12);
   lAppendElem(*lp, std::move(ep2));
   
   auto ep2_aulng = ocs::tom::ElementPtr<ocs::tom::AULNG>::create();
   ocs::tom::set<AULNG_href>(ep2_aulng, "Test_Name5");
   ocs::tom::set<AULNG_value>(ep2_aulng, 16);
   lAppendElem(*lp, std::move(ep2_aulng));

   ocs::tom::set<CQ_seq_no>(ep_cq, std::move(lp));

   auto lp_acelist = ocs::tom::ListPtr<ocs::tom::ACELIST>::create("Load Thresholds");
   
   auto ep2_acelist = ocs::tom::ElementPtr<ocs::tom::ACELIST>::create();
   ocs::tom::set<ACELIST_href>(ep2_acelist, "Test_Name6");

   auto lp2 = ocs::tom::ListPtr<ocs::tom::CE>::create("Complex List");
   
   auto ep3 = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ocs::tom::set<CE_name>(ep3, "Test_Name7");
   ocs::tom::set<CE_stringval>(ep3, "stringval");
   lAppendElem(*lp2, std::move(ep3));
   
   ocs::tom::set<ACELIST_value>(ep2_acelist, std::move(lp2));
   
   lAppendElem(*lp_acelist, std::move(ep2_acelist));

   ocs::tom::set<CQ_load_thresholds>(ep_cq, std::move(lp_acelist));
   
   auto lp_acelist_2 = ocs::tom::ListPtr<ocs::tom::ACELIST>::create("Suspend Thresholds");

   auto ep2_acelist_2 = ocs::tom::ElementPtr<ocs::tom::ACELIST>::create();
   ocs::tom::set<ACELIST_href>(ep2_acelist_2, "Test_Name15");

   auto lp2_ce = ocs::tom::ListPtr<ocs::tom::CE>::create("Complex_List2");
   
   auto ep3_ce = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ocs::tom::set<CE_name>(ep3_ce, "Test_Name16");
   ocs::tom::set<CE_stringval>(ep3_ce, "stringval");
   lAppendElem(*lp2_ce, std::move(ep3_ce));

   ocs::tom::set<ACELIST_value>(ep2_acelist_2, std::move(lp2_ce));
   
   lAppendElem(*lp_acelist_2, std::move(ep2_acelist_2));

   ocs::tom::set<CQ_suspend_thresholds>(ep_cq, std::move(lp_acelist_2));
   
   auto lp_aulng = ocs::tom::ListPtr<ocs::tom::AULNG>::create("NSuspend List");
   
   auto ep2_aulng_2 = ocs::tom::ElementPtr<ocs::tom::AULNG>::create();
   ocs::tom::set<AULNG_href>(ep2_aulng_2, "Test_Name18");
   ocs::tom::set<AULNG_value>(ep2_aulng_2, 12);
   lAppendElem(*lp_aulng, std::move(ep2_aulng_2));
   
   auto ep2_aulng_3 = ocs::tom::ElementPtr<ocs::tom::AULNG>::create();
   ocs::tom::set<AULNG_href>(ep2_aulng_3, "Test_Name19");
   ocs::tom::set<AULNG_value>(ep2_aulng_3, 16);
   lAppendElem(*lp_aulng, std::move(ep2_aulng_3));

   ocs::tom::set<CQ_nsuspend>(ep_cq, std::move(lp_aulng));

   auto lp_ainter = ocs::tom::ListPtr<ocs::tom::AINTER>::create("Suspend Intervals");
   
   auto ep2_ainter = ocs::tom::ElementPtr<ocs::tom::AINTER>::create();
   ocs::tom::set<AINTER_href>(ep2_ainter, "Test_Name20");
   ocs::tom::set<AINTER_value>(ep2_ainter, "1");
   lAppendElem(*lp_ainter, std::move(ep2_ainter));
   
   auto ep2_ainter_2 = ocs::tom::ElementPtr<ocs::tom::AINTER>::create();
   ocs::tom::set<AINTER_href>(ep2_ainter_2, "Test_Name21");
   ocs::tom::set<AINTER_value>(ep2_ainter_2, "2");
   lAppendElem(*lp_ainter, std::move(ep2_ainter_2));

   ocs::tom::set<CQ_suspend_interval>(ep_cq, std::move(lp_ainter));

   auto lp_astr = ocs::tom::ListPtr<ocs::tom::ASTR>::create("Priority List");
   
   auto ep2_astr = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ocs::tom::set<ASTR_href>(ep2_astr, "Test_Name22");
   ocs::tom::set<ASTR_value>(ep2_astr, "3");
   lAppendElem(*lp_astr, std::move(ep2_astr));
   
   auto ep2_astr_2 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ocs::tom::set<ASTR_href>(ep2_astr_2, "Test_Name23");
   ocs::tom::set<ASTR_value>(ep2_astr_2, "4");
   lAppendElem(*lp_astr, std::move(ep2_astr_2));

   ocs::tom::set<CQ_priority>(ep_cq, std::move(lp_astr));

   auto lp_ainter_2 = ocs::tom::ListPtr<ocs::tom::AINTER>::create("Min CPU Intervals");
   
   auto ep2_ainter_3 = ocs::tom::ElementPtr<ocs::tom::AINTER>::create();
   ocs::tom::set<AINTER_href>(ep2_ainter_3, "Test_Name24");
   ocs::tom::set<AINTER_value>(ep2_ainter_3, "5");
   lAppendElem(*lp_ainter_2, std::move(ep2_ainter_3));
   
   auto ep2_ainter_4 = ocs::tom::ElementPtr<ocs::tom::AINTER>::create();
   ocs::tom::set<AINTER_href>(ep2_ainter_4, "Test_Name25");
   ocs::tom::set<AINTER_value>(ep2_ainter_4, "6");
   lAppendElem(*lp_ainter_2, std::move(ep2_ainter_4));

   ocs::tom::set<CQ_min_cpu_interval>(ep_cq, std::move(lp_ainter_2));

   auto lp_astr_2 = ocs::tom::ListPtr<ocs::tom::ASTR>::create("Processor List");
   
   auto ep2_astr_3 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ocs::tom::set<ASTR_href>(ep2_astr_3, "Test_Name26");
   ocs::tom::set<ASTR_value>(ep2_astr_3, "7");
   lAppendElem(*lp_astr_2, std::move(ep2_astr_3));
   
   auto ep2_astr_4 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ocs::tom::set<ASTR_href>(ep2_astr_4, "Test_Name27");
   ocs::tom::set<ASTR_value>(ep2_astr_4, "8");
   lAppendElem(*lp_astr_2, std::move(ep2_astr_4));

   ocs::tom::set<CQ_processors>(ep_cq, std::move(lp_astr_2));

   auto lp_aqtlist = ocs::tom::ListPtr<ocs::tom::AQTLIST>::create("Q-Type List");
   
   auto ep2_aqtlist = ocs::tom::ElementPtr<ocs::tom::AQTLIST>::create();
   ocs::tom::set<AQTLIST_href>(ep2_aqtlist, "Test_Name28");
   ocs::tom::set<AQTLIST_value>(ep2_aqtlist, 12);
   lAppendElem(*lp_aqtlist, std::move(ep2_aqtlist));
   
   auto ep2_aqtlist_2 = ocs::tom::ElementPtr<ocs::tom::AQTLIST>::create();
   ocs::tom::set<AQTLIST_href>(ep2_aqtlist_2, "Test_Name29");
   ocs::tom::set<AQTLIST_value>(ep2_aqtlist_2, 16);
   lAppendElem(*lp_aqtlist, std::move(ep2_aqtlist_2));

   ocs::tom::set<CQ_qtype>(ep_cq, std::move(lp_aqtlist));

   auto lp_astrlist = ocs::tom::ListPtr<ocs::tom::ASTRLIST>::create("Checkpoint List");
   
   auto ep2_astrlist = ocs::tom::ElementPtr<ocs::tom::ASTRLIST>::create();
   ocs::tom::set<ASTRLIST_href>(ep2_astrlist, "Test_Name30");
   
   auto lp2_st = ocs::tom::ListPtr<ocs::tom::ST>::create("String List");
   
   auto ep3_st = ocs::tom::ElementPtr<ocs::tom::ST>::create();
   ocs::tom::set<ST_name>(ep3_st, "Test_Name31");
   lAppendElem(*lp2_st, std::move(ep3_st));
   
   auto ep3_st_2 = ocs::tom::ElementPtr<ocs::tom::ST>::create();
   ocs::tom::set<ST_name>(ep3_st_2, "Test_Name32");
   lAppendElem(*lp2_st, std::move(ep3_st_2));
   
   ocs::tom::set<ASTRLIST_value>(ep2_astrlist, std::move(lp2_st));
   lAppendElem(*lp_astrlist, std::move(ep2_astrlist));

   ocs::tom::set<CQ_ckpt_list>(ep_cq, std::move(lp_astrlist));

   auto lp_astrlist_2 = ocs::tom::ListPtr<ocs::tom::ASTRLIST>::create("PE List");
   
   auto ep2_astrlist_2 = ocs::tom::ElementPtr<ocs::tom::ASTRLIST>::create();
   ocs::tom::set<ASTRLIST_href>(ep2_astrlist_2, "Test_Name36");
   
   auto lp2_st_2 = ocs::tom::ListPtr<ocs::tom::ST>::create("String List");
   
   auto ep3_st_3 = ocs::tom::ElementPtr<ocs::tom::ST>::create();
   ocs::tom::set<ST_name>(ep3_st_3, "Test_Name37");
   lAppendElem(*lp2_st_2, std::move(ep3_st_3));

   auto ep3_st_4 = ocs::tom::ElementPtr<ocs::tom::ST>::create();
   ocs::tom::set<ST_name>(ep3_st_4, "Test_Name38");
   lAppendElem(*lp2_st_2, std::move(ep3_st_4));
   
   ocs::tom::set<ASTRLIST_value>(ep2_astrlist_2, std::move(lp2_st_2));
   lAppendElem(*lp_astrlist_2, std::move(ep2_astrlist_2));
   
   ocs::tom::set<CQ_pe_list>(ep_cq, std::move(lp_astrlist_2));

   auto lp_abool = ocs::tom::ListPtr<ocs::tom::ABOOL>::create("Rerun List");
   
   auto ep2_abool = ocs::tom::ElementPtr<ocs::tom::ABOOL>::create();
   ocs::tom::set<ABOOL_href>(ep2_abool, "Test_Name42");
   ocs::tom::set<ABOOL_value>(ep2_abool, true);
   lAppendElem(*lp_abool, std::move(ep2_abool));
   
   auto ep2_abool_2 = ocs::tom::ElementPtr<ocs::tom::ABOOL>::create();
   ocs::tom::set<ABOOL_href>(ep2_abool_2, "Test_Name43");
   ocs::tom::set<ABOOL_value>(ep2_abool_2, false);
   lAppendElem(*lp_abool, std::move(ep2_abool_2));

   ocs::tom::set<CQ_rerun>(ep_cq, std::move(lp_abool));

   auto lp_aulng_2 = ocs::tom::ListPtr<ocs::tom::AULNG>::create("Job Slots");
   
   auto ep2_aulng_4 = ocs::tom::ElementPtr<ocs::tom::AULNG>::create();
   ocs::tom::set<AULNG_href>(ep2_aulng_4, "Test_Name44");
   ocs::tom::set<AULNG_value>(ep2_aulng_4, 12);
   lAppendElem(*lp_aulng_2, std::move(ep2_aulng_4));
   
   auto ep2_aulng_5 = ocs::tom::ElementPtr<ocs::tom::AULNG>::create();
   ocs::tom::set<AULNG_href>(ep2_aulng_5, "Test_Name45");
   ocs::tom::set<AULNG_value>(ep2_aulng_5, 16);
   lAppendElem(*lp_aulng_2, std::move(ep2_aulng_5));

   auto ep2_aulng_6 = ocs::tom::ElementPtr<ocs::tom::AULNG>::create();
   ocs::tom::set<AULNG_href>(ep2_aulng_6, "Test_Name45_1");
   ocs::tom::set<AULNG_value>(ep2_aulng_6, 16);
   lAppendElem(*lp_aulng_2, std::move(ep2_aulng_6));

   auto ep2_aulng_7 = ocs::tom::ElementPtr<ocs::tom::AULNG>::create();
   ocs::tom::set<AULNG_href>(ep2_aulng_7, "Test_Name45_2");
   ocs::tom::set<AULNG_value>(ep2_aulng_7, 16);
   lAppendElem(*lp_aulng_2, std::move(ep2_aulng_7));

   auto ep2_aulng_8 = ocs::tom::ElementPtr<ocs::tom::AULNG>::create();
   ocs::tom::set<AULNG_href>(ep2_aulng_8, "Test_Name45_3");
   ocs::tom::set<AULNG_value>(ep2_aulng_8, 16);
   lAppendElem(*lp_aulng_2, std::move(ep2_aulng_8));

   ocs::tom::set<CQ_job_slots>(ep_cq, std::move(lp_aulng_2));

   auto lp_astr_3 = ocs::tom::ListPtr<ocs::tom::ASTR>::create("Tmp Dir List");
   
   auto ep2_astr_5 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ocs::tom::set<ASTR_href>(ep2_astr_5, "Test_Name46");
   ocs::tom::set<ASTR_value>(ep2_astr_5, "value");
   lAppendElem(*lp_astr_3, std::move(ep2_astr_5));
   
   auto ep2_astr_6 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ocs::tom::set<ASTR_href>(ep2_astr_6, "Test_Name47");
   ocs::tom::set<ASTR_value>(ep2_astr_6, "value");
   lAppendElem(*lp_astr_3, std::move(ep2_astr_6));

   ocs::tom::set<CQ_tmpdir>(ep_cq, std::move(lp_astr_3));

   auto lp_astr_4 = ocs::tom::ListPtr<ocs::tom::ASTR>::create("Shell List");
   
   auto ep2_astr_7 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ocs::tom::set<ASTR_href>(ep2_astr_7, "Test_Name48");
   ocs::tom::set<ASTR_value>(ep2_astr_7, "value");
   lAppendElem(*lp_astr_4, std::move(ep2_astr_7));
   
   auto ep2_astr_8 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ocs::tom::set<ASTR_href>(ep2_astr_8, "Test_Name49");
   ocs::tom::set<ASTR_value>(ep2_astr_8, "value");
   lAppendElem(*lp_astr_4, std::move(ep2_astr_8));

   ocs::tom::set<CQ_shell>(ep_cq, std::move(lp_astr_4));

   auto lp_astr_5 = ocs::tom::ListPtr<ocs::tom::ASTR>::create("Prolog List");
   
   auto ep2_astr_9 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ocs::tom::set<ASTR_href>(ep2_astr_9, "Test_Name50");
   ocs::tom::set<ASTR_value>(ep2_astr_9, "value");
   lAppendElem(*lp_astr_5, std::move(ep2_astr_9));
   
   auto ep2_astr_10 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ocs::tom::set<ASTR_href>(ep2_astr_10, "Test_Name51");
   ocs::tom::set<ASTR_value>(ep2_astr_10, "value");
   lAppendElem(*lp_astr_5, std::move(ep2_astr_10));

   ocs::tom::set<CQ_prolog>(ep_cq, std::move(lp_astr_5));

   auto lp_astr_6 = ocs::tom::ListPtr<ocs::tom::ASTR>::create("Epilog List");
   
   auto ep2_astr_11 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ocs::tom::set<ASTR_href>(ep2_astr_11, "Test_Name52");
   ocs::tom::set<ASTR_value>(ep2_astr_11, "value");
   lAppendElem(*lp_astr_6, std::move(ep2_astr_11));
   
   auto ep2_astr_12 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ocs::tom::set<ASTR_href>(ep2_astr_12, "Test_Name53");
   ocs::tom::set<ASTR_value>(ep2_astr_12, "value");
   lAppendElem(*lp_astr_6, std::move(ep2_astr_12));

   ocs::tom::set<CQ_epilog>(ep_cq, std::move(lp_astr_6));

   auto lp_astr_7 = ocs::tom::ListPtr<ocs::tom::ASTR>::create("Shell Start Modes");
   
   auto ep2_astr_13 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ocs::tom::set<ASTR_href>(ep2_astr_13, "Test_Name54");
   ocs::tom::set<ASTR_value>(ep2_astr_13, "value");
   lAppendElem(*lp_astr_7, std::move(ep2_astr_13));
   
   auto ep2_astr_14 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ocs::tom::set<ASTR_href>(ep2_astr_14, "Test_Name55");
   ocs::tom::set<ASTR_value>(ep2_astr_14, "value");
   lAppendElem(*lp_astr_7, std::move(ep2_astr_14));

   ocs::tom::set<CQ_shell_start_mode>(ep_cq, std::move(lp_astr_7));

   auto lp_astr_8 = ocs::tom::ListPtr<ocs::tom::ASTR>::create("Shell Start Methods");
   
   auto ep2_astr_15 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ocs::tom::set<ASTR_href>(ep2_astr_15, "Test_Name56");
   ocs::tom::set<ASTR_value>(ep2_astr_15, "value");
   lAppendElem(*lp_astr_8, std::move(ep2_astr_15));
   
   auto ep2_astr_16 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ocs::tom::set<ASTR_href>(ep2_astr_16, "Test_Name57");
   ocs::tom::set<ASTR_value>(ep2_astr_16, "value");
   lAppendElem(*lp_astr_8, std::move(ep2_astr_16));

   ocs::tom::set<CQ_starter_method>(ep_cq, std::move(lp_astr_8));

   auto lp_astr_9 = ocs::tom::ListPtr<ocs::tom::ASTR>::create("Shell Suspend Methods");
   
   auto ep2_astr_17 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ocs::tom::set<ASTR_href>(ep2_astr_17, "Test_Name58");
   ocs::tom::set<ASTR_value>(ep2_astr_17, "value");
   lAppendElem(*lp_astr_9, std::move(ep2_astr_17));
   
   auto ep2_astr_18 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ocs::tom::set<ASTR_href>(ep2_astr_18, "Test_Name59");
   ocs::tom::set<ASTR_value>(ep2_astr_18, "value");
   lAppendElem(*lp_astr_9, std::move(ep2_astr_18));

   ocs::tom::set<CQ_suspend_method>(ep_cq, std::move(lp_astr_9));

   auto lp_astr_10 = ocs::tom::ListPtr<ocs::tom::ASTR>::create("Shell Resume Methods");
   
   auto ep2_astr_19 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ocs::tom::set<ASTR_href>(ep2_astr_19, "Test_Name60");
   ocs::tom::set<ASTR_value>(ep2_astr_19, "value");
   lAppendElem(*lp_astr_10, std::move(ep2_astr_19));
   
   auto ep2_astr_20 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ocs::tom::set<ASTR_href>(ep2_astr_20, "Test_Name61");
   ocs::tom::set<ASTR_value>(ep2_astr_20, "value");
   lAppendElem(*lp_astr_10, std::move(ep2_astr_20));

   ocs::tom::set<CQ_resume_method>(ep_cq, std::move(lp_astr_10));

   auto lp_astr_11 = ocs::tom::ListPtr<ocs::tom::ASTR>::create("Shell Terminate Methods");
   
   auto ep2_astr_21 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ocs::tom::set<ASTR_href>(ep2_astr_21, "Test_Name62");
   ocs::tom::set<ASTR_value>(ep2_astr_21, "value");
   lAppendElem(*lp_astr_11, std::move(ep2_astr_21));
   
   auto ep2_astr_22 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ocs::tom::set<ASTR_href>(ep2_astr_22, "Test_Name63");
   ocs::tom::set<ASTR_value>(ep2_astr_22, "value");
   lAppendElem(*lp_astr_11, std::move(ep2_astr_22));

   ocs::tom::set<CQ_terminate_method>(ep_cq, std::move(lp_astr_11));

   auto lp_ainter_3 = ocs::tom::ListPtr<ocs::tom::AINTER>::create("Notify List");
   
   auto ep2_ainter_5 = ocs::tom::ElementPtr<ocs::tom::AINTER>::create();
   ocs::tom::set<AINTER_href>(ep2_ainter_5, "Test_Name64");
   ocs::tom::set<AINTER_value>(ep2_ainter_5, "46");
   lAppendElem(*lp_ainter_3, std::move(ep2_ainter_5));
   
   auto ep2_ainter_6 = ocs::tom::ElementPtr<ocs::tom::AINTER>::create();
   ocs::tom::set<AINTER_href>(ep2_ainter_6, "Test_Name65");
   ocs::tom::set<AINTER_value>(ep2_ainter_6, "77");
   lAppendElem(*lp_ainter_3, std::move(ep2_ainter_6));

   ocs::tom::set<CQ_notify>(ep_cq, std::move(lp_ainter_3));

   auto lp_ausrlist = ocs::tom::ListPtr<ocs::tom::AUSRLIST>::create("Owner List");
   
   auto ep2_ausrlist = ocs::tom::ElementPtr<ocs::tom::AUSRLIST>::create();
   ocs::tom::set<AUSRLIST_href>(ep2_ausrlist, "Test_Name66");
   
   auto lp2_us = ocs::tom::ListPtr<ocs::tom::US>::create("Userset List");
   
   auto ep3_us = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ocs::tom::set<US_name>(ep3_us, "Test_Name67");
   lAppendElem(*lp2_us, std::move(ep3_us));
   
   auto ep3_us_2 = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ocs::tom::set<US_name>(ep3_us_2, "Test_Name68");
   lAppendElem(*lp2_us, std::move(ep3_us_2));
   
   ocs::tom::set<AUSRLIST_value>(ep2_ausrlist, std::move(lp2_us));
   lAppendElem(*lp_ausrlist, std::move(ep2_ausrlist));
 
   ocs::tom::set<CQ_owner_list>(ep_cq, std::move(lp_ausrlist));

   auto lp_ausrlist_2 = ocs::tom::ListPtr<ocs::tom::AUSRLIST>::create("ACL List");
   
   auto ep2_ausrlist_2 = ocs::tom::ElementPtr<ocs::tom::AUSRLIST>::create();
   ocs::tom::set<AUSRLIST_href>(ep2_ausrlist_2, "Test_Name72");
   
   auto lp2_us_2 = ocs::tom::ListPtr<ocs::tom::US>::create("Userset List");
   
   auto ep3_us_3 = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ocs::tom::set<US_name>(ep3_us_3, "Test_Name73");
   lAppendElem(*lp2_us_2, std::move(ep3_us_3));
   
   auto ep3_us_4 = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ocs::tom::set<US_name>(ep3_us_4, "Test_Name74");
   lAppendElem(*lp2_us_2, std::move(ep3_us_4));
   
   ocs::tom::set<AUSRLIST_value>(ep2_ausrlist_2, std::move(lp2_us_2));
   lAppendElem(*lp_ausrlist_2, std::move(ep2_ausrlist_2));

   ocs::tom::set<CQ_acl>(ep_cq, std::move(lp_ausrlist_2));

   auto lp_ausrlist_3 = ocs::tom::ListPtr<ocs::tom::AUSRLIST>::create("XUser List");
   
   auto ep2_ausrlist_3 = ocs::tom::ElementPtr<ocs::tom::AUSRLIST>::create();
   ocs::tom::set<AUSRLIST_href>(ep2_ausrlist_3, "Test_Name78");
   
   auto lp2_us_3 = ocs::tom::ListPtr<ocs::tom::US>::create("Userset List");
   
   auto ep3_us_5 = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ocs::tom::set<US_name>(ep3_us_5, "Test_Name79");
   lAppendElem(*lp2_us_3, std::move(ep3_us_5));
   
   auto ep3_us_6 = ocs::tom::ElementPtr<ocs::tom::US>::create();
   ocs::tom::set<US_name>(ep3_us_6, "Test_Name80");
   lAppendElem(*lp2_us_3, std::move(ep3_us_6));
   
   ocs::tom::set<AUSRLIST_value>(ep2_ausrlist_3, std::move(lp2_us_3));
   lAppendElem(*lp_ausrlist_3, std::move(ep2_ausrlist_3));

   ocs::tom::set<CQ_xacl>(ep_cq, std::move(lp_ausrlist_3));

   auto lp_asolist = ocs::tom::ListPtr<ocs::tom::ASOLIST>::create("Subordinate List");
   
   auto ep2_asolist = ocs::tom::ElementPtr<ocs::tom::ASOLIST>::create();
   ocs::tom::set<ASOLIST_href>(ep2_asolist, "Test_Name84");

   auto lp2_so = ocs::tom::ListPtr<ocs::tom::SO>::create("Subordinate List");
   
   auto ep3_so = ocs::tom::ElementPtr<ocs::tom::SO>::create();
   ocs::tom::set<SO_name>(ep3_so, "Test_Name85");
   ocs::tom::set<SO_threshold>(ep3_so, 13);
   lAppendElem(*lp2_so, std::move(ep3_so));
   
   auto ep3_so_2 = ocs::tom::ElementPtr<ocs::tom::SO>::create();
   ocs::tom::set<SO_name>(ep3_so_2, "Test_Name86");
   ocs::tom::set<SO_threshold>(ep3_so_2, 16);
   lAppendElem(*lp2_so, std::move(ep3_so_2));

   ocs::tom::set<ASOLIST_value>(ep2_asolist, std::move(lp2_so));
   
   lAppendElem(*lp_asolist, std::move(ep2_asolist));
   
   ocs::tom::set<CQ_subordinate_list>(ep_cq, std::move(lp_asolist));
  
   auto lp_acelist_3 = ocs::tom::ListPtr<ocs::tom::ACELIST>::create("Consumable Configs List");
   
   auto ep2_acelist_3 = ocs::tom::ElementPtr<ocs::tom::ACELIST>::create();
   ocs::tom::set<ACELIST_href>(ep2_acelist_3, "Test_Name93");

   auto lp2_ce_2 = ocs::tom::ListPtr<ocs::tom::CE>::create("Complex List");
   
   auto ep3_ce_2 = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ocs::tom::set<CE_name>(ep3_ce_2, "Test_Name94");
   ocs::tom::set<CE_stringval>(ep3_ce_2, "stringval");
   lAppendElem(*lp2_ce_2, std::move(ep3_ce_2));

   ocs::tom::set<ACELIST_value>(ep2_acelist_3, std::move(lp2_ce_2));
   
   lAppendElem(*lp_acelist_3, std::move(ep2_acelist_3));

   ocs::tom::set<CQ_consumable_config_list>(ep_cq, std::move(lp_acelist_3));
   
   auto lp_aprjlist = ocs::tom::ListPtr<ocs::tom::APRJLIST>::create("Projects List");
   
   auto ep2_aprjlist = ocs::tom::ElementPtr<ocs::tom::APRJLIST>::create();
   ocs::tom::set<APRJLIST_href>(ep2_aprjlist, "Test_Name96");
   
   auto lp2_pr = ocs::tom::ListPtr<ocs::tom::PR>::create("Project List");
   
   auto ep3_pr = ocs::tom::ElementPtr<ocs::tom::PR>::create();
   ocs::tom::set<PR_name>(ep3_pr, "Test_Name97");
   lAppendElem(*lp2_pr, std::move(ep3_pr));
   
   auto ep3_pr_2 = ocs::tom::ElementPtr<ocs::tom::PR>::create();
   ocs::tom::set<PR_name>(ep3_pr_2, "Test_Name98");
   lAppendElem(*lp2_pr, std::move(ep3_pr_2));
   
   ocs::tom::set<APRJLIST_value>(ep2_aprjlist, std::move(lp2_pr));
   lAppendElem(*lp_aprjlist, std::move(ep2_aprjlist));

   ocs::tom::set<CQ_projects>(ep_cq, std::move(lp_aprjlist));

   auto lp_aprjlist_2 = ocs::tom::ListPtr<ocs::tom::APRJLIST>::create("XProjects List");
   
   auto ep2_aprjlist_2 = ocs::tom::ElementPtr<ocs::tom::APRJLIST>::create();
   ocs::tom::set<APRJLIST_href>(ep2_aprjlist_2, "Test_Name102");
   
   auto lp2_pr_2 = ocs::tom::ListPtr<ocs::tom::PR>::create("Project List");
   
   auto ep3_pr_3 = ocs::tom::ElementPtr<ocs::tom::PR>::create();
   ocs::tom::set<PR_name>(ep3_pr_3, "Test_Name103");
   lAppendElem(*lp2_pr_2, std::move(ep3_pr_3));
   
   auto ep3_pr_4 = ocs::tom::ElementPtr<ocs::tom::PR>::create();
   ocs::tom::set<PR_name>(ep3_pr_4, "Test_Name104");
   lAppendElem(*lp2_pr_2, std::move(ep3_pr_4));
   
   ocs::tom::set<APRJLIST_value>(ep2_aprjlist_2, std::move(lp2_pr_2));
   lAppendElem(*lp_aprjlist_2, std::move(ep2_aprjlist_2));

   ocs::tom::set<CQ_xprojects>(ep_cq, std::move(lp_aprjlist_2));

   auto lp_astr_12 = ocs::tom::ListPtr<ocs::tom::ASTR>::create("Calendar List");
   
   auto ep2_astr_23 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ocs::tom::set<ASTR_href>(ep2_astr_23, "Test_Name108");
   ocs::tom::set<ASTR_value>(ep2_astr_23, "value");
   lAppendElem(*lp_astr_12, std::move(ep2_astr_23));
   
   auto ep2_astr_24 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ocs::tom::set<ASTR_href>(ep2_astr_24, "Test_Name109");
   ocs::tom::set<ASTR_value>(ep2_astr_24, "value");
   lAppendElem(*lp_astr_12, std::move(ep2_astr_24));

   ocs::tom::set<CQ_calendar>(ep_cq, std::move(lp_astr_12));

   auto lp_astr_13 = ocs::tom::ListPtr<ocs::tom::ASTR>::create("Initial State List");
   
   auto ep2_astr_25 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ocs::tom::set<ASTR_href>(ep2_astr_25, "Test_Name110");
   ocs::tom::set<ASTR_value>(ep2_astr_25, "value");
   lAppendElem(*lp_astr_13, std::move(ep2_astr_25));
   
   auto ep2_astr_26 = ocs::tom::ElementPtr<ocs::tom::ASTR>::create();
   ocs::tom::set<ASTR_href>(ep2_astr_26, "Test_Name111");
   ocs::tom::set<ASTR_value>(ep2_astr_26, "value");
   lAppendElem(*lp_astr_13, std::move(ep2_astr_26));

   ocs::tom::set<CQ_initial_state>(ep_cq, std::move(lp_astr_13));

   auto lp_atime = ocs::tom::ListPtr<ocs::tom::ATIME>::create("s_rt List");
   
   auto ep2_atime = ocs::tom::ElementPtr<ocs::tom::ATIME>::create();
   ocs::tom::set<ATIME_href>(ep2_atime, "Test_Name112");
   ocs::tom::set<ATIME_value>(ep2_atime, "00:00:00");
   lAppendElem(*lp_atime, std::move(ep2_atime));
   
   auto ep2_atime_2 = ocs::tom::ElementPtr<ocs::tom::ATIME>::create();
   ocs::tom::set<ATIME_href>(ep2_atime_2, "Test_Name113");
   ocs::tom::set<ATIME_value>(ep2_atime_2, "23:59:59");
   lAppendElem(*lp_atime, std::move(ep2_atime_2));

   ocs::tom::set<CQ_s_rt>(ep_cq, std::move(lp_atime));

   auto lp_atime_2 = ocs::tom::ListPtr<ocs::tom::ATIME>::create("h_rt List");
   
   auto ep2_atime_3 = ocs::tom::ElementPtr<ocs::tom::ATIME>::create();
   ocs::tom::set<ATIME_href>(ep2_atime_3, "Test_Name114");
   ocs::tom::set<ATIME_value>(ep2_atime_3, "00:00:00");
   lAppendElem(*lp_atime_2, std::move(ep2_atime_3));
   
   auto ep2_atime_4 = ocs::tom::ElementPtr<ocs::tom::ATIME>::create();
   ocs::tom::set<ATIME_href>(ep2_atime_4, "Test_Name115");
   ocs::tom::set<ATIME_value>(ep2_atime_4, "23:59:59");
   lAppendElem(*lp_atime_2, std::move(ep2_atime_4));

   ocs::tom::set<CQ_h_rt>(ep_cq, std::move(lp_atime_2));

   auto lp_atime_3 = ocs::tom::ListPtr<ocs::tom::ATIME>::create("s_cpu List");
   
   auto ep2_atime_5 = ocs::tom::ElementPtr<ocs::tom::ATIME>::create();
   ocs::tom::set<ATIME_href>(ep2_atime_5, "Test_Name116");
   ocs::tom::set<ATIME_value>(ep2_atime_5, "00:00:00");
   lAppendElem(*lp_atime_3, std::move(ep2_atime_5));
   
   auto ep2_atime_6 = ocs::tom::ElementPtr<ocs::tom::ATIME>::create();
   ocs::tom::set<ATIME_href>(ep2_atime_6, "Test_Name117");
   ocs::tom::set<ATIME_value>(ep2_atime_6, "23:59:59");
   lAppendElem(*lp_atime_3, std::move(ep2_atime_6));

   ocs::tom::set<CQ_s_cpu>(ep_cq, std::move(lp_atime_3));

   auto lp_atime_4 = ocs::tom::ListPtr<ocs::tom::ATIME>::create("h_cpu List");
   
   auto ep2_atime_7 = ocs::tom::ElementPtr<ocs::tom::ATIME>::create();
   ocs::tom::set<ATIME_href>(ep2_atime_7, "Test_Name118");
   ocs::tom::set<ATIME_value>(ep2_atime_7, "00:00:00");
   lAppendElem(*lp_atime_4, std::move(ep2_atime_7));
   
   auto ep2_atime_8 = ocs::tom::ElementPtr<ocs::tom::ATIME>::create();
   ocs::tom::set<ATIME_href>(ep2_atime_8, "Test_Name119");
   ocs::tom::set<ATIME_value>(ep2_atime_8, "23:59:59");
   lAppendElem(*lp_atime_4, std::move(ep2_atime_8));

   ocs::tom::set<CQ_h_cpu>(ep_cq, std::move(lp_atime_4));

   auto lp_amem = ocs::tom::ListPtr<ocs::tom::AMEM>::create("s_fsize List");
   
   auto ep2_amem = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ocs::tom::set<AMEM_href>(ep2_amem, "Test_Name120");
   ocs::tom::set<AMEM_value>(ep2_amem, "1048576");
   lAppendElem(*lp_amem, std::move(ep2_amem));
   
   auto ep2_amem_2 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ocs::tom::set<AMEM_href>(ep2_amem_2, "Test_Name121");
   ocs::tom::set<AMEM_value>(ep2_amem_2, "1024");
   lAppendElem(*lp_amem, std::move(ep2_amem_2));

   ocs::tom::set<CQ_s_fsize>(ep_cq, std::move(lp_amem));

   auto lp_amem_2 = ocs::tom::ListPtr<ocs::tom::AMEM>::create("h_fsize List");
   
   auto ep2_amem_3 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ocs::tom::set<AMEM_href>(ep2_amem_3, "Test_Name122");
   ocs::tom::set<AMEM_value>(ep2_amem_3, "1048576");
   lAppendElem(*lp_amem_2, std::move(ep2_amem_3));
   
   auto ep2_amem_4 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ocs::tom::set<AMEM_href>(ep2_amem_4, "Test_Name123");
   ocs::tom::set<AMEM_value>(ep2_amem_4, "1024");
   lAppendElem(*lp_amem_2, std::move(ep2_amem_4));

   ocs::tom::set<CQ_h_fsize>(ep_cq, std::move(lp_amem_2));

   auto lp_amem_3 = ocs::tom::ListPtr<ocs::tom::AMEM>::create("s_data List");
   
   auto ep2_amem_5 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ocs::tom::set<AMEM_href>(ep2_amem_5, "Test_Name124");
   ocs::tom::set<AMEM_value>(ep2_amem_5, "1048576");
   lAppendElem(*lp_amem_3, std::move(ep2_amem_5));
   
   auto ep2_amem_6 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ocs::tom::set<AMEM_href>(ep2_amem_6, "Test_Name125");
   ocs::tom::set<AMEM_value>(ep2_amem_6, "1024");
   lAppendElem(*lp_amem_3, std::move(ep2_amem_6));

   ocs::tom::set<CQ_s_data>(ep_cq, std::move(lp_amem_3));

   auto lp_amem_4 = ocs::tom::ListPtr<ocs::tom::AMEM>::create("h_data List");
   
   auto ep2_amem_7 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ocs::tom::set<AMEM_href>(ep2_amem_7, "Test_Name126");
   ocs::tom::set<AMEM_value>(ep2_amem_7, "1048576");
   lAppendElem(*lp_amem_4, std::move(ep2_amem_7));
   
   auto ep2_amem_8 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ocs::tom::set<AMEM_href>(ep2_amem_8, "Test_Name127");
   ocs::tom::set<AMEM_value>(ep2_amem_8, "1024");
   lAppendElem(*lp_amem_4, std::move(ep2_amem_8));

   ocs::tom::set<CQ_h_data>(ep_cq, std::move(lp_amem_4));

   auto lp_amem_5 = ocs::tom::ListPtr<ocs::tom::AMEM>::create("s_stack List");
   
   auto ep2_amem_9 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ocs::tom::set<AMEM_href>(ep2_amem_9, "Test_Name128");
   ocs::tom::set<AMEM_value>(ep2_amem_9, "1048576");
   lAppendElem(*lp_amem_5, std::move(ep2_amem_9));
   
   auto ep2_amem_10 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ocs::tom::set<AMEM_href>(ep2_amem_10, "Test_Name129");
   ocs::tom::set<AMEM_value>(ep2_amem_10, "1024");
   lAppendElem(*lp_amem_5, std::move(ep2_amem_10));

   ocs::tom::set<CQ_s_stack>(ep_cq, std::move(lp_amem_5));

   auto lp_amem_6 = ocs::tom::ListPtr<ocs::tom::AMEM>::create("h_stack List");
   
   auto ep2_amem_11 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ocs::tom::set<AMEM_href>(ep2_amem_11, "Test_Name130");
   ocs::tom::set<AMEM_value>(ep2_amem_11, "1048576");
   lAppendElem(*lp_amem_6, std::move(ep2_amem_11));
   
   auto ep2_amem_12 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ocs::tom::set<AMEM_href>(ep2_amem_12, "Test_Name131");
   ocs::tom::set<AMEM_value>(ep2_amem_12, "1024");
   lAppendElem(*lp_amem_6, std::move(ep2_amem_12));

   ocs::tom::set<CQ_h_stack>(ep_cq, std::move(lp_amem_6));

   auto lp_amem_7 = ocs::tom::ListPtr<ocs::tom::AMEM>::create("s_core List");
   
   auto ep2_amem_13 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ocs::tom::set<AMEM_href>(ep2_amem_13, "Test_Name132");
   ocs::tom::set<AMEM_value>(ep2_amem_13, "1048576");
   lAppendElem(*lp_amem_7, std::move(ep2_amem_13));
   
   auto ep2_amem_14 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ocs::tom::set<AMEM_href>(ep2_amem_14, "Test_Name133");
   ocs::tom::set<AMEM_value>(ep2_amem_14, "1024");
   lAppendElem(*lp_amem_7, std::move(ep2_amem_14));

   ocs::tom::set<CQ_s_core>(ep_cq, std::move(lp_amem_7));

   auto lp_amem_8 = ocs::tom::ListPtr<ocs::tom::AMEM>::create("h_core List");
   
   auto ep2_amem_15 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ocs::tom::set<AMEM_href>(ep2_amem_15, "Test_Name134");
   ocs::tom::set<AMEM_value>(ep2_amem_15, "1048576");
   lAppendElem(*lp_amem_8, std::move(ep2_amem_15));
   
   auto ep2_amem_16 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ocs::tom::set<AMEM_href>(ep2_amem_16, "Test_Name135");
   ocs::tom::set<AMEM_value>(ep2_amem_16, "1024");
   lAppendElem(*lp_amem_8, std::move(ep2_amem_16));

   ocs::tom::set<CQ_h_core>(ep_cq, std::move(lp_amem_8));

   auto lp_amem_9 = ocs::tom::ListPtr<ocs::tom::AMEM>::create("s_rss List");
   
   auto ep2_amem_17 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ocs::tom::set<AMEM_href>(ep2_amem_17, "Test_Name136");
   ocs::tom::set<AMEM_value>(ep2_amem_17, "1048576");
   lAppendElem(*lp_amem_9, std::move(ep2_amem_17));
   
   auto ep2_amem_18 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ocs::tom::set<AMEM_href>(ep2_amem_18, "Test_Name137");
   ocs::tom::set<AMEM_value>(ep2_amem_18, "1024");
   lAppendElem(*lp_amem_9, std::move(ep2_amem_18));

   ocs::tom::set<CQ_s_rss>(ep_cq, std::move(lp_amem_9));

   auto lp_amem_10 = ocs::tom::ListPtr<ocs::tom::AMEM>::create("h_rss List");
   
   auto ep2_amem_19 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ocs::tom::set<AMEM_href>(ep2_amem_19, "Test_Name138");
   ocs::tom::set<AMEM_value>(ep2_amem_19, "1048576");
   lAppendElem(*lp_amem_10, std::move(ep2_amem_19));
   
   auto ep2_amem_20 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ocs::tom::set<AMEM_href>(ep2_amem_20, "Test_Name139");
   ocs::tom::set<AMEM_value>(ep2_amem_20, "1024");
   lAppendElem(*lp_amem_10, std::move(ep2_amem_20));

   ocs::tom::set<CQ_h_rss>(ep_cq, std::move(lp_amem_10));

   auto lp_amem_11 = ocs::tom::ListPtr<ocs::tom::AMEM>::create("s_vmem List");
   
   auto ep2_amem_21 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ocs::tom::set<AMEM_href>(ep2_amem_21, "Test_Name140");
   ocs::tom::set<AMEM_value>(ep2_amem_21, "1048576");
   lAppendElem(*lp_amem_11, std::move(ep2_amem_21));
   
   auto ep2_amem_22 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ocs::tom::set<AMEM_href>(ep2_amem_22, "Test_Name141");
   ocs::tom::set<AMEM_value>(ep2_amem_22, "1024");
   lAppendElem(*lp_amem_11, std::move(ep2_amem_22));

   ocs::tom::set<CQ_s_vmem>(ep_cq, std::move(lp_amem_11));

   auto lp_amem_12 = ocs::tom::ListPtr<ocs::tom::AMEM>::create("h_vmem List");
   
   auto ep2_amem_23 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ocs::tom::set<AMEM_href>(ep2_amem_23, "Test_Name142");
   ocs::tom::set<AMEM_value>(ep2_amem_23, "1048576");
   lAppendElem(*lp_amem_12, std::move(ep2_amem_23));
   
   auto ep2_amem_24 = ocs::tom::ElementPtr<ocs::tom::AMEM>::create();
   ocs::tom::set<AMEM_href>(ep2_amem_24, "Test_Name143");
   ocs::tom::set<AMEM_value>(ep2_amem_24, "1024");
   lAppendElem(*lp_amem_12, std::move(ep2_amem_24));

   ocs::tom::set<CQ_h_vmem>(ep_cq, std::move(lp_amem_12));

   /* Write a CQ file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, *ep_cq, false,
                                       CQ_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, false);
   
   /* Read a CQ file using flatfile spooling */
   ep_cq.reset();
   ep = spool_flatfile_read_object(&alp, CQ_Type, nullptr,
                                   CQ_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1);
   
   /* Write a CQ file using flatfile spooling */
   file2 = spool_flatfile_write_object(&alp, ep, false,
                                       CQ_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file2, false);
   
   lFreeElem(&ep);
   
   ret = diff(file1, file2);
   
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   
   answer_list_output(&alp);   
   
   return ret;
}

static int SC_test() {
   int ret = 0;
   lListElem *ep = nullptr;
   lList *alp = nullptr;
   const char *file1 = nullptr, *file2 = nullptr;
   
   auto ep_sc = ocs::tom::ElementPtr<ocs::tom::SC>::create();
   ocs::tom::set<SC_algorithm>(ep_sc, "algorithm");
   ocs::tom::set<SC_schedule_interval>(ep_sc, "30");
   ocs::tom::set<SC_maxujobs>(ep_sc, 1024);
   ocs::tom::set<SC_queue_sort_method>(ep_sc, 1);

   auto lp = ocs::tom::ListPtr<ocs::tom::CE>::create("Job Load Adjustments");

   auto ep2 = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ocs::tom::set<CE_name>(ep2, "Test_Name1");
   ocs::tom::set<CE_shortcut>(ep2, "tn1");
   ocs::tom::set<CE_valtype>(ep2, 1);
   ocs::tom::set<CE_relop>(ep2, 5);
   ocs::tom::set<CE_consumable>(ep2, 1);
   ocs::tom::set<CE_defaultval>(ep2, "15");
   ocs::tom::set<CE_requestable>(ep2, REQU_NO);
   ocs::tom::set<CE_urgency_weight>(ep2, "25");
   ocs::tom::set<CE_stringval>(ep2, "stringval");
   lAppendElem(*lp, std::move(ep2));
   
   auto ep2_ce = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ocs::tom::set<CE_name>(ep2_ce, "Test_Name2");
   ocs::tom::set<CE_shortcut>(ep2_ce, "tn2");
   ocs::tom::set<CE_valtype>(ep2_ce, 1);
   ocs::tom::set<CE_relop>(ep2_ce, 5);
   ocs::tom::set<CE_consumable>(ep2_ce, 0);
   ocs::tom::set<CE_defaultval>(ep2_ce, "15");
   ocs::tom::set<CE_requestable>(ep2_ce, REQU_YES);
   ocs::tom::set<CE_urgency_weight>(ep2_ce, "25");
   ocs::tom::set<CE_stringval>(ep2_ce, "stringval");
   lAppendElem(*lp, std::move(ep2_ce));

   ocs::tom::set<SC_job_load_adjustments>(ep_sc, std::move(lp));
   ocs::tom::set<SC_load_adjustment_decay_time>(ep_sc, "45");
   ocs::tom::set<SC_load_formula>(ep_sc, "load_formula");
   ocs::tom::set<SC_schedd_job_info>(ep_sc, "schedd_job_info");
   ocs::tom::set<SC_flush_submit_sec>(ep_sc, 128);
   ocs::tom::set<SC_flush_finish_sec>(ep_sc, 512);
   ocs::tom::set<SC_params>(ep_sc, "params");
   ocs::tom::set<SC_reprioritize_interval>(ep_sc, "15");
   ocs::tom::set<SC_halftime>(ep_sc, 32);
   
   auto lp_ua = ocs::tom::ListPtr<ocs::tom::UA>::create("Usage Weight List");
   
   auto ep2_ua = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ocs::tom::set<UA_name>(ep2_ua, "Test_Name3");
   ocs::tom::set<UA_value>(ep2_ua, 11.22);
   lAppendElem(*lp_ua, std::move(ep2_ua));
   
   auto ep2_ua_2 = ocs::tom::ElementPtr<ocs::tom::UA>::create();
   ocs::tom::set<UA_name>(ep2_ua_2, "Test_Name4");
   ocs::tom::set<UA_value>(ep2_ua_2, 33.44);
   lAppendElem(*lp_ua, std::move(ep2_ua_2));
      
   ocs::tom::set<SC_usage_weight_list>(ep_sc, std::move(lp_ua));
   
   ocs::tom::set<SC_compensation_factor>(ep_sc, 11.22);
   ocs::tom::set<SC_weight_user>(ep_sc, 11.22);
   ocs::tom::set<SC_weight_project>(ep_sc, 33.44);
   ocs::tom::set<SC_weight_department>(ep_sc, 66.77);
   ocs::tom::set<SC_weight_job>(ep_sc, 88.99);
   ocs::tom::set<SC_weight_tickets_functional>(ep_sc, 36);
   ocs::tom::set<SC_weight_tickets_share>(ep_sc, 18);
   ocs::tom::set<SC_share_override_tickets>(ep_sc, true);
   ocs::tom::set<SC_share_functional_shares>(ep_sc, true);
   ocs::tom::set<SC_max_functional_jobs_to_schedule>(ep_sc, 2048);
   ocs::tom::set<SC_report_pjob_tickets>(ep_sc, true);
   ocs::tom::set<SC_max_pending_tasks_per_job>(ep_sc, 256);
   ocs::tom::set<SC_halflife_decay_list>(ep_sc, "halflife_decay_list");
   ocs::tom::set<SC_policy_hierarchy>(ep_sc, "policy_hierarchy");
   ocs::tom::set<SC_weight_ticket>(ep_sc, 11.99);
   ocs::tom::set<SC_weight_waiting_time>(ep_sc, 22.88);
   ocs::tom::set<SC_weight_deadline>(ep_sc, 33.77);
   ocs::tom::set<SC_weight_urgency>(ep_sc, 44.66);
   ocs::tom::set<SC_weight_priority>(ep_sc, 55.99);
   ocs::tom::set<SC_max_reservation>(ep_sc, 33.00);
   ocs::tom::set<SC_default_duration>(ep_sc, "default_duration");

   /* Write a SC file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, *ep_sc, false,
                                       SC_fields,
                                       &qconf_comma_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, false);
   
   /* Read a SC file using flatfile spooling */
   ep_sc.reset();
   ep = spool_flatfile_read_object(&alp, SC_Type, nullptr,
                                   SC_fields, nullptr, true, &qconf_comma_sfi,
                                   SP_FORM_ASCII, nullptr, file1);
   
   /* Write a SC file using flatfile spooling */
   file2 = spool_flatfile_write_object(&alp, ep, false,
                                       SC_fields,
                                       &qconf_comma_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file2, false);
   
   lFreeElem(&ep);
   
   ret = diff(file1, file2);
   
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   
   answer_list_output(&alp);   
   
   return ret;
}

static int QU_test() {
   int ret = 0;
   lListElem *ep = nullptr;
   lList * alp = nullptr;
   spooling_field *fields = nullptr;
   const char *file1 = nullptr, *file2 = nullptr;
   
   auto ep_qu = ocs::tom::ElementPtr<ocs::tom::QU>::create();
   ocs::tom::set<QU_qhostname>(ep_qu, "Test_Name");
   ocs::tom::set<QU_qname>(ep_qu, "Test_Name2");
   ocs::tom::set<QU_state>(ep_qu, 1);
   ocs::tom::set<QU_pending_signal>(ep_qu, 2);
   ocs::tom::set<QU_pending_signal_delivery_time>(ep_qu, 3000000);
   ocs::tom::set<QU_version>(ep_qu, 4);


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
   ep = spool_flatfile_read_object(&alp, QU_Type, nullptr,
                                   fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1);
   
   /* Write a QU file using flatfile spooling */
   file2 = spool_flatfile_write_object(&alp, ep, false,
                                       fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file2, false);
   
   lFreeElem(&ep);
   
   ret = diff(file1, file2);
   
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   sge_free(&fields);
   
   answer_list_output(&alp);   
   
   return ret;
}

static int HGRP_test() {
   int ret = 0;
   lListElem *ep = nullptr;
   lList * alp = nullptr;
   const char *file1 = nullptr, *file2 = nullptr;
   
   auto ep_hgrp = ocs::tom::ElementPtr<ocs::tom::HGRP>::create();
   ocs::tom::set<HGRP_name>(ep_hgrp, "Test_Name");
  
   ocs::tom::emplace_back_in<HGRP_host_list, HR_name>(ep_hgrp, "Test_Name2");
   ocs::tom::emplace_back_in<HGRP_host_list, HR_name>(ep_hgrp, "Test_Name3");
   ocs::tom::emplace_back_in<HGRP_host_list, HR_name>(ep_hgrp, "Test_Name4");
   
   
   /* Write a HGRP file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, *ep_hgrp, false,
                                       HGRP_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, false);
   
   /* Read a HGRP file using flatfile spooling */
   ep_hgrp.reset();
   ep = spool_flatfile_read_object(&alp, HGRP_Type, nullptr,
                                   HGRP_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1);
   
   /* Write a HGRP file using flatfile spooling */
   file2 = spool_flatfile_write_object(&alp, ep, false,
                                       HGRP_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file2, false);
   lFreeElem(&ep);
   
   ret = diff(file1, file2);
   
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   
   answer_list_output(&alp);   
   
   return ret;
}

static int CONF_test() {
   int ret = 0;
   lListElem *ep = nullptr;
   lList * alp = nullptr;
   spooling_field *fields;
   const char *file1 = nullptr;
   const char *file2 = nullptr;
   
   auto ep_conf = ocs::tom::ElementPtr<ocs::tom::CONF>::create();
   ocs::tom::set<CONF_version>(ep_conf, 101);
   ocs::tom::set<CONF_name>(ep_conf, "host1");
   
   auto lp = ocs::tom::ListPtr<ocs::tom::CF>::create("Config List");
   
   auto ep2 = ocs::tom::ElementPtr<ocs::tom::CF>::create();
   ocs::tom::set<CF_name>(ep2, "gid_range");
   ocs::tom::set<CF_value>(ep2, "1000-1100");
   lAppendElem(*lp, std::move(ep2));
   
   auto ep2_cf = ocs::tom::ElementPtr<ocs::tom::CF>::create();
   ocs::tom::set<CF_name>(ep2_cf, "max_aj_tasks");
   ocs::tom::set<CF_value>(ep2_cf, "1001");
   lAppendElem(*lp, std::move(ep2_cf));
   
   auto ep2_cf_2 = ocs::tom::ElementPtr<ocs::tom::CF>::create();
   ocs::tom::set<CF_name>(ep2_cf_2, "admin_user");
   ocs::tom::set<CF_value>(ep2_cf_2, "testuser");
   lAppendElem(*lp, std::move(ep2_cf_2));
   
   auto ep2_cf_3 = ocs::tom::ElementPtr<ocs::tom::CF>::create();
   ocs::tom::set<CF_name>(ep2_cf_3, "user_lists");
   ocs::tom::set<CF_value>(ep2_cf_3, "me you someone_else");
   lAppendElem(*lp, std::move(ep2_cf_3));
   
   auto ep2_cf_4 = ocs::tom::ElementPtr<ocs::tom::CF>::create();
   ocs::tom::set<CF_name>(ep2_cf_4, "login_shells");
   ocs::tom::set<CF_value>(ep2_cf_4, "sh,ksh,csh,tcsh");
   lAppendElem(*lp, std::move(ep2_cf_4));

   auto ep2_cf_5 = ocs::tom::ElementPtr<ocs::tom::CF>::create();
   ocs::tom::set<CF_name>(ep2_cf_5, "execd_params");
   ocs::tom::set<CF_value>(ep2_cf_5, "PTF_MIN_PRIORITY=20,PTF_MAX_PRIORITY=0,SET_LIB_PATH=true");
   lAppendElem(*lp, std::move(ep2_cf_5));

   auto ep2_cf_6 = ocs::tom::ElementPtr<ocs::tom::CF>::create();
   ocs::tom::set<CF_name>(ep2_cf_6, "reporting_params");
   ocs::tom::set<CF_value>(ep2_cf_6, "accounting=true reporting=false flush_time=00:00:05 joblog=true sharelog=00:10:00=true");
   lAppendElem(*lp, std::move(ep2_cf_6));
   
   ocs::tom::set<CONF_entries>(ep_conf, std::move(lp));
   

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
   ep = spool_flatfile_read_object(&alp, CONF_Type, nullptr,
                                   fields, nullptr, false, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1);

   /* Write a CONF file using flatfile spooling */
   file2 = spool_flatfile_write_object(&alp, ep, false,
                                       fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file2, true);

   lFreeElem(&ep);
   
   ret = diff(file1, file2);
   
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   sge_free(&fields);
   
   answer_list_output(&alp);   
   
   return ret;
}

static int RQS_test() {
   int ret = 0;
   lList *alp = nullptr;
   const char *file1 = nullptr;
   const char *file2 = nullptr;
   lList* rqs_list = lCreateList("test", RQS_Type);

   auto ep = ocs::tom::ElementPtr<ocs::tom::RQS>::create();
   ocs::tom::set<RQS_name>(ep, "Test_Name1");
   ocs::tom::set<RQS_description>(ep, "Test Description");
   ocs::tom::set<RQS_enabled>(ep, false);
   auto lp1 = ocs::tom::ListPtr<ocs::tom::RQR>::create("Rule_List");
   /* rule 1 */
   auto ep2 = ocs::tom::ElementPtr<ocs::tom::RQR>::create();
      auto ep3 = ocs::tom::ElementPtr<ocs::tom::RQRF>::create();
      ocs::tom::set<RQRF_expand>(ep3, true);
      ocs::tom::emplace_back_in<RQRF_scope, ST_name>(ep3, "Test_User1");

      ocs::tom::emplace_back_in<RQRF_xscope, ST_name>(ep3, "Test_User2");
      ocs::tom::emplace_back_in<RQRF_xscope, ST_name>(ep3, "Test_User3");
      ocs::tom::set<RQR_filter_users>(ep2, std::move(ep3));
      
      auto ep3_rqrf = ocs::tom::ElementPtr<ocs::tom::RQRF>::create();
      ocs::tom::set<RQRF_expand>(ep3_rqrf, false);
      ocs::tom::emplace_back_in<RQRF_scope, ST_name>(ep3_rqrf, "Test_Host1");
      ocs::tom::emplace_back_in<RQRF_xscope, ST_name>(ep3_rqrf, "Test_Host2");
      ocs::tom::emplace_back_in<RQRF_xscope, ST_name>(ep3_rqrf, "Test_Host3");
      ocs::tom::set<RQR_filter_hosts>(ep2, std::move(ep3_rqrf));

      auto ep3_rqrf_2 = ocs::tom::ElementPtr<ocs::tom::RQRF>::create();
      ocs::tom::set<RQRF_expand>(ep3_rqrf_2, false);
      ocs::tom::emplace_back_in<RQRF_scope, ST_name>(ep3_rqrf_2, "Test_PE1");
      ocs::tom::emplace_back_in<RQRF_xscope, ST_name>(ep3_rqrf_2, "Test_PE2");
      ocs::tom::emplace_back_in<RQRF_xscope, ST_name>(ep3_rqrf_2, "Test_PE3");
      ocs::tom::set<RQR_filter_pes>(ep2, std::move(ep3_rqrf_2));


      auto limit_list = ocs::tom::ListPtr<ocs::tom::RQRL>::create("Limit_List");
      auto limit = ocs::tom::ElementPtr<ocs::tom::RQRL>::create();
      ocs::tom::set<RQRL_name>(limit, "slots");
      ocs::tom::set<RQRL_value>(limit, "2*$num_proc");
      lAppendElem(*limit_list, std::move(limit));
      ocs::tom::set<RQR_limit>(ep2, std::move(limit_list));
   lAppendElem(*lp1, std::move(ep2));
   /* rule 2 */
   auto ep2_rqr = ocs::tom::ElementPtr<ocs::tom::RQR>::create();
      auto ep3_rqrf_3 = ocs::tom::ElementPtr<ocs::tom::RQRF>::create();
      ocs::tom::set<RQRF_expand>(ep3_rqrf_3, true);
      ocs::tom::emplace_back_in<RQRF_scope, ST_name>(ep3_rqrf_3, "Test_Queue1");

      ocs::tom::emplace_back_in<RQRF_xscope, ST_name>(ep3_rqrf_3, "Test_Queue2");
      ocs::tom::set<RQR_filter_queues>(ep2_rqr, std::move(ep3_rqrf_3));

      auto limit_list_rqrl = ocs::tom::ListPtr<ocs::tom::RQRL>::create("Limit_List");
      auto limit_rqrl = ocs::tom::ElementPtr<ocs::tom::RQRL>::create();
      ocs::tom::set<RQRL_name>(limit_rqrl, "arch");
      ocs::tom::set<RQRL_value>(limit_rqrl, "lx-amd64");
      lAppendElem(*limit_list_rqrl, std::move(limit_rqrl));
      ocs::tom::set<RQR_limit>(ep2_rqr, std::move(limit_list_rqrl));
   lAppendElem(*lp1, std::move(ep2_rqr));
   /* rule 3 */
   auto ep2_rqr_2 = ocs::tom::ElementPtr<ocs::tom::RQR>::create();
   ocs::tom::set<RQR_name>(ep2_rqr_2, "rule3");
      auto ep3_rqrf_4 = ocs::tom::ElementPtr<ocs::tom::RQRF>::create();
      ocs::tom::set<RQRF_expand>(ep3_rqrf_4, true);
      ocs::tom::emplace_back_in<RQRF_scope, ST_name>(ep3_rqrf_4, "Test_Pe1");

      ocs::tom::emplace_back_in<RQRF_xscope, ST_name>(ep3_rqrf_4, "Test_Pe2");
      ocs::tom::set<RQR_filter_pes>(ep2_rqr_2, std::move(ep3_rqrf_4));

      auto limit_list_rqrl_2 = ocs::tom::ListPtr<ocs::tom::RQRL>::create("Limit_List");
      auto limit_rqrl_2 = ocs::tom::ElementPtr<ocs::tom::RQRL>::create();
      ocs::tom::set<RQRL_name>(limit_rqrl_2, "mem");
      ocs::tom::set<RQRL_value>(limit_rqrl_2, "1G");
      lAppendElem(*limit_list_rqrl_2, std::move(limit_rqrl_2));
      ocs::tom::set<RQR_limit>(ep2_rqr_2, std::move(limit_list_rqrl_2));
   lAppendElem(*lp1, std::move(ep2_rqr_2));
   ocs::tom::set<RQS_rule>(ep, std::move(lp1));
   lAppendElem(rqs_list, std::move(ep));

   /* rule 4 */
   auto ep_rqs = ocs::tom::ElementPtr<ocs::tom::RQS>::create();
   ocs::tom::set<RQS_name>(ep_rqs, "Test_Name2");
   ocs::tom::set<RQS_description>(ep_rqs, "Test Description");
   ocs::tom::set<RQS_enabled>(ep_rqs, true);
   auto lp1_rqr = ocs::tom::ListPtr<ocs::tom::RQR>::create("Rule_List");
   auto ep2_rqr_3 = ocs::tom::ElementPtr<ocs::tom::RQR>::create();
      auto ep3_rqrf_5 = ocs::tom::ElementPtr<ocs::tom::RQRF>::create();
      ocs::tom::set<RQRF_expand>(ep3_rqrf_5, false);
      ocs::tom::emplace_back_in<RQRF_scope, ST_name>(ep3_rqrf_5, "roland");

      ocs::tom::emplace_back_in<RQRF_xscope, ST_name>(ep3_rqrf_5, "andre");
      ocs::tom::set<RQR_filter_users>(ep2_rqr_3, std::move(ep3_rqrf_5));

      auto limit_list_rqrl_3 = ocs::tom::ListPtr<ocs::tom::RQRL>::create("Limit_List");
      /* first limit */
      auto limit_rqrl_3 = ocs::tom::ElementPtr<ocs::tom::RQRL>::create();
      ocs::tom::set<RQRL_name>(limit_rqrl_3, "mem");
      ocs::tom::set<RQRL_value>(limit_rqrl_3, "10G");
      lAppendElem(*limit_list_rqrl_3, std::move(limit_rqrl_3));
      /* second limit */
      auto limit_rqrl_4 = ocs::tom::ElementPtr<ocs::tom::RQRL>::create();
      ocs::tom::set<RQRL_name>(limit_rqrl_4, "arch");
      ocs::tom::set<RQRL_value>(limit_rqrl_4, "sol-sparc64");
      lAppendElem(*limit_list_rqrl_3, std::move(limit_rqrl_4));

      ocs::tom::set<RQR_limit>(ep2_rqr_3, std::move(limit_list_rqrl_3));
   lAppendElem(*lp1_rqr, std::move(ep2_rqr_3));
   ocs::tom::set<RQS_rule>(ep_rqs, std::move(lp1_rqr));
   lAppendElem(rqs_list, std::move(ep_rqs));


   /* Write a RQS file using classic spooling */
   file1 = spool_flatfile_write_list(&alp, rqs_list, RQS_fields, &qconf_rqs_sfi, SP_DEST_TMP, SP_FORM_ASCII, file1, false);

   /* Read a RQS file using flatfile spooling */
   lFreeList(&rqs_list);
   rqs_list = spool_flatfile_read_list(&alp, RQS_Type, RQS_fields, nullptr, true, &qconf_rqs_sfi,
                                   SP_FORM_ASCII, nullptr, file1);

   /* Write a RQS file using flatfile spooling */
   file2 = spool_flatfile_write_list(&alp, rqs_list, RQS_fields, &qconf_rqs_sfi, SP_DEST_TMP, SP_FORM_ASCII, file2, false);
   lFreeList(&rqs_list);

   ret = diff(file1, file2);

   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);

   answer_list_output(&alp);

   return ret;
}

static int AR_test() {
   int ret = 0;
   lListElem *ep = nullptr;
   lList *alp = nullptr;
   const char *file1 = nullptr, *file2 = nullptr;
   
   auto ep_ar = ocs::tom::ElementPtr<ocs::tom::AR>::create();
   ocs::tom::set<AR_id>(ep_ar, 1);
   ocs::tom::set<AR_name>(ep_ar, "Test_Name");

   auto lp = ocs::tom::ListPtr<ocs::tom::RN>::create("range_list");
   auto ep1 = ocs::tom::ElementPtr<ocs::tom::RN>::create();
   ocs::tom::set<RN_min>(ep1, 1);
   ocs::tom::set<RN_max>(ep1, 2);
   ocs::tom::set<RN_step>(ep1, 3);
   lAppendElem(*lp, std::move(ep1));
   auto ep1_rn = ocs::tom::ElementPtr<ocs::tom::RN>::create();
   ocs::tom::set<RN_min>(ep1_rn, 4);
   ocs::tom::set<RN_max>(ep1_rn, 5);
   ocs::tom::set<RN_step>(ep1_rn, 6);
   lAppendElem(*lp, std::move(ep1_rn));
   ocs::tom::set<AR_pe_range>(ep_ar, std::move(lp));

   auto lp_ce = ocs::tom::ListPtr<ocs::tom::CE>::create("resource_list");
   auto ep1_ce = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ocs::tom::set<CE_name>(ep1_ce, "ce1");
   ocs::tom::set<CE_stringval>(ep1_ce, "101");
   lAppendElem(*lp_ce, std::move(ep1_ce));
   auto ep1_ce_2 = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   ocs::tom::set<CE_name>(ep1_ce_2, "ce2");
   ocs::tom::set<CE_doubleval>(ep1_ce_2, 102);
   lAppendElem(*lp_ce, std::move(ep1_ce_2));
   ocs::tom::set<AR_resource_list>(ep_ar, std::move(lp_ce));

   ocs::tom::emplace_back_in<AR_queue_list, QR_name>(ep_ar, "queue1.q");
   ocs::tom::emplace_back_in<AR_queue_list, QR_name>(ep_ar, "queue2.q");
   ocs::tom::emplace_back_in<AR_queue_list, QR_name>(ep_ar, "queue3.q");
   ocs::tom::emplace_back_in<AR_queue_list, QR_name>(ep_ar, "queue4.q");

   ocs::tom::Element<ocs::tom::JG> ep1_jg = ocs::tom::emplace_back_in<AR_granted_slots, JG_qname>(ep_ar, "all.q@bla");
   ocs::tom::set<JG_slots>(ep1_jg, 5);
   ocs::tom::Element<ocs::tom::JG> ep1_jg_2 = ocs::tom::emplace_back_in<AR_granted_slots, JG_qname>(ep_ar, "my.q@bla");
   ocs::tom::set<JG_slots>(ep1_jg_2, 10);

   ocs::tom::emplace_back_in<AR_mail_list, MR_user>(ep_ar, "user1");
   ocs::tom::Element<ocs::tom::MR> ep1_mr = ocs::tom::emplace_back_in<AR_mail_list, MR_user>(ep_ar, "user2");
   ocs::tom::set<MR_host>(ep1_mr, "sun.com");

   ocs::tom::emplace_back_in<AR_master_queue_list, QR_name>(ep_ar, "queue1.q");
   ocs::tom::emplace_back_in<AR_master_queue_list, QR_name>(ep_ar, "queue2.q");
   ocs::tom::emplace_back_in<AR_master_queue_list, QR_name>(ep_ar, "queue3.q");
   ocs::tom::emplace_back_in<AR_master_queue_list, QR_name>(ep_ar, "queue4.q");

   ocs::tom::Element<ocs::tom::ARA> ep1_ara = ocs::tom::emplace_back_in<AR_acl_list, ARA_name>(ep_ar, "user1");
   ocs::tom::set<ARA_group>(ep1_ara, "group1");
   ocs::tom::Element<ocs::tom::ARA> ep1_ara_2 = ocs::tom::emplace_back_in<AR_acl_list, ARA_name>(ep_ar, "user2");
   ocs::tom::set<ARA_group>(ep1_ara_2, "group2");
   ocs::tom::emplace_back_in<AR_acl_list, ARA_name>(ep_ar, "@userset1");
   ocs::tom::emplace_back_in<AR_acl_list, ARA_name>(ep_ar, "@userset2");

  
   
   /* Write a AR file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, *ep_ar, false,
                                       AR_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, true);

   /* Read a AR file using flatfile spooling */
   ep_ar.reset();
   ep = spool_flatfile_read_object(&alp, AR_Type, nullptr,
                                   AR_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1);
   
   /* Write a AR file using flatfile spooling */
   file2 = spool_flatfile_write_object(&alp, ep, false,
                                       AR_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file2, true);
   lFreeElem(&ep);

   ret = diff(file1, file2);
  
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   
   answer_list_output(&alp);
   return ret;
}

static int PE_minimal_test() {
   lList *alp = nullptr;
   lListElem *ep = lCreateElem(PE_Type);
   const char *file1 = nullptr;
   const char *file2 = nullptr;

   // only name set — all other fields remain at CULL defaults
   ocs::tom::set<PE_name>(ep, "minimal_pe");

   file1 = spool_flatfile_write_object(&alp, ep, false,
                                       PE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file1, false);
   lFreeElem(&ep);
   ep = spool_flatfile_read_object(&alp, PE_Type, nullptr,
                                   PE_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1);
   file2 = spool_flatfile_write_object(&alp, ep, false,
                                       PE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file2, false);
   lFreeElem(&ep);
   int ret = diff(file1, file2);
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   answer_list_output(&alp);
   return ret;
}

// SECURITY REGRESSION (CS-2352, MEDIUM-SPOOL-001, CWE-732/CWE-276):
// spool_flatfile_open_file() used to create SP_DEST_SPOOL files with mode 0666
// (world-/group-writable, world-readable), relying solely on the daemon umask.
// Spool files hold authoritative cluster state and must be owner-only (0600).
// Write a minimal object via SP_DEST_SPOOL under umask(0) so the on-disk mode
// reflects exactly the requested create mode, then assert it is 0600 with no
// group/other bits. Pre-fix the file is 0666, so this returns non-zero.
static int SPOOL_perm_test() {
   lList *alp = nullptr;
   auto ep = ocs::tom::ElementPtr<ocs::tom::PE>::create();
   ocs::tom::set<PE_name>(ep, "perm_pe");

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

// SECURITY REGRESSION (CS-2352 directory hardening, CWE-732 / CWE-276):
// CS-2352 hardened the classic-spool *file* mode to 0600, but the directories
// that contain those files were created 0755 - world-readable and traversable -
// so their entry names (job IDs, queue-instance and object names, per-host
// config) still leaked to any local user able to reach the spool tree. The
// classic-spool startup function now creates its directories owner-only (0700).
// This exercises the real path via spool_classic_default_startup_func(), which -
// among the object directories - creates the "configs" subdirectory holding the
// global and per-host configurations (which now live in the spool dir, not the
// common dir), and asserts the resulting mode is exactly 0700. Red->green:
// pre-fix the directory is 0755.
static int SPOOL_dir_perm_test() {
   lList *alp = nullptr;

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
   ocs::tom::set<SPR_url>(rule, base);

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

// Regression for the classic spooling parameter change (RBAC config-in-spool-dir
// work): the configuration objects moved into the spool directory, so the classic
// spooling parameter is now a single absolute spool directory path. The obsolete
// two-argument "<common_dir>;<spool_dir>" form must be rejected (hard error, not
// silently accepted), as must a null or non-absolute path. The accept path (a
// valid single directory) allocates a full spooling context and is exercised by
// qmaster startup and the testsuite, so it is intentionally not re-created here;
// this test focuses on the rejection contract, which returns nullptr without
// allocating.
static int SPOOL_classic_params_test() {
   struct { const char *args; const char *desc; } rejected[] = {
      {nullptr,          "null args"},
      {"relative/path",  "non-absolute path"},
      {"/common;/spool", "legacy <common_dir>;<spool_dir> two-argument form"},
      {"/a;/b;/c",       "malformed multi-token form"},
   };

   int ret = 0;
   for (const auto &c : rejected) {
      lList *alp = nullptr;
      lListElem *ctx = spool_classic_create_context(&alp, c.args);
      if (ctx != nullptr) {
         printf("   spool_classic_create_context accepted %s (expected reject)\n", c.desc);
         lFreeElem(&ctx);
         ret = 1;
      }
      lFreeList(&alp);
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

static int CE_minimal_test() {
   lList *alp = nullptr;
   lListElem *ep = lCreateElem(CE_Type);
   const char *file1 = nullptr;
   const char *file2 = nullptr;

   // minimum valid CE: name + shortcut + lowest valid enum values; no defaultval or urgency_weight
   ocs::tom::set<CE_name>(ep, "minimal_ce");
   ocs::tom::set<CE_shortcut>(ep, "mc");
   ocs::tom::set<CE_valtype>(ep, 1);            // INT = first valid type (0 is not valid)
   ocs::tom::set<CE_relop>(ep, CMPLXEQ_OP);    // == = first valid relop
   ocs::tom::set<CE_requestable>(ep, REQU_NO);  // NO = first valid requestable

   file1 = spool_flatfile_write_object(&alp, ep, false,
                                       CE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file1, false);
   lFreeElem(&ep);
   ep = spool_flatfile_read_object(&alp, CE_Type, nullptr,
                                   CE_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1);
   file2 = spool_flatfile_write_object(&alp, ep, false,
                                       CE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file2, false);
   lFreeElem(&ep);
   int ret = diff(file1, file2);
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   answer_list_output(&alp);
   return ret;
}

static int PE_zero_slots_test() {
   lList *alp = nullptr;
   lListElem *ep = lCreateElem(PE_Type);
   const char *file1 = nullptr;
   const char *file2 = nullptr;

   ocs::tom::set<PE_name>(ep, "zero_slots_pe");
   ocs::tom::set<PE_slots>(ep, 0);

   file1 = spool_flatfile_write_object(&alp, ep, false,
                                       PE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file1, false);
   lFreeElem(&ep);
   ep = spool_flatfile_read_object(&alp, PE_Type, nullptr,
                                   PE_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1);
   file2 = spool_flatfile_write_object(&alp, ep, false,
                                       PE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file2, false);
   lFreeElem(&ep);
   int ret = diff(file1, file2);
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   answer_list_output(&alp);
   return ret;
}

static int PE_max_slots_test() {
   lList *alp = nullptr;
   lListElem *ep = lCreateElem(PE_Type);
   const char *file1 = nullptr;
   const char *file2 = nullptr;

   ocs::tom::set<PE_name>(ep, "max_slots_pe");
   ocs::tom::set<PE_slots>(ep, 4294967295U);   ///< UINT32_MAX — stress-tests ulong serialization

   file1 = spool_flatfile_write_object(&alp, ep, false,
                                       PE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file1, false);
   lFreeElem(&ep);
   ep = spool_flatfile_read_object(&alp, PE_Type, nullptr,
                                   PE_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1);
   file2 = spool_flatfile_write_object(&alp, ep, false,
                                       PE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file2, false);
   lFreeElem(&ep);
   int ret = diff(file1, file2);
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   answer_list_output(&alp);
   return ret;
}

static int CE_zero_values_test() {
   lList *alp = nullptr;
   lListElem *ep = lCreateElem(CE_Type);
   const char *file1 = nullptr;
   const char *file2 = nullptr;

   // enum fields at minimum valid values; consumable=0 (CONSUMABLE_NO is genuinely 0);
   // string fields explicitly set to "0" — tests zero-valued numeric strings round-trip
   ocs::tom::set<CE_name>(ep, "zero_ce");
   ocs::tom::set<CE_shortcut>(ep, "zc");
   ocs::tom::set<CE_valtype>(ep, 1);            // INT (0 is not a valid type)
   ocs::tom::set<CE_relop>(ep, CMPLXEQ_OP);    // == (0 is not a valid relop)
   ocs::tom::set<CE_consumable>(ep, CONSUMABLE_NO);  // 0 = "NO" (valid)
   ocs::tom::set<CE_defaultval>(ep, "0");
   ocs::tom::set<CE_requestable>(ep, REQU_NO);  // 1 (0 is not a valid requestable)
   ocs::tom::set<CE_urgency_weight>(ep, "0");

   file1 = spool_flatfile_write_object(&alp, ep, false,
                                       CE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file1, false);
   lFreeElem(&ep);
   ep = spool_flatfile_read_object(&alp, CE_Type, nullptr,
                                   CE_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1);
   file2 = spool_flatfile_write_object(&alp, ep, false,
                                       CE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file2, false);
   lFreeElem(&ep);
   int ret = diff(file1, file2);
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   answer_list_output(&alp);
   return ret;
}

static int CE_max_values_test() {
   lList *alp = nullptr;
   lListElem *ep = lCreateElem(CE_Type);
   const char *file1 = nullptr;
   const char *file2 = nullptr;

   // large numeric string in urgency_weight — tests wide-value serialization
   ocs::tom::set<CE_name>(ep, "max_ce");
   ocs::tom::set<CE_shortcut>(ep, "m");
   ocs::tom::set<CE_valtype>(ep, 1);
   ocs::tom::set<CE_relop>(ep, 5);
   ocs::tom::set<CE_consumable>(ep, 1);
   ocs::tom::set<CE_defaultval>(ep, "1234567890");
   ocs::tom::set<CE_requestable>(ep, REQU_NO);
   ocs::tom::set<CE_urgency_weight>(ep, "1234567890");

   file1 = spool_flatfile_write_object(&alp, ep, false,
                                       CE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file1, false);
   lFreeElem(&ep);
   ep = spool_flatfile_read_object(&alp, CE_Type, nullptr,
                                   CE_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1);
   file2 = spool_flatfile_write_object(&alp, ep, false,
                                       CE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file2, false);
   lFreeElem(&ep);
   int ret = diff(file1, file2);
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   answer_list_output(&alp);
   return ret;
}

static int PE_spaces_test() {
   lList *alp = nullptr;
   lListElem *ep = lCreateElem(PE_Type);
   const char *file1 = nullptr;
   const char *file2 = nullptr;

   // embedded spaces in proc args test token preservation across write/read
   ocs::tom::set<PE_name>(ep, "spaces_pe");
   ocs::tom::set<PE_slots>(ep, 4);
   ocs::tom::set<PE_start_proc_args>(ep, "/bin/sh -c start_script arg1 arg2");
   ocs::tom::set<PE_stop_proc_args>(ep, "/bin/sh -c stop_script arg1 arg2");
   ocs::tom::set<PE_allocation_rule>(ep, "$pe_slots");
   ocs::tom::set<PE_control_slaves>(ep, true);
   ocs::tom::set<PE_job_is_first_task>(ep, false);
   ocs::tom::set<PE_urgency_slots>(ep, "min");

   file1 = spool_flatfile_write_object(&alp, ep, false,
                                       PE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file1, false);
   lFreeElem(&ep);
   ep = spool_flatfile_read_object(&alp, PE_Type, nullptr,
                                   PE_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1);
   file2 = spool_flatfile_write_object(&alp, ep, false,
                                       PE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file2, false);
   lFreeElem(&ep);
   int ret = diff(file1, file2);
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   answer_list_output(&alp);
   return ret;
}

static int CE_special_test() {
   lList *alp = nullptr;
   lListElem *ep = lCreateElem(CE_Type);
   const char *file1 = nullptr;
   const char *file2 = nullptr;

   // decimal and scientific notation strings test parser tolerance for unusual number formats
   ocs::tom::set<CE_name>(ep, "special_ce");
   ocs::tom::set<CE_shortcut>(ep, "sc");
   ocs::tom::set<CE_valtype>(ep, 1);
   ocs::tom::set<CE_relop>(ep, 5);
   ocs::tom::set<CE_consumable>(ep, 0);
   ocs::tom::set<CE_defaultval>(ep, "0.001");
   ocs::tom::set<CE_requestable>(ep, REQU_YES);
   ocs::tom::set<CE_urgency_weight>(ep, "1.5e2");

   file1 = spool_flatfile_write_object(&alp, ep, false,
                                       CE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file1, false);
   lFreeElem(&ep);
   ep = spool_flatfile_read_object(&alp, CE_Type, nullptr,
                                   CE_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1);
   file2 = spool_flatfile_write_object(&alp, ep, false,
                                       CE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file2, false);
   lFreeElem(&ep);
   int ret = diff(file1, file2);
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   answer_list_output(&alp);
   return ret;
}

static int PE_empty_lists_test() {
   lList *alp = nullptr;
   lListElem *ep = lCreateElem(PE_Type);
   const char *file1 = nullptr;
   const char *file2 = nullptr;

   // explicitly empty lists (not null) — verify they serialize identically to null lists
   ocs::tom::set<PE_name>(ep, "empty_lists_pe");
   ocs::tom::set<PE_slots>(ep, 1);
   ocs::tom::set<PE_user_list>(ep,  lCreateList("", US_Type));
   ocs::tom::set<PE_xuser_list>(ep, lCreateList("", US_Type));

   file1 = spool_flatfile_write_object(&alp, ep, false,
                                       PE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file1, false);
   lFreeElem(&ep);
   ep = spool_flatfile_read_object(&alp, PE_Type, nullptr,
                                   PE_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1);
   file2 = spool_flatfile_write_object(&alp, ep, false,
                                       PE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file2, false);
   lFreeElem(&ep);
   int ret = diff(file1, file2);
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   answer_list_output(&alp);
   return ret;
}

static int PE_large_list_test() {
   lList *alp = nullptr;
   lListElem *ep = lCreateElem(PE_Type);
   const char *file1 = nullptr;
   const char *file2 = nullptr;

   ocs::tom::set<PE_name>(ep, "large_list_pe");
   ocs::tom::set<PE_slots>(ep, 10);

   // 10-element user_list — verifies that list ordering and content survive round-trip
   auto lp = ocs::tom::ListPtr<ocs::tom::US>::create("User_List");
   for (int i = 0; i < 10; i++) {
      char name[32];
      snprintf(name, sizeof(name), "user%02d", i);
      auto ep2 = ocs::tom::ElementPtr<ocs::tom::US>::create();
      ocs::tom::set<US_name>(ep2, name);
      ocs::tom::set<US_fshare>(ep2, i * 100);
      lAppendElem(*lp, std::move(ep2));
   }
   ocs::tom::set<PE_user_list>(ep, std::move(lp));

   file1 = spool_flatfile_write_object(&alp, ep, false,
                                       PE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file1, false);
   lFreeElem(&ep);
   ep = spool_flatfile_read_object(&alp, PE_Type, nullptr,
                                   PE_fields, nullptr, true, &qconf_sfi,
                                   SP_FORM_ASCII, nullptr, file1);
   file2 = spool_flatfile_write_object(&alp, ep, false,
                                       PE_fields, &qconf_sfi,
                                       SP_DEST_TMP, SP_FORM_ASCII, file2, false);
   lFreeElem(&ep);
   int ret = diff(file1, file2);
   sge_unlink(nullptr, file1);
   sge_unlink(nullptr, file2);
   sge_free(&file1);
   sge_free(&file2);
   answer_list_output(&alp);
   return ret;
}
