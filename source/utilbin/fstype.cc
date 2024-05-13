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
 *  Portions of this software are Copyright (c) 2011 Univa Corporation
 *
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <cerrno>
#include <cstring>

#if defined(DARWIN) || defined(FREEBSD) || defined(NETBSD)
#  include <sys/param.h>
#  include <sys/mount.h>
#elif defined(LINUX)
#  include <sys/vfs.h>
#  include "sge_string.h"
#else
#  include <sys/types.h>
#  include <sys/statvfs.h>
#endif

#if defined(SOLARIS)
#include <kstat.h>
#include <nfs/nfs.h>
#include <nfs/nfs_clnt.h>
#endif

#define BUF_SIZE 8 * 1024

int main(int argc, char *argv[]) {

   int ret=1;

   if (argc < 2) {
      printf("Usage: fstype <directory>\n");
      return 1;
   } else {
#if defined(LINUX)
   struct statfs buf;
   FILE *fd = nullptr;
   char buffer[BUF_SIZE];
   ret = statfs(argv[1], &buf);
#elif defined(DARWIN) || defined(FREEBSD) || (defined(NETBSD) && !defined(ST_RDONLY))
   struct statfs buf;
   ret = statfs(argv[1], &buf);
#else
   struct statvfs buf;
   ret = statvfs(argv[1], &buf);
#endif

   if(ret!=0) {
      printf("Error: %s\n", strerror(errno));
      return 2;
   }

#if defined (DARWIN) || defined(FREEBSD) || defined(NETBSD)
   printf("%s\n", buf.f_fstypename);
#elif defined(LINUX)
   /* 0x6969 is NFS_SUPER_MAGIC (see statfs(2) man page) */
   if (buf.f_type == 0x6969) {
      /*
       * Linux is not able to detect the right nfs version form the statfs struct.
       * f_type always returns nfs, even when it's a nfs4. We are looking into
       * the /etc/mtab file until we found a better solution to do this.
       */
      fd = fopen("/etc/mtab", "r");
      if (fd == nullptr) {
         fprintf(stderr, "file system type could not be detected\n");
         printf("unknown fs\n");
         return 1;
      } else {
         bool found_line = false;
         sge_strip_white_space_at_eol(argv[1]);
         sge_strip_slash_at_eol(argv[1]);

         while (fgets(buffer, sizeof(buffer), fd) != nullptr) {
            //char* export = nullptr; /* where is the nfs exported*/
            char* mountpoint = nullptr; /*where is it mounted to */
            char* fstype = nullptr; /*type of exported file system */

            /* export = */sge_strtok(buffer, " \t");
            mountpoint = sge_strtok(nullptr, " \t");
            fstype = sge_strtok(nullptr, " \t");

            /* search only in valid lines that contain NFS4 mounts */
            if (mountpoint != nullptr && fstype != nullptr && strcmp(fstype, "nfs4") == 0) {
               /* search mountpoint in given path */
               char *pos = strstr(argv[1], mountpoint);
               if (pos == argv[1]) {
                  /* we found the mountpoint at the start of the given path, this is it! */
                  found_line = true;
                  printf ("%s\n", fstype);
                  break;
               }
            }
         }

         fclose(fd);
         if (!found_line) { /*if type could not be detected via /etc/mtab, then we have to print out "nfs"*/
            printf("nfs\n");
         }
      }
   } else {
      if (buf.f_type == 0x52654973) {
         printf("reiserfs\n");
      } else if (buf.f_type == 0x2fc12fc1) {
         printf("zfs\n");
      } else if (buf.f_type == 0xef53) {
         printf("ext4\n");
      } else if (buf.f_type == 0x58465342) {
         printf("xfs\n");
      } else if (buf.f_type == 0x1021994) {
         printf("tmpfs\n");
      } else {
         printf("0x%lx\n", (long unsigned int)buf.f_type);
      }
   }
#else
   printf("%s\n", buf.f_basetype);
#endif
   }
   return 0;
}
