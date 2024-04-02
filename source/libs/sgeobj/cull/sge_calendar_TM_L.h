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

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_ULONG(TM_mday) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(TM_mon) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(TM_year) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(TM_sec) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(TM_min) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(TM_hour) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(TM_wday) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(TM_yday) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(TM_isdst) - @todo add summary
*    @todo add description
*
*/

enum {
   TM_mday = TM_LOWERBOUND,
   TM_mon,
   TM_year,
   TM_sec,
   TM_min,
   TM_hour,
   TM_wday,
   TM_yday,
   TM_isdst
};

LISTDEF(TM_Type)
   SGE_ULONG(TM_mday, CULL_DEFAULT)
   SGE_ULONG(TM_mon, CULL_DEFAULT)
   SGE_ULONG(TM_year, CULL_DEFAULT)
   SGE_ULONG(TM_sec, CULL_DEFAULT)
   SGE_ULONG(TM_min, CULL_DEFAULT)
   SGE_ULONG(TM_hour, CULL_DEFAULT)
   SGE_ULONG(TM_wday, CULL_DEFAULT)
   SGE_ULONG(TM_yday, CULL_DEFAULT)
   SGE_ULONG(TM_isdst, CULL_DEFAULT)
LISTEND

NAMEDEF(TMN)
   NAME("TM_mday")
   NAME("TM_mon")
   NAME("TM_year")
   NAME("TM_sec")
   NAME("TM_min")
   NAME("TM_hour")
   NAME("TM_wday")
   NAME("TM_yday")
   NAME("TM_isdst")
NAMEEND

#define TM_SIZE sizeof(TMN)/sizeof(char *)


