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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Commandline Argument
*
* Used in clients for parsing the commandline.
* One commandline argument (including data) is one SPA_Type object.
*
*    SGE_ULONG(SPA_number) - Option Number
*    Defines which option was parsed.
*    The option numbers are defined as enum in source/common/sge_options.h, e.g.
*    the -a option has option number a_OPT.
*    Every option has a unique option number.
*
*    SGE_ULONG(SPA_argtype) - Argument Type
*    If an option has additional arguments the type of the argument,
*    as defined in source/libs/cull/cull_list.h, enum _enum_lMultiType, e.g.
*    lStringT for the -N option (defining a job name as string).
*
*    SGE_STRING(SPA_switch_val) - Switch
*    The option as string, e.g. -N.
*
*    SGE_STRING(SPA_switch_arg) - Switch Argument
*    Optional the argument to the option, e.g. for the -N switch the job name.
*
*    SGE_ULONG(SPA_occurrence) - Occurence
*    @todo Seems to define if an option has arguments or not.
*    Possible values are defined in source/common/parse_qsub.h (@todo it is *not* qsub specific, move it):
*    - BIT_SPA_OCC_NOARG
*    - BIT_SPA_OCC_ARG
*
*    SGE_FLOAT(SPA_argval_lFloatT) - Parsed Fload Argument
*    If the option argument is a float then this value is the commandline argument (switch_arg) parsed to float.
*
*    SGE_DOUBLE(SPA_argval_lDoubleT) - Parsed Double Argument
*    If the option argument is a double then this value is the commandline argument (switch_arg) parsed to double.
*
*    SGE_ULONG(SPA_argval_lUlongT) - Parsed Ulong Argument
*    If the option argument is an ulong then this value is the commandline argument (switch_arg) parsed to ulong.
*
*    SGE_LONG(SPA_argval_lLongT) - Parsed Long Argument
*    If the option argument is a long then this value is the commandline argument (switch_arg) parsed to long.
*
*    SGE_CHAR(SPA_argval_lCharT) - Parsed Char Argument
*    If the option argument is a single char then this value is the commandline argument (first character of switch_arg).
*
*    SGE_INT(SPA_argval_lIntT) - Parsed Int Argument
*    If the option argument is an integer then this value is the commandline argument (switch_arg) parsed to integer.
*
*    SGE_STRING(SPA_argval_lStringT) - String Argument
*    The commandline argument (same value as the switch_arg attribute)
*
*    SGE_LIST(SPA_argval_lListT) - Parsed List Argument
*    If the option argument defines a list of items then this attribute contains the parsed list.
*
*/

enum {
   SPA_number = SPA_LOWERBOUND,
   SPA_argtype,
   SPA_switch_val,
   SPA_switch_arg,
   SPA_occurrence,
   SPA_argval_lFloatT,
   SPA_argval_lDoubleT,
   SPA_argval_lUlongT,
   SPA_argval_lLongT,
   SPA_argval_lCharT,
   SPA_argval_lIntT,
   SPA_argval_lStringT,
   SPA_argval_lListT
};

LISTDEF(SPA_Type)
   SGE_ULONG(SPA_number, CULL_DEFAULT)
   SGE_ULONG(SPA_argtype, CULL_DEFAULT)
   SGE_STRING(SPA_switch_val, CULL_HASH)
   SGE_STRING(SPA_switch_arg, CULL_DEFAULT)
   SGE_ULONG(SPA_occurrence, CULL_DEFAULT)
   SGE_FLOAT(SPA_argval_lFloatT, CULL_DEFAULT)
   SGE_DOUBLE(SPA_argval_lDoubleT, CULL_DEFAULT)
   SGE_ULONG(SPA_argval_lUlongT, CULL_DEFAULT)
   SGE_LONG(SPA_argval_lLongT, CULL_DEFAULT)
   SGE_CHAR(SPA_argval_lCharT, CULL_DEFAULT)
   SGE_INT(SPA_argval_lIntT, CULL_DEFAULT)
   SGE_STRING(SPA_argval_lStringT, CULL_DEFAULT)
   SGE_LIST(SPA_argval_lListT, ST_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(SPAN)
   NAME("SPA_number")
   NAME("SPA_argtype")
   NAME("SPA_switch_val")
   NAME("SPA_switch_arg")
   NAME("SPA_occurrence")
   NAME("SPA_argval_lFloatT")
   NAME("SPA_argval_lDoubleT")
   NAME("SPA_argval_lUlongT")
   NAME("SPA_argval_lLongT")
   NAME("SPA_argval_lCharT")
   NAME("SPA_argval_lIntT")
   NAME("SPA_argval_lStringT")
   NAME("SPA_argval_lListT")
NAMEEND

#define SPA_SIZE sizeof(SPAN)/sizeof(char *)


