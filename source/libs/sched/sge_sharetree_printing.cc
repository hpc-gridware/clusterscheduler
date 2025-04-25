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

#include <cstring>
#include <pthread.h>

#include "uti/sge_log.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"

#include "sgeobj/sge_sharetree.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_usage.h"

#include "sge_support.h"
#include "sge_sharetree_printing.h"

typedef enum {
   ULONG_T=0,
   DATE_T,
   STRING_T,
   DOUBLE_T
} item_type_t;

typedef struct {
   const char *name;
   item_type_t type;
   void *val;
} item_t;


/* This module uses a number of module global variables.
 * Access to these variables will be locked using this mutex.
 */
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

static double mem, cpu, io, ltmem, ltcpu, ltio, level, total,
       lt_share, st_share, actual_share, combined_usage;
static u_long64 current_time, time_stamp;
static lUlong shares, job_count;
static const char *node_name, *user_name, *project_name;

static const item_t item[] = {
    { "curr_time", DATE_T, &current_time },
    { "usage_time", DATE_T, &time_stamp },
    { "node_name", STRING_T, &node_name },
    { "user_name", STRING_T, &user_name },
    { "project_name", STRING_T, &project_name },
    { "shares", ULONG_T, &shares },
    { "job_count", ULONG_T, &job_count },
    { "level%", DOUBLE_T, &level },
    { "total%", DOUBLE_T, &total },
    { "long_target_share", DOUBLE_T, &lt_share },
    { "short_target_share", DOUBLE_T, &st_share },
    { "actual_share", DOUBLE_T, &actual_share },
    { "usage", DOUBLE_T, &combined_usage },
    { "cpu", DOUBLE_T, &cpu },
    { "mem", DOUBLE_T, &mem },
    { "io", DOUBLE_T, &io },
    { "ltcpu", DOUBLE_T, &ltcpu },
    { "ltmem", DOUBLE_T, &ltmem },
    { "ltio", DOUBLE_T, &ltio }
};

static const int items = sizeof(item) / sizeof(item_t);

/* ------------- static functions ---------------- */

static void
calculate_share_percents(lListElem *node, double parent_percent, double sibling_shares)
{
   lListElem *child;
   double sum_shares=0;

   for_each_rw(child, lGetList(node, STN_children)) {
      sum_shares += lGetUlong(child, STN_shares);
   }

   if (sibling_shares > 0) {
      lSetDouble(node, STN_proportion,
		 (double)lGetUlong(node, STN_shares) / (double)sibling_shares);
   } else {
      lSetDouble(node, STN_proportion, 0);
   }

   if (sibling_shares > 0) {
      lSetDouble(node, STN_adjusted_proportion,
		 parent_percent *
		 (double)lGetUlong(node, STN_shares) / (double)sibling_shares);
   } else {
      lSetDouble(node, STN_adjusted_proportion, 0);
   }

   for_each_rw(child, lGetList(node, STN_children)) {
      calculate_share_percents(child, lGetDouble(node, STN_adjusted_proportion), sum_shares);
   }
}

static double
get_usage_value(const lList *usage, const char *name)
{
   const lListElem *ue;
   double value = 0;

   if ((ue = lGetElemStr(usage, UA_name, name))) {
      value = lGetDouble(ue, UA_value);
   }

   return value;
}

