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
 *  Portions of this code are Copyright 2011 Univa Inc.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Complex entries: the definitions of the resources jobs request
 *
 * A complex entry (`CE_Type`) defines one resource - its type, how a request
 * is compared against what is offered, whether a job may request it, and
 * whether it is consumed. The same element is reused for a *request*, where
 * the value is what the job asked for rather than what is defined.
 *
 * Some resources are built in rather than configured; #host_resource and
 * #queue_resource map those onto the host and queue attributes they are read
 * from.
 *
 * @see sge_centry.h
 */

#include <cstring>
#include <cfloat>
#include <limits>
#include <cinttypes>

#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "cull/cull_list.h"

#include "sched/msg_schedd.h"

#include "sgeobj/sge_resource_quota.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_centry_rsmap.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/cull_parse_util.h"
#include "sgeobj/msg_sgeobjlib.h"

#include "msg_common.h"
#include "uti/sge.h"

#include "sge_centry_rsmap.h"

/// Debug layer the complex entry traces are written to
#define CENTRY_LAYER BASIS_LAYER

/// Number of elements in #host_resource
const int max_host_resources = 29;
/// The built in resources every execution host reports
const struct queue2cmplx host_resource[] = {
        {"arch",             0, 0, 0, ocs::CEntry::Type::STR},
        {"cpu",              0, 0, 0, ocs::CEntry::Type::DOUBLE},
        {"load_avg",         0, 0, 0, ocs::CEntry::Type::DOUBLE},
        {"load_long",        0, 0, 0, ocs::CEntry::Type::DOUBLE},
        {"load_medium",      0, 0, 0, ocs::CEntry::Type::DOUBLE},
        {"load_short",       0, 0, 0, ocs::CEntry::Type::DOUBLE},
        {"mem_free",         0, 0, 0, ocs::CEntry::Type::MEM},
        {"mem_total",        0, 0, 0, ocs::CEntry::Type::MEM},
        {"mem_used",         0, 0, 0, ocs::CEntry::Type::MEM},
        {"min_cpu_inter",    0, 0, 0, ocs::CEntry::Type::TIME},
        {"np_load_avg",      0, 0, 0, ocs::CEntry::Type::DOUBLE},
        {"np_load_long",     0, 0, 0, ocs::CEntry::Type::DOUBLE},
        {"np_load_medium",   0, 0, 0, ocs::CEntry::Type::DOUBLE},
        {"np_load_short",    0, 0, 0, ocs::CEntry::Type::DOUBLE},
        {"num_proc",         0, 0, 0, ocs::CEntry::Type::INT},
        {"swap_free",        0, 0, 0, ocs::CEntry::Type::MEM},
        {"swap_rate",        0, 0, 0, ocs::CEntry::Type::MEM},
        {"swap_rsvd",        0, 0, 0, ocs::CEntry::Type::MEM},
        {"swap_total",       0, 0, 0, ocs::CEntry::Type::MEM},
        {"swap_used",        0, 0, 0, ocs::CEntry::Type::MEM},
        {"virtual_free",     0, 0, 0, ocs::CEntry::Type::MEM},
        {"virtual_total",    0, 0, 0, ocs::CEntry::Type::MEM},
        {"virtual_used",     0, 0, 0, ocs::CEntry::Type::MEM},
        {"display_win_gui",  0, 0, 0, ocs::CEntry::Type::BOOL},
        {LOAD_ATTR_CORES,    0, 0, 0, ocs::CEntry::Type::INT},
        {LOAD_ATTR_SOCKETS,  0, 0, 0, ocs::CEntry::Type::INT},
        {LOAD_ATTR_THREADS,  0, 0, 0, ocs::CEntry::Type::INT},
        {LOAD_ATTR_TOPOLOGY, 0, 0, 0, ocs::CEntry::Type::STR},
        {LOAD_ATTR_DEVICES,  0, 0, 0, ocs::CEntry::Type::RESTR}
};

/// Number of elements in #queue_resource
const int max_queue_resources = 24;
/// The queue attributes that are also visible as complex entries
const struct queue2cmplx queue_resource[] = {
        {"qname",            QU_qname,            0,                   0,              ocs::CEntry::Type::STR},
        {"hostname",         QU_qhostname,        0,                   0,              ocs::CEntry::Type::HOST},
        {"slots",            QU_job_slots,        0,                   0,              ocs::CEntry::Type::INT},
        {"tmpdir",           QU_tmpdir,           0,                   0,              ocs::CEntry::Type::STR},
        {"seq_no",           QU_seq_no,           0,                   0,              ocs::CEntry::Type::INT},
        {"rerun",            QU_rerun,            0,                   0,              ocs::CEntry::Type::BOOL},
        {"calendar",         QU_calendar,         CQ_calendar,         ASTR_value,     ocs::CEntry::Type::STR}, /* value is SGE_STRING */
        {"s_rt",             QU_s_rt,             CQ_s_rt,             ATIME_value,    ocs::CEntry::Type::TIME}, /* value is SGE_STRING */
        {"h_rt",             QU_h_rt,             CQ_h_rt,             ATIME_value,    ocs::CEntry::Type::TIME}, /* value is SGE_STRING */
        {"s_cpu",            QU_s_cpu,            CQ_s_cpu,            ATIME_value,    ocs::CEntry::Type::TIME}, /* value is SGE_STRING */
        {"h_cpu",            QU_h_cpu,            CQ_h_cpu,            ATIME_value,    ocs::CEntry::Type::TIME}, /* value is SGE_STRING */
        {"s_fsize",          QU_s_fsize,          CQ_s_data,           AMEM_value,     ocs::CEntry::Type::MEM}, /* value is SGE_STRING */
        {"h_fsize",          QU_h_fsize,          CQ_h_fsize,          AMEM_value,     ocs::CEntry::Type::MEM}, /* value is SGE_STRING */
        {"s_data",           QU_s_data,           CQ_s_data,           AMEM_value,     ocs::CEntry::Type::MEM}, /* value is SGE_STRING */
        {"h_data",           QU_h_data,           CQ_h_data,           AMEM_value,     ocs::CEntry::Type::MEM}, /* value is SGE_STRING */
        {"s_stack",          QU_s_stack,          CQ_s_stack,          AMEM_value,     ocs::CEntry::Type::MEM}, /* value is SGE_STRING */
        {"h_stack",          QU_h_stack,          CQ_h_stack,          AMEM_value,     ocs::CEntry::Type::MEM}, /* value is SGE_STRING */
        {"s_core",           QU_s_core,           CQ_s_core,           AMEM_value,     ocs::CEntry::Type::MEM}, /* value is SGE_STRING */
        {"h_core",           QU_h_core,           CQ_h_core,           AMEM_value,     ocs::CEntry::Type::MEM}, /* value is SGE_STRING */
        {"s_rss",            QU_s_rss,            CQ_s_rss,            AMEM_value,     ocs::CEntry::Type::MEM}, /* value is SGE_STRING */
        {"h_rss",            QU_h_rss,            CQ_h_rss,            AMEM_value,     ocs::CEntry::Type::MEM}, /* value is SGE_STRING */
        {"s_vmem",           QU_s_vmem,           CQ_s_vmem,           AMEM_value,     ocs::CEntry::Type::MEM}, /* value is SGE_STRING */
        {"h_vmem",           QU_h_vmem,           CQ_h_vmem,           AMEM_value,     ocs::CEntry::Type::MEM}, /* value is SGE_STRING */
        {"min_cpu_interval", QU_min_cpu_interval, CQ_min_cpu_interval, AINTER_value,   ocs::CEntry::Type::TIME}  /* value is SGE_STRING */
};

