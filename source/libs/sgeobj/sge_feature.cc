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
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstring>
#include <pthread.h>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "cull/cull.h"

#include "sgeobj/ocs_Version.h"

#include "sgeobj/sge_feature.h"         
#include "sgeobj/msg_sgeobjlib.h"
#include "sgeobj/sge_answer.h"

#include "basis_types.h"

struct feature_state_t {
    int    already_read_from_file;
    lList* Master_FeatureSet_List;
};

static const featureset_names_t featureset_list[] = {
   {FEATURE_NO_SECURITY,            "none"},
   {FEATURE_AFS_SECURITY,           "afs"},
   {FEATURE_DCE_SECURITY,           "dce"},
   {FEATURE_KERBEROS_SECURITY,      "kerberos"},
   {FEATURE_CSP_SECURITY,           "csp"},
   {FEATURE_TLS_SECURITY,           "tls"},
   {FEATURE_MUNGE_SECURITY,         "munge"},
   {FEATURE_UNINITIALIZED,          nullptr}
};
/* *INDENT-ON* */

static pthread_once_t feature_once = PTHREAD_ONCE_INIT;
static pthread_key_t feature_state_key;

static void feature_once_init();
static void feature_state_destroy(void* theState);
static void feature_state_init(struct feature_state_t* theState);
static feature_id_t feature_get_featureset_id(const char *name); 


/****** sgeobj/feature/feature_mt_init() **************************************
*  NAME
*     feature_mt_init() -- Initialize feature code for multi threading use.
*
*  SYNOPSIS
*     void feature_mt_init() 
*
*  FUNCTION
*     Set up feature code. This function must be called at least once before
*     any of the feature oriented functions can be used. This function is
*     idempotent, i.e. it is safe to call it multiple times.
*
*     Thread local storage for the feature code state information is reserved. 
*
*  INPUTS
*     void - NONE 
*
*  RESULT
*     void - NONE
*
*  NOTES
*     MT-NOTE: feature_mt_init() is MT safe 
*******************************************************************************/
static void feature_mt_init()
{
   pthread_once(&feature_once, feature_once_init);
}

class FeatureThreadInit {
public:
   FeatureThreadInit() {
      feature_mt_init();
   }
};

// although not used the constructor call has the side effect to initialize the pthread_key => do not delete
static FeatureThreadInit feature_obj{};

/****** sgeobj/feature/feature_set_already_read_from_file() *******************
*  NAME
*     feature_set_already_read_from_file()
*
*  SYNOPSIS
*     void feature_set_already_read_from_file(int i)
*
*  FUNCTION
*     Set per thread global variable already_read_from_file.
*******************************************************************************/
void feature_set_already_read_from_file(int i)
{
   GET_SPECIFIC(struct feature_state_t, feature_state, feature_state_init, feature_state_key);
   feature_state->already_read_from_file = i;
}

/****** sgeobj/feature/feature_get_already_read_from_file() *******************
*  NAME
*     feature_get_already_read_from_file()
*
*  SYNOPSIS
*     void feature_get_already_read_from_file(int i)
*
*  RESULT
*     Returns value of per thread global variable already_read_from_file.
*******************************************************************************/
int feature_get_already_read_from_file()
{
   GET_SPECIFIC(struct feature_state_t, feature_state, feature_state_init, feature_state_key);
   return feature_state->already_read_from_file;
}

/****** sgeobj/feature/feature_get_master_featureset_list() *******************
*  NAME
*     feature_get_master_featureset_list()
*
*  SYNOPSIS
*     lList **feature_get_master_featureset_list()
*
*  RESULT
*     Returns pointer to location where per thread global list 
*     Master_FeatureSet_List is stored.
*******************************************************************************/
lList **feature_get_master_featureset_list()
{
   GET_SPECIFIC(struct feature_state_t, feature_state, feature_state_init, feature_state_key);
   return &(feature_state->Master_FeatureSet_List);
}

