#!/bin/sh
#
# SGE configuration script (Installation/Uninstallation/Upgrade/Downgrade)
# Scriptname: inst_qmaster.sh
# Module: qmaster installation functions
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
#  Portions of this code are Copyright 2011 Univa Inc.
#
#  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
#
##########################################################################
#___INFO__MARK_END__

#set -x


#-------------------------------------------------------------------------
# GetCellRoot
#
GetCellRoot()
{
   if [ $AUTO = true ]; then
      SGE_CELL_ROOT=$SGE_CELL_ROOT
       $INFOTEXT -log "Using >%s< as SGE_CELL_ROOT." "$SGE_CELL_ROOT"
   else
      $CLEAR
      $INFOTEXT -u "\nCluster Scheduler cell root"
      $INFOTEXT -n "Enter the cell root <<<"
      INP=`Enter `
      eval SGE_CELL_ROOT=$INP
      $INFOTEXT -wait -auto $AUTO -n "\nUsing cell root >%s<. Hit <RETURN> to continue >> " $SGE_CELL_ROOT
      $CLEAR
   fi
}

#-------------------------------------------------------------------------
# GetCell
#
GetCell()
{
   is_done="false"
   Overwrite="false"

   if [ $AUTO = true ]; then
    SGE_CELL=$CELL_NAME
    SGE_CELL_VAL=$CELL_NAME
    $INFOTEXT -log "Using >%s< as CELL_NAME." "$CELL_NAME"

    if [ -f $SGE_ROOT/$SGE_CELL/common/bootstrap -a "$QMASTER" = "install" ]; then
       $INFOTEXT -log "The cell name you have used and the bootstrap already exists!"
       $INFOTEXT -log "It seems that you have already a installed system."
       $INFOTEXT -log "A installation may cause, that data can be lost!"
       $INFOTEXT -log "Please, check this directory and remove it, or use any other cell name"
       $INFOTEXT -log "Exiting installation now!"
       MoveLog
       exit 1
    fi 

   else
   while [ $is_done = "false" ]; do 
      $CLEAR
      $INFOTEXT -u "\nCluster Scheduler cells"
      if [ "$SGE_CELL" = "" ]; then
         SGE_CELL=default
      fi
      $INFOTEXT -n "\nCluster Scheduler supports multiple cells.\n\n" \
                   "If you are not planning to run multiple Cluster Scheduler clusters or if you don't\n" \
                   "know yet what is a Cluster Scheduler cell it is safe to keep the default cell name\n\n" \
                   "   default\n\n" \
                   "If you want to install multiple cells you can enter a cell name now.\n\n" \
                   "The environment variable\n\n" \
                   "   \$SGE_CELL=<your_cell_name>\n\n" \
                   "will be set for all further Cluster Scheduler commands.\n\n" \
                   "Enter cell name [%s] >> " $SGE_CELL
      INP=`Enter $SGE_CELL`
      eval SGE_CELL=$INP
      SGE_CELL_VAL=`eval echo $SGE_CELL`
      if [ "$QMASTER" = "install" ]; then
         if [ -d $SGE_ROOT/$SGE_CELL/common ]; then
            $CLEAR
            $INFOTEXT "\nThe \"common\" directory in cell >%s< already exists!" $SGE_CELL_VAL
            $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n "Do you want to select another cell name? (y/n) [y] >> "
            if [ $? = 0 ]; then
               is_done="false"
            else
               with_bdb=0
               if [ ! -f $SGE_ROOT/$SGE_CELL/common/bootstrap -a -f $SGE_ROOT/$SGE_CELL/common/sgebdb ]; then
                  $INFOTEXT -n "Do you want to keep this directory? Choose\n" \
                               "(YES option) - if you have installed BDB server.\n" \
                               "(NO option)  - to delete the whole directory!\n"
                  $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n "Do you want to keep [y] or delete [n] the directory? (y/n) [y] >> "
                  with_bdb=1
               else
                  $INFOTEXT -n "You can overwrite or delete this directory. If you choose overwrite\n" \
                               "(YES option) only the \"bootstrap\" and \"cluster_name\" files will be deleted).\n" \
                               "Delete (NO option) - will delete the whole directory!\n"
                  $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n "Do you want to overwrite [y] or delete [n] the directory? (y/n) [y] >> "
               fi
               sel_ret=$?
               SearchForExistingInstallations "qmaster shadowd execd dbwriter"
               if [ $sel_ret = 0 -a $with_bdb = 0 ]; then
                  $INFOTEXT "Deleting bootstrap and cluster_name files!"
                  ExecuteAsAdmin rm -f $SGE_ROOT/$SGE_CELL_VAL/common/bootstrap
                  ExecuteAsAdmin rm -f $SGE_ROOT/$SGE_CELL_VAL/common/cluster_name
               elif [ $sel_ret -ne 0 ]; then
                  $INFOTEXT "Deleting directory \"%s\" now!" $SGE_ROOT/$SGE_CELL_VAL
                  Removedir $SGE_ROOT/$SGE_CELL_VAL
               fi
               if [ $sel_ret = 0 ]; then
                  Overwrite="true"
               fi
               is_done="true"
            fi
         else
            is_done="true"
         fi
      elif [ "$DBWRITER" = "install" ]; then
         SearchForExistingInstallations "dbwriter"
         is_done="true"
      else
         is_done="true"
      fi
   done

   $INFOTEXT -wait -auto $AUTO -n "\nUsing cell >%s<. \nHit <RETURN> to continue >> " $SGE_CELL_VAL
   $CLEAR
   fi
   export SGE_CELL

  HOST=`$SGE_UTILBIN/gethostname -aname`
  if [ "$HOST" = "" ]; then
     $INFOTEXT -e "can't get hostname of this machine. Installation failed."
     exit 1
  fi
}


#-------------------------------------------------------------------------
# GetQmasterSpoolDir()
#
GetQmasterSpoolDir()
{
   if [ $AUTO = true ]; then
      QMDIR="$QMASTER_SPOOL_DIR"
      $INFOTEXT -log "Using >%s< as QMASTER_SPOOL_DIR." "$QMDIR"
      #If directory exists and has files, we exit the auto installation
      if [ -d "$QMDIR" ]; then
         if [ `ls -1 "$QMDIR" | wc -l` -gt 0 ]; then
            $INFOTEXT -log "Specified qmaster spool directory \"$QMDIR\" is not empty!"
            $INFOTEXT -log "Maybe different system is using it. Check this directory"
            $INFOTEXT -log "and remove it, or use any other qmaster spool directory."
            $INFOTEXT -log "Exiting installation now!"
            MoveLog
            exit 1
         fi
      fi
   else
   euid=$1

   done=false
   while [ $done = false ]; do
      $CLEAR
      $INFOTEXT -u "\nCluster Scheduler qmaster spool directory"
      $INFOTEXT "\nThe qmaster spool directory is the place where the qmaster daemon stores\n" \
                "the configuration and the state of the queuing system.\n\n"

      if [ $euid = 0 ]; then
         if [ $ADMINUSER = default ]; then
            $INFOTEXT "User >root< on this host must have read/write access to the qmaster\n" \
                      "spool directory.\n"
         else
            $INFOTEXT "The admin user >%s< must have read/write access\n" \
                      "to the qmaster spool directory.\n" $ADMINUSER
         fi
      else
         $INFOTEXT "Your account on this host must have read/write access\n" \
                   "to the qmaster spool directory.\n"
      fi

      $INFOTEXT -n "If you will install shadow master hosts or if you want to be able to start\n" \
                   "the qmaster daemon on other hosts (see the corresponding section in the\n" \
                   "Cluster Scheduler Installation and Administration Manual for details) the account\n" \
                   "on the shadow master hosts also needs read/write access to this directory.\n\n" \
                   "Enter a qmaster spool directory [%s] >> " "$SGE_ROOT_VAL/$SGE_CELL_VAL/spool/qmaster"
      QMDIR=`Enter "$SGE_ROOT_VAL/$SGE_CELL_VAL/spool/qmaster"`
      done=true
      #If directory is not empty need now require a new one
      if [ -d "$QMDIR" ]; then
         if [ `ls -1 "$QMDIR" | wc -l` -gt 0 ]; then
            $INFOTEXT "Specified qmaster spool directory is not empty!"
            $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n \
               "Do you want to select another qmaster spool directory (y/n) [y] >> "
            if [ $? = 0 ]; then
               done=false
            fi
         fi
      fi
   done
   fi
   $INFOTEXT -wait -auto $AUTO -n "\nUsing qmaster spool directory >%s<. \nHit <RETURN> to continue >> " "$QMDIR"
   export QMDIR
}