static void
print_field(dstring *out, rapidjson::Writer<rapidjson::StringBuffer> *writer, const item_t *field,
            const format_t *format)
{
   // print item name (key in case of json)
   if (out != nullptr && format->name_format) {
      sge_dstring_sprintf_append(out, "%s=", field->name);
   }
   if (writer != nullptr) {
      writer->Key(field->name);
   }

   // print item data
   switch (field->type) {
      case ULONG_T:
         if (out != nullptr) {
            sge_dstring_sprintf_append(out, sge_u32, *static_cast<u_long32 *>(field->val));
         }
         if (writer != nullptr) {
            writer->Uint64(*(u_long32 *) field->val);
         }
         break;
      case DATE_T: {
         u_long64 t = *(u_long64 *) field->val;
         if (out != nullptr) {
            if (t && format->format_times) {
               DSTRING_STATIC(tc_dstr, 64);
               const char *tc_str;

               tc_str = sge_ctime64(t, &tc_dstr);
               sge_dstring_sprintf_append(out, format->str_format, tc_str);
            } else {
               sge_dstring_sprintf_append(out, sge_u64, t);
            }
         }
         if (writer != nullptr) {
            writer->Uint64(t);
         }
      }
         break;
      case DOUBLE_T:
         if (out != nullptr) {
            sge_dstring_sprintf_append(out, "%f", *(double *) field->val);
         }
         if (writer != nullptr) {
            writer->Double(*(double *) field->val);
         }
         break;
      case STRING_T:
         if (out != nullptr) {
            sge_dstring_sprintf_append(out, format->str_format, *(char **) field->val);
         }
         if (writer != nullptr) {
            writer->String(*(char **) field->val);
         }
         break;
   }

   if (out) {
      sge_dstring_sprintf_append(out, "%s", format->delim);
   }
}

static void
print_node(dstring *out, rapidjson::StringBuffer *jsonBuffer, const lListElem *node,
           const lListElem *user, const lListElem *project, const char **names, const format_t *format,
           const lListElem *parent, const char *parent_node_names)
{
   if (node != nullptr) {
      const lList *usage=nullptr, *ltusage=nullptr;
      int i, fields_printed=0;
      dstring node_name_dstring = DSTRING_INIT;
      rapidjson::Writer<rapidjson::StringBuffer> *writer = nullptr;
      if (jsonBuffer != nullptr) {
         // we write multiple lines into the buffer, start a new line
         // the current stable version of rapidjson (1.1.0) doesn't have GetLength().
         // GetSize() gives us the size in characters (not bytes) which is OK as well here
         if (jsonBuffer->GetSize() > 0) {
            jsonBuffer->Put('\n');
         }
         writer = new rapidjson::Writer<rapidjson::StringBuffer>(*jsonBuffer);
      }

      current_time = sge_get_gmt64();
      time_stamp = user != nullptr ? lGetUlong64(user, UU_usage_time_stamp) : 0;

      /*
       * we want to name the Root node simply /, instead of /Root 
       * but it is possible to create nodes /project1/Root
       */
      if (parent == nullptr) {
         sge_dstring_sprintf(&node_name_dstring, "/");
      } else {
         sge_dstring_sprintf(&node_name_dstring, "%s/%s", parent_node_names, lGetString(node, STN_name));
      }
      node_name = sge_dstring_get_string(&node_name_dstring);

      user_name = user ? lGetString(user, UU_name) : "";
      project_name = project ? lGetString(project, PR_name) : "";
      shares = lGetUlong(node, STN_shares);
      job_count = lGetUlong(node, STN_job_ref_count);
      level = lGetDouble(node, STN_proportion);
      total = lGetDouble(node, STN_adjusted_proportion);
      lt_share = lGetDouble(node, STN_m_share);
      st_share = lGetDouble(node, STN_adjusted_current_proportion);
      actual_share = lGetDouble(node, STN_actual_proportion);
      combined_usage = lGetDouble(node, STN_combined_usage);

      if (lGetList(node, STN_children) == nullptr && user && project) {
         const lList *projl = lGetList(user, UU_project);
         const lListElem *upp;
         if (projl) {
            if ((upp=lGetElemStr(projl, UPP_name,
                                 lGetString(project, PR_name)))) {
               usage = lGetList(upp, UPP_usage);
               ltusage = lGetList(upp, UPP_long_term_usage);
            }
         }
      } else if (user && strcmp(lGetString(user, UU_name), 
                                lGetString(node, STN_name))==0) {
         usage = lGetList(user, UU_usage);
         ltusage = lGetList(user, UU_long_term_usage);
      } else if (project && strcmp(lGetString(project, PR_name), 
                                   lGetString(node, STN_name))==0) {
         usage = lGetList(project, PR_usage);
         ltusage = lGetList(project, PR_long_term_usage);
      }

      if (usage != nullptr) {
         cpu = get_usage_value(usage, USAGE_ATTR_CPU);
         mem = get_usage_value(usage, USAGE_ATTR_MEM);
         io  = get_usage_value(usage, USAGE_ATTR_IO);
      } else {
         cpu = mem = io = 0;
      }

      if (ltusage != nullptr) {
         ltcpu = get_usage_value(ltusage, USAGE_ATTR_CPU);
         ltmem = get_usage_value(ltusage, USAGE_ATTR_MEM);
         ltio  = get_usage_value(ltusage, USAGE_ATTR_IO);
      } else {
         ltcpu = ltmem = ltio = 0;
      }

      if (names) {
         int found=0;
         const char **name = names;
         while (*name) {
            if (strcmp(*name, node_name)==0) {
               found = 1;
            }   
            name++;
         }
         if (!found) {
            sge_dstring_free(&node_name_dstring);
            return;
         }   
      }

      /* print line prefix */
      if (format->line_prefix != nullptr) {
         if (out != nullptr) {
            sge_dstring_append(out, format->line_prefix);
         }
         if (writer != nullptr) {
            writer->StartObject();
            writer->Key("time");
            writer->Uint64(current_time);
            writer->Key("type");
            writer->String(format->line_prefix);
         }
      }

      if (format->field_names != nullptr) {
         struct saved_vars_s *context = nullptr;
         char *field;

         field = sge_strtok_r(format->field_names, ",", &context);
         while (field) {
            for (i = 0; i < items; i++) {
               if (strcmp(field, item[i].name) == 0) {
                  print_field(out, writer, &item[i], format);
                  fields_printed++;
                  break;
               }
            }
            field = sge_strtok_r(nullptr, ",", &context);
         }
         sge_free_saved_vars(context);
      } else {
         for (i = 0; i < items; i++) {
            print_field(out, writer, &item[i], format);
            fields_printed++;
         }
      }

      if (fields_printed) {
         if (out != nullptr) {
            sge_dstring_sprintf_append(out, "%s", format->line_delim);
         }
         if (writer != nullptr) {
            writer->EndObject();
         }
      }

      sge_dstring_free(&node_name_dstring);
      if (writer != nullptr) {
         delete writer;
      }
   }
}


