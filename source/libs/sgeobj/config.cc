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
 * @brief Reading a flat `key value` configuration file into a CULL object
 *
 * The file is first parsed into a list of name/value pairs, then consumed key
 * by key: each `set_conf_*` call takes one key, converts its value to the
 * attribute's type and **removes the entry**. Whatever is left over afterwards
 * is exactly the set of keys nobody understood, which is how an unknown
 * setting is reported rather than silently ignored.
 *
 * @see config.h
 */

#include <cstdio>
#include <cstring>
#include <cctype>
#include <cstdlib>

#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "cull/cull.h"

#include "sgeobj/config.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/parse.h"
#include "sgeobj/sge_attr.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/msg_sgeobjlib.h"

/* 
**
** DESCRIPTION
**    read_config_list() reads the file into a configuration list 
**    of type 'delimitor' putting the name of the configuraion
**    attribute into the 'nm1'-field and the value into 'nm2'.
**    'delimitor' is used to separate the name and value.
**    It may be nullptr as it gets passed to sge_strtok().
**    If 'flag' has set the RCL_NO_VALUE bitmask then
**    also lines are accepted having only a name but no value.
**    
**
**
** RETURN
**    A nullptr pointer as answer list signals success.
*/
/**
 * @brief Read a configuration file into a name/value list
 *
 * Each line becomes one element: the text before `delimitor` goes into `nm1`,
 * the text after it into `nm2`. Without #RCL_NO_VALUE a line carrying only a
 * name is an error.
 *
 * @param fp the file to read
 * @param[out] lpp receives the parsed list
 * @param[out] alpp receives the error message; a nullptr answer list signals success
 * @param dp the descriptor of the list elements to create
 * @param nm1 the attribute the name is stored in
 * @param nm2 the attribute the value is stored in
 * @param nm3 the attribute a third field is stored in, or `NoName`
 * @param delimitor separates name and value; may be nullptr, it is passed on to `sge_strtok`
 * @param flag `RCL_NO_VALUE` to accept a line with a name but no value
 * @param[out] buffer scratch buffer the parser reads lines into
 * @param buffer_size the size of `buffer`
 * @return 0 on success
 */
int read_config_list(FILE *fp, lList **lpp, lList **alpp, lDescr *dp, int nm1,
                     int nm2, int nm3, const char *delimitor, int flag, char *buffer,
                     int buffer_size) 
{
   lListElem *ep;
   char *name; 
   char *value;
   char *tmp;
   char *s;
   struct saved_vars_s *last = nullptr;
   int force_value;
  
   DENTER(TOP_LAYER);

   force_value = ((flag&RCL_NO_VALUE)==0);

   while (fgets(buffer, buffer_size, fp)) {
      if (last) {
         sge_free_saved_vars(last);
         last = nullptr;
      }
      /*
      ** skip empty and comment lines
      */
      if (buffer[0] == '#')
         continue;
      for(s = buffer; *s == ' ' || *s == '\t'; s++)
         ;
      if (*s == '\n')
         continue;

      /*
      ** get name and value
      */
      if (*s != '"' || !(name = sge_strtok_r(s+1, "\"", &last))) {
         if (!(name = sge_strtok_r(buffer, delimitor, &last)))
            break;
      }
      value = sge_strtok_r(nullptr, "\n", &last);

      /* handle end of sub-list */
      if (nm3 && strcmp(name, "}") == 0 && !value) {
         break;
      }

      if (!value && force_value)
         break;

      /* skip leading delimitors */
      if (value) {
         while (*value && (delimitor? 
            (nullptr != strchr(delimitor, *value)):
            isspace((int) *value)))
            value++;
      }

      if(!value || !(*value)) {
         if (force_value) {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_GDI_CONFIGNOARGUMENTGIVEN_S , name);
            answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            goto Error;
         }
         value = nullptr;
      } else {
         /* skip trailing delimitors */
         tmp = &value[strlen(value)-1];

         while (tmp>value && (delimitor?
            (nullptr != strchr(delimitor, *tmp)):
            isspace((int) *tmp))) {
            *tmp = '\0';
            tmp--;
         }
      }

      /* handle sub-list */
      if (nm3 && value && strcmp(value, "{") == 0) {
         lList *slpp = nullptr;
         if (read_config_list(fp, &slpp, alpp, dp, nm1, nm2, nm3, delimitor, flag, buffer, buffer_size)<0) {
            goto Error;
         }
         ep = lAddElemStr(lpp, nm1, name, dp);
         if (!ep) { 
            ERROR(MSG_GDI_CONFIGADDLISTFAILED_S , name);
            answer_list_add(alpp, SGE_EVENT, STATUS_EDISK, ANSWER_QUALITY_ERROR);
            goto Error;
         } 
         lSetList(ep, nm3, slpp);
      } else {
         ep = lAddElemStr(lpp, nm1, name, dp);
         if (!ep) { 
            ERROR(MSG_GDI_CONFIGADDLISTFAILED_S , name);
            answer_list_add(alpp, SGE_EVENT, STATUS_EDISK, ANSWER_QUALITY_ERROR);
            goto Error;
         } 

         lSetString(ep, nm2, value);
      }
   }
   
   if (last) {
      sge_free_saved_vars(last);
   }   
   DRETURN(0); 

Error:
   if (last)
      sge_free_saved_vars(last);
   DRETURN(-1);
}

/*
**
** DESCRIPTION
**    'get_conf_sublist' searches in the configurations list 
**    for a config value with 'key' as name. 'name_nm' is
**    used as field for the key and 'value_nm' is taken
**    for value.
** 
**    If an alpp gets passed then an answer element 
**    gets added in case of error.
**
** RETURN
**    Returns sub-list value or nullptr if such a list
**    does not exist.
*/
/**
 * @brief The sublist value of one configuration entry
 *
 * @param[out] alpp receives the message naming the missing key; may be nullptr
 * @param lp the parsed configuration
 * @param name_nm the attribute holding an entry's name
 * @param value_nm the attribute holding its value
 * @param key the entry to look for
 * @return the sublist, or nullptr when the key is absent
 */