#--------------------------------------------------------------------------
# SetPermissions
#    - set permission for regular files to 644
#    - set permission for executables and directories to 755
#
SetPermissions()
{
   $CLEAR
   $INFOTEXT -u "\nVerifying and setting file permissions"
   $ECHO

   euid=`$SGE_UTILBIN/uidgid -euid`

   if [ $euid != 0 ]; then
      $INFOTEXT "You are not installing as user >root<\n"
      $INFOTEXT "Can't set the file owner/group and permissions\n"
      $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
      $CLEAR
      return 0
   else
      if [ "$AUTO" = "true" -a "$SET_FILE_PERMS" = "true" ]; then
         :
      else
         $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n \
                   "Did you install this version with >pkgadd< or did you already verify\n" \
                   "and set the file permissions of your distribution (enter: y) (y/n) [y] >> "
         if [ $? = 0 ]; then
            $INFOTEXT -wait -auto $AUTO -n "We do not verify file permissions. Hit <RETURN> to continue >> "
            $CLEAR
            return 0
         fi
      fi
   fi

   rm -f ./tst$$ 2> /dev/null > /dev/null
   touch ./tst$$ 2> /dev/null > /dev/null
   ret=$?
   rm -f ./tst$$ 2> /dev/null > /dev/null
   if [ $ret != 0 ]; then
      $INFOTEXT -u "\nVerifying and setting file permissions (continued)"

      $INFOTEXT "\nWe can't set file permissions on this machine, because user root\n" \
                  "has not the necessary privileges to change file permissions\n" \
                  "on this file system.\n\n" \
                  "Probably this file system is an NFS mount where user root is\n" \
                  "mapped to user >nobody<.\n\n" \
                  "Please login now at your file server and set the file permissions and\n" \
                  "ownership of the entire distribution with the command:\n\n" \
                  "   # \$SGE_ROOT/util/setfileperm.sh \$SGE_ROOT\n\n" 

      $INFOTEXT -wait -auto $AUTO -n "Please hit <RETURN> to continue once you set your file permissions >> "
      $CLEAR
      return 0
   else
      $CLEAR
      $INFOTEXT -u "\nVerifying and setting file permissions"
      $INFOTEXT "\nWe may now verify and set the file permissions of your Cluster Scheduler\n" \
                "distribution.\n\n" \
                 "This may be useful since due to unpacking and copying of your distribution\n" \
                 "your files may be unaccessible to other users.\n\n" \
                 "We will set the permissions of directories and binaries to\n\n" \
                 "   755 - that means executable are accessible for the world\n\n" \
                 "and for ordinary files to\n\n" \
                 "   644 - that means readable for the world\n\n"

      $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n \
               "Do you want to verify and set your file permissions (y/n) [y] >> "
      ret=$?
   fi

   if [ $ret = 0 ]; then

      if [ $RESPORT = true ]; then
         resportarg="-resport"
      else
         resportarg="-noresport"
      fi

      if [ $ADMINUSER = default ]; then
         fileowner=root
      else
         fileowner=$ADMINUSER
      fi

      filegid=`$SGE_UTILBIN/uidgid -gid`

      $CLEAR

      util/setfileperm.sh -auto $SGE_ROOT

      $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
   else
      $INFOTEXT -wait -auto $AUTO -n "We will not verify your file permissions. Hit <RETURN> to continue >>"
   fi
   $CLEAR
}


#SetSpoolingOptionsBerkeleyDB()
# $1 - new default spool_dir
SetSpoolingOptionsBerkeleyDB()
{
   SPOOLING_METHOD=berkeleydb
   SPOOLING_LIB=libspoolb
   SPOOLING_DIR="spooldb"
   MKDIR="mkdir -p"
   params_ok=0
   if [ "$AUTO" = "true" ]; then
      SPOOLING_DIR="$DB_SPOOLING_DIR"

      if [ "$SPOOLING_DIR" = "" ]; then
         $INFOTEXT -log "Please enter a Berkeley DB spooling directory!"
         MoveLog
         exit 1
      fi

      if [ -d "$SPOOLING_DIR" ]; then
         $INFOTEXT -log "The spooling directory [%s] already exists! Exiting installation!" "$SPOOLING_DIR"
         MoveLog
         exit 1 
      fi
      SpoolingCheckParams
      params_ok=$?
   fi
   if [ "$QMASTER" = "install" ]; then

      is_server="false"

      done="false"
      is_spool="false"

      while [ $is_spool = "false" ] && [ $done = "false" ]; do
         $CLEAR
         SpoolingQueryChange
         if [ -d $SPOOLING_DIR ]; then
            $INFOTEXT -n -ask "y" "n" -def "n" -auto $AUTO "The spooling directory already exists! Do you want to delete it? [n] >> "
            ret=$?               
            if [ $AUTO = true ]; then
               $INFOTEXT -log "The spooling directory already exists!\n Please remove it or choose any other spooling directory!"
               MoveLog
               exit 1
            fi
 
            if [ $ret = 0 ]; then
               Removedir $SPOOLING_DIR
               if [ -d $SPOOLING_DIR ]; then
                  $INFOTEXT "You are not the owner of this directory. You can't delete it!"
               else
                  is_spool="true"
               fi
            else
               $INFOTEXT -wait "Please hit <ENTER>, to choose any other spooling directory!"
            fi
          else
             is_spool="true"
          fi

          CheckLocalFilesystem $SPOOLING_DIR
          ret=$?
          if [ $ret -eq 0 -a "$AUTO" = "false" ]; then
             $INFOTEXT "\nThe database directory\n\n" \
                       "   %s\n\n" \
                       "is not on a local filesystem. Please choose a local filesystem or\n" \
                       "configure the RPC Client/Server mechanism." $SPOOLING_DIR
             $INFOTEXT -wait -auto $AUTO -n "\nHit <RETURN> to continue >> "
          else
             done="true" 
          fi
            
          if [ $is_spool = "false" ]; then
             done="false"
          elif [ $done = "false" ]; then
             is_spool="false"
          fi

      done

   fi

   $ECHO
   Makedir $SPOOLING_DIR
   SPOOLING_ARGS="$SPOOLING_DIR"
}

SetSpoolingOptionsClassic()
{
   SPOOLING_METHOD=classic
   SPOOLING_LIB=libspoolc
   SPOOLING_ARGS="$SGE_ROOT_VAL/$COMMONDIR;$QMDIR"
}

# $1 - spooling method
# $2 - suggested spooling params from the backup
SetSpoolingOptionsDynamic()
{
   suggested_method=$1
   if [ -z "$suggested_method" ]; then
      suggested_method=classic
   fi
   if [ "$AUTO" = "true" ]; then
      if [ "$SPOOLING_METHOD" != "berkeleydb" -a "$SPOOLING_METHOD" != "classic" ]; then
         SPOOLING_METHOD="classic"
      fi
   else
      $INFOTEXT -n "Your SGE binaries are compiled to link the spooling libraries\n" \
                   "during runtime (dynamically). So you can choose between Berkeley DB \n" \
                   "spooling and Classic spooling method."
      $INFOTEXT -n "\nPlease choose a spooling method (berkeleydb|classic) [%s] >> " "$suggested_method"
      SPOOLING_METHOD=`Enter $suggested_method`
   fi

   $CLEAR

   case $SPOOLING_METHOD in 
      classic)
         SetSpoolingOptionsClassic
         ;;
      berkeleydb)
         SetSpoolingOptionsBerkeleyDB $2
         ;;
      *)
         $INFOTEXT "\nUnknown spooling method. Exit."
         $INFOTEXT -log "\nUnknown spooling method. Exit."
         MoveLog
         exit 1
         ;;
   esac
}

#--------------------------------------------------------------------------
# SetSpoolingOptions sets / queries options for the spooling framework
# $1 - suggested spooling params from the old bootstrap file
SetSpoolingOptions()
{
   $INFOTEXT -u "\nSetup spooling"
   COMPILED_IN_METHOD=`ExecuteAsAdmin $SPOOLINIT method`
   $INFOTEXT -log "Setting spooling method to %s" $COMPILED_IN_METHOD
   case "$COMPILED_IN_METHOD" in 
      classic)
         SetSpoolingOptionsClassic
         ;;
      berkeleydb)
         SetSpoolingOptionsBerkeleyDB $1
         ;;
      dynamic)
         SetSpoolingOptionsDynamic $1
         ;;
      *)
         $INFOTEXT "\nUnknown spooling method. Exit."
         $INFOTEXT -log "\nUnknown spooling method. Exit."
         MoveLog
         exit 1
         ;;
   esac
}


#-------------------------------------------------------------------------
# Ask the installer for the hostname resolving method
# (IGNORE_FQND=true/false)
#
SelectHostNameResolving()
{
   if [ $AUTO = "true" ]; then
     IGNORE_FQDN_DEFAULT=$HOSTNAME_RESOLVING
     $INFOTEXT -log "Using >%s< as IGNORE_FQDN_DEFAULT." "$IGNORE_FQDN_DEFAULT"
     $INFOTEXT -log "If it's >true<, the domain name will be ignored."
     
   else
     $CLEAR
     $INFOTEXT -u "\nSelect default Cluster Scheduler hostname resolving method"
     $INFOTEXT "\nAre all hosts of your cluster in one DNS domain? If this is\n" \
               "the case the hostnames\n\n" \
               "   >hostA< and >hostA.foo.com<\n\n" \
               "would be treated as equal, because the DNS domain name >foo.com<\n" \
               "is ignored when comparing hostnames.\n\n"

     $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n \
               "Are all hosts of your cluster in a single DNS domain (y/n) [y] >> "
     if [ $? = 0 ]; then
        IGNORE_FQDN_DEFAULT=true
        $INFOTEXT "Ignoring domain name when comparing hostnames."
     else
        IGNORE_FQDN_DEFAULT=false
        $INFOTEXT "The domain name is not ignored when comparing hostnames."
     fi
     $INFOTEXT -wait -auto $AUTO -n "\nHit <RETURN> to continue >> "
     $CLEAR
   fi

   if [ "$IGNORE_FQDN_DEFAULT" = "false" ]; then
      GetDefaultDomain
   else
      CFG_DEFAULT_DOMAIN=none
   fi
}


