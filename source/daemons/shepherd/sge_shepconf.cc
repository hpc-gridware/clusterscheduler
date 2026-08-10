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
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Reading the shepherd's own configuration: methods, signals and notification
 */
#include <cstring>
#include <csignal>

#include "uti/config_file.h"
#include "uti/sge_signal.h"
#include "uti/sge_string.h"

#include "signal_queue.h"
#include "sge_shepconf.h"
#include "err_trace.h"

/** @brief Is a user defined method configured under this name?
 *
 * True only when the configured value is an absolute path; a value that is a
 * signal name belongs to shepconf_has_userdef_signal() instead, and the two
 * are how the same configuration entry can mean either.
 *
 * @param method_name "starter_method", "suspend_method", "resume_method" or
 *        "terminate_method"
 * @param[out] method receives the absolute filename of the method
 * @return true when a method is configured
 */
int shepconf_has_userdef_method(const char *method_name, dstring *method) {
   char *conf_val = search_nonone_conf_val(method_name);
   int ret = 0;

   if (conf_val != nullptr && conf_val[0] == '/') {
      sge_dstring_copy_string(method, conf_val);
      ret = 1;
   }
   return ret;
}

/** @brief Is a signal name configured under this name?
 *
 * The counterpart of shepconf_has_userdef_method(): the same entry may hold
 * either a path to run or a signal to send.
 *
 * @param method_name "starter_method", "suspend_method", "resume_method" or
 *        "terminate_method"
 * @param[out] signal receives the signal id
 * @return true when a signal is configured
 */
int shepconf_has_userdef_signal(const char *method_name, int *signal) {
   char *conf_val = search_nonone_conf_val(method_name);
   int ret = 0;

   if (conf_val != nullptr && conf_val[0] != '/') {
      *signal = shepherd_sys_str2signal(conf_val);
      ret = 1;
   }
   return ret;
}

/** @brief Is the notification mechanism enabled, and with which signal?
 *
 * A job may ask to be warned before it is suspended or killed, so that it can
 * save its work.
 *
 * @param notify_name "notify_susp" or "notify_kill"
 * @param[out] signal receives the signal id, default or user defined
 * @return true when notification is enabled
 */
int shepconf_has_notify_signal(const char *notify_name, int *signal) {
   const char *notify_array[] = {
      "notify_susp", "notify_kill", nullptr
   };
   int signal_array[] = {
      SIGUSR1, SIGUSR2, 0
   };
   dstring param_name = DSTRING_INIT;
   char *conf_type = nullptr;
   int conf_id;
   int ret = 0;

   /*
    * There are three possibilities:
    *    a) There is a user defined signal which should be used
    *    b) Default signal should be used
    *    c) Notification mechanism is disabled
    */
   sge_dstring_sprintf(&param_name, "%s%s", notify_name, "_type");
   conf_type = search_conf_val(sge_dstring_get_string(&param_name));
   sge_dstring_free(&param_name);
   if (conf_type != nullptr) {
      conf_id = atol(conf_type);
   } else {
      conf_id = 1;   /* Default signal should be used */
   }

   if (conf_id == 0) {
      char *conf_signal = search_conf_val(notify_name);

      if (conf_signal != nullptr) {
         *signal = sge_sys_str2signal(conf_signal);
         ret = 1;
      }
   } else if (conf_id == 1) {
      int i;

      for (i = 0; notify_array[i] != nullptr; i++) {
         if (!strcmp(notify_array[i], notify_name)) {
            break;
         }
      }
      *signal = signal_array[i];
      ret = 1;
   } else {
      *signal = 0;
      ret = 0;
   }
   return ret;
}

/** @brief How long to wait between the warning and the real signal
 * @param[out] seconds receives the delay
 * @return true when a delay is configured
 */
int shepconf_has_to_notify_before_signal(int *seconds) {
   *seconds = atoi(get_conf_val("notify"));

   return (*seconds > 0);
}