lList *get_conf_sublist(lList **alpp, lList *lp, int name_nm, int value_nm,
                        const char *key) 
{
   lList *value;

   DENTER(CULL_LAYER);
   
   const lListElem *ep = lGetElemStr(lp, name_nm, key);
   if (ep == nullptr) {
      if (alpp) {
         char error[1000];
         snprintf(error, sizeof(error), MSG_GDI_CONFIGMISSINGARGUMENT_S, key);
         answer_list_add(alpp, error, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      }
      DRETURN(nullptr);
   }

   value = lGetListRW(ep, value_nm);

   DRETURN(value);
}

/*
**
** DESCRIPTION
**    'get_conf_value' searches in the configurations list 
**    for a config value with 'key' as name. 'name_nm' is
**    used as field for the key and 'value_nm' is taken
**    for value.
** 
**    If an alpp gets passed then an answer element 
**    gets added in case of error.
**
** RETURN
**    Returns string value or nullptr if such a value
**    does not exist.
*/
/**
 * @brief The string value of one configuration entry
 *
 * @param[out] alpp receives the message naming the missing key; may be nullptr
 * @param lp the parsed configuration
 * @param name_nm the attribute holding an entry's name
 * @param value_nm the attribute holding its value
 * @param key the entry to look for
 * @return the value, or nullptr when the key is absent
 */
char *get_conf_value(lList **alpp, lList *lp, int name_nm, int value_nm,
                           const char *key) {
   char *value;

   DENTER(CULL_LAYER);
   
   const lListElem *ep = lGetElemStr(lp, name_nm, key);
   if (ep == nullptr) {
      if (alpp) {
         char error[1000];
         snprintf(error, sizeof(error), MSG_GDI_CONFIGMISSINGARGUMENT_S, key);
         answer_list_add(alpp, error, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      }
      DRETURN(nullptr);
   }

   /* FIX_CONST */
   value = (char*) lGetString(ep, value_nm);
   DPRINTF("%s = %s\n", key, value?value:"<null ptr>");

   DRETURN(value);
}

/****
 **** set_conf_string
 ****
 **** 'set_conf_string' searches in the configuration list
 **** (pointed to by clpp) for a string-config value with 'key'
 **** as name.
 **** If the value is found, it is stored in the lListElem
 **** 'ep' in the fied specified by 'name_nm'.
 **** If the config-string is not found, an error message
 **** is created.
 ****
 **** The function returns false on error, otherwise true.
 ****/
/**
 * @brief Consume a configuration key and store its value as a string
 *
 * The entry is removed from the parsed configuration, so what is left over
 * afterwards is exactly the set of keys nobody understood.
 *
 * @param[out] alpp receives the error message; pass nullptr when `fields` is set,
 *                  since a missing key is then not an error
 * @param[in,out] clpp the parsed configuration; the consumed entry is removed from it
 * @param fields when non-nullptr, the attribute is recorded here as "was set"
 *               and a missing key is accepted rather than reported
 * @param key the configuration key to consume
 * @param[in,out] ep the object the value is stored in
 * @param name_nm the attribute of `ep` to store it in
 * @return false on error, otherwise true
 */
bool set_conf_string(
lList **alpp,
lList **clpp,
int fields[],
const char *key,
lListElem *ep,
int name_nm 
) {
   const char *str;
   int pos;
   int dataType;

   DENTER(TOP_LAYER);

   if(!(str=get_conf_value(fields?nullptr:alpp, *clpp, CF_name, CF_value, key))) {
      DRETURN(fields?true:false);
   }

   pos = lGetPosViaElem(ep, name_nm, SGE_NO_ABORT);
   dataType = lGetPosType(lGetElemDescr(ep),pos);
   switch (dataType) {
      case lStringT:
         DPRINTF("set_conf_string: lStringT data type (Type: %s)\n",lNm2Str(name_nm));
         lSetString(ep, name_nm, str);
         break;
      case lHostT:
         DPRINTF("set_conf_string: lHostT data type (Type: %s)\n",lNm2Str(name_nm));
         lSetHost(ep, name_nm, str);
         break;
      default:
         DPRINTF("!!!!!!!!!set_conf_string: unexpected data type !!!!!!!!!!!!!!!!!\n");
         break;
   } 
   lDelElemStr(clpp, CF_name, key);
   add_nm_to_set(fields, name_nm);

   DRETURN(true);
}

/****
 **** set_conf_bool
 ****
 **** 'set_conf_bool' searches in the configuration list
 **** (pointed to by clpp) for a string-config value with 'key'
 **** as name.
 **** If the value is found, it is stored in the lListElem
 **** 'ep' in the field specified by 'name_nm'.
 **** the strings true and false are stored as the constants true/false
 **** in a uint32_t field
 **** If the config-string is not found, an error message
 **** is created.
 ****
 **** The function returns false on error, otherwise true.
 ****/
/**
 * @brief Consume a configuration key and store its value as a boolean
 *
 * The entry is removed from the parsed configuration, so what is left over
 * afterwards is exactly the set of keys nobody understood.
 *
 * @param[out] alpp receives the error message; pass nullptr when `fields` is set,
 *                  since a missing key is then not an error
 * @param[in,out] clpp the parsed configuration; the consumed entry is removed from it
 * @param fields when non-nullptr, the attribute is recorded here as "was set"
 *               and a missing key is accepted rather than reported
 * @param key the configuration key to consume
 * @param[in,out] ep the object the value is stored in
 * @param name_nm the attribute of `ep` to store it in
 * @return false on error, otherwise true
 */
bool set_conf_bool(
lList **alpp,
lList **clpp,
int fields[],
const char *key,
lListElem *ep,
int name_nm 
) {
   const char *str;

   DENTER(CULL_LAYER);

   if(!(str=get_conf_value(fields?nullptr:alpp, *clpp, CF_name, CF_value, key))) {
      DRETURN(fields?true:false);
   }
   if (!object_parse_bool_from_string(ep, nullptr, name_nm, str)) {
      DRETURN(false);
   }
      
   lDelElemStr(clpp, CF_name, key);
   add_nm_to_set(fields, name_nm);

   DRETURN(true);
}

/**
 * @brief Consume a configuration key and store its value as a complex entry type name
 *
 * The entry is removed from the parsed configuration, so what is left over
 * afterwards is exactly the set of keys nobody understood.
 *
 * @param[out] alpp receives the error message; pass nullptr when `fields` is set,
 *                  since a missing key is then not an error
 * @param[in,out] clpp the parsed configuration; the consumed entry is removed from it
 * @param fields when non-nullptr, the attribute is recorded here as "was set"
 *               and a missing key is accepted rather than reported
 * @param key the configuration key to consume
 * @param[in,out] ep the object the value is stored in
 * @param name_nm the attribute of `ep` to store it in
 * @return false on error, otherwise true
 */
bool set_conf_centry_type(
lList **alpp,
lList **clpp,
int fields[],
const char *key,
lListElem *ep,
int name_nm 
) {
   const char *str;
   uint32_t type;

   DENTER(CULL_LAYER);

   if(!(str=get_conf_value(fields?nullptr:alpp, *clpp, CF_name, CF_value, key))) {
      DRETURN(fields?true:false);
   }
   if (!ulong_parse_centry_type_from_string(&type, alpp, str)) {
      DRETURN(false);
   } else {
      lSetUlong(ep, name_nm, type);
   }

   lDelElemStr(clpp, CF_name, key);
   add_nm_to_set(fields, name_nm);

   DRETURN(true);
}

/**
 * @brief Consume a configuration key and store its value as a complex entry relational operator
 *
 * The entry is removed from the parsed configuration, so what is left over
 * afterwards is exactly the set of keys nobody understood.
 *
 * @param[out] alpp receives the error message; pass nullptr when `fields` is set,
 *                  since a missing key is then not an error
 * @param[in,out] clpp the parsed configuration; the consumed entry is removed from it
 * @param fields when non-nullptr, the attribute is recorded here as "was set"
 *               and a missing key is accepted rather than reported
 * @param key the configuration key to consume
 * @param[in,out] ep the object the value is stored in
 * @param name_nm the attribute of `ep` to store it in
 * @return false on error, otherwise true
 */
bool set_conf_centry_relop(
lList **alpp,
lList **clpp,
int fields[],
const char *key,
lListElem *ep,
int name_nm 
) {
   const char *str;
   uint32_t type;

   DENTER(CULL_LAYER);

   if(!(str=get_conf_value(fields?nullptr:alpp, *clpp, CF_name, CF_value, key))) {
      DRETURN(fields?true:false);
   }
   if (!ulong_parse_centry_relop_from_string(&type, alpp, str)) {
      DRETURN(false);
   } else {
      lSetUlong(ep, name_nm, type);
   }

   lDelElemStr(clpp, CF_name, key);
   add_nm_to_set(fields, name_nm);

   DRETURN(true);
}

/**
 * @brief Consume a configuration key and store its value as a complex entry requestable setting
 *
 * The entry is removed from the parsed configuration, so what is left over
 * afterwards is exactly the set of keys nobody understood.
 *
 * @param[out] alpp receives the error message; pass nullptr when `fields` is set,
 *                  since a missing key is then not an error
 * @param[in,out] clpp the parsed configuration; the consumed entry is removed from it
 * @param fields when non-nullptr, the attribute is recorded here as "was set"
 *               and a missing key is accepted rather than reported
 * @param key the configuration key to consume
 * @param[in,out] ep the object the value is stored in
 * @param name_nm the attribute of `ep` to store it in
 * @return false on error, otherwise true
 */
bool set_conf_centry_requestable(
lList **alpp,
lList **clpp,
int fields[],
const char *key,
lListElem *ep,
int name_nm 
) {
   const char *str;
   uint32_t flag;

   DENTER(CULL_LAYER);

   if(!(str=get_conf_value(fields?nullptr:alpp, *clpp, CF_name, CF_value, key))) {
      DRETURN(fields?true:false);
   }
   if (!strcasecmp(str, "y") || !strcasecmp(str, "yes")) {
      flag = REQU_YES;
   } else if (!strcasecmp(str, "n") || !strcasecmp(str, "no")) {
      flag = REQU_NO;
   } else if (!strcasecmp(str, "f") || !strcasecmp(str, "forced")) {
      flag = REQU_FORCED;
   } else {
      answer_list_add_sprintf(alpp, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_INVALID_CENTRY_REQUESTABLE_S, str);
      DRETURN(false);
   }
   lSetUlong(ep, name_nm, flag);

   lDelElemStr(clpp, CF_name, key);
   add_nm_to_set(fields, name_nm);

   DRETURN(true);
}

/****
 **** set_conf_ulong
 ****
 **** 'set_conf_ulong' searches in the configuration list
 **** (pointed to by clpp) for a ulong-config value with 'key'
 **** as name.
 **** If the value is found, it is stored in the lListElem
 **** 'ep' in the fied specified by 'name_nm'.
 **** If the config-value is not found, or it is not an
 **** integer, an error message is created.
 ****
 **** The function returns false on error, otherwise true.
 ****/
/**
 * @brief Consume a configuration key and store its value as an unsigned integer
 *
 * The entry is removed from the parsed configuration, so what is left over
 * afterwards is exactly the set of keys nobody understood.
 *
 * @param[out] alpp receives the error message; pass nullptr when `fields` is set,
 *                  since a missing key is then not an error
 * @param[in,out] clpp the parsed configuration; the consumed entry is removed from it
 * @param fields when non-nullptr, the attribute is recorded here as "was set"
 *               and a missing key is accepted rather than reported
 * @param key the configuration key to consume
 * @param[in,out] ep the object the value is stored in
 * @param name_nm the attribute of `ep` to store it in
 * @return false on error, otherwise true
 */
bool set_conf_ulong(
lList **alpp,
lList **clpp,
int fields[],
const char *key,
lListElem *ep,
int name_nm 
) {
   const char *str;

   DENTER(CULL_LAYER);

   if(!(str=get_conf_value(fields?nullptr:alpp, *clpp, CF_name, CF_value, key))) {
      DRETURN(fields?true:false);
   }
   if (!object_parse_ulong32_from_string(ep, alpp, name_nm, str)) {
      DRETURN(false);
   }
   lDelElemStr(clpp, CF_name, key);
   add_nm_to_set(fields, name_nm);

   DRETURN(true);
}

/****
 **** set_conf_double
 ****
 **** 'set_conf_double' searches in the configuration list
 **** (pointed to by clpp) for a double-config value with 'key'
 **** as name.
 **** If the value is found, it is stored in the lListElem
 **** 'ep' in the field specified by 'name_nm'.
 **** If operation_nm != 0, the double value can be preceded by
 **** +,-,= or nothing. This sets a flag specified by operation_nm.
 **** If the config-value is not found, or it is not a
 **** double, an error message is created.
 ****
 **** The function returns false on error, otherwise true.
 ****/
/**
 * @brief Consume a configuration key and store its value as a double, with a modification operator
 *
 * The entry is removed from the parsed configuration, so what is left over
 * afterwards is exactly the set of keys nobody understood.
 *
 * @param[out] alpp receives the error message; pass nullptr when `fields` is set,
 *                  since a missing key is then not an error
 * @param[in,out] clpp the parsed configuration; the consumed entry is removed from it
 * @param fields when non-nullptr, the attribute is recorded here as "was set"
 *               and a missing key is accepted rather than reported
 * @param key the configuration key to consume
 * @param[in,out] ep the object the value is stored in
 * @param name_nm the attribute of `ep` to store it in
 * @param operation_nm the attribute recording whether the value replaces, adds to or subtracts from the current one
 * @return false on error, otherwise true
 */
bool set_conf_double(
lList **alpp,
lList **clpp,
int fields[],
const char *key,
lListElem *ep,
int name_nm,
int operation_nm 
) {
   const char *str;
   double dval;

   DENTER(CULL_LAYER); 

   if(!(str=get_conf_value(fields?nullptr:alpp, *clpp, CF_name, CF_value, key))) {
      DRETURN(fields?true:false);
   }
   /*
   ** only treated if operation_nm != 0
   */
   if (operation_nm) {
      while (isspace((int) *str))
         str++;
      switch (*str) {
         case '+':   
            lSetUlong(ep, operation_nm, MODE_ADD);
            str++;
            break;
         case '-':
            lSetUlong(ep, operation_nm, MODE_SUB);
            str++;
            break;
         case '=':
            lSetUlong(ep, operation_nm, MODE_SET);
            str++;
            break;
         default:
            lSetUlong(ep, operation_nm, MODE_RELATIVE);
      }
   }

   if ( (sscanf(str, "%lf", &dval)!=1) || ( strncasecmp(str,"inf",3) == 0 ) ) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_GDI_CONFIGARGUMENTNOTDOUBLE_SS , key, str);
      answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   lSetDouble(ep, name_nm, dval);
   lDelElemStr(clpp, CF_name, key);
   add_nm_to_set(fields, name_nm);

   DRETURN(true);
}

/****
 **** set_conf_deflist
 ****
 **** 'set_conf_deflist' searches in the configuration list
 **** (pointed to by clpp) for a definition-list-config 
 **** value with 'key' as name.
 **** If the value is found, it is stored in the lListElem
 **** 'ep' in the fied specified by 'name_nm'.
 **** The definition-list is tokenized with 
 **** cull_parse_definition_list().
 **** The sub-list is created of type 'descr' and 
 **** interpreted by 'interpretation_rule'.
 ****
 **** The function returns false on error, otherwise true.
 ****/
/**
 * @brief Consume a configuration key and store its value as a definition list
 *
 * The entry is removed from the parsed configuration, so what is left over
 * afterwards is exactly the set of keys nobody understood.
 *
 * @param[out] alpp receives the error message; pass nullptr when `fields` is set,
 *                  since a missing key is then not an error
 * @param[in,out] clpp the parsed configuration; the consumed entry is removed from it
 * @param fields when non-nullptr, the attribute is recorded here as "was set"
 *               and a missing key is accepted rather than reported
 * @param key the configuration key to consume
 * @param[in,out] ep the object the value is stored in
 * @param name_nm the attribute of `ep` to store it in
 * @param descr the descriptor of the list elements
 * @param interpretation_rule the attribute order the entries are parsed into
 * @return false on error, otherwise true
 */
bool set_conf_deflist(
lList **alpp,
lList **clpp,
int fields[],
const char *key,
lListElem *ep,
int name_nm,
lDescr *descr,
int *interpretation_rule 

) {
   lList *tmplp = nullptr;
   char *str;

   DENTER(CULL_LAYER);


   if(!(str=get_conf_value(fields?nullptr:alpp, *clpp, CF_name, CF_value, key))) {
      DRETURN((fields?true:false));
   }

   if (cull_parse_definition_list(str, &tmplp, key, descr, 
         interpretation_rule) != 0) {
      DRETURN(false);
   }

   lSetList(ep, name_nm, tmplp);
   lDelElemStr(clpp, CF_name, key);
   add_nm_to_set(fields, name_nm);

   DRETURN(true);
}

/****
 **** set_conf_timestring
 ****
 **** 'set_conf_timestring' searches in the configuration list
 **** (pointed to by clpp) for a timestring-config value 
 **** with 'key' as name.
 **** If the value is found, it is stored in the lListElem
 **** 'ep' in the fied specified by 'name_nm'.
 **** If the config-string is not found, or it is not a
 **** valid time-string, an error message is created.
 ****
 **** The function returns false on error, otherwise true.
 ****/
/**
 * @brief Consume a configuration key and store its value as a time specification, stored as written
 *
 * The entry is removed from the parsed configuration, so what is left over
 * afterwards is exactly the set of keys nobody understood.
 *
 * @param[out] alpp receives the error message; pass nullptr when `fields` is set,
 *                  since a missing key is then not an error
 * @param[in,out] clpp the parsed configuration; the consumed entry is removed from it
 * @param fields when non-nullptr, the attribute is recorded here as "was set"
 *               and a missing key is accepted rather than reported
 * @param key the configuration key to consume
 * @param[in,out] ep the object the value is stored in
 * @param name_nm the attribute of `ep` to store it in
 * @return false on error, otherwise true
 */
bool set_conf_timestr(
lList **alpp,
lList **clpp,
int fields[],
const char *key,
lListElem *ep,
int name_nm 
) {
   const char *str;

   DENTER(CULL_LAYER);

   if (key == nullptr) {
      DRETURN(false);
   }

   if(!(str=get_conf_value(fields?nullptr:alpp, *clpp, CF_name, CF_value, key))) {
      DRETURN(fields?true:false);
   }
   if(!parse_ulong_val(nullptr, nullptr, ocs::CEntry::Type::TIME, str, nullptr, 0)) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_GDI_CONFIGARGUMENTNOTTIME_SS , key, str);
      answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   lSetString(ep, name_nm, str);
   lDelElemStr(clpp, CF_name, key);
   add_nm_to_set(fields, name_nm);

   DRETURN(true);
}

