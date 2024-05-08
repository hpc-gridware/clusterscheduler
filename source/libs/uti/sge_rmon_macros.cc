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

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <sstream>
#include <thread>
#include <unistd.h>

#include "uti/msg_utilib.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_rmon_monitoring_level.h"

#define DEBUG RMON_LOCAL

enum {
    RMON_NONE = 0,       /* monitoring off */
    RMON_LOCAL = 1,      /* monitoring on */
    RMON_BUF_SIZE = 5120 /* size of buffer used for monitoring messages */
};

monitoring_level RMON_DEBUG_ON = {{0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L}};
monitoring_level RMON_DEBUG_ON_STORAGE = {{0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L}};

static const char *empty = "    ";

static u_long mtype = RMON_NONE;
static FILE *rmon_fp;

static void mwrite(char *message, const char *thread_name, int thread_id);

static int set_debug_level_from_env();

static int set_debug_target_from_env();

static void rmon_mprintf_va(const char *fmt, va_list args);

static pthread_key_t rmon_helper_key;
static pthread_once_t rmon_helper_key_once = PTHREAD_ONCE_INIT;

static void rmon_helper_key_init();

static void rmon_helper_key_destroy(void *ctx);

rmon_helper_t *rmon_get_helper() {
    pthread_once(&rmon_helper_key_once, rmon_helper_key_init);
    auto *helper = (rmon_helper_t *) pthread_getspecific(rmon_helper_key);
    if (helper == nullptr) {
        helper = (rmon_helper_t *) sge_malloc(sizeof(rmon_helper_t));

        memset(helper, 0, sizeof(rmon_helper_t));
        pthread_setspecific(rmon_helper_key, helper);
    }
    return helper;
}

static void rmon_helper_key_init() {
    pthread_key_create(&rmon_helper_key, rmon_helper_key_destroy);
}

static void rmon_helper_key_destroy(void *ctx) {
    free(ctx);
}

/****** rmon/Introduction ******************************************************
*  NAME
*     RMON - Cluster Scheduler Monitoring Interface
*
*  FUNCTION
*     The RMON library is a set of functions, which do allow monitoring of 
*     of application execution. The functions provided, however, should not
*     be used directly. Rather the RMON functions are utilized by a set of
*     monitoring macros, like 'DENTER' or 'DRETURN'/'DRETURN_VOID'.
*
*     If monitoring is active, the RMON functions do get called very frequently.
*     Hence, the overhead caused by monitoring needs to be minimal. For this
*     reason, access to external global and static global variables is NOT
*     synchronized through a mutex! Not using a lock of type 'pthread_mutex_t'
*     also means that the RMON functions are async-signal safe.
* 
*     To use RMON library in a multi threaded environment, some restrictions
*     must be followed strictly! It is of utmost importance, that the function
*     'rmon_mopen()' is ONLY invoked from exactly one thread. The thread which
*     is calling 'rmon_mopen()' must be the ONLY thread at this point in time.
*     'DENTER_MAIN' is the only macro from which 'rmon_mopen()' is called. The
*     macro 'DENTER_MAIN' is used at the beginning of a main function. At this
*     point in time, the so called main-thread is the only thread.
*
*     It is safe to call the remaining RMON functions, like 'rmon_menter()' or
*     'rmon_mexit()', from within multiple threads. 'rmon_mopen()' is the only
*     RMON function which does change the critical global variables ('mtype',
*     'rmon_fp' and 'RMON_DEBUG_ON'). 'rmon_menter()' and 'rmon_mexit()' are used by
*     the macro 'DENTER' and ,'DRETURN'/'DRETURN_VOID' respectively.
*     
*******************************************************************************/

