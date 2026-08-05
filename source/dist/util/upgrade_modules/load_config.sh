#!/bin/sh
#
# SGE configuration script (Installation/Uninstallation/Upgrade/Downgrade)
# Scriptname: load_config.sh
# Module: common functions
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

INFOTEXT=echo

if [ -z "$SGE_ROOT" -o -z "$SGE_CELL" ]; then
   $INFOTEXT "Set your SGE_ROOT, SGE_CELL first!"
   exit 1
fi
cd $SGE_ROOT

ARCH=`$SGE_ROOT/util/arch`
CAT=cat
MKDIR=mkdir
LS=ls
QCONF=$SGE_ROOT/bin/$ARCH/qconf
HOST=`$SGE_ROOT/utilbin/$ARCH/gethostname -aname`
ON_ERROR="abort"

. "$SGE_ROOT/util/install_modules/inst_common.sh"
BasicSettings
GetAdminUser

SUCCEEDED_LOADLOC=""

Usage()
{
   myname=`basename $0`
   $INFOTEXT "Usage: $myname <dir> <log_file>\n" \
             "          [-log I|W|C]\n" \
             "          [-mode upgrade|copy]\n" \
             "          [-on_error abort|continue|cont_if_exist]\n" \
             "          [-newijs true|false]\n" \
             "          [-execd_spool_dir <value>]\n" \
             "          [-admin_mail <value>]\n" \
             "          [-gid_range <integer_range_value>]\n" \
             "          [-help]\n" \
             "\n" \
             "\nExample:\n" \
             "   $myname -log C -mode copy -newijs true -execd_spool_dir /sge/real_execd_spool -admin_mail user@host.com -gid_range 23000-24000\nLoads the configuration according to the following rules:\n" \
             "   Shows only critical errors\n" \
             "   Uses copy upgrade mode (local execd spool dirs will be changed)\n" \
             "   Enables new interactive job support\n" \
             "   Changes the global execution daemon spooling directory\n" \
             "   Sets the address to which to send mail\n" \
             "   Sets the group ID range"
}


#All logging is done by this functions
LogIt()
{
   urgency="${1:?Urgency is required [I,W,C]}"
   message="${2:?Message is required}"

   #log file contains all messages
   echo "${urgency} $message" >> $MESSAGE_FILE_NAME
   #log when urgency and level is meet
   case "${urgency}${LOGGER_LEVEL}" in
      CC|CW|CI)
         $INFOTEXT "[CRITICAL] $message"
      ;;
      WW|WI)
         $INFOTEXT "[WARNING] $message"
      ;;
      II)
         $INFOTEXT "[INFO] $message"
		;;
   esac
}

