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

# Software bill of materials (CS-1686)
#
# Writes a CycloneDX 1.6 JSON document listing the third party software the product is built
# from or depends on, and installs it as $SGE_ROOT/3rd_party/sbom.cyclonedx.json - next to
# 3rd_party_licscopyrights.md, and under the same name as the SBOM of the other products.
#
# Scanners like Syft or Trivy recognise dependencies through package manager metadata, which our
# C/C++ dependencies do not have: they are built by ExternalProject or CPM and partly linked
# statically. The build knows them, with the version actually built (see BuildThirdParty.cmake),
# so the document is written from there, at configure time.
#
# Two kinds of components:
#   - built and shipped by us, or compiled into our binaries: with version, purl, CPE, license,
#     download location and, for a tarball, its hash
#   - taken from the operating system, linked or loaded at runtime: without version - which one
#     is used is decided by the system the product is installed on
#
# Vulnerability scanners like Dependency-Track find the vulnerabilities of a component by its
# CPE (NVD) or by its purl (OSS Index, OSV, GitHub Advisories). The purl sources cover package
# ecosystems like Maven, Go or PyPI, but not the pkg:github and pkg:generic purls our C/C++
# components have - so these components carry the CPE the NVD files their vulnerabilities under.
#
# A vulnerability filed for a component does not always affect the product, e.g. when the product
# does not use the affected function or when the attack is not possible in its environment. The
# VEX document vex.cyclonedx.json, installed next to the SBOM, states these cases and the reasons,
# so that a scanner records them with its findings instead of reporting the vulnerability again.
#
# The JSON is written as plain text rather than with string(JSON), which needs CMake 3.19 - more
# than some of the older build hosts have. Every value written is a constant of the build, so
# escaping backslashes and double quotes is all it needs.

set(SBOM_FILE_NAME "sbom.cyclonedx.json")
# the VEX document: which vulnerabilities of the components do not affect the product, and why
set(SBOM_VEX_FILE_NAME "vex.cyclonedx.json")

# @brief escape a value for a JSON string
#
# @param out   name of the variable receiving the escaped value
# @param value the value
function(sbom_json_escape out value)
   string(REPLACE "\\" "\\\\" escaped "${value}")
   string(REPLACE "\"" "\\\"" escaped "${escaped}")
   set(${out} "${escaped}" PARENT_SCOPE)
endfunction()

