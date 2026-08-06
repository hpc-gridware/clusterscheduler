#pragma once
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

/** @file
 * @brief Memory and swap usage of the local host
 */

#if defined(SOLARIS) || defined(LINUX) || defined(DARWIN) || defined(FREEBSD) || defined(NETBSD)

#define SGE_LOADMEM

/**
 * @brief The host's memory and swap usage, in megabytes
 *
 * Filled in by #sge_loadmem.
 *
 * @see #sge_loadmem
 */
typedef struct {
   double mem_total;   ///< total physical memory, in MB
   double mem_free;    ///< free physical memory, in MB, counting buffers and page cache as free
   double swap_total;  ///< total swap space, in MB
   double swap_free;   ///< free swap space, in MB
} sge_mem_info_t;

int sge_loadmem(sge_mem_info_t *mem_info);

#endif