/****
 **** set_conf_memstr
 ****
 **** 'set_conf_memstr' searches in the configuration list
 **** (pointed to by clpp) for a memstring-config value 
 **** with 'key' as name.
 **** If the value is found, it is stored in the lListElem
 **** 'ep' in the fied specified by 'name_nm'.
 **** If the config-string is not found, or it is not a
 **** valid mem-string, an error message is created.
 ****
 **** The function returns false on error, otherwise true.
 ****/
/**
 * @brief Consume a configuration key and store its value as a memory specification, stored as written
 *
 * The entry is removed from the parsed configuration, so what is left over
 * afterwards is exactly the set of keys nobody understood.
 *
 * @param[out] alpp receives the error message; pass nullptr when `fields` is set,
 *                  since a missing key is then not an error
 * @param[in,out] clpp the parsed configuration; the consumed entry is removed from it
 * @param fields when non-nullptr, the attribute is recorded here as "was set"
 *               and a missing key is accepted rather than reported
 * @param key the configuration key to consume
 * @param[in,out] ep the object the value is stored in
 * @param name_nm the attribute of `ep` to store it in
 * @return false on error, otherwise true
 */
bool set_conf_memstr(
lList **alpp,
lList **clpp,
int fields[],
const char *key,
lListElem *ep,
int name_nm 
) {
   const char *str;

   DENTER(CULL_LAYER);

   if(!(str=get_conf_value(fields?nullptr:alpp, *clpp, CF_name, CF_value, key))) {
      DRETURN(fields?true:false);
   }
   if(!parse_ulong_val(nullptr, nullptr, ocs::CEntry::Type::MEM, str, nullptr, 0)) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_GDI_CONFIGARGUMENTNOMEMORY_SS , key, str);
      answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   lSetString(ep, name_nm, str);
   lDelElemStr(clpp, CF_name, key);
   add_nm_to_set(fields, name_nm);

   DRETURN(true);
}