# @brief add a component to the SBOM
#
# The components are collected in the global property SBOM_COMPONENTS, the references of the
# components in SBOM_COMPONENT_REFS, and written by sbom_write().
#
# @param BOM_REF      unique reference of the component within the document
# @param NAME         name of the component
# @param VERSION      optional: version, a release or a commit hash
# @param DESCRIPTION  optional: what it is and what we use it for
# @param LICENSE      optional: SPDX license id or expression
# @param PURL         optional: package URL
# @param CPE          optional: CPE 2.3 name, see sbom_cpe()
# @param VCS          optional: URL of the source repository
# @param DISTRIBUTION optional: URL the source is downloaded from
# @param SHA256       optional: SHA-256 hash of what DISTRIBUTION refers to
# @param PEDIGREE     optional: the CycloneDX pedigree object as JSON, e.g. our patches, see
#                     sbom_patch_pedigree()
# @param SCOPE        "required" or "optional" (runtime loaded, used only when configured)
# @param LINKAGE      how it is part of the product: static, shared, header-only, executable,
#                     runtime-loaded
# @param PROVIDED_BY  "product" (built and shipped or compiled in by us) or "operating-system"
function(sbom_add_component)
   set(one_value_args BOM_REF NAME VERSION DESCRIPTION LICENSE PURL CPE VCS DISTRIBUTION SHA256
                      PEDIGREE SCOPE LINKAGE PROVIDED_BY)
   cmake_parse_arguments(C "" "${one_value_args}" "" ${ARGN})

   foreach (field BOM_REF NAME DESCRIPTION LICENSE PURL VCS DISTRIBUTION)
      sbom_json_escape(C_${field} "${C_${field}}")
   endforeach ()

   set(json "      {\n")
   string(APPEND json "        \"type\": \"library\",\n")
   string(APPEND json "        \"bom-ref\": \"${C_BOM_REF}\",\n")
   string(APPEND json "        \"name\": \"${C_NAME}\",\n")
   if (C_VERSION)
      string(APPEND json "        \"version\": \"${C_VERSION}\",\n")
   endif ()
   if (C_DESCRIPTION)
      string(APPEND json "        \"description\": \"${C_DESCRIPTION}\",\n")
   endif ()
   string(APPEND json "        \"scope\": \"${C_SCOPE}\",\n")
   if (C_SHA256)
      set(hash "{ \"alg\": \"SHA-256\", \"content\": \"${C_SHA256}\" }")
      string(APPEND json "        \"hashes\": [ ${hash} ],\n")
   endif ()
   if (C_LICENSE)
      if (C_LICENSE MATCHES " ")
         string(APPEND json "        \"licenses\": [ { \"expression\": \"${C_LICENSE}\" } ],\n")
      else ()
         set(license "{ \"license\": { \"id\": \"${C_LICENSE}\" } }")
         string(APPEND json "        \"licenses\": [ ${license} ],\n")
      endif ()
   endif ()
   if (C_CPE)
      string(APPEND json "        \"cpe\": \"${C_CPE}\",\n")
   endif ()
   if (C_PURL)
      string(APPEND json "        \"purl\": \"${C_PURL}\",\n")
   endif ()
   if (C_VCS OR C_DISTRIBUTION)
      set(refs "")
      if (C_VCS)
         list(APPEND refs "{ \"type\": \"vcs\", \"url\": \"${C_VCS}\" }")
      endif ()
      if (C_DISTRIBUTION)
         list(APPEND refs "{ \"type\": \"distribution\", \"url\": \"${C_DISTRIBUTION}\" }")
      endif ()
      string(REPLACE ";" ", " refs "${refs}")
      string(APPEND json "        \"externalReferences\": [ ${refs} ],\n")
   endif ()
   if (C_PEDIGREE)
      string(APPEND json "        \"pedigree\": ${C_PEDIGREE},\n")
   endif ()
   string(APPEND json "        \"properties\": [\n")
   set(linkage "{ \"name\": \"ocs:linkage\", \"value\": \"${C_LINKAGE}\" }")
   set(provided_by "{ \"name\": \"ocs:provided-by\", \"value\": \"${C_PROVIDED_BY}\" }")
   string(APPEND json "          ${linkage},\n")
   string(APPEND json "          ${provided_by}\n")
   string(APPEND json "        ]\n")
   string(APPEND json "      }")

   get_property(components GLOBAL PROPERTY SBOM_COMPONENTS)
   if (components)
      string(APPEND components ",\n")
   endif ()
   string(APPEND components "${json}")
   set_property(GLOBAL PROPERTY SBOM_COMPONENTS "${components}")
   set_property(GLOBAL APPEND PROPERTY SBOM_COMPONENT_REFS "${C_BOM_REF}")
endfunction()

# @brief the CPE 2.3 name of an application
#
# The vendor and the product have to be those of the NVD CPE dictionary
# (https://nvd.nist.gov/products/cpe/search), otherwise the CPE matches no vulnerability.
#
# @param out     name of the variable receiving the CPE
# @param vendor  vendor as in the NVD CPE dictionary, e.g. oracle
# @param product product as in the NVD CPE dictionary, e.g. berkeley_db
# @param version version as the NVD records it, e.g. 5.3.28
function(sbom_cpe out vendor product version)
   set(${out} "cpe:2.3:a:${vendor}:${product}:${version}:*:*:*:*:*:*:*" PARENT_SCOPE)
endfunction()

# @brief the CycloneDX pedigree of a component we patch
#
# Each patch is listed as a backport which resolves the security issue its CVE names, so a
# scanner or a reader of the SBOM can tell that the vulnerability is fixed although the version
# says otherwise.
#
# @param out     name of the variable receiving the pedigree object as JSON, empty without patches
# @param patches list of <patch file>=<CVE>, e.g. PROJECT_3RDPARTY_BERKELEYDB_PATCHES
function(sbom_patch_pedigree out patches)
   set(entries "")
   foreach (entry ${patches})
      string(REGEX REPLACE "^.*=" "" cve "${entry}")
      set(source "{ \"name\": \"NVD\", \"url\": \"https://nvd.nist.gov/vuln/detail/${cve}\" }")
      set(resolves "{ \"type\": \"security\", \"id\": \"${cve}\", \"source\": ${source} }")
      list(APPEND entries "{ \"type\": \"backport\", \"resolves\": [ ${resolves} ] }")
   endforeach ()
   set(pedigree "")
   if (entries)
      string(REPLACE ";" ", " entries "${entries}")
      set(pedigree "{ \"patches\": [ ${entries} ] }")
   endif ()
   set(${out} "${pedigree}" PARENT_SCOPE)
