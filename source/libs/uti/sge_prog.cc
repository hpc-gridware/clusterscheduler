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

#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <pthread.h>

#include "uti/sge_rmon.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"
#include "uti/sge_prog.h"
#include "uti/sge_error_class.h"
#include "uti/sge_uidgid.h"
#include "uti/msg_utilib.h"
#include "uti/sge_bootstrap.h"

#include "sgeobj/sge_answer.h"

#include "comm/cl_commlib.h"

/* Must match Qxxx defines in sge_prog.h */
const char *prognames[] =
        {
                "unknown",
                "qalter",       /* 1   */
                "qconf",       /* 2   */
                "qdel",       /* 3   */
                "qhold",       /* 4   */
                "qmaster",       /* 5   */
                "qmod",       /* 6   */
                "qresub",       /* 7   */
                "qrls",       /* 8   */
                "qselect",       /* 9   */
                "qsh",       /* 10  */
                "qrsh",       /* 11  */
                "qlogin",       /* 12  */
                "qstat",       /* 13  */
                "qsub",       /* 14  */
                "execd",       /* 15  */
                "qevent",       /* 16  */
                "qrsub",       /* 17  */
                "qrdel",       /* 18  */
                "qrstat",       /* 19  */
                "unknown",       /* 20  */
                "unknown",       /* 21  */
                "qmon",       /* 22  */
                "schedd",       /* 23  */
                "qacct",       /* 24  */
                "shadowd",       /* 25  */
                "qhost",       /* 26  */
                "spoolinit",       /* 27  */
                "japi",       /* 28  */
                "drmaa",       /* 29  */
                "qping",       /* 30  */
                "qquota",       /* 31  */
                "sge_share_mon"     /* 32  */
        };

const char *threadnames[] = {
        "main",              /* 1 */
        "listener",          /* 2 */
        "event_master",      /* 3 */
        "timer",             /* 4 */
        "worker",            /* 5 */
        "signaler",          /* 6 */
        "scheduler",         /* 7 */
        "tester"             /* 8 */
};


typedef struct prog_state_str {
   char *sge_formal_prog_name;  /* taken from prognames[] */
   char *qualified_hostname;
   char *unqualified_hostname;
   u_long32 who;                   /* Qxxx defines  QUSERDEFINED  */
   u_long32 uid;
   u_long32 gid;
   bool daemonized;
   char *user_name;
   char *default_cell;
   sge_exit_func_t exit_func;
} prog_state_t;


static pthread_once_t prog_once = PTHREAD_ONCE_INIT;
static pthread_key_t prog_state_key;

static void prog_once_init(void);

static void prog_state_destroy(void *theState);

static prog_state_t *prog_state_getspecific(pthread_key_t aKey);

static void sge_show_me(void);

static void uti_state_set_sge_formal_prog_name(const char *s);

static void uti_state_set_uid(u_long32 uid);

static void uti_state_set_gid(u_long32 gid);

static void uti_state_set_user_name(const char *s);

static void uti_state_set_default_cell(const char *s);

/****** uti/prog/uti_state_get_????() ************************************
*  NAME
*     uti_state_get_????() - read access to utilib global variables
*
*  FUNCTION
*     Provides access to either global variable or per thread global variable.
******************************************************************************/
const char *uti_state_get_sge_formal_prog_name(void) {
   prog_state_t *prog_state = nullptr;

   pthread_once(&prog_once, prog_once_init);

   prog_state = prog_state_getspecific(prog_state_key);

   return prog_state->sge_formal_prog_name;
}

const char *uti_state_get_qualified_hostname(void) {
   prog_state_t *prog_state = nullptr;

   pthread_once(&prog_once, prog_once_init);

   prog_state = prog_state_getspecific(prog_state_key);

   return prog_state->qualified_hostname;
}

const char *uti_state_get_unqualified_hostname(void) {
   prog_state_t *prog_state = nullptr;

   pthread_once(&prog_once, prog_once_init);

   prog_state = prog_state_getspecific(prog_state_key);

   return prog_state->unqualified_hostname;
}

u_long32 uti_state_get_mewho(void) {
   prog_state_t *prog_state = nullptr;

   pthread_once(&prog_once, prog_once_init);

   prog_state = prog_state_getspecific(prog_state_key);

   return prog_state->who;
}

