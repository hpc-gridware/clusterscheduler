#!/bin/sh
#
# Set file permissions of Cluster Scheduler distribution
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
##########################################################################
#___INFO__MARK_END__
#
#
# The $OPTFILES are not mandatory for a distribution and will be set only if
# they exist
#
# This script must be called by user root on a machine where user root has
# permissions to change the ownership of a file
# 
# It is not necessary to run this script if the distribtuon has been
# installed with pkgadd, since pkgadd takes care about the correct
# permissions.
#

PATH=/bin:/usr/bin:/usr/sbin

umask 022


#---------------------------------------------------------------------
# MAIN MAIN
#


if [ -z "$SGE_ROOT" -o ! -d "$SGE_ROOT" ]; then
   echo 
   echo ERROR: Please set your \$SGE_ROOT environment variable
   echo and start this script again. Exit.
   echo 
   exit 1
fi

if [ -z "$SGE_CELL" -o ! -d "$SGE_ROOT/$SGE_CELL" ]; then
   echo 
   echo ERROR: Please set your \$SGE_CELL environment variable
   echo and start this script again. Exit.
   echo 
   exit 1
fi

if [ ! -f "$SGE_ROOT/util/arch" ]; then
   echo 
   echo ERROR: The shell script \"$SGE_ROOT/util/arch\" does not exist.
   echo Please verify your distribution and restart this script. Exit.
   echo
   exit 1
fi

SGE_ARCH=`$SGE_ROOT/util/arch`

if [ ! -f $SGE_ROOT/util/arch_variables ]; then
   echo
   echo ERROR: Missing shell script \"$SGE_ROOT/util/arch_variables\".
   echo Please verify your distribution and restart this script. Exit.
   echo
   exit 1
fi

. $SGE_ROOT/util/arch_variables


if [ $# -lt 2 ]; then
   echo
   echo Create a set of Windows CSP certificates from Cluster Scheduler 
   echo certificates installed in \$SGE_ROOT
   echo 
   echo "usage: $0 <sge_root> <sge_cell>"
   echo 
   echo example: $0 \$SGE_ROOT \$SGE_CELL
   echo
   exit 1
fi

. $SGE_ROOT/$SGE_CELL/common/settings.sh

is_su="false"
is_csp=`cat $SGE_ROOT/$SGE_CELL/common/bootstrap | grep security_mode | awk '{ print $2 }'`

if [ "$is_csp" != "csp" ]; then
   echo CSP mode, no need to copy certificates
else
   ADMINUSER=`cat $SGE_ROOT/$SGE_CELL/common/bootstrap | grep admin_user | awk '{ print $2 }'`

    UID=`id | cut -d"(" -f1 | cut -d"=" -f2`
    if [ $UID = "0" ]; then
       is_su="true"
       UNIX_SU_NAME="root"
    fi

   if [ "$is_su" = "true" ]; then
      if [ "$SGE_QMASTER_PORT" = "" ]; then
         CA_DIR="/var/sgeCA/sge_qmaster"
         USERKEY_DIR="$CA_DIR/$SGE_CELL/userkeys"
      else
         CA_DIR="/var/sgeCA/port$SGE_QMASTER_PORT"
         USERKEY_DIR="$CA_DIR/$SGE_CELL/userkeys"
      fi

      if [ ! -d $CA_DIR ]; then
         echo "Certificates directory could not be found, please copy certs first!"
         exit 1
      fi
   else
      echo You must be superuser for this operation!
   fi
fi