#-------------------------------------------------------------------------
# SetProductMode
#
SetProductMode()
{
   if [ "$AFS" = "true" ]; then
      AFS_PREFIX="afs"
   else
      AFS_PREFIX=""
   fi

   if [ "$CSP" = "true" ]; then
      SEC_COUNT=`strings "$SGE_BIN/sge_qmaster" | grep "AIMK_SECURE_OPTION_ENABLED" | wc -l`
      if [ "$SEC_COUNT" -ne 1 ]; then
         $INFOTEXT "\n>sge_qmaster< binary is not compiled with >-secure< option!\n"
         $INFOTEXT -wait -auto "$AUTO" -n "Hit <RETURN> to cancel the installation >> "
         exit 1
      else
         CSP_PREFIX="csp"
      fi
   else
      CSP_PREFIX=""
   fi

   if [ "$MUNGE" = "true" ]; then
      SEC_COUNT=`strings "$SGE_BIN/sge_qmaster" | grep "EMUNGE_SUCCESS" | wc -l`
      if [ "$SEC_COUNT" -ne 1 ]; then
         $INFOTEXT "\n>sge_qmaster< binary is not compiled with >-DWITH_MUNGE=ON< option!\n"
         $INFOTEXT -wait -auto "$AUTO" -n "Hit <RETURN> to cancel the installation >> "
         exit 1
      else
         MUNGE_PREFIX="munge"
      fi
   else
      MUNGE_PREFIX=""
   fi

   if [ "$TLS" = "true" ]; then
      SEC_COUNT=`strings "$SGE_BIN/sge_qmaster" | grep "OpenSSL" | wc -l`
      if [ "$SEC_COUNT" -lt 1 ]; then
         $INFOTEXT "\n>sge_qmaster< binary is not compiled with >-DWITH_OPENSSL=ON< option!\n"
         $INFOTEXT -wait -auto "$AUTO" -n "Hit <RETURN> to cancel the installation >> "
         exit 1
      else
         TLS_PREFIX="tls"
      fi
   else
      TLS_PREFIX=""
   fi

   # @todo CS-1523 allow multiple security options, at least munge and tls
   if [ "$AFS" = "true" ]; then
      if [ "$CSP" = "true" -o "$MUNGE" = "true" -o "$TLS" = "true" ]; then
         $INFOTEXT "\nAFS security can't be combined with other security options!\n"
         $INFOTEXT -wait -auto "$AUTO" -n "Hit <RETURN> to cancel the installation >> "
         exit 1
      else
         PRODUCT_MODE="${AFS_PREFIX}"
      fi
   else
      if [ "$CSP" = "true" ]; then
         if [ "$AFS" = "true" -o "$MUNGE" = "true" -o "$TLS" = "true" ]; then
            $INFOTEXT "\nCSP security can't be combined with other security options!\n"
            $INFOTEXT -wait -auto "$AUTO" -n "Hit <RETURN> to cancel the installation >> "
            exit 1
         else
            PRODUCT_MODE="${CSP_PREFIX}"
         fi
      else
         if [ "$TLS" = "true" -a "$MUNGE" = "true" ]; then
            PRODUCT_MODE="${MUNGE_PREFIX},${TLS_PREFIX}"
         elif [ "$TLS" = "true" ]; then
            PRODUCT_MODE="${TLS_PREFIX}"
         elif [ "$MUNGE" = "true" ]; then
            PRODUCT_MODE="${MUNGE_PREFIX}"
         else
            PRODUCT_MODE="none"
         fi
      fi
   fi
}


#-------------------------------------------------------------------------
# Make directories needed by qmaster
#
MakeDirsMaster()
{
   $INFOTEXT -u "\nMaking directories"
   $ECHO
   $INFOTEXT -log "Making directories"
   Makedir $SGE_CELL_VAL
   Makedir $COMMONDIR
   Makedir $QMDIR
   Makedir $QMDIR/job_scripts

   $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
   $CLEAR
}


#-------------------------------------------------------------------------
# Adding Bootstrap information
#
AddBootstrap()
{
   TOUCH=touch
   $INFOTEXT "Dumping bootstrapping information"
   $INFOTEXT -log "Dumping bootstrapping information"
   ExecuteAsAdmin rm -f $COMMONDIR/bootstrap
   ExecuteAsAdmin $TOUCH $COMMONDIR/bootstrap
   ExecuteAsAdmin chmod 666 $COMMONDIR/bootstrap
   PrintBootstrap >> $COMMONDIR/bootstrap
   ExecuteAsAdmin chmod 444 $COMMONDIR/bootstrap
}

#-------------------------------------------------------------------------
# PrintBootstrap: print SGE default configuration
#
PrintBootstrap()
{
   $ECHO "# Version: $SGE_VERSION"
   $ECHO "#"
   if [ $ADMINUSER != default ]; then
      $ECHO "admin_user              $ADMINUSER"
   else
      $ECHO "admin_user              none"
   fi
   $ECHO "default_domain          $CFG_DEFAULT_DOMAIN"
   $ECHO "ignore_fqdn             $IGNORE_FQDN_DEFAULT"
   $ECHO "spooling_method         $SPOOLING_METHOD"
   $ECHO "spooling_lib            $SPOOLING_LIB"
   $ECHO "spooling_params         $SPOOLING_ARGS"
   $ECHO "binary_path             $SGE_ROOT_VAL/bin"
   $ECHO "qmaster_spool_dir       $QMDIR"
   $ECHO "security_mode           $PRODUCT_MODE"
   $ECHO "listener_threads        4"
   $ECHO "worker_threads          4"
   $ECHO "reader_threads          4"
   $ECHO "scheduler_threads       1"
}


#-------------------------------------------------------------------------
# Initialize the spooling database (or directory structure)
#
InitSpoolingDatabase()
{
   $INFOTEXT "Initializing spooling database"
   $INFOTEXT -log "Initializing spooling database"
   ExecuteAsAdmin $SPOOLINIT $SPOOLING_METHOD $SPOOLING_LIB "$SPOOLING_ARGS" init

   $INFOTEXT -wait -auto $AUTO -n "\nHit <RETURN> to continue >> "
   $CLEAR
}


#-------------------------------------------------------------------------
# AddConfiguration
# optional args for GetConfigutration
# $1 - default CFG_EXE_SPOOL
# $2 - default CFG_MAIL_ADDR
AddConfiguration()
{
   useold=false

   if [ $useold = false ]; then
      GetConfiguration "$@"
      #TruncCreateAndMakeWriteable $COMMONDIR/configuration
      #PrintConf >> $COMMONDIR/configuration
      #SetPerm $COMMONDIR/configuration
      TMPC=/tmp/configuration_`date '+%Y-%m-%d_%H:%M:%S'`.$$
      TOUCH=touch
      rm -f $TMPC
      $TOUCH $TMPC
      PrintConf >> $TMPC
      ExecuteAsAdmin $SPOOLDEFAULTS configuration $TMPC
      rm -f $TMPC
   fi
}

#-------------------------------------------------------------------------
# PrintConf: print SGE default configuration
#
PrintConf()
{
   $ECHO "# Version: $SGE_VERSION"
   $ECHO "#"
   $ECHO "# DO NOT MODIFY THIS FILE MANUALLY!"
   $ECHO "#"
   $ECHO "conf_version           0"
   $ECHO "execd_spool_dir        $CFG_EXE_SPOOL"
   $ECHO "mailer                 $MAILER"
   $ECHO "xterm                  $XTERM"
   $ECHO "load_sensor            none"
   $ECHO "prolog                 none"
   $ECHO "epilog                 none"
   $ECHO "shell_start_mode       unix_behavior"
   $ECHO "login_shells           sh,bash,ksh,csh,tcsh"
   $ECHO "min_uid                0"
   $ECHO "min_gid                0"
   $ECHO "user_lists             none"
   $ECHO "xuser_lists            none"
   $ECHO "projects               none"
   $ECHO "xprojects              none"
   $ECHO "enforce_project        false"
   $ECHO "enforce_user           auto"
   $ECHO "load_report_time       00:00:15"
   $ECHO "max_unheard            00:05:00"
   $ECHO "reschedule_unknown     00:00:00"
   $ECHO "loglevel               log_warning"
   $ECHO "administrator_mail     $CFG_MAIL_ADDR"
   $ECHO "mail_tag               none"
   if [ "$AFS" = true ]; then
      $ECHO "set_token_cmd          /path_to_token_cmd/set_token_cmd"
      $ECHO "pag_cmd                /usr/afsws/bin/pagsh"
      $ECHO "token_extend_time      24:0:0"
   else
      $ECHO "set_token_cmd          none"
      $ECHO "pag_cmd                none"
      $ECHO "token_extend_time      none"
   fi
   $ECHO "shepherd_cmd           none"
   $ECHO "qmaster_params         none"
   $ECHO "execd_params           none"
   $ECHO "reporting_params       accounting=true reporting=false flush_time=00:00:1 joblog=false sharelog=00:00:00"
   $ECHO "finished_jobs          100"
   $ECHO "gid_range              $CFG_GID_RANGE"
   $ECHO "qlogin_command         $QLOGIN_COMMAND"
   $ECHO "qlogin_daemon          $QLOGIN_DAEMON"
   if [ "$RLOGIN_COMMAND" != "undef" ]; then
      $ECHO "rlogin_command         $RLOGIN_COMMAND"
   fi
   $ECHO "rlogin_daemon          $RLOGIN_DAEMON"
   if [ "$RSH_COMMAND" != "undef" ]; then
      $ECHO "rsh_command            $RSH_COMMAND"
   fi
   if [ "$RSH_DAEMON" != "undef" ]; then
      $ECHO "rsh_daemon             $RSH_DAEMON"
   fi

   $ECHO "max_aj_instances       2000"
   $ECHO "max_aj_tasks           75000"
   $ECHO "max_u_jobs             0"
   $ECHO "max_jobs               0"
   $ECHO "max_advance_reservations 0"
   $ECHO "auto_user_oticket      0"
   $ECHO "auto_user_fshare       0"
   $ECHO "auto_user_default_project none"
   $ECHO "auto_user_delete_time  86400"
   $ECHO "delegated_file_staging false"
   $ECHO "jsv_url                none"
   $ECHO "gdi_request_limits     none"
}


#-------------------------------------------------------------------------
# AddLocalConfiguration
#
AddLocalConfiguration()
{
   $CLEAR
   $INFOTEXT -u "\nCreating local configuration"

      ExecuteAsAdmin mkdir /tmp/$$
      TMPH=/tmp/$$/$HOST
      ExecuteAsAdmin rm -f $TMPH
      ExecuteAsAdmin touch $TMPH
      PrintLocalConf 1 >> $TMPH
      ExecuteAsAdmin $SPOOLDEFAULTS local_conf $TMPH $HOST
      ExecuteAsAdmin rm -rf /tmp/$$
}