/**
 * @brief Look a built in resource up in #host_resource or #queue_resource
 *
 * @param name the resource name, not its shortcut
 * @param queue true to search the queue resources, false for the host ones
 * @param[out] field receives the attribute within the queue instance; may be nullptr
 * @param[out] cqfld receives the cluster queue attribute; may be nullptr
 * @param[out] valfld receives the value field within the cluster queue sublist; may be nullptr
 * @param[out] type receives the resource's type; may be nullptr
 * @return the index in the array, or -1 when the name is not a built in resource
 */
int get_rsrc(const char *name, bool queue, int *field, int *cqfld, int *valfld, ocs::CEntry::Type *type) {
   int pos = 0;
   const struct queue2cmplx *rlist;
   int nitems;

   if (queue) {
      rlist = &queue_resource[0];
      nitems = max_queue_resources;
   } else {
      rlist = &host_resource[0];
      nitems = max_host_resources;
   }

   for (; pos < nitems; pos++) {
      if (strcmp(name, rlist[pos].name) == 0) {
         if (field) *field = rlist[pos].field;
         if (cqfld) *cqfld = rlist[pos].cqfld;
         if (valfld) *valfld = rlist[pos].valfld;
         if (type) *type = rlist[pos].type;
         return 0;
      }
   }

   return -1;
}

/**
 * @brief Fill and check the attribute
 *
 * fill and check the attribute
 *
 * @param this_elem CE_Type, this object will be checked
 * @param answer_list answer list
 * @param allow_empty_boolean true replaces a nullptr value of a boolean
 *                            attribute with "true"; false treats it as an error
 * @param allow_neg_consumable true allows a negative value for a consumable
 *                             resource; false makes the function return -1 for one
 *
 * @return 1 on error an error message will be written into SGE_EVENT
 */
int
centry_fill_and_check(lListElem *this_elem, lList **answer_list, bool allow_empty_boolean,
                      bool allow_neg_consumable) {
   DENTER(CENTRY_LAYER);
   static char tmp[1000];
   const char *name, *s;
   ocs::CEntry::Type type;
   double dval;
   int ret, allow_infinity;

   name = lGetString(this_elem, CE_name);
   s = lGetString(this_elem, CE_stringval);
   DPRINTF("   ===> centry_fill_and_check(%s, %s)\n", name, s);
   /* allow infinity for non-consumables only */
   allow_infinity = (lGetUlong(this_elem, CE_consumable) != CONSUMABLE_NO) ? 0 : 1;

   if (s == nullptr) {
      if (allow_empty_boolean && static_cast<ocs::CEntry::Type>(lGetUlong(this_elem, CE_valtype)) == ocs::CEntry::Type::BOOL) {
         lSetString(this_elem, CE_stringval, "TRUE");
         s = lGetString(this_elem, CE_stringval);
      } else {
/*          ERROR(MSG_CPLX_VALUEMISSING_S, name); */
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_CPLX_VALUEMISSING_S, name);
         DRETURN(-1);
      }
   }

   switch (type = static_cast<ocs::CEntry::Type>(lGetUlong(this_elem, CE_valtype))) {
      case ocs::CEntry::Type::RSMAP:
      case ocs::CEntry::Type::INT:
      case ocs::CEntry::Type::TIME:
      case ocs::CEntry::Type::MEM:
      case ocs::CEntry::Type::BOOL:
      case ocs::CEntry::Type::DOUBLE:
         if (!extended_parse_ulong_val(&dval, nullptr, type, s, tmp, sizeof(tmp)-1, allow_infinity, false)) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_ATTRIB_XISNOTAY_SS, name, tmp);
            DRETURN(-1);
         }
         DPRINTF("   ===> centry_fill_and_check(%s, %s), dval = %f\n", name, s, dval);
         lSetDouble(this_elem, CE_doubleval, dval);

         /* Neither MEM nor TIME values are normalized here.
          *
          * CS-2014 normalized both, so that job requests differing only in the
          * notation used ("80G" and "85899345920", "1:00:00" and "3600") end up
          * in the same category. But this function rewrites CE_stringval, i.e.
          * the value that is STORED and that every plain client prints back.
          * Plain output has to show what the user wrote -- qstat, qhost, qrsh
          * and qquota do not normalize, and only the JSON and XML renderings may
          * show a normalized value, and only when it is asked for.
          *
          * The normalization therefore belongs to the one place that needs a
          * canonical form: sge_unparse_resource_list_dstring() in sge_job.cc,
          * which builds the category string from CE_doubleval.
          *
          * CE_doubleval is set above for every numeric type, so anything that
          * needs the value rather than its notation reads that field and is
          * unaffected by this.
          */

         /* also the CE_defaultval must be parsable for numeric types */
         if ((s=lGetString(this_elem, CE_defaultval))
            && !parse_ulong_val(&dval, nullptr, type, s, tmp, sizeof(tmp)-1)) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_CPLX_WRONGTYPE_SSS, name, s, tmp);
            DRETURN(-1);
         }

         /* negative values are not allowed for consumable attributes */
         if (!allow_neg_consumable && (lGetUlong(this_elem, CE_consumable) != CONSUMABLE_NO)
             && lGetDouble(this_elem, CE_doubleval) < (double)0.0) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_CPLX_ATTRIBISNEG_S, name);

            DRETURN(-1);
         }
         break;
      case ocs::CEntry::Type::HOST:
         /* resolve hostname and store it */
         ret = sge_resolve_host(this_elem, CE_stringval);
         if (ret != CL_RETVAL_OK) {
            if (ret == CL_RETVAL_GETHOSTNAME_ERROR) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_SGETEXT_CANTRESOLVEHOST_S, s);
            } else {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_SGETEXT_INVALIDHOST_S, s);
            }
            DRETURN(-1);
         }
         break;
      case ocs::CEntry::Type::STR:
      case ocs::CEntry::Type::CSTR:
      case ocs::CEntry::Type::RESTR:
         /* no restrictions - so everything is ok */
         break;

      default:
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 MSG_SGETEXT_UNKNOWN_ATTR_TYPE_U, type);
         DRETURN(-1);
   }

   DRETURN(0);
}

/**
 * @brief The symbol a relational operator is written as
 *
 * @param op one of the `CMPLX*_OP` values
 * @return the symbol, e.g. `>=`
 */
const char *
map_op2str(uint32_t op) {
   static const char *opv[] = {
      "??",
      "==",  /* CMPLXEQ_OP */
      ">=",  /* CMPLXGE_OP */
      ">",   /* CMPLXGT_OP */
      "<",   /* CMPLXLT_OP */
      "<=",  /* CMPLXLE_OP */
      "!=",  /* CMPLXNE_OP */
      "EXCL" /* CMPLXEXCL_OP */
   };

   if (op < CMPLXEQ_OP || op > CMPLXEXCL_OP) {
      op = 0;
   }
   return opv[op];
}

/**
 * @brief The word a requestable setting is written as
 *
 * @param op one of the `REQU_*` values
 * @return the word, e.g. `FORCED`
 */
const char *
map_req2str(uint32_t op) {
   static const char *opv[] = {
      "??",
      "NO",       /* REQU_NO */
      "YES",      /* REQU_YES */
      "FORCED",   /* REQU_FORCED */
   };

   if (op < REQU_NO || op > REQU_FORCED) {
      op = 0;
   }
   return opv[op];
}

/**
 * @brief Map to consumable string
 *
 * maps int representation of CONSUMABLE to string
 *
 * @param op CONSUMABLE_*
 *
 * @return string representation of consumable definition
 *
 * @note MT-NOTE: map_consumable2str() is not safe
 */