#Resolve a result during Loading
ResolveResult()
{
   resOpt="${1:?Need an option to decide}"
   resFile="${2:?Need the file name to load}"
   resMsg="${3-""}"
   resRet="${4:?Need a return code to show the last result}"
   LogIt "I" "ResolveResult ret:$resRet,  opt:$resOpt, file:$resFile, msg:${resMsg}"

   obj=`echo ${resMsg} | awk -F'"' '{ print $2 }'`
   obj=${obj:-unknown}

   #we are expecting troubles, possitive match required
   ret=1
   case "$resOpt" in
      -ah|-ac|-as|-am|-ao)
         #We can ignore  (already exists)
         # CS-2394: -am/-ao now operate on the reserved manager/operator access
         # lists, so re-adding an existing entry reports "X is already in access
         # list Y" instead of the old "manager X already exists". Accept both.
         case "$resMsg" in
            *'already exists'|*'is already in access list'*)
               if [ "$ON_ERROR" = "abort" ]; then
                  LogIt "C" "Aborting on error as requested: $resOpt $obj already exists"
                  EXIT 1
               elif [ "$ON_ERROR" = "continue" ]; then
                  LogIt "W" "$obj already exists, accepted"
                  return 0
               else
                  LogIt "I" "$obj already exists, accepted"
                  return 0
               fi
            ;;
         esac
		;;
      -Acal)
         case "$resMsg" in
            *'already exists')
               if [ "$ON_ERROR" = "abort" ]; then
                  LogIt "C" "Aborting on error as requested: calendar $obj already exists"
                  EXIT 1
               elif [ "$ON_ERROR" = "continue" ]; then
                  LogIt "W" "$obj already exists, trying to modify -Mcal"
                  LoadConfigFile "$resFile" "-Mcal"
                  ret=$?
                  return $ret
               else
                  LogIt "I" "$obj already exists, accepted"
                  return 0
               fi
            ;;
         esac
      ;;
      -Aconf)
         case "$resMsg" in
            *'already exists')
               if [ "$ON_ERROR" = "abort" ]; then
                  LogIt "C" "Aborting on error as requested: configuration $obj already exists"
                  EXIT 1
               elif [ "$ON_ERROR" = "continue" ]; then
                  LogIt "W" "$obj already exists, trying to modify -Mconf"
                  LoadConfigFile "$resFile" "-Mconf"
                  ret=$?
                  return $ret
               else
                  LogIt "I" "$obj already exists, accepted"
                  return 0
               fi
            ;;
            *'will not be effective before sge_execd restart'*)
               #regular upgrade message
               return 0
            ;;
         esac
      ;;
      -Ackpt)
         case "$resMsg" in
            *'already exists')
               if [ "$ON_ERROR" = "abort" ]; then
                  LogIt "C" "Aborting on error as requested: ckpt. environment $obj already exists"
                  EXIT 1
               elif [ "$ON_ERROR" = "continue" ]; then
                  LogIt "W" "$obj already exists, trying to modify -Mckpt"
                  LoadConfigFile "$resFile" "-Mckpt"
                  ret=$?
                  return $ret
               else
                  LogIt "I" "$obj already exists, accepted"
                  return 0
               fi
            ;;
         esac
      ;;
      -Ae)
         case "$resMsg" in
            *'already exists')
               if [ "$ON_ERROR" = "abort" ]; then
                  LogIt "C" "Aborting on error as requested: execution host $obj already exists"
                  EXIT 1
               elif [ "$ON_ERROR" = "continue" ]; then
                  LogIt "W" "$obj already exists, trying to modify -Me"
                  LoadConfigFile "$resFile" "-Me"
                  ret=$?
                  return $ret
               else
                  LogIt "I" "$obj already exists, accepted"
                  return 0
               fi
            ;;
         esac
      ;;
      -Ahgrp)
         case "$resMsg" in
            *'already exists')
               if [ "$ON_ERROR" = "abort" ]; then
                  LogIt "C" "Aborting on error as requested: host group $obj already exists"
                  EXIT 1
               elif [ "$ON_ERROR" = "continue" ]; then
                  LogIt "W" "$obj already exists, trying to modify -Mhgrp"
                  LoadConfigFile "$resFile" "-Mhgrp"
                  ret=$?
                  return $ret
               else
                  LogIt "I" "$obj already exists, accepted"
                  return 0
               fi
            ;;
         esac
      ;;
      -Aprj)
         case "$resMsg" in
            *'already exists')
               if [ "$ON_ERROR" = "abort" ]; then
                  LogIt "C" "Aborting on error as requested: project $obj already exists"
                  EXIT 1
               elif [ "$ON_ERROR" = "continue" ]; then
                  LogIt "W" "$obj already exists, trying to modify -Mprj"
                  LoadConfigFile "$resFile" "-Mprj"
                  ret=$?
                  return $ret
               else
                  LogIt "I" "$obj already exists, accepted"
                  return 0
               fi
            ;;
         esac
      ;;
      -Auser)
         case "$resMsg" in
            *'already exists')
               if [ "$ON_ERROR" = "abort" ]; then
                  LogIt "C" "Aborting on error as requested: user $obj already exists"
                  EXIT 1
               elif [ "$ON_ERROR" = "continue" ]; then
                  LogIt "W" "$obj already exists, trying to modify -Muser"
                  LoadConfigFile "$resFile" "-Muser"
                  ret=$?
                  return $ret
               else
                  LogIt "I" "$obj already exists, accepted"
                  return 0
               fi
            ;;
         esac
      ;;
      -Au)
         case "$resMsg" in
            *'already exists')
               if [ "$ON_ERROR" = "abort" ]; then
                  LogIt "C" "Aborting on error as requested: userset $obj already exists"
                  EXIT 1
               elif [ "$ON_ERROR" = "continue" ]; then
                  LogIt "W" "$obj already exists, trying to modify -Mu"
                  LoadConfigFile "$resFile" "-Mu"
                  ret=$?
                  return $ret
               else
                  LogIt "I" "$obj already exists, accepted"
                  return 0
               fi
            ;;
         esac
      ;;
      -Ap)
         case "$resMsg" in
            *'already exists')
               if [ "$ON_ERROR" = "abort" ]; then
                  LogIt "C" "Aborting on error as requested: parallel object $obj already exists"
                  EXIT 1
               elif [ "$ON_ERROR" = "continue" ]; then
                  LogIt "W" "$obj already exists, trying to modify -Mp"
                  LoadConfigFile "$resFile" "-Mp"
                  ret=$?
                  return $ret
               else
                  LogIt "I" "$obj already exists, accepted"
                  return 0
               fi
            ;;
         esac
      ;;
      -Aq)
         case "$resMsg" in
            *'already exists')
               if [ "$ON_ERROR" = "abort" ]; then
                  LogIt "C" "Aborting on error as requested: queue $obj already exists"
                  EXIT 1
               elif [ "$ON_ERROR" = "continue" ]; then
                  LogIt "W" "$obj already exists, trying to modify -Mq"
                  LoadConfigFile "$resFile" "-Mq"
                  ret=$?
                  return $ret
               else
                  LogIt "I" "$obj already exists, accepted"
                  return 0
               fi
            ;;
            'Subordinated cluster queue'*)
               # is is no error case that would require an abort
               obj=`echo $resMsg | awk '{print $4}' | awk -F\" '{ print $2}'`
               LogIt "W" "Non-existing subordinated queue $obj encountered, creating dummy queue [REPEAT REQUIRED]"
               $QCONF -sq | sed "s/^qname.*/qname                    $obj/g" > ${DIR}/queue.tmp 2>/dev/null
               $QCONF -Aq ${DIR}/queue.tmp >/dev/null 2>&1
               rm -f ${DIR}/queue.tmp
               repeat=1
               return 1
            ;;
         esac
      ;;
      -Arqs)
         case "$resMsg" in
            *'already exists')
               if [ "$ON_ERROR" = "abort" ]; then
                  LogIt "C" "Aborting on error as requested: RQS $obj already exists"
                  EXIT 1
               elif [ "$ON_ERROR" = "continue" ]; then
                  LogIt "W" "$obj already exists, trying to modify -Mrqs"
                  LoadConfigFile "$resFile" "-Mrqs"
                  ret=$?
                  return $ret
               else
                  LogIt "I" "$obj already exists, accepted"
                  return 0
               fi
            ;;
         esac
      ;;
      -Mq)
         case "$resMsg" in
            # is is no error case that would require an abort
            'Subordinated cluster queue'*)
               obj=`echo $resMsg | awk '{print $4}' | awk -F\" '{ print $2}'`
               LogIt "W" "Non-existing subordinated queue $obj encountered, creating dummy queue [REPEAT REQUIRED]"
               $QCONF -sq | sed "s/^qname.*/qname                    $obj/g" > ${DIR}/queue.tmp 2>/dev/null
               $QCONF -Aq ${DIR}/queue.tmp >/dev/null 2>&1
               rm -f ${DIR}/queue.tmp
               repeat=1
               return 1
            ;;
         esac
      ;;
      -Mconf)
         case "$resMsg" in
            *'will not be effective before sge_execd restart'*)
               #regular upgrade message
               return 0
            ;;
         esac
      ;;
      -Mc)
         case "$resMsg" in
            *'to complex entry list'*)
               # success message for adding complex entries
               return 0
            ;;
            *'from complex list'*)
               # success message for removal of complex entries
               return 0
            ;;
            *'has not been changed'*)
               LogIt "I" "empty output from -Mc option accepted"
               return 0
            ;;
         esac
      ;;
   esac

   case "$resMsg" in
      *'unknown attribute name'*)
         if [ "$ON_ERROR" = "abort" ] || [ "$ON_ERROR" = "cont_if_exist" ]; then
            LogIt "C" "Aborting on error as requested: unknown attribute $obj"
            EXIT 1
         elif [ "$ON_ERROR" = "continue" ]; then
            RemoveLineWithMatch ${resFile} ${obj} ""
            LogIt "I" "$obj attribute was removed, trying again"
            LoadConfigFile "$resFile" "$resOpt"
            ret=$?
            return $ret
         fi
      ;;
      *'added'*)
         LogIt "I" "added $obj accepted"
         addedConf=1
         return 0
      ;;
      *'modified'*)
         LogIt "I" "modified $obj accepted"
         return 0
      ;;
      *'changed'*)
         LogIt "I" "changed $obj accepted"
         return 0
      ;;
      *'does not exist')
         #some object doesnot exists, must be reloaded
         LogIt "W" "$obj object does not exist. [REPEAT REQUIRED]"
         repeat=1
         return 1
      ;;
   esac
   return $ret
}