#-------------------------------------------------------------------------
# GetConfiguration: get some parameters for global configuration
# args are optional
# $1 - default CFG_EXE_SPOOL
# $2 - default CFG_MAIL_ADDR
GetConfiguration()
{

   GetGidRange

   #if [ $fast = true ]; then
   #   CFG_EXE_SPOOL=$SGE_ROOT_VAL/$SGE_CELL_VAL/spool
   #   CFG_MAIL_ADDR=none
   #   return 0
   #fi
   if [ $AUTO = true ]; then
     CFG_EXE_SPOOL=$EXECD_SPOOL_DIR
     CFG_MAIL_ADDR=$ADMIN_MAIL
     $INFOTEXT -log "Using >%s< as EXECD_SPOOL_DIR." "$CFG_EXE_SPOOL"
     $INFOTEXT -log "Using >%s< as ADMIN_MAIL." "$ADMIN_MAIL"
   else
   done=false
   while [ $done = false ]; do
      $CLEAR
      $INFOTEXT -u "\nCluster Scheduler cluster configuration"
      $INFOTEXT "\nPlease give the basic configuration parameters of your Cluster Scheduler\n" \
                "installation:\n\n   <execd_spool_dir>\n\n"

      if [ $ADMINUSER != default ]; then
            $INFOTEXT "The pathname of the spool directory of the execution hosts. User >%s<\n" \
                      "must have the right to create this directory and to write into it.\n" "$ADMINUSER"
      elif [ $euid = 0 ]; then
            $INFOTEXT "The pathname of the spool directory of the execution hosts. User >root<\n" \
                      "must have the right to create this directory and to write into it.\n"
      else
            $INFOTEXT "The pathname of the spool directory of the execution hosts. You\n" \
                      "must have the right to create this directory and to write into it.\n"
      fi
      
      if [ -z "$1" ]; then
         default_value=$SGE_ROOT_VAL/$SGE_CELL_VAL/spool
      else
         default_value="$1"
      fi

      $INFOTEXT -n "Default: [%s] >> " $default_value

      CFG_EXE_SPOOL=`Enter $default_value`

      $CLEAR
      if [ -z "$2" ]; then
         default_value=none
      else
         default_value="$2"
      fi
      $INFOTEXT -u "\nCluster Scheduler cluster configuration (continued)"
      $INFOTEXT -n "\n<administrator_mail>\n\n" \
                   "The email address of the administrator to whom problem reports are sent.\n\n" \
                   "It is recommended to configure this parameter. You may use >none<\n" \
                   "if you do not wish to receive administrator mail.\n\n" \
                   "Please enter an email address in the form >user@foo.com<.\n\n" \
                   "Default: [%s] >> " $default_value

      CFG_MAIL_ADDR=`Enter $default_value`

      $CLEAR
      $INFOTEXT "\nThe following parameters for the cluster configuration were configured:\n\n" \
                "   execd_spool_dir        %s\n" \
                "   administrator_mail     %s\n" $CFG_EXE_SPOOL $CFG_MAIL_ADDR

      $INFOTEXT -auto $AUTO -ask "y" "n" -def "n" -n \
                "Do you want to change the configuration parameters (y/n) [n] >> "
      if [ $? = 1 ]; then
         done=true
      fi
   done
   fi
   export CFG_EXE_SPOOL
}


#-------------------------------------------------------------------------
# GetGidRange
#
GetGidRange()
{
   done=false
   if [ -z "$GID_RANGE" ]; then
        GID_RANGE=20000-20100
   fi
   
   while [ $done = false ]; do
      $CLEAR
      $INFOTEXT -u "\nCluster Scheduler group id range"
      $INFOTEXT "\nWhen jobs are started under the control of Cluster Scheduler an additional group id\n" \
                "is set on platforms which do not support jobs. This is done to provide maximum\n" \
                "control for Cluster Scheduler jobs.\n\n" \
                "This additional UNIX group id range must be unused group id's in your system.\n" \
                "Each job will be assigned a unique id during the time it is running.\n" \
                "Therefore you need to provide a range of id's which will be assigned\n" \
                "dynamically for jobs.\n\n" \
                "The range must be big enough to provide enough numbers for the maximum number\n" \
                "of Cluster Scheduler jobs running at a single moment on a single host. E.g. a range\n" \
                "like >20000-20100< means, that Cluster Scheduler will use the group ids from\n" \
                "20000-20100 and provides a range for 100 Cluster Scheduler jobs at the same time\n" \
                "on a single host.\n\n" \
                "You can change at any time the group id range in your cluster configuration.\n"

      $INFOTEXT -n "Please enter a range [%s] >> " $GID_RANGE

      CFG_GID_RANGE=`Enter $GID_RANGE`

      if [ "$CFG_GID_RANGE" != "" ]; then
         $INFOTEXT -wait -auto $AUTO -n "\nUsing >%s< as gid range. Hit <RETURN> to continue >> " \
                   "$CFG_GID_RANGE"
         $CLEAR
         done=true
      fi
     if [ $AUTO = true -a "$GID_RANGE" = "" ]; then
        $INFOTEXT -log "Please enter a valid GID Range. Installation failed!"
        MoveLog
        exit 1
     fi
 
     $INFOTEXT -log "Using >%s< as gid range." "$CFG_GID_RANGE"  
   done
}


#-------------------------------------------------------------------------
# AddActQmaster: create act_qmaster file
#
AddActQmaster()
{
   $INFOTEXT "Creating >act_qmaster< file"

   TruncCreateAndMakeWriteable $COMMONDIR/act_qmaster
   $ECHO $HOST >> $COMMONDIR/act_qmaster
   SetPerm $COMMONDIR/act_qmaster
}


#-------------------------------------------------------------------------
# AddDefaultComplexes
#
AddDefaultComplexes()
{
   $INFOTEXT "Adding default complex attributes"
   ExecuteAsAdmin $SPOOLDEFAULTS complexes $SGE_ROOT_VAL/util/resources/centry

}


#-------------------------------------------------------------------------
# AddCommonFiles
#    Copy files from util directory to common dir
#
AddCommonFiles()
{
   for f in sge_aliases qtask sge_request; do
      if [ $f = sge_aliases ]; then
         $INFOTEXT "Adding >%s< path aliases file" $f
      elif [ $f = qtask ]; then
         $INFOTEXT "Adding >%s< qtcsh sample default request file" $f
      else
         $INFOTEXT "Adding >%s< default submit options file" $f
      fi
      ExecuteAsAdmin cp util/$f $COMMONDIR
      ExecuteAsAdmin chmod $FILEPERM $COMMONDIR/$f
   done

}

#-------------------------------------------------------------------------
# AddPEFiles
#    Copy files from PE template directory to qmaster spool dir
#
AddPEFiles()
{
   $INFOTEXT "Adding default parallel environments (PE)"
   $INFOTEXT -log "Adding default parallel environments (PE)"
   ExecuteAsAdmin $SPOOLDEFAULTS pes $SGE_ROOT_VAL/util/resources/pe
}


#-------------------------------------------------------------------------
# AddDefaultUsersets
#
AddDefaultUsersets()
{
      $INFOTEXT "Adding SGE default usersets"
      ExecuteAsAdmin $SPOOLDEFAULTS usersets $SGE_ROOT_VAL/util/resources/usersets
}


#-------------------------------------------------------------------------
# CreateSettingsFile: Create resource files for csh/sh
#
CreateSettingsFile()
{
   $INFOTEXT "Creating settings files for >.profile/.cshrc<"

   if [ $RECREATE_SETTINGS = "false" ]; then
      if [ -f $SGE_ROOT/$SGE_CELL/common/settings.sh ]; then
         ExecuteAsAdmin $RM $SGE_ROOT/$SGE_CELL/common/settings.sh
      fi
  
      if [ -f $SGE_ROOT/$SGE_CELL/common/settings.csh ]; then
         ExecuteAsAdmin $RM $SGE_ROOT/$SGE_CELL/common/settings.csh
      fi
   fi

   if [ "$execd_service" = "true" ]; then
      SGE_EXECD_PORT=""
      export SGE_EXECD_PORT
   fi

   if [ "$qmaster_service" = "true" ]; then
      SGE_QMASTER_PORT=""
      export SGE_QMASTER_PORT
   fi

   ExecuteAsAdmin util/create_settings.sh $SGE_ROOT_VAL/$COMMONDIR

   SetPerm $SGE_ROOT_VAL/$COMMONDIR/settings.sh
   SetPerm $SGE_ROOT_VAL/$COMMONDIR/settings.csh

   $INFOTEXT -wait -auto $AUTO -n "\nHit <RETURN> to continue >> "
}

#--------------------------------------------------------------------------
# InitCA Create CA and initialize it for daemons and users
#
InitCA()
{

   if [ "$CSP" = true ]; then
      # Initialize CA, make directories and get DN info
      #
      SGE_CA_CMD=util/sgeCA/sge_ca
      CATOP_TMP=`grep "CATOP=" util/sgeCA/sge_ca.cnf | awk -F= '{print $2}' 2>/dev/null`
      eval CATOP_TMP=$CATOP_TMP
      if [ "$AUTO" = "true" ]; then
         if [ "$CSP_RECREATE" != "false" -o ! -f $CATOP_TMP/certs/cert.pem ]; then
            $SGE_CA_CMD -init -days 365 -auto $FILE
            #if [ -f "$CSP_USERFILE" ]; then
            #   $SGE_CA_CMD -usercert $CSP_USERFILE
            #fi
         fi
      else
         $SGE_CA_CMD -init -days 365
      fi

      $INFOTEXT -auto $AUTO -wait -n "Hit <RETURN> to continue >> "
      $CLEAR
   fi
}


#--------------------------------------------------------------------------
# StartQmaster
#
StartQmaster()
{
   $INFOTEXT -u "\nCluster Scheduler qmaster startup"
   $INFOTEXT "\nStarting qmaster daemon. Please wait ..."

   if [ "$SGE_ENABLE_SMF" = "true" ]; then
      $SVCADM enable -s "svc:/application/sge/qmaster:$SGE_CLUSTER_NAME"
      if [ $? -ne 0 ]; then
         $INFOTEXT "\nFailed to start qmaster daemon over SMF. Check service by issuing "\
                   "svcs -l svc:/application/sge/qmaster:%s" $SGE_CLUSTER_NAME
         $INFOTEXT -log "\nFailed to start qmaster daemon over SMF. Check service by issuing "\
                        "svcs -l svc:/application/sge/qmaster:%s" $SGE_CLUSTER_NAME
         MoveLog
         exit 1
      fi
   elif [ $RC_FILE = "systemd" -a `IsSystemdServiceInstalled "qmaster"` = "true" ]; then
      SERVICE_NAME=`GetServiceName "qmaster"`
      systemctl start "$SERVICE_NAME"
      if [ $? -ne 0 ]; then
         $INFOTEXT "sge_qmaster start problem"
         $INFOTEXT -log "sge_qmaster start problem"
         MoveLog
         exit 1
      fi
   else
      $SGE_STARTUP_FILE -qmaster
      if [ $? -ne 0 ]; then
         $INFOTEXT "sge_qmaster start problem"
         $INFOTEXT -log "sge_qmaster start problem"
         MoveLog
         exit 1
      fi
   fi
   # wait till qmaster.pid file is written
   sleep 1
   CheckRunningDaemon sge_qmaster
   run=$?
   if [ $run -ne 0 ]; then
      $INFOTEXT "sge_qmaster daemon didn't start. Please check your\n" \
                "configuration! Installation failed!"
      $INFOTEXT -log "sge_qmaster daemon didn't start. Please check your\n" \
                     "autoinstall configuration file! Installation failed!"

      MoveLog
      exit 1
   fi
   $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
   $CLEAR
}


