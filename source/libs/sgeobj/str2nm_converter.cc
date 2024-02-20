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

#include <cstring>

#include "uti/sge_rmon_macros.h"

#include "cull/cull.h"

#include "sgeobj/str2nm_converter.h"


static int _lStr2Nm(const lNameSpace *nsp, const char *str) 
{
   int i;
   int ret = NoName;
   int found = 0;

   DENTER(CULL_LAYER);

   if (nsp) {
      for (i = 0; i < nsp->size; i++) {
         DPRINTF(("%d: %s\n", nsp->namev[i]));
         if (!strcmp(nsp->namev[i], str)) {
            found = 1;
            break;
         }
      }

      if (found)
         ret = nsp->lower + i;
      else
         ret = NoName;
   }

   DRETURN(ret);
}

int lStr2NmGenerator(const char *str, lNameSpace *ns)
{
   const lNameSpace *nsp;
   int ret;

   DENTER(CULL_LAYER);

   for (nsp = ns; nsp->lower; nsp++) {
      if ((ret = _lStr2Nm(nsp, str)) != NoName) {
         DPRINTF(("Name: %s Id: %d\n", str, ret));
         DRETURN(ret);
      }
   }

   DRETURN(NoName);
}


#if 0
int main(int argc, char *argv[])
{

    DENTER_MAIN(CULL_LAYER, "str2nm_converter");
   
    if (argc == 1) {
      printf("Pfirdi God !\n");
      return -1;
    }  
    
    while (argc > 1) {
       printf("%s -> %d\n", argv[argc-1], lStr2NmGenerator(argv[argc-1], nmv));
       argc--;
    }
    DRETURN(0);
}
#endif
