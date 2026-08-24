# cmake ... -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain.cmake
#   or
# export CMAKE_PROJECT_TOP_LEVEL_INCLUDES=/path/to/file.cmake
#
# will be evaluated before the project() call!
#
# Shows details about std support for gcc versions
# https://gcc.gnu.org/projects/cxx-status.html

# Get some details about the environment 
execute_process(COMMAND hostname OUTPUT_VARIABLE HOSTNAME_FQDN OUTPUT_STRIP_TRAILING_WHITESPACE)
string(REGEX REPLACE "\\..*$" "" HOSTNAME "${HOSTNAME_FQDN}")
# execute_process(COMMAND hostname -s OUTPUT_VARIABLE HOSTNAME OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND ${CMAKE_SOURCE_DIR}/source/dist/util/arch OUTPUT_VARIABLE SGE_ARCH_RAW)
string(STRIP "${SGE_ARCH_RAW}" SGE_ARCH)
set(SGE_ARCH "${SGE_ARCH}" PARENT_SCOPE)
message(STATUS "Toolchain evaluation started")
message(STATUS "Hostname: ${HOSTNAME}")
message(STATUS "Build ID: ${CMAKE_BUILD_ID}")
message(STATUS "Arch:     ${SGE_ARCH}")

set(LINK_CPP_STATICALLY OFF)