#Import item to file
LoadConfigFile()
{
   loadFile="${1:?Need the file name}"
   loadOpt="${2:?Need an option}"

   #do not load empty files
   if [ -f "$loadFile" -a ! -s "$loadFile" ]; then
      LogIt "I" "File $loadFile is empty. Skipping ..."
      return 0
   fi

   if [ "${configLevel:=1}" -gt 20 ]; then
   	LogIt "C" "Too deep in Load Config File"
	   EXIT 1
   fi

   configLevel=`expr ${configLevel} + 1`

   loadMsg=`$QCONF $loadOpt $loadFile 2>&1`

   ResolveResult "$loadOpt" "$loadFile" "$loadMsg" "$ret"
   ret=$?

   if [ "$ret" != "0" ]; then
      errorMsg="Load operation failed: qconf $loadOpt $loadFile -> $loadMsg"
      LogIt "W" "$errorMsg"
   fi

   configLevel=`expr ${configLevel} - 1`
   return $ret
}


#Import list of objects or directory of the objects
LoadListFromLocation()
{
   loadLoc="${1:?Need the location}"
   qconfOpt="${2:?Need an option}"

   failed=0

   for finished in `echo "$SUCCEEDED_LOADLOC" | awk '{for (i=1; i<=NF ; i++) print $i}'`; do
      if [ "$finished" = "$loadLoc" ]; then
         LogIt "I" "qconf $qconfOpt $loadLoc skipped because succeeded already in previous run"
         return 0
      fi
   done

   LogIt "I" "qconf $qconfOpt $loadLoc"

   #File list
   if [ -f "$loadLoc" ]; then
      list=`$CAT $loadLoc`
      if [ -z "$list" ]; then
	 return
      fi

      for item in $list; do
         LoadConfigFile $item $qconfOpt
         if [ $? -ne 0 ]; then
            failed=1
         fi
      done
   #Directory list is not empty
   elif [ -d "$loadLoc" ]; then
      llList=`ls -1 ${loadLoc}`
      if [ -z "$llList" ]; then
         return
      fi

      for item in ${loadLoc}/*; do
         #we prefer full file names
         full=`ls $item`
         LoadConfigFile $full $qconfOpt
         if [ $? -ne 0 ]; then
            failed=1
         fi
      done
   else
      #Not a file or directory (skip)
      #errorMsg="wrong directory or file: $loadLoc"
      #LogIt "W" "$errorMsg"
      :
   fi

   if [ $failed -eq  0 ]; then
      SUCCEEDED_LOADLOC="$SUCCEEDED_LOADLOC $loadLoc"
   fi

   return $ret
}


#All SGE objects
LoadConfigurations()
{
   dir=${1:?}
   # There are the add,Load oprtions
   #     -Aattr obj_spec fname obj_instance,...   <add to object attributes>
   #     -aattr obj_spec attr_name val obj_instance,...
   #     -astnode node_path=shares,... <add share tree node>

   # -ah hostname,... <add administrative host>
   LoadListFromLocation "$dir/admin_hosts" "-ah"

   # -as hostname,... <add submit hosts>
   LoadListFromLocation "$dir/submit_hosts" "-as"

   # -am user,... <add managers>
   LoadListFromLocation "$dir/managers" "-am"

   # -ao user,... <add operators>
   LoadListFromLocation "$dir/operators" "-ao"

   # -Mc fname <modify complex>
   LoadConfigFile "$dir/centry" "-Mc"

   # -Ae fname    <add execution host>
   LoadListFromLocation "$dir/execution" "-Ae"

   # -Acal fname <add calendar>
   LoadListFromLocation "$dir/calendars" "-Acal"

   # -Ackpt fname <add ckpt. environment>
   LoadListFromLocation "$dir/ckpt" "-Ackpt"

   # -Ahgrp file <add host group config>
   LoadListFromLocation "$dir/hostgroups" "-Ahgrp"

   # -Auser fname <add user>
   LoadListFromLocation "$dir/users" "-Auser"

   # -Au fname   <add an ACL>
   LoadListFromLocation "$dir/usersets" "-Au"

   # -Aprj fname <add new project>
   LoadListFromLocation "$dir/projects" "-Aprj"

   # -Ap fname <add PE configuration>
   LoadListFromLocation "$dir/pe" "-Ap"

   # -Aq fname  <add new queue>
   LoadListFromLocation "$dir/cqueues" "-Aq"

   # -Arqs fname <add RQS configuration>
   LoadListFromLocation "$dir/resource_quotas" "-Arqs"

   # -Aconf file_list  <add configurations>
   LoadListFromLocation "$dir/configurations" "-Aconf"

   # -Astree fname  <add share tree>
   LoadConfigFile "$dir/sharetree" "-Astree"

   # -Msconf  fname  <modify  scheduler   configuration
   LoadConfigFile "$dir/schedconf" "-Msconf"
}


#Load one all the configurations
LoadOnce()
{
   dir=${1:?}

   #clean added new configuration
   addedConf=0
   #clean the error code
   errorMsg=''

   LoadConfigurations "$dir"

   # no added configuration, stop to repeat
   if [ $addedConf = 0 ]; then
      repeat=0
   fi
}


#Reload the configuration till there is nothing to add
IterativeLoad()
{
   dir=${1:?}
   repeat=0
   loadLevel=1
   errorMsg=''
   LoadOnce "$dir"
   while [ $repeat -eq  1 ]; do
      loadLevel=`expr ${loadLevel} + 1`
      if [ "${loadLevel}" -gt 10 ]; then
         LogIt "C" "Too deep in Load Level"
         EXIT 1
      fi
      LogIt "W" "[REPEAT LOAD]"
      LoadOnce "$dir"
   done

   if [ -n "$errorMsg" ]; then
      LogIt "C" "$errorMsg"
      EXIT 1
   fi
}

EXIT() {
   exit "$1"
}

# CS-2394: since 9.2 the access list (userset) names "manager" and "operator" are
# reserved - they hold the manager and operator lists of the cluster. A cluster
# older than 9.2 may contain a user-defined access list of that name. It cannot be
# carried over automatically: everything that references it (user_lists/xuser_lists
# of queues, hosts, parallel environments and the cluster configuration, acl/xacl of
# projects, resource quota sets) would silently resolve to the reserved list after
# the upgrade, and with that to different access rights. Renaming it here would
# additionally be ambiguous, because user names and access list names share one
# namespace and user names take precedence.
#
# Refuse to load such a configuration. This runs before any object is loaded, so
# nothing has been changed in the new cluster when we abort.
#
# Note: loading a manager/operator access list from a 9.2 or newer backup is fine -
# there they ARE the reserved lists.
#
#   $1 - the backup directory (as saved by save_config.sh)
CheckReservedAccessListNames()
{
   dir=$1

   # $LOAD_VERSION is the version of the saved cluster, e.g.
   # "GCS 9.1.0beta1 (210226-1224)". Only a pre-9.2 backup can carry a
   # user-defined access list named manager/operator.
   ver=`echo "$LOAD_VERSION" | awk '{print $2}'`
   ver_major=`echo "$ver" | cut -d. -f1 | tr -cd '0-9'`
   ver_minor=`echo "$ver" | cut -d. -f2 | tr -cd '0-9'`
   if [ -n "$ver_major" ] && [ -n "$ver_minor" ]; then
      if [ "$ver_major" -gt 9 ] || { [ "$ver_major" -eq 9 ] && [ "$ver_minor" -ge 2 ]; }; then
         return 0
      fi
   fi

   for acl in manager operator; do
      if [ -f "$dir/usersets/$acl" ]; then
         $INFOTEXT ""
         $INFOTEXT "[CRITICAL] The saved configuration contains a user-defined access list"
         $INFOTEXT "named \"$acl\" (saved from version: $LOAD_VERSION)."
         $INFOTEXT ""
         $INFOTEXT "Beginning with version 9.2 the access list names \"manager\" and \"operator\""
         $INFOTEXT "are reserved: they hold the manager and operator lists of the cluster."
         $INFOTEXT ""
         $INFOTEXT "Rename the access list \"$acl\" in the old cluster and adapt everything that"
         $INFOTEXT "references it (user_lists/xuser_lists of queues, hosts, parallel environments"
         $INFOTEXT "and the cluster configuration, acl/xacl of projects, resource quota sets),"
         $INFOTEXT "save the configuration again, then repeat the upgrade."
         $INFOTEXT ""
         $INFOTEXT "Nothing has been loaded. The upgrade is aborted."
         LogIt "C" "Backup contains a user-defined access list named \"$acl\" - name is reserved since 9.2"
         EXIT 1
      fi
   done
   return 0
}

# CS-2438: a pre-9.2 cluster may own a user-defined host group named @admin_hosts,
# @submit_hosts or @exec_hosts. Since 9.2 those three names are reserved: they hold
# the admin and submit host lists of the cluster, and @exec_hosts mirrors the
# execution host list.
#
# Loading such a group would silently hand a security-relevant list to whatever the
# administrator happened to put in it - every reference to the group (queue host
# lists, "qconf -mhgrp" nesting, resource quota scopes) keeps resolving, but now to
# the reserved group, and the qmaster adds itself to @admin_hosts on top. The same
# hazard CheckReservedAccessListNames() closes for manager/operator.
#
# Refuse to load such a configuration. This runs before any object is loaded, so
# nothing has been changed in the new cluster when we abort.
#
# Note: loading these host groups from a 9.2 or newer backup is fine - there they
# ARE the reserved groups.
#
#   $1 - the backup directory (as saved by save_config.sh)
CheckReservedHostGroupNames()
{
   dir=$1

   # $LOAD_VERSION is the version of the saved cluster, e.g.
   # "GCS 9.1.0beta1 (210226-1224)". Only a pre-9.2 backup can carry a
   # user-defined host group under one of the reserved names.
   ver=`echo "$LOAD_VERSION" | awk '{print $2}'`
   ver_major=`echo "$ver" | cut -d. -f1 | tr -cd '0-9'`
   ver_minor=`echo "$ver" | cut -d. -f2 | tr -cd '0-9'`
   if [ -n "$ver_major" ] && [ -n "$ver_minor" ]; then
      if [ "$ver_major" -gt 9 ] || { [ "$ver_major" -eq 9 ] && [ "$ver_minor" -ge 2 ]; }; then
         return 0
      fi
   fi

   for hgrp in @admin_hosts @submit_hosts @exec_hosts; do
      if [ -f "$dir/hostgroups/$hgrp" ]; then
         $INFOTEXT ""
         $INFOTEXT "[CRITICAL] The saved configuration contains a user-defined host group"
         $INFOTEXT "named \"$hgrp\" (saved from version: $LOAD_VERSION)."
         $INFOTEXT ""
         $INFOTEXT "Beginning with version 9.2 the host group names \"@admin_hosts\","
         $INFOTEXT "\"@submit_hosts\" and \"@exec_hosts\" are reserved: they hold the admin and"
         $INFOTEXT "submit host lists of the cluster, and @exec_hosts mirrors the execution host"
         $INFOTEXT "list."
         $INFOTEXT ""
         $INFOTEXT "Rename the host group \"$hgrp\" in the old cluster and adapt everything that"
         $INFOTEXT "references it (host lists of queues, nested host groups, resource quota"
         $INFOTEXT "scopes), save the configuration again, then repeat the upgrade."
         $INFOTEXT ""
         $INFOTEXT "Nothing has been loaded. The upgrade is aborted."
         LogIt "C" "Backup contains a user-defined host group named \"$hgrp\" - name is reserved since 9.2"
         EXIT 1
      fi
   done
   return 0
}

# CS-2450: the primary name of a configuration object must not contain any of the
# characters that make up a wildcard expression - * ? [ ] & | ! ( ). Object names
# and the references pointing at them share one namespace, and wildcards are a
# legal means of *referencing* objects (resource quota scopes, "-q" queue patterns,
# "qsub -pe"), so an object whose name carries such a character cannot be addressed
# unambiguously - it resolves to different host/object sets depending on which code
# path does the resolution.
#
# Since 9.2 such a name is rejected when the object is added, so a backup taken from
# an older cluster may still contain one. Loading it would fail somewhere in the
# middle of the run with a message that does not explain the cause, leaving the new
# cluster half populated. Report every offender up front and refuse instead.
#
# Unlike CheckReservedAccessListNames() this is deliberately NOT gated on the saved
# version: a 9.2 cluster installed before this change can contain such names too.
#
#   $1 - the backup directory (as saved by save_config.sh)
CheckPatternObjectNames()
{
   dir=$1
   found=false

   # An object name IS the file name in the backup (see DumpItemToFile() in
   # save_config.sh), and the characters searched for here are exactly the ones the
   # shell expands. Switch pathname expansion off for the whole scan so that no name
   # can ever be interpreted as a glob.
   set -f

   # "<directory written by save_config.sh>:<what the object is called for a human>"
   for entry in \
      hostgroups:"host group" \
      cqueues:"cluster queue" \
      pe:"parallel environment" \
      usersets:"access list" \
      users:"user" \
      projects:"project" \
      calendars:"calendar" \
      ckpt:"checkpointing interface" \
      resource_quotas:"resource quota set"
   do
      subdir=`expr "$entry" : '\([^:]*\)'`
      label=`expr "$entry" : '[^:]*:\(.*\)'`

      if [ ! -d "$dir/$subdir" ]; then
         continue
      fi

      # An object name cannot contain whitespace - it is rejected on creation - so
      # splitting the listing on whitespace is safe here.
      for name in `ls -A -- "$dir/$subdir" 2>/dev/null | grep '[][*?&|!()]'`; do
         if [ "$found" = false ]; then
            $INFOTEXT ""
            $INFOTEXT "[CRITICAL] The saved configuration contains object names with wildcard"
            $INFOTEXT "expression characters (saved from version: $LOAD_VERSION):"
            $INFOTEXT ""
            found=true
         fi
         $INFOTEXT "   $label \"$name\""
         LogIt "I" "Backup contains $label \"$name\" carrying wildcard expression characters"
      done
   done

   set +f

   if [ "$found" = true ]; then
      $INFOTEXT ""
      $INFOTEXT "Beginning with version 9.2 the characters * ? [ ] & | ! ( ) are not allowed in"
      $INFOTEXT "the name of a configuration object. They are reserved for referencing objects:"
      $INFOTEXT "resource quota scopes, \"-q\" queue patterns and \"qsub -pe\" keep accepting"
      $INFOTEXT "wildcards, so a name containing one of them cannot be addressed unambiguously"
      $INFOTEXT "and does not resolve consistently."
      $INFOTEXT ""
      $INFOTEXT "Rename the objects listed above in the old cluster and adapt everything that"
      $INFOTEXT "references them, save the configuration again, then repeat the upgrade."
      $INFOTEXT ""
      $INFOTEXT "Nothing has been loaded. The upgrade is aborted."
      LogIt "C" "Backup contains object names with wildcard expression characters - upgrade aborted"
      EXIT 1
   fi
   return 0
}

########
# MAIN #
########
if [ "$1" = -help -o $# -eq 0 ]; then
   Usage
   exit 0
fi

DIR="${1:?The load directory is required}"
LOG_FILE_NAME="${2:?A log file path and name is required}"
shift 2

LOGGER_LEVEL="W"
mode=upgrade
newIJS=false
EXECD_SPOOL_DIR=""
ADMIN_MAIL=""
GID_RANGE=""

DATE=`date '+%Y-%m-%d_%H:%M:%S'`

MESSAGE_FILE_NAME="/tmp/load_config_${DATE}.log"

ARGC=$#
while [ $ARGC -gt 0 ]; do
   case $1 in
      -log)
         shift
         if [ "$1" != "C" -a "$1" != "W" -a "$1" != "I" ]; then
            LogIt "W" "LOAD invoked with invalid log level "$1" using W"
         else
            LOGGER_LEVEL="$1"
         fi
         ;;
      -mode)
         shift
	 if [ "$1" != "upgrade" -a "$1" != "copy" ]; then
            LogIt "W" "LOAD invoked with invalid mode "$1" using $mode"
         else
            LogIt "I" "LOAD invoked with -mode $1"
            mode="$1"
         fi
	 ;;
      -newijs)
         shift
         if [ "$1" != "true" -a "$1" != "false" ]; then
            LogIt "W" "LOAD invoked with invalid newijs "$1" using $newIJS"
         else
            LogIt "I" "LOAD invoked with -newijs true"
            newIJS="$1"
         fi
         ;;
      -execd_spool_dir)
         shift
         LogIt "I" "LOAD invoked with -execd_spool_dir $1"
         EXECD_SPOOL_DIR="$1"
         ;;
      -admin_mail)
         shift
         LogIt "I" "LOAD invoked with -admin_mail $1"
         ADMIN_MAIL="$1"
         ;;
      -gid_range)
         shift
         LogIt "I" "LOAD invoked with -gid_range $1"
         GID_RANGE="$1"
         ;;
      -on_error)
         shift
         if [ "$1" != "abort" -a "$1" != "continue" -a "$1" != "cont_if_exist" ]; then
            LogIt "C" "LOAD invoked with -on_error $1"
            EXIT 1
         else
            LogIt "I" "LOAD invoked with -on_error $1"
            ON_ERROR="$1"
         fi
         ON_ERROR="$1"
         ;;
      *)
         echo "Invalid argument \'$1\'"
         Usage
         exit 1
         ;;
   esac
   shift
   ARGC=`expr $ARGC - 2`
done

CURRENT_VERSION=`$QCONF -help | sed  -n '1,1 p'` 2>&1
ret=$?
if [ "$ret" != "0" ]; then
   $INFOTEXT "ERROR: qconf -help failed"
   LogIt "C" "qmaster is not installed"
   EXIT 1
fi
admin_hosts=`$QCONF -sh 2>/dev/null`
if [ -z "$admin_hosts" ]; then
   $INFOTEXT "ERROR: qconf -sh failed. Qmaster is probably not running?"
   LogIt "C" "qmaster is not running"
   EXIT 1
fi
tmp_adminhost=`$QCONF -sh | grep "^${HOST}$"`
if [ "$tmp_adminhost" != "$HOST" ]; then
   $INFOTEXT "ERROR: Load must be started on admin host (qmaster host recommended)."
   LogIt "C" "Can't start load_config.sh on $HOST: not an admin host"
   EXIT 1
fi

# Verify that we were pointed at a saved configuration at all. Without this the
# empty LOAD_VERSION below reaches LogIt(), whose "${2:?Message is required}"
# terminates the script mid-logging with a message that names neither the
# directory nor the problem (CS-2470).
if [ ! -d "$DIR" ]; then
   $INFOTEXT "ERROR: \"$DIR\" does not exist."
   LogIt "C" "Backup directory does not exist: $DIR"
   EXIT 1
fi
if [ ! -f "$DIR/version" ]; then
   $INFOTEXT "ERROR: \"$DIR\" is not a saved cluster configuration - the file \"version\" is missing."
   $INFOTEXT "Pass the directory that save_config.sh wrote."
   LogIt "C" "Not a saved cluster configuration, no version file: $DIR"
   EXIT 1
fi

LOAD_VERSION=`cat ${DIR}/version`
LogIt "I" "LOAD $DIR"
LogIt "I" "$CURRENT_VERSION"
LogIt "I" "$LOAD_VERSION"

# CS-2394: refuse a pre-9.2 backup that uses the now-reserved access list names.
# Must run before IterativeLoad, i.e. before anything is loaded.
CheckReservedAccessListNames "${DIR}"

# CS-2438: refuse a pre-9.2 backup that uses the now-reserved host group names.
# Must run before IterativeLoad, i.e. before anything is loaded.
CheckReservedHostGroupNames "${DIR}"

# CS-2450: refuse a backup whose object names carry wildcard expression characters.
# Must run before IterativeLoad, i.e. before anything is loaded.
CheckPatternObjectNames "${DIR}"

$INFOTEXT "Loading saved cluster configuration from $DIR (log in $MESSAGE_FILE_NAME)..."

IterativeLoad "${DIR}"

LogIt "I" "LOADING FINISHED"
$INFOTEXT "Done"
EXIT 0
