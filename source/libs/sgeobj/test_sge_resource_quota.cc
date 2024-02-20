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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "uti/sge_bootstrap.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_href.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_resource_quota.h"

typedef struct {
   const char* users;
   const char* group;
   const char* projects;
   const char* pes;
   const char* hosts;
   const char* queues;
} filter_t;

typedef struct {
   filter_t rule;
   filter_t query;
   bool last;
} filter_test_t;

int main(int argc, char *argv[])
{
   int pos_tests_failed = 0;
   int neg_tests_failed = 0;
   int i = 0;
   lListElem *filter;

   filter_test_t positiv_test[] = {
   /* simple search */
      {{"user1,user2,user3", nullptr, nullptr, nullptr, nullptr, nullptr}, {"user3", "staff", "*", "*", "*", "*"}, false},
      {{nullptr, nullptr, "project1,project2,project3", nullptr, nullptr, nullptr}, {"*", "staff", "project2", "*", "*", "*"}, false},
      {{nullptr, nullptr, nullptr, "pe1,pe2,pe3", nullptr, nullptr}, {"*", "staff", "*", "pe3", "*", "*"}, false},
      {{nullptr, nullptr, nullptr, nullptr, "h1,h2,h3", nullptr}, {"*", "staff", "*", "*", "h3", "*"}, false},
      {{nullptr, nullptr, nullptr, nullptr, nullptr, "queue1,queue2,queue3"}, {"*", "staff", "*", "*", "*", "queue1"}, false},
   /* wildcard search */
      {{"user1,user2,user3", nullptr, nullptr, nullptr, nullptr, nullptr}, {"user*", "*", "staff", "*", "*", "*"}, false},
      {{nullptr, nullptr, "project1,project2,project3", nullptr, nullptr, nullptr}, {"*", "staff", "project*", "*", "*", "*"}, false},
      {{nullptr, nullptr, nullptr, "pe1,pe2,pe3", nullptr, nullptr}, {"*", "staff", "*", "pe*", "*", "*"}, false},
      {{nullptr, nullptr, nullptr, nullptr, "h1,h2,h3", nullptr}, {"*", "staff", "*", "*", "h*", "*"}, false},
      {{nullptr, nullptr, nullptr, nullptr, nullptr, "queue1,queue2,queue3"}, {"*", "staff", "*", "*", "*", "que*"}, false},
   /* wildcard definition */
      {{"user*", nullptr, nullptr, nullptr, nullptr, nullptr}, {"user3", "staff", "*", "*", "*", "*"}, false},
      {{nullptr, nullptr, "project*", nullptr, nullptr, nullptr}, {"*", "staff", "project2", "*", "*", "*"}, false},
      {{nullptr, nullptr, nullptr, "pe*", nullptr, nullptr}, {"*", "staff", "*", "pe3", "*", "*"}, false},
      {{nullptr, nullptr, nullptr, nullptr, "h*", nullptr}, {"*", "staff", "*", "*", "h1", "*"}, false},
      {{nullptr, nullptr, nullptr, nullptr, nullptr, "queue*"}, {"*", "staff", "*", "*", "*", "queue1"}, false},
   /* wildcard definition, wildcard search */
      {{"user*", nullptr, nullptr, nullptr, nullptr, nullptr}, {"u*", "staff", "*", "*", "*", "*"}, false},
      {{nullptr, nullptr, "project*", nullptr, nullptr, nullptr}, {"*", "staff", "pro*", "*", "*", "*"}, false},
      {{nullptr, nullptr, nullptr, "pe*", nullptr, nullptr}, {"*", "staff", "*", "p*", "*", "*"}, false},
      {{nullptr, nullptr, nullptr, nullptr, "host*", nullptr}, {"*", "staff", "*", "*", "h*", "*"}, false},
      {{nullptr, nullptr, nullptr, nullptr, nullptr, "queue*"}, {"*", "staff", "*", "*", "*", "qu*"}, false},
   /* hostgroup definition*/
      {{nullptr, nullptr, nullptr, nullptr, "@hgrp1", nullptr}, {"*", "staff", "*", "*", "host1", "*"}, false},
      {{nullptr, nullptr, nullptr, nullptr, "@hgrp1", nullptr}, {"*", "staff", "*", "*", "ho*", "*"}, false},
      {{nullptr, nullptr, nullptr, nullptr, "@hgr*", nullptr}, {"*", "staff", "*", "*", "host1", "*"}, false},
      {{nullptr, nullptr, nullptr, nullptr, "@hgr*", nullptr}, {"*", "staff", "*", "*", "hos*", "*"}, false},
      {{nullptr, nullptr, nullptr, nullptr, "@hgrp1", nullptr}, {"*", "staff", "*", "*", "@hgrp1", "*"}, false},
      {{nullptr, nullptr, nullptr, nullptr, "host1", nullptr}, {"*", "staff", "*", "*", "@hgrp1", "*"}, false},
      {{nullptr, nullptr, nullptr, nullptr, "ho*", nullptr}, {"*", "staff", "*", "*", "@hgrp1", "*"}, false},
      {{nullptr, nullptr, nullptr, nullptr, "host1", nullptr}, {"*", "staff", "*", "*", "@hgrp*", "*"}, false},
      {{nullptr, nullptr, nullptr, nullptr, "ho*", nullptr}, {"*", "staff", "*", "*", "@hgrp*", "*"}, false},
      {{nullptr, nullptr, nullptr, nullptr, "ho*", nullptr}, {"*", "staff", "*", "*", "*", "*"}, false},
      {{nullptr, nullptr, nullptr, nullptr, "!@hgrp1", nullptr}, {"*", "staff", "*", "*", "*", "*"}, false},
      {{nullptr, nullptr, nullptr, nullptr, "!@hgrp1", nullptr}, {"*", "staff", "*", "*", "@hgrp*", "*"}, false},
   /* userset definition */
      {{"@userset1", nullptr, nullptr, nullptr, nullptr, nullptr}, {"user1", "staff", "*", "*", "*", "*"}, false},
      {{"@userset1", nullptr, nullptr, nullptr, nullptr, nullptr}, {"use*", "staff", "*", "*", "*", "*"}, false},
      {{"@users*", nullptr, nullptr, nullptr, nullptr, nullptr}, {"user1", "staff", "*", "*", "*", "*"}, false},
      {{"@users*", nullptr, nullptr, nullptr, nullptr, nullptr}, {"user*", "staff", "*", "*", "*", "*"}, false},
      {{"user1", nullptr, nullptr, nullptr, nullptr, nullptr}, {"@userset1", "staff", "*", "*", "*", "*"}, false},
      {{"us*", nullptr, nullptr, nullptr, nullptr, nullptr}, {"@userset1", "staff", "*", "*", "*", "*"}, false},
      {{"user1", nullptr, nullptr, nullptr, nullptr, nullptr}, {"@use*", "staff", "*", "*", "*", "*"}, false},
      {{"use*", nullptr, nullptr, nullptr, nullptr, nullptr}, {"@use*", "staff", "*", "*", "*", "*"}, false},
      {{"@user*2", nullptr, nullptr, nullptr, nullptr, nullptr}, {"user1", "staff", "*", "*", "*", "*"}, false},
      {{"@user*2", nullptr, nullptr, nullptr, nullptr, nullptr}, {"user1", "*", "*", "*", "*", "*"}, false},
      {{"!@user*", nullptr, nullptr, nullptr, nullptr, nullptr}, {"*", "*", "*", "*", "*", "*"}, false},
   /* project definition */
      {{nullptr, nullptr, "!*", nullptr, nullptr, nullptr}, {"*", "staff", nullptr, "*", "*", "*"}, false},
   /* end test */
      {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr}, {"*", "staff", "*", "*", "*", "*"}, true},
   };

   filter_test_t negativ_test[] = {
   /* simple search */
      {{"*,!user3", nullptr, nullptr, nullptr, nullptr, nullptr}, {"user3", "staff", "*", "*", "*", "*"}, false},
      {{"user1,user2", nullptr, nullptr, nullptr, nullptr, nullptr}, {"user3", "staff", "*", "*", "*", "*"}, false},
      {{nullptr, nullptr, "*,!project2", nullptr, nullptr, nullptr}, {"*", "staff", "project2", "*", "*", "*"}, false},
      {{nullptr, nullptr, "project1,project3", nullptr, nullptr, nullptr}, {"*", "staff", "project2", "*", "*", "*"}, false},
      {{nullptr, nullptr, nullptr, "*,!pe3", nullptr, nullptr}, {"*", "staff", "*", "pe3", "*", "*"}, false},
      {{nullptr, nullptr, nullptr, "pe1,pe2", nullptr, nullptr}, {"*", "staff", "*", "pe3", "*", "*"}, false},
      {{nullptr, nullptr, nullptr, nullptr, "*,!h3", nullptr}, {"*", "staff", "*", "*", "h3", "*"}, false},
      {{nullptr, nullptr, nullptr, nullptr, "h1,h2", nullptr}, {"*", "staff", "*", "*", "h3", "*"}, false},
      {{nullptr, nullptr, nullptr, nullptr, nullptr, "*,!queue1"}, {"*", "staff", "*", "*", "*", "queue1"}, false},
      {{nullptr, nullptr, nullptr, nullptr, nullptr, "queue2,queue3"}, {"*", "staff", "*", "*", "*", "queue1"}, false},
   /* wildcard definition, wildcard search */
      {{"!us*", nullptr, nullptr, nullptr, nullptr, nullptr}, {"user*", "staff", "*", "*", "*", "*"}, false},
      {{nullptr, nullptr, "!pro*", nullptr, nullptr, nullptr}, {"*", "staff", "project*", "*", "*", "*"}, false},
      {{nullptr, nullptr, nullptr, "!p*", nullptr, nullptr}, {"*", "staff", "*", "pe*", "*", "*"}, false},
      {{nullptr, nullptr, nullptr, nullptr, "!h*", nullptr}, {"*", "staff", "*", "*", "hos*", "*"}, false},
      {{nullptr, nullptr, nullptr, nullptr, nullptr, "!qu*"}, {"*", "staff", "*", "*", "*", "que*"}, false},
   /* hostgroup definition*/
      {{nullptr, nullptr, nullptr, nullptr, "!@hgrp1", nullptr}, {"*", "staff", "*", "*", "host1", "*"}, false},
      {{nullptr, nullptr, nullptr, nullptr, "!@hgrp1", nullptr}, {"*", "staff", "*", "*", "ho*", "*"}, false},
      {{nullptr, nullptr, nullptr, nullptr, "!@hgr*", nullptr}, {"*", "staff", "*", "*", "host1", "*"}, false},
      {{nullptr, nullptr, nullptr, nullptr, "!@hgr*", nullptr}, {"*", "staff", "*", "*", "hos*", "*"}, false},
      {{nullptr, nullptr, nullptr, nullptr, "!hgrp1", nullptr}, {"*", "staff", "*", "*", "hgrp1", "*"}, false},
      {{nullptr, nullptr, nullptr, nullptr, "!hgrp*", nullptr}, {"*", "staff", "*", "*", "hgrp*", "*"}, false},
   /* userset definition */
      {{"!@userset1", nullptr, nullptr, nullptr, nullptr, nullptr}, {"user1", "staff", "*", "*", "*", "*"}, false},
      {{"!@userset1", nullptr, nullptr, nullptr, nullptr, nullptr}, {"use*", "staff", "*", "*", "*", "*"}, false},
      {{"!@users*", nullptr, nullptr, nullptr, nullptr, nullptr}, {"user1", "staff", "*", "*", "*", "*"}, false},
      {{"!@users*", nullptr, nullptr, nullptr, nullptr, nullptr}, {"user*", "staff", "*", "*", "*", "*"}, false},
      {{"!@userset2", nullptr, nullptr, nullptr, nullptr, nullptr}, {"user1", "staff", "*", "*", "*", "*"}, false},
      {{"@userset1,!@userset2", nullptr, nullptr, nullptr, nullptr, nullptr}, {"user1", "staff", "*", "*", "*", "*"}, false},
      {{"@user*2", nullptr, nullptr, nullptr, nullptr, nullptr}, {"user1", nullptr, "*", "*", "*", "*"}, false},
   /* project definition */
      {{nullptr, nullptr, "*", nullptr, nullptr, nullptr}, {"*", "staff", nullptr, "*", "*", "*"}, false},
      {{nullptr, nullptr, "!*", nullptr, nullptr, nullptr}, {"*", "staff", "project1", "*", "*", "*"}, false},
   /* end test */
      {{"!*", nullptr, nullptr, nullptr, nullptr, nullptr}, {"*", "staff", "*", "*", "*", "*"}, true},
   };

   lList *hgroup_list;
   lListElem *hgroup;
   lList *userset_list;
   lListElem *userset;

   DENTER_MAIN(TOP_LAYER, "test_sge_resouce_quota");

   lInit(nmv);

   hgroup_list = lCreateList("" , HGRP_Type);
   hgroup = lCreateElem(HGRP_Type);
   lSetHost(hgroup, HGRP_name, "@hgrp1");
   lAddSubHost(hgroup, HR_name, "host1", HGRP_host_list, HR_Type);
   lAppendElem(hgroup_list, hgroup);
   hgroup = lCreateElem(HGRP_Type);
   lSetHost(hgroup, HGRP_name, "@hgrp2");
   lAddSubHost(hgroup, HR_name, "host2", HGRP_host_list, HR_Type);
   lAppendElem(hgroup_list, hgroup);

   userset_list = lCreateList("", US_Type);
   userset = lCreateElem(US_Type);
   lSetString(userset, US_name, "userset1");
   lAddSubStr(userset, UE_name, "user1", US_entries, UE_Type);
   lAppendElem(userset_list, userset);
   userset = lCreateElem(US_Type);
   lSetString(userset, US_name, "userset2");
   lAddSubStr(userset, UE_name, "@staff", US_entries, UE_Type);
   lAppendElem(userset_list, userset);

   for (i=0; ; i++){
      lListElem *rule = lCreateElem(RQR_Type);
      filter_t rule_filter = positiv_test[i].rule;
      filter_t query_filter = positiv_test[i].query;

      if (rqs_parse_filter_from_string(&filter, rule_filter.users, nullptr)) {
         lSetObject(rule, RQR_filter_users, filter);
      }
      if (rqs_parse_filter_from_string(&filter, rule_filter.projects, nullptr)) {
         lSetObject(rule, RQR_filter_projects, filter);
      }
      if (rqs_parse_filter_from_string(&filter, rule_filter.pes, nullptr)) {
         lSetObject(rule, RQR_filter_pes, filter);
      }
      if (rqs_parse_filter_from_string(&filter, rule_filter.hosts, nullptr)) {
         lSetObject(rule, RQR_filter_hosts, filter);
      }
      if (rqs_parse_filter_from_string(&filter, rule_filter.queues, nullptr)) {
         lSetObject(rule, RQR_filter_queues, filter);
      }
      if(!rqs_is_matching_rule(rule, query_filter.users, query_filter.group, query_filter.projects, 
                                query_filter.pes, query_filter.hosts, query_filter.queues,
                                userset_list, hgroup_list)) {
         printf("positiv filter matching failed (test %d)\n", i+1);
         pos_tests_failed++;
      }
      lFreeElem(&rule);
      if (positiv_test[i].last == true) {
         break;
      }
   }
   printf("%d positiv test(s) failed\n", pos_tests_failed);

   for (i=0; ; i++) {
      lListElem *rule = lCreateElem(RQR_Type);
      filter_t rule_filter = negativ_test[i].rule;
      filter_t query_filter = negativ_test[i].query;

      if (rqs_parse_filter_from_string(&filter, rule_filter.users, nullptr)) {
         lSetObject(rule, RQR_filter_users, filter);
      }
      if (rqs_parse_filter_from_string(&filter, rule_filter.projects, nullptr)) {
         lSetObject(rule, RQR_filter_projects, filter);
      }
      if (rqs_parse_filter_from_string(&filter, rule_filter.pes, nullptr)) {
         lSetObject(rule, RQR_filter_pes, filter);
      }
      if (rqs_parse_filter_from_string(&filter, rule_filter.hosts, nullptr)) {
         lSetObject(rule, RQR_filter_hosts, filter);
      }
      if (rqs_parse_filter_from_string(&filter, rule_filter.queues, nullptr)) {
         lSetObject(rule, RQR_filter_queues, filter);
      }
      if (rqs_is_matching_rule(rule, query_filter.users, query_filter.group, query_filter.projects, 
                                query_filter.pes, query_filter.hosts, query_filter.queues,
                                userset_list, hgroup_list)) {
         printf("negativ filter matching failed (test %d)\n", i+1);
         
         neg_tests_failed++;
      }

      lFreeElem(&rule);

      if (negativ_test[i].last == true) {
         break;
      }
  }

  printf("%d negativ test(s) failed\n", neg_tests_failed);
  
  lFreeList(&hgroup_list);
  lFreeList(&userset_list);

  DRETURN(pos_tests_failed + neg_tests_failed);
}
