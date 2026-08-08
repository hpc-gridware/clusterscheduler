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
 * @brief The product name in the forms the clients print it
 *
 * @see sge_feature.h
 */

#include <cstring>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/ocs_Version.h"

#include "sgeobj/sge_feature.h"         

/**
 * @brief Get product name string
 *
 * This function will return a text string containing the
 * product name. The return value depends on the style
 * parameter. An invalid style value will automatically be
 * interpreted as FS_SHORT.
 *
 * @param style FS_SHORT         = return short name FS_LONG          = return long name FS_VERSION       = return version FS_SHORT_VERSION = return short name and version FS_LONG_VERSION  = return long name and version
 * @param buffer buffer provided by caller the string returned by this function points to this buffer
 *
 * @return string
 *
 * @note MT-NOTE: feature_get_product_name() is MT safe
 */
const char *feature_get_product_name(featureset_product_name_id_t style, dstring *buffer) {
   DENTER(TOP_LAYER);

   const char *ret = nullptr;
   const std::string short_name = ocs::Version::get_short_product_name();
   const std::string long_name = ocs::Version::get_long_product_name();
   const std::string version = ocs::Version::get_version_string();

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
   if (CMAKE_BUILD_ID != nullptr && strlen(CMAKE_BUILD_ID) > 0) {
      ret = sge_dstring_sprintf_append(buffer, " (%s)", CMAKE_BUILD_ID);
   }
#endif

   DRETURN(ret);
}