/****** sgeobj/feature/feature_initialize_from_string() ***********************
*  NAME
*     feature_initialize_from_string() -- tag one featureset as active 
*
*  SYNOPSIS
*     int feature_initialize_from_string(char *mode) 
*
*  FUNCTION
*     This function interprets the mode string and tags the 
*     corresponding featureset enty within the Master_FeatureSet_List 
*     as active.  
*
*  INPUTS
*     char *mode - product mode string (valid strings are defined in
*                  the arry featureset_list[])
*
*  RESULT
*     0 OK
*    -3 unknown mode-string
*
*  NOTES
*     MT-NOTE: feature_initialize_from_string() is MT safe
******************************************************************************/
int feature_initialize_from_string(const char *mode, lList **answer_list)
{
   DENTER(TOP_LAYER);
   feature_id_t id = feature_get_featureset_id(mode);
   int ret;

   if (id == FEATURE_UNINITIALIZED) {
      CRITICAL(MSG_GDI_INVALIDPRODUCTMODESTRING_S, mode);
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_CRITICAL);
      ret = -3;
   } else {
      feature_activate(id);
      ret = 0;
   }
   DRETURN(ret);
}

/****** sgeobj/feature/feature_initialize() ***********************************
*  NAME
*     feature_initialize() -- initialize this module
*
*  SYNOPSIS
*     static void feature_initialize()
*
*  FUNCTION
*     build up the CULL list "Master_FeatureSet_List" (FES_Type) with
*     information found in the array "enabled_features_mask"
*
*  INPUTS
*     static array enabled_features_mask[][]
*
*  RESULT
*     initialized Master_FeatureSet_List
*
*  NOTES
*     MT-NOTE: feature_initialize() is MT safe
******************************************************************************/
void feature_initialize()
{
   if (!*feature_get_master_featureset_list()) {
      lListElem *featureset;
      int featureset_id;

      for(featureset_id = 0;
          featureset_id < FEATURE_LAST_ENTRY;
          featureset_id++) {
         featureset = lAddElemUlong(feature_get_master_featureset_list(), FES_id,
                                  featureset_id, FES_Type);
         lSetUlong(featureset, FES_active, 0);
      }
   }
}

/****** sgeobj/feature/feature_activate() *************************************
*  NAME
*     feature_activate() -- switches the active featureset 
*
*  SYNOPSIS
*     void feature_activate(featureset_ id) 
*
*  FUNCTION
*     Marks the current active featureset within the 
*     Master_FeatureSet_List as inactive and flags the featureset 
*     given as parameter as active.
*      
*
*  INPUTS
*     id - feature set constant 
*
*  RESULT
*     modifies the Master_FeatureSet_List  
*
*  NOTES
*     MT-NOTE: feature_activate() is MT safe
******************************************************************************/
void feature_activate(feature_id_t id) 
{
   lList **feature_list_pp;
   lList *feature_list;
   lListElem *active_set;
   lListElem *inactive_set;

   DENTER(TOP_LAYER);

   /* get fature list, if it hasn't been initialized yet, do so */
   feature_list_pp = feature_get_master_featureset_list();
   feature_list = *feature_list_pp;
   if (feature_list == nullptr) {
      feature_initialize();
      feature_list_pp = feature_get_master_featureset_list();
      feature_list = *feature_list_pp;
   }

   /* get the feature we want to activate */
   inactive_set = lGetElemUlongRW(feature_list, FES_id, id);
   /* get the feature that is currently activated */
   active_set = lGetElemUlongRW(feature_list, FES_active, 1L);

   /* if both exist, we have to deactivate the former active, activate the new active */
   if (inactive_set && active_set) {
      lSetUlong(active_set, FES_active, 0);
      lSetUlong(inactive_set, FES_active, 1);

      if (lGetUlong(active_set, FES_id) != id) {
         WARNING(MSG_GDI_SWITCHFROMTO_SS, feature_get_featureset_name((feature_id_t)lGetUlong(active_set, FES_id)), feature_get_featureset_name(id));
      }
   } else if (inactive_set) {
      /* there was no active feature before */
      lSetUlong(inactive_set, FES_active, 1);
   }

   DRETURN_VOID;
}

