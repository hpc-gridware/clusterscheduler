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
#include <unistd.h>
#include <cstring>
#include <ctime>

#include "uti/sge_dstring.h"
#include "uti/sge_language.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"

#include "gdi/sge_gdi_ctx.h"
#include "gdi/sge_gdi.h"
#include "gdi/sge_gdi2.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_usage.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_schedd_conf.h"

#include "sched/sge_sharetree_printing.h"

#include "msg_smon.h"

static int
free_lists(lList **sharetree, lList **users, lList **projects, lList **usersets, lList **config)
{
   lFreeList(sharetree);
   lFreeList(users);
   lFreeList(projects);
   lFreeList(usersets);
   lFreeList(config);

   return 0;
}

static int
setup_lists(lList **sharetree, lList **users, lList **projects, lList **usersets, lList **config)
{
   lList *alp = nullptr;               /* answer list for individual gdi_multi */
   lList *malp = nullptr;              /* answer list for final gdi_multi */
   lEnumeration *what;
   state_gdi_multi state = STATE_GDI_MULTI_INIT;
   int sharetree_id = 0;
   int sched_conf_id = 0;
   int user_id = 0;
   int project_id = 0;
   int userset_id = 0;
   bool error;

   /* get share tree */
   what = lWhat("%T(ALL)", STN_Type);
   sharetree_id = sge_gdi2_multi(&alp, SGE_GDI_RECORD, SGE_STN_LIST,
                                 SGE_GDI_GET, nullptr, nullptr, what, &state, true);
   lFreeWhat(&what);
   error = answer_list_output(&alp);

   /* get config list */
   if (!error) {
      what = lWhat("%T(ALL)", SC_Type);
      sched_conf_id = sge_gdi2_multi(&alp, SGE_GDI_RECORD, SGE_SC_LIST, SGE_GDI_GET,
                                    nullptr, nullptr, what, &state, true);
      lFreeWhat(&what);
      error = answer_list_output(&alp);
   }

   /* get user list */
   if (!error) {
      what = lWhat("%T(ALL)", UU_Type);
      user_id = sge_gdi2_multi(&alp, SGE_GDI_RECORD, SGE_UU_LIST, SGE_GDI_GET,
                              nullptr, nullptr, what, &state, true);
      lFreeWhat(&what);
      error = answer_list_output(&alp);
   }

   /* get project list */
   if (!error) {
      what = lWhat("%T(ALL)", PR_Type);
      project_id = sge_gdi2_multi(&alp, SGE_GDI_RECORD, SGE_PR_LIST,
                                  SGE_GDI_GET, nullptr, nullptr, what, &state, true);
      lFreeWhat(&what);
      error = answer_list_output(&alp);
   }

   /*
    * get userset list 
    * send gdi multi request to qmaster
    */
   if (!error) {
      what = lWhat("%T(ALL)", US_Type);
      userset_id = sge_gdi2_multi(&alp, SGE_GDI_SEND, SGE_US_LIST, SGE_GDI_GET,
                                 nullptr, nullptr, what, &state, true);
      sge_gdi2_wait(&alp, &malp, &state);
      lFreeWhat(&what);
      error = answer_list_output(&alp);
   }

   /* if any of the above operations failed, exit */
   if (error) {
      lFreeList(&malp);
      exit(3);
   }

   /* extract the sharetree lists */
   sge_gdi_extract_answer(&alp, SGE_GDI_GET, SGE_STN_LIST, sharetree_id, malp, sharetree);
   error = answer_list_output(&alp);

   /* if we have no sharetree, output message and exit */
   if (!error) {
      if (!*sharetree || lFirst(*sharetree) == nullptr) {
         fprintf(stderr, "%s\n", MSG_SGESHAREMON_NOSHARETREE);
         lFreeList(sharetree);
         lFreeList(&malp);
         exit(2);
      }
   }

   /* extract sched_conf, users, projects, usersets */
   if (!error) {
      sge_gdi_extract_answer(&alp, SGE_GDI_GET, SGE_SC_LIST, sched_conf_id, malp, config);
      error = answer_list_output(&alp);
   }
   if (!error) {
      sge_gdi_extract_answer(&alp, SGE_GDI_GET, SGE_UU_LIST, user_id, malp, users);
      error = answer_list_output(&alp);
   }
   if (!error) {
      sge_gdi_extract_answer(&alp, SGE_GDI_GET, SGE_PR_LIST, project_id, malp, projects);
      error = answer_list_output(&alp);
   }
   if (!error) {
      sge_gdi_extract_answer(&alp, SGE_GDI_GET, SGE_US_LIST, userset_id, malp, usersets);
      error = answer_list_output(&alp);
   }

   /* cleanup */
   lFreeList(&malp);

   /* if any of the above operations failed, exit */
   if (error) {
      free_lists(sharetree, users, projects, usersets, config);
      exit(3);
   }

   return 0;
}