const char *map_consumable2str(uint32_t op) {
   static const char *opv[] = {
      "NO",       /* CONSUMABLE_NO */
      "YES",      /* CONSUMABLE_YES */
      "JOB",      /* CONSUMABLE_JOB */
      "HOST",     /* CONSUMABLE_HOST */
   };

   if (op > CONSUMABLE_HOST) {
      op = CONSUMABLE_NO;
   }
   return opv[op];
}

/**
 * @brief The name a resource type is written as
 *
 * @param type the type
 * @return the name, e.g. `MEMORY`
 */
const char *
map_type2str(ocs::CEntry::Type type) {
   static const char *typev[] = {
      "??????",
      "INT",      /*  1 TYPE_INT */
      "STRING",   /*  2 TYPE_STR */
      "TIME",     /*  3 TYPE_TIM */
      "MEMORY",   /*  4 TYPE_MEM */
      "BOOL",     /*  5 TYPE_BOO */
      "CSTRING",  /*  6 TYPE_CSTR */
      "HOST",     /*  7 TYPE_HOST */
      "DOUBLE",   /*  8 TYPE_DOUBLE */
      "RESTRING", /*  9 TYPE_RESTR */
      "RSMAP",    /* 10 TYPE_RSMAP */

      "TYPE_ACC", /* 11 TYPE_ACC */
      "TYPE_LOG"  /* 12 TYPE_LOG */
   };

   if (type < ocs::CEntry::Type::FIRST || type > ocs::CEntry::Type::TYPE_LAST) {
      type = ocs::CEntry::Type::NONE;
   }
   return typev[static_cast<uint32_t>(type)];
}

/**
 * @brief Create a preinitialized centry element
 *
 * Create a preinitialized centry element with the given "name".
 *
 * @param answer_list AN_Type
 * @param name full name
 *
 * @return CE_Type element
 */
lListElem *
centry_create(lList **answer_list, const char *name) {
   DENTER(CENTRY_LAYER);

   lListElem *ret = nullptr;  /* CE_Type */

   if (name != nullptr) {
      ret = lCreateElem(CE_Type);
      if (ret != nullptr) {
         lSetString(ret, CE_name, name);
         lSetString(ret, CE_shortcut, name);
         lSetUlong(ret, CE_valtype, static_cast<uint32_t>(ocs::CEntry::Type::INT));
         lSetUlong(ret, CE_relop, CMPLXLE_OP);
         lSetUlong(ret, CE_requestable, REQU_NO);
         lSetUlong(ret, CE_consumable, CONSUMABLE_NO);
         lSetString(ret, CE_defaultval, "0");
         lSetString(ret, CE_urgency_weight, "0");
      } else {
         answer_list_add_sprintf(answer_list, STATUS_EMALLOC,
                                 ANSWER_QUALITY_ERROR,
                                 MSG_MEM_MEMORYALLOCFAILED_S, __func__);
      }
   } else {
      answer_list_add_sprintf(answer_list, STATUS_ERROR1, ANSWER_QUALITY_ERROR,
                              MSG_INAVLID_PARAMETER_IN_S, __func__);
   }
   DRETURN(ret);
}

/**
 * @brief Is centry element referenced?
 *
 * Is the centry element referenced in a sublist of
 * "master_queue_list", "master_exechost_list" or
 * "master_sconf_list".
 *
 * @param centry CE_Type
 * @param answer_list AN_Type
 * @param master_cqueue_list CQ_Type
 * @param master_exechost_list EH_Type
 * @param master_rqs_list RQS_Type
 *
 * @return true or false
 */
bool
centry_is_referenced(const lListElem *centry, lList **answer_list,
                     const lList *master_cqueue_list,
                     const lList *master_exechost_list,
                     const lList *master_rqs_list) {
   DENTER(CENTRY_LAYER);

   bool ret = false;
   const char *centry_name = lGetString(centry, CE_name);

   if (sconf_is_centry_referenced(centry)) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                              ANSWER_QUALITY_INFO,
                              MSG_CENTRYREFINSCONF_S, centry_name);
      ret = true;
   }
   if (!ret) {
      const lListElem *cqueue = nullptr, *cel = nullptr;

      /* fix for bug 6422335
       * check the cq configuration for centry references instead of qinstances
       */
      for_each_ep(cqueue, master_cqueue_list) {
         for_each_ep(cel, lGetList(cqueue, CQ_consumable_config_list)) {
            if (lGetSubStr(cel, CE_name, centry_name, ACELIST_value)) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                       ANSWER_QUALITY_INFO,
                                       MSG_CENTRYREFINQUEUE_SS,
                                       centry_name,
                                       lGetString(cqueue, CQ_name));
               ret = true;
               break;
            }
         }
         if (ret) {
            break;
         }
      }
   }
   if (!ret) {
      const lListElem *host = nullptr;    /* EH_Type */

      for_each_ep(host, master_exechost_list) {
         if (host_is_centry_referenced(host, centry)) {
            const char *host_name = lGetHost(host, EH_name);

            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                    ANSWER_QUALITY_INFO,
                                    MSG_CENTRYREFINHOST_SS,
                                    centry_name, host_name);
            ret = true;
            break;
         }
      }
   }
   if (!ret) {
      const lListElem *rqs = nullptr;
      for_each_ep(rqs, master_rqs_list) {
         if (sge_centry_referenced_in_rqs(rqs, centry)) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                    ANSWER_QUALITY_INFO,
                                    MSG_CENTRYREFINRQS_SS,
                                    centry_name, lGetString(rqs, RQS_name));
            ret = true;
            break;
         }
      }
   }

   DRETURN(ret);
}

/**
 * @brief Print to dstring
 *
 * Print resource string (memory, time) to dstring.
 *
 * @param this_elem CE_Type
 * @param string dynamic string
 *
 * @return error state true  - success false - error
 *
 * @note MT-NOTE: centry_print_resource_to_dstring() is MT safe
 */
bool
centry_print_resource_to_dstring(const lListElem *this_elem, dstring *string) {
   DENTER(CENTRY_LAYER);

   if (this_elem != nullptr && string != nullptr) {
      const auto type = static_cast<ocs::CEntry::Type>(lGetUlong(this_elem, CE_valtype));
      const double val = lGetDouble(this_elem, CE_doubleval);

      switch (type) {
         case ocs::CEntry::Type::TIME:
            double_print_time_to_dstring(val, string);
            break;
         case ocs::CEntry::Type::MEM:
            double_print_memory_to_dstring(val, string);
            break;
         default:
            double_print_to_dstring(val, string);
            break;
      }
   }
   DRETURN(true);
}

/**
 * @brief Find Centry element
 *
 * Find CEntry element with "name" in "this_list".
 *
 * @param this_list CE_Type list
 * @param name name of an CE_Type entry
 *
 * @return CE_Type element
 */
lListElem *
centry_list_locate(const lList *this_list, const char *name) {
   DENTER(CENTRY_LAYER);

   lListElem *ret = nullptr;   /* CE_Type */

   if (this_list != nullptr && name != nullptr) {
      ret = lGetElemStrRW(this_list, CE_name, name);
      if (ret == nullptr) {
         ret = lGetElemStrRW(this_list, CE_shortcut, name);
      }
   }
   DRETURN(ret);
}

/**
 * @brief Sort a CE_Type list
 *
 * @param this_list CE_Type list
 *
 * @return error state true  - success false - error
 */
bool
centry_list_sort(lList *this_list) {
   DENTER(CENTRY_LAYER);

   bool ret = true;

   if (this_list != nullptr) {
      lSortOrder *order = nullptr;

      order = lParseSortOrderVarArg(lGetListDescr(this_list), "%I+", CE_name);
      lSortList(this_list, order);
      lFreeSortOrder(&order);
   }
   DRETURN(ret);
}

