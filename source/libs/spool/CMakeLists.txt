#___INFO__MARK_BEGIN_NEW__
###########################################################################
#  
#  Copyright 2023-2024,2026 HPC-Gridware GmbH
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

# source/libs/spool
set(LIBRARY_NAME spool)
set(LIBRARY_SOURCES sge_dirent.cc sge_spooling.cc sge_spooling_utilities.cc)
set(LIBRARY_INCLUDES "./")

# CS-199 makes use of readdir_r/readdir64_r which is deprecated for Linux only
set_source_files_properties(sge_dirent.cc PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${SGE_INCLUDES}
      ${LIBRARY_INCLUDES})




add_subdirectory(berkeleydb)
# add_subdirectory(classic)
add_subdirectory(dynamic)
add_subdirectory(flatfile)
add_subdirectory(loader)

# The postgres backend lives in the sibling gcs-extensions repository
# (source/libs/spool/postgres there). The clusterscheduler-side wiring
# pulls it in as a cross-repo subdirectory whenever
# PROJECT_FEATURES=gcs-extensions is active. A WITH_SPOOL_POSTGRES=ON
# build without gcs-extensions is a configuration error rather than a
# silent no-op so the operator gets a clear diagnostic.
if (WITH_SPOOL_POSTGRES AND NOT PROJECT_FEATURES MATCHES "gcs-extensions")
    message(FATAL_ERROR
        "WITH_SPOOL_POSTGRES=ON requires PROJECT_FEATURES=gcs-extensions; "
        "the postgres spooling backend lives in the gcs-extensions repository.")
endif ()
if (PROJECT_FEATURES MATCHES "gcs-extensions")
    add_subdirectory(
        ../../../../${PROJECT_FEATURES}/source/libs/spool/postgres
        postgres)
endif ()