/****** rmon_macros/rmon_condition() *******************************************
*  NAME
*     rmon_condition() -- Check monitoring condition. 
*
*  SYNOPSIS
*     int rmon_condition(int layer, int rmon_class)
*
*  FUNCTION
*     Check whether monitoring should be enabled for the given combination of
*     'layer' and 'rmon_class'.
*
*  INPUTS
*     int layer - monitor layer 
*     int rmon_class - monitor class
*
*  RESULT
*     1 - do monitor
*     0 - do not monitor
*
*  NOTES
*     MT-NOTE: 'rmon_condition()' is MT safe with exceptions. See introduction!
*
*******************************************************************************/
int rmon_condition(int layer, int rmon_class) {
    int ret_val;
#define MLGETL(s, i) ((s)->ml[i]) /* for the sake of speed */
    ret_val = ((mtype != RMON_NONE) && (rmon_class & MLGETL(&RMON_DEBUG_ON, layer))) ? 1 : 0;
    return ret_val;
#undef MLGETL
} /* rmon_condition() */

/****** rmon_macros/rmon_is_enabled() ******************************************
*  NAME
*     rmon_is_enabled() -- Check if monitoring is enabled. 
*
*  SYNOPSIS
*     int rmon_is_enabled(void) 
*
*  FUNCTION
*     Check if monitoring is enabled. Note that even if monitoring is enabled
*     no actual monitoring output may be generated. Generation of monitoring
*     output is controlled by 'rmon_condition()'.   
*
*  INPUTS
*     void - none 
*
*  RESULT
*     1 - monitoring enabled 
*     0 - monitoring disabled
*
*  NOTES
*     MT-NOTE: 'rmon_is_enabled()' is MT safe with exceptions. See introduction! 
*
*******************************************************************************/
int rmon_is_enabled() {
    return ((mtype == RMON_LOCAL) ? 1 : 0);
}

void rmon_disable() {
   mtype = RMON_NONE;
}

void rmon_enable() {
   mtype = RMON_LOCAL;
}


/****** rmon_macros/rmon_mopen() ***********************************************
*  NAME
*     rmon_mopen() -- Open, i.e. initialize monitoring. 
*
*  SYNOPSIS
*     void rmon_mopen(int *argc, char *argv[], char *programname) 
*
*  FUNCTION
*     Initialize monitoring. Clear all monitoring levels. Set monitoring levels
*     according to 'SGE_DEBUG_LEVEL' environment variable. Set monitoring
*     target (i.e. output stream) according to 'SGE_DEBUG_TARGET' environment
*     variable. Enable monitoring.  
*
*     NOTE: Even though 'argc' and 'argv' are not used, they do make sure that
*     'rmon_mopen()' is only used within a main function to a certain degree.
*
*  INPUTS
*     int *argc         - not used 
*     char *argv[]      - not used 
*     char *programname - not used 
*
*  RESULT
*     void - none
*
*  NOTES
*     MT-NOTE: 'rmon_mopen()' is NOT MT safe. See introduction! 
*
*******************************************************************************/
void rmon_mopen() {
    rmon_mlclr(&RMON_DEBUG_ON);
    rmon_fp = stderr;

    (void) set_debug_level_from_env();
    int ret = set_debug_target_from_env();

    if (ret != 0) {
        exit(-1);
    }
    mtype = RMON_LOCAL;
}

/****** rmon_macros/rmon_menter() **********************************************
*  NAME
*     rmon_menter() -- Monitor function entry 
*
*  SYNOPSIS
*     void rmon_menter(const char *func) 
*
*  FUNCTION
*     Monitor function entry. Generate function entry message. 
*
*  INPUTS
*     const char *func - function name 
*
*  RESULT
*     void - none 
*
*  NOTES
*     MT-NOTE: 'rmon_menter()' is MT safe with exceptions. See introduction! 
*
*******************************************************************************/

void rmon_menter(const char *func, const char *thread_name, int thread_id) {
    char msgbuf[RMON_BUF_SIZE];
    snprintf(msgbuf, sizeof(msgbuf), "--> %s() {\n", func);
    mwrite(msgbuf, thread_name ? thread_name : "NA", thread_id);
}

