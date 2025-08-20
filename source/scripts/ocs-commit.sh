#!/bin/sh
#
#___INFO__MARK_BEGIN_NEW__
###########################################################################
#
#  Copyright 2025 HPC-Gridware GmbH
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

# This git pre-commit hook will update the copyright headers in the files to be committed.
# It uses the master branch version of gcs-extensions/source/scripts/update_copyright.tcl.
# The following tools need to be installed on the host doing the git commit:
#    - wget (to fetch the update_copyright.tcl file)
#    - /usr/bin/tclsh
UPDATE_COPYRIGHT_SOURCE="https://raw.githubusercontent.com/hpc-gridware/clusterscheduler/refs/heads/master/source/scripts/update_copyright.tcl"
UPDATE_COPYRIGHT="/tmp/update_copyright.tcl"

CheckFetchUpdateCopyrightScript()
{
   if [ -x "$UPDATE_COPYRIGHT" ]; then
      echo "using $UPDATE_COPYRIGHT script to update the copyright header"
      echo "remove it to fetch the latest version from github"
   else
      echo "downloading $UPDATE_COPYRIGHT_SOURCE"
      wget -O $UPDATE_COPYRIGHT $UPDATE_COPYRIGHT_SOURCE
      if [ $? -ne 0 ]; then
         echo "failed to download $UPDATE_COPYRIGHT_SOURCE"
         exit 1
      fi
      chmod 755 $UPDATE_COPYRIGHT
   fi
}

echo ""

# MAIN
CheckFetchUpdateCopyrightScript

# if individual files are specified, use them
# if no files are specified, or if no options are given at all,
# use all files that have been changed
if [ $# -eq 0 ]; then
   #echo "no files specified, using all files marked to be committed"
   FILES=`git diff --cached --name-only`
elif [ $# -gt 0 -a $1 = "-a" ]; then
   #echo "using all files to be committed"
   FILES=`git diff --name-only`
else
   #echo "using specified files to update copyright header"
   FILES="$@"
fi

$UPDATE_COPYRIGHT --with-now $FILES
if [ $? -ne 0 ]; then
   echo "update_copyright.tcl failed"
   exit 1
fi

exec git commit "$@"

