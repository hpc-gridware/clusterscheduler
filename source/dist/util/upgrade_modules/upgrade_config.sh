#!/bin/sh
#___INFO__MARK_BEGIN_NEW__
###########################################################################
#
#  Copyright 2026 HPC-Gridware GmbH
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
###########################################################################
#___INFO__MARK_END_NEW__

INFOTEXT=echo
CAT=cat
MKDIR=mkdir
LS=ls
SCRIPTNAME=`basename "$0"`

if [ -z "$SGE_ROOT" -o -z "$SGE_CELL" ]; then
   $INFOTEXT "Set your SGE_ROOT, SGE_CELL first!"
   exit 1
fi
cd $SGE_ROOT

ROOTDIR=$SGE_ROOT
CELLDIR=$SGE_CELL
ARCH=`$ROOTDIR/util/arch`
QCONF=$ROOTDIR/bin/$ARCH/qconf
QCONF_NAME="qconf"
QMASTER_NAME="qmaster"
PRODUCT_SHORTCUT="sge"

HOST=`$ROOTDIR/utilbin/$ARCH/gethostname -aname`

. $ROOTDIR/util/install_modules/inst_common.sh
BasicSettings
GetAdminUser

###############################################################################
# Helper functions
###############################################################################

Usage()
{
   myname=`basename $0`
   $INFOTEXT "Usage: $myname <dir> <log_file> <date>\n" \
             "         [-admin_mail <value>]\n" \
             "         [-gid_range <integer_range_value>]\n" \
             "         [-help]\n" \
             "         [-execd_spool_dir <value>]\n" \
             "         [-mode upgrade|copy]\n" \
             "         [-log I|W|C]\n" \
             "         [-newijs true|false]\n" \
             "\n" \
             "   date      timestamp to be used by this script\n" \
             "   dir       location of copy of the cluster configuration backup\n" \
             "   log_file  log_file to store the log messages\n" \
             "\n"
}

EXIT() {
   exit "$1"
}