u_long32 uti_state_get_uid(void) {
   prog_state_t *prog_state = nullptr;

   pthread_once(&prog_once, prog_once_init);

   prog_state = prog_state_getspecific(prog_state_key);

   return prog_state->uid;
}

u_long32 uti_state_get_gid(void) {
   prog_state_t *prog_state = nullptr;

   pthread_once(&prog_once, prog_once_init);

   prog_state = prog_state_getspecific(prog_state_key);

   return prog_state->gid;
}

int uti_state_get_daemonized(void) {
   prog_state_t *prog_state = nullptr;

   pthread_once(&prog_once, prog_once_init);

   prog_state = prog_state_getspecific(prog_state_key);

   return prog_state->daemonized;
}

const char *uti_state_get_user_name(void) {
   prog_state_t *prog_state = nullptr;

   pthread_once(&prog_once, prog_once_init);

   prog_state = prog_state_getspecific(prog_state_key);

   return prog_state->user_name;
}

const char *uti_state_get_default_cell(void) {
   prog_state_t *prog_state = nullptr;

   pthread_once(&prog_once, prog_once_init);

   prog_state = prog_state_getspecific(prog_state_key);

   return prog_state->default_cell;
}

/****** uti/prog/uti_state_set_????() ************************************
*  NAME
*     uti_state_set_????() - write access to utilib global variables
*
*  FUNCTION
*     Provides access to either global variable or per thread global variable.
******************************************************************************/
static void uti_state_set_sge_formal_prog_name(const char *s) {
   prog_state_t *prog_state = nullptr;

   prog_state = prog_state_getspecific(prog_state_key);

   prog_state->sge_formal_prog_name = sge_strdup(prog_state->sge_formal_prog_name, s);

   return;
}

void uti_state_set_qualified_hostname(const char *s) {
   prog_state_t *prog_state = nullptr;

   pthread_once(&prog_once, prog_once_init);

   prog_state = prog_state_getspecific(prog_state_key);

   prog_state->qualified_hostname = sge_strdup(prog_state->qualified_hostname, s);

   return;
}

void uti_state_set_unqualified_hostname(const char *s) {
   prog_state_t *prog_state = nullptr;

   pthread_once(&prog_once, prog_once_init);

   prog_state = prog_state_getspecific(prog_state_key);

   prog_state->unqualified_hostname = sge_strdup(prog_state->unqualified_hostname, s);

   return;
}

void uti_state_set_daemonized(int daemonized) {
   prog_state_t *prog_state = nullptr;

   pthread_once(&prog_once, prog_once_init);

   prog_state = prog_state_getspecific(prog_state_key);

   prog_state->daemonized = daemonized ? true : false;

   return;
}

void uti_state_set_mewho(u_long32 who) {
   prog_state_t *prog_state = nullptr;

   pthread_once(&prog_once, prog_once_init);

   prog_state = prog_state_getspecific(prog_state_key);

   prog_state->who = who;

   return;
}

static void uti_state_set_uid(u_long32 uid) {
   prog_state_t *prog_state = nullptr;

   prog_state = prog_state_getspecific(prog_state_key);

   prog_state->uid = uid;

   return;
}

static void uti_state_set_gid(u_long32 gid) {
   prog_state_t *prog_state = nullptr;

   prog_state = prog_state_getspecific(prog_state_key);

   prog_state->gid = gid;

   return;
}

static void uti_state_set_user_name(const char *s) {
   prog_state_t *prog_state = nullptr;

   prog_state = prog_state_getspecific(prog_state_key);

   prog_state->user_name = sge_strdup(prog_state->user_name, s);

   return;
}

static void uti_state_set_default_cell(const char *s) {
   prog_state_t *prog_state = nullptr;

   prog_state = prog_state_getspecific(prog_state_key);

   prog_state->default_cell = sge_strdup(prog_state->default_cell, s);

   return;
}

