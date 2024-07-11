# Full List of Fixes

## Improvement

[CS-10](https://hpc-gridware.atlassian.net/browse/CS-10) Allow to build on darwin-arm64

[CS-11](https://hpc-gridware.atlassian.net/browse/CS-11) Allow to build on lx-arm6

[CS-13](https://hpc-gridware.atlassian.net/browse/CS-13) cleanup CULL definitions to ease generation of object definitions

[CS-14](https://hpc-gridware.atlassian.net/browse/CS-14) generate object definition from CULL in independent format \(JSON\)

[CS-15](https://hpc-gridware.atlassian.net/browse/CS-15) generate new CULL headers from the JSON object definitions

[CS-18](https://hpc-gridware.atlassian.net/browse/CS-18) cleanup in aimk build

[CS-19](https://hpc-gridware.atlassian.net/browse/CS-19) initial cmake build

[CS-20](https://hpc-gridware.atlassian.net/browse/CS-20) improvements to the cmake build process

[CS-24](https://hpc-gridware.atlassian.net/browse/CS-24) speedup build with testsuite

[CS-25](https://hpc-gridware.atlassian.net/browse/CS-25) fstype should support additional FS types

[CS-26](https://hpc-gridware.atlassian.net/browse/CS-26) allow to compile on arm64 \(Linux and macOS\)

[CS-35](https://hpc-gridware.atlassian.net/browse/CS-35) cmake build from testsuite

[CS-36](https://hpc-gridware.atlassian.net/browse/CS-36) accept kernel 6.x \(e.g. Ubuntu 22.04\)

[CS-37](https://hpc-gridware.atlassian.net/browse/CS-37) cache localised messages

[CS-42](https://hpc-gridware.atlassian.net/browse/CS-42) generate man pages from markdown

[CS-44](https://hpc-gridware.atlassian.net/browse/CS-44) create installation guide

[CS-58](https://hpc-gridware.atlassian.net/browse/CS-58) switch default build OSes to newer versions

[CS-62](https://hpc-gridware.atlassian.net/browse/CS-62) remove obsolete code for not supported architectures

[CS-68](https://hpc-gridware.atlassian.net/browse/CS-68) Adapt qdel, qhost, qhold and move env variable description

[CS-69](https://hpc-gridware.atlassian.net/browse/CS-69) allow to adjust complex entries individually with qconf

[CS-70](https://hpc-gridware.atlassian.net/browse/CS-70) Add information about architecture support and complexes to the man pages

[CS-76](https://hpc-gridware.atlassian.net/browse/CS-76) add doc build to testsuite

[CS-81](https://hpc-gridware.atlassian.net/browse/CS-81) make mk\_dist generatate a doc package

[CS-89](https://hpc-gridware.atlassian.net/browse/CS-89) strip binaries and provide additional debug symbols

[CS-90](https://hpc-gridware.atlassian.net/browse/CS-90) sign packaged binaries

[CS-91](https://hpc-gridware.atlassian.net/browse/CS-91) qconf send modification requests even if the configuration of an object is not changed

[CS-93](https://hpc-gridware.atlassian.net/browse/CS-93) port to FreeBSD

[CS-99](https://hpc-gridware.atlassian.net/browse/CS-99) perl client JSV not executed on fbsd-amd64

[CS-102](https://hpc-gridware.atlassian.net/browse/CS-102) make admin\_only\_hosts parameter active in installed OGE system

[CS-114](https://hpc-gridware.atlassian.net/browse/CS-114) add build option to use OS provided packages for 3rdparty tools

[CS-130](https://hpc-gridware.atlassian.net/browse/CS-130) Optimise bootstrap module

[CS-133](https://hpc-gridware.atlassian.net/browse/CS-133) merge code from RSMAP branch

[CS-134](https://hpc-gridware.atlassian.net/browse/CS-134) re-implement code which is only available as stubs

[CS-136](https://hpc-gridware.atlassian.net/browse/CS-136) remove the init\_level function from tests

[CS-142](https://hpc-gridware.atlassian.net/browse/CS-142) add details about distinst and mk\_dist to devel-guide

[CS-144](https://hpc-gridware.atlassian.net/browse/CS-144) add menu item 1t to testsuite which compiles the source and replaces the binaries without the need to re-install

[CS-151](https://hpc-gridware.atlassian.net/browse/CS-151) Improve runtime of qsub test

[CS-152](https://hpc-gridware.atlassian.net/browse/CS-152) enable qstat tests

[CS-154](https://hpc-gridware.atlassian.net/browse/CS-154) make it possible to start tests according to runtime

[CS-156](https://hpc-gridware.atlassian.net/browse/CS-156) add wallclock time, rss and maxrss to online usage

[CS-157](https://hpc-gridware.atlassian.net/browse/CS-157) add a configuration option for the timeout of prolog, epilog, pe start/stop procedures

[CS-159](https://hpc-gridware.atlassian.net/browse/CS-159) lx-riscv64 port

[CS-160](https://hpc-gridware.atlassian.net/browse/CS-160) port qmake to lx-riscv64

[CS-175](https://hpc-gridware.atlassian.net/browse/CS-175) add wallclock time and maxrss to accounting

[CS-180](https://hpc-gridware.atlassian.net/browse/CS-180) Cleanup existing accounting and reporting code

[CS-181](https://hpc-gridware.atlassian.net/browse/CS-181) Add reporting / accounting file writer for new format

[CS-182](https://hpc-gridware.atlassian.net/browse/CS-182) Make qacct read both old and new format

[CS-196](https://hpc-gridware.atlassian.net/browse/CS-196) testsuite framework needs a function to easily detect if the installed system is of a certain version

[CS-197](https://hpc-gridware.atlassian.net/browse/CS-197) testsuite function ge\_has\_feature shall cache its results

[CS-203](https://hpc-gridware.atlassian.net/browse/CS-203) Specification for request limit

[CS-269](https://hpc-gridware.atlassian.net/browse/CS-269) Allow listener threads to handle permission requests independent from worker threads

[CS-291](https://hpc-gridware.atlassian.net/browse/CS-291) gen\_types does not print the details in case of parsing erros

[CS-293](https://hpc-gridware.atlassian.net/browse/CS-293) module test should be executable within CLion

[CS-295](https://hpc-gridware.atlassian.net/browse/CS-295) lx-ppc64le port

[CS-297](https://hpc-gridware.atlassian.net/browse/CS-297) allow to specify patterns for groups of usage values

[CS-298](https://hpc-gridware.atlassian.net/browse/CS-298) show the pe task id in qacct

[CS-299](https://hpc-gridware.atlassian.net/browse/CS-299) queue\_config: set default shell to /bin/sh and shell\_start\_mode to unix\_behavior

[CS-300](https://hpc-gridware.atlassian.net/browse/CS-300) Default installation parameters should be set for improving usability 

[CS-303](https://hpc-gridware.atlassian.net/browse/CS-303) add per test configuration variable\(s\) defining for which OGE versions the test shall be run

[CS-313](https://hpc-gridware.atlassian.net/browse/CS-313) unifiy delimiters in configuration objects

[CS-329](https://hpc-gridware.atlassian.net/browse/CS-329) Improvements to the Leap/Rocky-9/Ubuntu-22 Dockerfiles

[CS-330](https://hpc-gridware.atlassian.net/browse/CS-330) Provide a Dockerfile for Rocky-8 \(our default build platform for lx-amd64 and lx-arm64\)

[CS-331](https://hpc-gridware.atlassian.net/browse/CS-331) Create a build script as entry point

[CS-332](https://hpc-gridware.atlassian.net/browse/CS-332) Create packages

[CS-350](https://hpc-gridware.atlassian.net/browse/CS-350) implement higher resolution timestamps

[CS-351](https://hpc-gridware.atlassian.net/browse/CS-351) adapt testsuite to higher resolution timestamps

[CS-359](https://hpc-gridware.atlassian.net/browse/CS-359) Add a build ID to the version string of each application and enhance build environment to specify that ID

[CS-361](https://hpc-gridware.atlassian.net/browse/CS-361) Adapt build process of TS

[CS-366](https://hpc-gridware.atlassian.net/browse/CS-366) test binaries are missing in test clusters installed from packages

[CS-373](https://hpc-gridware.atlassian.net/browse/CS-373) Connect web service to S3 using account with read-only permissions

[CS-377](https://hpc-gridware.atlassian.net/browse/CS-377) improve build performance on sol-amd64

## New Feature

[CS-30](https://hpc-gridware.atlassian.net/browse/CS-30) Allow more than one data store within qmaster

[CS-308](https://hpc-gridware.atlassian.net/browse/CS-308) test\_uti\_profiling causes segmentation fault

## Epic

[CS-8](https://hpc-gridware.atlassian.net/browse/CS-8) Simplify development for the future

[CS-17](https://hpc-gridware.atlassian.net/browse/CS-17) replace aimk by a cmake based build

[CS-21](https://hpc-gridware.atlassian.net/browse/CS-21) make testsuite run smoothly

[CS-31](https://hpc-gridware.atlassian.net/browse/CS-31) Create product manuals

[CS-33](https://hpc-gridware.atlassian.net/browse/CS-33) Important Bug Fixes

[CS-41](https://hpc-gridware.atlassian.net/browse/CS-41) Provide man pages 

[CS-48](https://hpc-gridware.atlassian.net/browse/CS-48) re-enable Java DRMAA

[CS-177](https://hpc-gridware.atlassian.net/browse/CS-177) Need higher resolution in time stamps

## Sub-task

[CS-73](https://hpc-gridware.atlassian.net/browse/CS-73) remove old version of qmake

[CS-74](https://hpc-gridware.atlassian.net/browse/CS-74) remove aimk build

[CS-80](https://hpc-gridware.atlassian.net/browse/CS-80) remove bdb\_server from code and testsuite

[CS-86](https://hpc-gridware.atlassian.net/browse/CS-86) remove not required includes

[CS-119](https://hpc-gridware.atlassian.net/browse/CS-119) Build on Solaris and test hwloc

[CS-120](https://hpc-gridware.atlassian.net/browse/CS-120) Remove function pointers from gdi context that are part of setup\_path module

[CS-122](https://hpc-gridware.atlassian.net/browse/CS-122) Remove function pointers from gdi context that are part of sge\_env module

[CS-123](https://hpc-gridware.atlassian.net/browse/CS-123) Remove function pointers from gdi context that are part of sge\_uidgid module

[CS-125](https://hpc-gridware.atlassian.net/browse/CS-125) Split GDI related functions into two sets, internally used and externally used.

[CS-128](https://hpc-gridware.atlassian.net/browse/CS-128) Remove CSP related parts from the GDI context

[CS-129](https://hpc-gridware.atlassian.net/browse/CS-129) Remove duplicate functionality available in various modules uidgid/prog/bootstrap/...

[CS-131](https://hpc-gridware.atlassian.net/browse/CS-131) Remove ctx argument from functions

[CS-138](https://hpc-gridware.atlassian.net/browse/CS-138) Improve shutdown performance of qmaster

[CS-143](https://hpc-gridware.atlassian.net/browse/CS-143) add thread that fills additional data store

[CS-163](https://hpc-gridware.atlassian.net/browse/CS-163) enable compile warnings to show deprecated-declarations

[CS-164](https://hpc-gridware.atlassian.net/browse/CS-164) enable compile warnings to show write-strings

[CS-240](https://hpc-gridware.atlassian.net/browse/CS-240) listener threads should access own DS

[CS-246](https://hpc-gridware.atlassian.net/browse/CS-246) handle auth request by listener threads

[CS-249](https://hpc-gridware.atlassian.net/browse/CS-249) add ds for RO requests

[CS-255](https://hpc-gridware.atlassian.net/browse/CS-255) decouple data stores and mirror interface

[CS-274](https://hpc-gridware.atlassian.net/browse/CS-274) enhance lock module by trylock functionality

[CS-283](https://hpc-gridware.atlassian.net/browse/CS-283) handle auth requests that are usually done in worker threads before GDI within the listener threads

[CS-322](https://hpc-gridware.atlassian.net/browse/CS-322) Permission requests for AR and other future requests need access to user lists

## Task

[CS-1](https://hpc-gridware.atlassian.net/browse/CS-1) Import all repositories from the Univa gridengine project into opengridengine

[CS-2](https://hpc-gridware.atlassian.net/browse/CS-2) make code build on CentOS 7

[CS-4](https://hpc-gridware.atlassian.net/browse/CS-4) Remove old components

[CS-5](https://hpc-gridware.atlassian.net/browse/CS-5) cleanup of object module in sgeobj

[CS-6](https://hpc-gridware.atlassian.net/browse/CS-6) add a test module for multi threading

[CS-7](https://hpc-gridware.atlassian.net/browse/CS-7) make use of const keyword where possible

[CS-9](https://hpc-gridware.atlassian.net/browse/CS-9) cleanup DENTER and DRETURN

[CS-22](https://hpc-gridware.atlassian.net/browse/CS-22) make testsuite run on current OSes in our tests environments

[CS-23](https://hpc-gridware.atlassian.net/browse/CS-23) use immediate scheduling in testsuite

[CS-27](https://hpc-gridware.atlassian.net/browse/CS-27) replace SGE\_FUNC by \_\_func\_\_

[CS-34](https://hpc-gridware.atlassian.net/browse/CS-34) Define TOC for the Installation Guide

[CS-38](https://hpc-gridware.atlassian.net/browse/CS-38) allow the use of newer compilers

[CS-39](https://hpc-gridware.atlassian.net/browse/CS-39) build all sgeobj related code as C\+\+ code

[CS-40](https://hpc-gridware.atlassian.net/browse/CS-40) add documentation comments from old CULL files to the JSON files

[CS-43](https://hpc-gridware.atlassian.net/browse/CS-43) create templates and build mechanism for documentation

[CS-47](https://hpc-gridware.atlassian.net/browse/CS-47) add cmake doc build to testsuite

[CS-49](https://hpc-gridware.atlassian.net/browse/CS-49) Make it possible to use Rocky Linux 8 as build platform

[CS-50](https://hpc-gridware.atlassian.net/browse/CS-50) Remove the installation steps for Windows

[CS-52](https://hpc-gridware.atlassian.net/browse/CS-52) some make target ignore fixed spooling method

[CS-61](https://hpc-gridware.atlassian.net/browse/CS-61) Modify 3 man pages and format them properly

[CS-64](https://hpc-gridware.atlassian.net/browse/CS-64) remove JMX and Java related parameter from the configuration

[CS-65](https://hpc-gridware.atlassian.net/browse/CS-65) -jmx switch of installer is deprecated and has to be removed

[CS-66](https://hpc-gridware.atlassian.net/browse/CS-66) jvm\_threads in bootstrap file is obsolete

[CS-67](https://hpc-gridware.atlassian.net/browse/CS-67) Adapted qconf man page.

[CS-72](https://hpc-gridware.atlassian.net/browse/CS-72) remove obsolete libraries, documents, images and more

[CS-75](https://hpc-gridware.atlassian.net/browse/CS-75) replace include guard by pragma once

[CS-78](https://hpc-gridware.atlassian.net/browse/CS-78) add gridengine\_version 90 to testsuite 

[CS-79](https://hpc-gridware.atlassian.net/browse/CS-79) move messages into the msg file of the component where they are used

[CS-82](https://hpc-gridware.atlassian.net/browse/CS-82) Adapt qmake, qhost, qmod 

[CS-84](https://hpc-gridware.atlassian.net/browse/CS-84) Adapt qping, qquota, qrdel markdown files

[CS-87](https://hpc-gridware.atlassian.net/browse/CS-87) add comment to generated CULL header files

[CS-92](https://hpc-gridware.atlassian.net/browse/CS-92) create template for release notes

[CS-94](https://hpc-gridware.atlassian.net/browse/CS-94) build libcull as C\+\+ code

[CS-95](https://hpc-gridware.atlassian.net/browse/CS-95) build libcomm as C\+\+ code

[CS-96](https://hpc-gridware.atlassian.net/browse/CS-96) build libuti as C\+\+ code

[CS-97](https://hpc-gridware.atlassian.net/browse/CS-97) build libijs as C\+\+ code

[CS-98](https://hpc-gridware.atlassian.net/browse/CS-98) replace plpa by hwloc

[CS-104](https://hpc-gridware.atlassian.net/browse/CS-104) reformat qrls, qrstat, qrsub man pages

[CS-109](https://hpc-gridware.atlassian.net/browse/CS-109) create a development guide

[CS-110](https://hpc-gridware.atlassian.net/browse/CS-110) enhance build environment to create documentation on top of common documentation

[CS-113](https://hpc-gridware.atlassian.net/browse/CS-113) remove dependency of most build target to SGE\_TOPO\_LIBs

[CS-127](https://hpc-gridware.atlassian.net/browse/CS-127) remove USE\_POLL define from commlib code

[CS-139](https://hpc-gridware.atlassian.net/browse/CS-139) Make Rocky/Alma/CentOS 8 the build host for lx-amd64 and CentOS 7 for ulx-amd64

[CS-140](https://hpc-gridware.atlassian.net/browse/CS-140) remove unused files from source/scripts directory

[CS-141](https://hpc-gridware.atlassian.net/browse/CS-141) handle critical compiler warnings

[CS-178](https://hpc-gridware.atlassian.net/browse/CS-178) Create specification for higher resolution timestamps

[CS-179](https://hpc-gridware.atlassian.net/browse/CS-179) Specify new accounting and reporting file format

[CS-256](https://hpc-gridware.atlassian.net/browse/CS-256) duplicate functions providing functionality like lStr2Nm\(\)

[CS-292](https://hpc-gridware.atlassian.net/browse/CS-292) verify and update copyright headers in the code

[CS-306](https://hpc-gridware.atlassian.net/browse/CS-306) verify if usage values written to the shepherd usage are passed on by sge\_execd to sge\_qmaster

[CS-317](https://hpc-gridware.atlassian.net/browse/CS-317) updated xml schema files

[CS-318](https://hpc-gridware.atlassian.net/browse/CS-318) cleanup: use boolean logic on boolean data

[CS-324](https://hpc-gridware.atlassian.net/browse/CS-324) As a developer I need a Dockerfile with all dependencies in order to build Open Cluster Scheduler on Ubuntu 22.04

[CS-325](https://hpc-gridware.atlassian.net/browse/CS-325) As a developer I need a Dockerfile with all dependencies in order to build Open Cluster Scheduler on Rocky 9.3

[CS-326](https://hpc-gridware.atlassian.net/browse/CS-326) As a developer I need a Dockerfile with all dependencies in order to build Open Cluster Scheduler on Leap 15.4

## Bug

[CS-3](https://hpc-gridware.atlassian.net/browse/CS-3) Manual and auto installation broken

[CS-32](https://hpc-gridware.atlassian.net/browse/CS-32) fix tests which are not working with the current OGE version

[CS-53](https://hpc-gridware.atlassian.net/browse/CS-53) installation with classic spooling fails due to spoolinit failing

[CS-54](https://hpc-gridware.atlassian.net/browse/CS-54) spooledit list does not output any data

[CS-60](https://hpc-gridware.atlassian.net/browse/CS-60) Remove non printable characters from mardown man pages

[CS-63](https://hpc-gridware.atlassian.net/browse/CS-63) add support for non-cluster and admin-only hosts in TS

[CS-100](https://hpc-gridware.atlassian.net/browse/CS-100) test DRMAA binary does not find shared library on fbsd-amd64

[CS-101](https://hpc-gridware.atlassian.net/browse/CS-101) some automated test report errors for operations on non-root accessible FSs

[CS-103](https://hpc-gridware.atlassian.net/browse/CS-103) qrsh reports a job as not schedule-able but job is executed

[CS-135](https://hpc-gridware.atlassian.net/browse/CS-135) testsuite complains about a lot of message macros not being used

[CS-137](https://hpc-gridware.atlassian.net/browse/CS-137) do not use true and false in TCL code

[CS-145](https://hpc-gridware.atlassian.net/browse/CS-145) qping monitoring output is broken

[CS-146](https://hpc-gridware.atlassian.net/browse/CS-146) terminate method receives PID of job for $job\_pid argument, also for qrsh jobs.

[CS-147](https://hpc-gridware.atlassian.net/browse/CS-147) Windows specific configuration parameters are all deprecated

[CS-149](https://hpc-gridware.atlassian.net/browse/CS-149) Invalid prolog or shell in the queue sets job into error state instead of the job

[CS-153](https://hpc-gridware.atlassian.net/browse/CS-153) job spooling broken when system is installed with TS

[CS-158](https://hpc-gridware.atlassian.net/browse/CS-158) loadcheck does not output topology when loading the topology from an xml file

[CS-161](https://hpc-gridware.atlassian.net/browse/CS-161) enable compiler warnings

[CS-165](https://hpc-gridware.atlassian.net/browse/CS-165) qsub gets into endless loop

[CS-195](https://hpc-gridware.atlassian.net/browse/CS-195) read-only and read-write variants are missing for lGetSub\* set of functions

[CS-201](https://hpc-gridware.atlassian.net/browse/CS-201) sigignore is not threads safe and also deprecated

[CS-247](https://hpc-gridware.atlassian.net/browse/CS-247) default event handler for CONF\_Type object causes critical master error

[CS-282](https://hpc-gridware.atlassian.net/browse/CS-282) qmaster returns with error reason for completely different cause of error

[CS-284](https://hpc-gridware.atlassian.net/browse/CS-284) in case of GDI version mismatch error message does not show hex version number

[CS-285](https://hpc-gridware.atlassian.net/browse/CS-285) listener threads do not reject requests with incompatible GDI version

[CS-286](https://hpc-gridware.atlassian.net/browse/CS-286) error during decryption of authentication information does not reject a GDI request in the listener.

[CS-287](https://hpc-gridware.atlassian.net/browse/CS-287) fake client with invalid authinfo is not rejected but might lead to core of qmaster

[CS-288](https://hpc-gridware.atlassian.net/browse/CS-288) in CSP mode all errors of sge\_security\_verify\_unique\_identifier\(\) do not cause request rejection in listener thread.

[CS-289](https://hpc-gridware.atlassian.net/browse/CS-289) license string in the binaries needs to be replaced

[CS-290](https://hpc-gridware.atlassian.net/browse/CS-290) add a build step that allows to generate CULL header from JSON files

[CS-302](https://hpc-gridware.atlassian.net/browse/CS-302) RSMAP does not work for Job Array tasks which are scheduled in the same scheduling interval

[CS-304](https://hpc-gridware.atlassian.net/browse/CS-304) permission check for qmod -r has changed, used to require operator rights, now manager rights

[CS-307](https://hpc-gridware.atlassian.net/browse/CS-307) qsub crashes when debug printing is enabled

[CS-327](https://hpc-gridware.atlassian.net/browse/CS-327) default installation directory is not used

[CS-344](https://hpc-gridware.atlassian.net/browse/CS-344) queue does not switch into error state if it contains an invalid prolog 

[CS-345](https://hpc-gridware.atlassian.net/browse/CS-345) XDR related includes on Ubuntu 24.04 cause compile errors

[CS-355](https://hpc-gridware.atlassian.net/browse/CS-355) build qmake on lx-s390x

[CS-356](https://hpc-gridware.atlassian.net/browse/CS-356) Change architecture on Raspian 11 on Pi 4 to lx-armhf

[CS-357](https://hpc-gridware.atlassian.net/browse/CS-357) Libtirpc is not found on lx-riscv64 \(Suse Tumbleweed\)

[CS-362](https://hpc-gridware.atlassian.net/browse/CS-362) libdrmaa.so has unresolved symbols hwloc\_\*

[CS-409](https://hpc-gridware.atlassian.net/browse/CS-409) ncores is incorrectly reported as the number of threads