/****
 **** set_conf_enum
 ****
 **** 'set_conf_enum' searches in the configuration list
 **** (pointed to by clpp) for a enumstring-config value 
 **** with 'key' as name.
 **** If the value is found, it is stored in the lListElem
 **** 'ep' in the field specified by 'name_nm'.
 **** If the config-string is not found, or it is not a
 **** valid enum-string, an error message is created.
 ****
 **** The function returns false on error, otherwise true.
 ****/
/**
 * @brief Consume a configuration key and store its value as one of a fixed set of names
 *
 * The entry is removed from the parsed configuration, so what is left over
 * afterwards is exactly the set of keys nobody understood.
 *
 * @param[out] alpp receives the error message; pass nullptr when `fields` is set,
 *                  since a missing key is then not an error
 * @param[in,out] clpp the parsed configuration; the consumed entry is removed from it
 * @param fields when non-nullptr, the attribute is recorded here as "was set"
 *               and a missing key is accepted rather than reported
 * @param key the configuration key to consume
 * @param[in,out] ep the object the value is stored in
 * @param name_nm the attribute of `ep` to store it in
 * @param enum_strings the accepted names, terminated by nullptr
 * @return false on error, otherwise true
 */
bool set_conf_enum(lList **alpp, lList **clpp, int fields[], const char *key,
                  lListElem *ep, int name_nm, const char **enum_strings) 
{
   const char *str;
   uint32_t uval = 0;

   DENTER(CULL_LAYER);

   if(!(str=get_conf_value(fields?nullptr:alpp, *clpp, CF_name, CF_value, key))) {
      DRETURN(fields?true:false);
   }
   if(!sge_parse_bitfield_str(str, enum_strings, &uval, key, alpp, false)) {
      DRETURN(false);
   }
   
   if(!uval) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_GDI_CONFIGINVALIDQUEUESPECIFIED);
      answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   lSetUlong(ep, name_nm, uval);
   lDelElemStr(clpp, CF_name, key);
   add_nm_to_set(fields, name_nm);


   DRETURN(true);
}

/**
 * @brief Consume a configuration key and store its value as one of a fixed set of names, or `NONE`
 *
 * The entry is removed from the parsed configuration, so what is left over
 * afterwards is exactly the set of keys nobody understood.
 *
 * @param[out] alpp receives the error message; pass nullptr when `fields` is set,
 *                  since a missing key is then not an error
 * @param[in,out] clpp the parsed configuration; the consumed entry is removed from it
 * @param fields when non-nullptr, the attribute is recorded here as "was set"
 *               and a missing key is accepted rather than reported
 * @param key the configuration key to consume
 * @param[in,out] ep the object the value is stored in
 * @param name_nm the attribute of `ep` to store it in
 * @param enum_strings the accepted names, terminated by nullptr
 * @return false on error, otherwise true
 */
bool set_conf_enum_none(lList **alpp, lList **clpp, int fields[], const char *key,
                  lListElem *ep, int name_nm, const char **enum_strings) 
{
   const char *str;
   uint32_t uval = 0;

   DENTER(TOP_LAYER);

   if(!(str=get_conf_value(fields?nullptr:alpp, *clpp, CF_name, CF_value, key))) {
      DRETURN(fields?true:false);
   }
   if(!sge_parse_bitfield_str(str, enum_strings, &uval, key, alpp, true)) {
      DRETURN(false);
   }
   
   lSetUlong(ep, name_nm, uval);
   lDelElemStr(clpp, CF_name, key);
   add_nm_to_set(fields, name_nm);


   DRETURN(true);
}

/****
 **** set_conf_list
 ****
 **** 'set_conf_list' searches in the configuration list
 **** (pointed to by clpp) for a list-config value with 
 **** 'key' as name.
 **** If the value is found, it is stored in the lListElem
 **** 'ep' in the fied specified by 'name_nm'.
 **** The definition-list is tokenized with 
 **** lString2List().
 **** The sub-list is created of type 'descr' and 
 **** the strings are stored in the field 'sub_name_nm'.
 ****
 **** The function returns false on error, otherwise true.
 ****/
