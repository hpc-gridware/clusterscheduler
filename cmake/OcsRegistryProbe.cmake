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

# ocs_link_registry_probe(<target>)
#
# A loadable object (a spooling plugin) that links the list core statically must
# carry the probe ocs_tom_descr_registry_seen(), which the loading process calls
# to check that the object shares its descriptor registry (N-A-22, see
# ocs::tom::descr_registry_shared()). Nothing in the object calls the probe, so
# the static library does not link its translation unit on its own; the linker is
# told to keep the symbol.
function(ocs_link_registry_probe target)
   if (APPLE)
      target_link_options(${target} PRIVATE "LINKER:-u,_ocs_tom_descr_registry_seen")
   else ()
      target_link_options(${target} PRIVATE "LINKER:-u,ocs_tom_descr_registry_seen")
   endif ()
endfunction()