/**
 * @brief Initialize double from string
 *
 * Initialize all double values contained in "this_list"
 *
 * @param this_list CE_Type list
 *
 * @note bool - true
 */
void
centry_list_init_double(const lList *this_list) {
   DENTER(CENTRY_LAYER);
   for_each_rw_lv (centry, this_list) {
      double new_val = 0.0; // parse_ulong_val will not set it for all data types!
      parse_ulong_val(&new_val, nullptr, static_cast<ocs::CEntry::Type>(lGetUlong(centry, CE_valtype)),
                      lGetString(centry, CE_stringval), nullptr, 0);
      lSetDouble(centry, CE_doubleval, new_val);
   }
   DRETURN_VOID;
}

/**
 * @brief Fills and checks list of complex entries
 *
 * This function fills a given list of complex entries with missing
 * attributes which can be found in the complex. It checks also
 * wether the given in the centry_list-List are valid.
 *
 * @param this_list resources as complex list CE_Type
 * @param answer_list answer list
 * @param master_centry_list the global complex list
 * @param allow_non_requestable needed for qstat -l or qmon customize dialog
 * @param allow_empty_boolean true replaces a nullptr value of a boolean
 *                            attribute with "true"; false treats it as an error
 * @param allow_neg_consumable true allows a negative value for a consumable
 *                             resource; false makes the function return -1 for one
 *
 * @return error 0 on success -1 on error an error message will be written into SGE_EVENT
 */
int
centry_list_fill_request(const lList *this_list, lList **answer_list, const lList *master_centry_list,
                         bool allow_non_requestable, bool allow_empty_boolean,
                         bool allow_neg_consumable) {
   DENTER(TOP_LAYER);
   lListElem *cep = nullptr;

   for_each_rw_lv(entry, this_list) {
      const char *name = lGetString(entry, CE_name);

      cep = centry_list_locate(master_centry_list, name);
      if (cep != nullptr) {
         uint32_t requestable = lGetUlong(cep, CE_requestable);
         if (!allow_non_requestable && requestable == REQU_NO) {
/*             ERROR(MSG_SGETEXT_RESOURCE_NOT_REQUESTABLE_S, name); */
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_SGETEXT_RESOURCE_NOT_REQUESTABLE_S, name);
            DRETURN(-1);
         }

         /* replace name in request/threshold/consumable list,
            it may have been a shortcut */
         lSetString(entry, CE_name, lGetString(cep, CE_name));

         /* we found the right complex attrib */
         /* so we know the type of the requested data */
         lSetUlong(entry, CE_valtype, lGetUlong(cep, CE_valtype));

         /* we also know whether it is a consumable attribute */
         lSetUlong(entry, CE_consumable, lGetUlong(cep, CE_consumable));

         if (centry_fill_and_check(entry, answer_list, allow_empty_boolean, allow_neg_consumable)) {
            /* no error msg here - centry_fill_and_check() makes it */
            DRETURN(-1);
         }

         /* RSMAP entries may carry per-instance characteristics on
            RESL_properties. Resolve them against the master centry list,
            populate valtype, and type-check each value. */
         if (static_cast<ocs::CEntry::Type>(lGetUlong(entry, CE_valtype)) == ocs::CEntry::Type::RSMAP) {
            if (!centry_check_rsmap_characteristics(answer_list, entry, master_centry_list)) {
               DRETURN(-1);
            }
         }
      } else {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_SGETEXT_UNKNOWN_RESOURCE_S, name);
         DRETURN(-1);
      }
   }

   DRETURN(0);
}

/**
 * @brief fill in configuration attributes into a list of complex attributes
 *
 * The function makes sure that all configuration attributes of objects in a CE_Type list
 * are properly filled in.
 * The actual configuration values are taken from the complex definition (the master_centry_list).
 *
 * @param centry_list
 * @param master_centry_list
 */
void
centry_list_fill_config(lList *centry_list, const lList *master_centry_list) {
   for_each_rw_lv(centry, centry_list) {
      const lListElem *master_centry = centry_list_locate(master_centry_list, lGetString(centry, CE_name));
      if (master_centry != nullptr) {
         lSetString(centry, CE_name, lGetString(master_centry, CE_name));
         lSetString(centry, CE_shortcut, lGetString(master_centry, CE_shortcut));
         lSetUlong(centry, CE_valtype, lGetUlong(master_centry, CE_valtype));
         lSetUlong(centry, CE_relop, lGetUlong(master_centry, CE_relop));
         lSetUlong(centry, CE_requestable, lGetUlong(master_centry, CE_requestable));
         lSetUlong(centry, CE_consumable, lGetUlong(master_centry, CE_consumable));
         lSetString(centry, CE_defaultval, lGetString(master_centry, CE_defaultval));
         lSetString(centry, CE_urgency_weight, lGetString(master_centry, CE_urgency_weight));
      }
   }
}

bool
/**
 * @brief May a job request a specific queue?
 *
 * Decided by whether the `qname` complex entry is requestable, which an
 * administrator can turn off to stop users from bypassing the scheduler.
 *
 * @param this_list the complex entries to look in
 * @return true when `qname` exists and is requestable
 */
centry_list_are_queues_requestable(const lList *this_list) {
   DENTER(CENTRY_LAYER);

   bool ret = false;

   if (this_list != nullptr) {
      lListElem *centry = centry_list_locate(this_list, "qname");

      if (centry != nullptr) {
         ret = (lGetUlong(centry, CE_requestable) != REQU_NO) ? true : false;
      }
   }
   DRETURN(ret);
}

const char *
/**
 * @brief Render a complex entry list into a dstring
 *
 * @param this_list the entries to render
 * @param[out] string receives the text, appended
 * @return the resulting text
 */
centry_list_append_to_dstring(const lList *this_list, dstring *string) {
   DENTER(CENTRY_LAYER);

   const char *ret = nullptr;

   if (string != nullptr) {
      bool printed = false;

      const lListElem *elem;
      for_each_ep(elem, this_list) {
         if (printed) {
            sge_dstring_append(string, ",");
         }
         sge_dstring_sprintf_append(string, "%s=", lGetString(elem, CE_name));
         if (lGetString(elem, CE_stringval) != nullptr) {
            sge_dstring_append(string, lGetString(elem, CE_stringval));
         }
         printed = true;
      }
      if (!printed) {
         sge_dstring_append(string, "NONE");
      }
      ret = sge_dstring_get_string(string);
   }

   DRETURN(ret);
}

/* CLEANUP: should be replaced by centry_list_append_to_dstring() */
int
/**
 * @brief Render a complex entry list into a fixed size buffer
 *
 * @param this_list the entries to render
 * @param[out] buff receives the text
 * @param max_len the size of `buff`
 * @return true when the text fitted
 */
centry_list_append_to_string(lList *this_list, char *buff, uint32_t max_len) {
   DENTER(TOP_LAYER);

   int attr_fields[] = {CE_name, CE_stringval, 0};
   const char *attr_delis[] = {"=", ",", "\n"};
   int ret;

   if (buff)
      buff[0] = '\0';

   lPSortList(this_list, "%I+", CE_name);

   ret = uni_print_list(nullptr, buff, max_len, this_list, attr_fields, attr_delis, 0);
   if (ret) {
      DRETURN(ret);
   }

   DRETURN(0);
}

/* CLEANUP: add answer_list remove SGE_EVENT */
/*
 * NOTE
 *    MT-NOTE: centry_list_parse_from_string() is MT safe
 */