/****** sgeobj/feature/feature_get_active_featureset_id() *********************
*  NAME
*     feature_get_active_featureset_id() -- current active featureset 
*
*  SYNOPSIS
*     feature_id_t feature_get_active_featureset_id() 
*
*  FUNCTION
*     return an id of the current active featureset 
*
*  RESULT
*     feature_id_t - (find the definition in the .h file)
*
*  NOTES
*     MT-NOTE: feature_get_active_featureset_id() is MT safe
******************************************************************************/
feature_id_t feature_get_active_featureset_id() 
{
   const lListElem *feature;
   feature_id_t ret = FEATURE_UNINITIALIZED;
   lList **featurelist_pp = nullptr;

   DENTER(TOP_LAYER);

   featurelist_pp = feature_get_master_featureset_list();
   if (featurelist_pp != nullptr) {
      for_each_ep(feature, *featurelist_pp) {
         if (lGetUlong(feature, FES_active)) {
            ret = (feature_id_t)lGetUlong(feature, FES_id);
            break;
         }
      }
   }
   DRETURN(ret);  
}

/****** sgeobj/feature/feature_get_featureset_name() **************************
*  NAME
*     feature_get_featureset_name() -- return the product mode string
*
*  SYNOPSIS
*     char* feature_get_featureset_name(feature_id_t id) 
*
*  FUNCTION
*     returns the corresponding modestring for a featureset constant 
*
*  INPUTS
*     feature_id_t id - constant
*
*  RESULT
*     mode string 
*
*  NOTES
*     MT-NOTE: feature_get_featureset_name() is MT safe
******************************************************************************/
const char *feature_get_featureset_name(feature_id_t id) 
{
   int i = 0;
   const char *ret = "<<unknown>>";

   DENTER(TOP_LAYER);
   while (featureset_list[i].name && featureset_list[i].id != id) {
      i++;
   }
   if (featureset_list[i].name) {
      ret = featureset_list[i].name;
   } 
   DRETURN(ret); 
}

/****** sgeobj/feature/feature_get_featureset_id() ****************************
*  NAME
*     feature_get_featureset_id() -- Value for a featureset string 
*
*  SYNOPSIS
*     feature_id_t feature_get_featureset_id(char* name) 
*
*  FUNCTION
*     This function returns the corresponding enum value for
*     a given featureset string 
*
*  INPUTS
*     char* name - feature set name earlier known as product 
*                  mode string 
*
*  RESULT
*     feature_id_t 
*
*  NOTES
*     MT-NOTE: feature_get_featureset_id() is MT safe
******************************************************************************/
static feature_id_t feature_get_featureset_id(const char *name) 
{
   int i = 0;
   feature_id_t ret = FEATURE_UNINITIALIZED;

   DENTER(TOP_LAYER);
   if (!name) {
      DRETURN(ret);
   }

   while (featureset_list[i].name && strcmp(featureset_list[i].name, name)) {
      i++;
   }
   if (featureset_list[i].name) {
      ret = featureset_list[i].id;
   } 
   DRETURN(ret);
}

/****** sgeobj/feature/feature_is_enabled() ***********************************
*  NAME
*     feature_is_enabled() -- 0/1 whether the feature is enabled 
*
*  SYNOPSIS
*     bool feature_is_enabled(feature_id_t id) 
*
*  FUNCTION
*     return true or false whether the given feature is enabled or 
*     disabled in the current active featureset 
*
*  INPUTS
*     feature_id_t id 
*
*  RESULT
*     true or false
*
*  NOTES
*     MT-NOTE: feature_is_enabled() is MT safe
******************************************************************************/
bool feature_is_enabled(feature_id_t id) 
{
   const lListElem *active_set;
   bool ret = false;

   DENTER(BASIS_LAYER);
   active_set = lGetElemUlong(*feature_get_master_featureset_list(), FES_active, 1);
   if (active_set) {
      if ( lGetUlong(active_set, FES_id) == id ) {
         ret = true;
      }
   }
   DRETURN(ret);
}  
 
