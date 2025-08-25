# Full List of Fixes

## v9.0.8

### Improvement

[CS-739](https://hpc-gridware.atlassian.net/browse/CS-739) qstat -j output should contain job state, start time, queue name, and host names

### Task

[CS-1407](https://hpc-gridware.atlassian.net/browse/CS-1407) Add SUSE SLES 15 support in support matrix of release notes

[CS-1440](https://hpc-gridware.atlassian.net/browse/CS-1440) Add qtelemetry licenses to GCS 3rdparty licenses directory

[CS-1470](https://hpc-gridware.atlassian.net/browse/CS-1470) do memory testing on V90\_BRANCH for the 9.0.8 release

### Sub-task

[CS-1394](https://hpc-gridware.atlassian.net/browse/CS-1394) Add start\_time of array jobs tasks to qstat -j <jid>

[CS-1395](https://hpc-gridware.atlassian.net/browse/CS-1395) Cleanup of job states and show states also in qstat -j <jid> output

[CS-1396](https://hpc-gridware.atlassian.net/browse/CS-1396) Show granted host information in qstat -j <jid> output

[CS-1404](https://hpc-gridware.atlassian.net/browse/CS-1404) Show granted queues in qstat -j <jid> output

[CS-1410](https://hpc-gridware.atlassian.net/browse/CS-1410) Show priority in qstat -j <jid> output even if it is the base priority

### Bug

[CS-671](https://hpc-gridware.atlassian.net/browse/CS-671) qrsh truncates the command line and/or output at 927 characters

[CS-1019](https://hpc-gridware.atlassian.net/browse/CS-1019) sge\_execd logs errors when running tightly integrated parallel jobs

[CS-1270](https://hpc-gridware.atlassian.net/browse/CS-1270) Installation script clears screen in case of an error which make issues harder to debug

[CS-1381](https://hpc-gridware.atlassian.net/browse/CS-1381) qacct complains "error: ignoring invalid entry in line 436" for accounting records with huge command line entry

[CS-1386](https://hpc-gridware.atlassian.net/browse/CS-1386) man page for sge\_share\_mon is missing

[CS-1403](https://hpc-gridware.atlassian.net/browse/CS-1403) sge\_ckpt man-page is in wrong section \(1 instead of 5\)

[CS-1422](https://hpc-gridware.atlassian.net/browse/CS-1422) endless loop in protocol between sge\_qmaster and sge\_execd in certain job failure situations

[CS-1424](https://hpc-gridware.atlassian.net/browse/CS-1424) qmod -sj on own job fails on submit only host

[CS-1429](https://hpc-gridware.atlassian.net/browse/CS-1429) sge\_qmaster can segfault on qdel -f

[CS-1434](https://hpc-gridware.atlassian.net/browse/CS-1434) clearing error state of a job leads to event callback error logging in qmaster messages file

[CS-1435](https://hpc-gridware.atlassian.net/browse/CS-1435) rescheduling of jobs requires manager rights, documented is "manager or operator rights"

[CS-1436](https://hpc-gridware.atlassian.net/browse/CS-1436) qmod man pages says it requires manager or operator privileges to rerun a job, but a job owner may rerun his own jobs as well

[CS-1451](https://hpc-gridware.atlassian.net/browse/CS-1451) option -out of examples/jobsbin/<arch>/work is broken

[CS-1476](https://hpc-gridware.atlassian.net/browse/CS-1476) Go DRMAA does not set JoinFiles\(\) correctly

[CS-1477](https://hpc-gridware.atlassian.net/browse/CS-1477) In Go DRMAA TransferFiles\(\) does not set all values

## v9.0.7

### Improvement

[CS-194](https://hpc-gridware.atlassian.net/browse/CS-194) add rss limits

[CS-315](https://hpc-gridware.atlassian.net/browse/CS-315) make sure that the accounting and reporting code is thread safe

[CS-443](https://hpc-gridware.atlassian.net/browse/CS-443) export environment variables to qrsh jobs without command

[CS-1303](https://hpc-gridware.atlassian.net/browse/CS-1303) Add qtelemetry description into the Admin Guide

### Task

[CS-1304](https://hpc-gridware.atlassian.net/browse/CS-1304) Enable OCS/GCS build with gcov

[CS-1309](https://hpc-gridware.atlassian.net/browse/CS-1309) Do lvoc run for GCS version 9.0.6

[CS-1310](https://hpc-gridware.atlassian.net/browse/CS-1310) Add bind tests to check manual start position for binding strategies

### Sub-task

[CS-1285](https://hpc-gridware.atlassian.net/browse/CS-1285) Split binding specific code in test suite and add modules that allow to implement new tests for scheduler binding

### Bug

[CS-1269](https://hpc-gridware.atlassian.net/browse/CS-1269) error reason reported in error file and shown by  qstat -j jobid is truncated

[CS-1281](https://hpc-gridware.atlassian.net/browse/CS-1281) testsuite fails in post-check tests when option "interactive ssh" is used

[CS-1286](https://hpc-gridware.atlassian.net/browse/CS-1286) Core binding \(striding strategy\) not applied although cores might be available.

[CS-1288](https://hpc-gridware.atlassian.net/browse/CS-1288) Core binding \(explicit strategy\) does not reserve cores correctly

[CS-1311](https://hpc-gridware.atlassian.net/browse/CS-1311) Error message in autoinstaller: MakeUserKs command not found

[CS-1323](https://hpc-gridware.atlassian.net/browse/CS-1323) code writing add\_grp\_id to shepherd config file is duplicated and looks incorrect

[CS-1340](https://hpc-gridware.atlassian.net/browse/CS-1340) testsuite installation fails with coverage testing enabled

[CS-1341](https://hpc-gridware.atlassian.net/browse/CS-1341) installation guide mentions scheduler profiles which do not exist

## v9.0.6

### Improvement

[CS-760](https://hpc-gridware.atlassian.net/browse/CS-760) JSONL output shows nested element \("usage"\) with the same name which can confuse and lead to parsing issues

[CS-1165](https://hpc-gridware.atlassian.net/browse/CS-1165) add a testsuite test for the ssh wrapper \(MPI\) template

[CS-1186](https://hpc-gridware.atlassian.net/browse/CS-1186) default PE settings trigger unnecessary calls to /bin/true

[CS-1195](https://hpc-gridware.atlassian.net/browse/CS-1195) make classic spooling the default

[CS-1196](https://hpc-gridware.atlassian.net/browse/CS-1196) the "starting up" info message should be the first message to print to the messages file at daemon startup

[CS-1256](https://hpc-gridware.atlassian.net/browse/CS-1256) allow one slave task to be started on the master host with the pe templates for mpich and mvapich

[CS-1257](https://hpc-gridware.atlassian.net/browse/CS-1257) in the MPI build scripts save the configure/make/make install log files into the installation directory

[CS-1258](https://hpc-gridware.atlassian.net/browse/CS-1258) log to execd messages file when and why sge\_execd rejects pe task requests \(qrsh -inherit\)

[CS-1261](https://hpc-gridware.atlassian.net/browse/CS-1261) qrsh -verbose shall log which transport client it uses and where the information comes from

[CS-1271](https://hpc-gridware.atlassian.net/browse/CS-1271) One line installer script should check if /etc/hosts is configured correctly to prevent installation issues

### Task

[CS-1059](https://hpc-gridware.atlassian.net/browse/CS-1059) verify sorting of JAT\_granted\_destin\_identifier\_list

[CS-1110](https://hpc-gridware.atlassian.net/browse/CS-1110) verify caching mechanism in qrsh -inherit

### Sub-task

[CS-645](https://hpc-gridware.atlassian.net/browse/CS-645) in schedule file print separator \*after\* writing the schedule records

[CS-1264](https://hpc-gridware.atlassian.net/browse/CS-1264) Clone and update Solaris 11 VM

### Bug

[CS-619](https://hpc-gridware.atlassian.net/browse/CS-619) queue limits which are overwritten on the submission command line have only effect if they are overwritten in the global scope

[CS-1132](https://hpc-gridware.atlassian.net/browse/CS-1132) reserved usage is not calculated for rss

[CS-1193](https://hpc-gridware.atlassian.net/browse/CS-1193) uninstalling a sge\_execd on the master host fails with "denied: can't delete master host "ubuntu-24-amd64-1" from admin host list"

[CS-1227](https://hpc-gridware.atlassian.net/browse/CS-1227) \(Auto\)installer fails to install OCS 9.0.5 packages on Rocky Linux 9

[CS-1231](https://hpc-gridware.atlassian.net/browse/CS-1231) Auto install fails on openSUSE Leap 15.6 during copying rc template

[CS-1267](https://hpc-gridware.atlassian.net/browse/CS-1267) broken build on Solaris 11 after update

## v9.0.5

### Improvement

[CS-342](https://hpc-gridware.atlassian.net/browse/CS-342) provide an openmpi integration

[CS-343](https://hpc-gridware.atlassian.net/browse/CS-343) provide an example and test program using MPI

[CS-791](https://hpc-gridware.atlassian.net/browse/CS-791) sge\_root should be available as special variable in the configuration of prolog, epilog, queue, pe, ckpt

[CS-914](https://hpc-gridware.atlassian.net/browse/CS-914) Make ARCH script more robust

[CS-1090](https://hpc-gridware.atlassian.net/browse/CS-1090) qstat -r shall report resource requests by scope

[CS-1094](https://hpc-gridware.atlassian.net/browse/CS-1094) Update sge\_pe.md to better explain PE\_HOSTFILE

[CS-1114](https://hpc-gridware.atlassian.net/browse/CS-1114) Add GPU monitoring examples to qtelemetry Grafana dashboard

[CS-1115](https://hpc-gridware.atlassian.net/browse/CS-1115) Build qtelemetry in containers for lx-amd64 and lx-arm64

[CS-1126](https://hpc-gridware.atlassian.net/browse/CS-1126) in the environment of tasks of tightly integrated parallel jobs set the pe\_task\_id

[CS-1128](https://hpc-gridware.atlassian.net/browse/CS-1128) Add enroot to worker GPU VM image for GCP

[CS-1143](https://hpc-gridware.atlassian.net/browse/CS-1143) provide a MPICH integration

[CS-1144](https://hpc-gridware.atlassian.net/browse/CS-1144) provide a MVAPICH integration

[CS-1145](https://hpc-gridware.atlassian.net/browse/CS-1145) provide an Intel MPI integration

[CS-1146](https://hpc-gridware.atlassian.net/browse/CS-1146) cleanup and document the ssh wrapper MPI template and scripts

[CS-1152](https://hpc-gridware.atlassian.net/browse/CS-1152) add a checktree\_mpi to testsuite with configuration and tests making use of the various MPI integrations

[CS-1158](https://hpc-gridware.atlassian.net/browse/CS-1158) Add qtelemetry Grafana dashboard to public Grafana Cloud Dashboards

### New Feature

[CS-1091](https://hpc-gridware.atlassian.net/browse/CS-1091) Clearly document the slots syntax in man5 sge\_queue\_conf.md

### Sub-task

[CS-697](https://hpc-gridware.atlassian.net/browse/CS-697) Jenkins: enable issue\_3013

[CS-698](https://hpc-gridware.atlassian.net/browse/CS-698) Jenkins: enable issue\_3179

### Task

[CS-662](https://hpc-gridware.atlassian.net/browse/CS-662) verify delayed job reporting of sge\_execd after reconnecting to sge\_qmaster

[CS-1117](https://hpc-gridware.atlassian.net/browse/CS-1117) Add qtelemetry as developer preview to GCS distribution

[CS-1118](https://hpc-gridware.atlassian.net/browse/CS-1118) Create a packer file which builds a GPU enabled VM with and without GCS for fast deployment on GCP

[CS-1125](https://hpc-gridware.atlassian.net/browse/CS-1125) Provide a basic examples of how enroot can be used with the GPU integration

[CS-1134](https://hpc-gridware.atlassian.net/browse/CS-1134) message cutoff after 8 characters

[CS-1136](https://hpc-gridware.atlassian.net/browse/CS-1136) add checktree\_qtelemetry to all build environments \+ Jenkins setup

### Bug

[CS-430](https://hpc-gridware.atlassian.net/browse/CS-430) booking of resources into advance reservations needs to distinguish between host and queue resources

[CS-722](https://hpc-gridware.atlassian.net/browse/CS-722) env\_list in qstat should show NONE if not set

[CS-1028](https://hpc-gridware.atlassian.net/browse/CS-1028) qtelemetry should support NVIDIA loadsensor values for hosts

[CS-1085](https://hpc-gridware.atlassian.net/browse/CS-1085) BDB build error on lx-riscv64 after OS update.

[CS-1096](https://hpc-gridware.atlassian.net/browse/CS-1096) USE\_QSUB\_GID functionality fails on FreeBSD 14

[CS-1111](https://hpc-gridware.atlassian.net/browse/CS-1111) minimum and maximum thread counts in the bootstrap.5 man page are incorrect

[CS-1131](https://hpc-gridware.atlassian.net/browse/CS-1131) wallclock time reported for tasks of a tightly integrated parallel job is incorrect

[CS-1139](https://hpc-gridware.atlassian.net/browse/CS-1139) job deletion via JAPI/DRMAA fails if job ID exceeds INT\_MAX

[CS-1140](https://hpc-gridware.atlassian.net/browse/CS-1140) termination of event client via JAPI fails if event client ID exceeds INT\_MAX

[CS-1141](https://hpc-gridware.atlassian.net/browse/CS-1141) MacOS build broken due to unavailability of getgrouplist\(\)

[CS-1163](https://hpc-gridware.atlassian.net/browse/CS-1163) when a queue is signalled then additional invalid entries are created in the berkeleydb spooling database

## v9.0.4

### Improvement

[CS-566](https://hpc-gridware.atlassian.net/browse/CS-566) Add new test scenarios for JSV \[master|slave\]\_l\_hard attribute

### New Feature

[CS-777](https://hpc-gridware.atlassian.net/browse/CS-777) Improve qsub -sync so that is support multiple state \(like r\)

[CS-1056](https://hpc-gridware.atlassian.net/browse/CS-1056) Develop a User-Friendly ShareTree Editor for Admins to Easily Manage and View Large ShareTrees

### Sub-task

[CS-1035](https://hpc-gridware.atlassian.net/browse/CS-1035) Fix compile issues for macOS

[CS-1037](https://hpc-gridware.atlassian.net/browse/CS-1037) Fix build environment for macOS and build for OCS and GCS

[CS-1038](https://hpc-gridware.atlassian.net/browse/CS-1038) Fix remote login problem for macOS regular users

### Task

[CS-937](https://hpc-gridware.atlassian.net/browse/CS-937) do full valgrind test on V90\_BRANCH \(9.0.2/9.0.3\)

### Bug

[CS-634](https://hpc-gridware.atlassian.net/browse/CS-634) a job requesting a pty fails on freebsd

[CS-1022](https://hpc-gridware.atlassian.net/browse/CS-1022) qrsh to a freebsd host fails with "qrsh\_starter: executing child process bash failed: No such file or directory"

[CS-1023](https://hpc-gridware.atlassian.net/browse/CS-1023) qtelemetry reports hosts with long and short hostnames

[CS-1024](https://hpc-gridware.atlassian.net/browse/CS-1024) qdel reports jobs as "is already in deletion" although qstat shows that job in 'qw' state

[CS-1047](https://hpc-gridware.atlassian.net/browse/CS-1047) JSV might have access to job attributes of a previous job verification

[CS-1049](https://hpc-gridware.atlassian.net/browse/CS-1049) Fix MPI templates to contain ignore slave task requests on master host

[CS-1053](https://hpc-gridware.atlassian.net/browse/CS-1053) with pe setting ign\_sreq\_on\_mhost=true job having a slave request on a globally defined resource are not scheduled

[CS-1054](https://hpc-gridware.atlassian.net/browse/CS-1054) with pe setting ign\_sreq\_on\_mhost=true a globally defined resource which is requested for slave scope is not booked into the resource diagram

[CS-1055](https://hpc-gridware.atlassian.net/browse/CS-1055) sge prefix in man pages is not correctly replaced in certain man page files

## v9.0.3

### Improvement

[CS-400](https://hpc-gridware.atlassian.net/browse/CS-400) need different resource requests for master and slave tasks

[CS-558](https://hpc-gridware.atlassian.net/browse/CS-558) Install guide should explain automatic installation

[CS-986](https://hpc-gridware.atlassian.net/browse/CS-986) debug output \(DPRINTF etc., enabled via dl.\[c\]sh\) should contain a timestamp

[CS-987](https://hpc-gridware.atlassian.net/browse/CS-987) shadowd\_migrate test should be reported as unsupported instead of failed when the cluster set-up does not support running it

[CS-989](https://hpc-gridware.atlassian.net/browse/CS-989) add testsuite option to stop testing if a valgrind error is found

[CS-996](https://hpc-gridware.atlassian.net/browse/CS-996) sge\_qmaster internal gdi requests do not need to initialize and later parse authentication information

[CS-1010](https://hpc-gridware.atlassian.net/browse/CS-1010) qmaster complains about event client not properly initialized

[CS-1016](https://hpc-gridware.atlassian.net/browse/CS-1016) Provide a description of using Podman with Gridware Cluster Scheduler in the Admin Guide

[CS-1017](https://hpc-gridware.atlassian.net/browse/CS-1017) Provide a description of resource reservation in the Admin Guide

### Epic

[CS-173](https://hpc-gridware.atlassian.net/browse/CS-173) Job counter should be greater than 10 mio.

### Task

[CS-945](https://hpc-gridware.atlassian.net/browse/CS-945) Adapt cmake to change version strings on various places when sgeobj/ocs\_Version.cc is changed

[CS-1006](https://hpc-gridware.atlassian.net/browse/CS-1006) Release notes are not available for OCS on the web server

### Sub-task

[CS-929](https://hpc-gridware.atlassian.net/browse/CS-929) Enhance the gen\_types tool to generate lwdb object files

[CS-932](https://hpc-gridware.atlassian.net/browse/CS-932) documents all lwdb methods

### Bug

[CS-630](https://hpc-gridware.atlassian.net/browse/CS-630) several testsuite tests fail in environments where primary hostname is fully qualified

[CS-715](https://hpc-gridware.atlassian.net/browse/CS-715) Describe binary replacement for patch release in the installation guide

[CS-921](https://hpc-gridware.atlassian.net/browse/CS-921) behavior of qalter is unclear regarding the modification of command arguments

[CS-959](https://hpc-gridware.atlassian.net/browse/CS-959) sge\_qmaster shutdown takes too long

[CS-981](https://hpc-gridware.atlassian.net/browse/CS-981) start of advance reservation can be slightly delayed

[CS-990](https://hpc-gridware.atlassian.net/browse/CS-990) Active reservations for parallel jobs will cause core of master during restart

[CS-992](https://hpc-gridware.atlassian.net/browse/CS-992) Scheduler makes no reservation for jobs after execd or sometimes qmaster restart

[CS-994](https://hpc-gridware.atlassian.net/browse/CS-994) Instead of reserving slots for a PE job it is started immediately

[CS-997](https://hpc-gridware.atlassian.net/browse/CS-997) Output shows negative timestamps for resource bookings

[CS-998](https://hpc-gridware.atlassian.net/browse/CS-998) Scheduler creates unnecessary resource schedules that cost performance

## v9.0.2

### Improvement

[CS-270](https://hpc-gridware.atlassian.net/browse/CS-270) Allow threads to handle RO-request in parallel

[CS-305](https://hpc-gridware.atlassian.net/browse/CS-305) Enhance testsuite framework function is\_version\_in\_range to allow multiple versions and optionally customer specific versions

[CS-323](https://hpc-gridware.atlassian.net/browse/CS-323) Allow synchronized access to data stores by adding session 

[CS-386](https://hpc-gridware.atlassian.net/browse/CS-386) Race condition in TS when multiple TS instances use the same test source directory

[CS-768](https://hpc-gridware.atlassian.net/browse/CS-768) allow testsuite to bootstrap its configuration from an existing cluster

[CS-769](https://hpc-gridware.atlassian.net/browse/CS-769) cloud deployment: evaluate and possibly improve Terraform script to testsuite needs

[CS-770](https://hpc-gridware.atlassian.net/browse/CS-770) make performance/throughput test run in the cloud environment

[CS-773](https://hpc-gridware.atlassian.net/browse/CS-773) add a runlevel to the performance/throughput test doing massive read only requests while submitting and running the test jobs

[CS-781](https://hpc-gridware.atlassian.net/browse/CS-781) Create spec for binding and memory binding

[CS-782](https://hpc-gridware.atlassian.net/browse/CS-782) create test application for hwloc topology detection

[CS-784](https://hpc-gridware.atlassian.net/browse/CS-784) cloud deployment: \(optionally\) also install an execution daemon on the master node

[CS-786](https://hpc-gridware.atlassian.net/browse/CS-786) cloud deployment: \(optionally\) add the user's ssh public key to sgetest's authorized keys

[CS-787](https://hpc-gridware.atlassian.net/browse/CS-787) testsuite: add menu item resetting the testsuite configuration to defaults without re-installing

[CS-790](https://hpc-gridware.atlassian.net/browse/CS-790) cloud deployment: install mailutils on the head node

[CS-796](https://hpc-gridware.atlassian.net/browse/CS-796) cloud deployment: make sure that core size limit is set to unlimited on master and exec hosts

[CS-798](https://hpc-gridware.atlassian.net/browse/CS-798) testsuite: throughput test sometimes fails with "TCL Error domain error: argument not in valid range"

[CS-800](https://hpc-gridware.atlassian.net/browse/CS-800) cloud deployment: need to configure the bucket name

[CS-827](https://hpc-gridware.atlassian.net/browse/CS-827) send scheduler start orders during a scheduling run more often

[CS-829](https://hpc-gridware.atlassian.net/browse/CS-829) testsuite: add option to install / configure cluster without local host configurations

[CS-832](https://hpc-gridware.atlassian.net/browse/CS-832) cloud deployment: make sure that the master-instance is started first

[CS-863](https://hpc-gridware.atlassian.net/browse/CS-863) testsuite: while or after scanning the checktree a high number of ssh connections is opened

[CS-871](https://hpc-gridware.atlassian.net/browse/CS-871) monitor cpu and memory usage of the expect process itself to identify testsuite itself being a bottleneck

[CS-874](https://hpc-gridware.atlassian.net/browse/CS-874) allow to specify the OS image for the head node installation

[CS-877](https://hpc-gridware.atlassian.net/browse/CS-877) improvements to the performance/throughput test

[CS-887](https://hpc-gridware.atlassian.net/browse/CS-887) create checktree\_qgpu in testsuite for building and testing qgpu

[CS-898](https://hpc-gridware.atlassian.net/browse/CS-898) Write qmaster monitoring metrics to a monitoring file

### New Feature

[CS-215](https://hpc-gridware.atlassian.net/browse/CS-215) Loadsensor for reporting GPU metrics

[CS-216](https://hpc-gridware.atlassian.net/browse/CS-216) Provide GPU accounting facility per job

[CS-753](https://hpc-gridware.atlassian.net/browse/CS-753) HowTo: Allow users to find the error reason for a job or queue

[CS-927](https://hpc-gridware.atlassian.net/browse/CS-927) Add MIG support to qgpu loadsensor

### Epic

[CS-214](https://hpc-gridware.atlassian.net/browse/CS-214) Add Support for NVIDIA Grace Hopper Platform

### Sub-task

[CS-278](https://hpc-gridware.atlassian.net/browse/CS-278) add session concept for incoming requests

[CS-279](https://hpc-gridware.atlassian.net/browse/CS-279) store requests in different request queues

[CS-340](https://hpc-gridware.atlassian.net/browse/CS-340) testsuite configuration options for the cmake build

[CS-668](https://hpc-gridware.atlassian.net/browse/CS-668) Add a GDI session store and feed it with data from mirror threads

[CS-669](https://hpc-gridware.atlassian.net/browse/CS-669) Introduce queue for requests waiting for secondary DS

[CS-686](https://hpc-gridware.atlassian.net/browse/CS-686) Jenkins: enable config\_qmaster\_param\_keep\_active

[CS-694](https://hpc-gridware.atlassian.net/browse/CS-694) Jenkins: enable issue\_1473

[CS-695](https://hpc-gridware.atlassian.net/browse/CS-695) Jenkins: enable issue\_1741

[CS-725](https://hpc-gridware.atlassian.net/browse/CS-725) striding binding is broken if automatic striding algorithm does not find a position for the first used core on the first socket.

[CS-726](https://hpc-gridware.atlassian.net/browse/CS-726) explicit core binding test does only work with hosts having up to two cores

[CS-743](https://hpc-gridware.atlassian.net/browse/CS-743) All execds should fetch configuration updates from reader DS/threads

[CS-744](https://hpc-gridware.atlassian.net/browse/CS-744) DRMAA requests can be handled by secondary DS if sessions are enabled

[CS-745](https://hpc-gridware.atlassian.net/browse/CS-745) Allow to disable automatic sessions \(to allow performance measurements\)

[CS-746](https://hpc-gridware.atlassian.net/browse/CS-746) Improve automatic session performance with 10k of execution hosts

[CS-747](https://hpc-gridware.atlassian.net/browse/CS-747) Show waiting reader queue length in monitoring

[CS-755](https://hpc-gridware.atlassian.net/browse/CS-755) reduce wait time for pending read requests \(with enabled sessions\)

[CS-757](https://hpc-gridware.atlassian.net/browse/CS-757) Create a module test for sessions that also measures performance with 5M sessions

[CS-779](https://hpc-gridware.atlassian.net/browse/CS-779) testsuite performance/throughput test: need a more efficient way to read accounting data

[CS-793](https://hpc-gridware.atlassian.net/browse/CS-793) Add -sdv switch to qstat that allows to hide queues and jobs from qstat that do not provide access or where user does not belong to

[CS-794](https://hpc-gridware.atlassian.net/browse/CS-794) Department is not visible in qstat -j output

[CS-795](https://hpc-gridware.atlassian.net/browse/CS-795) Verification of department lists is incorrect

[CS-803](https://hpc-gridware.atlassian.net/browse/CS-803) Add -sda switch to qhost and filter data according to same rules as for qstat

[CS-804](https://hpc-gridware.atlassian.net/browse/CS-804) Add support for sge\_qhost and .sge\_qhost files

[CS-805](https://hpc-gridware.atlassian.net/browse/CS-805) Adapt man pages for qstat and qhost that document the department view

[CS-806](https://hpc-gridware.atlassian.net/browse/CS-806) Change sge\_access\_list and document behaviour if user is part of multiple departments

[CS-807](https://hpc-gridware.atlassian.net/browse/CS-807) Add new switch to submit commands and qalter that allows to define department 

[CS-808](https://hpc-gridware.atlassian.net/browse/CS-808) Hide objects in qstat if parent objects does not allow access

[CS-809](https://hpc-gridware.atlassian.net/browse/CS-809) Hide objects in qhost if parent objects do not give access

[CS-810](https://hpc-gridware.atlassian.net/browse/CS-810) Hide jobs in qstat/qhost that that do not belong to users part of the same department

[CS-811](https://hpc-gridware.atlassian.net/browse/CS-811) Summarize identical code of qstat/qhost

[CS-812](https://hpc-gridware.atlassian.net/browse/CS-812) Enforce full view in qstat/qhost for managers

[CS-813](https://hpc-gridware.atlassian.net/browse/CS-813) Apply the same filter rules for the xml output that are also applied for the  plain output

[CS-814](https://hpc-gridware.atlassian.net/browse/CS-814) Move service functions used by multiple clients to sgeobj and document the code

[CS-815](https://hpc-gridware.atlassian.net/browse/CS-815) Remove the incorrect department tests in qmaster.

[CS-816](https://hpc-gridware.atlassian.net/browse/CS-816) Add doxygen comments for functions touched with the CS-748 enhancements

[CS-839](https://hpc-gridware.atlassian.net/browse/CS-839) Add a CentOS 6.10 VM in our Lab

[CS-856](https://hpc-gridware.atlassian.net/browse/CS-856) Install python build environment for all architectures that allow to run qmaster

[CS-857](https://hpc-gridware.atlassian.net/browse/CS-857) Create  pybind11 compatible GCS library

[CS-859](https://hpc-gridware.atlassian.net/browse/CS-859) Adapt TS so that additional cmake options can be specified via TS parameter

[CS-869](https://hpc-gridware.atlassian.net/browse/CS-869) Embed python interpreter in qmaster and enable it in all reader threads

[CS-875](https://hpc-gridware.atlassian.net/browse/CS-875) Rewrite version module, make information available vi C\+\+ class and expose it to python

[CS-878](https://hpc-gridware.atlassian.net/browse/CS-878) do not use CHECK\_USER \(admin user\) for cluster actions \(qsub, qstat\)

[CS-879](https://hpc-gridware.atlassian.net/browse/CS-879) use a higher number of reader threads

[CS-880](https://hpc-gridware.atlassian.net/browse/CS-880) do not limit the number of submit and qstat hosts

[CS-881](https://hpc-gridware.atlassian.net/browse/CS-881) in mixed\_with\_qstat scenario use a qstat request which is independent on the number of jobs to get consistent behavior

[CS-889](https://hpc-gridware.atlassian.net/browse/CS-889) Add a new class that represents a DataStore on client side and make it available for Python.

[CS-890](https://hpc-gridware.atlassian.net/browse/CS-890) PyCharm code completion not working for the ocs-bridge

[CS-903](https://hpc-gridware.atlassian.net/browse/CS-903) auto \(un\)installation not covered by the installation guide

[CS-925](https://hpc-gridware.atlassian.net/browse/CS-925) Create CULL interface to create/destroy  descriptors dynamically

[CS-926](https://hpc-gridware.atlassian.net/browse/CS-926) Add methods to create a full descriptor with a reduced set of attributes

### Task

[CS-226](https://hpc-gridware.atlassian.net/browse/CS-226) eliminate gdi function wrapper and use the same prefix

[CS-316](https://hpc-gridware.atlassian.net/browse/CS-316) replace deprecated function calls

[CS-719](https://hpc-gridware.atlassian.net/browse/CS-719) do full valgrind test on master branch \(9.0.1\)

[CS-758](https://hpc-gridware.atlassian.net/browse/CS-758) Document the automated build, release and publish processes.

[CS-759](https://hpc-gridware.atlassian.net/browse/CS-759) Write blog article about secondary DS and automatic session

[CS-765](https://hpc-gridware.atlassian.net/browse/CS-765) Allow to compile GCS on CentOs 6

[CS-820](https://hpc-gridware.atlassian.net/browse/CS-820) create a module test for the fifo trylock implementation

[CS-828](https://hpc-gridware.atlassian.net/browse/CS-828) Provide ready to use qgpu loadsensor / prolog and epilog scripts

[CS-891](https://hpc-gridware.atlassian.net/browse/CS-891) Verify GCS support on NVIDIA Grace Hopper Superchip

### Bug

[CS-276](https://hpc-gridware.atlassian.net/browse/CS-276) The default queue configuration contains /bin/csh but csh is not per default installed in most Linux distros

[CS-290](https://hpc-gridware.atlassian.net/browse/CS-290) add a build step that allows to generate CULL header from JSON files

[CS-321](https://hpc-gridware.atlassian.net/browse/CS-321) sorting tests by duration is broken

[CS-632](https://hpc-gridware.atlassian.net/browse/CS-632) Event master does not clean up event clients and their data at shutdown

[CS-716](https://hpc-gridware.atlassian.net/browse/CS-716) Describe side by side upgrade in installation guide.

[CS-720](https://hpc-gridware.atlassian.net/browse/CS-720) reset of PDC\_INTERVAL is delayed by one interval

[CS-721](https://hpc-gridware.atlassian.net/browse/CS-721) Testsuite: Submitted jobs are 'invisible' in tests if scenarios are executed under Jenkins control and when job runtime is smaller 30s

[CS-763](https://hpc-gridware.atlassian.net/browse/CS-763) testsuite: deletion of global temp files is broken

[CS-778](https://hpc-gridware.atlassian.net/browse/CS-778) testsuite: performance/throughput test does not generate any reports

[CS-797](https://hpc-gridware.atlassian.net/browse/CS-797) testsuite: throughput test sometimes reports "we had job submission errors - job got submitted, but qsub reported an error"

[CS-802](https://hpc-gridware.atlassian.net/browse/CS-802) sge\_qmaster core dump when processing qstat request

[CS-819](https://hpc-gridware.atlassian.net/browse/CS-819) potential sge\_qmaster core dump at startup

[CS-848](https://hpc-gridware.atlassian.net/browse/CS-848) Update logchecker.sh script to support new accounting and reporting files

[CS-867](https://hpc-gridware.atlassian.net/browse/CS-867) performance/throughput with 30.000 jobs fails on Ubuntu 22 when writing its test data to file

[CS-882](https://hpc-gridware.atlassian.net/browse/CS-882) Jobs stuck in t-state although they have been delivered to execd.

[CS-884](https://hpc-gridware.atlassian.net/browse/CS-884) There is a session 0 created although it is not required

[CS-886](https://hpc-gridware.atlassian.net/browse/CS-886) qresub fails for jobs that are part of the defaultdepartment

[CS-893](https://hpc-gridware.atlassian.net/browse/CS-893) Fix TS issues on CentOS 6

[CS-894](https://hpc-gridware.atlassian.net/browse/CS-894) testsuite: start\_remote\_prog will never time out if the executed command is constantly doing output

[CS-895](https://hpc-gridware.atlassian.net/browse/CS-895) testsuite: function drmaa\_test is duplicated

[CS-896](https://hpc-gridware.atlassian.net/browse/CS-896) resource reservation test fails due to too low h\_vmem limit

[CS-897](https://hpc-gridware.atlassian.net/browse/CS-897) testsuite reports lots of errors when initializing the messages cache and OCS/GCS is not yet installed

[CS-908](https://hpc-gridware.atlassian.net/browse/CS-908) qevent reports too high running job count

[CS-910](https://hpc-gridware.atlassian.net/browse/CS-910) updates of job priority and ticket values done by scheduler are not visible in qstat

[CS-915](https://hpc-gridware.atlassian.net/browse/CS-915) job is assigned to the defaultdepartment despite the user being member of an other department

[CS-917](https://hpc-gridware.atlassian.net/browse/CS-917) updates of job states due to slotwise suspend on subordinate are not visible in qstat

[CS-918](https://hpc-gridware.atlassian.net/browse/CS-918) some job modifications done with qalter are not visible in qstat -j job\_id

[CS-922](https://hpc-gridware.atlassian.net/browse/CS-922) jobs are not shown in dr state after deleting them with qdel

[CS-947](https://hpc-gridware.atlassian.net/browse/CS-947) Wait for events of 'own' session only and allow to disable cluster-wide sessions

## v9.0.1

### Improvement

[CS-270](https://hpc-gridware.atlassian.net/browse/CS-270) Allow threads to handle RO-request in parallel

[CS-546](https://hpc-gridware.atlassian.net/browse/CS-546) Support download of bundles from the WEB-server

[CS-580](https://hpc-gridware.atlassian.net/browse/CS-580) memory checking with valgrind

[CS-650](https://hpc-gridware.atlassian.net/browse/CS-650) testsuite remote connections shall not autologout

[CS-713](https://hpc-gridware.atlassian.net/browse/CS-713) print more information about shutting down the individual qmaster threads / thread pools to the messages file at qmaster shutdown

### New Feature

[CS-578](https://hpc-gridware.atlassian.net/browse/CS-578) Allow to measure thread performance with Google Performance Tools

### Epic

[CS-589](https://hpc-gridware.atlassian.net/browse/CS-589) cleanup memory \(and possibly other resources\) at process end

### Sub-task

[CS-277](https://hpc-gridware.atlassian.net/browse/CS-277) introduce unique event IDs

[CS-281](https://hpc-gridware.atlassian.net/browse/CS-281) add a pool of threads for each request queue

[CS-582](https://hpc-gridware.atlassian.net/browse/CS-582) do basic memory testing of OCS/GCS daemons and clients

[CS-583](https://hpc-gridware.atlassian.net/browse/CS-583) add valgrind testing to the testsuite framework

[CS-655](https://hpc-gridware.atlassian.net/browse/CS-655) Block requests for secondary DS until initialization is complete

[CS-656](https://hpc-gridware.atlassian.net/browse/CS-656) Allow to enforce that execd requests will be handled with data from primary DS

[CS-657](https://hpc-gridware.atlassian.net/browse/CS-657) Allow to configure the max reader delay

[CS-658](https://hpc-gridware.atlassian.net/browse/CS-658) Allow to disable use of secondary DS

[CS-659](https://hpc-gridware.atlassian.net/browse/CS-659) Increase the max  number of allowed listener/reader/worker threads

[CS-667](https://hpc-gridware.atlassian.net/browse/CS-667) Disable the use of the reader DS unless automatic sessions are available

[CS-675](https://hpc-gridware.atlassian.net/browse/CS-675) Enhance monitoring so that it can observe more than 256 threads

[CS-687](https://hpc-gridware.atlassian.net/browse/CS-687) Jenkins: enable config\_user\_xuser

[CS-689](https://hpc-gridware.atlassian.net/browse/CS-689) Jenkins: enable pe\_user\_xuser

[CS-690](https://hpc-gridware.atlassian.net/browse/CS-690) Jenkins: enable project\_user\_xuser

[CS-691](https://hpc-gridware.atlassian.net/browse/CS-691) Jenkins: enable queue\_user\_xuser

[CS-692](https://hpc-gridware.atlassian.net/browse/CS-692) Jenkins: enable rqs\_user\_xuser

[CS-704](https://hpc-gridware.atlassian.net/browse/CS-704) Jenkins: enable x\_forks\_slaves

### Bug

[CS-145](https://hpc-gridware.atlassian.net/browse/CS-145) qping monitoring output is broken

[CS-663](https://hpc-gridware.atlassian.net/browse/CS-663) In a default installation on Google Cloud the hostname length is too long for commands

[CS-665](https://hpc-gridware.atlassian.net/browse/CS-665) Potential core of qmaster \(in listener thread\) due to concurrent access to data

[CS-674](https://hpc-gridware.atlassian.net/browse/CS-674) Monitoring show threads in W or E state although they are working properly

[CS-676](https://hpc-gridware.atlassian.net/browse/CS-676) Improve qmaster shutdown performance for scenarios where the master has more that 128 threads

[CS-677](https://hpc-gridware.atlassian.net/browse/CS-677) Version in documentation is not changed automatically

[CS-678](https://hpc-gridware.atlassian.net/browse/CS-678) Incorrect format in qstat man page and manuals

[CS-679](https://hpc-gridware.atlassian.net/browse/CS-679) test: tight integration test fails if cluster is installed on FS where root is mapped to nobody

[CS-681](https://hpc-gridware.atlassian.net/browse/CS-681) testsuite test jsv\_issues leaves a global sge\_request file with a -jsv option

[CS-682](https://hpc-gridware.atlassian.net/browse/CS-682) qacct returns error state != 0 in common non-error scenario

[CS-683](https://hpc-gridware.atlassian.net/browse/CS-683) a MirrorDataStore is not always properly cleaned up at sge\_qmaster shutdown when the reader thread pool is enabled

[CS-684](https://hpc-gridware.atlassian.net/browse/CS-684) found workaround for: JSV stderr output contains "dpkg: warning: ..."

[CS-714](https://hpc-gridware.atlassian.net/browse/CS-714) shutting down the listener thread pool takes too long

## v9.0.0

### Improvement

[CS-629](https://hpc-gridware.atlassian.net/browse/CS-629) testsuite: enhance inefficiant deletion of local tmp files

### Bug

[CS-636](https://hpc-gridware.atlassian.net/browse/CS-636) CULL function lSetObject\(\) can not be used for clearing sub object \(setting nullptr\)

[CS-637](https://hpc-gridware.atlassian.net/browse/CS-637) PE attribute ign\_sreq\_on\_mhost is not considered in booking to the resource diagram

[CS-638](https://hpc-gridware.atlassian.net/browse/CS-638) PE attribute ign\_sreq\_on\_mhost is not considered in  resource quota booking

[CS-639](https://hpc-gridware.atlassian.net/browse/CS-639) testsuite function qstat\_F\_plain\_parse truncates long queue/host names

[CS-644](https://hpc-gridware.atlassian.net/browse/CS-644) gdi\_timeout, gdi\_retries and cl\_ping settings are ignored by client commands when defaults are overwritten

[CS-648](https://hpc-gridware.atlassian.net/browse/CS-648) qstat -j jobid -xml outputs the current time for JB\_execution\_time and JB\_deadline\_time instead of 0 when no execution time or deadline is set

[CS-649](https://hpc-gridware.atlassian.net/browse/CS-649) an invalid epilog does not set a queue error state

[CS-651](https://hpc-gridware.atlassian.net/browse/CS-651) testsuite does not build on architectures which are only required for configured non cluster hosts

[CS-652](https://hpc-gridware.atlassian.net/browse/CS-652) testsuite usage test fails from time to time with error message "job terminated abnormally \(1\), master host ..."

[CS-653](https://hpc-gridware.atlassian.net/browse/CS-653) a tightly integrated parallel job running in a pe with job\_is\_first\_task = FALSE and with limits set might get killed erroneously

## v9.0.0 RC4

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

## v9.0.0 RC3

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

## v9.0.0 RC2

### Improvement

[CS-586](https://hpc-gridware.atlassian.net/browse/CS-586) testsuite: add an option to switch between random and deterministic test host selection

### Bug

[CS-569](https://hpc-gridware.atlassian.net/browse/CS-569) Category strings are not correct

[CS-571](https://hpc-gridware.atlassian.net/browse/CS-571) Translate EULA and add it to GCS packages

[CS-574](https://hpc-gridware.atlassian.net/browse/CS-574) information about job requests gets lost when restarting sge\_qmaster

## v9.0.0 RC1

### Improvement

[CS-46](https://hpc-gridware.atlassian.net/browse/CS-46) create user guide

### Sub-task

[CS-568](https://hpc-gridware.atlassian.net/browse/CS-568) make sure that qalter works correctly with the -scope switch

## v9.0.0alpha1

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

[//]: # (Eeach file has to end with two emty lines)