/****** uti/prog/uti_state_get_exit_func() ************************************
*  NAME
*     uti_state_get_exit_func() -- Return installed exit funciton 
*
*  SYNOPSIS
*     sge_exit_func_t uti_state_get_exit_func(void)
*
*  FUNCTION
*     Returns installed exit funciton. Exit function
*     will be called be sge_exit()
*
*  RESULT
*     sge_exit_func_t - function pointer 
*
*  SEE ALSO
*     uti/unistd/sge_exit() 
******************************************************************************/
sge_exit_func_t uti_state_get_exit_func(void) {
   prog_state_t *prog_state = nullptr;

   pthread_once(&prog_once, prog_once_init);

   prog_state = prog_state_getspecific(prog_state_key);

   return prog_state->exit_func;
}

/****** uti/prog/uti_state_set_exit_func() ************************************
*  NAME
*     uti_state_set_exit_func() -- Installs a new exit handler 
*
*  SYNOPSIS
*     void uti_state_set_exit_func(sge_exit_func_t f)
*
*  FUNCTION
*     Installs a new exit handler. Exit function will be called be sge_exit()
*
*  INPUTS
*     sge_exit_func_t f - new function pointer 
*
*  SEE ALSO
*     uti/unistd/sge_exit() 
******************************************************************************/
void uti_state_set_exit_func(sge_exit_func_t f) {
   prog_state_t *prog_state = nullptr;

   pthread_once(&prog_once, prog_once_init);

   prog_state = prog_state_getspecific(prog_state_key);

   prog_state->exit_func = f;

   return;
}

/****** uti/prog/sge_getme() *************************************************
*  NAME
*     sge_getme() -- Initialize me-struct
*
*  SYNOPSIS
*     void sge_getme(u_long32 program_number)
*
*  FUNCTION
*     Initialize me-struct according to 'program_number'
*
*  INPUTS
*     u_long32 program_number - uniq internal program number
*
*  NOTES
*     MT-NOTE: sge_getme() is MT safe
******************************************************************************/
void sge_getme(u_long32 program_number) {
   char *s = nullptr;
   stringT tmp_str;
   struct hostent *hent = nullptr;

   DENTER(TOP_LAYER);

   pthread_once(&prog_once, prog_once_init);

   /* get program info */
   uti_state_set_mewho(program_number);
   uti_state_set_sge_formal_prog_name(prognames[program_number]);

   /* Fetch hostnames */
   SGE_ASSERT((gethostname(tmp_str, sizeof(tmp_str)) == 0));
   SGE_ASSERT(((hent = sge_gethostbyname(tmp_str, nullptr)) != nullptr));

   uti_state_set_qualified_hostname(hent->h_name);
   s = sge_dirname(hent->h_name, '.');
   uti_state_set_unqualified_hostname(s);
   sge_free(&s);

   /* Bad resolving in some networks leads to short qualified host names */
   if (!strcmp(uti_state_get_qualified_hostname(), uti_state_get_unqualified_hostname())) {
      char tmp_addr[8];
      struct hostent *hent2 = nullptr;
      memcpy(tmp_addr, hent->h_addr, hent->h_length);
      SGE_ASSERT(((hent2 = sge_gethostbyaddr((const struct in_addr *) tmp_addr, nullptr)) != nullptr));

      uti_state_set_qualified_hostname(hent2->h_name);
      s = sge_dirname(hent2->h_name, '.');
      uti_state_set_unqualified_hostname(s);
      sge_free(&s);
      sge_free_hostent(&hent2);
   }

   sge_free_hostent(&hent);

   /* SETPGRP; */
   uti_state_set_uid(getuid());
   uti_state_set_gid(getgid());

   {
      struct passwd *paswd = nullptr;
      char *buffer;
      int size;
      struct passwd pwentry;

      size = get_pw_buffer_size();
      buffer = sge_malloc(size);
      SGE_ASSERT(getpwuid_r((uid_t) uti_state_get_uid(), &pwentry, buffer, size, &paswd) == 0)
      uti_state_set_user_name(paswd->pw_name);
      sge_free(&buffer);
   }

   uti_state_set_default_cell(sge_get_default_cell());

   sge_show_me();

   DRETURN_VOID;
}