static void
usage(void)
{
      fprintf(stderr, "%s sge_share_mon [-cdfhilmnorsux] [node_names ...]\n", MSG_USAGE); 
      fprintf(stderr, "\n" );
      fprintf(stderr, "   -c count          %s\n", MSG_SGESHAREMON_c_OPT_USAGE );
      fprintf(stderr, "   -d delimiter      %s\n", MSG_SGESHAREMON_d_OPT_USAGE );
      fprintf(stderr, "   -f field[,field]  %s\n", MSG_SGESHAREMON_f_OPT_USAGE );
      fprintf(stderr, "   -h                %s\n", MSG_SGESHAREMON_h_OPT_USAGE );
      fprintf(stderr, "   -i interval       %s\n", MSG_SGESHAREMON_i_OPT_USAGE );
      fprintf(stderr, "   -l delimiter      %s\n", MSG_SGESHAREMON_l_OPT_USAGE );
      fprintf(stderr, "   -m output_mode    %s\n", MSG_SGESHAREMON_m_OPT_USAGE );
      fprintf(stderr, "   -n                %s\n", MSG_SGESHAREMON_n_OPT_USAGE );
      fprintf(stderr, "   -o output_file    %s\n", MSG_SGESHAREMON_o_OPT_USAGE );
      fprintf(stderr, "   -r delimiter      %s\n", MSG_SGESHAREMON_r_OPT_USAGE );
      fprintf(stderr, "   -s string_format  %s\n", MSG_SGESHAREMON_s_OPT_USAGE );
      fprintf(stderr, "   -t                %s\n", MSG_SGESHAREMON_t_OPT_USAGE );
      fprintf(stderr, "   -u                %s\n", MSG_SGESHAREMON_u_OPT_USAGE );
      fprintf(stderr, "   -x                %s\n", MSG_SGESHAREMON_x_OPT_USAGE );
      fprintf(stderr,"\n");
}

static FILE *
open_output(const char *file_name, const char *mode) 
{
   FILE *file = stdout;
   
   if (file_name != nullptr) {
      file = fopen(file_name, mode);
      if (file == nullptr) {
         fprintf(stderr, MSG_FILE_COULDNOTOPENXFORY_SS , file_name, mode);
         fprintf(stderr, "\n");
         exit(1);
      }
   }

   return file;
}

static FILE *
close_output(FILE *file)
{
   if (file != stdout) {
      FCLOSE(file);
      file = nullptr;
   }
FCLOSE_ERROR:
   return file;
}