endfunction()

# @brief add a statement to the VEX document
#
# A statement says how a vulnerability reported for a component affects the product, e.g. that
# it does not, and why. The statements are collected in the global property SBOM_VEX_STATEMENTS
# and written by sbom_write() into a VEX document of their own. Dependency-Track and other
# scanners import it and record the analysis with the finding of the vulnerability.
#
# A statement names the product as the affected item, not the component: Dependency-Track keeps
# no bom-ref of the components it imports, and matches the bom-ref of the product only, applying
# the analysis to every component of the project which has the vulnerability.
#
# @param ID            the CVE id of the vulnerability, as the NVD files it
# @param STATE         CycloneDX analysis state, e.g. not_affected
# @param JUSTIFICATION CycloneDX justification, required with not_affected, e.g.
#                      protected_by_mitigating_control
# @param DETAIL        the reasoning
function(sbom_add_vex_statement)
   cmake_parse_arguments(V "" "ID;STATE;JUSTIFICATION;DETAIL" "" ${ARGN})
   sbom_json_escape(V_DETAIL "${V_DETAIL}")

   set(source "{ \"name\": \"NVD\", \"url\": \"https://nvd.nist.gov/vuln/detail/${V_ID}\" }")
   set(analysis "\"state\": \"${V_STATE}\"")
   if (V_JUSTIFICATION)
      string(APPEND analysis ", \"justification\": \"${V_JUSTIFICATION}\"")
   endif ()
   string(APPEND analysis ", \"detail\": \"${V_DETAIL}\"")

   set(json "    {\n")
   string(APPEND json "      \"id\": \"${V_ID}\",\n")
   string(APPEND json "      \"source\": ${source},\n")
   string(APPEND json "      \"analysis\": { ${analysis} },\n")
   string(APPEND json "      \"affects\": [ { \"ref\": \"product\" } ]\n")
   string(APPEND json "    }")

   get_property(statements GLOBAL PROPERTY SBOM_VEX_STATEMENTS)
   if (statements)
      string(APPEND statements ",\n")
   endif ()
   string(APPEND statements "${json}")
   set_property(GLOBAL PROPERTY SBOM_VEX_STATEMENTS "${statements}")
endfunction()

# @brief add a component provided by the operating system
#
# @param name        name of the library, e.g. libssl
# @param scope       "required", or "optional" for a library loaded at runtime when configured
# @param linkage     "shared" or "runtime-loaded"
# @param description what we use it for
function(sbom_add_os_component name scope linkage description)
   sbom_add_component(BOM_REF "os-${name}" NAME "${name}" DESCRIPTION "${description}"
                      SCOPE "${scope}" LINKAGE "${linkage}" PROVIDED_BY "operating-system")
endfunction()