static void
print_nodes(dstring *out, rapidjson::StringBuffer *jsonBuffer, const lListElem *node, const lListElem *parent,
            const lListElem *project, const lList *users, const lList *projects, bool group_nodes, const char **names,
            const format_t *format, const char *parent_node_names)
{
   const lListElem *user, *child;
   const lList *children = lGetList(node, STN_children);
   dstring node_name_dstring = DSTRING_INIT;

   if (!project) {
      project = prj_list_locate(projects, lGetString(node, STN_name));
   }

   if (children == nullptr) {
      user = user_list_locate(users, lGetString(node, STN_name));
   } else {
      user = nullptr;
   }

   if (group_nodes || (children == nullptr)) {
      print_node(out, jsonBuffer, node, user, project, names, format, parent, parent_node_names);
   }

   for_each_ep(child, children) {
      /* we want to name the Root node simply /, instead of /Root */
      if (parent == nullptr) {
         sge_dstring_sprintf(&node_name_dstring, "");
      } else {
         sge_dstring_sprintf(&node_name_dstring, "%s/%s", parent_node_names, lGetString(node, STN_name));
      }
      print_nodes(out, jsonBuffer, child, node, project, users, projects,
                  group_nodes, names, format, sge_dstring_get_string(&node_name_dstring));
   }

   sge_dstring_free(&node_name_dstring);
}

/* ------------- public functions ---------------- */