/**
 * @brief Consume a configuration key and store its value as a list
 *
 * The entry is removed from the parsed configuration, so what is left over
 * afterwards is exactly the set of keys nobody understood.
 *
 * @param[out] alpp receives the error message; pass nullptr when `fields` is set,
 *                  since a missing key is then not an error
 * @param[in,out] clpp the parsed configuration; the consumed entry is removed from it
 * @param fields when non-nullptr, the attribute is recorded here as "was set"
 *               and a missing key is accepted rather than reported
 * @param key the configuration key to consume
 * @param[in,out] ep the object the value is stored in
 * @param name_nm the attribute of `ep` to store it in
 * @param descr the descriptor of the list elements
 * @param sub_name_nm the attribute within an element the words are stored in
 * @return false on error, otherwise true
 */
bool set_conf_list(lList **alpp, lList **clpp, int fields[], const char *key, 
                  lListElem *ep, int name_nm, lDescr *descr, int sub_name_nm) 
{
   lList *tmplp = nullptr;
   const char *str;
   const char *tmp_str = nullptr;
   char delims[] = "\t \v\r,"; 

   DENTER(TOP_LAYER);

   if(!(str=get_conf_value(fields?nullptr:alpp, *clpp, CF_name, CF_value, key))) {
      DRETURN(fields?true:false);
   }
   lString2List(str, &tmplp, descr, sub_name_nm, delims); 

   lDelElemStr(clpp, CF_name, key);
   add_nm_to_set(fields, name_nm);

   

   if (tmplp != nullptr) {
      int pos, dataType;
      const lListElem *lep = lFirst(tmplp);

      pos = lGetPosViaElem(lep, sub_name_nm, SGE_NO_ABORT);
      dataType = lGetPosType(lGetElemDescr(lep),pos);
      switch (dataType) {
         case lStringT:
            DPRINTF("set_conf_list: lStringT data type (Type: %s)\n",lNm2Str(name_nm));
            tmp_str = lGetString(lep, sub_name_nm);
            break;
         case lHostT:
            DPRINTF("set_conf_list: lHostT data type (Type: %s)\n",lNm2Str(name_nm));
            tmp_str = lGetHost(lep, sub_name_nm);
            break;
         default:
            DPRINTF("!!!!!!!!!set_conf_string: unexpected data type !!!!!!!!!!!!!!!!!\n");
            break;
      }
      if (strcasecmp("NONE", tmp_str)) {
         lSetList(ep, name_nm, tmplp);
         DRETURN(true);
      } else {
         lFreeList(&tmplp);
      }
   }

   DRETURN(true);
}

/**
 * @brief Consume a configuration key holding a per host list of strings
 *
 * A cluster queue attribute is written as a list of host or host group
 * specific values; this parses that form and stores the resulting list.
 *
 * @param[out] alpp receives the error message; pass nullptr when `fields` is set,
 *                  since a missing key is then not an error
 * @param[in,out] clpp the parsed configuration; the consumed entry is removed from it
 * @param fields when non-nullptr, the attribute is recorded here as "was set"
 *               and a missing key is accepted rather than reported
 * @param key the configuration key to consume
 * @param[in,out] ep the object the value is stored in
 * @param name_nm the attribute of `ep` to store it in
 * @param descr the descriptor of the list elements
 * @param sub_name_nm the attribute within an element the value is stored in
 * @param master_hgroup_list the host groups, needed to resolve a host group name
 * @return false on error, otherwise true
 */
bool set_conf_str_attr_list(lList **alpp, lList **clpp, int fields[], 
                            const char *key, lListElem *ep, int name_nm, 
                            lDescr *descr, int sub_name_nm, const lList *master_hgroup_list) 
{
   bool ret;
   lList *tmplp = nullptr;
   const char *str;
   lList *lanswer_list = nullptr;

   DENTER(TOP_LAYER);

   if(!(str=get_conf_value(fields?nullptr:alpp, *clpp, CF_name, CF_value, key))) {
      DRETURN(fields?true:false);
   }
   ret = str_attr_list_parse_from_string(&tmplp, &lanswer_list, str,
                                         HOSTATTR_ALLOW_AMBIGUITY, master_hgroup_list);
   if (!ret) {
      const char *text = lGetString(lFirst(lanswer_list), AN_text);

      snprintf(SGE_EVENT, SGE_EVENT_SIZE, "%s - %s", key, text);
      answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      return ret;
   }
   lDelElemStr(clpp, CF_name, key);
   add_nm_to_set(fields, name_nm);

   if (tmplp != nullptr) {
      lSetList(ep, name_nm, tmplp);
      DRETURN(true);
   }

   DRETURN(true);
}

/**
 * @brief Consume a configuration key holding a per host list of string lists
 *
 * A cluster queue attribute is written as a list of host or host group
 * specific values; this parses that form and stores the resulting list.
 *
 * @param[out] alpp receives the error message; pass nullptr when `fields` is set,
 *                  since a missing key is then not an error
 * @param[in,out] clpp the parsed configuration; the consumed entry is removed from it
 * @param fields when non-nullptr, the attribute is recorded here as "was set"
 *               and a missing key is accepted rather than reported
 * @param key the configuration key to consume
 * @param[in,out] ep the object the value is stored in
 * @param name_nm the attribute of `ep` to store it in
 * @param descr the descriptor of the list elements
 * @param sub_name_nm the attribute within an element the value is stored in
 * @param master_hgroup_list the host groups, needed to resolve a host group name
 * @return false on error, otherwise true
 */
bool set_conf_strlist_attr_list(lList **alpp, lList **clpp, int fields[], 
                                const char *key, lListElem *ep, int name_nm, 
                                lDescr *descr, int sub_name_nm, const lList *master_hgroup_list) 
{
   bool ret;
   lList *tmplp = nullptr;
   const char *str;
   lList *lanswer_list = nullptr;

   DENTER(TOP_LAYER);

   if(!(str=get_conf_value(fields?nullptr:alpp, *clpp, CF_name, CF_value, key))) {
      DRETURN(fields?true:false);
   }

   ret = strlist_attr_list_parse_from_string(&tmplp, &lanswer_list, str,
                                             HOSTATTR_ALLOW_AMBIGUITY, master_hgroup_list);
   if (!ret) {
      const char *text = lGetString(lFirst(lanswer_list), AN_text);

      snprintf(SGE_EVENT, SGE_EVENT_SIZE, "%s - %s", key, text);
      answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      return ret;
   }
   lDelElemStr(clpp, CF_name, key);
   add_nm_to_set(fields, name_nm);

   if (tmplp != nullptr) {
      lSetList(ep, name_nm, tmplp);
      DRETURN(true);
   }

   DRETURN(true);
}

/**
 * @brief Consume a configuration key holding a per host list of user lists
 *
 * A cluster queue attribute is written as a list of host or host group
 * specific values; this parses that form and stores the resulting list.
 *
 * @param[out] alpp receives the error message; pass nullptr when `fields` is set,
 *                  since a missing key is then not an error
 * @param[in,out] clpp the parsed configuration; the consumed entry is removed from it
 * @param fields when non-nullptr, the attribute is recorded here as "was set"
 *               and a missing key is accepted rather than reported
 * @param key the configuration key to consume
 * @param[in,out] ep the object the value is stored in
 * @param name_nm the attribute of `ep` to store it in
 * @param descr the descriptor of the list elements
 * @param sub_name_nm the attribute within an element the value is stored in
 * @param master_hgroup_list the host groups, needed to resolve a host group name
 * @return false on error, otherwise true
 */
