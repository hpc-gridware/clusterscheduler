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
#include <cstdio>
#include <cstdlib>
#include <pwd.h>

/* for service provider info (SPI) entries and projects */

#include "uti/config_file.h"
#include "uti/sge_stdio.h"

#include "basis_types.h"
#include "err_trace.h"
#include "setosjobid.h"

#include <uti/sge.h>

void setosjobid(pid_t sid, gid_t *add_grp_id_ptr, struct passwd *pw)
{
   shepherd_trace("setosjobid: uid = " pid_t_fmt ", euid = " pid_t_fmt, getuid(), geteuid());

   FILE *fp = nullptr;

#  if defined(SOLARIS) || defined(LINUX) || defined(FREEBSD) || defined(DARWIN)
      /* Read SgeId from config-File and create Addgrpid-File */
      {
         char *cp = search_conf_val("add_grp_id");
         if (cp != nullptr) {
            *add_grp_id_ptr = atol(cp);
         } else {
            *add_grp_id_ptr = 0;
         }
      }
      fp = fopen(ADDGRPID, "w");
      if (fp == nullptr) {
         shepherd_error(1, "can't open \"addgrpid\" file");
      }
      fprintf(fp, gid_t_fmt"\n", *add_grp_id_ptr);
      FCLOSE(fp);   
#  else
   {
      char osjobid[100];
      if ((fp = fopen(OSJOBID, "w")) == nullptr) {
         shepherd_error(1, "can't open \"osjobid\" file");
      }

      if(sge_switch2start_user() == 0) {
         /* write a default os-jobid to file */
         sprintf(osjobid, pid_t_fmt, sid);
         sge_switch2admin_user();
      } 
      else /* not running as super user --> we want a default os-jobid */
         sprintf(osjobid, "0");
      
      if(fprintf(fp, "%s\n", osjobid) < 0)
         shepherd_trace("error writing osjobid file");
         
      FCLOSE(fp); /* Close os-jobid file */   
   }
#  endif
   return;
FCLOSE_ERROR:
   shepherd_error(1, "can't close file"); 
}