# @brief collect the components of this build
#
# Follows the options of the build: a component is listed when the build uses it, and as built
# by us or taken from the operating system as WITH_OS_3RDPARTY says.
function(sbom_collect_components)
   if (WITH_OS_3RDPARTY)
      sbom_add_os_component(rapidjson "required" "header-only" "JSON parser and writer")
   else ()
      set(rapidjson_commit ${PROJECT_3RDPARTY_RAPIDJSON_COMMIT})
      # The NVD records the affected versions as "up to 1.1.0", the only release, and a commit
      # as version would match none of them. The commit is later than 1.1.0, so a finding has to
      # be checked against it, and marked as not affected when the commit has the fix.
      sbom_cpe(rapidjson_cpe "tencent" "rapidjson" "1.1.0")
      sbom_add_component(BOM_REF "rapidjson" NAME "rapidjson" VERSION "${rapidjson_commit}"
                         DESCRIPTION "JSON parser and writer, a commit of the master branch"
                         LICENSE "MIT"
                         PURL "pkg:github/Tencent/rapidjson@${rapidjson_commit}"
                         CPE "${rapidjson_cpe}"
                         VCS "https://github.com/Tencent/rapidjson.git"
                         SCOPE "required" LINKAGE "header-only" PROVIDED_BY "product")
   endif ()

   if (WITH_SPOOL_BERKELEYDB)
      if (WITH_OS_3RDPARTY)
         sbom_add_os_component(libdb "required" "shared" "Berkeley DB, spooling")
      else ()
         set(bdb_commit ${PROJECT_3RDPARTY_BERKELEYDB_COMMIT})
         set(description "Berkeley DB, spooling - libdb and the db_* utilities, built from")
         string(APPEND description " commit ${bdb_commit} of the libdb5 repository, with the")
         string(APPEND description " security patches the distributions apply to 5.3.28")
         sbom_cpe(bdb_cpe "oracle" "berkeley_db" "${PROJECT_3RDPARTY_BERKELEYDB_VERSION}")
         sbom_patch_pedigree(bdb_pedigree "${PROJECT_3RDPARTY_BERKELEYDB_PATCHES}")
         sbom_add_component(BOM_REF "berkeleydb" NAME "berkeleydb"
                            VERSION "${PROJECT_3RDPARTY_BERKELEYDB_VERSION}"
                            DESCRIPTION "${description}"
                            LICENSE "Sleepycat"
                            PURL "pkg:github/Positeral/libdb5@${bdb_commit}"
                            CPE "${bdb_cpe}"
                            VCS "https://github.com/Positeral/libdb5.git"
                            PEDIGREE "${bdb_pedigree}"
                            SCOPE "required" LINKAGE "shared" PROVIDED_BY "product")

         # The Oracle vulnerabilities the NVD files for 5.3.28 which do not affect the product.
         # Oracle published no details and no fix for 5.x. CVE-2019-2708 is not listed: there is
         # a fix, but it is taken from a later, AGPL licensed version.
         set(bdb_not_affected CVE-2017-3604 CVE-2017-3605 CVE-2017-3606 CVE-2017-3607
                              CVE-2017-3608 CVE-2017-3609 CVE-2017-3610 CVE-2017-3611
                              CVE-2017-3612 CVE-2017-3613 CVE-2017-3614 CVE-2017-3615
                              CVE-2017-3616 CVE-2017-3617 CVE-2020-2981)
         set(detail "Oracle describes the vulnerability as a local one: the attacker has to be")
         string(APPEND detail " logged in on the host where Berkeley DB runs. The product uses")
         string(APPEND detail " Berkeley DB for the BDB spooling only and opens the database")
         string(APPEND detail " environment with an explicit home directory, the BDB spooling")
         string(APPEND detail " directory. Only the admin user can write to it (directory mode")
         string(APPEND detail " 755, database files mode 0600). The database utilities run as")
         string(APPEND detail " the admin user on the same directory. Debian and Ubuntu consider")
         string(APPEND detail " the vulnerability not applicable to Berkeley DB 5.3.28.")
         foreach (cve ${bdb_not_affected})
            sbom_add_vex_statement(ID "${cve}" STATE "not_affected"
                                   JUSTIFICATION "protected_by_mitigating_control"
                                   DETAIL "${detail}")
         endforeach ()
      endif ()
   endif ()

   if (WITH_JEMALLOC)
      if (WITH_OS_3RDPARTY)
         sbom_add_os_component(jemalloc "required" "static" "memory allocator")
      else ()
         set(jemalloc_version ${PROJECT_3RDPARTY_JEMALLOC_VERSION})
         # not in the NVD CPE dictionary yet, as no vulnerability has been filed for jemalloc:
         # the likely name, check it once there is one
         sbom_cpe(jemalloc_cpe "jemalloc" "jemalloc" "${jemalloc_version}")
         sbom_add_component(BOM_REF "jemalloc" NAME "jemalloc" VERSION "${jemalloc_version}"
                            DESCRIPTION "memory allocator" LICENSE "BSD-2-Clause"
                            PURL "pkg:github/jemalloc/jemalloc@${jemalloc_version}"
                            CPE "${jemalloc_cpe}"
                            VCS "https://github.com/jemalloc/jemalloc.git"
                            SCOPE "required" LINKAGE "static" PROVIDED_BY "product")
      endif ()
   endif ()

   if (WITH_HWLOC)
      if (WITH_OS_3RDPARTY)
         sbom_add_os_component(hwloc "required" "shared" "hardware topology, core binding")
      else ()
         set(hwloc_version ${PROJECT_3RDPARTY_HWLOC_VERSION})
         set(hwloc_url ${PROJECT_3RDPARTY_HWLOC_URL})
         # not in the NVD CPE dictionary yet, as no vulnerability has been filed for hwloc: the
         # likely name, check it once there is one
         sbom_cpe(hwloc_cpe "open-mpi" "hwloc" "${hwloc_version}")
         sbom_add_component(BOM_REF "hwloc" NAME "hwloc" VERSION "${hwloc_version}"
                            DESCRIPTION "hardware topology, core binding" LICENSE "BSD-3-Clause"
                            PURL "pkg:generic/hwloc@${hwloc_version}?download_url=${hwloc_url}"
                            CPE "${hwloc_cpe}"
                            DISTRIBUTION "${hwloc_url}"
                            SHA256 "${PROJECT_3RDPARTY_HWLOC_SHA256}"
                            SCOPE "required" LINKAGE "static" PROVIDED_BY "product")
      endif ()
      # what the statically linked hwloc needs from the system
      if ("udev" IN_LIST SGE_TOPO_LIB)
         sbom_add_os_component(libudev "required" "shared" "device information, used by hwloc")
      endif ()
      if ("OpenCL" IN_LIST SGE_TOPO_LIB)
         sbom_add_os_component(libOpenCL "required" "shared" "OpenCL devices, used by hwloc")
      endif ()
   endif ()

   if (WITH_QMAKE)
      set(description "GNU make, extended and shipped as qmake (source/3rdparty/qmake-4.4)")
      sbom_cpe(make_cpe "gnu" "make" "4.4")
      sbom_add_component(BOM_REF "qmake" NAME "gnu-make" VERSION "4.4"
                         DESCRIPTION "${description}"
                         LICENSE "GPL-3.0-or-later" PURL "pkg:generic/gnu-make@4.4"
                         CPE "${make_cpe}"
                         SCOPE "required" LINKAGE "executable" PROVIDED_BY "product")
   endif ()

   # the C and C++ runtime every binary links
   sbom_add_os_component(libc "required" "shared" "C library")
   sbom_add_os_component(libstdc++ "required" "shared" "C++ standard library")
   sbom_add_os_component(libgcc_s "required" "shared" "compiler runtime")

   # loaded at runtime, only where it is configured
   if (WITH_OPENSSL)
      sbom_add_os_component(libssl "optional" "runtime-loaded"
                            "TLS encryption of the communication")
   endif ()
   if (WITH_MUNGE)
      sbom_add_os_component(libmunge "optional" "runtime-loaded" "MUNGE authentication")
   endif ()
   if (WITH_SYSTEMD)
      sbom_add_os_component(libsystemd "optional" "runtime-loaded"
                            "systemd integration of the daemons and jobs")
   endif ()

   if (WITH_SPOOL_POSTGRES)
      sbom_add_os_component(libpq "required" "shared" "PostgreSQL client library, spooling")
   endif ()

   # only linked when it was found, see the WITH_CURL block in CMakeLists.txt
   if (WITH_CURL AND CURL_FOUND)
      sbom_add_os_component(libcurl "required" "shared"
                            "HTTP client library, cloud provider detection")
   endif ()
