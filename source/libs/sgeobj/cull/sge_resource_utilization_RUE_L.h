#ifndef SGE_RUE_L_H
#define SGE_RUE_L_H
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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_STRING(RUE_name) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(RUE_utilized_now) - @todo add summary
*    @todo add description
*
*    SGE_LIST(RUE_utilized) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(RUE_utilized_now_nonexclusive) - @todo add summary
*    @todo add description
*
*    SGE_LIST(RUE_utilized_nonexclusive) - @todo add summary
*    @todo add description
*
*/

enum {
   RUE_name = RUE_LOWERBOUND,
   RUE_utilized_now,
   RUE_utilized,
   RUE_utilized_now_nonexclusive,
   RUE_utilized_nonexclusive
};

LISTDEF(RUE_Type)
   SGE_STRING(RUE_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL | CULL_SUBLIST)
   SGE_DOUBLE(RUE_utilized_now, CULL_DEFAULT)
   SGE_LIST(RUE_utilized, RDE_Type, CULL_DEFAULT)
   SGE_DOUBLE(RUE_utilized_now_nonexclusive, CULL_DEFAULT)
   SGE_LIST(RUE_utilized_nonexclusive, RDE_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(RUEN)
   NAME("RUE_name")
   NAME("RUE_utilized_now")
   NAME("RUE_utilized")
   NAME("RUE_utilized_now_nonexclusive")
   NAME("RUE_utilized_nonexclusive")
NAMEEND

#define RUE_SIZE sizeof(RUEN)/sizeof(char *)


#endif
