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
#include <Xmt/Xmt.h>
#include <Xmt/Dialogs.h>

#include "commlib.h"
#include "sge_all_listsL.h"
#include "sge_gdi_intern.h"
#include "qmon_rmon.h"
#include "qmon_cull.h"
#include "qmon_timer.h"
#include "qmon_comm.h"
#include "qmon_appres.h"
#include "qmon_globals.h"
#include "qmon_init.h"
#include "qmon_message.h"
#include "qm_name.h"

static tTimer timer_struct;


static char *sge_gdi_list_timers[] = {
   "ADMINHOST",
   "SUBMITHOST",
   "EXECHOST",
   "QUEUE",
   "JOB",
   "EVENT",
   "COMPLEX",
   "ORDER",
   "MASTER_EVENT",
   "CONFIG",
   "MANAGER",
   "OPERATOR",
   "PE",
   "SC",
   "USER"
   "USERSET",
   "PROJECT",
   "SHARETREE",
   "CKPT",
   "CALENDAR",
   "SCHEDD_INFO",
   "ZOMBIE_JOBS",
   "END"
};

#define NR_TIMERS    XtNumber(sge_gdi_list_timers)

static tQmonPoll QmonListTimer[] = {
   /*  type   timercount fetch_frequency fetch update_proc_list */
   { SGE_ADMINHOST_LIST, 0, 1, 0, NULL },  
   { SGE_SUBMITHOST_LIST, 0, 1, 0, NULL },  
   { SGE_EXECHOST_LIST, 0, 1, 0, NULL },  
   { SGE_QUEUE_LIST, 0, 1, 0, NULL },  
   { SGE_JOB_LIST, 0, 1, 0, NULL },  
   { SGE_EVENT_LIST, 0, 1, 0, NULL },  
   { SGE_COMPLEX_LIST, 0, 1, 0, NULL },  
   { SGE_ORDER_LIST, 0, 1, 0, NULL },  
   { SGE_MASTER_EVENT, 0, 1, 0, NULL },  
   { SGE_CONFIG_LIST, 0, 1, 0, NULL },  
   { SGE_MANAGER_LIST, 0, 1, 0, NULL },  
   { SGE_OPERATOR_LIST, 0, 1, 0, NULL },
   { SGE_PE_LIST, 0, 1, 0, NULL }, 
   { SGE_SC_LIST, 0, 1, 0, NULL },
   { SGE_USER_LIST, 0, 1, 0, NULL },
   { SGE_USERSET_LIST, 0, 1, 0, NULL }, 
   { SGE_PROJECT_LIST, 0, 1, 0, NULL }, 
   { SGE_SHARETREE_LIST, 0, 1, 0, NULL },
   { SGE_CKPT_LIST, 0, 1, 0, NULL },
   { SGE_CALENDAR_LIST, 0, 1, 0, NULL },
   { SGE_JOB_SCHEDD_INFO, 0, 1, 0, NULL },
   { SGE_ZOMBIE_LIST, 0, 1, 0, NULL },
   { 0, 0, 0, 0, NULL}
};


/*-------------------------------------------------------------------------*/
static void qmonTimerCheckInteractiveJob(XtPointer cld, XtIntervalId *id);

/*-------------------------------------------------------------------------*/
void qmonStartPolling(
XtAppContext app 
) {

   DENTER(GUI_LAYER, "qmonStartPolling");
   
   timer_struct.timerapp = app;
   timer_struct.timeout = FETCH_TIME;
   timer_struct.timerproc = qmonListTimerProc;
   timer_struct.timerdata = NULL;
   
   timer_struct.timerid = XtAppAddTimeOut(timer_struct.timerapp,
                           timer_struct.timeout,
                           timer_struct.timerproc,
                           &timer_struct);

   DEXIT;
}

/*-------------------------------------------------------------------------*/
void qmonStopPolling(void)
{

   DENTER(GUI_LAYER, "qmonStopPolling");

   XtRemoveTimeOut(timer_struct.timerid);

   DEXIT;
}