/**
 * @brief Parse a `name=value,name=value` list into complex entries
 *
 * @param[in,out] complex_attributes the list the parsed entries are added to;
 *                                   a new list is created when it is nullptr
 * @param str the text to parse
 * @param check_value true to reject a value that does not fit the resource's type
 * @return the resulting list, or nullptr when the text could not be parsed
 *
 * @note MT-NOTE: centry_list_parse_from_string() is MT safe
 */
lList *
centry_list_parse_from_string(lList *complex_attributes,
                              const char *str, bool check_value) {
   DENTER(TOP_LAYER);

   char *cp;
   struct saved_vars_s *context = nullptr;

   /* allocate space for attribute list if no list is passed */
   if (complex_attributes == nullptr) {
      if ((complex_attributes = lCreateList("qstat_l_requests", CE_Type)) == nullptr) {
         ERROR(SFNMAX, MSG_PARSE_NOALLOCATTRLIST);
         DRETURN(nullptr);
      }
   }

   /* str now points to the attr=value pairs */
   while ((cp = sge_strtok_r(str, ", ", &context))) {
      lListElem *complex_attribute = nullptr;
      const char *attr = nullptr;
      char *value = nullptr;

      str = nullptr;       /* for the next strtoks */

      /*
      ** recursive strtoks did not work
      */
      attr = cp;
      if ((value = strchr(cp, '='))) {
         *value++ = 0;
      }

      if (attr == nullptr || *attr == '\0') {
         ERROR(MSG_SGETEXT_UNKNOWN_RESOURCE_S, "");
         lFreeList(&complex_attributes);
         sge_free_saved_vars(context);
         DRETURN(nullptr);
      }

      /*
       * If no default value was specified then use TRUE
       */
      if (!check_value && value == nullptr) {
         value = (char*)TRUE_STR;
      } else if (check_value && (value == nullptr || *value == '\0')) {
         ERROR(MSG_CPLX_VALUEMISSING_S, attr);
         lFreeList(&complex_attributes);
         sge_free_saved_vars(context);
         DRETURN(nullptr);
      }

      /* create new element, fill in the values and append it */
      complex_attribute = lGetElemStrRW(complex_attributes, CE_name, attr);
      if (complex_attribute == nullptr) {
         complex_attribute = lCreateElem(CE_Type);
         lSetString(complex_attribute, CE_name, attr);
         lAppendElem(complex_attributes, complex_attribute);
      }

      lSetString(complex_attribute, CE_stringval, value);
   }

   sge_free_saved_vars(context);

   DRETURN(complex_attributes);
}

void
/**
 * @brief Collapse repeated requests for the same resource
 *
 * The last value for a name wins, which is what a user writing the same
 * `-l` option twice expects.
 *
 * @param[in,out] this_list the request list to compress
 */
centry_list_remove_duplicates(lList *this_list) {
   DENTER(TOP_LAYER);
   cull_compress_definition_list(this_list, CE_name, CE_stringval, 0);
   DRETURN_VOID;
}


/**
 * @brief Validates a element and checks for duplicates
 *
 * Checks weather the configuration within the new centry is okay or not.
 * A centry is valid, when it satisfies the following rules:
 *     name 	  : has to be unique
 *     Short cu  : has to be unique
 *     Type	     : every type from the list (string, host, cstring, int,
 *                                           double, boolean, memory, time)
 *     Consumable : can only be defined for: int, double, memory, time, RSMAP
 *     Relational operator:
 *     - for consumables:              only <=
 *     - for non consumables:
 *        - string, host, cstring:     only ==, !=
 *        - boolean:	                  only ==
 *        - int, double, memory, time: ==, !=, <=, <, =>, >
 *     Requestable	   : for all attribute
 *     default value 	: only for consumables
 * A RSMAP must be a consumable.
 * The type for build in attributes is not allowed to be changed!
 * When no centy list is passed in, the check for uniqie name and
 * short cuts is skipt.
 *
 * @param centry the centry list, which should be validated
 * @param centry_list if not null, the function checks, if the centry element is already in the list
 * @param answer_list contains the error messages
 *
 * @return error (the anwer_list contains the error message) true - okay
 */
