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
   lList *lp = nullptr;
   lList *lp2 = nullptr;
   lList *alp = nullptr;
   lListElem *ep = lCreateElem(PE_Type);
   lListElem *ep2 = nullptr;
   lListElem *ep3 = nullptr;
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

   lp = lCreateList("Resource Utilization List", RUE_Type);
   
   ep2 = lCreateElem(RUE_Type);
   ocs::tom::set<RUE_name>(ep2, "Test_Name2");
   ocs::tom::set<RUE_utilized_now>(ep2, 12.345);
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(RUE_Type);
   ocs::tom::set<RUE_name>(ep2, "Test_Name3");
   ocs::tom::set<RUE_utilized_now>(ep2, 678.9);
   lAppendElem(lp, ep2);
   
   ocs::tom::set<PE_resource_utilization>(ep, lp);
   
   lp = lCreateList("User_List", US_Type);

   ep2 = lCreateElem(US_Type);   
   ocs::tom::set<US_name>(ep2, "First_User_List");
   ocs::tom::set<US_type>(ep2, 101);
   ocs::tom::set<US_fshare>(ep2, 303);
   ocs::tom::set<US_oticket>(ep2, 505);
   ocs::tom::set<US_job_cnt>(ep2, 707);
   ocs::tom::set<US_pending_job_cnt>(ep2, 909);
   
   lp2 = lCreateList("User_Entry_List", UE_Type);
   
   ep3 = lCreateElem(UE_Type);
   ocs::tom::set<UE_name>(ep3, "First_User");
   lAppendElem(lp2, ep3);
   
   ep3 = lCreateElem(UE_Type);
   ocs::tom::set<UE_name>(ep3, "Second_User");
   lAppendElem(lp2, ep3);

   ocs::tom::set<US_entries>(ep2, lp2);
   
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(US_Type);   
   ocs::tom::set<US_name>(ep2, "Second_User_List");
   ocs::tom::set<US_type>(ep2, 202);
   ocs::tom::set<US_fshare>(ep2, 404);
   ocs::tom::set<US_oticket>(ep2, 606);
   ocs::tom::set<US_job_cnt>(ep2, 808);
   ocs::tom::set<US_pending_job_cnt>(ep2, 0);
   
   lp2 = lCreateList("User_Entry_List", UE_Type);
   
   ep3 = lCreateElem(UE_Type);
   ocs::tom::set<UE_name>(ep3, "Third_User");
   lAppendElem(lp2, ep3);
   
   ep3 = lCreateElem(UE_Type);
   ocs::tom::set<UE_name>(ep3, "Fourth_User");
   lAppendElem(lp2, ep3);

   ocs::tom::set<US_entries>(ep2, lp2);
   
   lAppendElem(lp, ep2);
   
   ocs::tom::set<PE_user_list>(ep, lp);
   
   lp = lCreateList("XUser_List", US_Type);
   
   ep2 = lCreateElem(US_Type);   
   ocs::tom::set<US_name>(ep2, "First_XUser_List");
   ocs::tom::set<US_type>(ep2, 101);
   ocs::tom::set<US_fshare>(ep2, 303);
   ocs::tom::set<US_oticket>(ep2, 505);
   ocs::tom::set<US_job_cnt>(ep2, 707);
   ocs::tom::set<US_pending_job_cnt>(ep2, 909);
   
   lp2 = lCreateList("XUser_Entry_List", UE_Type);
   
   ep3 = lCreateElem(UE_Type);
   ocs::tom::set<UE_name>(ep3, "First_XUser");
   lAppendElem(lp2, ep3);
   
   ep3 = lCreateElem(UE_Type);
   ocs::tom::set<UE_name>(ep3, "Second_XUser");
   lAppendElem(lp2, ep3);

   ocs::tom::set<US_entries>(ep2, lp2);
   
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(US_Type);   
   ocs::tom::set<US_name>(ep2, "Second_XUser_List");
   ocs::tom::set<US_type>(ep2, 202);
   ocs::tom::set<US_fshare>(ep2, 404);
   ocs::tom::set<US_oticket>(ep2, 606);
   ocs::tom::set<US_job_cnt>(ep2, 808);
   ocs::tom::set<US_pending_job_cnt>(ep2, 0);
   
   lp2 = lCreateList("XUser_Entry_List", UE_Type);
   
   ep3 = lCreateElem(UE_Type);
   ocs::tom::set<UE_name>(ep3, "Third_XUser");
   lAppendElem(lp2, ep3);
   
   ep3 = lCreateElem(UE_Type);
   ocs::tom::set<UE_name>(ep3, "Fourth_XUser");
   lAppendElem(lp2, ep3);

   ocs::tom::set<US_entries>(ep2, lp2);
   
   lAppendElem(lp, ep2);
   
   ocs::tom::set<PE_xuser_list>(ep, lp);

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
   
   ep = lCreateElem(CAL_Type);   
   ocs::tom::set<CAL_name>(ep, "Test_Name");
   ocs::tom::set<CAL_year_calendar>(ep, "2004");
   ocs::tom::set<CAL_week_calendar>(ep, "KW 05");

   /* Write a CAL file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, ep, false,
                                       CAL_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, false);
   
   /* Read a CAL file using flatfile spooling */
   lFreeElem(&ep);
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
   
   ep = lCreateElem(CK_Type);
   ocs::tom::set<CK_name>(ep, "Test_Name");
   ocs::tom::set<CK_interface>(ep, "interface");
   ocs::tom::set<CK_ckpt_command>(ep, "ckpt_command");
   ocs::tom::set<CK_migr_command>(ep, "migr_command");
   ocs::tom::set<CK_rest_command>(ep, "rest_command");
   ocs::tom::set<CK_ckpt_dir>(ep, "ckpt_dir");
   ocs::tom::set<CK_when>(ep, "when");
   ocs::tom::set<CK_signal>(ep, "signal");
   ocs::tom::set<CK_job_pid>(ep, 1313);
   ocs::tom::set<CK_clean_command>(ep, "clean_command");
   
   /* Write a CK file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, ep, false,
                                       CK_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, false);
   
   /* Read a CK file using flatfile spooling */
   lFreeElem(&ep);
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
   lListElem *ep2 = nullptr;
   lListElem *ep3 = nullptr;
   lList *lp = nullptr;
   lList *lp2 = nullptr;
   lList *alp = nullptr;
   spooling_field *fields = nullptr;
   const char *file1 = nullptr;
   const char *file2 = nullptr;
   
   ep = lCreateElem(STN_Type);
   ocs::tom::set<STN_name>(ep, "Test_Name");
   ocs::tom::set<STN_type>(ep, 1);
   ocs::tom::set<STN_id>(ep, 0);
   ocs::tom::set<STN_shares>(ep, 99);
   ocs::tom::set<STN_version>(ep, 1);
   
   lp = lCreateList("Sharetree List", STN_Type);
   
   ep2 = lCreateElem(STN_Type);
   ocs::tom::set<STN_name>(ep2, "Test_Name2");
   ocs::tom::set<STN_type>(ep2, 1);
   ocs::tom::set<STN_id>(ep2, 1);
   ocs::tom::set<STN_shares>(ep2, 99);
   
   lp2 = lCreateList("Sharetree List", STN_Type);
   
   ep3 = lCreateElem(STN_Type);
   ocs::tom::set<STN_name>(ep3, "Test_Name3");
   ocs::tom::set<STN_type>(ep3, 1);
   ocs::tom::set<STN_id>(ep3, 2);
   ocs::tom::set<STN_shares>(ep3, 99);
   ocs::tom::set<STN_children>(ep3, nullptr);
   lAppendElem(lp2, ep3);
   
   ep3 = lCreateElem(STN_Type);
   ocs::tom::set<STN_name>(ep3, "Test_Name4");
   ocs::tom::set<STN_type>(ep3, 1);
   ocs::tom::set<STN_id>(ep3, 4);
   ocs::tom::set<STN_shares>(ep3, 99);
   ocs::tom::set<STN_children>(ep3, nullptr);
   lAppendElem(lp2, ep3);
   
   ocs::tom::set<STN_children>(ep2, lp2);
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(STN_Type);
   ocs::tom::set<STN_name>(ep2, "Test_Name5");
   ocs::tom::set<STN_type>(ep2, 1);
   ocs::tom::set<STN_id>(ep2, 5);
   ocs::tom::set<STN_shares>(ep2, 99);
   
   lp2 = lCreateList("Sharetree List", STN_Type);
   
   ep3 = lCreateElem(STN_Type);
   ocs::tom::set<STN_name>(ep3, "Test_Name6");
   ocs::tom::set<STN_type>(ep3, 1);
   ocs::tom::set<STN_id>(ep3, 7);
   ocs::tom::set<STN_shares>(ep3, 99);
   ocs::tom::set<STN_children>(ep3, nullptr);
   lAppendElem(lp2, ep3);
   
   ocs::tom::set<STN_children>(ep2, lp2);
   lAppendElem(lp, ep2);
   
   ocs::tom::set<STN_children>(ep, lp);

   fields = sge_build_STN_field_list(true, true);
   
   /* Write a STN file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, ep, false,
                                       fields,
                                       &qconf_name_value_list_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, false);

   /* Read a STN file using flatfile spooling */
   lFreeElem(&ep);
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
   
   ep = lCreateElem(CE_Type);
   ocs::tom::set<CE_name>(ep, "Test_Name");
   ocs::tom::set<CE_shortcut>(ep, "shortcut");
   ocs::tom::set<CE_valtype>(ep, 1);
   ocs::tom::set<CE_relop>(ep, 5);
   ocs::tom::set<CE_consumable>(ep, 1);
   ocs::tom::set<CE_defaultval>(ep, "10");
   ocs::tom::set<CE_requestable>(ep, REQU_NO);
   ocs::tom::set<CE_urgency_weight>(ep, "20");
   
   /* Write a CE file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, ep, false,
                                       CE_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, false);
   
   /* Read a CE file using flatfile spooling */
   lFreeElem(&ep);
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
   lListElem *ep = nullptr;
   lList *lp = nullptr;
   lList *alp = nullptr;
   const char *file1 = nullptr;
   const char *file2 = nullptr;
   
   lp = lCreateList("Complex List", CE_Type);
   
   ep = lCreateElem(CE_Type);
   ocs::tom::set<CE_name>(ep, "Test_Name1");
   ocs::tom::set<CE_shortcut>(ep, "tn1");
   ocs::tom::set<CE_valtype>(ep, 1);
   ocs::tom::set<CE_relop>(ep, 5);
   ocs::tom::set<CE_consumable>(ep, 2);
   ocs::tom::set<CE_defaultval>(ep, "15");
   ocs::tom::set<CE_requestable>(ep, REQU_NO);
   ocs::tom::set<CE_urgency_weight>(ep, "25");
   lAppendElem(lp, ep);
   
   ep = lCreateElem(CE_Type);
   ocs::tom::set<CE_name>(ep, "Test_Name2");
   ocs::tom::set<CE_shortcut>(ep, "tn2");
   ocs::tom::set<CE_valtype>(ep, 1);
   ocs::tom::set<CE_relop>(ep, 5);
   ocs::tom::set<CE_consumable>(ep, 0);
   ocs::tom::set<CE_defaultval>(ep, "15");
   ocs::tom::set<CE_requestable>(ep, REQU_YES);
   ocs::tom::set<CE_urgency_weight>(ep, "25");
   lAppendElem(lp, ep);
   
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
   lList *lp = nullptr;
   lListElem *ep2 = nullptr;

   lp = lCreateList("Usage List", UA_Type);
   
   ep2 = lCreateElem(UA_Type);
   ocs::tom::set<UA_name>(ep2, "Test_Name2");
   ocs::tom::set<UA_value>(ep2, 11.22);
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(UA_Type);
   ocs::tom::set<UA_name>(ep2, "Test_Name3");
   ocs::tom::set<UA_value>(ep2, 33.44);
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(UA_Type);
   ocs::tom::set<UA_name>(ep2, "Test_Name4");
   ocs::tom::set<UA_value>(ep2, 55.66);
   lAppendElem(lp, ep2);

   return lp;
}