/*-------------------------------------------------------------------------*/
void qmonStartTimer(
long type 
) {
   int i;
   
   DENTER(GUI_LAYER, "qmonStartTimer");

   for (i=0; i<XtNumber(QmonListTimer); i++) {
      if ( (type&(1<<i))) {
         DPRINTF(("Timer %d enabled\n", i)); 
         QmonListTimer[i].timercount++;
         DPRINTF(("Timer%d %s/%d enabled\n", i, 
            sge_gdi_list_timers[i], QmonListTimer[i].timercount));
         QmonListTimer[i].fetch = 0;
      }
   }

   /*
   ** force update
   */
   XtRemoveTimeOut(timer_struct.timerid);
   qmonListTimerProc(&timer_struct, &timer_struct.timerid);

   DEXIT;
}


/*-------------------------------------------------------------------------*/
void qmonStopTimer(
long type 
) {
   int i;

   DENTER(GUI_LAYER, "qmonStopTimer");

   for (i=0; i<XtNumber(QmonListTimer); i++) {
      if ( (type&(1<<i)) && QmonListTimer[i].timercount) {
         QmonListTimer[i].timercount--;
         DPRINTF(("Timer %s/%d disabled\n", 
            sge_gdi_list_timers[i], QmonListTimer[i].timercount+1 ));

      }
   }

   DEXIT;
}


/*-------------------------------------------------------------------------*/
void qmonListTimerProc(
XtPointer cld,
XtIntervalId *id 
) {
   tTimer *td = (tTimer *)cld;
   tUpdateRec *uproc;
   int i;
   u_long32 selector = 0;

   DENTER(GUI_LAYER, "qmonListTimerProc");

   /*
   ** first we fetch all lists and then we call the update procs
   */
   for (i=0; i<XtNumber(QmonListTimer); i++) {
      if (QmonListTimer[i].timercount > 0 && QmonListTimer[i].fetch == 0) {
         selector |= (1<<i);
      }
   } 
   qmonMirrorMulti(selector);
   
   /*
   ** call the registered update procs and reset fetch
   */
   for (i=0; i<XtNumber(QmonListTimer); i++) {
      if (QmonListTimer[i].timercount > 0 && QmonListTimer[i].fetch == 0) {
         DPRINTF(("Update for %s\n", sge_gdi_list_timers[i]));
         QmonListTimer[i].fetch = QmonListTimer[i].fetch_frequency;
         for (uproc = QmonListTimer[i].update_proc_list; uproc;
                  uproc = uproc->next) {
            DPRINTF(("Update proc %s called\n", XrmQuarkToString(uproc->id)));
            uproc->proc();   
         }
      }
      else {
         QmonListTimer[i].fetch--;
      }
   }
          
   /* 
   ** keep the timer running
   */
   td->timerid = XtAppAddTimeOut(td->timerapp,
                           td->timeout,
                           td->timerproc,
                           td );
   
   DEXIT;
   
}

/*-------------------------------------------------------------------------*/
int qmonTimerAddUpdateProc(
long type,
String name,
tUpdateProc proc 
) {
   tUpdateRec *new, *current;
   int i;
   
   DENTER(GUI_LAYER, "qmonTimerAddUpdateProc");

   /*
   ** malloc a new element
   */
   
   new = (tUpdateRec*) XtMalloc(sizeof(tUpdateRec));
   new->id = XrmStringToQuark(name);
   new->proc = proc;
   new->next = NULL;

   /*
   ** get the correct index
   */
   for (i=0; i<XtNumber(QmonListTimer); i++) {
      if ( (type&(1<<i)))
         break;
   }

   /* 
   ** go to the end of the update proc list if it exists
   */
   if (!QmonListTimer[i].update_proc_list) {
      QmonListTimer[i].update_proc_list = new;
   }
   else {
      current = QmonListTimer[i].update_proc_list; 
      while (current->next) 
         current = current->next;
      current->next = new;
   }
   
   for (current=QmonListTimer[i].update_proc_list; current; 
            current=current->next)
      DPRINTF(("****** Update Proc: %s\n", XrmQuarkToString(current->id) ));

   DEXIT;
   return new->id;
}


