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

#include <cstdio>
#include <syslog.h>

#include "basis_types.h"
#include "msg_utilib.h"

#include "uti/sge_bootstrap.h"
#include "uti/sge_component.h"

#define LOG_PROF       0      /* no action, but it has to be printed allways */

void log_state_set_log_level(u_long32);

void log_state_set_log_file(char *file);

void log_state_set_log_verbose(int i);

void log_state_set_log_gui(int i);

void log_state_set_log_as_admin_user(int i);

u_long32 log_state_get_log_level();

const char *log_state_get_log_file();

int log_state_get_log_verbose();

int log_state_get_log_gui();

void
sge_log(u_long32 log_level, const char *msg, const char *file, int line);

/* extern stringTlong SGE_EVENT; */
#define SGE_EVENT component_get_log_buffer()

#if defined(__INSURE__)
#   define PROFILING(x)     (sprintf x,sge_log(LOG_PROF,   SGE_EVENT,__FILE__,__LINE__)) ? 1 : 0
#   define CRITICAL(x) (sprintf x,sge_log(LOG_CRIT,   SGE_EVENT,__FILE__,__LINE__)) ? 1 : 0
#   define ERROR(x)    (sprintf x,sge_log(LOG_ERR,    SGE_EVENT,__FILE__,__LINE__)) ? 1 : 0
#   define WARNING(x)  (sprintf x,sge_log(LOG_WARNING,SGE_EVENT,__FILE__,__LINE__)) ? 1 : 0
#   define NOTICE(x)   (sprintf x,sge_log(LOG_NOTICE, SGE_EVENT,__FILE__,__LINE__)) ? 1 : 0
#   define INFO(x)     (sprintf x,sge_log(LOG_INFO,   SGE_EVENT,__FILE__,__LINE__)) ? 1 : 0
#   define DEBUG(x)    (sprintf x,sge_log(LOG_DEBUG,  SGE_EVENT,__FILE__,__LINE__)) ? 1 : 0
#else

/****** uti/log/PROFILING() ****************************************************
*  NAME
*     PROFILING() -- Log a profiling message 
*
*  SYNOPSIS
*     #define PROFILING(params)
*     void PROFILING(char *buffer, const char* formatstring, ...) 
*
*  FUNCTION
*     Log a profiling message 
*
*  INPUTS
*     buffer       - e.g SGE_EVENT
*     formatstring - printf formatstring
*     ...
******************************************************************************/
#ifdef __SGE_COMPILE_WITH_GETTEXT__
#   define PROFILING(x) (sge_set_message_id_output(1), \
                        sprintf x, \
                        sge_set_message_id_output(0), \
                        sge_log(LOG_PROF, SGE_EVENT,__FILE__,__LINE__))
#else
#   define PROFILING(x) (sprintf x, sge_log(LOG_PROF, SGE_EVENT,__FILE__,__LINE__))
#endif

/****** uti/log/CRITICAL() ****************************************************
*  NAME
*     CRITICAL() -- Log a critical message 
*
*  SYNOPSIS
*     #define CRITICAL(params)
*     void CRITICAL(char *buffer, const char* formatstring, ...) 
*
*  FUNCTION
*     Log a critical message 
*
*  INPUTS
*     buffer       - e.g SGE_EVENT
*     formatstring - printf formatstring
*     ...
******************************************************************************/
#ifdef __SGE_COMPILE_WITH_GETTEXT__
#   define CRITICAL(x) (sge_set_message_id_output(1), sprintf x, sge_set_message_id_output(0), \
                        sge_log(LOG_CRIT, SGE_EVENT,__FILE__,__LINE__))
#else
#   define CRITICAL(x) (sprintf x, sge_log(LOG_CRIT, SGE_EVENT,__FILE__,__LINE__))
#endif

/****** uti/log/ERROR() *******************************************************
*  NAME
*     ERROR() -- Log an error message 
*
*  SYNOPSIS
*     #define ERROR(params)
*     void ERROR(char *buffer, const char* formatstring, ...) 
*
*  FUNCTION
*     Log a error message 
*
*  INPUTS
*     buffer       - e.g SGE_EVENT
*     formatstring - printf formatstring
*     ...
******************************************************************************/
#ifdef __SGE_COMPILE_WITH_GETTEXT__
#   define ERROR(x) ( sge_set_message_id_output(1),                          \
                        sprintf x,                                             \
                        sge_set_message_id_output(0),                          \
                        sge_log(LOG_ERR,SGE_EVENT,__FILE__,__LINE__))
