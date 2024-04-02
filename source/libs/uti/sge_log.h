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

#include <cstdio>
#include <syslog.h>

#include "basis_types.h"
#include "msg_utilib.h"

#include "uti/sge_bootstrap.h"
#include "uti/sge_component.h"

#define LOG_PROF       0      /* no action, but it has to be printed allways */

void log_state_set_log_level(u_long32);

void log_state_set_log_file(const char *file);

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
#define SGE_EVENT_SIZE component_get_log_buffer_size()

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
#   define PROFILING(...) { \
   char *log_buffer = component_get_log_buffer(); \
   size_t log_buffer_size = component_get_log_buffer_size(); \
   sge_set_message_id_output(1); \
   snprintf(log_buffer, log_buffer_size, __VA_ARGS__); \
   sge_set_message_id_output(0); \
   sge_log(LOG_PROF, SGE_EVENT, __FILE__, __LINE__); \
} void()
#else
#   define PROFILING(...) { \
   char *log_buffer = component_get_log_buffer(); \
   size_t log_buffer_size = component_get_log_buffer_size(); \
   snprintf(log_buffer, log_buffer_size, __VA_ARGS__);      \
   sge_log(LOG_PROF, log_buffer, __FILE__, __LINE__); \
} void()
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
#   define CRITICAL(...) { \
   char *log_buffer = component_get_log_buffer(); \
   size_t log_buffer_size = component_get_log_buffer_size(); \
   sge_set_message_id_output(1); \
   snprintf(log_buffer, log_buffer_size, __VA_ARGS__); \
   sge_set_message_id_output(0); \
   sge_log(LOG_CRIT, SGE_EVENT, __FILE__, __LINE__); \
} void()
#else
#   define CRITICAL(...) { \
   char *log_buffer = component_get_log_buffer(); \
   size_t log_buffer_size = component_get_log_buffer_size(); \
   snprintf(log_buffer, log_buffer_size, __VA_ARGS__);      \
   sge_log(LOG_CRIT, log_buffer, __FILE__, __LINE__); \
} void()
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
#   define ERROR(...) { \
   char *log_buffer = component_get_log_buffer(); \
   size_t log_buffer_size = component_get_log_buffer_size(); \
   sge_set_message_id_output(1); \
   snprintf(log_buffer, log_buffer_size, __VA_ARGS__); \
   sge_set_message_id_output(0); \
   sge_log(LOG_ERR, SGE_EVENT, __FILE__, __LINE__);          \
} void()
#else
#   define ERROR(...) { \
   char *log_buffer = component_get_log_buffer(); \
   size_t log_buffer_size = component_get_log_buffer_size(); \
   snprintf(log_buffer, log_buffer_size, __VA_ARGS__);      \
   sge_log(LOG_ERR, log_buffer, __FILE__, __LINE__); \
} void()
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
#   define WARNING(...) { \
   char *log_buffer = component_get_log_buffer(); \
   size_t log_buffer_size = component_get_log_buffer_size(); \
   sge_set_message_id_output(1); \
   snprintf(log_buffer, log_buffer_size, __VA_ARGS__); \
   sge_set_message_id_output(0); \
   sge_log(LOG_WARNING, SGE_EVENT, __FILE__, __LINE__);     \
} void()
#else
#   define WARNING(...) { \
   char *log_buffer = component_get_log_buffer(); \
   size_t log_buffer_size = component_get_log_buffer_size(); \
   snprintf(log_buffer, log_buffer_size, __VA_ARGS__);      \
   sge_log(LOG_WARNING, log_buffer, __FILE__, __LINE__); \
} void()
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
#   define NOTICE(...) { \
   char *log_buffer = component_get_log_buffer(); \
   size_t log_buffer_size = component_get_log_buffer_size(); \
   snprintf(log_buffer, log_buffer_size, __VA_ARGS__);      \
   sge_log(LOG_NOTICE, log_buffer, __FILE__, __LINE__); \
} void()

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
/*
 * I18N/L10N was missing for INFO messages.
 * But enabling it leads to all clients printing "ok" as last output line
 * coming from MSG_GDI_OKNL being generated as ANSWER_QUALITY_END in sge_c_gdi()
 * might possibly need changes in some answer_list logging function?
#ifdef __SGE_COMPILE_WITH_GETTEXT__
#   define INFO(...) { \
   if (LOG_INFO <= MAX(log_state_get_log_level(), LOG_WARNING)) { \
      char *log_buffer = component_get_log_buffer(); \
      size_t log_buffer_size = component_get_log_buffer_size(); \
      sge_set_message_id_output(1); \
      snprintf(log_buffer, log_buffer_size, __VA_ARGS__); \
      sge_set_message_id_output(0); \
      sge_log(LOG_INFO, SGE_EVENT, __FILE__, __LINE__);     \
   } \
} void()
#else
*/
#   define INFO(...) { \
   char *log_buffer = component_get_log_buffer(); \
   size_t log_buffer_size = component_get_log_buffer_size(); \
   snprintf(log_buffer, log_buffer_size, __VA_ARGS__);      \
   sge_log(LOG_INFO, log_buffer, __FILE__, __LINE__); \
} void()
/* #endif */

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
#   define DEBUG(...) { \
   if (LOG_DEBUG <= MAX(log_state_get_log_level(), LOG_WARNING)) { \
      char *log_buffer = component_get_log_buffer(); \
      size_t log_buffer_size = component_get_log_buffer_size(); \
      sge_set_message_id_output(1); \
      snprintf(log_buffer, log_buffer_size, __VA_ARGS__); \
      sge_set_message_id_output(0); \
      sge_log(LOG_DEBUG, SGE_EVENT,__FILE__,__LINE__); \
   } \
} void()
#else
#   define DEBUG(...) { \
   char *log_buffer = component_get_log_buffer(); \
   size_t log_buffer_size = component_get_log_buffer_size(); \
   snprintf(log_buffer, log_buffer_size, __VA_ARGS__);      \
   sge_log(LOG_DEBUG, log_buffer, __FILE__, __LINE__); \
} void()
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