#-------------------------------------------------------------------------
# AddHosts
#
AddHosts()
{
   if [ "$AUTO" = "true" ]; then
      for h in $ADMIN_HOST_LIST; do
        if [ -f $h ]; then
           $INFOTEXT -log "Adding ADMIN_HOSTS from file %s" $h
           for tmp in `cat $h`; do
             $INFOTEXT -log "Adding ADMIN_HOST %s" $tmp
             $SGE_BIN/qconf -ah $tmp
           done
        else
             $INFOTEXT -log "Adding ADMIN_HOST %s" $h
             $SGE_BIN/qconf -ah $h
        fi
      done
      
      for h in $SUBMIT_HOST_LIST; do
        if [ -f $h ]; then
           $INFOTEXT -log "Adding SUBMIT_HOSTS from file %s" $h
           for tmp in `cat $h`; do
             $INFOTEXT -log "Adding SUBMIT_HOST %s" $tmp
             $SGE_BIN/qconf -as $tmp
           done
        else
             $INFOTEXT -log "Adding SUBMIT_HOST %s" $h
             $SGE_BIN/qconf -as $h
        fi
      done  

      for h in $SHADOW_HOST; do
        if [ -f $h ]; then
           $INFOTEXT -log "Adding SHADOW_HOSTS from file %s" $h
           for tmp in `cat $h`; do
             $INFOTEXT -log "Adding SHADOW_HOST %s" $tmp
             $SGE_BIN/qconf -ah $tmp
           done
        else
             $INFOTEXT -log "Adding SHADOW_HOST %s" $h
             $SGE_BIN/qconf -ah $h
        fi
      done
  
   else
      $INFOTEXT -u "\nAdding Cluster Scheduler hosts"
      $INFOTEXT "\nPlease now add the list of hosts, where you will later install your execution\n" \
                "daemons. These hosts will be also added as valid submit hosts.\n\n" \
                "Please enter a blank separated list of your execution hosts. You may\n" \
                "press <RETURN> if the line is getting too long. Once you are finished\n" \
                "simply press <RETURN> without entering a name.\n\n" \
                "You also may prepare a file with the hostnames of the machines where you plan\n" \
                "to install Cluster Scheduler. This may be convenient if you are installing Grid\n" \
                "Engine on many hosts.\n\n"

      $INFOTEXT -auto $AUTO -ask "y" "n" -def "n" -n \
                "Do you want to use a file which contains the list of hosts (y/n) [n] >> "
      ret=$?
      if [ $ret = 0 ]; then
         AddHostsFromFile execd
         ret=$?
      fi

      if [ $ret = 1 ]; then
         AddHostsFromTerminal execd
      fi

      $INFOTEXT -wait -auto $AUTO -n "Finished adding hosts. Hit <RETURN> to continue >> "
      $CLEAR

      # Adding later shadow hosts to the admin host list
      $INFOTEXT "\nIf you want to use a shadow host, it is recommended to add this host\n" \
                "to the list of administrative hosts.\n\n" \
                "If you are not sure, it is also possible to add or remove hosts after the\n" \
                "installation with <qconf -ah hostname> for adding and <qconf -dh hostname>\n" \
                "for removing this host\n\nAttention: This is not the shadow host installation\n" \
                "procedure.\n You still have to install the shadow host separately\n\n"
      $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n \
                "Do you want to add your shadow host(s) now? (y/n) [y] >> "
      ret=$?
      if [ "$ret" = 0 ]; then
         $CLEAR
         $INFOTEXT -u "\nAdding Cluster Scheduler shadow hosts"
         $INFOTEXT "\nPlease now add the list of hosts, where you will later install your shadow\n" \
                   "daemon.\n\n" \
                   "Please enter a blank separated list of your execution hosts. You may\n" \
                   "press <RETURN> if the line is getting too long. Once you are finished\n" \
                   "simply press <RETURN> without entering a name.\n\n" \
                   "You also may prepare a file with the hostnames of the machines where you plan\n" \
                   "to install Cluster Scheduler. This may be convenient if you are installing Grid\n" \
                   "Engine on many hosts.\n\n"

         $INFOTEXT -auto $AUTO -ask "y" "n" -def "n" -n \
                   "Do you want to use a file which contains the list of hosts (y/n) [n] >> "
         ret=$?
         if [ $ret = 0 ]; then
            AddHostsFromFile shadowd 
            ret=$?
         fi

         if [ $ret = 1 ]; then
            AddHostsFromTerminal shadowd
         fi

         $INFOTEXT -wait -auto $AUTO -n "Finished adding hosts. Hit <RETURN> to continue >> "
      fi
      $CLEAR

   fi

   if [ "$Overwrite" = "true" -a "$SPOOLING_METHOD" = "classic" ]; then
      $INFOTEXT -u "\nSkipping creation of the default <all.q> queue and <allhosts> hostgroup"
   else
      $INFOTEXT -u "\nCreating the default <all.q> queue and <allhosts> hostgroup"
      echo
      $INFOTEXT -log "Creating the default <all.q> queue and <allhosts> hostgroup"
      TMPL=/tmp/hostqueue$$
      TMPL2=${TMPL}.q
      rm -f $TMPL $TMPL2
      if [ -f $TMPL -o -f $TMPL2 ]; then
         $INFOTEXT "\nCan't delete template files >%s< or >%s<" "$TMPL" "$TMPL2"
      else
		   #Issue if old qmaster is running, new installation succeeds, but in fact the old qmaster is still running!
		   #Reinstall can cause, that these already exist. So we skip them if they already exist.
		   if [ x`$SGE_BIN/qconf -shgrpl 2>/dev/null | grep '^@allhosts$'` = x ]; then
            PrintHostGroup @allhosts > $TMPL
            Execute $SGE_BIN/qconf -Ahgrp $TMPL
			else
			   $INFOTEXT "Skipping creation of <allhosts> hostgroup as it already exists"
				$INFOTEXT -log "Skipping creation of <allhosts> hostgroup as it already exists"
			fi
			if [ x`$SGE_BIN/qconf -sql 2>/dev/null | grep '^all.q$'` = x ]; then
            Execute $SGE_BIN/qconf -sq > $TMPL
            Execute sed -e "/qname/s/template/all.q/" \
                        -e "/hostlist/s/NONE/@allhosts/" \
                        -e "/pe_list/s/NONE/make/" $TMPL > $TMPL2
            Execute $SGE_BIN/qconf -Aq $TMPL2
			else
			   $INFOTEXT "Skipping creation of <all.q> queue as it already exists"
				$INFOTEXT -log "Skipping creation of <all.q> queue  as it already exists"
			fi
         rm -f $TMPL $TMPL2        
      fi

      $INFOTEXT -wait -auto $AUTO -n "\nHit <RETURN> to continue >> "
      $CLEAR
   fi
}


#-------------------------------------------------------------------------
# AddSubmitHosts
#
AddSubmitHosts()
{
   CERT_COPY_HOST_LIST=""
   if [ "$AUTO" = "true" ]; then
      CERT_COPY_HOST_LIST=$SUBMIT_HOST_LIST
      for h in $SUBMIT_HOST_LIST; do
        if [ -f $h ]; then
           $INFOTEXT -log "Adding SUBMIT_HOSTS from file %s" $h
           for tmp in `cat $h`; do
             $INFOTEXT -log "Adding SUBMIT_HOST %s" $tmp
             $SGE_BIN/qconf -as $tmp
           done
        else
             $INFOTEXT -log "Adding SUBMIT_HOST %s" $h
             $SGE_BIN/qconf -as $h
        fi
      done  
   else
      $INFOTEXT -u "\nAdding Cluster Scheduler submit hosts"
      $INFOTEXT "\nPlease now add the list of hosts, which should become submit hosts.\n" \
                "Please enter a blank separated list of your submit hosts. You may\n" \
                "press <RETURN> if the line is getting too long. Once you are finished\n" \
                "simply press <RETURN> without entering a name.\n\n" \
                "You also may prepare a file with the hostnames of the machines where you plan\n" \
                "to install Cluster Scheduler. This may be convenient if you are installing Grid\n" \
                "Engine on many hosts.\n\n"

      $INFOTEXT -auto $AUTO -ask "y" "n" -def "n" -n \
                "Do you want to use a file which contains the list of hosts (y/n) [n] >> "
      ret=$?
      if [ $ret = 0 ]; then
         AddHostsFromFile submit 
         ret=$?
      fi

      if [ $ret = 1 ]; then
         AddHostsFromTerminal submit 
      fi

      $INFOTEXT -wait -auto $AUTO -n "Finished adding hosts. Hit <RETURN> to continue >> "
      $CLEAR
   fi
}