/****** uti/prog/sge_show_me() ************************************************
*  NAME
*     sge_show_me() -- Show content of me structure
*
*  SYNOPSIS
*     static void sge_show_me()
*
*  FUNCTION
*     Show content of me structure in debug output
*
*  NOTES
*     MT-NOTE: sge_show_me() is MT safe
******************************************************************************/
static void sge_show_me(void) {
   DENTER(TOP_LAYER);

#ifdef NO_SGE_COMPILE_DEBUG
   return;
#else
   if (!TRACEON) {
      DRETURN_VOID;
   }
#endif

   DPRINTF(("me.who                      >%d<\n", (int) uti_state_get_mewho()));
   DPRINTF(("me.sge_formal_prog_name     >%s<\n", uti_state_get_sge_formal_prog_name()));
   DPRINTF(("me.qualified_hostname       >%s<\n", uti_state_get_qualified_hostname()));
   DPRINTF(("me.unqualified_hostname     >%s<\n", uti_state_get_unqualified_hostname()));
   DPRINTF(("me.uid                      >%d<\n", (int) uti_state_get_uid()));
   DPRINTF(("me.gid                      >%d<\n", (int) uti_state_get_gid()));
   DPRINTF(("me.daemonized               >%d<\n", uti_state_get_daemonized()));
   DPRINTF(("me.user_name                >%s<\n", uti_state_get_user_name()));
   DPRINTF(("me.default_cell             >%s<\n", uti_state_get_default_cell()));

   DRETURN_VOID;
}

/****** uti/prog/prog_once_init() *********************************************
*  NAME
*     prog_once_init() -- One-time executable state initialization.
*
*  SYNOPSIS
*     static prog_once_init(void) 
*
*  FUNCTION
*     Create access key for thread local storage. Register cleanup function.
*
*     This function must be called exactly once.
*
*  INPUTS
*     void - none
*
*  RESULT
*     void - none 
*
*  NOTES
*     MT-NOTE: prog_once_init() is MT safe. 
*
*******************************************************************************/
static void prog_once_init(void) {
   pthread_key_create(&prog_state_key, &prog_state_destroy);
   return;
} /* prog_once_init() */

/****** uti/prog/prog_state_destroy() ******************************************
*  NAME
*     prog_state_destroy() -- Free thread local storage
*
*  SYNOPSIS
*     static void prog_state_destroy(void* theState) 
*
*  FUNCTION
*     Free thread local storage.
*
*  INPUTS
*     void* theState - Pointer to memroy which should be freed.
*
*  RESULT
*     static void - none
*
*  NOTES
*     MT-NOTE: prog_state_destroy() is MT safe.
*
*******************************************************************************/
static void prog_state_destroy(void *theState) {
   prog_state_t *s = (prog_state_t *) theState;

   sge_free(&(s->sge_formal_prog_name));
   sge_free(&(s->sge_formal_prog_name));
   sge_free(&(s->qualified_hostname));
   sge_free(&(s->unqualified_hostname));
   sge_free(&(s->user_name));
   sge_free(&(s->default_cell));
   sge_free(&s);
}

/****** uti/prog/prog_state_getspecific() **************************************
*  NAME
*     prog_state_getspecific() -- Get thread local prog state 
*
*  SYNOPSIS
*     static prog_state_t* prog_state_getspecific(pthread_key_t aKey) 
*
*  FUNCTION
*     Return thread local prog state. 
*
*     If a given thread does call this function for the first time, no thread
*     local prog state is available for this particular thread. In this case the
*     thread local prog state is allocated and set.
*
*  INPUTS
*     pthread_key_t aKey - Key for thread local prog state. 
*
*  RESULT
*     static prog_state_t* - Pointer to thread local prog state
*
*  NOTES
*     MT-NOTE: prog_state_getspecific() is MT safe 
*
*******************************************************************************/
static prog_state_t *prog_state_getspecific(pthread_key_t aKey) {
   prog_state_t *prog_state = nullptr;
   int res = EINVAL;

   if ((prog_state = (prog_state_t *)pthread_getspecific(aKey)) != nullptr) { return prog_state; }

   prog_state = (prog_state_t *) sge_malloc(sizeof(prog_state_t));

   res = pthread_setspecific(prog_state_key, (const void *) prog_state);

   if (0 != res) {
      fprintf(stderr, "pthread_set_specific(%s) failed: %s\n", "prog_state_getspecific", strerror(res));
      abort();
   }

   return prog_state;
} /* prog_state_getspecific() */