/****** sgeobj/feature/feature_get_product_name() *****************************
*  NAME
*     feature_get_product_name() -- get product name string
*
*  SYNOPSIS
*     char* feature_get_product_name(featureset_product_name_id_t style)
*
*  FUNCTION
*     This function will return a text string containing the
*     product name. The return value depends on the style
*     parameter. An invalid style value will automatically be
*     interpreted as FS_SHORT.
*
*  INPUTS
*     style     - FS_SHORT         = return short name
*                 FS_LONG          = return long name
*                 FS_VERSION       = return version
*                 FS_SHORT_VERSION = return short name and version
*                 FS_LONG_VERSION  = return long name and version
*     buffer    - buffer provided by caller 
*                 the string returned by this function points to 
*                 this buffer
*
*  RESULT
*     char* - string
*
*  NOTES
*     MT-NOTE: feature_get_product_name() is MT safe
******************************************************************************/
const char *feature_get_product_name(featureset_product_name_id_t style, dstring *buffer)
{
   DENTER(TOP_LAYER);

   std::string long_name;
   std::string short_name;
   std::string version;
   const char *ret = nullptr;

   if (feature_get_active_featureset_id() != FEATURE_UNINITIALIZED ) {
      short_name = ocs::Version::get_short_product_name();
      long_name  = ocs::Version::get_long_product_name();
   }
   version = ocs::Version::get_version_string();

   switch (style) {
      case FS_SHORT:
         ret = sge_dstring_copy_string(buffer, short_name.c_str());
         break;

      case FS_LONG:
         ret = sge_dstring_copy_string(buffer, long_name.c_str());
         break;

      case FS_VERSION:
         ret = sge_dstring_copy_string(buffer, version.c_str());
         break;

      case FS_SHORT_VERSION:
         ret = sge_dstring_sprintf(buffer, "" SFN " " SFN "", short_name.c_str(), version.c_str());
         break;

      case FS_LONG_VERSION:
         ret = sge_dstring_sprintf(buffer, "" SFN " " SFN "", long_name.c_str(), version.c_str());
         break;

      default:
         ret = sge_dstring_copy_string(buffer, short_name.c_str());
         break;
   }
#ifdef CMAKE_BUILD_ID
   if (CMAKE_BUILD_ID != NULL && strlen(CMAKE_BUILD_ID) > 0) {
      ret = sge_dstring_sprintf_append(buffer, " (%s)", CMAKE_BUILD_ID);
   }
#endif

   DRETURN(ret);
}

/****** sgeobj/feature/feature_once_init() ************************************
*  NAME
*     feature_once_init() -- One-time feature code initialization.
*
*  SYNOPSIS
*     static feature_once_init() 
*
*  FUNCTION
*     Create access key for thread local storage. Register cleanup function.
*
*     This function must be called exactly once.
*
*  INPUTS
*     void - none
*
*  RESULT
*     void - none 
*
*  NOTES
*     MT-NOTE: feature_once_init() is MT safe. 
*******************************************************************************/
static void feature_once_init()
{
   pthread_key_create(&feature_state_key, feature_state_destroy);
}

/****** sgeobj/feature/feature_state_destroy() ********************************
*  NAME
*     feature_state_destroy() -- Free thread local storage
*
*  SYNOPSIS
*     static void feature_state_destroy(void* theState) 
*
*  FUNCTION
*     Free thread local storage.
*
*  INPUTS
*     void* theState - Pointer to memory which should be freed.
*
*  RESULT
*     static void - none
*
*  NOTES
*     MT-NOTE: feature_state_destroy() is MT safe.
*******************************************************************************/
static void feature_state_destroy(void* theState)
{
   struct feature_state_t* state = (struct feature_state_t *)theState;

   lFreeList(&(state->Master_FeatureSet_List));
   sge_free(&state);
}

/****** sgeobj/feature/feature_state_init() ***********************************
*  NAME
*     feature_state_init() -- Initialize feature code state.
*
*  SYNOPSIS
*     static void feature_state_init(struct feature_state_t* theState) 
*
*  FUNCTION
*     Initialize feature code state.
*
*  INPUTS
*     struct feature_state_t* theState - Pointer to feature state structure.
*
*  RESULT
*     static void - none
*
*  NOTES
*     MT-NOTE: feature_state_init() is MT safe. 
*******************************************************************************/
static void feature_state_init(struct feature_state_t* theState)
{
   memset(theState, 0, sizeof(struct feature_state_t));
}