bool centry_elem_validate(lListElem *centry, const lList *centry_list,
                          lList **answer_list) {
   DENTER(TOP_LAYER);
   uint32_t relop = lGetUlong(centry, CE_relop);
   auto type = static_cast<ocs::CEntry::Type>(lGetUlong(centry, CE_valtype));
   const char *attrname = lGetString(centry, CE_name);
   const char *temp;
   bool ret = true;

   switch (type) {
      case ocs::CEntry::Type::RSMAP:
      case ocs::CEntry::Type::INT:
      case ocs::CEntry::Type::MEM:
      case ocs::CEntry::Type::DOUBLE:
      case ocs::CEntry::Type::TIME:
         if (relop == CMPLXEXCL_OP) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_MUST_BOOL_TO_BE_EXCL_S, attrname);
            ret = false;
         }
         break;

      case ocs::CEntry::Type::STR:
      case ocs::CEntry::Type::CSTR:
      case ocs::CEntry::Type::RESTR:
      case ocs::CEntry::Type::HOST:
         if (!(relop == CMPLXEQ_OP || relop == CMPLXNE_OP)) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_INVALID_CENTRY_TYPE_RELOP_S, attrname);
            ret = false;
         }
         if (lGetUlong(centry, CE_consumable)) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_INVALID_CENTRY_CONSUMABLE_TYPE_SS, attrname,
                                    map_type2str(type));
            ret = false;
         }
         break;

      case ocs::CEntry::Type::BOOL:
         if (relop != CMPLXEQ_OP && relop != CMPLXEXCL_OP) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_INVALID_CENTRY_TYPE_RELOP_S, attrname);
            ret = false;
         }
         if (lGetUlong(centry, CE_consumable) && relop != CMPLXEXCL_OP) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_INVALID_CENTRY_EXCL_S, attrname,
                                    map_type2str(type));
            ret = false;
         }
         if (relop == CMPLXEXCL_OP && !lGetUlong(centry, CE_consumable)) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_EXCL_MUST_BE_CONSUMABLE_S, attrname,
                                    map_type2str(type));
            ret = false;
         }

         break;

      default: /* error unknown type */
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 MSG_SGETEXT_UNKNOWN_ATTR_TYPE_U, type);
         ret = false;
         break;
   }

   {
      double dval;
      char error_msg[200];
      error_msg[0] = '\0';

      /* donot allow REQUESTABLE for "tmpdir" attribute, refer CR6650497 */
      if (!strcmp(attrname, "tmpdir") && lGetUlong(centry, CE_requestable) != REQU_NO) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_CENTRY_NOTREQUESTABLE_S, attrname);
         ret = false;

      }

      if (lGetUlong(centry, CE_consumable)) {

         if (relop != CMPLXEXCL_OP && relop != CMPLXLE_OP) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                    MSG_INVALID_CENTRY_CONSUMABLE_RELOP_S, attrname);
            ret = false;
         }

         if (lGetUlong(centry, CE_requestable) == REQU_NO) {
            if (!parse_ulong_val(&dval, nullptr, type, lGetString(centry, CE_defaultval), error_msg, 199)) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                       MSG_INVALID_CENTRY_PARSE_DEFAULT_SS, attrname, error_msg);
               ret = false;
            }
            if (dval == 0) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                       MSG_INVALID_CENTRY_CONSUMABLE_REQ1_S, attrname);
               ret = false;
            }
         } else if (lGetUlong(centry, CE_requestable) == REQU_FORCED) {
            if (!parse_ulong_val(&dval, nullptr, type, lGetString(centry, CE_defaultval), error_msg, 199)) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                       MSG_INVALID_CENTRY_PARSE_DEFAULT_SS, attrname, error_msg);
               ret = false;
            }
            if (dval != 0) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                       MSG_INVALID_CENTRY_CONSUMABLE_REQ2_S, attrname);
               ret = false;
            }
         }
      } else if ((temp = lGetString(centry, CE_defaultval))) {

         switch (type) {
            case ocs::CEntry::Type::RSMAP:
            case ocs::CEntry::Type::INT:
            case ocs::CEntry::Type::TIME:
            case ocs::CEntry::Type::MEM:
            case ocs::CEntry::Type::BOOL:
            case ocs::CEntry::Type::DOUBLE:

               if (!parse_ulong_val(&dval, nullptr, type, temp, error_msg, 199)) {
                  answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                          MSG_INVALID_CENTRY_PARSE_DEFAULT_SS, attrname, error_msg);
                  ret = false;
               }

               /* accept non-zero default values for consumables only */
               if (dval != 0) {
                  answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                          MSG_INVALID_CENTRY_DEFAULT_S, attrname);
                  ret = false;
               }

               break;
            case ocs::CEntry::Type::HOST:
            case ocs::CEntry::Type::STR:
            case ocs::CEntry::Type::RESTR:
            case ocs::CEntry::Type::CSTR:
               if (strcasecmp(temp, "NONE") != 0) {
                  answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                          MSG_INVALID_CENTRY_DEFAULT_S, attrname);
                  ret = false;
               }
               break;
            default:
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                       MSG_SGETEXT_UNKNOWN_ATTR_TYPE_U, type);
               ret = false;
         }
      }

      /* verify urgency always */
      if ((temp = lGetString(centry, CE_urgency_weight))) {
         switch (type) {
            case ocs::CEntry::Type::RSMAP:
            case ocs::CEntry::Type::INT:
            case ocs::CEntry::Type::TIME:
            case ocs::CEntry::Type::MEM:
            case ocs::CEntry::Type::BOOL:
            case ocs::CEntry::Type::DOUBLE:
            case ocs::CEntry::Type::HOST:
            case ocs::CEntry::Type::STR:
            case ocs::CEntry::Type::CSTR:
            case ocs::CEntry::Type::RESTR:
               if (!parse_ulong_val(&dval, nullptr, ocs::CEntry::Type::DOUBLE, temp, error_msg, 199)) {
                  answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                          MSG_INVALID_CENTRY_PARSE_URGENCY_SS, attrname, error_msg);
                  ret = false;
               }
               break;

            default:
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                       MSG_SGETEXT_UNKNOWN_ATTR_TYPE_U, type);
               ret = false;
         }
      }
   }

   if (type == ocs::CEntry::Type::RSMAP) {
      ret = centry_check_rsmap(answer_list, lGetUlong(centry, CE_consumable), attrname);
   }


   /* check if it's a built-in value and if the type is correct */
   {
      int i;
      auto type = static_cast<ocs::CEntry::Type>(lGetUlong(centry, CE_valtype));
      for (i = 0; i < max_queue_resources; i++) {
         if (strcmp(queue_resource[i].name, attrname) == 0 &&
             queue_resource[i].type != type) {
            if ((queue_resource[i].type != ocs::CEntry::Type::STR && queue_resource[i].type != ocs::CEntry::Type::CSTR &&
                 queue_resource[i].type != ocs::CEntry::Type::RESTR) ||
                (type != ocs::CEntry::Type::CSTR && type != ocs::CEntry::Type::RESTR && type !=ocs::CEntry::Type:: STR)) {

               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                       MSG_INVALID_CENTRY_TYPE_CHANGE_S, attrname);

               ret = false;
               break;
            }
         }
      }

      for (i = 0; i < max_host_resources; i++) {
         if (strcmp(host_resource[i].name, attrname) == 0 &&
             host_resource[i].type != type) {

            if ((host_resource[i].type != ocs::CEntry::Type::STR && host_resource[i].type != ocs::CEntry::Type::CSTR &&
                 host_resource[i].type != ocs::CEntry::Type::RESTR) ||
                (type != ocs::CEntry::Type::CSTR && type != ocs::CEntry::Type::RESTR && type != ocs::CEntry::Type::STR)) {

               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                       MSG_INVALID_CENTRY_TYPE_CHANGE_S, attrname);

               ret = false;
               break;
            }
         }
      }
   }

   /* check for duplicates */
   if (centry_list) {
      const char *shortcut = lGetString(centry, CE_shortcut);
      const lListElem *ce1 = centry_list_locate(centry_list, attrname);
      const lListElem *ce2 = centry_list_locate(centry_list, shortcut);

      /*
       * if we already have a centry with this name or shortcut,
       * that is not the current centry -> cannot add/mod this one
       */
      if ((ce1 != nullptr && ce1 != centry) ||
          (ce2 != nullptr && ce2 != centry)) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                 ANSWER_QUALITY_ERROR,
                                 MSG_ANSWER_COMPLEXXALREADYEXISTS_SS,
                                 attrname, shortcut);
         ret = false;
      }
   }
   DRETURN(ret);
}

/**
 * @brief Compute urgency for a particular resource
 *
 * The urgency contribution for a particular resource 'name' is determined
 * based on the 'slot' amount and using 'value' as per slot request. The
 * urgency value in the 'centry' element is used.
 *
 * @param slots The slot amount assumed.
 * @param name The resource name.
 * @param value The per slot request.
 * @param centry The centry element (CE_Type)
 *
 * @return The resulting urgency contribution
 *
 * @note MT-NOTES: centry_urgency_contribution() is MT safe
 */
double
centry_urgency_contribution(int slots, const char *name, double value,
                            const lListElem *centry) {
   DENTER(TOP_LAYER);

   double contribution, weight;
   const char *strval;

   if (!centry ||
       !(strval = lGetString(centry, CE_urgency_weight)) ||
       !(parse_ulong_val(&weight, nullptr, ocs::CEntry::Type::INT, strval, nullptr, 0))) {
      DPRINTF("no contribution for attribute\n");
      DRETURN(0);
   }

   switch (const auto complex_type = static_cast<ocs::CEntry::Type>(lGetUlong(centry, CE_valtype))) {
      case ocs::CEntry::Type::RSMAP:
      case ocs::CEntry::Type::INT:
      case ocs::CEntry::Type::TIME:
      case ocs::CEntry::Type::MEM:
      case ocs::CEntry::Type::BOOL:
      case ocs::CEntry::Type::DOUBLE:
         contribution = value * weight * slots;
         DPRINTF("   %s: %7f * %7f * %d    ---> %7f\n", name, value, weight, slots, contribution);
         break;

      case ocs::CEntry::Type::STR:
      case ocs::CEntry::Type::CSTR:
      case ocs::CEntry::Type::HOST:
      case ocs::CEntry::Type::RESTR:
         contribution = weight;
         DPRINTF("   %s: using weight as contrib ---> %7f\n", name, weight);
         break;

   default:
      ERROR(MSG_SGETEXT_UNKNOWN_ATTR_TYPE_U, static_cast<uint32_t>(complex_type));
      contribution = 0;
      break;
   }

   DRETURN(contribution);
}

bool
/**
 * @brief Is every entry of a list defined in the complex configuration?
 *
 * @param this_list the complex configuration to check against
 * @param[out] answer_list receives the name of the first entry that is missing
 * @param centry_list the entries to look for
 * @return true when all of them exist
 */