#-------------------------------------------------------------------------
# AddHostsFromFile: Get a list of hosts and add them as
# admin and submit hosts
#
AddHostsFromFile()
{
   hosttype=$1
   file=$2
   done=false
   while [ $done = false ]; do
      $CLEAR
      if [ "$hosttype" = "execd" ]; then
         $INFOTEXT -u "\nAdding admin and submit hosts from file"
      elif [ "$hosttype" = "submit" ]; then
         $INFOTEXT -u "\nAdding submit hosts"
      else
         $INFOTEXT -u "\nAdding admin hosts from file"
      fi
      $INFOTEXT -n "\nPlease enter the file name which contains the host list: "
      file=`Enter none`
      if [ "$file" = "none" -o ! -f "$file" ]; then
         $INFOTEXT "\nYou entered an invalid file name or the file does not exist."
         $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n \
                   "Do you want to enter a new file name (y/n) [y] >> "
         if [ $? = 1 ]; then
            return 1
         fi
      else
         if [ "$hosttype" = "execd" ]; then
            for h in `cat $file`; do
               $SGE_BIN/qconf -ah $h
               $SGE_BIN/qconf -as $h
            done
         elif [ "$hosttype" = "submit" ]; then
            for h in $hlist; do
               $SGE_BIN/qconf -as $h
               CERT_COPY_HOST_LIST="$CERT_COPY_HOST_LIST $h" 
            done
         else
            for h in `cat $file`; do
               $SGE_BIN/qconf -ah $h
            done
         fi
         done=true
      fi
   done
}

#-------------------------------------------------------------------------
# AddHostsFromTerminal
#    Get a list of hosts and add the mas admin and submit hosts
#
AddHostsFromTerminal()
{
   hosttype=$1
   stop=false
   while [ $stop = false ]; do
      $CLEAR
      if [ "$hosttype" = "execd" ]; then
         $INFOTEXT -u "\nAdding admin and submit hosts"
      elif [ "$hosttype" = "submit" ]; then
         $INFOTEXT -u "\nAdding submit hosts"
      else
         $INFOTEXT -u "\nAdding admin hosts"
      fi
      $INFOTEXT "\nPlease enter a blank seperated list of hosts.\n\n" \
                "Stop by entering <RETURN>. You may repeat this step until you are\n" \
                "entering an empty list. You will see messages from Cluster Scheduler\n" \
                "when the hosts are added.\n"

      $INFOTEXT -n "Host(s): "

      hlist=`Enter ""`
      if [ "$hosttype" = "execd" ]; then
         for h in $hlist; do
            $SGE_BIN/qconf -ah $h
            $SGE_BIN/qconf -as $h
         done
      elif [ "$hosttype" = "submit" ]; then
         for h in $hlist; do
            $SGE_BIN/qconf -as $h
            CERT_COPY_HOST_LIST="$CERT_COPY_HOST_LIST $h"
         done
      else
         for h in $hlist; do
            $SGE_BIN/qconf -ah $h
         done
      fi
      if [ "$hlist" = "" ]; then
         stop=true
      else
         $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
      fi
  done
}


#-------------------------------------------------------------------------
# PrintHostGroup:  print an empty hostgroup
#
PrintHostGroup()
{
   $ECHO "group_name  $1"
   $ECHO "hostlist    NONE"
}


#-------------------------------------------------------------------------
# GetQmasterPort: get communication port SGE_QMASTER_PORT
#
GetQmasterPort()
{

   if [ $RESPORT = true ]; then
      comm_port_max=1023
   else
      comm_port_max=65500
   fi
    PortCollision $SGE_QMASTER_SRV
    PortSourceSelect $SGE_QMASTER_SRV

   if [ "$SGE_QMASTER_PORT" != "" -a "$port_source" != "db" ]; then
      $INFOTEXT -u "\nCluster Scheduler TCP/IP communication service"

      if [ $SGE_QMASTER_PORT -ge 1 -a $SGE_QMASTER_PORT -le $comm_port_max ]; then
         $INFOTEXT "\nUsing the environment variable\n\n" \
                   "   \$SGE_QMASTER_PORT=%s\n\n" \
                     "as port for communication.\n\n" $SGE_QMASTER_PORT
                      export SGE_QMASTER_PORT
                      $INFOTEXT -log "Using SGE_QMASTER_PORT >%s<." $SGE_QMASTER_PORT
         if [ "$collision_flag" = "services_only" -o "$collision_flag" = "services_env" ]; then
            $INFOTEXT "This overrides the preset TCP/IP service >sge_qmaster<.\n"
         fi
         $INFOTEXT -auto $AUTO -ask "y" "n" -def "n" -n "Do you want to change the port number? (y/n) [n] >> "
         if [ "$?" = 0 ]; then
            EnterPortAndCheck $SGE_QMASTER_SRV
         fi
         $CLEAR
         return
      else
         $INFOTEXT "\nThe environment variable\n\n" \
                   "   \$SGE_QMASTER_PORT=%s\n\n" \
                   "has an invalid value (it must be in range 1..%s).\n\n" \
                   "Please set the environment variable \$SGE_QMASTER_PORT and restart\n" \
                   "the installation or configure the service >sge_qmaster<." $SGE_QMASTER_PORT $comm_port_max
         $INFOTEXT -log "Your \$SGE_QMASTER_PORT=%s\n\n" \
                   "has an invalid value (it must be in range 1..%s).\n\n" \
                   "Please check your configuration file and restart\n" \
                   "the installation or configure the service >sge_qmaster<." $SGE_QMASTER_PORT $comm_port_max
      fi
   fi
   $INFOTEXT -u "\nCluster Scheduler TCP/IP service >sge_qmaster<"
   if [ "$port_source" = "env" ]; then
      EnterPortAndCheck $SGE_QMASTER_SRV
      $CLEAR
   else
      EnterServiceOrPortAndCheck $SGE_QMASTER_SRV
      if [ "$port_source" = "db" ]; then
         $INFOTEXT "\nUsing the service\n\n" \
                   "   sge_qmaster\n\n" \
                   "for communication with Cluster Scheduler.\n"
         qmaster_service="true"
      fi
      $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
      $CLEAR
   fi
}

EnterServiceOrPortAndCheck()
{
   if [ "$1" = "sge_qmaster" ]; then
      service_name="sge_qmaster"
      port_var_name="SGE_QMASTER_PORT"
   else
      service_name="sge_execd"
      port_var_name="SGE_EXECD_PORT"

   fi

      # Check if $SGE_SERVICE service is available now
      service_available=false
      done=false
      while [ $done = false ]; do
         CheckServiceAndPorts service $service_name
         if [ $ret != 0 ]; then
            $CLEAR
            $INFOTEXT -u "\nNo TCP/IP service >%s< yet" $service_name
            $INFOTEXT -n "\nIf you have just added the service it may take a while until the service\n" \
                         "propagates in your network. If this is true we can again check for\n" \
                         "the service >%s<. If you don't want to add this service or if\n" \
                         "you want to install Cluster Scheduler just for testing purposes you can enter\n" \
                         "a port number.\n" $service_name

              if [ $AUTO != "true" ]; then
                 $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n \
                      "Check again (enter [n] to specify a port number) (y/n) [y] >> "
                 ret=$?
              else
                 $INFOTEXT -log "Setting %s" $port_var_name
                 ret=1
              fi

            if [ $ret = 1 ]; then
               if [ $AUTO = true ]; then
                  $INFOTEXT -log "Please use an unused port number!"
                  MoveLog
                  exit 1
               fi
               $CLEAR
               EnterAndValidatePortNumber $1
               SelectedPortOutput $1
               port_source="env"
            fi
         else
            done=true
            service_available=true
         fi
      done
}


EnterPortAndCheck()
{

   if [ "$1" = "sge_qmaster" ]; then
      service_name="sge_qmaster"
      port_var_name="SGE_QMASTER_PORT"
   else
      service_name="sge_execd"
      port_var_name="SGE_EXECD_PORT"

   fi
      # Check if $SGE_SERVICE service is available now
      done=false
      while [ $done = false ]; do
            $CLEAR
            if [ $AUTO = true ]; then
               $INFOTEXT -log "Please use an unused port number!"
               MoveLog
               exit 1
            fi

            EnterAndValidatePortNumber $1
            SelectedPortOutput $1
      done
   $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
}


SelectedPortOutput()
{
   if [ "$1" = "sge_qmaster" ]; then
      SGE_QMASTER_PORT=$INP
      $INFOTEXT "\nUsing the environment variable\n\n" \
      "   \$SGE_QMASTER_PORT=%s\n\n" \
      "as port for communication.\n\n" $SGE_QMASTER_PORT
      export SGE_QMASTER_PORT
      $INFOTEXT -log "Using SGE_QMASTER_PORT >%s<." $SGE_QMASTER_PORT
   else
      SGE_EXECD_PORT=$INP
      $INFOTEXT "\nUsing the environment variable\n\n" \
      "   \$SGE_EXECD_PORT=%s\n\n" \
      "as port for communication.\n\n" $SGE_EXECD_PORT
      export SGE_EXECD_PORT
      $INFOTEXT -log "Using SGE_EXECD_PORT >%s<." $SGE_EXECD_PORT
   fi
}



EnterAndValidatePortNumber()
{
   $INFOTEXT -u "\nCluster Scheduler TCP/IP service >%s<\n" $service_name
   $INFOTEXT -n "\n"

   port_ok="false"

   if [ "$1" = "sge_qmaster" ]; then
      $INFOTEXT -n "Please enter an unused port number >> "
      INP=`Enter $SGE_QMASTER_PORT`
   else
      while [ $port_ok = "false" ]; do 
         $INFOTEXT -n "Please enter an unused port number >> "
         INP=`Enter $SGE_EXECD_PORT`
         port_ok="true"
         if [ "$INP" = "$SGE_QMASTER_PORT" -a $1 = "sge_execd" ]; then
            $INFOTEXT "\nPlease use any other port number!"
            $INFOTEXT "Port number %s is already used by sge_qmaster\n" $SGE_QMASTER_PORT
            port_ok="false"
            if [ $AUTO = "true" ]; then
               $INFOTEXT -log "Please use any other port number!"
               $INFOTEXT -log "Port number %s is already used by sge_qmaster" $SGE_QMASTER_PORT
               $INFOTEXT -log "Installation failed!!!"
               MoveLog
               exit 1
            fi
         fi
      done
   fi

   chars=`echo $INP | wc -c`
   chars=`expr $chars - 1`
   digits=`expr $INP : "[0-9][0-9]*"`
   if [ "$INP" = "" ]; then
      $INFOTEXT "\nYou must enter an integer value."
   elif [ "$chars" != "$digits" ]; then
      $INFOTEXT "\nInvalid input. Must be a number."
   elif [ $INP -le 1 -o $INP -ge $comm_port_max ]; then
      $INFOTEXT "\nInvalid port number. Must be in range [1..%s]." $comm_port_max
   elif [ $INP -le 1024 -a $euid != 0 ]; then
      $INFOTEXT "\nYou are not user >root<. You need to use a port above 1024."
   else
      CheckServiceAndPorts port ${INP}

      if [ $ret = 0 ]; then
         $INFOTEXT "\nFound service with port number >%s< in >/etc/services<. Choose again." "$INP"
      else
         done=true
      fi
   fi
   if [ $done = false ]; then
      $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
   fi
}