/****** rmon_macros/rmon_mexit() ***********************************************
*  NAME
*     rmon_mexit() -- Monitor function exit 
*
*  SYNOPSIS
*     void rmon_mexit(const char *func, const char *file, int line) 
*
*  FUNCTION
*     Monitor function exit. Generate function exit message. 
*
*  INPUTS
*     const char *func - function name 
*     const char *file - source file in which function is defined 
*     int line         - number of invokation source line
*
*  RESULT
*     void - none
*
*  NOTES
*     MT-NOTE: 'rmon_mexit()' is MT safe with exceptions. See introduction! 
*
*******************************************************************************/
void rmon_mexit(const char *func, const char *file, int line, const char *thread_name, int thread_id) {
    char msgbuf[RMON_BUF_SIZE];
    snprintf(msgbuf, sizeof(msgbuf), "<-- %s() %s %d }\n", func, file, line);
    mwrite(msgbuf, thread_name, thread_id);
}

/****** rmon_macros/rmon_mtrace() **********************************************
*  NAME
*     rmon_mtrace() -- Monitor function progress 
*
*  SYNOPSIS
*     void rmon_mtrace(const char *func, const char *file, int line) 
*
*  FUNCTION
*     Monitor function progress. Generate function trace message. 
*
*  INPUTS
*     const char *func - function name 
*     const char *file - source file in which function is defined 
*     int line         - number of invokation source line
*
*  RESULT
*     void - none 
*
*  NOTES
*     MT-NOTE: 'rmon_mtrace()' is MT safe with exceptions. See introduction! 
*
*******************************************************************************/
void rmon_mtrace(const char *func, const char *file, int line, const char *thread_name, int thread_id) {
    char msgbuf[RMON_BUF_SIZE];
    strcpy(msgbuf, empty);
    snprintf(&msgbuf[4], sizeof(msgbuf) - 4, "%s:%s:%d\n", func, file, line);
    mwrite(msgbuf, thread_name, thread_id);
}

/****** rmon_macros/rmon_mprintf() *********************************************
*  NAME
*     rmon_mprintf() -- Print formatted monitoring message. 
*
*  SYNOPSIS
*     void rmon_mprintf(const char *fmt, ...) 
*
*  FUNCTION
*     Print formatted monitoring message. 
*
*  INPUTS
*     const char *fmt - format string 
*     ...             - variable argument list 
*
*  RESULT
*     void - none
*
*  NOTES
*     MT-NOTE: 'rmon_mprintf()' is MT safe with exceptions. See introduction! 
*
*******************************************************************************/
void rmon_mprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    rmon_mprintf_va(fmt, args);
    va_end(args);
}

static void rmon_mprintf_va(const char *fmt, va_list args) {
    char msgbuf[RMON_BUF_SIZE];
    rmon_helper_t *helper = rmon_get_helper();
    strcpy(msgbuf, empty);
    vsnprintf(&msgbuf[4], (RMON_BUF_SIZE) -10, fmt, args);
    if ((helper != nullptr) && (strlen(helper->thread_name) > 0)) {
        mwrite(msgbuf, helper->thread_name, helper->thread_id);
    } else {
        mwrite(msgbuf, nullptr, -1);
    }
}

/****** rmon_macros/mwrite() ***************************************************
*  NAME
*     mwrite() -- Write monitoring message
*
*  SYNOPSIS
*     static void mwrite(char *message) 
*
*  FUNCTION
*     Write monitoring message. The message is written to the output stream
*     associated with 'rmon_fp'. The output stream is flushed immediately. 
*
*     A prefix is added to 'message'. It does consist of a trace sequence number,
*     the PID and the thread ID of the calling thread.
*
*  INPUTS
*     char *message - monitoring message
*
*  RESULT
*     void - none 
*
*  NOTES
*     MT-NOTE: 'mwrite()' is MT safe with exceptions. See introduction!
*     MT-NOTE:
*     MT-NOTE: It is guaranteed that the output of different threads is not
*     MT-NOTE: mingled.
*
*******************************************************************************/
static void mwrite(char *message, const char *thread_name, int thread_id) {
    static u_long traceid = 0;

    flockfile(rmon_fp);
    if (thread_name != nullptr) {
        fprintf(rmon_fp, "%7ld %16.16s %02d ", traceid, thread_name, thread_id);
    } else {
        std::ostringstream oss_thread_id;
        oss_thread_id << std::this_thread::get_id();
        fprintf(rmon_fp, "%7ld %16.16s %02d ", traceid, oss_thread_id.str().c_str(), thread_id);
    }
    fprintf(rmon_fp, "%s", message);
    fflush(rmon_fp);

    traceid++;
    funlockfile(rmon_fp);
}

