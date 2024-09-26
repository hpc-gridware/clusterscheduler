#!/bin/sh

#___INFO__MARK_BEGIN_NEW__
###########################################################################
#
#  Copyright 2024 HPC-Gridware GmbH
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

# Returns the idle time of the workstation in milliseconds.
#
# on Solaris idle time is calculated by the last access time of /dev/kbd and /dev/mouse
# on Linux idle time is fetched from X11 or Wayland
#
# The idle time is returned in Cluster Scheduler load sensor format:
#
# Note: Load sensor scripts are started with root permissions. In an admin_user system: euid=0; uid=admin_user

PATH=/bin:/usr/bin
ARCH=$("$SGE_ROOT/util/arch")

# print idle time in seconds to stdout
get_idle_time_linux () {
   # on workstations with X
   #xprintidle

   # on workstations with KDE (X or Wayland)
   #dbus org.kde.screensaver /ScreenSaver GetSessionIdleTime

   # on workstation with gnome (X or Wayland)
   destination="org.gnome.Mutter.IdleMonitor"
   path="/org/gnome/Mutter/IdleMonitor/Core"
   message="$destination.GetIdletime"
   msec=$(dbus-send --print-reply --dest=$destination $path $message | tail -1 | awk '{print $NF;}')
   echo "scale=0; $msec / 1000" | bc
}

# print idle time in seconds to stdout
get_idle_time_solaris () {
   base_path="$SGE_ROOT/utilbin/$ARCH"
   kbd_time=$($base_path/filestat -atime /dev/kbd)
   mouse_time=$($base_path/filestat -atime /dev/mouse)
   now=$($base_path/now)

   if [ "$kbd_time" -gt "$mouse_time" ]; then
      timestamp="$kbd_time"
   else
      timestamp="$mouse_time"
   fi
   echo "scale=0; $now - $timestamp" | bc
}

# main loop that implements the load sensor protocol
end=false
while [ $end = false ]; do

   # wait for command
   if ! read -r input; then
      end=true
      break
   fi

   # got quit command?
   if [ "$input" = "quit" ]; then
      end=true
      break
   fi

   # get idle time
   if [ "$ARCH" = "sol-amd64" ]; then
      idletime=$(get_idle_time_solaris)
   elif [ "$ARCH" = "lx-amd64" ]; then
      idletime=$(get_idle_time_linux)
   else
      idletime=0
   fi

   # return idle time in load sensor format
   echo "begin"
   echo "$HOST:iidle:$idletime"
   echo "end"
done