centry_list_do_all_exists(const lList *this_list, lList **answer_list,
                          const lList *centry_list) {
   DENTER(TOP_LAYER);

   bool ret = true;
   const lListElem *centry = nullptr;

   for_each_ep(centry, centry_list) {
      const char *name = lGetString(centry, CE_name);

      if (centry_list_locate(this_list, name) == nullptr) {
         answer_list_add_sprintf(answer_list, STATUS_EEXIST,
                                 ANSWER_QUALITY_ERROR,
                                 MSG_CQUEUE_UNKNOWNCENTRY_S, name);
         DTRACE;
         ret = false;
         break;
      }
   }
   DRETURN(ret);
}

bool
/**
 * @brief Reject a complex configuration that cannot work
 *
 * @param this_list the complex configuration to check
 * @param[out] answer_list receives the reason it was rejected
 * @return true when the configuration is usable
 */
centry_list_is_correct(lList *this_list, lList **answer_list) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (this_list != nullptr) {
      const lListElem *centry = lGetElemStr(this_list, CE_name, "qname");

      if (centry != nullptr) {
         const char *value = lGetString(centry, CE_stringval);

         if (strchr(value, (int) '@')) {
            answer_list_add_sprintf(answer_list, STATUS_EEXIST,
                                    ANSWER_QUALITY_ERROR,
                                    MSG_CENTRY_QINOTALLOWED);
            ret = false;
         }
      }
   }

/* do complex attributes syntax verification */
   if (ret) {
      const lListElem *elem;
      for_each_ep(elem, this_list) {
         ret = object_verify_expression_syntax(elem, answer_list);
         if (!ret) break;
      }
   }
   DRETURN(ret);
}

/**
 * @brief Reject an object that refers to a resource the complex does not define
 *
 * @param[out] alpp receives the name of the unknown resource
 * @param ep the object to check
 * @param nm the attribute of `ep` holding the resource list
 * @param master_centry_list the defined complex entries
 * @return 0 when every referenced resource exists
 */
int ensure_attrib_available(lList **alpp, lListElem *ep, int nm, const lList *master_centry_list) {
   DENTER(TOP_LAYER);
   int ret = 0;
   if (ep != nullptr) {
      for_each_rw_lv (attr, lGetList(ep, nm)) {
         const char *name = lGetString(attr, CE_name);
         lListElem *centry = centry_list_locate(master_centry_list, name);

         if (centry == nullptr) {
            ERROR(MSG_GDI_NO_ATTRIBUTE_S, name != nullptr ? name : "<noname>");
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            ret = STATUS_EUNKNOWN;
            break;
         } else {
            const char *fullname = lGetString(centry, CE_name);

            /*
             * Replace shortcuts by the fullname silently
             */
            if (strcmp(fullname, name) != 0) {
               lSetString(attr, CE_name, fullname);
            }
         }
      }
   }
   DRETURN(ret);
}

/** @brief Adds the slot complex to the complex values
 *
 * The global and the template host are skipped: neither runs jobs, so neither
 * needs a slot count.
 *
 * @param ehost Host object
 * @param processors how many processors the host reported, used as the default
 *                   slot count
 * @return an error status, or 0 on success
 */
int host_ensure_slots_are_defined(lListElem *ehost, uint32_t processors) {
   DENTER(TOP_LAYER);

   // Input argument incorrect
   if (ehost == nullptr) {
      DRETURN(STATUS_EUNKNOWN);
   }

   // No need to add slots to global host
   const char *name = lGetHost(ehost, EH_name);
   if (strcmp(name, SGE_GLOBAL_NAME) == 0 || strcmp(name, SGE_TEMPLATE_NAME) == 0) {
      DRETURN(0);
   }

   // Complex is already there.
   const lListElem *slots_complex = lGetSubStr(ehost, CE_name, SGE_ATTR_SLOTS, EH_consumable_config_list);
   if (slots_complex != nullptr) {
      DPRINTF("slots complex is already there\n");
      return 0;
   }

   if (processors == 0) {
      processors = std::numeric_limits<int>::max();
   }

   // Add the slot complex and use the given processors as value
   lListElem *new_entry = lAddSubStr(ehost, CE_name, SGE_ATTR_SLOTS, EH_consumable_config_list, CE_Type);
   lSetString(new_entry, CE_stringval, std::to_string(processors).c_str());
   lSetDouble(new_entry, CE_doubleval, processors);

   DRETURN(0);
}

/**
 * @brief The function validates a load formula string
 *
 * The function validates a load formula string.
 *
 * @param load_formula string that should be a valid load formula
 * @param answer_list error messages
 * @param centry_list list of defined complex values
 * @param name the attribute the formula belongs to, used in error messages
 *
 * @return true if valid false if unvalid
 *
 * @note MT-NOTE: is MT-safe, works only on the passed in data
 */
bool validate_load_formula(const char *load_formula, lList **answer_list, const lList *centry_list, const char *name) {
   DENTER(TOP_LAYER);

   bool ret = true;

   /* Check for keyword 'none' */
   if (!strcasecmp(load_formula, "none")) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_NONE_NOT_ALLOWED_S, name);
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX,
                      ANSWER_QUALITY_ERROR);
      ret = false;
   }

   /* Check complex attributes and type */
   if (ret) {
      const char *term_delim = "+-";
      const char *term, *next_term;
      struct saved_vars_s *term_context = nullptr;

      next_term = sge_strtok_r(load_formula, term_delim, &term_context);
      while ((term = next_term) && ret) {
         const char *fact_delim = "*";
         const char *fact, *next_fact, *end;
         struct saved_vars_s *fact_context = nullptr;

         next_term = sge_strtok_r(nullptr, term_delim, &term_context);

         fact = sge_strtok_r(term, fact_delim, &fact_context);
         next_fact = sge_strtok_r(nullptr, fact_delim, &fact_context);
         end = sge_strtok_r(nullptr, fact_delim, &fact_context);

         /* first factor has to be a complex attr */
         if (fact != nullptr) {
            if (strchr(fact, '$')) {
               fact++;
            }

            // in case the master list is available we can check that it is a complex name
            if (centry_list != nullptr) {
               const lListElem *cmplx_attr = centry_list_locate(centry_list, fact);

               if (cmplx_attr != nullptr) {
                  auto type = static_cast<ocs::CEntry::Type>(lGetUlong(cmplx_attr, CE_valtype));

                  if (type == ocs::CEntry::Type::STR || type == ocs::CEntry::Type::CSTR || type == ocs::CEntry::Type::HOST || type == ocs::CEntry::Type::RESTR) {
                     snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_WRONGTYPE_ATTRIBUTE_SS, name, fact);
                     answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                     ret = false;
                  }
               } else if (!sge_str_is_number(fact)) {
                  snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_NOTEXISTING_ATTRIBUTE_SS, name, fact);
                  answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                  ret = false;
               }
            }
         }
         /* is weighting factor a number? */
         if (next_fact != nullptr) {
            if (!sge_str_is_number(next_fact)) {
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_WEIGHTFACTNONUMB_SS, name, next_fact);
               answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
               ret = false;
            }
         }

         /* multiple weighting factors? */
         if (end != nullptr) {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_MULTIPLEWEIGHTFACT_S, name);
            answer_list_add(answer_list, SGE_EVENT,
                            STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            ret = false;
         }
         sge_free_saved_vars(fact_context);
      }
      sge_free_saved_vars(term_context);
   }

   DRETURN(ret);
}

/**
 * @brief Search load formula for centry reference
 *
 * This function searches for a centry reference in the defined algebraic
 * expression
 *
 * @param load_formula load formula expression
 * @param centry centry to search for
 *
 * @return true if referenced false if not referenced
 *
 * @note MT-NOTE: load_formula_is_centry_referenced() is MT safe
 */