static lList *buildLTUsageList() {
   lList *lp = nullptr;
   lListElem *ep2 = nullptr;

   lp = lCreateList("LT Usage List", UA_Type);
   
   ep2 = lCreateElem(UA_Type);
   ocs::tom::set<UA_name>(ep2, "Test_Name5");
   ocs::tom::set<UA_value>(ep2, 11.22);
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(UA_Type);
   ocs::tom::set<UA_name>(ep2, "Test_Name6");
   ocs::tom::set<UA_value>(ep2, 33.44);
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(UA_Type);
   ocs::tom::set<UA_name>(ep2, "Test_Name7");
   ocs::tom::set<UA_value>(ep2, 55.66);
   lAppendElem(lp, ep2);
 
   return lp;
}      

static lList *buildProjectList() {
   lList *lp = nullptr;
   lList *lp2 = nullptr;
   lListElem *ep2 = nullptr;
   lListElem *ep3 = nullptr;

   lp = lCreateList("Project List", UPP_Type);
   
   ep2 = lCreateElem(UPP_Type);
   ocs::tom::set<UPP_name>(ep2, "Test_Name8");
   
   lp2 = lCreateList("Usage List", UA_Type);
   
   ep3 = lCreateElem(UA_Type);
   ocs::tom::set<UA_name>(ep3, "Test_Name9");
   ocs::tom::set<UA_value>(ep3, 11.22);
   lAppendElem(lp2, ep3);
   
   ep3 = lCreateElem(UA_Type);
   ocs::tom::set<UA_name>(ep3, "Test_Name10");
   ocs::tom::set<UA_value>(ep3, 33.44);
   lAppendElem(lp2, ep3);
   
   ep3 = lCreateElem(UA_Type);
   ocs::tom::set<UA_name>(ep3, "Test_Name11");
   ocs::tom::set<UA_value>(ep3, 55.66);
   lAppendElem(lp2, ep3);
      
   ocs::tom::set<UPP_usage>(ep2, lp2);
   
   lp2 = lCreateList("LT Usage List", UA_Type);
   
   ep3 = lCreateElem(UA_Type);
   ocs::tom::set<UA_name>(ep3, "Test_Name12");
   ocs::tom::set<UA_value>(ep3, 11.22);
   lAppendElem(lp2, ep3);
   
   ep3 = lCreateElem(UA_Type);
   ocs::tom::set<UA_name>(ep3, "Test_Name13");
   ocs::tom::set<UA_value>(ep3, 33.44);
   lAppendElem(lp2, ep3);
   
   ep3 = lCreateElem(UA_Type);
   ocs::tom::set<UA_name>(ep3, "Test_Name14");
   ocs::tom::set<UA_value>(ep3, 55.66);
   lAppendElem(lp2, ep3);
      
   ocs::tom::set<UPP_long_term_usage>(ep2, lp2);
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(UPP_Type);
   ocs::tom::set<UPP_name>(ep2, "Test_Name15");
   
   lp2 = lCreateList("Usage List", UA_Type);
   
   ep3 = lCreateElem(UA_Type);
   ocs::tom::set<UA_name>(ep3, "Test_Name16");
   ocs::tom::set<UA_value>(ep3, 11.22);
   lAppendElem(lp2, ep3);
   
   ep3 = lCreateElem(UA_Type);
   ocs::tom::set<UA_name>(ep3, "Test_Name17");
   ocs::tom::set<UA_value>(ep3, 33.44);
   lAppendElem(lp2, ep3);
   
   ep3 = lCreateElem(UA_Type);
   ocs::tom::set<UA_name>(ep3, "Test_Name18");
   ocs::tom::set<UA_value>(ep3, 55.66);
   lAppendElem(lp2, ep3);
      
   ocs::tom::set<UPP_usage>(ep2, lp2);
   
   lp2 = lCreateList("LT Usage List", UA_Type);
   
   ep3 = lCreateElem(UA_Type);
   ocs::tom::set<UA_name>(ep3, "Test_Name19");
   ocs::tom::set<UA_value>(ep3, 11.22);
   lAppendElem(lp2, ep3);
   
   ep3 = lCreateElem(UA_Type);
   ocs::tom::set<UA_name>(ep3, "Test_Name20");
   ocs::tom::set<UA_value>(ep3, 33.44);
   lAppendElem(lp2, ep3);
   
   ep3 = lCreateElem(UA_Type);
   ocs::tom::set<UA_name>(ep3, "Test_Name21");
   ocs::tom::set<UA_value>(ep3, 55.66);
   lAppendElem(lp2, ep3);
      
   ocs::tom::set<UPP_long_term_usage>(ep2, lp2);
   lAppendElem(lp, ep2);

   return lp;
}

static lList *buildDebitedUsageList() {
   lList *lp = nullptr;
   lList *lp2 = nullptr;
   lListElem *ep = nullptr;
   lListElem *ep1 = nullptr;

   lp = lCreateList("Debited Usage List", UPU_Type);
   
   ep = lCreateElem(UPU_Type);
   ocs::tom::set<UPU_job_number>(ep, 13579);
   
   lp2 = lCreateList("Old Usage List", UA_Type);
   
   ep1 = lCreateElem(UA_Type);
   ocs::tom::set<UA_name>(ep1, "Test_Name26");
   ocs::tom::set<UA_value>(ep1, 11.22);
   lAppendElem(lp2, ep1);
   
   ep1 = lCreateElem(UA_Type);
   ocs::tom::set<UA_name>(ep1, "Test_Name27");
   ocs::tom::set<UA_value>(ep1, 33.44);
   lAppendElem(lp2, ep1);
   
   ep1 = lCreateElem(UA_Type);
   ocs::tom::set<UA_name>(ep1, "Test_Name28");
   ocs::tom::set<UA_value>(ep1, 55.66);
   lAppendElem(lp2, ep1);
      
   ocs::tom::set<UPU_old_usage_list>(ep, lp2);
   lAppendElem(lp, ep);
   
   ep = lCreateElem(UPU_Type);
   ocs::tom::set<UPU_job_number>(ep, 97531);
   
   lp2 = lCreateList("Old Usage List", UA_Type);
   
   ep1 = lCreateElem(UA_Type);
   ocs::tom::set<UA_name>(ep1, "Test_Name29");
   ocs::tom::set<UA_value>(ep1, 11.22);
   lAppendElem(lp2, ep1);
   
   ep1 = lCreateElem(UA_Type);
   ocs::tom::set<UA_name>(ep1, "Test_Name30");
   ocs::tom::set<UA_value>(ep1, 33.44);
   lAppendElem(lp2, ep1);
   
   ep1 = lCreateElem(UA_Type);
   ocs::tom::set<UA_name>(ep1, "Test_Name31");
   ocs::tom::set<UA_value>(ep1, 55.66);
   lAppendElem(lp2, ep1);
      
   ocs::tom::set<UPU_old_usage_list>(ep, lp2);
   lAppendElem(lp, ep);

   return lp;
}   

static lList *buildACLList() {
   lList *lp = nullptr;
   lListElem *ep2 = nullptr;

   lp = lCreateList("ACL List", US_Type);
   
   ep2 = lCreateElem(US_Type);
   ocs::tom::set<US_name>(ep2, "Test_Name22");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(US_Type);
   ocs::tom::set<US_name>(ep2, "Test_Name23");
   lAppendElem(lp, ep2);

   return lp;
}   
   
static lList *buildXACLList() {
   lList *lp = nullptr;
   lListElem *ep2 = nullptr;

   lp = lCreateList("XACL List", US_Type);
   
   ep2 = lCreateElem(US_Type);
   ocs::tom::set<US_name>(ep2, "Test_Name24");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(US_Type);
   ocs::tom::set<US_name>(ep2, "Test_Name25");
   lAppendElem(lp, ep2);

   return lp;
}   

