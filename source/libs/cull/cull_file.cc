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
#include <cstring>
#include <cerrno>

/* do not compile in monitoring code */
#ifndef NO_SGE_COMPILE_DEBUG
#define NO_SGE_COMPILE_DEBUG
#endif

#include "uti/sge_io.h"
#include "uti/sge_log.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_unistd.h"

#include "cull/cull_list.h"
#include "cull/cull_listP.h"
#include "cull/cull_multitypeP.h"
#include "cull/cull_whereP.h"
#include "cull/cull_pack.h"
#include "cull/cull_file.h"
#include "cull/msg_cull.h"

/****** cull/file/lWriteElemToDisk() ******************************************
*  NAME
*     lWriteElemToDisk() -- Writes a element to file 
*
*  SYNOPSIS
*     int lWriteElemToDisk(const lListElem *ep, const char *prefix, 
*                          const char *name, const char *obj_name) 
*
*  FUNCTION
*     Writes the Element 'ep' to the file named 'prefix'/'name'.
*     Either 'prefix' or 'name can be null. 
*
*  INPUTS
*     const lListElem *ep  - CULL element 
*     const char *prefix   - Path 
*     const char *name     - Filename 
*     const char *obj_name - 
*
*  RESULT
*     int - error state 
*         0 - OK
*         1 - Error
******************************************************************************/
int lWriteElemToDisk(const lListElem *ep, const char *prefix, const char *name,
                     const char *obj_name) {
   stringT filename;
   sge_pack_buffer pb;
   int ret, fd;

   DENTER(TOP_LAYER);

   if (!prefix && !name) {
      ERROR(SFNMAX, MSG_CULL_NOPREFIXANDNOFILENAMEINWRITEELMTODISK);
      DRETURN(1);
   }

   /* init packing buffer */
   ret = init_packbuffer(&pb, 8192, false, false);

   /* pack ListElement */
   if (ret == PACK_SUCCESS) {
      ret = cull_pack_elem(&pb, ep);
   }

   switch (ret) {
      case PACK_SUCCESS:
         break;

      case PACK_ENOMEM:
         ERROR(MSG_CULL_NOTENOUGHMEMORYFORPACKINGXY_SS, obj_name, name ? name : "null");
         clear_packbuffer(&pb);
         DRETURN(1);

      case PACK_FORMAT:
         ERROR(MSG_CULL_FORMATERRORWHILEPACKINGXY_SS, obj_name, name ? name : "null");
         clear_packbuffer(&pb);
         DRETURN(1);

      default:
         ERROR(MSG_CULL_UNEXPECTEDERRORWHILEPACKINGXY_SS, obj_name, name ? name : "null");
         clear_packbuffer(&pb);
         DRETURN(1);
   }

   /* create full file name */
   if (prefix && name) {
      snprintf(filename, sizeof(filename), "%s/%s", prefix, name);
   } else if (prefix) {
      snprintf(filename, sizeof(filename), "%s", prefix);
   } else {
      snprintf(filename, sizeof(filename), "%s", name);
   }

   PROF_START_MEASUREMENT(SGE_PROF_SPOOLINGIO);

   /* open file */
   if ((fd = SGE_OPEN3(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0) {
      CRITICAL(MSG_CULL_CANTOPENXFORWRITINGOFYZ_SSS, filename, obj_name, strerror(errno));
      clear_packbuffer(&pb);
      PROF_STOP_MEASUREMENT(SGE_PROF_SPOOLINGIO);
      DRETURN(1);
   }

   /* write packing buffer */
   if (sge_writenbytes(fd, pb.head_ptr, pb_used(&pb)) != pb_used(&pb)) {
      CRITICAL(MSG_CULL_CANTWRITEXTOFILEY_SS, obj_name, filename);
      clear_packbuffer(&pb);
      close(fd);
      PROF_STOP_MEASUREMENT(SGE_PROF_SPOOLINGIO);
      DRETURN(1);
   }

   /* close file and exit */
   close(fd);
   PROF_STOP_MEASUREMENT(SGE_PROF_SPOOLINGIO);
   clear_packbuffer(&pb);

   DRETURN(0);
}

/****** cull/file/lReadElemFromDisk() ****************************************
*  NAME
*     lReadElemFromDisk() -- Reads a cull element from file 
*
*  SYNOPSIS
*     lListElem* lReadElemFromDisk(const char *prefix, 
*                                  const char *name, 
*                                  const lDescr *type, 
*                                  const char *obj_name) 
*
*  FUNCTION
*     Reads a lListElem of the specified 'type' from the file
*     'prefix'/'name'. Either 'prefix' or 'name' can be null.
*     Returns a pointer to the read element or nullptr in case
*     of an error 
*
*  INPUTS
*     const char *prefix   - Path 
*     const char *name     - Filename 
*     const lDescr *type   - Type 
*     const char *obj_name - 
*
*  RESULT
*     lListElem* - Read CULL element
*******************************************************************************/
lListElem *lReadElemFromDisk(const char *prefix, const char *name,
                             const lDescr *type, const char *obj_name) {
   stringT filename;
   sge_pack_buffer pb;
   SGE_STRUCT_STAT statbuf;
   lListElem *ep;
   int ret, fd;
   char *buf;
   size_t size;

   DENTER(TOP_LAYER);

   if (!prefix && !name) {
      ERROR(SFNMAX, MSG_CULL_NOPREFIXANDNOFILENAMEINREADELEMFROMDISK);
      DRETURN(nullptr);
   }

   /* create full file name */
   if (prefix && name)
      snprintf(filename, sizeof(filename), "%s/%s", prefix, name);
   else if (prefix)
      snprintf(filename, sizeof(filename), "%s", prefix);
   else
      snprintf(filename, sizeof(filename), "%s", name);

   /* get file size */
   if (SGE_STAT(filename, &statbuf) == -1) {
      CRITICAL(MSG_CULL_CANTGETFILESTATFORXFILEY_SS, obj_name, filename);
      DRETURN(nullptr);
   }

   if (!statbuf.st_size) {
      CRITICAL(MSG_CULL_XFILEYHASZEROSIYE_SS, obj_name, filename);
      DRETURN(nullptr);
   }

   /* init packing buffer */
   size = statbuf.st_size;
   if (((SGE_OFF_T) size != statbuf.st_size)
       || !(buf = sge_malloc(statbuf.st_size))) {
      CRITICAL(SFNMAX, MSG_CULL_LEMALLOC);
      clear_packbuffer(&pb);
      DRETURN(nullptr);
   }

   /* open file */
   if ((fd = SGE_OPEN2(filename, O_RDONLY)) < 0) {
      CRITICAL(MSG_CULL_CANTREADXFROMFILEY_SS, obj_name, filename);
      clear_packbuffer(&pb);    /* this one frees buf */
      DRETURN(nullptr);
   }

   /* read packing buffer */
   if (sge_readnbytes(fd, buf, statbuf.st_size) != statbuf.st_size) {
      CRITICAL(MSG_CULL_ERRORREADINGXINFILEY_SS, obj_name, filename);
      close(fd);
      DRETURN(nullptr);
   }

   if ((ret = init_packbuffer_from_buffer(&pb, buf, statbuf.st_size, false)) != PACK_SUCCESS) {
      ERROR(MSG_CULL_ERRORININITPACKBUFFER_S, cull_pack_strerror(ret));
   }
   ret = cull_unpack_elem(&pb, &ep, type);
   close(fd);
   clear_packbuffer(&pb);     /* this one frees buf */

   switch (ret) {
      case PACK_SUCCESS:
         break;

      case PACK_ENOMEM:
         ERROR(MSG_CULL_NOTENOUGHMEMORYFORUNPACKINGXY_SS, obj_name, filename);
         DRETURN(nullptr);

      case PACK_FORMAT:
         ERROR(MSG_CULL_FORMATERRORWHILEUNPACKINGXY_SS, obj_name, filename);
         DRETURN(nullptr);

      case PACK_BADARG:
         ERROR(MSG_CULL_BADARGUMENTWHILEUNPACKINGXY_SS, obj_name, filename);
         DRETURN(nullptr);

      default:
         ERROR(MSG_CULL_UNEXPECTEDERRORWHILEUNPACKINGXY_SS, obj_name, filename);
         DRETURN(nullptr);
   }

   DRETURN(ep);
}