/*-------------------------------------------------------------------------*/
void qmonTimerRmUpdateProc(
long type,
String name 
) {
   tUpdateRec *current;
   tUpdateRec *prev = NULL;
   int id;
   int i;
   
   DENTER(GUI_LAYER, "qmonTimerRmUpdateProc");
   
   /*
   ** get the correct index
   */
   for (i=0; i<XtNumber(QmonListTimer); i++) {
      if ( (type&(1<<i)))
         break;
   }
   current = QmonListTimer[i].update_proc_list;
   id = XrmStringToQuark(name);

   /* find the right Update Proc entry */
   while (current && current->id != id) {
      prev = current;
      current = current->next;
   }

   /* unchain and free */
   if (current && current->id == id) {
      if (prev)
         prev->next = current->next;
      else
         QmonListTimer[i].update_proc_list = NULL;

      XtFree((char*) current);
      current = NULL;
   }
   
   DEXIT;
}

/*-------------------------------------------------------------------------*/
/* check interactive job, cld contains the job_number                      */
/*-------------------------------------------------------------------------*/
void qmonTimerCheckInteractive(w, cld, cad)
Widget w;
XtPointer cld, cad;
{
   static tTimer timer;
   static Boolean initialized = False;

   DENTER(GUI_LAYER, "qmonTimerCheckInteractive");

   if (!initialized) {
      timer.timerapp = XtWidgetToApplicationContext(w);
      timer.timeout = FETCH_TIME;
      timer.timerproc = qmonTimerCheckInteractiveJob;
      timer.timerdata = cld;
   }
   timer.timerid = XtAppAddTimeOut( timer.timerapp,
                                    timer.timeout,
                                    timer.timerproc,
                                    &timer);

   DEXIT;
}

/*-------------------------------------------------------------------------*/
/* Timer proc to check if interactive job has been started successfully    */
/*-------------------------------------------------------------------------*/
static void qmonTimerCheckInteractiveJob(
XtPointer cld,
XtIntervalId *id 
) {
   tTimer *td = (tTimer *)cld;
   int status;
   lList *lp = NULL;
   lListElem *ep = NULL;
   lList *alp = NULL;
   lListElem *aep = NULL;
   lEnumeration *what = NULL;
   lCondition *where = NULL;
   Boolean cont = False;
   char msg[256];
   u_long32 job_number = (u_long32)td->timerdata;
   int contact_ok = 1;

   DENTER(GUI_LAYER, "qmonTimerCheckInteractiveJob");

   /*
   ** ask if the master is available, if not show warning dialog
   ** and leave the timerproc
   */
   status = check_isalive(sge_get_master(0));
   DPRINTF(("check_isalive() returns %d (%s)\n", status, cl_errstr(status)));

   if (status != CL_OK) {
      if (status == CL_UNKNOWN_RECEIVER)
         sprintf(msg, "can't reach qmaster\n");
      else
         sprintf(msg, "can't reach:\n%s\n", cl_errstr(status));

      contact_ok = XmtDisplayErrorAndAsk(AppShell, "nocontact",
                                                msg, "Retry", "Abort",
                                                XmtYesButton, NULL);
      /*
      ** we don't want to retry, so go down
      */
      if (!contact_ok) {
         DEXIT;
         qmonExitFunc(1);
      }
   }
   
      
   /*
   ** everything went ok so fetch the lists
   ** first we fetch all lists and then we call the update procs
   */
   what = lWhat("%T(ALL)", JB_Type);
   where = lWhere("%T(%I == %u)", JB_Type, JB_job_number, job_number);
   alp = sge_gdi(SGE_JOB_LIST, SGE_GDI_GET, &lp, where, what);

   aep = lFirst(alp);
   ep = lFirst(lp);

   if (!ep) {
      sprintf(msg, "No free slots for interactive job %d !", (int) job_number);
      qmonMessageShow(AppShell, True, msg);
      cont = False;
   }

   if (ep && aep && lGetUlong(aep, AN_status) == STATUS_OK) {
      lListElem *jatep;
      for_each (jatep, lGetList(ep, JB_ja_tasks)) {
         if ((lGetUlong(jatep, JAT_status) & JRUNNING) || 
            (lGetUlong(jatep, JAT_status) & JTRANSITING))
            cont = False;
         else
            cont = True;
      }
   }
   
   lFreeWhat(what);
   lFreeWhere(where);
   lFreeList(alp);
   lFreeList(lp);
   
   /* 
   ** keep the timer running
   */
   if (cont)
      td->timerid = XtAppAddTimeOut(td->timerapp,
                           td->timeout,
                           td->timerproc,
                           td );
   
   DEXIT;
}