static int UU_test() {
   int ret = 0;
   lListElem *user = nullptr;
   lList *lp = nullptr;
   lList *alp = nullptr;
   spooling_field *fields = nullptr;
   const char *file1 = nullptr;
   const char *file2 = nullptr;
   
   user = lCreateElem(UU_Type);
   ocs::tom::set<UU_name>(user, "Test_Name");
   ocs::tom::set<UU_oticket>(user, 100);
   ocs::tom::set<UU_fshare>(user, 50);
   ocs::tom::set<UU_delete_time>(user, (uint64_t)123456789000000);
   ocs::tom::set<UU_default_project>(user, "default_project");

   ocs::tom::set<UU_usage_time_stamp>(user, 987654321);

   lp = buildUsageList();
   ocs::tom::set<UU_usage>(user, lp);
   
   lp = buildLTUsageList();
   ocs::tom::set<UU_long_term_usage>(user, lp);
   
   lp = buildProjectList();
   ocs::tom::set<UU_project>(user, lp);
   
   lp = buildDebitedUsageList();
   ocs::tom::set<UU_debited_job_usage>(user, lp);


   fields = sge_build_UU_field_list(true);

   /* Write a UU file using flatfile spooling */
   file1 = spool_flatfile_write_object(&alp, user, false,
                                     fields,
                                     &qconf_sfi,
                                     SP_DEST_TMP,
                                     SP_FORM_ASCII, 
                                     file1, false);
   lFreeElem(&user);
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
   
   prj = lCreateElem(PR_Type);
   ocs::tom::set<PR_name>(prj, "Test_Name");
   ocs::tom::set<PR_oticket>(prj, 100);
   ocs::tom::set<PR_fshare>(prj, 50);
   ocs::tom::set<PR_usage_time_stamp>(prj, 987654321);

   lp = buildUsageList();
   ocs::tom::set<PR_usage>(prj, lp);
   
   lp = buildLTUsageList();
   ocs::tom::set<PR_long_term_usage>(prj, lp);
   
   lp = buildProjectList();
   ocs::tom::set<PR_project>(prj, lp);
   
   lp = buildACLList();
   ocs::tom::set<PR_acl>(prj, lp);
   
   lp = buildXACLList();
   ocs::tom::set<PR_xacl>(prj, lp);
   
   lp = buildDebitedUsageList();
   ocs::tom::set<PR_debited_job_usage>(prj, lp);

   fields = sge_build_PR_field_list(true);

   /* Write a PR file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, prj, false,
                                     fields,
                                     &qconf_sfi,
                                     SP_DEST_TMP,
                                     SP_FORM_ASCII, 
                                     file1, false);
   
   /* Read a PR file using flatfile spooling */
   lFreeElem(&prj);
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
   lListElem *ep2 = nullptr;
   lList *lp = nullptr;
   lList *alp = nullptr;
   const char *file1 = nullptr;
   const char *file2 = nullptr;
   
   ep = lCreateElem(US_Type);
   ocs::tom::set<US_name>(ep, "Test_Name");
   ocs::tom::set<US_type>(ep, 1);
   ocs::tom::set<US_fshare>(ep, 10);
   ocs::tom::set<US_oticket>(ep, 100);
   
   lp = lCreateList("Entry List", UE_Type);
   
   ep2 = lCreateElem(UE_Type);
   ocs::tom::set<UE_name>(ep2, "Test_Name2");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(UE_Type);
   ocs::tom::set<UE_name>(ep2, "Test_Name3");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(UE_Type);
   ocs::tom::set<UE_name>(ep2, "Test_Name4");
   lAppendElem(lp, ep2);
   
   ocs::tom::set<US_entries>(ep, lp);
   
   /* Write a US file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, ep, false,
                                       US_fields,
                                       &qconf_param_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, false);

   /* Read a US file using flatfile spooling */
   lFreeElem(&ep);
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
   lListElem *ep2 = nullptr;
   lList *lp = nullptr;
   lList *alp = nullptr;
   spooling_field *fields = nullptr;
   const char *file1 = nullptr;
   const char *file2 = nullptr;
   
   ep = lCreateElem(EH_Type);
   ocs::tom::set<EH_name>(ep, "Test_Name");
   
   lp = lCreateList("Load Scaling List", HS_Type);
   
   ep2 = lCreateElem(HS_Type);
   ocs::tom::set<HS_name>(ep2, "Test_Name2");
   ocs::tom::set<HS_value>(ep2, 1234.567);
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(HS_Type);
   ocs::tom::set<HS_name>(ep2, "Test_Name3");
   ocs::tom::set<HS_value>(ep2, 6.7);
   lAppendElem(lp, ep2);
   
   ocs::tom::set<EH_scaling_list>(ep, lp);
   
   lp = lCreateList("Consumable Config List", CE_Type);
   
   ep2 = lCreateElem(CE_Type);
   ocs::tom::set<CE_name>(ep2, "Test_Name4");
   ocs::tom::set<CE_shortcut>(ep2, "tn4");
   ocs::tom::set<CE_valtype>(ep2, 1);
   ocs::tom::set<CE_relop>(ep2, 5);
   ocs::tom::set<CE_consumable>(ep2, 1);
   ocs::tom::set<CE_defaultval>(ep2, "15");
   ocs::tom::set<CE_requestable>(ep2, REQU_NO);
   ocs::tom::set<CE_urgency_weight>(ep2, "25");
   ocs::tom::set<CE_stringval>(ep2, "stringval");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(CE_Type);
   ocs::tom::set<CE_name>(ep2, "Test_Name5");
   ocs::tom::set<CE_shortcut>(ep2, "tn5");
   ocs::tom::set<CE_valtype>(ep2, 1);
   ocs::tom::set<CE_relop>(ep2, 5);
   ocs::tom::set<CE_consumable>(ep2, 0);
   ocs::tom::set<CE_defaultval>(ep2, "15");
   ocs::tom::set<CE_requestable>(ep2, REQU_YES);
   ocs::tom::set<CE_urgency_weight>(ep2, "25");
   ocs::tom::set<CE_doubleval>(ep2, 6969.9696);
   lAppendElem(lp, ep2);
   
   ocs::tom::set<EH_consumable_config_list>(ep, lp);
   
   lp = lCreateList("Actual Complex Values List", CE_Type);

   ep2 = lCreateElem(CE_Type);
   ocs::tom::set<CE_name>(ep2, "Test_Name6");
   ocs::tom::set<CE_shortcut>(ep2, "tn6");
   ocs::tom::set<CE_valtype>(ep2, 1);
   ocs::tom::set<CE_relop>(ep2, 5);
   ocs::tom::set<CE_consumable>(ep2, 1);
   ocs::tom::set<CE_defaultval>(ep2, "15");
   ocs::tom::set<CE_requestable>(ep2, REQU_NO);
   ocs::tom::set<CE_urgency_weight>(ep2, "25");
   ocs::tom::set<CE_stringval>(ep2, "stringval");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(CE_Type);
   ocs::tom::set<CE_name>(ep2, "Test_Name7");
   ocs::tom::set<CE_shortcut>(ep2, "tn7");
   ocs::tom::set<CE_valtype>(ep2, 1);
   ocs::tom::set<CE_relop>(ep2, 5);
   ocs::tom::set<CE_consumable>(ep2, 0);
   ocs::tom::set<CE_defaultval>(ep2, "15");
   ocs::tom::set<CE_requestable>(ep2, REQU_YES);
   ocs::tom::set<CE_urgency_weight>(ep2, "25");
   ocs::tom::set<CE_doubleval>(ep2, 6969.9696);
   lAppendElem(lp, ep2);

   ocs::tom::set<EH_resource_utilization>(ep, lp);
   
   lp = lCreateList("Load Values List", HL_Type);
   
   ep2 = lCreateElem(HL_Type);
   ocs::tom::set<HL_name>(ep2, "Test_Name8");
   ocs::tom::set<HL_value>(ep2, "1234.567");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(HL_Type);
   ocs::tom::set<HL_name>(ep2, "Test_Name9");
   ocs::tom::set<HL_value>(ep2, "6.7");
   lAppendElem(lp, ep2);
   
   ocs::tom::set<EH_load_list>(ep, lp);

   ocs::tom::set<EH_processors>(ep, 64);
   
   lp = lCreateList("Resched Unknown List", RU_Type);
   
   ep2 = lCreateElem(RU_Type);
   ocs::tom::set<RU_job_number>(ep2, 1);
   ocs::tom::set<RU_task_number>(ep2, 1);
   ocs::tom::set<RU_state>(ep2, 0);
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(RU_Type);
   ocs::tom::set<RU_job_number>(ep2, 1);
   ocs::tom::set<RU_task_number>(ep2, 2);
   ocs::tom::set<RU_state>(ep2, 1);
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(RU_Type);
   ocs::tom::set<RU_job_number>(ep2, 2);
   ocs::tom::set<RU_task_number>(ep2, 1);
   ocs::tom::set<RU_state>(ep2, 2);
   lAppendElem(lp, ep2);

   ocs::tom::set<EH_reschedule_unknown_list>(ep, lp);
   
   lp = lCreateList("User List", US_Type);
   
   ep2 = lCreateElem(US_Type);
   ocs::tom::set<US_name>(ep2, "Test_Name9");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(US_Type);
   ocs::tom::set<US_name>(ep2, "Test_Name10");
   lAppendElem(lp, ep2);
   
   ocs::tom::set<EH_acl>(ep, lp);
   
   lp = lCreateList("XUser List", US_Type);
   
   ep2 = lCreateElem(US_Type);
   ocs::tom::set<US_name>(ep2, "Test_Name11");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(US_Type);
   ocs::tom::set<US_name>(ep2, "Test_Name12");
   lAppendElem(lp, ep2);
   
   ocs::tom::set<EH_xacl>(ep, lp);
   
   lp = lCreateList("Projects List", PR_Type);
   
   ep2 = lCreateElem(PR_Type);
   ocs::tom::set<PR_name>(ep2, "Test_Name13");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(PR_Type);
   ocs::tom::set<PR_name>(ep2, "Test_Name14");
   lAppendElem(lp, ep2);
   
   ocs::tom::set<EH_prj>(ep, lp);
   
   lp = lCreateList("XProjects List", PR_Type);
   
   ep2 = lCreateElem(PR_Type);
   ocs::tom::set<PR_name>(ep2, "Test_Name15");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(PR_Type);
   ocs::tom::set<PR_name>(ep2, "Test_Name16");
   lAppendElem(lp, ep2);
   
   ocs::tom::set<EH_xprj>(ep, lp);
   
   lp = lCreateList("Usage Scaling List", HS_Type);
   
   ep2 = lCreateElem(HS_Type);
   ocs::tom::set<HS_name>(ep2, "Test_Name17");
   ocs::tom::set<HS_value>(ep2, 1234.567);
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(HS_Type);
   ocs::tom::set<HS_name>(ep2, "Test_Name18");
   ocs::tom::set<HS_value>(ep2, 6.7);
   lAppendElem(lp, ep2);
   
   ocs::tom::set<EH_usage_scaling_list>(ep, lp);
   
   lp = lCreateList("Report Vars List", STU_Type);
   
   ep2 = lCreateElem(STU_Type);
   ocs::tom::set<STU_name>(ep2, "Test_Name19");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(STU_Type);
   ocs::tom::set<STU_name>(ep2, "Test_Name20");
   lAppendElem(lp, ep2);
   
   ocs::tom::set<EH_report_variables>(ep, lp);

   /* the time at which the host added itself. The spool file carries the raw value, so that it
    * can be read back - the formatted date and time which qconf -se shows contains a blank,
    * which is the delimiter between the name of an attribute and its value in this format */
   ocs::tom::set<EH_auto_added_time>(ep, 1792143190772380);

   fields = sge_build_EH_field_list(true, false, false, false);

   /* Write a EH file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, ep, false,
                                       fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, false);
   
   /* Read a EH file using flatfile spooling */
   lFreeElem(&ep);
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
   lListElem *ep2 = nullptr;
   lListElem *ep3 = nullptr;
   lList *lp = nullptr;
   lList *lp2 = nullptr;
   lList *alp = nullptr;
   const char *file1 = nullptr, *file2 = nullptr;
   
   ep = lCreateElem(CQ_Type);
   
   ocs::tom::set<CQ_name>(ep, "Test_Name");
   
   ocs::tom::emplace_back_in<CQ_hostlist, HR_name>(ep, "Test_Name2");
   ocs::tom::emplace_back_in<CQ_hostlist, HR_name>(ep, "Test_Name3");
   ocs::tom::emplace_back_in<CQ_hostlist, HR_name>(ep, "Test_Name4");
   ocs::tom::emplace_back_in<CQ_hostlist, HR_name>(ep, "Test_Name5");
   ocs::tom::emplace_back_in<CQ_hostlist, HR_name>(ep, "Test_Name6");
   ocs::tom::emplace_back_in<CQ_hostlist, HR_name>(ep, "Test_Name7");

   lp = lCreateList("Seq No. List", AULNG_Type);
   
   ep2 = lCreateElem(AULNG_Type);
   ocs::tom::set<AULNG_href>(ep2, "Test_Name4");
   ocs::tom::set<AULNG_value>(ep2, 12);
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(AULNG_Type);
   ocs::tom::set<AULNG_href>(ep2, "Test_Name5");
   ocs::tom::set<AULNG_value>(ep2, 16);
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_seq_no>(ep, lp);

   lp = lCreateList("Load Thresholds", ACELIST_Type);
   
   ep2 = lCreateElem(ACELIST_Type);
   ocs::tom::set<ACELIST_href>(ep2, "Test_Name6");

   lp2 = lCreateList("Complex List", CE_Type);
   
   ep3 = lCreateElem(CE_Type);
   ocs::tom::set<CE_name>(ep3, "Test_Name7");
   ocs::tom::set<CE_stringval>(ep3, "stringval");
   lAppendElem(lp2, ep3);
   
   ocs::tom::set<ACELIST_value>(ep2, lp2);
   
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_load_thresholds>(ep, lp);
   
   lp = lCreateList("Suspend Thresholds", ACELIST_Type);

   ep2 = lCreateElem(ACELIST_Type);
   ocs::tom::set<ACELIST_href>(ep2, "Test_Name15");

   lp2 = lCreateList("Complex_List2", CE_Type);
   
   ep3 = lCreateElem(CE_Type);
   ocs::tom::set<CE_name>(ep3, "Test_Name16");
   ocs::tom::set<CE_stringval>(ep3, "stringval");
   lAppendElem(lp2, ep3);

   ocs::tom::set<ACELIST_value>(ep2, lp2);
   
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_suspend_thresholds>(ep, lp);
   
   lp = lCreateList("NSuspend List", AULNG_Type);
   
   ep2 = lCreateElem(AULNG_Type);
   ocs::tom::set<AULNG_href>(ep2, "Test_Name18");
   ocs::tom::set<AULNG_value>(ep2, 12);
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(AULNG_Type);
   ocs::tom::set<AULNG_href>(ep2, "Test_Name19");
   ocs::tom::set<AULNG_value>(ep2, 16);
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_nsuspend>(ep, lp);

   lp = lCreateList("Suspend Intervals", AINTER_Type);
   
   ep2 = lCreateElem(AINTER_Type);
   ocs::tom::set<AINTER_href>(ep2, "Test_Name20");
   ocs::tom::set<AINTER_value>(ep2, "1");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(AINTER_Type);
   ocs::tom::set<AINTER_href>(ep2, "Test_Name21");
   ocs::tom::set<AINTER_value>(ep2, "2");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_suspend_interval>(ep, lp);

   lp = lCreateList("Priority List", ASTR_Type);
   
   ep2 = lCreateElem(ASTR_Type);
   ocs::tom::set<ASTR_href>(ep2, "Test_Name22");
   ocs::tom::set<ASTR_value>(ep2, "3");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(ASTR_Type);
   ocs::tom::set<ASTR_href>(ep2, "Test_Name23");
   ocs::tom::set<ASTR_value>(ep2, "4");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_priority>(ep, lp);

   lp = lCreateList("Min CPU Intervals", AINTER_Type);
   
   ep2 = lCreateElem(AINTER_Type);
   ocs::tom::set<AINTER_href>(ep2, "Test_Name24");
   ocs::tom::set<AINTER_value>(ep2, "5");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(AINTER_Type);
   ocs::tom::set<AINTER_href>(ep2, "Test_Name25");
   ocs::tom::set<AINTER_value>(ep2, "6");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_min_cpu_interval>(ep, lp);

   lp = lCreateList("Processor List", ASTR_Type);
   
   ep2 = lCreateElem(ASTR_Type);
   ocs::tom::set<ASTR_href>(ep2, "Test_Name26");
   ocs::tom::set<ASTR_value>(ep2, "7");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(ASTR_Type);
   ocs::tom::set<ASTR_href>(ep2, "Test_Name27");
   ocs::tom::set<ASTR_value>(ep2, "8");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_processors>(ep, lp);

   lp = lCreateList("Q-Type List", AQTLIST_Type);
   
   ep2 = lCreateElem(AQTLIST_Type);
   ocs::tom::set<AQTLIST_href>(ep2, "Test_Name28");
   ocs::tom::set<AQTLIST_value>(ep2, 12);
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(AQTLIST_Type);
   ocs::tom::set<AQTLIST_href>(ep2, "Test_Name29");
   ocs::tom::set<AQTLIST_value>(ep2, 16);
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_qtype>(ep, lp);

   lp = lCreateList("Checkpoint List", ASTRLIST_Type);
   
   ep2 = lCreateElem(ASTRLIST_Type);
   ocs::tom::set<ASTRLIST_href>(ep2, "Test_Name30");
   
   lp2 = lCreateList("String List", ST_Type);
   
   ep3 = lCreateElem(ST_Type);
   ocs::tom::set<ST_name>(ep3, "Test_Name31");
   lAppendElem(lp2, ep3);
   
   ep3 = lCreateElem(ST_Type);
   ocs::tom::set<ST_name>(ep3, "Test_Name32");
   lAppendElem(lp2, ep3);
   
   ocs::tom::set<ASTRLIST_value>(ep2, lp2);
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_ckpt_list>(ep, lp);

   lp = lCreateList("PE List", ASTRLIST_Type);
   
   ep2 = lCreateElem(ASTRLIST_Type);
   ocs::tom::set<ASTRLIST_href>(ep2, "Test_Name36");
   
   lp2 = lCreateList("String List", ST_Type);
   
   ep3 = lCreateElem(ST_Type);
   ocs::tom::set<ST_name>(ep3, "Test_Name37");
   lAppendElem(lp2, ep3);

   ep3 = lCreateElem(ST_Type);
   ocs::tom::set<ST_name>(ep3, "Test_Name38");
   lAppendElem(lp2, ep3);
   
   ocs::tom::set<ASTRLIST_value>(ep2, lp2);
   lAppendElem(lp, ep2);
   
   ocs::tom::set<CQ_pe_list>(ep, lp);

   lp = lCreateList("Rerun List", ABOOL_Type);
   
   ep2 = lCreateElem(ABOOL_Type);
   ocs::tom::set<ABOOL_href>(ep2, "Test_Name42");
   ocs::tom::set<ABOOL_value>(ep2, true);
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(ABOOL_Type);
   ocs::tom::set<ABOOL_href>(ep2, "Test_Name43");
   ocs::tom::set<ABOOL_value>(ep2, false);
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_rerun>(ep, lp);

   lp = lCreateList("Job Slots", AULNG_Type);
   
   ep2 = lCreateElem(AULNG_Type);
   ocs::tom::set<AULNG_href>(ep2, "Test_Name44");
   ocs::tom::set<AULNG_value>(ep2, 12);
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(AULNG_Type);
   ocs::tom::set<AULNG_href>(ep2, "Test_Name45");
   ocs::tom::set<AULNG_value>(ep2, 16);
   lAppendElem(lp, ep2);

   ep2 = lCreateElem(AULNG_Type);
   ocs::tom::set<AULNG_href>(ep2, "Test_Name45_1");
   ocs::tom::set<AULNG_value>(ep2, 16);
   lAppendElem(lp, ep2);

   ep2 = lCreateElem(AULNG_Type);
   ocs::tom::set<AULNG_href>(ep2, "Test_Name45_2");
   ocs::tom::set<AULNG_value>(ep2, 16);
   lAppendElem(lp, ep2);

   ep2 = lCreateElem(AULNG_Type);
   ocs::tom::set<AULNG_href>(ep2, "Test_Name45_3");
   ocs::tom::set<AULNG_value>(ep2, 16);
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_job_slots>(ep, lp);

   lp = lCreateList("Tmp Dir List", ASTR_Type);
   
   ep2 = lCreateElem(ASTR_Type);
   ocs::tom::set<ASTR_href>(ep2, "Test_Name46");
   ocs::tom::set<ASTR_value>(ep2, "value");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(ASTR_Type);
   ocs::tom::set<ASTR_href>(ep2, "Test_Name47");
   ocs::tom::set<ASTR_value>(ep2, "value");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_tmpdir>(ep, lp);

   lp = lCreateList("Shell List", ASTR_Type);
   
   ep2 = lCreateElem(ASTR_Type);
   ocs::tom::set<ASTR_href>(ep2, "Test_Name48");
   ocs::tom::set<ASTR_value>(ep2, "value");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(ASTR_Type);
   ocs::tom::set<ASTR_href>(ep2, "Test_Name49");
   ocs::tom::set<ASTR_value>(ep2, "value");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_shell>(ep, lp);

   lp = lCreateList("Prolog List", ASTR_Type);
   
   ep2 = lCreateElem(ASTR_Type);
   ocs::tom::set<ASTR_href>(ep2, "Test_Name50");
   ocs::tom::set<ASTR_value>(ep2, "value");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(ASTR_Type);
   ocs::tom::set<ASTR_href>(ep2, "Test_Name51");
   ocs::tom::set<ASTR_value>(ep2, "value");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_prolog>(ep, lp);

   lp = lCreateList("Epilog List", ASTR_Type);
   
   ep2 = lCreateElem(ASTR_Type);
   ocs::tom::set<ASTR_href>(ep2, "Test_Name52");
   ocs::tom::set<ASTR_value>(ep2, "value");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(ASTR_Type);
   ocs::tom::set<ASTR_href>(ep2, "Test_Name53");
   ocs::tom::set<ASTR_value>(ep2, "value");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_epilog>(ep, lp);

   lp = lCreateList("Shell Start Modes", ASTR_Type);
   
   ep2 = lCreateElem(ASTR_Type);
   ocs::tom::set<ASTR_href>(ep2, "Test_Name54");
   ocs::tom::set<ASTR_value>(ep2, "value");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(ASTR_Type);
   ocs::tom::set<ASTR_href>(ep2, "Test_Name55");
   ocs::tom::set<ASTR_value>(ep2, "value");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_shell_start_mode>(ep, lp);

   lp = lCreateList("Shell Start Methods", ASTR_Type);
   
   ep2 = lCreateElem(ASTR_Type);
   ocs::tom::set<ASTR_href>(ep2, "Test_Name56");
   ocs::tom::set<ASTR_value>(ep2, "value");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(ASTR_Type);
   ocs::tom::set<ASTR_href>(ep2, "Test_Name57");
   ocs::tom::set<ASTR_value>(ep2, "value");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_starter_method>(ep, lp);

   lp = lCreateList("Shell Suspend Methods", ASTR_Type);
   
   ep2 = lCreateElem(ASTR_Type);
   ocs::tom::set<ASTR_href>(ep2, "Test_Name58");
   ocs::tom::set<ASTR_value>(ep2, "value");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(ASTR_Type);
   ocs::tom::set<ASTR_href>(ep2, "Test_Name59");
   ocs::tom::set<ASTR_value>(ep2, "value");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_suspend_method>(ep, lp);

   lp = lCreateList("Shell Resume Methods", ASTR_Type);
   
   ep2 = lCreateElem(ASTR_Type);
   ocs::tom::set<ASTR_href>(ep2, "Test_Name60");
   ocs::tom::set<ASTR_value>(ep2, "value");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(ASTR_Type);
   ocs::tom::set<ASTR_href>(ep2, "Test_Name61");
   ocs::tom::set<ASTR_value>(ep2, "value");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_resume_method>(ep, lp);

   lp = lCreateList("Shell Terminate Methods", ASTR_Type);
   
   ep2 = lCreateElem(ASTR_Type);
   ocs::tom::set<ASTR_href>(ep2, "Test_Name62");
   ocs::tom::set<ASTR_value>(ep2, "value");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(ASTR_Type);
   ocs::tom::set<ASTR_href>(ep2, "Test_Name63");
   ocs::tom::set<ASTR_value>(ep2, "value");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_terminate_method>(ep, lp);

   lp = lCreateList("Notify List", AINTER_Type);
   
   ep2 = lCreateElem(AINTER_Type);
   ocs::tom::set<AINTER_href>(ep2, "Test_Name64");
   ocs::tom::set<AINTER_value>(ep2, "46");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(AINTER_Type);
   ocs::tom::set<AINTER_href>(ep2, "Test_Name65");
   ocs::tom::set<AINTER_value>(ep2, "77");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_notify>(ep, lp);

   lp = lCreateList("Owner List", AUSRLIST_Type);
   
   ep2 = lCreateElem(AUSRLIST_Type);
   ocs::tom::set<AUSRLIST_href>(ep2, "Test_Name66");
   
   lp2 = lCreateList("Userset List", US_Type);
   
   ep3 = lCreateElem(US_Type);
   ocs::tom::set<US_name>(ep3, "Test_Name67");
   lAppendElem(lp2, ep3);
   
   ep3 = lCreateElem(US_Type);
   ocs::tom::set<US_name>(ep3, "Test_Name68");
   lAppendElem(lp2, ep3);
   
   ocs::tom::set<AUSRLIST_value>(ep2, lp2);
   lAppendElem(lp, ep2);
 
   ocs::tom::set<CQ_owner_list>(ep, lp);

   lp = lCreateList("ACL List", AUSRLIST_Type);
   
   ep2 = lCreateElem(AUSRLIST_Type);
   ocs::tom::set<AUSRLIST_href>(ep2, "Test_Name72");
   
   lp2 = lCreateList("Userset List", US_Type);
   
   ep3 = lCreateElem(US_Type);
   ocs::tom::set<US_name>(ep3, "Test_Name73");
   lAppendElem(lp2, ep3);
   
   ep3 = lCreateElem(US_Type);
   ocs::tom::set<US_name>(ep3, "Test_Name74");
   lAppendElem(lp2, ep3);
   
   ocs::tom::set<AUSRLIST_value>(ep2, lp2);
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_acl>(ep, lp);

   lp = lCreateList("XUser List", AUSRLIST_Type);
   
   ep2 = lCreateElem(AUSRLIST_Type);
   ocs::tom::set<AUSRLIST_href>(ep2, "Test_Name78");
   
   lp2 = lCreateList("Userset List", US_Type);
   
   ep3 = lCreateElem(US_Type);
   ocs::tom::set<US_name>(ep3, "Test_Name79");
   lAppendElem(lp2, ep3);
   
   ep3 = lCreateElem(US_Type);
   ocs::tom::set<US_name>(ep3, "Test_Name80");
   lAppendElem(lp2, ep3);
   
   ocs::tom::set<AUSRLIST_value>(ep2, lp2);
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_xacl>(ep, lp);

   lp = lCreateList("Subordinate List", ASOLIST_Type);
   
   ep2 = lCreateElem(ASOLIST_Type);
   ocs::tom::set<ASOLIST_href>(ep2, "Test_Name84");

   lp2 = lCreateList("Subordinate List", SO_Type);
   
   ep3 = lCreateElem(SO_Type);
   ocs::tom::set<SO_name>(ep3, "Test_Name85");
   ocs::tom::set<SO_threshold>(ep3, 13);
   lAppendElem(lp2, ep3);
   
   ep3 = lCreateElem(SO_Type);
   ocs::tom::set<SO_name>(ep3, "Test_Name86");
   ocs::tom::set<SO_threshold>(ep3, 16);
   lAppendElem(lp2, ep3);

   ocs::tom::set<ASOLIST_value>(ep2, lp2);
   
   lAppendElem(lp, ep2);
   
   ocs::tom::set<CQ_subordinate_list>(ep, lp);
  
   lp = lCreateList("Consumable Configs List", ACELIST_Type);
   
   ep2 = lCreateElem(ACELIST_Type);
   ocs::tom::set<ACELIST_href>(ep2, "Test_Name93");

   lp2 = lCreateList("Complex List", CE_Type);
   
   ep3 = lCreateElem(CE_Type);
   ocs::tom::set<CE_name>(ep3, "Test_Name94");
   ocs::tom::set<CE_stringval>(ep3, "stringval");
   lAppendElem(lp2, ep3);

   ocs::tom::set<ACELIST_value>(ep2, lp2);
   
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_consumable_config_list>(ep, lp);
   
   lp = lCreateList("Projects List", APRJLIST_Type);
   
   ep2 = lCreateElem(APRJLIST_Type);
   ocs::tom::set<APRJLIST_href>(ep2, "Test_Name96");
   
   lp2 = lCreateList("Project List", PR_Type);
   
   ep3 = lCreateElem(PR_Type);
   ocs::tom::set<PR_name>(ep3, "Test_Name97");
   lAppendElem(lp2, ep3);
   
   ep3 = lCreateElem(PR_Type);
   ocs::tom::set<PR_name>(ep3, "Test_Name98");
   lAppendElem(lp2, ep3);
   
   ocs::tom::set<APRJLIST_value>(ep2, lp2);
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_projects>(ep, lp);

   lp = lCreateList("XProjects List", APRJLIST_Type);
   
   ep2 = lCreateElem(APRJLIST_Type);
   ocs::tom::set<APRJLIST_href>(ep2, "Test_Name102");
   
   lp2 = lCreateList("Project List", PR_Type);
   
   ep3 = lCreateElem(PR_Type);
   ocs::tom::set<PR_name>(ep3, "Test_Name103");
   lAppendElem(lp2, ep3);
   
   ep3 = lCreateElem(PR_Type);
   ocs::tom::set<PR_name>(ep3, "Test_Name104");
   lAppendElem(lp2, ep3);
   
   ocs::tom::set<APRJLIST_value>(ep2, lp2);
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_xprojects>(ep, lp);

   lp = lCreateList("Calendar List", ASTR_Type);
   
   ep2 = lCreateElem(ASTR_Type);
   ocs::tom::set<ASTR_href>(ep2, "Test_Name108");
   ocs::tom::set<ASTR_value>(ep2, "value");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(ASTR_Type);
   ocs::tom::set<ASTR_href>(ep2, "Test_Name109");
   ocs::tom::set<ASTR_value>(ep2, "value");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_calendar>(ep, lp);

   lp = lCreateList("Initial State List", ASTR_Type);
   
   ep2 = lCreateElem(ASTR_Type);
   ocs::tom::set<ASTR_href>(ep2, "Test_Name110");
   ocs::tom::set<ASTR_value>(ep2, "value");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(ASTR_Type);
   ocs::tom::set<ASTR_href>(ep2, "Test_Name111");
   ocs::tom::set<ASTR_value>(ep2, "value");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_initial_state>(ep, lp);

   lp = lCreateList("s_rt List", ATIME_Type);
   
   ep2 = lCreateElem(ATIME_Type);
   ocs::tom::set<ATIME_href>(ep2, "Test_Name112");
   ocs::tom::set<ATIME_value>(ep2, "00:00:00");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(ATIME_Type);
   ocs::tom::set<ATIME_href>(ep2, "Test_Name113");
   ocs::tom::set<ATIME_value>(ep2, "23:59:59");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_s_rt>(ep, lp);

   lp = lCreateList("h_rt List", ATIME_Type);
   
   ep2 = lCreateElem(ATIME_Type);
   ocs::tom::set<ATIME_href>(ep2, "Test_Name114");
   ocs::tom::set<ATIME_value>(ep2, "00:00:00");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(ATIME_Type);
   ocs::tom::set<ATIME_href>(ep2, "Test_Name115");
   ocs::tom::set<ATIME_value>(ep2, "23:59:59");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_h_rt>(ep, lp);

   lp = lCreateList("s_cpu List", ATIME_Type);
   
   ep2 = lCreateElem(ATIME_Type);
   ocs::tom::set<ATIME_href>(ep2, "Test_Name116");
   ocs::tom::set<ATIME_value>(ep2, "00:00:00");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(ATIME_Type);
   ocs::tom::set<ATIME_href>(ep2, "Test_Name117");
   ocs::tom::set<ATIME_value>(ep2, "23:59:59");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_s_cpu>(ep, lp);

   lp = lCreateList("h_cpu List", ATIME_Type);
   
   ep2 = lCreateElem(ATIME_Type);
   ocs::tom::set<ATIME_href>(ep2, "Test_Name118");
   ocs::tom::set<ATIME_value>(ep2, "00:00:00");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(ATIME_Type);
   ocs::tom::set<ATIME_href>(ep2, "Test_Name119");
   ocs::tom::set<ATIME_value>(ep2, "23:59:59");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_h_cpu>(ep, lp);

   lp = lCreateList("s_fsize List", AMEM_Type);
   
   ep2 = lCreateElem(AMEM_Type);
   ocs::tom::set<AMEM_href>(ep2, "Test_Name120");
   ocs::tom::set<AMEM_value>(ep2, "1048576");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(AMEM_Type);
   ocs::tom::set<AMEM_href>(ep2, "Test_Name121");
   ocs::tom::set<AMEM_value>(ep2, "1024");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_s_fsize>(ep, lp);

   lp = lCreateList("h_fsize List", AMEM_Type);
   
   ep2 = lCreateElem(AMEM_Type);
   ocs::tom::set<AMEM_href>(ep2, "Test_Name122");
   ocs::tom::set<AMEM_value>(ep2, "1048576");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(AMEM_Type);
   ocs::tom::set<AMEM_href>(ep2, "Test_Name123");
   ocs::tom::set<AMEM_value>(ep2, "1024");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_h_fsize>(ep, lp);

   lp = lCreateList("s_data List", AMEM_Type);
   
   ep2 = lCreateElem(AMEM_Type);
   ocs::tom::set<AMEM_href>(ep2, "Test_Name124");
   ocs::tom::set<AMEM_value>(ep2, "1048576");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(AMEM_Type);
   ocs::tom::set<AMEM_href>(ep2, "Test_Name125");
   ocs::tom::set<AMEM_value>(ep2, "1024");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_s_data>(ep, lp);

   lp = lCreateList("h_data List", AMEM_Type);
   
   ep2 = lCreateElem(AMEM_Type);
   ocs::tom::set<AMEM_href>(ep2, "Test_Name126");
   ocs::tom::set<AMEM_value>(ep2, "1048576");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(AMEM_Type);
   ocs::tom::set<AMEM_href>(ep2, "Test_Name127");
   ocs::tom::set<AMEM_value>(ep2, "1024");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_h_data>(ep, lp);

   lp = lCreateList("s_stack List", AMEM_Type);
   
   ep2 = lCreateElem(AMEM_Type);
   ocs::tom::set<AMEM_href>(ep2, "Test_Name128");
   ocs::tom::set<AMEM_value>(ep2, "1048576");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(AMEM_Type);
   ocs::tom::set<AMEM_href>(ep2, "Test_Name129");
   ocs::tom::set<AMEM_value>(ep2, "1024");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_s_stack>(ep, lp);

   lp = lCreateList("h_stack List", AMEM_Type);
   
   ep2 = lCreateElem(AMEM_Type);
   ocs::tom::set<AMEM_href>(ep2, "Test_Name130");
   ocs::tom::set<AMEM_value>(ep2, "1048576");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(AMEM_Type);
   ocs::tom::set<AMEM_href>(ep2, "Test_Name131");
   ocs::tom::set<AMEM_value>(ep2, "1024");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_h_stack>(ep, lp);

   lp = lCreateList("s_core List", AMEM_Type);
   
   ep2 = lCreateElem(AMEM_Type);
   ocs::tom::set<AMEM_href>(ep2, "Test_Name132");
   ocs::tom::set<AMEM_value>(ep2, "1048576");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(AMEM_Type);
   ocs::tom::set<AMEM_href>(ep2, "Test_Name133");
   ocs::tom::set<AMEM_value>(ep2, "1024");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_s_core>(ep, lp);

   lp = lCreateList("h_core List", AMEM_Type);
   
   ep2 = lCreateElem(AMEM_Type);
   ocs::tom::set<AMEM_href>(ep2, "Test_Name134");
   ocs::tom::set<AMEM_value>(ep2, "1048576");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(AMEM_Type);
   ocs::tom::set<AMEM_href>(ep2, "Test_Name135");
   ocs::tom::set<AMEM_value>(ep2, "1024");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_h_core>(ep, lp);

   lp = lCreateList("s_rss List", AMEM_Type);
   
   ep2 = lCreateElem(AMEM_Type);
   ocs::tom::set<AMEM_href>(ep2, "Test_Name136");
   ocs::tom::set<AMEM_value>(ep2, "1048576");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(AMEM_Type);
   ocs::tom::set<AMEM_href>(ep2, "Test_Name137");
   ocs::tom::set<AMEM_value>(ep2, "1024");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_s_rss>(ep, lp);

   lp = lCreateList("h_rss List", AMEM_Type);
   
   ep2 = lCreateElem(AMEM_Type);
   ocs::tom::set<AMEM_href>(ep2, "Test_Name138");
   ocs::tom::set<AMEM_value>(ep2, "1048576");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(AMEM_Type);
   ocs::tom::set<AMEM_href>(ep2, "Test_Name139");
   ocs::tom::set<AMEM_value>(ep2, "1024");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_h_rss>(ep, lp);

   lp = lCreateList("s_vmem List", AMEM_Type);
   
   ep2 = lCreateElem(AMEM_Type);
   ocs::tom::set<AMEM_href>(ep2, "Test_Name140");
   ocs::tom::set<AMEM_value>(ep2, "1048576");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(AMEM_Type);
   ocs::tom::set<AMEM_href>(ep2, "Test_Name141");
   ocs::tom::set<AMEM_value>(ep2, "1024");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_s_vmem>(ep, lp);

   lp = lCreateList("h_vmem List", AMEM_Type);
   
   ep2 = lCreateElem(AMEM_Type);
   ocs::tom::set<AMEM_href>(ep2, "Test_Name142");
   ocs::tom::set<AMEM_value>(ep2, "1048576");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(AMEM_Type);
   ocs::tom::set<AMEM_href>(ep2, "Test_Name143");
   ocs::tom::set<AMEM_value>(ep2, "1024");
   lAppendElem(lp, ep2);

   ocs::tom::set<CQ_h_vmem>(ep, lp);

   /* Write a CQ file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, ep, false,
                                       CQ_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, false);
   
   /* Read a CQ file using flatfile spooling */
   lFreeElem(&ep);
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
   lListElem *ep2 = nullptr;
   lList *lp = nullptr;
   lList *alp = nullptr;
   const char *file1 = nullptr, *file2 = nullptr;
   
   ep = lCreateElem(SC_Type);
   ocs::tom::set<SC_algorithm>(ep, "algorithm");
   ocs::tom::set<SC_schedule_interval>(ep, "30");
   ocs::tom::set<SC_maxujobs>(ep, 1024);
   ocs::tom::set<SC_queue_sort_method>(ep, 1);

   lp = lCreateList("Job Load Adjustments", CE_Type);

   ep2 = lCreateElem(CE_Type);
   ocs::tom::set<CE_name>(ep2, "Test_Name1");
   ocs::tom::set<CE_shortcut>(ep2, "tn1");
   ocs::tom::set<CE_valtype>(ep2, 1);
   ocs::tom::set<CE_relop>(ep2, 5);
   ocs::tom::set<CE_consumable>(ep2, 1);
   ocs::tom::set<CE_defaultval>(ep2, "15");
   ocs::tom::set<CE_requestable>(ep2, REQU_NO);
   ocs::tom::set<CE_urgency_weight>(ep2, "25");
   ocs::tom::set<CE_stringval>(ep2, "stringval");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(CE_Type);
   ocs::tom::set<CE_name>(ep2, "Test_Name2");
   ocs::tom::set<CE_shortcut>(ep2, "tn2");
   ocs::tom::set<CE_valtype>(ep2, 1);
   ocs::tom::set<CE_relop>(ep2, 5);
   ocs::tom::set<CE_consumable>(ep2, 0);
   ocs::tom::set<CE_defaultval>(ep2, "15");
   ocs::tom::set<CE_requestable>(ep2, REQU_YES);
   ocs::tom::set<CE_urgency_weight>(ep2, "25");
   ocs::tom::set<CE_stringval>(ep2, "stringval");
   lAppendElem(lp, ep2);

   ocs::tom::set<SC_job_load_adjustments>(ep, lp);
   ocs::tom::set<SC_load_adjustment_decay_time>(ep, "45");
   ocs::tom::set<SC_load_formula>(ep, "load_formula");
   ocs::tom::set<SC_schedd_job_info>(ep, "schedd_job_info");
   ocs::tom::set<SC_flush_submit_sec>(ep, 128);
   ocs::tom::set<SC_flush_finish_sec>(ep, 512);
   ocs::tom::set<SC_params>(ep, "params");
   ocs::tom::set<SC_reprioritize_interval>(ep, "15");
   ocs::tom::set<SC_halftime>(ep, 32);
   
   lp = lCreateList("Usage Weight List", UA_Type);
   
   ep2 = lCreateElem(UA_Type);
   ocs::tom::set<UA_name>(ep2, "Test_Name3");
   ocs::tom::set<UA_value>(ep2, 11.22);
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(UA_Type);
   ocs::tom::set<UA_name>(ep2, "Test_Name4");
   ocs::tom::set<UA_value>(ep2, 33.44);
   lAppendElem(lp, ep2);
      
   ocs::tom::set<SC_usage_weight_list>(ep, lp);
   
   ocs::tom::set<SC_compensation_factor>(ep, 11.22);
   ocs::tom::set<SC_weight_user>(ep, 11.22);
   ocs::tom::set<SC_weight_project>(ep, 33.44);
   ocs::tom::set<SC_weight_department>(ep, 66.77);
   ocs::tom::set<SC_weight_job>(ep, 88.99);
   ocs::tom::set<SC_weight_tickets_functional>(ep, 36);
   ocs::tom::set<SC_weight_tickets_share>(ep, 18);
   ocs::tom::set<SC_share_override_tickets>(ep, true);
   ocs::tom::set<SC_share_functional_shares>(ep, true);
   ocs::tom::set<SC_max_functional_jobs_to_schedule>(ep, 2048);
   ocs::tom::set<SC_report_pjob_tickets>(ep, true);
   ocs::tom::set<SC_max_pending_tasks_per_job>(ep, 256);
   ocs::tom::set<SC_halflife_decay_list>(ep, "halflife_decay_list");
   ocs::tom::set<SC_policy_hierarchy>(ep, "policy_hierarchy");
   ocs::tom::set<SC_weight_ticket>(ep, 11.99);
   ocs::tom::set<SC_weight_waiting_time>(ep, 22.88);
   ocs::tom::set<SC_weight_deadline>(ep, 33.77);
   ocs::tom::set<SC_weight_urgency>(ep, 44.66);
   ocs::tom::set<SC_weight_priority>(ep, 55.99);
   ocs::tom::set<SC_max_reservation>(ep, 33.00);
   ocs::tom::set<SC_default_duration>(ep, "default_duration");

   /* Write a SC file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, ep, false,
                                       SC_fields,
                                       &qconf_comma_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, false);
   
   /* Read a SC file using flatfile spooling */
   lFreeElem(&ep);
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
   
   ep = lCreateElem(QU_Type);
   ocs::tom::set<QU_qhostname>(ep, "Test_Name");
   ocs::tom::set<QU_qname>(ep, "Test_Name2");
   ocs::tom::set<QU_state>(ep, 1);
   ocs::tom::set<QU_pending_signal>(ep, 2);
   ocs::tom::set<QU_pending_signal_delivery_time>(ep, 3000000);
   ocs::tom::set<QU_version>(ep, 4);


   fields = sge_build_QU_field_list(false, true);
   
   /* Write a QU file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, ep, false,
                                       fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, false);
   
   /* Read a QU file using flatfile spooling */
   lFreeElem(&ep);
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
   
   ep = lCreateElem(HGRP_Type);
   ocs::tom::set<HGRP_name>(ep, "Test_Name");
  
   ocs::tom::emplace_back_in<HGRP_host_list, HR_name>(ep, "Test_Name2");
   ocs::tom::emplace_back_in<HGRP_host_list, HR_name>(ep, "Test_Name3");
   ocs::tom::emplace_back_in<HGRP_host_list, HR_name>(ep, "Test_Name4");
   
   
   /* Write a HGRP file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, ep, false,
                                       HGRP_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, false);
   
   /* Read a HGRP file using flatfile spooling */
   lFreeElem(&ep);
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
   lListElem *ep2 = nullptr;
   lList * lp = nullptr;
   lList * alp = nullptr;
   spooling_field *fields;
   const char *file1 = nullptr;
   const char *file2 = nullptr;
   
   ep = lCreateElem(CONF_Type);
   ocs::tom::set<CONF_version>(ep, 101);
   ocs::tom::set<CONF_name>(ep, "host1");
   
   lp = lCreateList("Config List", CF_Type);
   
   ep2 = lCreateElem(CF_Type);
   ocs::tom::set<CF_name>(ep2, "gid_range");
   ocs::tom::set<CF_value>(ep2, "1000-1100");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(CF_Type);
   ocs::tom::set<CF_name>(ep2, "max_aj_tasks");
   ocs::tom::set<CF_value>(ep2, "1001");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(CF_Type);
   ocs::tom::set<CF_name>(ep2, "admin_user");
   ocs::tom::set<CF_value>(ep2, "testuser");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(CF_Type);
   ocs::tom::set<CF_name>(ep2, "user_lists");
   ocs::tom::set<CF_value>(ep2, "me you someone_else");
   lAppendElem(lp, ep2);
   
   ep2 = lCreateElem(CF_Type);
   ocs::tom::set<CF_name>(ep2, "login_shells");
   ocs::tom::set<CF_value>(ep2, "sh,ksh,csh,tcsh");
   lAppendElem(lp, ep2);

   ep2 = lCreateElem(CF_Type);
   ocs::tom::set<CF_name>(ep2, "execd_params");
   ocs::tom::set<CF_value>(ep2, "PTF_MIN_PRIORITY=20,PTF_MAX_PRIORITY=0,SET_LIB_PATH=true");
   lAppendElem(lp, ep2);

   ep2 = lCreateElem(CF_Type);
   ocs::tom::set<CF_name>(ep2, "reporting_params");
   ocs::tom::set<CF_value>(ep2, "accounting=true reporting=false flush_time=00:00:05 joblog=true sharelog=00:10:00=true");
   lAppendElem(lp, ep2);
   
   ocs::tom::set<CONF_entries>(ep, lp);
   

   fields = sge_build_CONF_field_list(true);
   
   /* Write a CONF file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, ep, false,
                                       fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, true);

   
   /* Read a CONF file using flatfile spooling */
   lFreeElem(&ep);
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
   lListElem *ep = nullptr;
   lListElem *ep2 = nullptr;
   lListElem *ep3 = nullptr;
   lListElem *limit = nullptr;
   lList *lp1 = nullptr;
   lList *limit_list= nullptr;
   lList *alp = nullptr;
   const char *file1 = nullptr;
   const char *file2 = nullptr;
   lList* rqs_list = lCreateList("test", RQS_Type);

   ep = lCreateElem(RQS_Type);
   ocs::tom::set<RQS_name>(ep, "Test_Name1");
   ocs::tom::set<RQS_description>(ep, "Test Description");
   ocs::tom::set<RQS_enabled>(ep, false);
   lp1 = lCreateList("Rule_List", RQR_Type);
   /* rule 1 */
   ep2 = lCreateElem(RQR_Type);
      ep3 = lCreateElem(RQRF_Type);
      ocs::tom::set<RQRF_expand>(ep3, true);
      ocs::tom::emplace_back_in<RQRF_scope, ST_name>(ep3, "Test_User1");

      ocs::tom::emplace_back_in<RQRF_xscope, ST_name>(ep3, "Test_User2");
      ocs::tom::emplace_back_in<RQRF_xscope, ST_name>(ep3, "Test_User3");
      ocs::tom::set<RQR_filter_users>(ep2, ep3);
      
      ep3 = lCreateElem(RQRF_Type);
      ocs::tom::set<RQRF_expand>(ep3, false);
      ocs::tom::emplace_back_in<RQRF_scope, ST_name>(ep3, "Test_Host1");
      ocs::tom::emplace_back_in<RQRF_xscope, ST_name>(ep3, "Test_Host2");
      ocs::tom::emplace_back_in<RQRF_xscope, ST_name>(ep3, "Test_Host3");
      ocs::tom::set<RQR_filter_hosts>(ep2, ep3);

      ep3 = lCreateElem(RQRF_Type);
      ocs::tom::set<RQRF_expand>(ep3, false);
      ocs::tom::emplace_back_in<RQRF_scope, ST_name>(ep3, "Test_PE1");
      ocs::tom::emplace_back_in<RQRF_xscope, ST_name>(ep3, "Test_PE2");
      ocs::tom::emplace_back_in<RQRF_xscope, ST_name>(ep3, "Test_PE3");
      ocs::tom::set<RQR_filter_pes>(ep2, ep3);


      limit_list = lCreateList("Limit_List", RQRL_Type);
      limit = lCreateElem(RQRL_Type);
      ocs::tom::set<RQRL_name>(limit, "slots");
      ocs::tom::set<RQRL_value>(limit, "2*$num_proc");
      lAppendElem(limit_list, limit);
      ocs::tom::set<RQR_limit>(ep2, limit_list);
   lAppendElem(lp1, ep2);
   /* rule 2 */
   ep2 = lCreateElem(RQR_Type);
      ep3 = lCreateElem(RQRF_Type);
      ocs::tom::set<RQRF_expand>(ep3, true);
      ocs::tom::emplace_back_in<RQRF_scope, ST_name>(ep3, "Test_Queue1");

      ocs::tom::emplace_back_in<RQRF_xscope, ST_name>(ep3, "Test_Queue2");
      ocs::tom::set<RQR_filter_queues>(ep2, ep3);

      limit_list = lCreateList("Limit_List", RQRL_Type);
      limit = lCreateElem(RQRL_Type);
      ocs::tom::set<RQRL_name>(limit, "arch");
      ocs::tom::set<RQRL_value>(limit, "lx-amd64");
      lAppendElem(limit_list, limit);
      ocs::tom::set<RQR_limit>(ep2, limit_list);
   lAppendElem(lp1, ep2);
   /* rule 3 */
   ep2 = lCreateElem(RQR_Type);
   ocs::tom::set<RQR_name>(ep2, "rule3");
      ep3 = lCreateElem(RQRF_Type);
      ocs::tom::set<RQRF_expand>(ep3, true);
      ocs::tom::emplace_back_in<RQRF_scope, ST_name>(ep3, "Test_Pe1");

      ocs::tom::emplace_back_in<RQRF_xscope, ST_name>(ep3, "Test_Pe2");
      ocs::tom::set<RQR_filter_pes>(ep2, ep3);

      limit_list = lCreateList("Limit_List", RQRL_Type);
      limit = lCreateElem(RQRL_Type);
      ocs::tom::set<RQRL_name>(limit, "mem");
      ocs::tom::set<RQRL_value>(limit, "1G");
      lAppendElem(limit_list, limit);
      ocs::tom::set<RQR_limit>(ep2, limit_list);
   lAppendElem(lp1, ep2);
   ocs::tom::set<RQS_rule>(ep, lp1);
   lAppendElem(rqs_list, ep);

   /* rule 4 */
   ep = lCreateElem(RQS_Type);
   ocs::tom::set<RQS_name>(ep, "Test_Name2");
   ocs::tom::set<RQS_description>(ep, "Test Description");
   ocs::tom::set<RQS_enabled>(ep, true);
   lp1 = lCreateList("Rule_List", RQR_Type);
   ep2 = lCreateElem(RQR_Type);
      ep3 = lCreateElem(RQRF_Type);
      ocs::tom::set<RQRF_expand>(ep3, false);
      ocs::tom::emplace_back_in<RQRF_scope, ST_name>(ep3, "roland");

      ocs::tom::emplace_back_in<RQRF_xscope, ST_name>(ep3, "andre");
      ocs::tom::set<RQR_filter_users>(ep2, ep3);

      limit_list = lCreateList("Limit_List", RQRL_Type);
      /* first limit */
      limit = lCreateElem(RQRL_Type);
      ocs::tom::set<RQRL_name>(limit, "mem");
      ocs::tom::set<RQRL_value>(limit, "10G");
      lAppendElem(limit_list, limit);
      /* second limit */
      limit = lCreateElem(RQRL_Type);
      ocs::tom::set<RQRL_name>(limit, "arch");
      ocs::tom::set<RQRL_value>(limit, "sol-sparc64");
      lAppendElem(limit_list, limit);

      ocs::tom::set<RQR_limit>(ep2, limit_list);
   lAppendElem(lp1, ep2);
   ocs::tom::set<RQS_rule>(ep, lp1);
   lAppendElem(rqs_list, ep);


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
   lList *lp;
   lListElem *ep = nullptr;
   lListElem *ep1;
   lList *alp = nullptr;
   const char *file1 = nullptr, *file2 = nullptr;
   
   ep = lCreateElem(AR_Type);
   ocs::tom::set<AR_id>(ep, 1);
   ocs::tom::set<AR_name>(ep, "Test_Name");

   lp = lCreateList("range_list", RN_Type);
   ep1 = lCreateElem(RN_Type);
   ocs::tom::set<RN_min>(ep1, 1);
   ocs::tom::set<RN_max>(ep1, 2);
   ocs::tom::set<RN_step>(ep1, 3);
   lAppendElem(lp, ep1);
   ep1 = lCreateElem(RN_Type);
   ocs::tom::set<RN_min>(ep1, 4);
   ocs::tom::set<RN_max>(ep1, 5);
   ocs::tom::set<RN_step>(ep1, 6);
   lAppendElem(lp, ep1);
   ocs::tom::set<AR_pe_range>(ep, lp);

   lp = lCreateList("resource_list", CE_Type);
   ep1 = lCreateElem(CE_Type);
   ocs::tom::set<CE_name>(ep1, "ce1");
   ocs::tom::set<CE_stringval>(ep1, "101");
   lAppendElem(lp, ep1);
   ep1 = lCreateElem(CE_Type);
   ocs::tom::set<CE_name>(ep1, "ce2");
   ocs::tom::set<CE_doubleval>(ep1, 102);
   lAppendElem(lp, ep1);
   ocs::tom::set<AR_resource_list>(ep, lp);

   ocs::tom::emplace_back_in<AR_queue_list, QR_name>(ep, "queue1.q");
   ocs::tom::emplace_back_in<AR_queue_list, QR_name>(ep, "queue2.q");
   ocs::tom::emplace_back_in<AR_queue_list, QR_name>(ep, "queue3.q");
   ocs::tom::emplace_back_in<AR_queue_list, QR_name>(ep, "queue4.q");

   ocs::tom::Element<ocs::tom::JG> ep1_jg = ocs::tom::emplace_back_in<AR_granted_slots, JG_qname>(ep, "all.q@bla");
   ocs::tom::set<JG_slots>(ep1_jg, 5);
   ocs::tom::Element<ocs::tom::JG> ep1_jg_2 = ocs::tom::emplace_back_in<AR_granted_slots, JG_qname>(ep, "my.q@bla");
   ocs::tom::set<JG_slots>(ep1_jg_2, 10);

   ocs::tom::emplace_back_in<AR_mail_list, MR_user>(ep, "user1");
   ocs::tom::Element<ocs::tom::MR> ep1_mr = ocs::tom::emplace_back_in<AR_mail_list, MR_user>(ep, "user2");
   ocs::tom::set<MR_host>(ep1_mr, "sun.com");

   ocs::tom::emplace_back_in<AR_master_queue_list, QR_name>(ep, "queue1.q");
   ocs::tom::emplace_back_in<AR_master_queue_list, QR_name>(ep, "queue2.q");
   ocs::tom::emplace_back_in<AR_master_queue_list, QR_name>(ep, "queue3.q");
   ocs::tom::emplace_back_in<AR_master_queue_list, QR_name>(ep, "queue4.q");

   ocs::tom::Element<ocs::tom::ARA> ep1_ara = ocs::tom::emplace_back_in<AR_acl_list, ARA_name>(ep, "user1");
   ocs::tom::set<ARA_group>(ep1_ara, "group1");
   ocs::tom::Element<ocs::tom::ARA> ep1_ara_2 = ocs::tom::emplace_back_in<AR_acl_list, ARA_name>(ep, "user2");
   ocs::tom::set<ARA_group>(ep1_ara_2, "group2");
   ocs::tom::emplace_back_in<AR_acl_list, ARA_name>(ep, "@userset1");
   ocs::tom::emplace_back_in<AR_acl_list, ARA_name>(ep, "@userset2");

  
   
   /* Write a AR file using classic spooling */
   file1 = spool_flatfile_write_object(&alp, ep, false,
                                       AR_fields,
                                       &qconf_sfi,
                                       SP_DEST_TMP,
                                       SP_FORM_ASCII, 
                                       file1, true);

   /* Read a AR file using flatfile spooling */
   lFreeElem(&ep);
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
   lListElem *ep = lCreateElem(PE_Type);
   ocs::tom::set<PE_name>(ep, "perm_pe");

   char path[1024];
   snprintf(path, sizeof(path), "/tmp/test_spool_perm.%ld", (long)getpid());
   unlink(path);

   // Neutralise the ambient umask so the resulting mode is exactly what the
   // code requested (otherwise it would be mode & ~umask).
   mode_t old_umask = umask(0);
   const char *result = spool_flatfile_write_object(&alp, ep, false,
                                                    PE_fields, &qconf_sfi,
                                                    SP_DEST_SPOOL, SP_FORM_ASCII,
                                                    path, false);
   umask(old_umask);
   lFreeElem(&ep);

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

   lListElem *rule = lCreateElem(SPR_Type);
   ocs::tom::set<SPR_url>(rule, base);

   // Neutralise the ambient umask so the resulting mode is exactly what the
   // code requested (otherwise it would be mode & ~umask).
   mode_t old_umask = umask(0);
   bool ok = spool_classic_default_startup_func(&alp, rule, false);
   umask(old_umask);
   lFreeElem(&rule);

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
   lList *lp = lCreateList("User_List", US_Type);
   for (int i = 0; i < 10; i++) {
      char name[32];
      snprintf(name, sizeof(name), "user%02d", i);
      lListElem *ep2 = lCreateElem(US_Type);
      ocs::tom::set<US_name>(ep2, name);
      ocs::tom::set<US_fshare>(ep2, i * 100);
      lAppendElem(lp, ep2);
   }
   ocs::tom::set<PE_user_list>(ep, lp);

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
