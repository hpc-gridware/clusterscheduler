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
 *   Copyright: 2008 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <cstring>

/*
 * Implements a binary which reads in from stdin in "raw" mode thill 
 * the first newline character occures
 *
 * This command can be used as a replacement for the buildin command
 * "read" in bourne shell. On most platforms this not able to read
 * in raw mode therefore it does backslash escaping. 
 *
 * As a result "binary" data with masked newline and backslash
 * characters can be read into a bourne shell scripts with this 
 * command.
 */

#define BUF_SIZE 8 * 1024

int main(int argc, char *argv[])
{
   char buffer[BUF_SIZE];
   char *ret;

   setvbuf(stdin,nullptr,_IONBF,0);

   ret = fgets(buffer, BUF_SIZE, stdin);
   if (ret != nullptr) {
      size_t length = strlen(ret);

      while (length-- > 0) {
         fputc(*(ret++), stdout);
      }
   }
   return feof(stdin);
}