########################################################
#
# Version to convert a version string in X.Y.Z-* or
# X.Y.X_NN format to XYZNN format so can be treated as a
# number.
#
# $1 = version string
# Returns numerical version
#
########################################################
JavaVersionString2Num () {
   if [ -z "$1" ]; then
      echo 0
      return
   fi
   
   # Minor and micro default to 0 if not specified.
   major=`echo $1 | awk -F. '{print $1}'`
   minor=`echo $1 | awk -F. '{print $2}'`
   if [ ! -n "$minor" ]; then
      minor="0"
   fi
   micro=`echo $1 | awk -F. '{print $3}'`
   if [ ! -n "$micro" ]; then
      micro="0"
   fi

   # The micro version may further be extended to include a patch number.
   # This is typically of the form <micro>_NN, where NN is the 2-digit
   # patch number.  However it can also be of the form <micro>-XX, where
   # XX is some arbitrary non-digit sequence (eg., "rc").  This latter
   # form is typically used for internal-only release candidates or
   # development builds.
   #
   # For these internal builds, we drop the -XX and assume a patch number 
   # of "00".  Otherwise, we extract that patch number.
   #
   # OpenJDK uses "0" instead of "00"
   #
   patch="00"
   dash=`echo $micro | grep "-"`
   if [ $? -eq 0 ]; then
      # Must be internal build, so drop the trailing variant.
      micro=`echo $micro | awk -F- '{print $1}'`
   fi

   underscore=`echo $micro | grep "_"`
   if [ $? -eq 0 ]; then
      # Extract the seperate micro and patch numbers, ignoring anything
      # after the 2-digit patch.
      # we ensure we return 2 characters as patch
      patch=`echo $micro | awk -F_ '{print substr($2, 1, 2)}' | awk '{if (length($0) < 2) print 0$0; else print $0}'`
      micro=`echo $micro | awk -F_ '{print $1}'`
   fi

   echo "${major}${minor}${micro}${patch}"

} # versionString2Num



#---------------------------------------------------------------------------
# GetJvmLib - sets jvm_lib_path if found from a java_bin
#             java_bin version must be checked before going here
# $1 - java_bin
GetJvmLib()
{
   java_bin=$1
   java_home=`dirname $java_bin`
   java_home=`dirname $java_home`
   jvm_lib_path=`GetJvmLibFromJavaHome $java_home`
    
   # Not found, let's try a java based detection
   if [ -z "$jvm_lib_path" ]; then
      FLAGS=""
      if [ x`echo $SGE_ARCH | grep 64` != x ]; then
         FLAGS="-d64"
      fi         
      jvm_lib_path=`$java_bin $FLAGS -jar $SGE_ROOT/util/DetectJvmLibrary.jar 2>/dev/null`
      if [ -z "$jvm_lib_path" ]; then
         java_home=""
      fi
   fi
}


#---------------------------------------------------------------------------
# IsJavaBinSuitable - prints the java_bin if valid min version
# $1 - java_bin
# $2 - MIN_JAVA_VERSION
# $3 - (optional) check type, default is "none", allowed is "jvm"
IsJavaBinSuitable()
{
   java_bin=$1
   NUM_MIN_JAVA_VERSION=`JavaVersionString2Num $2`
   check=$3
   
   JAVA_VERSION=`$java_bin -version 2>&1 | head -1`
   JAVA_VERSION=`echo $JAVA_VERSION | awk '$3 ~ /"1.[45678]/ {print $3}' 2>/dev/null | sed -e "s/\"//g" 2>/dev/null`
   NUM_JAVA_VERSION=`JavaVersionString2Num $JAVA_VERSION`     
   if [ $NUM_JAVA_VERSION -ge $NUM_MIN_JAVA_VERSION ]; then
      if [ "$check" = "jvm" ]; then
         jvm_lib_path=""
         GetJvmLib $java_bin
         if [ -n "$jvm_lib_path" ]; then
            #Verify we can load it
            $SGE_ROOT/utilbin/$SGE_ARCH/valid_jvmlib "$jvm_lib_path" >/dev/null 2>&1
            return $?
         fi
      else
         return 0
      fi
   fi
   return 1
}


#---------------------------------------------------------------------------
# GetSuitableJavaBin - finds first valid java_bin valid min version
# $1 - list
# $2 - MIN_JAVA_VERSION
# $3 - (optional) check type, default is "none", allowed is "jvm"
GetSuitableJavaBin()
{
   list=$1
   # Make the list unique and correct list
   list=`echo $list | awk '{for (i=1; i<=NF ; i++) print $i}' 2>/dev/null| uniq 2>/dev/null`
   #Find a first good one
   for java_bin in $list; do
      IsJavaBinSuitable $java_bin $2 $3
      if [ $? -eq 0 ]; then
         #echo $java_bin
         return 0
      fi
   done
   return 1
}

#---------------------------------------------------------------------------
# GetDefaultJavaForPlatform - helper for HaveSuitableJavaBinList
#                             adds java_homes to the list variable
#
GetDefaultJavaForPlatform()
{
   case $SGE_ARCH in
      sol-sparc64) 
         java_homes="/usr/java
/usr/jdk/latest"
         ;;
      sol-amd64)   
         java_homes="/usr/java
/usr/jdk/latest"
         ;;
      lx*-amd64)
         java_homes="/usr/java
/usr/jdk/latest
/usr/java/latest
/etc/alternatives/jre"
         ;;
      lx*-x86)     
         java_homes="/usr/java
/usr/jdk/latest
/usr/java/latest
/etc/alternatives/jre"
         ;;
      darwin-x64)
         java_homes="/Library/Java/Home
/System/Library/Frameworks/JavaVM.framework/Home"
         ;;
   esac
   for java_home in $java_homes; do
      if [ -x $java_home/bin/java ]; then
         list="$list $java_home/bin/java"
      fi
   done
}


#---------------------------------------------------------------------------
# HaveSuitableJavaBin
# $1 - minimal Java version
# $2 - (optional) check type, default is "none", allowed is "jvm"
HaveSuitableJavaBin() {
   MIN_JAVA_VERSION=$1
   check=$2
   if [ "$check" != "jvm" ]; then
      check="none"
   fi
   #Set SGE_ARCH if not available
   if [ -z "$SGE_ARCH" ]; then
      if [ -z "$SGE_ROOT" ]; then
         SGE_ROOT="."
      fi
      SGE_ARCH=`$SGE_ROOT/util/arch`
   fi
   
   #Default paths to look for Java
   list=""
   if [ -n "$JAVA_HOME" ]; then
      list="$list $JAVA_HOME/bin/java"
      list="$list $JAVA_HOME/jre/bin/java"
   fi
   if [ -n "$JDK_HOME" ]; then
      list="$list $JDK_HOME/bin/java"
      list="$list $JDK_HOME/jre/bin/java"
   fi

   on_path=`which java 2>/dev/null`
   if [ -n "$on_path" ]; then
      list="$list $on_path"
   fi
   
   #Try known default locations for each platform
   GetDefaultJavaForPlatform   
   GetSuitableJavaBin "$list" $MIN_JAVA_VERSION $check
   res=$?
   #If we didn't find a suitable jvm library we need to try further
   if [ -z "$res" ]; then
      list=""
      #Arch specific methods to look for a bin/java
      #Breakdown the detection for each plaform since same tools might behave differently
      #Solaris
      if [ x`echo $SGE_ARCH| grep "sol-"` != x ]; then
         #TODO: How to do it on Solaris 9/10?
         SOLARIS_VERSION=`uname -r 2>/dev/null| awk -F. '{print $2}' 2>/dev/null`
         #OpenSolaris
         if [ "$SOLARIS_VERSION" = 11 ]; then        
            list="$list `pkg search java 2>/dev/null| grep bin/java | grep file | awk '{print "/"$3}' 2>/dev/null`"
         fi
      #Linux
      elif [ x`echo $SGE_ARCH| grep "lx-"` != x ]; then
         #Try whereis
         temp=`whereis java 2>/dev/null`
         if [ -n "$temp" ]; then
            list="$list `echo $temp | awk '{for (i=1; i<=NF ; i++) print $i}' 2>/dev/null`"
         fi
         #TODO: other tools like locate?
      fi
      #TODO: Other platforms
      
      GetSuitableJavaBin "$list" $MIN_JAVA_VERSION $check
      res=$?
   fi
   if [ "$3" = "print" -a "$res" = 0 ]; then
      if [ "$2" = "jvm" ]; then
         echo "$jvm_lib_path"
      else
         echo "$java_bin"
      fi
   fi
   return $res
}

#---------------------------------------------------------------------------
# GetJvmLibFromJavaHome
# $1 - java binary
#
GetJvmLibFromJavaHome() {
   java_home=$1
   suffix=""
   case $SGE_ARCH in
      sol-sparc64) 
         suffix=lib/sparcv9/server/libjvm.so
         ;;
      sol-amd64)   
         suffix=lib/amd64/server/libjvm.so
         ;;
      sol-x86)     
         #causes a SEGV of libjvm.so for JVM_RawMonitorCreate
         #suffix=lib/i386/server/libjvm.so
         suffix=lib/i386/client/libjvm.so
         ;;
      lx*-amd64)   
         suffix=lib/amd64/server/libjvm.so
         ;;
      lx*-x86)     
         suffix=lib/i386/server/libjvm.so
         ;;
      darwin-x64)
         suffix=../Libraries/libjvm.dylib
         ;;
   #TODO: Missing HP, AIX platforms
   esac
   if [ -f $java_home/$suffix ]; then
      echo $java_home/$suffix
   fi
}