/****** rmon_macros/set_debug_level_from_env() *********************************
*  NAME
*     set_debug_level_from_env() -- Set debug level from environment variable.
*
*  SYNOPSIS
*     static int set_debug_level_from_env(void) 
*
*  FUNCTION
*     Set debug level. Read environment variable "SGE_DEBUG_LEVEL" and use it
*     to initialize debug levels.
*
*  INPUTS
*     void - none 
*
*  RESULT
*     0 - successful
*     ENOENT - environment variable not set
*     EINVAL - unexpected format
*
*  NOTES
*     MT-NOTE: 'set_debug_level_from_env()' is MT safe with exceptions.
*     MT-NOTE:  See introduction!
*
*******************************************************************************/
static int set_debug_level_from_env() {
    const char *env = getenv("SGE_DEBUG_LEVEL");
    if (env == nullptr) {
        return ENOENT;
    }

    int l[N_LAYER];
    const char *s = strdup(env);
    int i = sscanf(s, "%d%d%d%d%d%d%d%d", l, l + 1, l + 2, l + 3, l + 4, l + 5, l + 6, l + 7);
    if (i != N_LAYER) {
        printf("%s\n", MSG_RMON_ILLEGALDBUGLEVELFORMAT);
        free((char *) s);
        return EINVAL;
    }

    for (i = 0; i < N_LAYER; i++) {
        rmon_mlputl(&RMON_DEBUG_ON, i, l[i]);
        rmon_mlputl(&RMON_DEBUG_ON_STORAGE, i, l[i]);
    }

    free((char *) s);
    return 0;
}

/****** rmon_macros/set_debug_target_from_env() *********************************
*  NAME
*     set_debug_target_from_env() -- Set debug target from environment variable.
*
*  SYNOPSIS
*     static int set_debug_target_from_env(void) 
*
*  FUNCTION
*     Set debug target. Read environment variable "SGE_DEBUG_TARGET" and use it
*     to initialize debug output target. 
*
*     'SGE_DEBUG_TARGET' may either be 'stdout', 'stderr' or a fully qualified
*     file name (that is file name and path). If a file name is given an 
*     already existing file with the same name will be overwritten.
*
*  INPUTS
*     void - none 
*
*  RESULT
*     0 - successful
*     EACCES - file name is invalid or unable to open file
*
*  NOTES
*     MT-NOTE: 'set_debug_target_from_env()' is MT safe with exceptions.
*     MT-NOTE: See introduction!
*
*******************************************************************************/
static int set_debug_target_from_env() {
    const char *env = getenv("SGE_DEBUG_TARGET");
    if (env == nullptr) {
        return 0;
    }

    const char *s = strdup(env);
    if (strcmp(s, "stdout") == 0) {
        rmon_fp = stdout;
    } else if (strcmp(s, "stderr") == 0) {
        rmon_fp = stderr;
    } else if ((rmon_fp = fopen(s, "w")) == nullptr) {
        rmon_fp = stderr;
        fprintf(rmon_fp, MSG_RMON_UNABLETOOPENXFORWRITING_S, s);
        fprintf(rmon_fp, MSG_RMON_ERRNOXY_DS, errno, strerror(errno));
        free((char *) s);
        return EINVAL;
    }

    free((char *) s);
    return 0;
}