# All logging is done by this functions
LogIt()
{
   urgency="${1:?Urgency is required [I,W,C]}"
   message="${2:?Message is required}"

   #log file contains all messages
   echo "${urgency} $message" >> $LOG_FILE_NAME
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

#################################################################################
# Version comparison functions
#################################################################################

VersionString2Product() {
   version_string="${1:?Missing version string}"

   product=$(echo $version_string | sed -n 's/\([a-zA-Z]*\) \([0-9.]*\)\(.*\)/\1/p')
   echo $product
}

VersionString2VersionNumber() {
   version_string="${1:?Missing version string}"

   version_number=$(echo "$version_string" | sed -n 's/\([a-zA-Z]*\) \([0-9.]*\)\(.*\)/\2/p')
   echo "$version_number"
}

VersionString2Suffix() {
   version_string="${1:?Missing version string}"

   suffix=$(echo "$version_string" | sed -n 's/\([a-zA-Z]*\) \([0-9.]*\)\(.*\)/\3/p')
   echo "$suffix"
}

TableCoordinates2Value() {
   table_name="${1:?Missing table name parameter}"
   line="${2:?Missing line parameter}"
   column="${3:?Missing column parameter}"

   table_name=$(echo "$table_name" | tr '[:upper:]' '[:lower:]')
   filename="$SGE_ROOT/util/upgrade_modules/upgrade_table_${table_name}.csv"
   field=$(grep -v '^#' "$filename" | sed -n "${line}p"  | cut -d";" -f "$column")
   echo "$field"
}

Table2Field() {
   table_name="${1:?Missing table name parameter}"
   version_string="${2:?Missing version string}"
   column="${3:?Missing column parameter}"

   version_number=$(VersionString2VersionNumber "$version_string")

   field=0
   do_loop="true"
   i=1
   while [ $do_loop = "true" ]; do
      version_name=$(TableCoordinates2Value "$table_name" "$i" 1)

      if [ -z "$version_name" ]; then
         do_loop="false"
         continue
      else
         table_version_number=$(VersionString2VersionNumber "$version_name")
         if [ "$version_number" = "$table_version_number" ]; then
            if [ "$column" -eq 0 ]; then
               # return the line number for column 0
               field=$i
            else
               field=$(TableCoordinates2Value "$table_name" "$i" $column)
            fi
            do_loop="false"
         else
            i=$(expr "$i" + 1)
         fi
      fi
   done

   echo "$field"
}

Table2VersionInteger() {
   table_name="${1:?Missing table name parameter}"
   version_string="${2:?Missing version string}"

   version_integer=$(Table2Field "$table_name" "$version_string" 2)
   echo "$version_integer"
}

Table2LineNumber() {
   table_name="${1:?Missing table name parameter}"
   version_string="${2:?Missing version string}"

   line_number=$(Table2Field "$table_name" "$version_string" 0)
   echo "$line_number"
}

## @brief Determine if an upgrade or downgrade is necessary
#
# @param[in] $1   current_version: current version string, e.g. "GCS 9.1.0alpha1"
# @param[in] $2   target_version: target version string
# @param[in] $3   value_type_to_return:
#                    0=return upgrade/downgrade code,
#                    1=return next version string
# @return   0 if both versions are equal
#           1 if an upgrade is necessary
#           2 if a downgrade is necessary
#          10 if products do not match
#          11 if current version is unknown
#          12 if target version is unknown
#
VersionString2Next() {
   current_version="${1:?Missing current version string}"
   target_version="${2:?Missing target version string}"
   value_type_to_return="${3:?Missing value type}"

   # Find current version details
   current_product=`VersionString2Product "$current_version"`
   current_integer=`Table2VersionInteger "$current_product" "$current_version"`
   current_line_number=`Table2LineNumber "$current_product" "$current_version"`

   # Find target version details
   target_product=`VersionString2Product "$target_version"`
   target_integer=`Table2VersionInteger "$target_product" "$target_version"`

   # Initialize return values with defaults
   next_version=0
   next_version_integer=0
   do_upgrade_downgrade_ret=0

   # Find product mismatch
   if [ "$current_product" != "$target_product" ]; then
      if [ "$current_product" = "OCS" ] && [ "$target_product" = "GCS" ]; then
         # Transition from OCS to GCS detected
         :
      else
         # Cannot upgrade/downgrade between different products
         do_upgrade_downgrade_ret=10
      fi
   fi

   # Find unknown product versions or skip if already an error
   if [ $do_upgrade_downgrade_ret -eq 0 ]; then
      if [ -z "$current_integer" ] || [ "$current_integer" -eq 0 ]; then
         # Current version not found in the list of known versions
         do_upgrade_downgrade_ret=11
      fi
      if [ -z "$target_integer" ] || [ "$target_integer" -eq 0 ]; then
         # Target version not found in the list of known versions
         do_upgrade_downgrade_ret=12
      fi
   fi

   # Determine upgrade or downgrade or no action or skip if already an error
   if [ $do_upgrade_downgrade_ret -eq 0 ]; then
      current_int=$((current_integer + 0))
      target_int=$((target_integer + 0))
      if [ $current_int -lt $target_int ]; then
         # Performing upgrade from '$current_version' to next version '$next_version'
         do_upgrade_downgrade_ret=1
         line_number=$(expr "$current_line_number" + 1)
         next_version=$(TableCoordinates2Value "$current_product" "$line_number" 1)
         next_version_integer=$(TableCoordinates2Value "$current_product" "$line_number" 2)
      elif [ $current_int -gt $target_int ]; then
         # Performing upgrade from '$current_version' to previous version '$next_version'
         do_upgrade_downgrade_ret=2
         line_number=$(expr "$current_line_number" - 1)
         next_version=$(TableCoordinates2Value "$current_product" "$line_number" 1)
         next_version_integer=$(TableCoordinates2Value "$current_product" "$line_number" 2)
      else
         do_upgrade_downgrade_ret=0
      fi
   fi

   if [ "$value_type_to_return" -eq 0 ]; then
      return "$do_upgrade_downgrade_ret"
   elif [ "$value_type_to_return" -eq 1 ]; then
      echo "$next_version"
      return "$do_upgrade_downgrade_ret"
   else
      echo "$next_version_integer"
      return "$do_upgrade_downgrade_ret"
   fi
}

TriggerUpOrDowngradeFunc() {
   current_version="${1:?Missing current version string}"
   target_version="${2:?Missing target version string}"
   do_upgrade="${3:?Missing upgrade/downgrade parameter}"

   next_version=$(VersionString2Next "$current_version" "$target_version" 1)
   next_version_integer=$(VersionString2Next "$current_version" "$target_version" 2)
   function_name="UpOrDowngradeTo${next_version_integer}"
   if type "$function_name" >/dev/null 2>&1; then
      LogIt "I" "Starting $function_name to change '$current_version' to '$next_version'."
      $function_name "$do_upgrade" "${DIR}"
      if [ $? -eq 0 ]; then
         echo "$next_version" > "${working_dir}/version"
      else
         LogIt "C" "Upgrade step failed in function '$function_name'."
         return 1
      fi
   else
      LogIt "I" "No function '$function_name' found for change from '$current_version' to '$next_version'."
   fi
}

TriggerUpOrDowngradeStep() {
   current_version="${1:?Missing current version string}"
   target_version="${2:?Missing target version string}"

   VersionString2Next "$current_version" "$target_version" 0
   do_upgrade_downgrade_ret=$?
   if [ $do_upgrade_downgrade_ret -eq 0 ]; then
      LogIt "I" "Final target version '$target_version' reached."
      return 99
   elif [ $do_upgrade_downgrade_ret -eq 1 ]; then
      TriggerUpOrDowngradeFunc "$current_version" "$target_version" 1
      return $?
   elif [ $do_upgrade_downgrade_ret -eq 2 ]; then
      TriggerUpOrDowngradeFunc "$current_version" "$target_version" 0
      return $?
   else
      LogIt "C" "No upgrade step available from '$current_version' to version '$target_version'."
      EXIT 1
   fi
}

TriggerUpOrDowngrade() {
   current_version="${1:?Missing current version string}"
   target_version="${2:?Missing target version string}"
   working_dir="${2:?Missing working dir parameter}"

   do_loop="true"
   while [ $do_loop = "true" ] && [ "$target_version" != "$current_version" ] ; do
      next_version=$(VersionString2Next "$current_version" "$target_version" 1)
      TriggerUpOrDowngradeStep "$current_version" "$target_version"
      result=$?
      if [ $result -eq 99 ]; then
         do_loop="false"
         return 0
      elif [ $result -ne 0 ]; then
         do_loop="false"
         return $result
      fi
      current_version="$next_version"
   done
   UpOrDowngradeGeneral working_dir
   return $?
}

#################################################################################
# Upgrade functions
#################################################################################

UpOrDowngradeGeneral()
{
   working_dir="${1:?Missing working dir parameter}"

   LogIt "I" "Performing general tasks"

   # EXECUTION HOST CONFIG
   # files contain attributes that cannot be set during upgrade.
   # These attributes are dynamic and get their values from the execution daemon at runtime.
   for item in "$working_dir/execution/"*; do
      RemoveLineWithMatch ${working_dir}/execution/$item "" 'load_values.*'
      RemoveLineWithMatch ${working_dir}/execution/$item "" 'processors.*'
   done

   # CONFIGURATION OBJECTS
   for filename in "$working_dir/configurations/"*; do
      file="$working_dir/configurations/$filename"

      if [ "$file" = "global" ]; then
         # Enable interactive job support (builtin)
         if [ "$newIJS" = true ]; then
            ReplaceOrAddLine "${file}" 'qlogin_command.*' "qlogin_command builtin"
            ReplaceOrAddLine "${file}" 'qlogin_daemon.*'  "qlogin_daemon builtin"
            ReplaceOrAddLine "${file}" 'rlogin_command.*' "rlogin_command builtin"
            ReplaceOrAddLine "${file}" 'rlogin_daemon.*'  "rlogin_daemon builtin"
            ReplaceOrAddLine "${file}" 'rsh_command.*'    "rsh_command builtin"
            ReplaceOrAddLine "${file}" 'rsh_daemon.*'     "rsh_daemon builtin"
         fi

         # Set new execd spool dir if provided
         if [ -n "$EXECD_SPOOL_DIR" ]; then
            ReplaceOrAddLine "${file}" 'execd_spool_dir.*' "execd_spool_dir $EXECD_SPOOL_DIR"
         fi

         # Set gid_range if provided
         if [ -n "$GID_RANGE" ]; then
            ReplaceOrAddLine "${file}" 'gid_range.*' "gid_range $GID_RANGE"
         fi

         # Set administrator_mail if provided
         if [ -n "$ADMIN_MAIL" ]; then
            ReplaceOrAddLine "${file}" 'administrator_mail.*' "administrator_mail $ADMIN_MAIL"
         fi

         ReplaceOrAddLine ${file} 'max_advance_reservations.*' "max_advance_reservations 0"
      else
         # Remove local settings for interactive job support
         if [ "$newIJS" = true ]; then
            RemoveLineWithMatch ${file} "" 'qlogin_command.*'
            RemoveLineWithMatch ${file} "" 'qlogin_daemon.*'
            RemoveLineWithMatch ${file} "" 'rlogin_command.*'
            RemoveLineWithMatch ${file} "" 'rlogin_daemon.*'
            RemoveLineWithMatch ${file} "" 'rsh_command.*'
            RemoveLineWithMatch ${file} "" 'rsh_daemon.*'
         fi

         # Change local execd-spool dirs to unique names in the new cluster
         if [ "$mode" = copy ]; then
            local_dir=`grep execd_spool_dir ${file} 2>/dev/null | awk '{print $2}'`
            if [ -n "$local_dir" ]; then
               local_dir=`dirname $local_dir 2>/dev/null`
               if [ -n "$local_dir" ]; then
                  if [ -n "$SGE_CLUSTER_NAME" ]; then
                     local_dir="${local_dir}/${SGE_CLUSTER_NAME}"
                  elif [ -n "$SGE_QMASTER_PORT" ]; then
                     local_dir="${local_dir}/${SGE_QMASTER_PORT}"
                  else
                     local_dir="${local_dir}/${SGE_CELL}"
                  fi
                  ReplaceOrAddLine ${file} 'execd_spool_dir.*' "execd_spool_dir $local_dir"
               fi
            fi
         fi
      fi
   done

   return 0
}

# shellcheck disable=SC2317
UpOrDowngradeTo90000()
{
   do_upgrade="${1:?Missing upgrade/downgrade parameter}"
   working_dir="${2:?Missing working dir parameter}"

   if [ "$do_upgrade" -eq 1 ]; then
      # NA - First version of OCS/GCS
      :
   else
      LogIt "I" "Downgrade to 90000"
   fi

   return 0
}

# shellcheck disable=SC2317
UpOrDowngradeTo90100() {
   do_upgrade="${1:?Missing upgrade/downgrade parameter}"
   working_dir="${2:?Missing working dir parameter}"

   if [ "$do_upgrade" -eq 1 ]; then
      LogIt "I" "Upgrade to 90100"
   else
      LogIt "I" "Downgrade to 90100"
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

DIR="${1:?The backup directory is required}"
LOG_FILE_NAME="${2:?A log file path and name is required}"
TARGET_VERSION=""

shift 2

ARGC=$#
while [ $ARGC -gt 0 ]; do
   case $1 in
      -target)
         shift
         LogIt "I" "UPGRADE invoked with -target_version $1"
         TARGET_VERSION="$1"
         ;;
      -log)
         shift
         if [ "$1" != "C" -a "$1" != "W" -a "$1" != "I" ]; then
            LogIt "W" "UPGRADE invoked with invalid log level "$1" using W"
         else
            LOGGER_LEVEL="$1"
         fi
         ;;
      -mode)
         shift
         if [ "$1" != "upgrade" -a "$1" != "copy" ]; then
            LogIt "W" "UPGRADE invoked with invalid mode "$1" using $mode"
         else
            LogIt "I" "UPGRADE invoked with -mode $1"
            mode="$1"
         fi
         ;;
      -newijs)
         shift
         if [ "$1" != "true" -a "$1" != "false" ]; then
            LogIt "W" "UPGRADE invoked with invalid newijs "$1" using $newIJS"
         else
            LogIt "I" "UPGRADE invoked with -newijs true"
            newIJS="$1"
         fi
         ;;
      -execd_spool_dir)
         shift
         LogIt "I" "UPGRADE invoked with -execd_spool_dir $1"
         EXECD_SPOOL_DIR="$1"
         ;;
      -admin_mail)
         shift
         LogIt "I" "UPGRADE invoked with -admin_mail $1"
         ADMIN_MAIL="$1"
         ;;
      -gid_range)
         shift
         LogIt "I" "UPGRADE invoked with -gid_range $1"
         GID_RANGE="$1"
         ;;
   esac
   ARGC=`expr $ARGC - 2`
   shift