bool set_conf_usrlist_attr_list(lList **alpp, lList **clpp, int fields[], 
                                const char *key, lListElem *ep, int name_nm, 
                                lDescr *descr, int sub_name_nm, const lList *master_hgroup_list) 
{
   bool ret;
   lList *tmplp = nullptr;
   const char *str;
   lList *lanswer_list = nullptr;

   DENTER(TOP_LAYER);

   if(!(str=get_conf_value(fields?nullptr:alpp, *clpp, CF_name, CF_value, key))) {
      DRETURN(fields?true:false);
   }

   ret = usrlist_attr_list_parse_from_string(&tmplp, &lanswer_list, str,
                                             HOSTATTR_ALLOW_AMBIGUITY, master_hgroup_list);
   if (!ret) {
      const char *text = lGetString(lFirst(lanswer_list), AN_text);

      snprintf(SGE_EVENT, SGE_EVENT_SIZE, "%s - %s", key, text);
      answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      return ret;
   }
   lDelElemStr(clpp, CF_name, key);
   add_nm_to_set(fields, name_nm);

   if (tmplp != nullptr) {
      lSetList(ep, name_nm, tmplp);
      DRETURN(true);
   }

   DRETURN(true);
}

/**
 * @brief Consume a configuration key holding a per host list of project lists
 *
 * A cluster queue attribute is written as a list of host or host group
 * specific values; this parses that form and stores the resulting list.
 *
 * @param[out] alpp receives the error message; pass nullptr when `fields` is set,
 *                  since a missing key is then not an error
 * @param[in,out] clpp the parsed configuration; the consumed entry is removed from it
 * @param fields when non-nullptr, the attribute is recorded here as "was set"
 *               and a missing key is accepted rather than reported
 * @param key the configuration key to consume
 * @param[in,out] ep the object the value is stored in
 * @param name_nm the attribute of `ep` to store it in
 * @param descr the descriptor of the list elements
 * @param sub_name_nm the attribute within an element the value is stored in
 * @param master_hgroup_list the host groups, needed to resolve a host group name
 * @return false on error, otherwise true
 */
bool set_conf_prjlist_attr_list(lList **alpp, lList **clpp, int fields[], 
                                const char *key, lListElem *ep, int name_nm, 
                                lDescr *descr, int sub_name_nm, const lList *master_hgroup_list) 
{
   bool ret;
   lList *tmplp = nullptr;
   const char *str;
   lList *lanswer_list = nullptr;

   DENTER(TOP_LAYER);

   if(!(str=get_conf_value(fields?nullptr:alpp, *clpp, CF_name, CF_value, key))) {
      DRETURN(fields?true:false);
   }

   ret = prjlist_attr_list_parse_from_string(&tmplp, &lanswer_list, str,
                                             HOSTATTR_ALLOW_AMBIGUITY, master_hgroup_list);
   if (!ret) {
      const char *text = lGetString(lFirst(lanswer_list), AN_text);

      snprintf(SGE_EVENT, SGE_EVENT_SIZE, "%s - %s", key, text);
      answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      return ret;
   }
   lDelElemStr(clpp, CF_name, key);
   add_nm_to_set(fields, name_nm);

   if (tmplp != nullptr) {
      lSetList(ep, name_nm, tmplp);
      DRETURN(true);
   }

   DRETURN(true);
}

/**
 * @brief Consume a configuration key holding a per host list of complex entry lists
 *
 * A cluster queue attribute is written as a list of host or host group
 * specific values; this parses that form and stores the resulting list.
 *
 * @param[out] alpp receives the error message; pass nullptr when `fields` is set,
 *                  since a missing key is then not an error
 * @param[in,out] clpp the parsed configuration; the consumed entry is removed from it
 * @param fields when non-nullptr, the attribute is recorded here as "was set"
 *               and a missing key is accepted rather than reported
 * @param key the configuration key to consume
 * @param[in,out] ep the object the value is stored in
 * @param name_nm the attribute of `ep` to store it in
 * @param descr the descriptor of the list elements
 * @param sub_name_nm the attribute within an element the value is stored in
 * @param master_hgroup_list the host groups, needed to resolve a host group name
 * @return false on error, otherwise true
 */
bool set_conf_celist_attr_list(lList **alpp, lList **clpp, int fields[], 
                               const char *key, lListElem *ep, int name_nm, 
                               lDescr *descr, int sub_name_nm, const lList *master_hgroup_list) 
{
   bool ret;
   lList *tmplp = nullptr;
   const char *str;
   lList *lanswer_list = nullptr;

   DENTER(TOP_LAYER);

   if(!(str=get_conf_value(fields?nullptr:alpp, *clpp, CF_name, CF_value, key))) {
      DRETURN(fields?true:false);
   }

   ret = celist_attr_list_parse_from_string(&tmplp, &lanswer_list, str,
                                            HOSTATTR_ALLOW_AMBIGUITY, master_hgroup_list);
   if (!ret) {
      const char *text = lGetString(lFirst(lanswer_list), AN_text);

      snprintf(SGE_EVENT, SGE_EVENT_SIZE, "%s - %s", key, text);
      answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      return ret;
   }
   lDelElemStr(clpp, CF_name, key);
   add_nm_to_set(fields, name_nm);

   if (tmplp != nullptr) {
      lSetList(ep, name_nm, tmplp);
      DRETURN(true);
   }

   DRETURN(true);
}

/**
 * @brief Consume a configuration key holding a per host list of subordinate lists
 *
 * A cluster queue attribute is written as a list of host or host group
 * specific values; this parses that form and stores the resulting list.
 *
 * @param[out] alpp receives the error message; pass nullptr when `fields` is set,
 *                  since a missing key is then not an error
 * @param[in,out] clpp the parsed configuration; the consumed entry is removed from it
 * @param fields when non-nullptr, the attribute is recorded here as "was set"
 *               and a missing key is accepted rather than reported
 * @param key the configuration key to consume
 * @param[in,out] ep the object the value is stored in
 * @param name_nm the attribute of `ep` to store it in
 * @param descr the descriptor of the list elements
 * @param sub_name_nm the attribute within an element the value is stored in
 * @param master_hgroup_list the host groups, needed to resolve a host group name
 * @return false on error, otherwise true
 */
bool set_conf_solist_attr_list(lList **alpp, lList **clpp, int fields[], 
                               const char *key, lListElem *ep, int name_nm, 
                               lDescr *descr, int sub_name_nm, const lList *master_hgroup_list) 
{
   bool ret;
   lList *tmplp = nullptr;
   const char *str;
   lList *lanswer_list = nullptr;

   DENTER(TOP_LAYER);

   if(!(str=get_conf_value(fields?nullptr:alpp, *clpp, CF_name, CF_value, key))) {
      DRETURN(fields?true:false);
   }

   ret = solist_attr_list_parse_from_string(&tmplp, &lanswer_list, str,
                                            HOSTATTR_ALLOW_AMBIGUITY, master_hgroup_list);
   if (!ret) {
      const char *text = lGetString(lFirst(lanswer_list), AN_text);

      snprintf(SGE_EVENT, SGE_EVENT_SIZE, "%s - %s", key, text);
      answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      return ret;
   }
   lDelElemStr(clpp, CF_name, key);
   add_nm_to_set(fields, name_nm);

   if (tmplp != nullptr) {
      lSetList(ep, name_nm, tmplp);
      DRETURN(true);
   }

   DRETURN(true);
}

/**
 * @brief Consume a configuration key holding a per host list of queue type lists
 *
 * A cluster queue attribute is written as a list of host or host group
 * specific values; this parses that form and stores the resulting list.
 *
 * @param[out] alpp receives the error message; pass nullptr when `fields` is set,
 *                  since a missing key is then not an error
 * @param[in,out] clpp the parsed configuration; the consumed entry is removed from it
 * @param fields when non-nullptr, the attribute is recorded here as "was set"
 *               and a missing key is accepted rather than reported
 * @param key the configuration key to consume
 * @param[in,out] ep the object the value is stored in
 * @param name_nm the attribute of `ep` to store it in
 * @param descr the descriptor of the list elements
 * @param sub_name_nm the attribute within an element the value is stored in
 * @param master_hgroup_list the host groups, needed to resolve a host group name
 * @return false on error, otherwise true
 */
