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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/                                   

#include <cstdio>
#include <cstdlib>

#include "cull/cull.h"

#include "sge_stdlib.h"
#include "sge_string.h"
#include "uti/sge_dstring.h"
#include "sge_unistd.h"

#include "sgeobj/cull/sge_all_listsL.h"

#include "sge_answer.h"

#include "spool/flatfile/sge_flatfile.h"
#include "spool/sge_spooling_utilities.h"
#include "spool/flatfile/sge_spooling_flatfile.h"
#include "spool/flatfile/sge_flatfile_obj.h"

#include "sgeobj/sge_cqueue.h"

/* 
 * RD 05/10/2006
 * This test does not work! It's only necessary if flatfile code is used for qmaster spooling.
 * Currently the classic spooling code is used so there is no need to use this test.
 */
   

int main(int argc, char *argv[])
{
   lList* lp = nullptr;
   lList* cqueue_list = nullptr;
   lListElem* ep = nullptr;
   lListElem *cqueue = nullptr;
   lList *answer_list = nullptr;
   spooling_field *fields;
   const char *filepath;

   dstring queue_str = DSTRING_INIT;
   dstring copy_str  = DSTRING_INIT;

   int i;
   int width;
   char format[100];

   lInit(nmv);

   cqueue = cqueue_create(nullptr, "template");
   cqueue_set_template_attributes(cqueue, &answer_list);

   lSetString(cqueue, CQ_name, "foobar");

   lp = lCreateList ("Shell Terminate Methods", ASTR_Type);
   ep = lCreateElem(ASTR_Type);
   lSetHost(ep, ASTR_href, "global");
   lSetString(ep, ASTR_value, "/tmp/myterminate_method.sh");
   lAppendElem(lp, ep);
   lSetList(cqueue, CQ_terminate_method, lp);

   cqueue_list = lCreateList("CQ List", CQ_Type);
   lAppendElem(cqueue_list, cqueue);
 
   fields = spool_get_fields_to_spool(&answer_list, QU_Type, &spool_config_instr);
   answer_list_output(&answer_list);
   lFreeList(&answer_list);
   printf("\nthe following fields are spooled:");
   for(i = 0; fields[i].nm != NoName; i++) {
      printf(" %s", lNm2Str(fields[i].nm));
   }
   printf("\n");

   spool_flatfile_align_object(&answer_list, fields);
   answer_list_output(&answer_list);
   lFreeList(&answer_list);
   width = fields[0].width;
   printf("alignment for attribute names is %d\n", width);

   spool_flatfile_align_list(&answer_list, cqueue_list, fields, 0);
   answer_list_output(&answer_list);
   lFreeList(&answer_list);
   printf("field widths for list output is as follows:\n");
   
   snprintf(format, sizeof(format), "%%%ds: %%d\n", width);

   for(i = 0; fields[i].nm != NoName; i++) {
      printf(format, lNm2Str(fields[i].nm), fields[i].width);
   }

   filepath = spool_flatfile_write_object(&answer_list, (const lListElem *)cqueue, false, CQ_fields, &qconf_sfi,
                                          SP_DEST_STDOUT, SP_FORM_ASCII, nullptr, false);
   if(filepath != nullptr) {
      printf("\ndata successfully written to stdout\n");
      sge_free(&filepath);
   } else {
      answer_list_print_err_warn(&answer_list, nullptr, nullptr, nullptr);
   }
                               
   printf("\n");

   filepath = spool_flatfile_write_object(&answer_list, cqueue, false,
                               CQ_fields,
                               &qconf_sfi,
                               SP_DEST_TMP, SP_FORM_ASCII, nullptr, false);
   if(filepath != nullptr) {
      printf("temporary file %s successfully written\n", filepath);
      sge_unlink(nullptr, filepath);
      sge_free(&filepath);
   } else {
      answer_list_print_err_warn(&answer_list, nullptr, nullptr, nullptr);
   }
                               
   filepath = spool_flatfile_write_object(&answer_list, cqueue, false,
                               CQ_fields,
                               &qconf_sfi,
                               SP_DEST_SPOOL, SP_FORM_ASCII, 
                               "test_sge_spooling_flatfile.dat", false);
   if(filepath != nullptr) {
      int fields_out[MAX_NUM_FIELDS];
      lListElem *reread_queue;

      printf("spool file %s successfully written\n", filepath);

      /* reread queue from file */
      reread_queue = spool_flatfile_read_object(&answer_list, QU_Type, nullptr, CQ_fields, fields_out, true,
                                                &qconf_sfi, 
                                                SP_FORM_ASCII, nullptr,
                                                "test_sge_spooling_flatfile.dat");
     
      if(reread_queue == nullptr) {
         answer_list_print_err_warn(&answer_list, nullptr, nullptr, nullptr);
      } else {
         lWriteElemTo(reread_queue, stdout);
         lFreeElem(&reread_queue);
      }
     
      sge_unlink(nullptr, filepath);
      sge_free(&filepath);
   } else {
      answer_list_print_err_warn(&answer_list, nullptr, nullptr, nullptr);
   }
  
   filepath = spool_flatfile_write_list(&answer_list, cqueue_list,
                                        CQ_fields,
                                        &qconf_ce_list_sfi,
                                        SP_DEST_STDOUT, SP_FORM_ASCII, 
                                        nullptr, false);
   if(filepath != nullptr) {
      printf("\ndata successfully written to stdout\n");
      sge_free(&filepath);
   } else {
      answer_list_print_err_warn(&answer_list, nullptr, nullptr, nullptr);
   }
   
   filepath = spool_flatfile_write_list(&answer_list, cqueue_list,
                                        CQ_fields,
                                        &qconf_ce_list_sfi,
                                        SP_DEST_SPOOL, SP_FORM_ASCII, 
                                        "test_sge_spooling_flatfile.dat", false);
   if(filepath != nullptr) {
      int fields_out[MAX_NUM_FIELDS];
      lList *reread_list;

      printf("spool file %s successfully written\n", filepath);

      reread_list = spool_flatfile_read_list(&answer_list, QU_Type, CQ_fields, fields_out, true,
                                             &qconf_ce_list_sfi, SP_FORM_ASCII, nullptr, "test_sge_spooling_flatfile.dat");
      if (reread_list == nullptr) {
         answer_list_print_err_warn(&answer_list, nullptr, nullptr, nullptr);
      } else {
         lWriteListTo(reread_list, stdout);
         lFreeList(&reread_list);
      }
/*       sge_unlink(nullptr, filepath); */
      sge_free(&filepath);
   } else {
      answer_list_print_err_warn(&answer_list, nullptr, nullptr, nullptr);
   }

   /* test reading object */
   /* test nonexisting filename */
   
   /* test behaviour with nullptr-pointer passed */
   printf("\n\ntesting error handling, the next calls have to fail\n");
   spool_flatfile_align_object(&answer_list, nullptr);
   spool_flatfile_align_list(&answer_list, nullptr, fields, 0);
   spool_flatfile_align_list(&answer_list, cqueue_list, nullptr, 0);
   answer_list_print_err_warn(&answer_list, nullptr, nullptr, nullptr);

   /* cleanup */
   lFreeList(&cqueue_list);

   sge_dstring_free(&queue_str);
   sge_dstring_free(&copy_str);

   fields = spool_free_spooling_fields(fields);

   fprintf(stdout, "file handle stdout still alive\n");
   fprintf(stderr, "file handle stderr still alive\n");

   return EXIT_SUCCESS;
}
