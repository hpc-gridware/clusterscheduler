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

#include <cstdio>
#include <cstdlib>

#include "cull/cull.h"

#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"
#include "uti/sge_dstring.h"
#include "uti/sge_unistd.h"

#include "sgeobj/cull/sge_all_listsL.h"

#include "sge_answer.h"

#include "spool/sge_spooling_utilities.h"
#include "spool/template/sge_spooling_template.h"

int main(int argc, char *argv[])
{
   lListElem *queue, *copy;
   lList *queue_list;
   lList *answer_list = nullptr;
   const lDescr *descr;
   spooling_field *fields;
   const char *filepath;
   int i;
   int width;
   char format[100];

   lInit(nmv);

   queue = queue_create_template();
   lSetString(queue, QU_terminate_method, "/tmp/myterminate_method.sh");
   lAddSubStr(queue, CE_name, "foo", QU_suspend_thresholds, CE_Type);
   lAddSubStr(queue, CE_name, "bar", QU_suspend_thresholds, CE_Type);
   copy  = lCreateElem(QU_Type);

   queue_list = lCreateList("queue_list", QU_Type);
   lAppendElem(queue_list, queue);
   lAppendElem(queue_list, copy);

   descr = lGetElemDescr(queue);
   
   fields = spool_get_fields_to_spool(&answer_list, QU_Type, &spool_config_instr);
   printf("\nthe following fields are spooled:");
   for(i = 0; fields[i].nm != NoName; i++) {
      printf(" %s", lNm2Str(fields[i].nm));
   }
   printf("\n");

   spool_flatfile_align_object(&answer_list, fields);
   width = fields[0].width;
   printf("alignment for attribute names is %d\n", width);

   spool_flatfile_align_list(&answer_list, queue_list, fields);
   printf("field widths for list output is as follows:\n");
   
   snprintf(format, sizeof(format), "%%%ds: %%d\n", width);

   for(i = 0; fields[i].nm != NoName; i++) {
      printf(format, lNm2Str(fields[i].nm), fields[i].width);
   }

   filepath = spool_flatfile_write_object(&answer_list, queue,
                                          nullptr,
                                          &spool_flatfile_instr_config,
                                          SP_DEST_STDOUT, SP_FORM_ASCII, nullptr);
   if(filepath != nullptr) {
      printf("\ndata successfully written to stdout\n");
      sge_free(&filepath);
   } else {
      answer_list_print_err_warn(&answer_list, nullptr, nullptr);
   }
                               
   printf("\n");

   filepath = spool_flatfile_write_object(&answer_list, queue,
                               nullptr,
                               &spool_flatfile_instr_config,
                               SP_DEST_TMP, SP_FORM_ASCII, nullptr);
   if(filepath != nullptr) {
      printf("temporary file %s successfully written\n", filepath);
      sge_unlink(nullptr, filepath);
      sge_free(&filepath);
   } else {
      answer_list_print_err_warn(&answer_list, nullptr, nullptr);
   }
                               
   filepath = spool_flatfile_write_object(&answer_list, queue,
                               nullptr,
                               &spool_flatfile_instr_config,
                               SP_DEST_SPOOL, SP_FORM_ASCII, 
                               "test_sge_spooling_flatfile.dat");
   if(filepath != nullptr) {
      lListElem *reread_queue;

      printf("spool file %s successfully written\n", filepath);

      /* reread queue from file */
      reread_queue = spool_flatfile_read_object(&answer_list, QU_Type, nullptr, nullptr,
                                                &spool_flatfile_instr_config, 
                                                SP_FORM_ASCII, nullptr,
                                                "test_sge_spooling_flatfile.dat");
     
      if(reread_queue == nullptr) {
         answer_list_print_err_warn(&answer_list, nullptr, nullptr);
      } else {
         lWriteElemTo(reread_queue, stdout);
         lFreeElem(&reread_queue);
      }
     
      sge_unlink(nullptr, filepath);
      sge_free(&filepath);
   } else {
      answer_list_print_err_warn(&answer_list, nullptr, nullptr);
   }
   
   filepath = spool_flatfile_write_list(&answer_list, queue_list,
                                        nullptr,
                                        &spool_flatfile_instr_config_list,
                                        SP_DEST_STDOUT, SP_FORM_ASCII, 
                                        nullptr);
   if(filepath != nullptr) {
      printf("\ndata successfully written to stdout\n");
      sge_free(&filepath);
   } else {
      answer_list_print_err_warn(&answer_list, nullptr, nullptr);
   }
   
   filepath = spool_flatfile_write_list(&answer_list, queue_list,
                                        nullptr,
                                        &spool_flatfile_instr_config_list,
                                        SP_DEST_SPOOL, SP_FORM_ASCII, 
                                        "test_sge_spooling_flatfile.dat");
   if(filepath != nullptr) {
      lList *reread_list;

      printf("spool file %s successfully written\n", filepath);

      reread_list = spool_flatfile_read_list(&answer_list, QU_Type, nullptr, nullptr, &spool_flatfile_instr_config_list, SP_FORM_ASCII, nullptr, "test_sge_spooling_flatfile.dat");
      if (reread_list == nullptr) {
         answer_list_print_err_warn(&answer_list, nullptr, nullptr);
      } else {
         lWriteListTo(reread_list, stdout);
         lFreeList(&reread_list);
      }
/*       sge_unlink(nullptr, filepath); */
      sge_free(&filepath);
   } else {
      answer_list_print_err_warn(&answer_list, nullptr, nullptr);
   }

   /* test reading object */
   /* test nonexisting filename */
   
   /* test behaviour with nullptr-pointer passed */
   printf("\n\ntesting error handling, the next calls have to fail\n");
   spool_flatfile_align_object(&answer_list, nullptr);
   spool_flatfile_align_list(&answer_list, nullptr, fields);
   spool_flatfile_align_list(&answer_list, queue_list, nullptr);
   answer_list_print_err_warn(&answer_list, nullptr, nullptr);

   /* cleanup */
   lFreeList(&queue_list);

   fields = spool_free_spooling_fields(fields);

   fprintf(stdout, "file handle stdout still alive\n");
   fprintf(stderr, "file handle stderr still alive\n");

   return EXIT_SUCCESS;
}
