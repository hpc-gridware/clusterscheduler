#!/bin/sh
#
#___INFO__MARK_BEGIN__
##########################################################################
#
#  The Contents of this file are made available subject to the terms of
#  the Sun Industry Standards Source License Version 1.2
#
#  Sun Microsystems Inc., March, 2001
#
#
#  Sun Industry Standards Source License Version 1.2
#  =================================================
#  The contents of this file are subject to the Sun Industry Standards
#  Source License Version 1.2 (the "License"); You may not use this file
#  except in compliance with the License. You may obtain a copy of the
#  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
#
#  Software provided under this License is provided on an "AS IS" basis,
#  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
#  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
#  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
#  See the License for the specific provisions governing your rights and
#  obligations concerning the Software.
#
#  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
#
#  Copyright: 2001 by Sun Microsystems, Inc.
#
#  All Rights Reserved.
#
#  Portions of this software are Copyright (c) 2024-2025 HPC-Gridware GmbH
#
##########################################################################
#___INFO__MARK_END__

#
# search for symbols of non-reentrant functions in object modules, libraries and binaries
#

if [ "$SKIP_NONREENTRANT" != "" ]; then
   exit 0
fi


Filter1()
{
   egrep '[^a-zA-Z_]asctime$|[^a-zA-Z_]localtime$|[^a-zA-Z_]ctime$|[^a-zA-Z_]gmtime$|[^a-zA-Z_]gethostbyname$|[^a-zA-Z_]gethostbyaddr$|[^a-zA-Z_]gethostent$|[^a-zA-Z_]getservbyname$|[^a-zA-Z_]getservbyport$|[^a-zA-Z_]getservent$|[^a-zA-Z_]strtok$|[^a-zA-Z_]getgrnam$|[^a-zA-Z_]getgrent$|[^a-zA-Z_]getgrgid$|[^a-zA-Z_]fgetgrent$|[^a-zA-Z_]getpwnam$|[^a-zA-Z_]getpwent$|[^a-zA-Z_]getpwuid$|[^a-zA-Z_]fgetpwent$|[^a-zA-Z_]getpwnam$|[^a-zA-Z_]getpwent$|[^a-zA-Z_]getpwuid$|[^a-zA-Z_]fgetpwent$|[^a-zA-Z_]getlogin$|[^a-zA-Z_]getspnam$|[^a-zA-Z_]getspent$|[^a-zA-Z_]fgetspent$|[^a-zA-Z_]readdir$|[^a-zA-Z_]ttyname|ctermid$|[^a-zA-Z_]tmpnam$|[^a-zA-Z_]tmpnam$'
}

Filter2()
{
      egrep '[^a-zA-Z_]asctime@|[^a-zA-Z_]localtime@|[^a-zA-Z_]ctime@|[^a-zA-Z_]gmtime@|[^a-zA-Z_]gethostbyname@|[^a-zA-Z_]gethostbyaddr@|[^a-zA-Z_]gethostent@|[^a-zA-Z_]getservbyname@|[^a-zA-Z_]getservbyport@|[^a-zA-Z_]getservent@|[^a-zA-Z_]strtok@|[^a-zA-Z_]getgrnam@|[^a-zA-Z_]getgrent@|[^a-zA-Z_]getgrgid@|[^a-zA-Z_]fgetgrent@|[^a-zA-Z_]getpwnam@|[^a-zA-Z_]getpwent@|[^a-zA-Z_]getpwuid@|[^a-zA-Z_]fgetpwent@|[^a-zA-Z_]getpwnam@|[^a-zA-Z_]getpwent@|[^a-zA-Z_]getpwuid@|[^a-zA-Z_]fgetpwent@|[^a-zA-Z_]getlogin@|[^a-zA-Z_]getspnam@|[^a-zA-Z_]getspent@|[^a-zA-Z_]fgetspent@|[^a-zA-Z_]readdir@|[^a-zA-Z_]ttyname@|ctermid@|[^a-zA-Z_]tmpnam@|[^a-zA-Z_]tmpnam@'
}