endfunction()

# @brief write the SBOM document and the VEX document and install them
#
# The VEX document is written next to the SBOM, as SBOM_VEX_FILE_NAME, when there is a statement
# (see sbom_add_vex_statement()).
#
# @param file the SBOM file to write, installed into 3rd_party/ when INSTALL_SGE_COMMON is set
function(sbom_write file)
   if (PROJECT_FEATURES MATCHES "gcs-extensions")
      set(product_name "Gridware Cluster Scheduler")
   else ()
      set(product_name "Open Cluster Scheduler")
   endif ()
   set(product_version "${PROJECT_VERSION}${VERSION_SUFFIX}")

   # honours SOURCE_DATE_EPOCH, so a reproducible build writes a reproducible document
   string(TIMESTAMP timestamp "%Y-%m-%dT%H:%M:%SZ" UTC)
   # a name based UUID: the same product, version and time give the same serial number
   string(UUID serial NAMESPACE "6ba7b811-9dad-11d1-80b4-00c04fd430c8"
          NAME "${product_name} ${product_version} ${timestamp}" TYPE SHA1)

   get_property(components GLOBAL PROPERTY SBOM_COMPONENTS)
   get_property(component_refs GLOBAL PROPERTY SBOM_COMPONENT_REFS)
   set(refs "")
   foreach (ref IN LISTS component_refs)
      if (refs)
         string(APPEND refs ", ")
      endif ()
      string(APPEND refs "\"${ref}\"")
   endforeach ()

   set(json "{\n")
   string(APPEND json "  \"bomFormat\": \"CycloneDX\",\n")
   string(APPEND json "  \"specVersion\": \"1.6\",\n")
   string(APPEND json "  \"serialNumber\": \"urn:uuid:${serial}\",\n")
   string(APPEND json "  \"version\": 1,\n")
   string(APPEND json "  \"metadata\": {\n")
   string(APPEND json "    \"timestamp\": \"${timestamp}\",\n")
   set(tools "{ \"type\": \"application\", \"name\": \"cmake\",")
   string(APPEND tools " \"version\": \"${CMAKE_VERSION}\" }")
   if (NOT WITH_OS_3RDPARTY)
      # CPM fetches rapidjson, see BuildThirdParty.cmake
      set(cpm_version ${PROJECT_3RDPARTY_CPM_VERSION})
      set(cpm_hash "{ \"alg\": \"SHA-256\", \"content\": \"${PROJECT_3RDPARTY_CPM_SHA256}\" }")
      string(APPEND tools ",\n      { \"type\": \"application\", \"name\": \"CPM.cmake\",")
      string(APPEND tools " \"version\": \"${cpm_version}\",")
      string(APPEND tools " \"purl\": \"pkg:github/cpm-cmake/CPM.cmake@v${cpm_version}\",")
      string(APPEND tools " \"hashes\": [ ${cpm_hash} ] }")
   endif ()
   string(APPEND json "    \"tools\": { \"components\": [ ${tools} ] },\n")
   # the product, described by the SBOM and by the VEX document
   set(product "    \"component\": {\n")
   string(APPEND product "      \"type\": \"application\",\n")
   string(APPEND product "      \"bom-ref\": \"product\",\n")
   string(APPEND product "      \"name\": \"${product_name}\",\n")
   string(APPEND product "      \"version\": \"${product_version}\",\n")
   set(supplier_url "https://www.hpc-gridware.com")
   set(supplier "{ \"name\": \"HPC-Gridware GmbH\", \"url\": [ \"${supplier_url}\" ] }")
   set(arch "{ \"name\": \"ocs:arch\", \"value\": \"${SGE_ARCH}\" }")
   string(APPEND product "      \"supplier\": ${supplier},\n")
   string(APPEND product "      \"properties\": [ ${arch} ]\n")
   string(APPEND product "    }\n")
   string(APPEND json "${product}")
   string(APPEND json "  },\n")
   string(APPEND json "  \"components\": [\n${components}\n  ],\n")
   string(APPEND json "  \"dependencies\": [\n")
   string(APPEND json "    { \"ref\": \"product\", \"dependsOn\": [ ${refs} ] }\n")
   string(APPEND json "  ]\n")
   string(APPEND json "}\n")

   file(WRITE "${file}" "${json}")
   message(STATUS "SBOM written to ${file}")

   if (INSTALL_SGE_COMMON)
      install(FILES "${file}" DESTINATION 3rd_party)
   endif ()

   # the VEX document, next to the SBOM, when there is a statement
   get_property(statements GLOBAL PROPERTY SBOM_VEX_STATEMENTS)
   get_filename_component(vex_file "${file}" DIRECTORY)
   set(vex_file "${vex_file}/${SBOM_VEX_FILE_NAME}")
   if (NOT statements)
      file(REMOVE "${vex_file}")
      return()
   endif ()
   string(UUID vex_serial NAMESPACE "6ba7b811-9dad-11d1-80b4-00c04fd430c8"
          NAME "${product_name} ${product_version} ${timestamp} VEX" TYPE SHA1)
   set(json "{\n")
   string(APPEND json "  \"bomFormat\": \"CycloneDX\",\n")
   string(APPEND json "  \"specVersion\": \"1.6\",\n")
   string(APPEND json "  \"serialNumber\": \"urn:uuid:${vex_serial}\",\n")
   string(APPEND json "  \"version\": 1,\n")
   string(APPEND json "  \"metadata\": {\n")
   string(APPEND json "    \"timestamp\": \"${timestamp}\",\n")
   string(APPEND json "${product}")
   string(APPEND json "  },\n")
   string(APPEND json "  \"vulnerabilities\": [\n${statements}\n  ]\n")
   string(APPEND json "}\n")

   file(WRITE "${vex_file}" "${json}")
   message(STATUS "VEX written to ${vex_file}")

   if (INSTALL_SGE_COMMON)
      install(FILES "${vex_file}" DESTINATION 3rd_party)
   endif ()
endfunction()

# @brief collect the components of this build, write the SBOM and install it
function(sbom_generate)
   set_property(GLOBAL PROPERTY SBOM_COMPONENTS "")
   set_property(GLOBAL PROPERTY SBOM_COMPONENT_REFS "")
   set_property(GLOBAL PROPERTY SBOM_VEX_STATEMENTS "")
   sbom_collect_components()
   sbom_write("${CMAKE_BINARY_DIR}/${SBOM_FILE_NAME}")
endfunction()
