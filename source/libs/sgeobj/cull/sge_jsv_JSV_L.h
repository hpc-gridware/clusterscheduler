#ifndef SGE_JSV_L_H
#define SGE_JSV_L_H
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

#ifdef __cplusplus
extern "C" {
#endif

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_STRING(JSV_name) - @todo add summary
*    @todo add description
*
*    SGE_STRING(JSV_context) - @todo add summary
*    @todo add description
*
*    SGE_STRING(JSV_url) - @todo add summary
*    @todo add description
*
*    SGE_STRING(JSV_type) - @todo add summary
*    @todo add description
*
*    SGE_STRING(JSV_user) - @todo add summary
*    @todo add description
*
*    SGE_STRING(JSV_command) - @todo add summary
*    @todo add description
*
*    SGE_STRING(JSV_pid) - @todo add summary
*    @todo add description
*
*    SGE_REF(JSV_in) - @todo add summary
*    @todo add description
*
*    SGE_REF(JSV_out) - @todo add summary
*    @todo add description
*
*    SGE_REF(JSV_err) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(JSV_has_to_restart) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JSV_last_mod) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(JSV_send_env) - @todo add summary
*    @todo add description
*
*    SGE_REF(JSV_old_job) - @todo add summary
*    @todo add description
*
*    SGE_REF(JSV_new_job) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(JSV_restart) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(JSV_accept) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(JSV_done) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(JSV_soft_shutdown) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(JSV_test) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JSV_test_pos) - @todo add summary
*    @todo add description
*
*    SGE_STRING(JSV_result) - @todo add summary
*    @todo add description
*
*/

enum {
   JSV_name = JSV_LOWERBOUND,
   JSV_context,
   JSV_url,
   JSV_type,
   JSV_user,
   JSV_command,
   JSV_pid,
   JSV_in,
   JSV_out,
   JSV_err,
   JSV_has_to_restart,
   JSV_last_mod,
   JSV_send_env,
   JSV_old_job,
   JSV_new_job,
   JSV_restart,
   JSV_accept,
   JSV_done,
   JSV_soft_shutdown,
   JSV_test,
   JSV_test_pos,
   JSV_result
};

LISTDEF(JSV_Type)
   SGE_STRING(JSV_name, CULL_DEFAULT)
   SGE_STRING(JSV_context, CULL_DEFAULT)
   SGE_STRING(JSV_url, CULL_DEFAULT)
   SGE_STRING(JSV_type, CULL_DEFAULT)
   SGE_STRING(JSV_user, CULL_DEFAULT)
   SGE_STRING(JSV_command, CULL_DEFAULT)
   SGE_STRING(JSV_pid, CULL_DEFAULT)
   SGE_REF(JSV_in, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(JSV_out, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(JSV_err, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_BOOL(JSV_has_to_restart, CULL_DEFAULT)
   SGE_ULONG(JSV_last_mod, CULL_DEFAULT)
   SGE_BOOL(JSV_send_env, CULL_DEFAULT)
   SGE_REF(JSV_old_job, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(JSV_new_job, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_BOOL(JSV_restart, CULL_DEFAULT)
   SGE_BOOL(JSV_accept, CULL_DEFAULT)
   SGE_BOOL(JSV_done, CULL_DEFAULT)
   SGE_BOOL(JSV_soft_shutdown, CULL_DEFAULT)
   SGE_BOOL(JSV_test, CULL_DEFAULT)
   SGE_ULONG(JSV_test_pos, CULL_DEFAULT)
   SGE_STRING(JSV_result, CULL_DEFAULT)
LISTEND

NAMEDEF(JSVN)
   NAME("JSV_name")
   NAME("JSV_context")
   NAME("JSV_url")
   NAME("JSV_type")
   NAME("JSV_user")
   NAME("JSV_command")
   NAME("JSV_pid")
   NAME("JSV_in")
   NAME("JSV_out")
   NAME("JSV_err")
   NAME("JSV_has_to_restart")
   NAME("JSV_last_mod")
   NAME("JSV_send_env")
   NAME("JSV_old_job")
   NAME("JSV_new_job")
   NAME("JSV_restart")
   NAME("JSV_accept")
   NAME("JSV_done")
   NAME("JSV_soft_shutdown")
   NAME("JSV_test")
   NAME("JSV_test_pos")
   NAME("JSV_result")
NAMEEND

#define JSV_SIZE sizeof(JSVN)/sizeof(char *)

#ifdef __cplusplus
}
#endif

#endif