Filter3()
{
      egrep ' _asctime$| _localtime$| _ctime$| _gmtime$| _gethostbyname$| _gethostbyaddr$| _gethostent$| _getservbyname$| _getservbyport$| _getservent$| _strtok$| _getgrnam$| _getgrent$| _getgrgid$| _fgetgrent$| _getpwnam$| _getpwent$| _getpwuid$| _fgetpwent$| _getpwnam$| _getpwent$| _getpwuid$| _fgetpwent$| _getlogin$| _getspnam$| _getspent$| _fgetspent$| _readdir$| _ttyname$|ctermid$| _tmpnam$| _tmpnam$'
}

Filter4()
{
   egrep '^asctime[^_a-zA-Z0-9]|^localtime[^_a-zA-Z0-9]|^ctime[^_a-zA-Z0-9]|^gmtime[^_a-zA-Z0-9]|^gethostbyname[^_a-zA-Z0-9]|^gethostbyaddr[^_a-zA-Z0-9]|^gethostent[^_a-zA-Z0-9]|^getservbyname[^_a-zA-Z0-9]|^getservbyport[^_a-zA-Z0-9]|^getservent[^_a-zA-Z0-9]|^strtok[^_a-zA-Z0-9]|^getgrnam[^_a-zA-Z0-9]|^getgrent[^_a-zA-Z0-9]|^getgrgid[^_a-zA-Z0-9]|^fgetgrent[^_a-zA-Z0-9]|^getpwnam[^_a-zA-Z0-9]|^getpwent[^_a-zA-Z0-9]|^getpwuid[^_a-zA-Z0-9]|^fgetpwent[^_a-zA-Z0-9]|^getpwnam[^_a-zA-Z0-9]|^getpwent[^_a-zA-Z0-9]|^getpwuid[^_a-zA-Z0-9]|^fgetpwent[^_a-zA-Z0-9]|^getlogin[^_a-zA-Z0-9]|^getspnam[^_a-zA-Z0-9]|^getspent[^_a-zA-Z0-9]|^fgetspent[^_a-zA-Z0-9]|^readdir[^_a-zA-Z0-9]|^ttyname[^_a-zA-Z0-9]|ctermid[^_a-zA-Z0-9]|^tmpnam[^_a-zA-Z0-9]|^tmpnam[^_a-zA-Z0-9]'
}

Filter()
{
case $arch in
sol-sparc|sol-sparc64|sol-x86|sol-amd64)
   Filter1
   ;;
lx2*)
   case $1 in
   *.o)
      # echo "$1 is object"
      Filter1
      ;;
   *)
      # echo "$1 is binary/library"
      Filter2
      ;;
   esac
   ;;
darwin)
   Filter3
   ;;
esac
}

ge_nonreentrant=false
arch=none
waiver=false
docheck=true

while [ $# -ge 2 ]; do
   case $1 in
   -w)
      waiver=true
      shift
      ;;
   -a)
      shift
      arch=$1
      shift
      ;;
   *)
      break
      ;;
   esac
done

if [ $arch = "none" ]; then
   arch=`$SGE_ROOT/util/arch`
   echo "ARCH = $arch"
fi

if [ $waiver = true ]; then  
   case $arch in 
   sol-sparc|sol-sparc64|sol-x86|sol-amd64|lx2*)
      ;;
   *)
      # when run with -w option we don't check at all in these cases
      docheck=false
      ;;
   esac
fi

nonreentrant=0
for f in $*; do
   if [ $docheck = true ]; then
      echo "### $f"
      if [ `nm $f | Filter $f | wc -l` -gt 0 ]; then 
         nonreentrant=1
         # do it again just to get the output
         nm $f | Filter $f
	      if [ $waiver = true ]; then
            rm $f
	      fi
      fi
   fi
done
exit $nonreentrant