bool set_conf_qtlist_attr_list(lList **alpp, lList **clpp, int fields[], 
                               const char *key, lListElem *ep, int name_nm, 
                               lDescr *descr, int sub_name_nm, const lList *master_hgroup_list) 
{
   bool ret;
   lList *tmplp = nullptr;
   const char *str;
   lList *lanswer_list = nullptr;

   DENTER(TOP_LAYER);

   if(!(str=get_conf_value(fields?nullptr:alpp, *clpp, CF_name, CF_value, key))) {
      DRETURN(fields?true:false);
   }

   ret = qtlist_attr_list_parse_from_string(&tmplp, &lanswer_list, str,
                                            HOSTATTR_ALLOW_AMBIGUITY, master_hgroup_list);
   if (!ret) {
      const char *text = lGetString(lFirst(lanswer_list), AN_text);

      snprintf(SGE_EVENT, SGE_EVENT_SIZE, "%s - %s", key, text);
      answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      return ret;
   }
   lDelElemStr(clpp, CF_name, key);
   add_nm_to_set(fields, name_nm);

   if (tmplp != nullptr) {
      lSetList(ep, name_nm, tmplp);
      DRETURN(true);
   }

   DRETURN(true);
}

/**
 * @brief Consume a configuration key holding a per host list of unsigned integers
 *
 * A cluster queue attribute is written as a list of host or host group
 * specific values; this parses that form and stores the resulting list.
 *
 * @param[out] alpp receives the error message; pass nullptr when `fields` is set,
 *                  since a missing key is then not an error
 * @param[in,out] clpp the parsed configuration; the consumed entry is removed from it
 * @param fields when non-nullptr, the attribute is recorded here as "was set"
 *               and a missing key is accepted rather than reported
 * @param key the configuration key to consume
 * @param[in,out] ep the object the value is stored in
 * @param name_nm the attribute of `ep` to store it in
 * @param descr the descriptor of the list elements
 * @param sub_name_nm the attribute within an element the value is stored in
 * @param master_hgroup_list the host groups, needed to resolve a host group name
 * @return false on error, otherwise true
 */
bool set_conf_ulng_attr_list(lList **alpp, lList **clpp, int fields[], 
                             const char *key, lListElem *ep, int name_nm, 
                             lDescr *descr, int sub_name_nm, const lList *master_hgroup_list) 
{
   bool ret;
   lList *tmplp = nullptr;
   const char *str;
   lList *lanswer_list = nullptr;

   DENTER(TOP_LAYER);

   if(!(str=get_conf_value(fields?nullptr:alpp, *clpp, CF_name, CF_value, key))) {
      DRETURN(fields?true:false);
   }
   ret = ulng_attr_list_parse_from_string(&tmplp, &lanswer_list, str,
                                          HOSTATTR_ALLOW_AMBIGUITY, master_hgroup_list);
   if (!ret) {
      const char *text = lGetString(lFirst(lanswer_list), AN_text);

      snprintf(SGE_EVENT, SGE_EVENT_SIZE, "%s - %s", key, text);
      answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      return ret;
   }
   lDelElemStr(clpp, CF_name, key);
   add_nm_to_set(fields, name_nm);

   if (tmplp != nullptr) {
      lSetList(ep, name_nm, tmplp);
      DRETURN(true);
   }

   DRETURN(true);
}

/**
 * @brief Consume a configuration key holding a per host list of booleans
 *
 * A cluster queue attribute is written as a list of host or host group
 * specific values; this parses that form and stores the resulting list.
 *
 * @param[out] alpp receives the error message; pass nullptr when `fields` is set,
 *                  since a missing key is then not an error
 * @param[in,out] clpp the parsed configuration; the consumed entry is removed from it
 * @param fields when non-nullptr, the attribute is recorded here as "was set"
 *               and a missing key is accepted rather than reported
 * @param key the configuration key to consume
 * @param[in,out] ep the object the value is stored in
 * @param name_nm the attribute of `ep` to store it in
 * @param descr the descriptor of the list elements
 * @param sub_name_nm the attribute within an element the value is stored in
 * @param master_hgroup_list the host groups, needed to resolve a host group name
 * @return false on error, otherwise true
 */
bool set_conf_bool_attr_list(lList **alpp, lList **clpp, int fields[], 
                             const char *key, lListElem *ep, int name_nm, 
                             lDescr *descr, int sub_name_nm, const lList *master_hgroup_list) 
{
   bool ret;
   lList *tmplp = nullptr;
   const char *str;
   lList *lanswer_list = nullptr;

   DENTER(TOP_LAYER);

   if(!(str=get_conf_value(fields?nullptr:alpp, *clpp, CF_name, CF_value, key))) {
      DRETURN(fields?true:false);
   }
   ret = bool_attr_list_parse_from_string(&tmplp, &lanswer_list, str,
                                          HOSTATTR_ALLOW_AMBIGUITY, master_hgroup_list);
   if (!ret) {
      const char *text = lGetString(lFirst(lanswer_list), AN_text);

      snprintf(SGE_EVENT, SGE_EVENT_SIZE, "%s - %s", key, text);
      answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      return ret;
   }
   lDelElemStr(clpp, CF_name, key);
   add_nm_to_set(fields, name_nm);

   if (tmplp != nullptr) {
      lSetList(ep, name_nm, tmplp);
      DRETURN(true);
   }

   DRETURN(true);
}

/**
 * @brief Consume a configuration key holding a per host list of time specifications
 *
 * A cluster queue attribute is written as a list of host or host group
 * specific values; this parses that form and stores the resulting list.
 *
 * @param[out] alpp receives the error message; pass nullptr when `fields` is set,
 *                  since a missing key is then not an error
 * @param[in,out] clpp the parsed configuration; the consumed entry is removed from it
 * @param fields when non-nullptr, the attribute is recorded here as "was set"
 *               and a missing key is accepted rather than reported
 * @param key the configuration key to consume
 * @param[in,out] ep the object the value is stored in
 * @param name_nm the attribute of `ep` to store it in
 * @param descr the descriptor of the list elements
 * @param sub_name_nm the attribute within an element the value is stored in
 * @param master_hgroup_list the host groups, needed to resolve a host group name
 * @return false on error, otherwise true
 */
bool set_conf_time_attr_list(lList **alpp, lList **clpp, int fields[], 
                             const char *key, lListElem *ep, int name_nm, 
                             lDescr *descr, int sub_name_nm, const lList *master_hgroup_list) 
{
   bool ret;
   lList *tmplp = nullptr;
   const char *str;
   lList *lanswer_list = nullptr;

   DENTER(TOP_LAYER);

   if(!(str=get_conf_value(fields?nullptr:alpp, *clpp, CF_name, CF_value, key))) {
      DRETURN(fields?true:false);
   }
   ret = time_attr_list_parse_from_string(&tmplp, &lanswer_list, str,
                                          HOSTATTR_ALLOW_AMBIGUITY, master_hgroup_list);
   if (!ret) {
      const char *text = lGetString(lFirst(lanswer_list), AN_text);

      snprintf(SGE_EVENT, SGE_EVENT_SIZE, "%s - %s", key, text);
      answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      return ret;
   }
   lDelElemStr(clpp, CF_name, key);
   add_nm_to_set(fields, name_nm);

   if (tmplp != nullptr) {
      lSetList(ep, name_nm, tmplp);
      DRETURN(true);
   }

   DRETURN(true);
}

/**
 * @brief Consume a configuration key holding a per host list of memory specifications
 *
 * A cluster queue attribute is written as a list of host or host group
 * specific values; this parses that form and stores the resulting list.
 *
 * @param[out] alpp receives the error message; pass nullptr when `fields` is set,
 *                  since a missing key is then not an error
 * @param[in,out] clpp the parsed configuration; the consumed entry is removed from it
 * @param fields when non-nullptr, the attribute is recorded here as "was set"
 *               and a missing key is accepted rather than reported
 * @param key the configuration key to consume
 * @param[in,out] ep the object the value is stored in
 * @param name_nm the attribute of `ep` to store it in
 * @param descr the descriptor of the list elements
 * @param sub_name_nm the attribute within an element the value is stored in
 * @param master_hgroup_list the host groups, needed to resolve a host group name
 * @return false on error, otherwise true
 */