#else
#   define ERROR(x) (sprintf x, sge_log(LOG_ERR,SGE_EVENT,__FILE__,__LINE__))
#endif

/****** uti/log/WARNING() ******************************************************
*  NAME
*     WARNING() -- Log an warning message
*
*  SYNOPSIS
*     #define WARNING(params)
*     void WARNING(char *buffer, const char* formatstring, ...)
*
*  FUNCTION
*     Log a warning message
*
*  INPUTS
*     buffer       - e.g SGE_EVENT
*     formatstring - printf formatstring
*     ...
******************************************************************************/
#ifdef __SGE_COMPILE_WITH_GETTEXT__
#   define WARNING(x) ( sge_set_message_id_output(1), \
                        sprintf x,       \
                        sge_set_message_id_output(0), \
                        sge_log(LOG_WARNING,SGE_EVENT,__FILE__,__LINE__))
#else
#   define WARNING(x) (sprintf x, sge_log(LOG_WARNING,SGE_EVENT,__FILE__,__LINE__))
#endif

/****** uti/log/NOTICE() ******************************************************
*  NAME
*     NOTICE() -- Log a notice message
*
*  SYNOPSIS
*     #define NOTICE(params)
*     void NOTICE(char *buffer, const char* formatstring, ...)
*
*  FUNCTION
*     Log a notice message
*
*  INPUTS
*     buffer       - e.g SGE_EVENT
*     formatstring - printf formatstring
*     ...
******************************************************************************/
#   define NOTICE(x) (sprintf x, sge_log(LOG_NOTICE, SGE_EVENT,__FILE__,__func__,__LINE__))

/****** uti/log/INFO() ********************************************************
*  NAME
*     INFO() -- Log an info message
*
*  SYNOPSIS
*     #define INFO(params)
*     void INFO(char *buffer, const char* formatstring, ...)
*
*  FUNCTION
*     Log an info message
*
*  INPUTS
*     buffer       - e.g SGE_EVENT
*     formatstring - printf formatstring
*     ...
******************************************************************************/
#   define INFO(x) (sprintf x, sge_log(LOG_INFO, SGE_EVENT,__FILE__,__LINE__))

/****** uti/log/DEBUG() ******************************************************
*  NAME
*     DEBUG() -- Log a debug message
*
*  SYNOPSIS
*     #define DEBUG(params)
*     void DEBUG(char *buffer, const char* formatstring, ...)
*
*  FUNCTION
*     Log a debug message
*
*  INPUTS
*     buffer       - e.g SGE_EVENT
*     formatstring - printf formatstring
*     ...
******************************************************************************/
#ifdef __SGE_COMPILE_WITH_GETTEXT__
#   define DEBUG(x)  ((LOG_DEBUG <= MAX(log_state_get_log_level(), LOG_WARNING)) ? \
                      (sge_set_message_id_output(1), sprintf x, sge_set_message_id_output(0), \
                      sge_log(LOG_DEBUG, SGE_EVENT,__FILE__,__LINE__), 0): 0)
#else
#   define DEBUG(x) (sprintf x, sge_log(LOG_DEBUG,  SGE_EVENT,__FILE__,__LINE__))
#endif
#endif

/****** uti/log/SGE_ASSERT() **************************************************
*  NAME
*     SGE_ASSERT() -- Log a message and exit if assertion is false 
*
*  SYNOPSIS
*     #define SGE_ASSERT(expression)
*     void SGE_ASSERT(int expression) 
*
*  FUNCTION
*     Log a critical message and exit if assertion is false.
*
*  INPUTS
*     expression   - a logical expression 
******************************************************************************/
#  define SGE_ASSERT(x) \
   if (!(x)) { \
      sge_log(LOG_CRIT, MSG_UNREC_ERROR,__FILE__,__LINE__); \
      abort(); \
   } \
   void()
