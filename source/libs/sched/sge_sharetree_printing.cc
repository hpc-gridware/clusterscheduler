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
 * @brief Rendering a share tree, for `qconf -sst` and friends
 *
 * The share tree of the fair share policy is a tree of users and projects
 * with configured shares and accumulated usage. This module renders it - as
 * plain text or as JSON - with the field selection and the delimiters the
 * caller asks for, which is how the same data feeds a human readable table
 * and a machine readable dump.
 */

#include <cstring>
#include <pthread.h>

#include "uti/sge_log.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"

#include "sgeobj/ocs_ShareTree.h"
#include "sgeobj/sge_sharetree.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_usage.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_STN.h"
#include "sgeobj/tom/ocs_tom_UA.h"
#include "sgeobj/tom/ocs_tom_UPP.h"

#include "sge_sharetree_printing.h"

/** @brief Type of one printable field of a share tree node */
typedef enum {
   ULONG_T=0,   ///< Unsigned integer
   DATE_T,      ///< Time value, printed as a date when `format_times` is set
   STRING_T,    ///< String
   DOUBLE_T     ///< Floating point value
} item_type_t;

/** @brief One printable field of a share tree node */
typedef struct {
   const char *name;   ///< Field name, as it appears in the header and in `field_names`
   item_type_t type;   ///< How to render `val`
   void *val;          ///< Pointer to the value, interpreted according to `type`
} item_t;


/* This module uses a number of module global variables.
 * Access to these variables will be locked using this mutex.
 */
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

static double mem, cpu, io, ltmem, ltcpu, ltio, level, total,
       lt_share, st_share, actual_share, combined_usage;
static uint64_t current_time, time_stamp;
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
calculate_share_percents(lListElem *node, double parent_percent, double sibling_shares) {
   double sum_shares=0;

   for (ocs::tom::Element<ocs::tom::STN> child : ocs::tom::get<STN_children>(node)) {
      sum_shares += ocs::tom::get<STN_shares>(child);
   }

   if (sibling_shares > 0) {
      ocs::tom::set<STN_proportion>(node,
		 (double)ocs::tom::get<STN_shares>(node) / (double)sibling_shares);
   } else {
      ocs::tom::set<STN_proportion>(node, 0);
   }

   if (sibling_shares > 0) {
      ocs::tom::set<STN_adjusted_proportion>(node,
		 parent_percent *
		 (double)ocs::tom::get<STN_shares>(node) / (double)sibling_shares);
   } else {
      ocs::tom::set<STN_adjusted_proportion>(node, 0);
   }

   for (ocs::tom::Element<ocs::tom::STN> child : ocs::tom::get<STN_children>(node)) {
      calculate_share_percents(child, ocs::tom::get<STN_adjusted_proportion>(node), sum_shares);
   }
}

static double
get_usage_value(const lList *usage, const char *name) {
   ocs::tom::ConstElement<ocs::tom::UA> ue;
   double value = 0;

   if ((ue = ocs::tom::find_first<UA_name>(usage, name))) {
      value = ocs::tom::get<UA_value>(ue);
   }

   return value;
}

