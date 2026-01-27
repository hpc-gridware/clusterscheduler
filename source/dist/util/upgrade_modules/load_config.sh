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

ReplaceLineWithMatch()
{
   repFile="${1:?Need the file name to operate}"
   repExpr="${2:?Need an expression, where to replace}"
   replace="${3:?Need the replacement text}"

   #Return if no match
   grep ${repExpr} $repFile >/dev/null 2>&1
   if [ $? -ne 0 ]; then
      return
   fi
   SEP="|"
   echo "$repExpr $replace" | grep "|" >/dev/null 2>&1
   if [ $? -eq 0 ]; then
      echo "$repExpr $replace" | grep "%" >/dev/null 2>&1
      if [ $? -ne 0 ]; then
         SEP="%"
      else
         echo "$repExpr $replace" | grep "?" >/dev/null 2>&1
         if [ $? -ne 0 ]; then
            SEP="?"
         else
            LogIt "C" "$repExpr $replace contains |,% and ? character cannot use sed"
         fi
      fi
   fi
   #We need to change the file
   sed -e "s${SEP}${repExpr}${SEP}${replace}${SEP}g" "$repFile" > "${repFile}.tmp"
   mv -f "${repFile}.tmp"  "${repFile}"
}

#Modify before load
ModifyData()
{
   modOpt="${1:?An option is required}"
   modFile="${2:?The file name is required}"
   #echo "ModifyData opt:$modOpt file:$modFile"

   #test only, comment in production
   case "$modOpt" in
      -Ae)
         #FlatFile ${modFile}
         RemoveLineWithMatch ${modFile} "" 'load_values.*'
         RemoveLineWithMatch ${modFile} "" 'processors.*'
      ;;
   esac

   return $ret
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
         case "$resMsg" in
            *'already exists')
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
            '')
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
            RemoveLineWithMatch ${resFile} "" ${obj}
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


   ModifyData "$loadOpt" "$loadFile"
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
      errorMsg="wrong directory or file: $loadLoc"
      LogIt "W" "$errorMsg"
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

LOAD_VERSION=`cat ${DIR}/version`
LogIt "I" "LOAD $DIR"
LogIt "I" "$CURRENT_VERSION"
LogIt "I" "$LOAD_VERSION"

$INFOTEXT "Loading saved cluster configuration from $DIR (log in $MESSAGE_FILE_NAME)..."

IterativeLoad "${DIR}"

LogIt "I" "LOADING FINISHED"
$INFOTEXT "Done"
EXIT 0
