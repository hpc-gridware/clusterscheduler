# SPDX-License-Identifier: MIT
#
# SPDX-FileCopyrightText: Copyright (c) 2019-2023 Lars Melchior and contributors

# get_cpm.cmake of the CPM.cmake release, except for the version and its hash: they are set in
# cmake/BuildThirdParty.cmake, which the SBOM reads them from as well
set(CPM_DOWNLOAD_VERSION ${PROJECT_3RDPARTY_CPM_VERSION})
set(CPM_HASH_SUM ${PROJECT_3RDPARTY_CPM_SHA256})

if(CPM_PATH)
  set(CPM_DOWNLOAD_LOCATION "${CPM_PATH}/CPM.cmake")
elseif(DEFINED ENV{CPM_PATH})
  file(TO_CMAKE_PATH "$ENV{CPM_PATH}/CPM.cmake" CPM_DOWNLOAD_LOCATION)
elseif(CPM_SOURCE_CACHE)
  set(CPM_DOWNLOAD_LOCATION "${CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
elseif(DEFINED ENV{CPM_SOURCE_CACHE})
  set(CPM_DOWNLOAD_LOCATION "$ENV{CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
else()
  set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
endif()

# Expand relative path. This is important if the provided path contains a tilde (~)
get_filename_component(CPM_DOWNLOAD_LOCATION ${CPM_DOWNLOAD_LOCATION} ABSOLUTE)

file(DOWNLOAD
     https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
     ${CPM_DOWNLOAD_LOCATION} EXPECTED_HASH SHA256=${CPM_HASH_SUM}
)

include(${CPM_DOWNLOAD_LOCATION})
