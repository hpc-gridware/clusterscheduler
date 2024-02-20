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
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>
#include <cctype>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysymdef.h>
#include <sys/types.h>
#include <csignal>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <ctime>
#include <setjmp.h>
#include <cstdlib>

#include "cull/cull.h"
#include "sge_load_sensor.h"
#include "sge_rmon.h"
#include "sge_bootstrap.h"
#include "sge_bootstrap.h"
#include "sge_reportL.h"

#if 0
Qidle contains a small C-program which reports whether there is interactive
usage on a host. The reports are in a format compliant with the Grid Engine
load sensor interface (see the Grid Engine manual), so qidle can be used
to withdraw a host from a batch cluster if it is used interactively and
to make it available for batch jobs if the host has not been used by interactive
users for a while. Qidle also can be used to trigger suspend/resume events
for batch jobs or even checkpointing and migration. Interactive usage is
detected via monitoring the X-Server activity on the host. See the README
file included in the module for some important information in this context.
There is no further documentation besides the README.
<p>The module is relatively independent of the Grid Engine version, so
it should be possible to get qidle working, at least with a small bit of
effort.
#endif

struct times {
   int sek;
   int min;
   int std;
};

struct times converttime(int s);
struct times getidletime(Display *dpy);
void getextloadsensor(const char*qualified_hostname, const char *binary_path, char *script);
int XIOHandler(Display *dpy);

jmp_buf restartpoint;
time_t starttime;

int main(argc, argv)
int argc;
char ** argv;
{
   Display *dpy;
   char *dpy_name, buffer[20], *host, *script;
   int i, dispopen, *status, enter, entersum, DEBUG; 
   fd_set readfds;
   struct times tt;
   struct timeval timeout;
   int fd;

   dpy = nullptr;
   status = nullptr;
   dpy_name = host = script = nullptr;
   buffer[0] = '\0';
   i = dispopen = enter = entersum = DEBUG = 0;
   time(&starttime);
   host = getenv("HOST");
   dpy_name = sge_malloc(strlen(host)+3);
   strcpy(dpy_name, host);
   component_set_qualified_hostname(host);
   strcat(dpy_name, ":0");

   for (i=1; i<argc; i++) {
      if (!strcmp("-d",argv[i])) {
         dpy_name = argv[++i];
         host = sge_malloc(strlen(dpy_name)-1);
         strncpy(host, dpy_name, (strchr(dpy_name, ':'))-dpy_name);
      } else if (!strcmp("-debug", argv[i])) {
         DEBUG = 1;
         i++;
      } else if (!strcmp("-s",argv[i])) {
         script = argv[++i];
      } else {
         fprintf(stderr,"usage: %s [-s loadsensorscript] [-d display] [-debug]\n",argv[0]);
         exit(-1);
      }
   }   

   if (!DEBUG) {
      close(2);
      fd = open("/dev/null", O_WRONLY, 0);
   }

   XSetIOErrorHandler(XIOHandler);
   while(1) {
      if (setjmp(restartpoint) != 0) 
         dispopen = 0;
      entersum += enter;
      if (!dispopen && (!enter || entersum>3)) {
         dpy = XOpenDisplay(dpy_name);
         if (entersum>3) 
            entersum = 0;
      }
      if (dpy == nullptr) {
         timeout.tv_sec  = 20;
         timeout.tv_usec = 0;
      } else {
         timeout.tv_sec  = 0;
         timeout.tv_usec = 1000;
         dispopen = 1;
      }
      FD_ZERO(&readfds);
      FD_SET(0, &readfds);
      if (select(1, &readfds, nullptr, nullptr, &timeout)) {
         fgets(buffer, 5, stdin);
         if (!strcmp(buffer, "quit"))
         exit(0);
         enter = 0;
         if (!strncmp(buffer, "\n", 1)) {	
            enter = 1;
            printf("begin\n"); fflush(nullptr);
            tt = getidletime(dpy);
            printf("%s:idle:%d:%02d:%02d\n", host, tt.std, tt.min, tt.sek); fflush(nullptr);
            if (script != nullptr)
               getextloadsensor(component_get_qualified_hostname(), bootstrap_get_binary_path(), script);
            printf("end\n"); fflush(nullptr);
         }
         else 
            entersum = 0;
      }
      getidletime(dpy);
   } 
   return 0;
}


/* Calculates Idletime */
struct times getidletime(dpy)
Display *dpy;
{
   static int xold, yold;
   time_t akttime;
   int xnew, ynew, x, y;
   int idle;
   unsigned int mask;
   Window root, child;
   time(&akttime); 
   if (dpy != nullptr) {
      root = DefaultRootWindow(dpy);
      XQueryPointer(dpy, root, &root, &child, &xnew, &ynew, &x, &y, &mask);  
      if (xnew != xold || ynew != yold) {
         xold = xnew; yold = ynew;
         time(&starttime);
      }
   }
   idle = (int)(difftime(akttime, starttime));  
   return converttime(idle);
}

/* Converts seconds in hour:min:sec format */
struct times converttime(s)
int s;
{
   struct times tt;
   tt.min = tt.std = 0;
   tt.sek = s;
   if (s >= 60) {
      tt.min = s / 60;
      tt.sek = s % 60;
      if (tt.min >= 60) {
         tt.std = tt.min / 60;
         tt.min = tt.min % 60;
      }
   }	
   return tt;
}


/* Gets external loadsensor values */
void getextloadsensor(const char *qualified_hostname, const char *binary_path, 
                      char *script)
{
   lList *lpp = nullptr;
   lListElem *ep = nullptr;
   sge_ls_get(qualified_hostname, binary_path, &lpp);
   for_each_ep(ep, lpp) {
      printf("%s:%s:%s\n", 
      lGetHost(ep, LR_host),
      lGetString(ep, LR_name),
      lGetString(ep, LR_value)); fflush(nullptr);
   }
}

/* Catches Broken Display Connection */
int XIOHandler(dpy)
Display *dpy;
{
   XCloseDisplay(dpy);
   longjmp (restartpoint, 1);
}