bool load_formula_is_centry_referenced(const char *load_formula, const lListElem *centry) {
   DENTER(TOP_LAYER);

   bool ret = false;
   const char *term_delim = "+-";
   const char *term, *next_term;
   struct saved_vars_s *term_context = nullptr;
   const char *centry_name = lGetString(centry, CE_name);

   if (load_formula == nullptr) {
      DRETURN(ret);
   }

   next_term = sge_strtok_r(load_formula, term_delim, &term_context);
   while ((term = next_term) && !ret) {
      const char *fact_delim = "*";
      const char *fact;
      struct saved_vars_s *fact_context = nullptr;

      next_term = sge_strtok_r(nullptr, term_delim, &term_context);

      fact = sge_strtok_r(term, fact_delim, &fact_context);
      if (fact != nullptr) {
         if (strchr(fact, '$')) {
            fact++;
         }
         if (strcmp(fact, centry_name) == 0) {
            ret = true;
         }
      }
      sge_free_saved_vars(fact_context);
   }
   sge_free_saved_vars(term_context);

   DRETURN(ret);
}

/**
 * @brief The value of a resource together with where it came from
 *
 * A resource can be defined at several layers; this reports the value that
 * actually applies and, through `dominant_p`, which layer and kind it came
 * from - see the `DOMINANT_*` bits.
 *
 * @param rep the resource element to read
 * @param[out] dominant_p receives the `DOMINANT_*` bits
 * @param[out] resource_string_p receives the value as text
 * @param[out] dbl_value receives the value as a double
 * @param[out] uint64_value receives the value as an unsigned integer
 * @return the value as text
 */
const char *sge_get_dominant_stringval(const lListElem *rep, uint32_t *dominant_p, dstring *resource_string_p, double *dbl_value, uint64_t *uint64_value) {
   DENTER(TOP_LAYER);

   const char *s = nullptr;
   double val = 0;
   switch (static_cast<ocs::CEntry::Type>(lGetUlong(rep, CE_valtype))) {
      case ocs::CEntry::Type::HOST:
      case ocs::CEntry::Type::STR:
      case ocs::CEntry::Type::CSTR:
      case ocs::CEntry::Type::RESTR:
         if (!(lGetUlong(rep, CE_pj_dominant) & DOMINANT_TYPE_VALUE)) {
            *dominant_p = lGetUlong(rep, CE_pj_dominant);
            s = lGetString(rep, CE_pj_stringval);
         } else {
            *dominant_p = lGetUlong(rep, CE_dominant);
            s = lGetString(rep, CE_stringval);
         }
         break;
      case ocs::CEntry::Type::TIME:
         if (!(lGetUlong(rep, CE_pj_dominant) & DOMINANT_TYPE_VALUE)) {
            val = lGetDouble(rep, CE_pj_doubleval);
            *dominant_p = lGetUlong(rep, CE_pj_dominant);
            double_print_time_to_dstring(val, resource_string_p);
            s = sge_dstring_get_string(resource_string_p);
         } else {
            val = lGetDouble(rep, CE_doubleval);
            *dominant_p = lGetUlong(rep, CE_dominant);
            double_print_time_to_dstring(val, resource_string_p);
            s = sge_dstring_get_string(resource_string_p);
         }
         break;
      case ocs::CEntry::Type::MEM:
         if (!(lGetUlong(rep, CE_pj_dominant) & DOMINANT_TYPE_VALUE)) {
            val = lGetDouble(rep, CE_pj_doubleval);
            *dominant_p = lGetUlong(rep, CE_pj_dominant);
            double_print_memory_to_dstring(val, resource_string_p);
            s = sge_dstring_get_string(resource_string_p);
         } else {
            val = lGetDouble(rep, CE_doubleval);
            *dominant_p = lGetUlong(rep, CE_dominant);
            double_print_memory_to_dstring(val, resource_string_p);
            s = sge_dstring_get_string(resource_string_p);
         }
         break;
      case ocs::CEntry::Type::INT:
      case ocs::CEntry::Type::RSMAP:
         if (!(lGetUlong(rep, CE_pj_dominant) & DOMINANT_TYPE_VALUE)) {
            val = lGetDouble(rep, CE_pj_doubleval);
            *dominant_p = lGetUlong(rep, CE_pj_dominant);
            double_print_int_to_dstring(val, resource_string_p);
            s = sge_dstring_get_string(resource_string_p);
         } else {
            val = lGetDouble(rep, CE_doubleval);

            *dominant_p = lGetUlong(rep, CE_dominant);
            double_print_int_to_dstring(val, resource_string_p);
            s = sge_dstring_get_string(resource_string_p);
         }
         break;
      default:
         if (!(lGetUlong(rep, CE_pj_dominant) & DOMINANT_TYPE_VALUE)) {
            val = lGetDouble(rep, CE_pj_doubleval);
            *dominant_p = lGetUlong(rep, CE_pj_dominant);
            double_print_to_dstring(val, resource_string_p);
            s = sge_dstring_get_string(resource_string_p);
         } else {
            val = lGetDouble(rep, CE_doubleval);
            *dominant_p = lGetUlong(rep, CE_dominant);
            double_print_to_dstring(val, resource_string_p);
            s = sge_dstring_get_string(resource_string_p);
         }
         break;
   }
   if (dbl_value != nullptr) {
      *dbl_value = val;
   }
   if (uint64_value != nullptr) {
      *uint64_value = static_cast<uint64_t>(val);
   }

   DRETURN(s);
}

/**
 * @brief The sign of a slot count
 *
 * Booking and unbooking share the same code paths with a positive or negative
 * slot count, so the sign says which of the two is happening.
 *
 * @param slots the slot count
 * @return 1 for a positive count, -1 for a negative one, 0 for zero
 */
int slot_signum(int slots) {
   int ret = 0;

   if (slots > 0) {
      ret = 1;
   } else if (slots < 0) {
      ret = -1;
   }

   return ret;
}

/**
 * Evaluate if booking for a specific consumable shall actually be done.
 *
 * @param[in] consumable type, e.g. CONSUMABLE_NO, CONSUMABLE_YES, ...
 * @param[in] is_master_task is booking done for the master task of a parallel job (or the one task of a sequential job)
 * @param[in] do_per_host_booking shall booking be done for a per-host consumable (true only for the first task on a host)
 * @return true, if booking shall be done, else false
 */
bool consumable_do_booking(uint32_t consumable, bool is_master_task, bool do_per_host_booking) {
   bool ret = true;

   switch (consumable) {
      case CONSUMABLE_NO:
         ret = false;
         break;
      case CONSUMABLE_JOB:
         if (!is_master_task) {
            ret = false;
         }
         break;
      case CONSUMABLE_HOST:
         // if we do not do per_host_booking,
         // or we already did the booking for a per-host variable
         if (!do_per_host_booking) {
            ret = false;
         }
         break;
   }

   return ret;
}

/**
 * Returns the number of slots which shall be debited for.
 * Depends on the consumable type,
 * for CONSUMABLE_YES it is the given number of slots,
 * but for CONSUMABLE_JOB and CONSUMABLE_HOST requests are only booked once (1, or -1 for undebiting)
 *
 * @param[in] consumable type, e.g. CONSUMABLE_NO, CONSUMABLE_YES, ...
 * @param[in] slots the number of slots (tasks) which shall be booked on a resource
 * @return the slot count to debit
 */
int consumable_get_debit_slots(uint32_t consumable, int slots) {
   // default: CONSUMABLE_YES
   int ret = slots;

   if (consumable == CONSUMABLE_JOB || consumable == CONSUMABLE_HOST) {
      // it's a job consumable or a host consumable, we don't multiply with slots
      ret = slot_signum(slots);
   }

   return ret;
}