bool set_conf_mem_attr_list(lList **alpp, lList **clpp, int fields[], 
                             const char *key, lListElem *ep, int name_nm, 
                             lDescr *descr, int sub_name_nm, const lList *master_hgroup_list) 
{
   bool ret;
   lList *tmplp = nullptr;
   const char *str;
   lList *lanswer_list = nullptr;

   DENTER(TOP_LAYER);

   if(!(str=get_conf_value(fields?nullptr:alpp, *clpp, CF_name, CF_value, key))) {
      DRETURN(fields?true:false);
   }
   ret = mem_attr_list_parse_from_string(&tmplp, &lanswer_list, str,
                                          HOSTATTR_ALLOW_AMBIGUITY, master_hgroup_list);
   if (!ret) {
      const char *text = lGetString(lFirst(lanswer_list), AN_text);

      snprintf(SGE_EVENT, SGE_EVENT_SIZE, "%s - %s", key, text);
      answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      return ret;
   }
   lDelElemStr(clpp, CF_name, key);
   add_nm_to_set(fields, name_nm);

   if (tmplp != nullptr) {
      lSetList(ep, name_nm, tmplp);
      DRETURN(true);
   }

   DRETURN(true);
}

/**
 * @brief Consume a configuration key holding a per host list of intervals
 *
 * A cluster queue attribute is written as a list of host or host group
 * specific values; this parses that form and stores the resulting list.
 *
 * @param[out] alpp receives the error message; pass nullptr when `fields` is set,
 *                  since a missing key is then not an error
 * @param[in,out] clpp the parsed configuration; the consumed entry is removed from it
 * @param fields when non-nullptr, the attribute is recorded here as "was set"
 *               and a missing key is accepted rather than reported
 * @param key the configuration key to consume
 * @param[in,out] ep the object the value is stored in
 * @param name_nm the attribute of `ep` to store it in
 * @param descr the descriptor of the list elements
 * @param sub_name_nm the attribute within an element the value is stored in
 * @param master_hgroup_list the host groups, needed to resolve a host group name
 * @return false on error, otherwise true
 */
bool set_conf_inter_attr_list(lList **alpp, lList **clpp, int fields[], 
                              const char *key, lListElem *ep, int name_nm, 
                              lDescr *descr, int sub_name_nm, const lList *master_hgroup_list) 
{
   bool ret;
   lList *tmplp = nullptr;
   const char *str;
   lList *lanswer_list = nullptr;

   DENTER(TOP_LAYER);

   if(!(str=get_conf_value(fields?nullptr:alpp, *clpp, CF_name, CF_value, key))) {
      DRETURN(fields?true:false);
   }
   ret = inter_attr_list_parse_from_string(&tmplp, &lanswer_list, str,
                                           HOSTATTR_ALLOW_AMBIGUITY, master_hgroup_list);

   if (!ret) {
      const char *text = lGetString(lFirst(lanswer_list), AN_text);

      snprintf(SGE_EVENT, SGE_EVENT_SIZE, "%s - %s", key, text);
      answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
      return ret;
   }
   lDelElemStr(clpp, CF_name, key);
   add_nm_to_set(fields, name_nm);

   if (tmplp != nullptr) {
      lSetList(ep, name_nm, tmplp);
      DRETURN(true);
   }

   DRETURN(true);
}

/****
 **** set_conf_subordlist
 ****
 **** 'set_conf_subordlist' searches in the configuration list
 **** (pointed to by clpp) for a subordinate-list-config 
 **** value with 'key' as name.
 **** The subordinate-list looks like:
 **** name1=val1,name2,name3,name4=val2
 **** The divider between two entries are ',', space or tab,
 **** between name and value are '=' or ':'.
 **** If the list is found, it is stored in the lListElem
 **** 'ep' in the fied specified by 'name_nm'.
 **** The sub-list is created of type 'descr', the name of
 **** the subord-list-field is stored in the field
 **** 'subname_nm', it's value in 'subval_nm'.
 **** If the subordinate list is 'NONE', no list will
 **** be created (null-pointer!).
 ****
 **** The function returns false on error, otherwise true.
 ****/
/**
 * @brief Consume a configuration key and store its value as a subordinate list
 *
 * The entry is removed from the parsed configuration, so what is left over
 * afterwards is exactly the set of keys nobody understood.
 *
 * @param[out] alpp receives the error message; pass nullptr when `fields` is set,
 *                  since a missing key is then not an error
 * @param[in,out] clpp the parsed configuration; the consumed entry is removed from it
 * @param fields when non-nullptr, the attribute is recorded here as "was set"
 *               and a missing key is accepted rather than reported
 * @param key the configuration key to consume
 * @param[in,out] ep the object the value is stored in
 * @param name_nm the attribute of `ep` to store it in
 * @param descr the descriptor of the list elements
 * @param subname_nm the attribute holding a subordinate queue name
 * @param subval_nm the attribute holding its slot count
 * @return false on error, otherwise true
 */
bool set_conf_subordlist(
lList **alpp,
lList **clpp,
int fields[],
const char *key,
lListElem *ep,
int name_nm,
lDescr *descr,
int subname_nm,
int subval_nm 
) {
   DENTER(CULL_LAYER);
   lList *tmplp = nullptr;
   const char *str;
   const char *s;
   char *endptr;

   if(!(str=get_conf_value(fields?nullptr:alpp, *clpp, CF_name, CF_value, key))) {
      DRETURN(fields?true:false);
   }
   lString2List(str, &tmplp, descr, subname_nm, ", \t");
   for_each_rw_lv(tmpep, tmplp) {
      s = sge_strtok(lGetString(tmpep, subname_nm), ":=");
      lSetString(tmpep, subname_nm, s);
      if (!(s=sge_strtok(nullptr, ":=")))
         continue;
      lSetUlong(tmpep, subval_nm, strtol(s, &endptr, 10));
      if (*endptr) {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_GDI_CONFIGREADFILEERRORNEAR_SS , key, endptr);
         answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
         DRETURN(false);
      }
   }
   
   if (!strcasecmp("NONE", lGetString(lFirst(tmplp), subname_nm))) {
      lFreeList(&tmplp);
   }   

   lSetList(ep, name_nm, tmplp);
   lDelElemStr(clpp, CF_name, key);
   add_nm_to_set(fields, name_nm);

   DRETURN(true);
}


/* 
   Append 'name_nm' into the int array 'fields'.
   In case the 'name_nm' is already contained
   in 'fields' -1 is returned. 
*/

/**
 * @brief Record that an attribute was set, refusing a duplicate
 *
 * The `fields` array is how a caller learns which attributes a configuration
 * actually mentioned, so that the rest can be left at their previous values.
 *
 * @param[in,out] fields the `NoName` terminated array to append to; nullptr is
 *                       accepted and does nothing
 * @param name_nm the attribute to record
 * @return 0 on success, -1 when the attribute was already recorded
 */
int add_nm_to_set(
int fields[],
int name_nm 
) {
   int i = 0;

   DENTER(CULL_LAYER);

   if (!fields) {
      DRETURN(0); /* we leave here in most cases */
   }

   /* seek end and check whether 'name_nm' 
      is already in 'fields' */
   while (fields[i]!=NoName && fields[i]!=name_nm)
      i++;
   
   if (fields[i]==name_nm) {
      DRETURN(-1);
   }

   fields[i] = name_nm;      
   fields[++i] = NoName;      

   DRETURN(0);
}

