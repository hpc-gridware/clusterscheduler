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
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

int main(int argc, char **argv)
{
   int block_sigusr1 = 1;
   int block_sigusr2 = 1;

   if (argc < 2) {
      printf("usage: %s progpath [progargs]\n", argv[0]);
      return 1;
   }
 
   if (block_sigusr1) {
      sigset_t sigset;
      sigemptyset(&sigset);
      sigaddset(&sigset, SIGUSR1);
      sigprocmask(SIG_BLOCK, &sigset, nullptr);
      printf("blocking signal SIGUSR1\n");
      fflush(stdout);
   }
   if (block_sigusr2) {
      sigset_t sigset;
      sigemptyset(&sigset);
      sigaddset(&sigset, SIGUSR2);
      sigprocmask(SIG_BLOCK, &sigset, nullptr);
      printf("blocking signal SIGUSR2\n");
      fflush(stdout);
   }

   argv++;

   execv(argv[0], argv);

   printf("Signal Blocker exec failed\n");
   return 127;
}