int
main(int argc, char **argv)
{
   lList *sharetree = nullptr;
   lList *users = nullptr;
   lList *projects = nullptr;
   lList *usersets = nullptr;
   lList *config = nullptr;

   int interval=15;
   int err=0;
   int count=-1;
   int header=0;
   format_t format;
   char *ofile=nullptr;
   const char **names=nullptr;
   bool group_nodes=true;
   bool decay_usage=false;
   int c;
   extern char *optarg;
   extern int optind;
   FILE *outfile = nullptr;
   char *output_mode = "a";

   dstring output_dstring = DSTRING_INIT;

   lList *alp = nullptr;
   
   DENTER_MAIN(TOP_LAYER, "share_mon");

   format.name_format  = false;
   format.delim        = "\t";
   format.line_delim   = "\n";
   format.rec_delim    = "\n";
   format.str_format   = "%s";
   format.field_names  = nullptr;
   format.format_times = false;
   format.line_prefix  = nullptr;


#ifdef __SGE_COMPILE_WITH_GETTEXT__  
   /* init language output for gettext() , it will use the right language */
   sge_init_language_func((gettext_func_type)        gettext,
                         (setlocale_func_type)      setlocale,
                         (bindtextdomain_func_type) bindtextdomain,
                         (textdomain_func_type)     textdomain);
   sge_init_language(nullptr,nullptr);
#endif /* __SGE_COMPILE_WITH_GETTEXT__  */


   while ((c = getopt(argc, argv, "i:c:f:d:hl:m:no:r:s:tux")) != -1) {
      switch(c) {
	 case 'i':
	    if (sscanf(optarg, "%d", &interval) != 1) {
		fprintf(stderr, MSG_ERROR_XISNOTAVALIDINTERVAL_S , optarg);
		err++;
	    }
	    break;
	 case 'c':
	    if (sscanf(optarg, "%d", &count) != 1) {
		fprintf(stderr, MSG_ERROR_XISNOTAVALIDCOUNT_S , optarg);
		err++;
	    }
	    break;
         case 'f':
            format.field_names = strdup(optarg);
            break;
	 case 'o':
	    ofile = strdup(optarg);
	    break;
	 case 'd':
	    format.delim = strdup(optarg);
	    break;
         case 'h':
            header = 1;
            break;
	 case 'm':
	    output_mode = strdup(optarg);
	    break;
	 case 'n':
	    format.name_format = true;
	    break;
	 case 'l':
	    format.line_delim = strdup(optarg);
	    break;
	 case 'r':
	    format.rec_delim = strdup(optarg);
	    break;
	 case 's':
	    format.str_format = strdup(optarg);
	    break;
	 case 't':
	    format.format_times = true;
	    break;
	 case 'u':
	    decay_usage = true;
	    break;
	 case 'x':
	    group_nodes = false;
	    break;
	 case '?':
	    err++;
      }
   }

   if (err) {
      usage();
      sge_prof_cleanup();
      sge_exit(1);
   }

   if ((argc - optind) > 0) {
       names = (const char **)&argv[optind];
   }

   if (sge_gdi2_setup(SGE_SHARE_MON, MAIN_THREAD, &alp) != AE_OK) {
      answer_list_output(&alp);
      sge_exit(1);
   }

   if (header) {
      print_hdr(&output_dstring, &format);
   }

   while(count == -1 || count-- > 0) {
      setup_lists(&sharetree, &users, &projects, &usersets, &config);

      sconf_set_config(&config, nullptr);

      sge_sharetree_print(&output_dstring, sharetree, users, projects, usersets, group_nodes, decay_usage, names, &format);

      if (count && strlen(format.rec_delim)) {
	      sge_dstring_sprintf_append(&output_dstring, "%s", format.rec_delim);
      }

      outfile = open_output(ofile, output_mode);
      fwrite(sge_dstring_get_string(&output_dstring), 
             sge_dstring_strlen(&output_dstring), 1, outfile);
      outfile = close_output(outfile);

      free_lists(&sharetree, &users, &projects, &usersets, &config);
      sge_dstring_clear(&output_dstring);

      if (count) {
         sleep(interval);
      }
   }

   sge_gdi2_shutdown();

   sge_prof_cleanup();
   sge_dstring_free(&output_dstring);

   DRETURN(0);
}