#-------------------------------------------------------------------------
# GetExecdPort: get communication port SGE_EXECD_PORT
#
GetExecdPort()
{

    if [ $RESPORT = true ]; then
       comm_port_max=1023
    else
       comm_port_max=65500
    fi

    PortCollision $SGE_EXECD_SRV
    PortSourceSelect $SGE_EXECD_SRV
    if [ "$SGE_EXECD_PORT" != "" -a "$port_source" != "db" ]; then
      $INFOTEXT -u "\nCluster Scheduler TCP/IP communication service"

      if [ $SGE_EXECD_PORT -ge 1 -a $SGE_EXECD_PORT -le $comm_port_max ]; then
         $INFOTEXT "\nUsing the environment variable\n\n" \
                   "   \$SGE_EXECD_PORT=%s\n\n" \
                     "as port for communication.\n\n" $SGE_EXECD_PORT
                      export SGE_EXECD_PORT
                      $INFOTEXT -log "Using SGE_EXECD_PORT >%s<." $SGE_EXECD_PORT
         if [ "$collision_flag" = "services_only" -o "$collision_flag" = "services_env" ]; then
            $INFOTEXT "This overrides the preset TCP/IP service >sge_execd<.\n"
         fi
         $INFOTEXT -auto $AUTO -ask "y" "n" -def "n" -n "Do you want to change the port number? (y/n) [n] >> "
         if [ "$?" = 0 ]; then
            EnterPortAndCheck $SGE_EXECD_SRV 
         fi
         $CLEAR
         return
      else
         $INFOTEXT "\nThe environment variable\n\n" \
                   "   \$SGE_EXECD_PORT=%s\n\n" \
                   "has an invalid value (it must be in range 1..%s).\n\n" \
                   "Please set the environment variable \$SGE_EXECD_PORT and restart\n" \
                   "the installation or configure the service >sge_execd<." $SGE_EXECD_PORT $comm_port_max
         $INFOTEXT -log "Your \$SGE_EXECD_PORT=%s\n\n" \
                   "has an invalid value (it must be in range 1..%s).\n\n" \
                   "Please check your configuration file and restart\n" \
                   "the installation or configure the service >sge_execd<." $SGE_EXECD_PORT $comm_port_max
      fi
   fi         
      $INFOTEXT -u "\nCluster Scheduler TCP/IP communication service "
   if [ "$port_source" = "env" ]; then

         $INFOTEXT "Make sure to use a different port number for the execution host\n" \
                   "as on the qmaster machine\n"
         if [ `$SGE_UTILBIN/getservbyname $SGE_QMASTER_SRV 2>/dev/null | wc -w` = 0 -a "$SGE_QMASTER_PORT" != "" ]; then
            $INFOTEXT "The qmaster port SGE_QMASTER_PORT = %s\n" $SGE_QMASTER_PORT
         elif [ `$SGE_UTILBIN/getservbyname sge_qmaster 2>/dev/null | wc -w` != 0 -a "$SGE_QMASTER_PORT" = "" ]; then
            $INFOTEXT "sge_qmaster service set to port %s\n" `$SGE_UTILBIN/getservbyname $SGE_QMASTER_SRV | cut -d" " -f2`
         else 
            $INFOTEXT "The qmaster port SGE_QMASTER_PORT = %s" $SGE_QMASTER_PORT
            $INFOTEXT "sge_qmaster service set to port %s\n" `$SGE_UTILBIN/getservbyname $SGE_QMASTER_SRV | cut -d" " -f2`
         fi 
         $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "

      # Check if $SGE_SERVICE service is available now
      EnterPortAndCheck $SGE_EXECD_SRV
      $CLEAR
   else
      EnterServiceOrPortAndCheck $SGE_EXECD_SRV
      if [ "$service_available" = "true" ]; then
         $INFOTEXT "\nUsing the service\n\n" \
                   "   sge_execd\n\n" \
                   "for communication with Cluster Scheduler.\n"
         execd_service="true"
      fi
      $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
      $CLEAR
   fi
}


#-------------------------------------------------------------------------
# GetDefaultDomain
#
GetDefaultDomain()
{
   done=false

   if [ $AUTO = "true" ]; then
      $INFOTEXT -log "Using >%s< as default domain." $DEFAULT_DOMAIN
      CFG_DEFAULT_DOMAIN=$DEFAULT_DOMAIN
      done=true
   fi

   while [ $done = false ]; do
      $CLEAR
      $INFOTEXT -u "\nDefault domain for hostnames"

      $INFOTEXT "\nSometimes the primary hostname of machines returns the short hostname\n" \
                  "without a domain suffix like >foo.com<.\n\n" \
                  "This can cause problems with getting load values of your execution hosts.\n" \
                  "If you are using DNS or you are using domains in your >/etc/hosts< file or\n" \
                  "your NIS configuration it is usually safe to define a default domain\n" \
                  "because it is only used if your execution hosts return the short hostname\n" \
                  "as their primary name.\n\n" \
                  "If your execution hosts reside in more than one domain, the default domain\n" \
                  "parameter must be set on all execution hosts individually.\n"

      $INFOTEXT -auto $AUTO -ask "y" "n" -def "y" -n \
                "Do you want to configure a default domain (y/n) [y] >> "
      if [ $? = 0 ]; then
         $INFOTEXT -n "\nPlease enter your default domain >> "
         CFG_DEFAULT_DOMAIN=`Enter ""`
         if [ "$CFG_DEFAULT_DOMAIN" != "" ]; then
            $INFOTEXT -wait -auto $AUTO -n "\nUsing >%s< as default domain. Hit <RETURN> to continue >> " \
                      $CFG_DEFAULT_DOMAIN
            $CLEAR
            done=true
         fi
      else
         CFG_DEFAULT_DOMAIN=none
         done=true
      fi
   done
}

SetScheddConfig()
{
   $CLEAR

   $INFOTEXT -u "Scheduler Configuration"
   $INFOTEXT -n "\nThe details on the different options are described in the manual. \n"

   is_selected="default"
   $INFOTEXT -n "Setting scheduler configuration to >%s< setting!\n " $is_selected
   $SGE_BIN/qconf -Msconf ./util/install_modules/inst_schedd_default.conf

   $INFOTEXT -wait -auto $AUTO -n "Hit <RETURN> to continue >> "
   $CLEAR
}


#-------------------------------------------------------------------------
# PortCollision: Is there port collison for service, SGE_QMASTER or
#                  SGE_EXECD
PortCollision()
{

   service=$1
   # Call CheckPortsCollision conflict and depending on $ret, print out
   # appropriate text

   CheckPortsCollision $service

   #$ECHO "collision_flag is $collision_flag \n"
   $INFOTEXT -u "\nCluster Scheduler TCP/IP communication service"

   case "$collision_flag" in

      env_only)
         $INFOTEXT "\nThe port for %s is currently set by the shell environment.\n\n" $service
         if [ "$service" = "sge_qmaster" ]; then
            $INFOTEXT "   SGE_QMASTER_PORT = %s" $SGE_QMASTER_PORT
         else
            $INFOTEXT "   SGE_EXECD_PORT = %s" $SGE_EXECD_PORT
         fi
         INP=1
      ;;

      services_only)
         $INFOTEXT "\nThe port for %s is currently set as service.\n" $service
         if [ "$service" = "sge_qmaster" ]; then
            $INFOTEXT "   sge_qmaster service set to port %s" `$SGE_UTILBIN/getservbyname $service | cut -d" " -f2` 
         else
            $INFOTEXT "   sge_execd service set to port %s" `$SGE_UTILBIN/getservbyname $service | cut -d" " -f2` 
         fi
         INP=2
      ;;

      services_env)
         $INFOTEXT "\nThe port for %s is curently set BOTH as service and by the\nshell environment\n" $service
         if [ "$service" = "sge_qmaster" ]; then
            $INFOTEXT "   SGE_QMASTER_PORT = %s" $SGE_QMASTER_PORT
            $INFOTEXT "   sge_qmaster service set to port %s" `$SGE_UTILBIN/getservbyname $service | cut -d" " -f2` 
            $INFOTEXT "\n   Currently SGE_QMASTER_PORT = %s is active!" $SGE_QMASTER_PORT 
         else
            $INFOTEXT "   SGE_EXECD_PORT = %s" $SGE_EXECD_PORT
            $INFOTEXT "   sge_execd service set to port %s" `$SGE_UTILBIN/getservbyname $service | cut -d" " -f2` 
            $INFOTEXT "\n   Currently SGE_EXECD_PORT = %s is active!" $SGE_EXECD_PORT 
         fi
         INP=1
      ;;

      no_ports)
         $INFOTEXT "\nThe communication settings for %s are currently not done.\n\n" $service
         INP=1
       ;;

   esac
}


PortSourceSelect()
{
   $INFOTEXT "\nNow you have the possibility to set/change the communication ports by using the\n>shell environment< or you may configure it via a network service, configured\nin local >/etc/service<, >NIS< or >NIS+<, adding an entry in the form\n\n"
   $INFOTEXT "    %s <port_number>/tcp\n\n" $1
   $INFOTEXT "to your services database and make sure to use an unused port number.\n\n"
   $INFOTEXT -n "How do you want to configure the Cluster Scheduler communication ports?\n\n"
   $INFOTEXT "Using the >shell environment<:                           [1]\n"
   $INFOTEXT -n "Using a network service like >/etc/service<, >NIS/NIS+<: [2]\n\n(default: %s) >> " $INP
   #INP will be set in function: PortCollision, we need this as default value for auto install
   INP=`Enter $INP`

   if [ "$INP" = "1" ]; then
      port_source="env"
   elif [ "$INP" = "2" ]; then
      port_source="db"
      if [ "$1" = "sge_qmaster" ]; then
         unset SGE_QMASTER_PORT
         export SGE_QMASTER_PORT
      else
         unset SGE_EXECD_PORT
         export SGE_EXECD_PORT
      fi
   fi
   
   $CLEAR
}
