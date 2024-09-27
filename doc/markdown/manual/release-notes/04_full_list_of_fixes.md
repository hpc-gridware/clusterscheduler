# Full List of Fixes

# 9.0.0 RC4

### Improvement

[CS-70](https://hpc-gridware.atlassian.net/browse/CS-70) Add information about architecture support and complexes to the man pages

[CS-71](https://hpc-gridware.atlassian.net/browse/CS-71) create a how-to area where such documents can be stored

[CS-258](https://hpc-gridware.atlassian.net/browse/CS-258) reformat qsub, sge\_ckpt, sge\_hostnameutils

[CS-260](https://hpc-gridware.atlassian.net/browse/CS-260) reformat submit.include man pages

[CS-262](https://hpc-gridware.atlassian.net/browse/CS-262) reformat sge\_bootstrap, sge\_calendar\_conf, sge\_checkpoint man pages

[CS-263](https://hpc-gridware.atlassian.net/browse/CS-263) reformat sge\_complex, sge\_conf, sge\_host\_aliases man page

[CS-264](https://hpc-gridware.atlassian.net/browse/CS-264) reformat sge\_host\_conf, sge\_hostgroup, sge\_pe man page

[CS-265](https://hpc-gridware.atlassian.net/browse/CS-265) reformat sge\_priority, sge\_project, sge\_qstat man pages

[CS-266](https://hpc-gridware.atlassian.net/browse/CS-266) reformat sge\_qtask, sge\_queue\_conf, sge\_reporting man page

[CS-267](https://hpc-gridware.atlassian.net/browse/CS-267) reformat sge\_request, sge\_resource\_quota, sge\_sched\_conf man page

[CS-268](https://hpc-gridware.atlassian.net/browse/CS-268) reformat sge\_share\_tree, sge\_user man page

[CS-608](https://hpc-gridware.atlassian.net/browse/CS-608) add options to the parallel environment for multithreaded or multiprocess applications

[CS-611](https://hpc-gridware.atlassian.net/browse/CS-611) add option to the parallel environment to ignore slave requests for slave tasks on the master host

[CS-617](https://hpc-gridware.atlassian.net/browse/CS-617) reformat sge\_execd, sge\_qmaster, sge\_shadowd, sge\_shepherd

### Epic

[CS-41](https://hpc-gridware.atlassian.net/browse/CS-41) Provide man pages 

### Bug

[CS-614](https://hpc-gridware.atlassian.net/browse/CS-614) testsuite shows tcl\_files directory as part of the checktree

[CS-615](https://hpc-gridware.atlassian.net/browse/CS-615) qacct working on one line JSON accesses already freed memory

[CS-616](https://hpc-gridware.atlassian.net/browse/CS-616) Remove references to sgepasswd which is not required anymore

[CS-621](https://hpc-gridware.atlassian.net/browse/CS-621) final job cleanup broken on execd when keep\_active is reset from 'error' to default

[CS-622](https://hpc-gridware.atlassian.net/browse/CS-622) TS: create\_shell\_script\(\) does reuse old script file although arguments changed 

[CS-623](https://hpc-gridware.atlassian.net/browse/CS-623) TS: -scope tests fail in environments where primary hostname is fully qualified

[CS-627](https://hpc-gridware.atlassian.net/browse/CS-627) error during start of prolog/epilog/pe_start/pe_stop is interpreted as job error and not as queue error

## 9.0.0 RC3

### Improvement

[CS-592](https://hpc-gridware.atlassian.net/browse/CS-592) Zombie jobs show a different time stamp as normal jobs

### Epic

[CS-33](https://hpc-gridware.atlassian.net/browse/CS-33) Important Bug Fixes

### Bug

[CS-32](https://hpc-gridware.atlassian.net/browse/CS-32) fix tests which are not working with the current Cluster Scheduler version

[CS-282](https://hpc-gridware.atlassian.net/browse/CS-282) qmaster returns with error reason for completely different cause of error

[CS-348](https://hpc-gridware.atlassian.net/browse/CS-348) incorrect timestamp from qstat -j jobid -xml

[CS-575](https://hpc-gridware.atlassian.net/browse/CS-575) execution daemon installer fails when slots amount of  all.q should be changed

[CS-593](https://hpc-gridware.atlassian.net/browse/CS-593) memory leak during unpacking of incoming requests

[CS-594](https://hpc-gridware.atlassian.net/browse/CS-594) memory leak during thread init

[CS-595](https://hpc-gridware.atlassian.net/browse/CS-595) memory leak during initialize of reporting module

[CS-596](https://hpc-gridware.atlassian.net/browse/CS-596) loadcheck -loadval broken

[CS-597](https://hpc-gridware.atlassian.net/browse/CS-597) memory leak during subscription of events in mirror threads

[CS-598](https://hpc-gridware.atlassian.net/browse/CS-598) uninitialized attributes in packet data structure

[CS-603](https://hpc-gridware.atlassian.net/browse/CS-603) memory leak if xuser\_list causes rejection of incoming request

[CS-604](https://hpc-gridware.atlassian.net/browse/CS-604) memory leak during shutdown of event clients

[CS-605](https://hpc-gridware.atlassian.net/browse/CS-605) memory leak in profiling module during shutdown of qmaster

[CS-606](https://hpc-gridware.atlassian.net/browse/CS-606) multiple memory leaks during shutdown of qmaster in combination with classic spooling

[CS-607](https://hpc-gridware.atlassian.net/browse/CS-607) double free of memory required to handle incoming requests

[CS-610](https://hpc-gridware.atlassian.net/browse/CS-610) leak within pthread call during shutdown of scheduler

## 9.0.0 RC2

### Improvement

[CS-586](https://hpc-gridware.atlassian.net/browse/CS-586) testsuite: add an option to switch between random and deterministic test host selection

### Bug

[CS-569](https://hpc-gridware.atlassian.net/browse/CS-569) Category strings are not correct

[CS-571](https://hpc-gridware.atlassian.net/browse/CS-571) Translate EULA and add it to GCS packages

[CS-574](https://hpc-gridware.atlassian.net/browse/CS-574) information about job requests gets lost when restarting sge\_qmaster

## 9.0.0 RC1

### Improvement

[CS-46](https://hpc-gridware.atlassian.net/browse/CS-46) create user guide

### Sub-task

[CS-568](https://hpc-gridware.atlassian.net/browse/CS-568) make sure that qalter works correctly with the -scope switch

## 9.0.0alpha1

### Improvement

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

[CS-36](https://hpc-gridware.atlassian.net/browse/CS-36) accept kernel 6.x \(e.g. Ubuntu 22.04\)

[CS-37](https://hpc-gridware.atlassian.net/browse/CS-37) cache localised messages

[CS-42](https://hpc-gridware.atlassian.net/browse/CS-42) generate man pages from markdown

[CS-44](https://hpc-gridware.atlassian.net/browse/CS-44) create installation guide

[CS-58](https://hpc-gridware.atlassian.net/browse/CS-58) switch default build OSes to newer versions

[CS-62](https://hpc-gridware.atlassian.net/browse/CS-62) remove obsolete code for not supported architectures

[CS-68](https://hpc-gridware.atlassian.net/browse/CS-68) Adapt qdel, qhost, qhold and move env variable description

[CS-69](https://hpc-gridware.atlassian.net/browse/CS-69) allow to adjust complex entries individually with qconf

[CS-76](https://hpc-gridware.atlassian.net/browse/CS-76) add doc build to testsuite

[CS-81](https://hpc-gridware.atlassian.net/browse/CS-81) make mk\_dist generatate a doc package

[CS-93](https://hpc-gridware.atlassian.net/browse/CS-93) port to FreeBSD

[CS-99](https://hpc-gridware.atlassian.net/browse/CS-99) perl client JSV not executed on fbsd-amd64

[CS-102](https://hpc-gridware.atlassian.net/browse/CS-102) make admin\_only\_hosts parameter active in installed OGE system

[CS-114](https://hpc-gridware.atlassian.net/browse/CS-114) add build option to use OS provided packages for 3rdparty tools

[CS-130](https://hpc-gridware.atlassian.net/browse/CS-130) Optimise bootstrap module

[CS-133](https://hpc-gridware.atlassian.net/browse/CS-133) merge code from RSMAP branch

[CS-134](https://hpc-gridware.atlassian.net/browse/CS-134) re-implement code which is only available as stubs

[CS-136](https://hpc-gridware.atlassian.net/browse/CS-136) remove the init\_level function from tests

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

[CS-239](https://hpc-gridware.atlassian.net/browse/CS-239) allow TS to build with extensions

[CS-269](https://hpc-gridware.atlassian.net/browse/CS-269) Allow listener threads to handle permission requests independent from worker threads

[CS-293](https://hpc-gridware.atlassian.net/browse/CS-293) module test should be executable within CLion

[CS-295](https://hpc-gridware.atlassian.net/browse/CS-295) lx-ppc64le port

[CS-297](https://hpc-gridware.atlassian.net/browse/CS-297) allow to specify patterns for groups of usage values

[CS-298](https://hpc-gridware.atlassian.net/browse/CS-298) show the pe task id in qacct

[CS-299](https://hpc-gridware.atlassian.net/browse/CS-299) queue\_config: set default shell to /bin/sh and shell\_start\_mode to unix\_behavior

[CS-300](https://hpc-gridware.atlassian.net/browse/CS-300) Default installation parameters should be set for improving usability 

[CS-303](https://hpc-gridware.atlassian.net/browse/CS-303) add per test configuration variable\(s\) defining for which OGE versions the test shall be run

[CS-309](https://hpc-gridware.atlassian.net/browse/CS-309) add the submission command line to qstat -j <job\_id> and to accounting/reporting

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

[CS-383](https://hpc-gridware.atlassian.net/browse/CS-383) TS should allow to dump groovy files required to generate CI/CD jobs for a pipeline

[CS-384](https://hpc-gridware.atlassian.net/browse/CS-384) add global resources and per job consumables to testsuite test

[CS-410](https://hpc-gridware.atlassian.net/browse/CS-410) testsuite option compile\_clean shall not trigger a 3rdparty build

[CS-422](https://hpc-gridware.atlassian.net/browse/CS-422) add basic documentation for SIMULATE\_EXECDS to the developer documentation

[CS-444](https://hpc-gridware.atlassian.net/browse/CS-444) transfer supplementary group ids with job submission and consider them in matching ACLs

[CS-445](https://hpc-gridware.atlassian.net/browse/CS-445) cleanup: remove SGE\_PQS\_API

[CS-548](https://hpc-gridware.atlassian.net/browse/CS-548) Add product name replacement in supported packages

### New Feature

[CS-30](https://hpc-gridware.atlassian.net/browse/CS-30) Allow more than one data store within qmaster

[CS-308](https://hpc-gridware.atlassian.net/browse/CS-308) test\_uti\_profiling causes segmentation fault

### Epic

[CS-8](https://hpc-gridware.atlassian.net/browse/CS-8) Simplify development for the future

[CS-17](https://hpc-gridware.atlassian.net/browse/CS-17) replace aimk by a cmake based build

[CS-48](https://hpc-gridware.atlassian.net/browse/CS-48) re-enable Java DRMAA

[CS-177](https://hpc-gridware.atlassian.net/browse/CS-177) Need higher resolution in time stamps

### Sub-task

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

[CS-370](https://hpc-gridware.atlassian.net/browse/CS-370) enable JNI code in libdrmaa

[CS-371](https://hpc-gridware.atlassian.net/browse/CS-371) move Java code to a separate repository and build it as Maven project

[CS-372](https://hpc-gridware.atlassian.net/browse/CS-372) make sure Java DRMAA is tested in testsuite

[CS-380](https://hpc-gridware.atlassian.net/browse/CS-380) Test build and checktree\_jdrmaa in the Lab

[CS-395](https://hpc-gridware.atlassian.net/browse/CS-395) add HOST consumables to the complex definition

[CS-396](https://hpc-gridware.atlassian.net/browse/CS-396) respect HOST consumables in scheduling and resource booking

[CS-428](https://hpc-gridware.atlassian.net/browse/CS-428) make HOST consumables work with ARs

[CS-454](https://hpc-gridware.atlassian.net/browse/CS-454) make one line macros for gathering scheduling statistics

[CS-543](https://hpc-gridware.atlassian.net/browse/CS-543) Add new test that checks KEEP\_ACTIVE=ERROR behavior

### Task

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

[CS-318](https://hpc-gridware.atlassian.net/browse/CS-318) cleanup: use boolean logic on boolean data

[CS-324](https://hpc-gridware.atlassian.net/browse/CS-324) As a developer I need a Dockerfile with all dependencies in order to build Open Cluster Scheduler on Ubuntu 22.04

[CS-325](https://hpc-gridware.atlassian.net/browse/CS-325) As a developer I need a Dockerfile with all dependencies in order to build Open Cluster Scheduler on Rocky 9.3

[CS-326](https://hpc-gridware.atlassian.net/browse/CS-326) As a developer I need a Dockerfile with all dependencies in order to build Open Cluster Scheduler on Leap 15.4

[CS-420](https://hpc-gridware.atlassian.net/browse/CS-420) extend the testsuite framework with utility functions around exec host simulation

[CS-421](https://hpc-gridware.atlassian.net/browse/CS-421) create a testsuite test for basic functionality of exec host simulation

[CS-489](https://hpc-gridware.atlassian.net/browse/CS-489) Support ENABLE\_SUBMIT\_LD\_PRELOAD=true in qmaster\_params

[CS-490](https://hpc-gridware.atlassian.net/browse/CS-490) Support OLD\_RESCHEDULE\_BEHAVIOR=1 in qmaster\_params

[CS-491](https://hpc-gridware.atlassian.net/browse/CS-491) Support KEEP\_ACTIVE=ERROR in execd\_params

[CS-538](https://hpc-gridware.atlassian.net/browse/CS-538) Add dummy name/value lists to all major objects

### Bug

[CS-3](https://hpc-gridware.atlassian.net/browse/CS-3) Manual and auto installation broken

[CS-53](https://hpc-gridware.atlassian.net/browse/CS-53) installation with classic spooling fails due to spoolinit failing

[CS-54](https://hpc-gridware.atlassian.net/browse/CS-54) spooledit list does not output any data

[CS-60](https://hpc-gridware.atlassian.net/browse/CS-60) Remove non printable characters from mardown man pages

[CS-63](https://hpc-gridware.atlassian.net/browse/CS-63) add support for non-cluster and admin-only hosts in TS

[CS-100](https://hpc-gridware.atlassian.net/browse/CS-100) test DRMAA binary does not find shared library on fbsd-amd64

[CS-101](https://hpc-gridware.atlassian.net/browse/CS-101) some automated test report errors for operations on non-root accessible FSs

[CS-103](https://hpc-gridware.atlassian.net/browse/CS-103) qrsh reports a job as not schedule-able but job is executed

[CS-135](https://hpc-gridware.atlassian.net/browse/CS-135) testsuite complains about a lot of message macros not being used

[CS-137](https://hpc-gridware.atlassian.net/browse/CS-137) do not use true and false in TCL code

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

[CS-284](https://hpc-gridware.atlassian.net/browse/CS-284) in case of GDI version mismatch error message does not show hex version number

[CS-285](https://hpc-gridware.atlassian.net/browse/CS-285) listener threads do not reject requests with incompatible GDI version

[CS-286](https://hpc-gridware.atlassian.net/browse/CS-286) error during decryption of authentication information does not reject a GDI request in the listener.

[CS-287](https://hpc-gridware.atlassian.net/browse/CS-287) fake client with invalid authinfo is not rejected but might lead to core of qmaster

[CS-288](https://hpc-gridware.atlassian.net/browse/CS-288) in CSP mode all errors of sge\_security\_verify\_unique\_identifier\(\) do not cause request rejection in listener thread.

[CS-289](https://hpc-gridware.atlassian.net/browse/CS-289) license string in the binaries needs to be replaced

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

[CS-379](https://hpc-gridware.atlassian.net/browse/CS-379) packages installation does not work with TS if cluster does not contain hosts for all architectures

[CS-391](https://hpc-gridware.atlassian.net/browse/CS-391) cetrain test scenarios fail if they are executed individually

[CS-392](https://hpc-gridware.atlassian.net/browse/CS-392) testsuite does not return with an error if a test fails and reinit of the cluster is enabled

[CS-398](https://hpc-gridware.atlassian.net/browse/CS-398) cull packing test does not verify the result of unpacking

[CS-402](https://hpc-gridware.atlassian.net/browse/CS-402) Enable Jenkins CI/CD piplinte to test GCS

[CS-405](https://hpc-gridware.atlassian.net/browse/CS-405) qhost test fails sometimes.

[CS-407](https://hpc-gridware.atlassian.net/browse/CS-407) test\_drmaa sometimes dumped core

[CS-408](https://hpc-gridware.atlassian.net/browse/CS-408) some qstat test scenarios fail because queue instance names are truncated

[CS-409](https://hpc-gridware.atlassian.net/browse/CS-409) ncores is incorrectly reported as the number of threads

[CS-412](https://hpc-gridware.atlassian.net/browse/CS-412) New jsonl accounting file should not have comments at the beginning

[CS-418](https://hpc-gridware.atlassian.net/browse/CS-418) deleting jobs in a simulated cluster gets into "an endless loop"

[CS-419](https://hpc-gridware.atlassian.net/browse/CS-419) accounting of simulated jobs is missing basic information

[CS-426](https://hpc-gridware.atlassian.net/browse/CS-426) qstat -F should print the remaining capacity of RSMAPs as integer, not as double

[CS-431](https://hpc-gridware.atlassian.net/browse/CS-431) mirror 2 does not shutdown when qmaster is running in the foreground

[CS-465](https://hpc-gridware.atlassian.net/browse/CS-465) qstat returns negative values as microseconds

[CS-560](https://hpc-gridware.atlassian.net/browse/CS-560) Core of qstat -j <jid>

[CS-561](https://hpc-gridware.atlassian.net/browse/CS-561) Resource requests cannot be reset to NONE via JSV