static void
print_field(dstring *out, rapidjson::Writer<rapidjson::StringBuffer> *writer, const item_t *field,
            const format_t *format) {
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
            sge_dstring_sprintf_append(out, sge_u32, *static_cast<uint32_t *>(field->val));
         }
         if (writer != nullptr) {
            writer->Uint64(*(uint32_t *) field->val);
         }
         break;
      case DATE_T: {
         uint64_t t = *(uint64_t *) field->val;
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
           const lListElem *parent, const char *parent_node_names) {
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
      /* CS-1239: previously this only read UU_usage_time_stamp, so project
       * leaves (which match a project but no user) always showed usage_time=0.
       * Fall back to PR_usage_time_stamp when the leaf maps to a project,
       * so the displayed timestamp reflects the actual usage age regardless
       * of whether the node is a user, a project, or a UPP leaf (where the
       * user UU stamp wins because it's set by worker booking on every
       * project-job finish too). */
      time_stamp = user != nullptr ? ocs::tom::get<UU_usage_time_stamp>(user) :
                   project != nullptr ? ocs::tom::get<PR_usage_time_stamp>(project) : 0;

      /*
       * we want to name the Root node simply /, instead of /Root 
       * but it is possible to create nodes /project1/Root
       */
      if (parent == nullptr) {
         sge_dstring_sprintf(&node_name_dstring, "/");
      } else {
         sge_dstring_sprintf(&node_name_dstring, "%s/%s", parent_node_names, ocs::tom::get<STN_name>(node));
      }
      node_name = sge_dstring_get_string(&node_name_dstring);

      user_name = user ? ocs::tom::get<UU_name>(user) : "";
      project_name = project ? ocs::tom::get<PR_name>(project) : "";
      shares = ocs::tom::get<STN_shares>(node);
      job_count = ocs::tom::get<STN_job_ref_count>(node);
      level = ocs::tom::get<STN_proportion>(node);
      total = ocs::tom::get<STN_adjusted_proportion>(node);
      lt_share = ocs::tom::get<STN_m_share>(node);
      st_share = ocs::tom::get<STN_adjusted_current_proportion>(node);
      actual_share = ocs::tom::get<STN_actual_proportion>(node);
      combined_usage = ocs::tom::get<STN_combined_usage>(node);

      if (static_cast<const lList *>(ocs::tom::get<STN_children>(node)) == nullptr && user && project) {
         ocs::tom::ConstList<ocs::tom::UPP> projl = ocs::tom::get<UU_project>(user);
         ocs::tom::ConstElement<ocs::tom::UPP> upp;
         if (projl) {
            if ((upp=ocs::tom::find_first<UPP_name>(projl,
                                 ocs::tom::get<PR_name>(project)))) {
               usage = ocs::tom::get<UPP_usage>(upp);
               ltusage = ocs::tom::get<UPP_long_term_usage>(upp);
            }
         }
      } else if (user && strcmp(ocs::tom::get<UU_name>(user), 
                                ocs::tom::get<STN_name>(node))==0) {
         usage = ocs::tom::get<UU_usage>(user);
         ltusage = ocs::tom::get<UU_long_term_usage>(user);
      } else if (project && strcmp(ocs::tom::get<PR_name>(project), 
                                   ocs::tom::get<STN_name>(node))==0) {
         usage = ocs::tom::get<PR_usage>(project);
         ltusage = ocs::tom::get<PR_long_term_usage>(project);
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
            const format_t *format, const char *parent_node_names) {
   const lListElem *user;
   ocs::tom::ConstList<ocs::tom::STN> children = ocs::tom::get<STN_children>(node);
   dstring node_name_dstring = DSTRING_INIT;

   if (!project) {
      project = prj_list_locate(projects, ocs::tom::get<STN_name>(node));
   }

   if (children == nullptr) {
      user = user_list_locate(users, ocs::tom::get<STN_name>(node));
   } else {
      user = nullptr;
   }

   if (group_nodes || (children == nullptr)) {
      print_node(out, jsonBuffer, node, user, project, names, format, parent, parent_node_names);
   }

   for_each_ep_lv(child, children) {
      /* we want to name the Root node simply /, instead of /Root */
      if (parent == nullptr) {
         sge_dstring_sprintf(&node_name_dstring, "");
      } else {
         sge_dstring_sprintf(&node_name_dstring, "%s/%s", parent_node_names, ocs::tom::get<STN_name>(node));
      }
      print_nodes(out, jsonBuffer, child, node, project, users, projects,
                  group_nodes, names, format, sge_dstring_get_string(&node_name_dstring));
   }

   sge_dstring_free(&node_name_dstring);
}

/* ------------- public functions ---------------- */

/**
 * @brief Print a header for the sharetree dump
 *
 * Prints a header for data output using the sge_sharetree_print function.
 *
 * @param out dstring into which data will be written
 * @param format format description
 *
 * @note MT-NOTE: print_hdr() is MT-safe
 *
 * @see #sge_sharetree_print
 */
void print_hdr(dstring *out, const format_t *format) {
   DENTER(TOP_LAYER);

   int i;

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

/**
 * @brief Dump sharetree information to a dstring
 *
 * Dumps information about a sharetree into a given dstring. Information
 * is appended.
 * Outputs information like times, node (user/project) names, configured
 * shares, actually received shares, targeted shares, usage information
 * like cpu, memory and io.
 * It is possible to restrict the number of fields that are output.
 * Header information and formatting can be configured.
 *
 * @param[out] out          dstring into which the text form is written
 * @param[out] jsonBuffer    buffer receiving the JSON form, or nullptr
 * @param[in]  sharetree_in  the share tree to dump
 * @param[in]  users         the user list
 * @param[in]  projects      the project list
 * @param[in]  usersets      the userset list, to resolve the members of a
 *                           node that stands for a userset
 * @param[in]  group_nodes   sum the usage of a node's children into the node
 *                           instead of printing them separately
 * @param[in]  decay_usage   apply the configured usage decay before printing,
 *                           so the values are the ones the policy currently
 *                           works with
 * @param[in]  names         fields to output
 * @param[in]  format        format description
 *
 * @note MT-NOTE: sge_sharetree_print() is  MT-safe
 *
 * @see #print_hdr
 */
void sge_sharetree_print(dstring *out, rapidjson::StringBuffer *jsonBuffer, const lList *sharetree_in,
                         const lList *users, const lList *projects, const lList *usersets, bool group_nodes,
                         bool decay_usage, const char **names, const format_t *format) {
   DENTER(TOP_LAYER);

   lListElem *root;
   uint64_t curr_time = 0;
   lList *sharetree;

   /* 
    * The sharetree might contain "default" nodes which
    * have to be resolved to individual user nodes.
    * This implies modifying the sharetree - so we better create a 
    * copy of the sharetree
    */
   sharetree = lCopyList("copy of sharetree", sharetree_in);
   
   /* Resolve the default users */
   ocs::ShareTree::add_all_auto_users(lFirstRW(sharetree), users, projects, usersets);

   /* 
    * The sharetree calculation and output uses lots of global variables
    * Better control access to them through a mutex.
    */
   sge_mutex_lock("sharetree_printing", __func__, __LINE__, &mtx);

   root = lFirstRW(sharetree);

   calculate_share_percents(root, 1.0, ocs::tom::get<STN_shares>(root));

   if (decay_usage) {
      curr_time = sge_get_gmt64();
   }

   ocs::ShareTree::calc_proportions(sharetree, users, projects, nullptr, curr_time);

   print_nodes(out, jsonBuffer, root, nullptr, nullptr, users, projects, group_nodes, names, format, "");

   sge_mutex_unlock("sharetree_printing", __func__, __LINE__, &mtx);

   /* free our sharetree copy */
   lFreeList(&sharetree);
   DRETURN_VOID;
}