/****** sge_sharetree_printing/print_hdr() *************************************
*  NAME
*     print_hdr() -- print a header for the sharetree dump
*
*  SYNOPSIS
*     void 
*     print_hdr(dstring *out, const format_t *format) 
*
*  FUNCTION
*     Prints a header for data output using the sge_sharetree_print function.
*
*  INPUTS
*     dstring *out           - dstring into which data will be written
*     const format_t *format - format description
*
*  NOTES
*     MT-NOTE: print_hdr() is MT-safe
*
*  SEE ALSO
*     sge_sharetree_printing/sge_sharetree_print()
*******************************************************************************/
void
print_hdr(dstring *out, const format_t *format)
{
   int i;

   DENTER(TOP_LAYER);
   sge_mutex_lock("sharetree_printing", __func__, __LINE__, &mtx);
   
   if (format->field_names) {
      struct saved_vars_s *context = nullptr;
      char *field;

      field = sge_strtok_r(format->field_names, ",", &context);
      while (field) {
         for (i=0; i<items; i++) {
            if (strcmp(field, item[i].name) == 0) {
               sge_dstring_sprintf_append(out, "%s%s", item[i].name, 
                                          format->delim);
               break;
            }
         }
         field = sge_strtok_r(nullptr, ",", &context);
      }
      sge_free_saved_vars(context);
   } else {
      for (i=0; i<items; i++) {
         sge_dstring_sprintf_append(out, "%s%s", item[i].name, format->delim);
      }
   }

   sge_dstring_sprintf_append(out, "%s", format->line_delim);
   sge_dstring_sprintf_append(out, "%s", format->rec_delim);

   sge_mutex_unlock("sharetree_printing", __func__, __LINE__, &mtx);
   DRETURN_VOID;
}

/****** sge_sharetree_printing/sge_sharetree_print() ***************************
*  NAME
*     sge_sharetree_print() -- dump sharetree information to a dstring
*
*  SYNOPSIS
*     void sge_sharetree_print(dstring *out, lList *sharetree, lList *users, 
*                              lList *projects, lList *config, 
*                              bool group_nodes, bool decay_usage, 
*                              const char **names, const format_t *format) 
*
*  FUNCTION
*     Dumps information about a sharetree into a given dstring. Information
*     is appended.
*
*     Outputs information like times, node (user/project) names, configured
*     shares, actually received shares, targeted shares, usage information
*     like cpu, memory and io.
*
*     It is possible to restrict the number of fields that are output.
*
*     Header information and formatting can be configured.
*
*  INPUTS
*     dstring *out           - dstring into which data will be written
*     lList *sharetree       - the sharetree to dump
*     lList *users           - the user list
*     lList *projects        - the project list
*     lList *config          - the scheduler configuration list
*     bool group_nodes       - ??? 
*     bool decay_usage       - ??? 
*     const char **names     - fields to output
*     const format_t *format - format description
*
*  NOTES
*     MT-NOTE: sge_sharetree_print() is  MT-safe 
*
*  SEE ALSO
*     sge_sharetree_printing/print_hdr()
*******************************************************************************/
void
sge_sharetree_print(dstring *out, rapidjson::StringBuffer *jsonBuffer, const lList *sharetree_in,
                    const lList *users, const lList *projects, const lList *usersets, bool group_nodes,
                    bool decay_usage, const char **names, const format_t *format)
{

   lListElem *root;
   u_long64 curr_time = 0;
   lList *sharetree;

   DENTER(TOP_LAYER);

   /* 
    * The sharetree might contain "default" nodes which
    * have to be resolved to individual user nodes.
    * This implies modifying the sharetree - so we better create a 
    * copy of the sharetree
    */
   sharetree = lCopyList("copy of sharetree", sharetree_in);
   
   /* Resolve the default users */
   sge_add_default_user_nodes(lFirstRW(sharetree), users, projects, usersets);

   /* 
    * The sharetree calculation and output uses lots of global variables
    * Better control access to them through a mutex.
    */
   sge_mutex_lock("sharetree_printing", __func__, __LINE__, &mtx);

   root = lFirstRW(sharetree);

   calculate_share_percents(root, 1.0, lGetUlong(root, STN_shares));

   if (decay_usage) {
      curr_time = sge_get_gmt64();
   }

   _sge_calc_share_tree_proportions(sharetree, users, projects, nullptr, curr_time);

   print_nodes(out, jsonBuffer, root, nullptr, nullptr, users, projects, group_nodes, names, format, "");

   sge_mutex_unlock("sharetree_printing", __func__, __LINE__, &mtx);

   /* free our sharetree copy */
   lFreeList(&sharetree);
   DRETURN_VOID;
}

