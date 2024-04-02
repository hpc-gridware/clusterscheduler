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
#include <cstdio>
#include <fcntl.h>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_unistd.h"

#include "lock.h"

/*-------------------------------------------------------------
 * Name:   qmaster_lock
 * Return: 0 if creation of file was successful
 *         -1 if file couldn't be created
 *-------------------------------------------------------------*/
int qmaster_lock(
const char *file
) {
   int fd;

   DENTER(TOP_LAYER);

   fd = SGE_OPEN3(file, O_RDWR | O_CREAT | O_EXCL, 0600);
   if (fd == -1) {
      DRETURN(-1);
   }
   else {
      close(fd);
      DRETURN(0);
   }
}

/*-------------------------------------------------------------
 * Name:   qmaster_unlock
 * Return: 0 if file could be unlinked
 *         -1 if file couldn't be unlinked
 *-------------------------------------------------------------*/
int qmaster_unlock(
const char *file
) {
   int ret;

   DENTER(TOP_LAYER);

   ret = unlink(file);

   DRETURN(ret);
}

/*-------------------------------------------------------------
 * Name:   isLocked
 * Return: 1 if file exists
 *         0 if file doesn't exist
 *-------------------------------------------------------------*/
int isLocked(
const char *file
) {
   int ret;
   SGE_STRUCT_STAT sb;

   DENTER(TOP_LAYER);

   ret = SGE_STAT(file, &sb);
   ret = ret ? 0 : 1;

   DRETURN(ret);
}