# The lab build hosts (h047, h018) are Debian style multiarch systems, but the
# gcc under /tools was not configured for one: on its own it finds neither
# crt1.o (in /usr/lib/<triplet>) nor bits/wordsize.h (in /usr/include/<triplet>),
# so both locations have to be handed to it.  The triplet is asked from the
# *system* compiler - the one that actually knows it - so that a further
# architecture needs no table here, only a branch below.
# Optional argument: a different binutils directory, for the case that several
# distributions report the same SGE_ARCH and one tree does not serve them all
# (same pattern cmake uses: ulx-amd64-centos6 / -centos7). Currently unused --
# binutils are built on the *oldest* system of an architecture, then they run
# everywhere: glibc is backwards compatible, not forwards.
macro(gcs_use_tools_toolchain)
   set(_gcs_bu "${ARGN}")
   if ("${_gcs_bu}" STREQUAL "")
      set(_gcs_bu "${SGE_ARCH}")
   endif()
   set(CMAKE_C_COMPILER "/tools/PKG/gcc-15.2/${SGE_ARCH}/bin/gcc")
   set(CMAKE_CXX_COMPILER "/tools/PKG/gcc-15.2/${SGE_ARCH}/bin/g++")
   set(_gcs_flags "-B/tools/PKG/binutils-2.45/${_gcs_bu}/bin")

   execute_process(COMMAND /usr/bin/gcc -print-multiarch
                   OUTPUT_VARIABLE _gcs_multiarch
                   OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
   if (NOT "${_gcs_multiarch}" STREQUAL "")
      string(APPEND _gcs_flags " -B/usr/lib/${_gcs_multiarch}"
                               " -isystem /usr/include/${_gcs_multiarch}"
                               " -L/usr/lib/${_gcs_multiarch}")
      message(STATUS "Multiarch: ${_gcs_multiarch}")
   endif()

   # _INIT only seeds CMAKE_C_FLAGS; it survives just as long as nobody later
   # overwrites the variable.  ArchitectureSpecificSettings.cmake appends its
   # warning flags with "${CMAKE_C_FLAGS} ..." for exactly that reason - a plain
   # replacement there would drop the -B and multiarch paths set here, and the
   # compiler would stop finding crt1.o.
   set(CMAKE_C_FLAGS_INIT "${_gcs_flags}")
   set(CMAKE_CXX_FLAGS_INIT "${_gcs_flags}")
   message(STATUS "Selected: C++ v15.2 (GCC); binutils v2.45 => c++23 fully supported and stable")

   # Belongs to the compiler, not to the host: a gcc that does not come from the
   # OS must not leave its C++ runtime to be resolved on whatever machine the
   # binary is executed on later.  Whether the system libstdc++ happens to be new
   # enough today is a coincidence and must not decide this - so it is set here,
   # where the non-OS compiler is chosen, and cannot be forgotten in a new block.
   set(LINK_CPP_STATICALLY ON)
endmacro()

if (("${SGE_ARCH}" STREQUAL "ulx-amd64") AND ((${HOSTNAME} STREQUAL "ce7-0-ulx-amd64") OR (${HOSTNAME} STREQUAL "v047-ulx-amd64")))
   # CentOS 7: the system compiler is gcc 4.8.5 and provides c++20 partially only.
   # The macro picks the same tools that used to be listed by hand here; its
   # multiarch part stays without effect because "gcc -print-multiarch" returns
   # nothing on CentOS.
   gcs_use_tools_toolchain()
elseif (("${SGE_ARCH}" STREQUAL "lx-amd64") AND ((${HOSTNAME} STREQUAL "ce8-0-lx-amd64") OR (${HOSTNAME} STREQUAL "v047-lx-amd64")))
   # CentOS 8: the system compiler supports c++20 only partially.
   gcs_use_tools_toolchain()
elseif (("${SGE_ARCH}" STREQUAL "xlx-amd64") AND ((${HOSTNAME} STREQUAL "ce6-0-xlx-amd64") OR (${HOSTNAME} STREQUAL "v047-xlx-amd64")))
   # CentOS 6: the system compiler supports c++20 only partially.
   gcs_use_tools_toolchain()
elseif (("${SGE_ARCH}" STREQUAL "lx-amd64") AND (${HOSTNAME} STREQUAL "h047"))
   # Ubuntu 26.04; system gcc is 15 already, but use the same tool chain as
   # every other build host so that what one host accepts the next accepts too.
   gcs_use_tools_toolchain()
elseif (("${SGE_ARCH}" STREQUAL "lx-arm64") AND (${HOSTNAME} STREQUAL "h018"))
   # Ubuntu 22.04; here the system libstdc++ (GLIBCXX_3.4.30, gcc 11) is visibly
   # older than what gcc 15.2 emits - the static C++ runtime the macro selects is
   # not optional on this host.
   gcs_use_tools_toolchain()
elseif (("${SGE_ARCH}" STREQUAL "lx-riscv64") AND (${HOSTNAME} STREQUAL "su0-0-lx-riscv64"))
   # Default is gcc 15.2.1 => c++23 fully supported and stable
elseif (("${SGE_ARCH}" STREQUAL "lx-riscv64") AND (${HOSTNAME} STREQUAL "v047-lx-riscv64"))
   # Ubuntu 24.04, emulated on h047. Unlike su0-0 - which ships gcc 15.2 itself -
   # the system compiler here is gcc 13, so the tool chain under /tools is used.
   gcs_use_tools_toolchain()
elseif (("${SGE_ARCH}" STREQUAL "lx-loong64") AND (${HOSTNAME} STREQUAL "v047-lx-loong64"))
   # openEuler 24.03 LTS-SP4, emulated on h047. System compiler is gcc 12.3,
   # so the tool chain under /tools is used here as well.
   gcs_use_tools_toolchain()
elseif (("${SGE_ARCH}" STREQUAL "lx-arm64") AND (${HOSTNAME} STREQUAL "ce8-3-lx-arm64"))
   # Default is gcc 13.2.1 => c++23 fully supported and stable
elseif (("${SGE_ARCH}" STREQUAL "lx-ppc64le") AND (${HOSTNAME} STREQUAL "al8-1-lx-ppc64le"))
   # Default is gcc 13.2.1 => c++23 fully supported and stable
elseif (("${SGE_ARCH}" STREQUAL "lx-s390x") AND (${HOSTNAME} STREQUAL "al8-2-lx-s390x"))
   # Default is gcc 11.2.1 => c++20 partially 
elseif (("${SGE_ARCH}" STREQUAL "fbsd-amd64") AND ((${HOSTNAME} STREQUAL "fr13-0-fbsd-amd64") OR (${HOSTNAME} STREQUAL "v047-fbsd-amd64")))
   # FreeBSD 13: default is clang 17.0.6 => c++23 fully supported and stable
elseif (("${SGE_ARCH}" STREQUAL "fbsd-amd64") AND ((${HOSTNAME} STREQUAL "fr14-0-fbsd-amd64") OR (${HOSTNAME} STREQUAL "v047-fbsd-amd64-14")))
   # FreeBSD 14: default is clang 18.1.5 => c++23 fully supported and stable.
   # The system compiler stays: FreeBSD ships clang *and* libc++, so a gcc from
   # the ports would be a foreign body next to the system library. 
endif()

if (LINK_CPP_STATICALLY) 
   # IMPORTANT:
   #
   # stdc++ MUST be loaded **statically** otherwise the created libraries/binaries will
   #        ino run on systems with a smaller library version which is the case with
   #        non-os distributed g++.
   # libgcc MUST be loaded **dynamically** because other libraries (like libpthread) are also linked
   #        shared. Handling otherwise would cause different unwinding stacks in an application
   #        that would cause pthread_cancel() or exception handling in C++ to fail.
   #        (see also CS-1894 and CS-1176)
   set(CMAKE_CXX_STANDARD_LIBRARIES_INIT "-static-libstdc++ -shared-libgcc")
   message(STATUS "Linking: static C++ libs but dynamic C lib")
endif()

message(STATUS "Toolchain evaluation finished")