done

# Find current version
CURRENT_VERSION=$(cat ${DIR}/version)
product=`VersionString2Product "$CURRENT_VERSION"`
version=`VersionString2VersionNumber "$CURRENT_VERSION"`
suffix=`VersionString2Suffix "$CURRENT_VERSION"`
integer=`Table2VersionInteger "$product" "$CURRENT_VERSION"`
LogIt "I" "Current version: product='$product' version='$version' suffix='$suffix' integer='$integer'"


# Determine target version
if [ -z "$TARGET_VERSION" ]; then
   TARGET_VERSION=`$QCONF -help | sed  -n '1,1 p'` 2>&1
   if [ $? -ne 0 ]; then
      LogIt "C" "$QCONF_NAME -help failed"
      LogIt "C" "$QMASTER_NAME is not installed"
      EXIT 1
   fi
fi
product=`VersionString2Product "$TARGET_VERSION"`
version=`VersionString2VersionNumber "$TARGET_VERSION"`
suffix=`VersionString2Suffix "$TARGET_VERSION"`
integer=`Table2VersionInteger "$product" "$TARGET_VERSION"`
LogIt "I" "Target version: product='$product' version='$version' suffix='$suffix' integer='$integer'"

admin_hosts=`$QCONF -sh 2>/dev/null`
if [ -z "$admin_hosts" ]; then
   $INFOTEXT "ERROR: $QCONF_NAME -sh failed. Qmaster is probably not running?"
   LogIt "C" "$QMASTER_NAME is not running"
   EXIT 1
fi
tmp_adminhost=`$QCONF -sh | grep "^${HOST}$" 2>/dev/null`
if [ "$tmp_adminhost" != "$HOST" ]; then
   $INFOTEXT "ERROR: Load must be started on admin host ($QMASTER_NAME host recommended)."
   LogIt "C" "Can't start load_${PRODUCT_SHORTCUT}_config.sh on $HOST: not an admin host"
   EXIT 1
fi

TriggerUpOrDowngrade "$CURRENT_VERSION" "$TARGET_VERSION" "$DIR"
if [ $? -ne 0 ]; then
   LogIt "C" "Upgrading failed"
   EXIT 1
else
   LogIt "I" "Finished upgrading"
   EXIT 0
fi
