# Full List of Fixes

# Release notes - Cluster Scheduler - 9.1.0

## v9.1.0alpha1

### Improvement

[CS-194](https://hpc-gridware.atlassian.net/browse/CS-194) add rss limits

[CS-206](https://hpc-gridware.atlassian.net/browse/CS-206) Introduce new GDI category objects

[CS-207](https://hpc-gridware.atlassian.net/browse/CS-207) Trigger category add/del as part of job add/del

[CS-208](https://hpc-gridware.atlassian.net/browse/CS-208) Replace scheduler categories by GDI categories

[CS-209](https://hpc-gridware.atlassian.net/browse/CS-209) Implement client functionality to show details about categories

[CS-210](https://hpc-gridware.atlassian.net/browse/CS-210) Document qconf -scat and -scatl

[CS-220](https://hpc-gridware.atlassian.net/browse/CS-220) Implement limit counter in the listener thread pool

[CS-221](https://hpc-gridware.atlassian.net/browse/CS-221) Handle exceeded limits in listener threads and respond in clients

[CS-222](https://hpc-gridware.atlassian.net/browse/CS-222) Add support for user lists and host groups when listener threads have own data store

[CS-223](https://hpc-gridware.atlassian.net/browse/CS-223) create man page documentation that describe how to prevent DOS attacks

[CS-225](https://hpc-gridware.atlassian.net/browse/CS-225) add gdi\_request\_limit test scenarios to test suite

[CS-342](https://hpc-gridware.atlassian.net/browse/CS-342) provide an openmpi integration

[CS-343](https://hpc-gridware.atlassian.net/browse/CS-343) provide an example and test program using MPI

[CS-443](https://hpc-gridware.atlassian.net/browse/CS-443) export environment variables to qrsh jobs without command

[CS-566](https://hpc-gridware.atlassian.net/browse/CS-566) Add new test scenarios for JSV \[master|slave\]\_l\_hard attribute

[CS-727](https://hpc-gridware.atlassian.net/browse/CS-727) Fix CPU Core binding tests and known bugs

[CS-761](https://hpc-gridware.atlassian.net/browse/CS-761) qstat -j \* returns an error when there is no job

[CS-791](https://hpc-gridware.atlassian.net/browse/CS-791) sge\_root should be available as special variable in the configuration of prolog, epilog, queue, pe, ckpt

[CS-914](https://hpc-gridware.atlassian.net/browse/CS-914) Make ARCH script more robust

[CS-916](https://hpc-gridware.atlassian.net/browse/CS-916) Cull packing can be optimized by eliminating the packing hacks

[CS-986](https://hpc-gridware.atlassian.net/browse/CS-986) debug output \(DPRINTF etc., enabled via dl.\[c\]sh\) should contain a timestamp

[CS-987](https://hpc-gridware.atlassian.net/browse/CS-987) shadowd\_migrate test should be reported as unsupported instead of failed when the cluster set-up does not support running it

[CS-989](https://hpc-gridware.atlassian.net/browse/CS-989) add testsuite option to stop testing if a valgrind error is found

[CS-996](https://hpc-gridware.atlassian.net/browse/CS-996) sge\_qmaster internal gdi requests do not need to initialize and later parse authentication information

[CS-1009](https://hpc-gridware.atlassian.net/browse/CS-1009) create templete for gperf test an add a test that measures reservation performance

[CS-1010](https://hpc-gridware.atlassian.net/browse/CS-1010) qmaster complains about event client not properly initialized

[CS-1011](https://hpc-gridware.atlassian.net/browse/CS-1011) clean up formatting / output of unsigned long data

[CS-1084](https://hpc-gridware.atlassian.net/browse/CS-1084) with Munge authentication need to re-resolve user and group names

[CS-1090](https://hpc-gridware.atlassian.net/browse/CS-1090) qstat -r shall report resource requests by scope

[CS-1126](https://hpc-gridware.atlassian.net/browse/CS-1126) in the environment of tasks of tightly integrated parallel jobs set the pe\_task\_id

[CS-1143](https://hpc-gridware.atlassian.net/browse/CS-1143) provide a MPICH integration

[CS-1152](https://hpc-gridware.atlassian.net/browse/CS-1152) add a checktree\_mpi to testsuite with configuration and tests making use of the various MPI integrations

[CS-1154](https://hpc-gridware.atlassian.net/browse/CS-1154) category string shown in accounting and reporting might be truncated if it exceeds a certain fixed length

[CS-1159](https://hpc-gridware.atlassian.net/browse/CS-1159) Ensure that categories get still unused ID as primary key

[CS-1165](https://hpc-gridware.atlassian.net/browse/CS-1165) add a testsuite test for the ssh wrapper \(MPI\) template

[CS-1175](https://hpc-gridware.atlassian.net/browse/CS-1175) allow backslash in user names

[CS-1185](https://hpc-gridware.atlassian.net/browse/CS-1185) give more details in error message "<user> must be manager for this operation"

[CS-1186](https://hpc-gridware.atlassian.net/browse/CS-1186) default PE settings trigger unnecessary calls to /bin/true

[CS-1188](https://hpc-gridware.atlassian.net/browse/CS-1188) control daemons with systemd

[CS-1189](https://hpc-gridware.atlassian.net/browse/CS-1189) make testsuite optionally use systemd to startup/shutdown daemons

[CS-1191](https://hpc-gridware.atlassian.net/browse/CS-1191) build systems on macOS broken after application \(not system\) upgrade

[CS-1192](https://hpc-gridware.atlassian.net/browse/CS-1192) at startup of daemons output the cgroups slice the service is running in

[CS-1195](https://hpc-gridware.atlassian.net/browse/CS-1195) make classic spooling the default

[CS-1196](https://hpc-gridware.atlassian.net/browse/CS-1196) the "starting up" info message should be the first message to print to the messages file at daemon startup

[CS-1202](https://hpc-gridware.atlassian.net/browse/CS-1202) Installer specific cleanup and fixes

[CS-1205](https://hpc-gridware.atlassian.net/browse/CS-1205) When no PE is defined, the UI layout should be consistent to when no queue is defined or no calendar is defined

[CS-1212](https://hpc-gridware.atlassian.net/browse/CS-1212) Eliminate before/after event handler within the scheduler thread

[CS-1223](https://hpc-gridware.atlassian.net/browse/CS-1223) with systemd integration, move sge\_shepherd processes out of the sge\_execd service cgroup

[CS-1225](https://hpc-gridware.atlassian.net/browse/CS-1225) Client requests are rejected by qmaster because listener accept them before secondary DS are initialized

[CS-1226](https://hpc-gridware.atlassian.net/browse/CS-1226) user and project spooling triggered by scheduler might not be done consistently.

[CS-1234](https://hpc-gridware.atlassian.net/browse/CS-1234) remove deprecated reprioritize configuration parameters

[CS-1241](https://hpc-gridware.atlassian.net/browse/CS-1241) add profiling information for systemd operations

[CS-1256](https://hpc-gridware.atlassian.net/browse/CS-1256) allow one slave task to be started on the master host with the pe templates for mpich and mvapich

[CS-1257](https://hpc-gridware.atlassian.net/browse/CS-1257) in the MPI build scripts save the configure/make/make install log files into the installation directory

[CS-1258](https://hpc-gridware.atlassian.net/browse/CS-1258) log to execd messages file when and why sge\_execd rejects pe task requests \(qrsh -inherit\)

[CS-1260](https://hpc-gridware.atlassian.net/browse/CS-1260) In munge enabled environment, clients dump core if the munged is not running

[CS-1261](https://hpc-gridware.atlassian.net/browse/CS-1261) qrsh -verbose shall log which transport client it uses and where the information comes from

[CS-1280](https://hpc-gridware.atlassian.net/browse/CS-1280) testsuits should show runtime estimates for tests/test-branches and category/runlevel selection

[CS-1291](https://hpc-gridware.atlassian.net/browse/CS-1291) move shepherd child to its own scope

[CS-1292](https://hpc-gridware.atlassian.net/browse/CS-1292) get job online usage information via systemd

[CS-1294](https://hpc-gridware.atlassian.net/browse/CS-1294) set job limits via systemd

[CS-1295](https://hpc-gridware.atlassian.net/browse/CS-1295) set device isolation via systemd

[CS-1296](https://hpc-gridware.atlassian.net/browse/CS-1296) kill jobs via systemd

[CS-1315](https://hpc-gridware.atlassian.net/browse/CS-1315) set binding via systemd

[CS-1318](https://hpc-gridware.atlassian.net/browse/CS-1318) allow to run jobs under systemd control even if sge\_execd itself is not started as systemd service

[CS-1319](https://hpc-gridware.atlassian.net/browse/CS-1319) make running jobs under systemd control configurable

[CS-1321](https://hpc-gridware.atlassian.net/browse/CS-1321) allow to configure a hybrid usage data collection \(both via systemd and the pdc\)

[CS-1322](https://hpc-gridware.atlassian.net/browse/CS-1322) the job specific scopes need to contain the toplevel slice name to be unique

[CS-1331](https://hpc-gridware.atlassian.net/browse/CS-1331) add ioops to online usage and accounting

[CS-1335](https://hpc-gridware.atlassian.net/browse/CS-1335) need special handling for interrupted system call

[CS-1342](https://hpc-gridware.atlassian.net/browse/CS-1342) add systemd specific settings \(toplevel slice name\) to the installation guide

[CS-1343](https://hpc-gridware.atlassian.net/browse/CS-1343) add section\(s\) about systemd to the admin guide

[CS-1398](https://hpc-gridware.atlassian.net/browse/CS-1398) re-evaluate using the MemoryPeak property for maxrss

[CS-1405](https://hpc-gridware.atlassian.net/browse/CS-1405) add testsuite test for the ENABLE\_SYSTEMD execd\_param

[CS-1406](https://hpc-gridware.atlassian.net/browse/CS-1406) add test for the USAGE\_COLLECTION execd\_param

[CS-1408](https://hpc-gridware.atlassian.net/browse/CS-1408) USAGE\_COLLECTION mode must be kept consistent for running jobs

[CS-1419](https://hpc-gridware.atlassian.net/browse/CS-1419) disable systemd integration if sge\_execd is started as non privileged user

### New Feature

[CS-527](https://hpc-gridware.atlassian.net/browse/CS-527) Adapt qhost output to be able to deal with nodes with more than 1000 cores

[CS-748](https://hpc-gridware.atlassian.net/browse/CS-748) Configure qstat job visibility based on groups (-sdv) to ensure privacy

[CS-777](https://hpc-gridware.atlassian.net/browse/CS-777) Improve qsub -sync so that is support multiple state \(like r\)

[CS-962](https://hpc-gridware.atlassian.net/browse/CS-962) Show stacktrace if an uncaught exeception is thrown or signal is received that will trigger an abort\(\)

[CS-1033](https://hpc-gridware.atlassian.net/browse/CS-1033) Transfer information about -sync to qmaster and show the information within qstat -j

[CS-1047](https://hpc-gridware.atlassian.net/browse/CS-1047) JSV might have access to job attributes of a previous job verification

[CS-1048](https://hpc-gridware.atlassian.net/browse/CS-1048) Format in qsub man page is partially broken. 

[CS-1091](https://hpc-gridware.atlassian.net/browse/CS-1091) Clearly document the slots syntax in man5 sge\_queue\_conf.md

[CS-1104](https://hpc-gridware.atlassian.net/browse/CS-1104) Allow to add a configurable mail tag to all admin mails.

[CS-1147](https://hpc-gridware.atlassian.net/browse/CS-1147) Add local host overrides in qcontrol

[CS-1208](https://hpc-gridware.atlassian.net/browse/CS-1208) Implement remote execution of plain qconf command with arbitrary commands for UI

[CS-1219](https://hpc-gridware.atlassian.net/browse/CS-1219) As a user I want to select Parallel Environment configuration templates in qontrol to simplify installation

### Epic

[CS-173](https://hpc-gridware.atlassian.net/browse/CS-173) Job counter should be greater than 10 mio.

[CS-202](https://hpc-gridware.atlassian.net/browse/CS-202) Prevent denial of service attacks

[CS-1187](https://hpc-gridware.atlassian.net/browse/CS-1187) add systemd and cgroups integration

### Task

[CS-662](https://hpc-gridware.atlassian.net/browse/CS-662) verify delayed job reporting of sge\_execd after reconnecting to sge\_qmaster

[CS-945](https://hpc-gridware.atlassian.net/browse/CS-945) Adapt cmake to change version strings on various places when sgeobj/ocs\_Version.cc is changed

[CS-957](https://hpc-gridware.atlassian.net/browse/CS-957) scheduler subscribes SGE\_TYPE\_GLOBAL\_CONFIG event instead of SGE\_TYPE\_CONFIG events

[CS-958](https://hpc-gridware.atlassian.net/browse/CS-958) SGE\_TYPE\_GLOBAL\_CONFIG events does not contain any data

[CS-970](https://hpc-gridware.atlassian.net/browse/CS-970) Log\_State.gui\_log is unused

[CS-1005](https://hpc-gridware.atlassian.net/browse/CS-1005) gperf integration causes qmaster two create multiple profiles instead of just one for a dispatch cycle

[CS-1021](https://hpc-gridware.atlassian.net/browse/CS-1021) change template configuration and testsuite that use /bin/true to /usr/bin/true

[CS-1095](https://hpc-gridware.atlassian.net/browse/CS-1095) Add request limit stub to OCS

[CS-1133](https://hpc-gridware.atlassian.net/browse/CS-1133) qstat/option\_r-test broken with FQDN-hostnames

[CS-1134](https://hpc-gridware.atlassian.net/browse/CS-1134) message cutoff after 8 characters

[CS-1236](https://hpc-gridware.atlassian.net/browse/CS-1236) Message file contains empty lines

[CS-1237](https://hpc-gridware.atlassian.net/browse/CS-1237) Ensure that reprioritization test also runs on newer Linuxes with kernel auto grouping enabled

[CS-1299](https://hpc-gridware.atlassian.net/browse/CS-1299) Evaluate lcov and do an integration with our test environment

[CS-1304](https://hpc-gridware.atlassian.net/browse/CS-1304) Enable OCS/GCS build with gcov

[CS-1310](https://hpc-gridware.atlassian.net/browse/CS-1310) Add bind tests to check manual start position for binding startegies

[CS-1407](https://hpc-gridware.atlassian.net/browse/CS-1407) Add SUSE SLES 15 support in support matrix of release notes

[CS-1415](https://hpc-gridware.atlassian.net/browse/CS-1415) do memory testing on master branch for 9.1.0

[CS-1440](https://hpc-gridware.atlassian.net/browse/CS-1440) Add qtelemetry licenses to GCS 3rdparty licenses directory

[CS-1466](https://hpc-gridware.atlassian.net/browse/CS-1466) Create initial draft for a specification for further discussion with customer

### Sub-task

[CS-599](https://hpc-gridware.atlassian.net/browse/CS-599) use known global host object in scheduling instead of searching it in the host list

[CS-696](https://hpc-gridware.atlassian.net/browse/CS-696) Jenkins: enable sge\_strdup\_test

[CS-697](https://hpc-gridware.atlassian.net/browse/CS-697) Jenkins: enable issue\_3013

[CS-702](https://hpc-gridware.atlassian.net/browse/CS-702) Jenkins: enable basic\_test

[CS-728](https://hpc-gridware.atlassian.net/browse/CS-728) Implement striding-oversubscription test scenarios

[CS-729](https://hpc-gridware.atlassian.net/browse/CS-729) Implement explicit-oversubscription scenarios

[CS-730](https://hpc-gridware.atlassian.net/browse/CS-730) Enable existing binding test scenarios

[CS-817](https://hpc-gridware.atlassian.net/browse/CS-817) qselect should show only those queues where the user has access

[CS-841](https://hpc-gridware.atlassian.net/browse/CS-841) qstat -j should hide jobs similar to qstat -f if -sdv is set in qstat-file

[CS-842](https://hpc-gridware.atlassian.net/browse/CS-842) qstat -sdv  considers supplementary groups even if this is not enabled in the qmaster\_params

[CS-843](https://hpc-gridware.atlassian.net/browse/CS-843) add test scenarios for CS-748 \(department view enabled with -sdv\)

[CS-907](https://hpc-gridware.atlassian.net/browse/CS-907) Refactor/rewrite GDI to make it accessible from Python

[CS-933](https://hpc-gridware.atlassian.net/browse/CS-933) lwdb: add support for object, list and reference type as well as hostnames

[CS-941](https://hpc-gridware.atlassian.net/browse/CS-941) lwdb: Create module tests for all element methods

[CS-961](https://hpc-gridware.atlassian.net/browse/CS-961) add a service function to print a stacktrace 

[CS-963](https://hpc-gridware.atlassian.net/browse/CS-963) overwrite default action for uncaught exception and write message to cerr and message file

[CS-965](https://hpc-gridware.atlassian.net/browse/CS-965) create service functions to simulate various issues like segfault, stack-overflow, floating point errors, ...

[CS-967](https://hpc-gridware.atlassian.net/browse/CS-967) add support for BSD for get\_backtrace\(\)

[CS-969](https://hpc-gridware.atlassian.net/browse/CS-969) Generate stacktrace also in execd and all clients

[CS-972](https://hpc-gridware.atlassian.net/browse/CS-972) Jenkins: enable qhost test

[CS-995](https://hpc-gridware.atlassian.net/browse/CS-995) Add munge authentication

[CS-1012](https://hpc-gridware.atlassian.net/browse/CS-1012) replace sge\_u32 by sge\_uu32 in format strings

[CS-1013](https://hpc-gridware.atlassian.net/browse/CS-1013) replace sge\_U32CFormat by sge\_uu32 and remove typecasts with sge\_u32c

[CS-1014](https://hpc-gridware.atlassian.net/browse/CS-1014) verify all calls to xml\_Append\_Attr\_I

[CS-1015](https://hpc-gridware.atlassian.net/browse/CS-1015) increase max job id to U\_LONG32\_MAX

[CS-1025](https://hpc-gridware.atlassian.net/browse/CS-1025) remove static\_cast<u\_long32> for pid\_t/uid\_t and gid\_t and correct format strings

[CS-1029](https://hpc-gridware.atlassian.net/browse/CS-1029) Add test that checks the output of qsub -sync y

[CS-1030](https://hpc-gridware.atlassian.net/browse/CS-1030) Add test that checks the reported exit status for individual tasks of commands like qsub -t 1-2 -sync y

[CS-1035](https://hpc-gridware.atlassian.net/browse/CS-1035) Fix compile issues for macOS

[CS-1036](https://hpc-gridware.atlassian.net/browse/CS-1036) Fix issues with test users for macOS

[CS-1037](https://hpc-gridware.atlassian.net/browse/CS-1037) Fix build environment for macOS and build for OCS and GCS

[CS-1038](https://hpc-gridware.atlassian.net/browse/CS-1038) Fix remote login problem for macOS regular users

[CS-1042](https://hpc-gridware.atlassian.net/browse/CS-1042) Enable munge for macOS

[CS-1044](https://hpc-gridware.atlassian.net/browse/CS-1044) Testsuite does not find JAVA\_HOME on macOS

[CS-1045](https://hpc-gridware.atlassian.net/browse/CS-1045) libmunge cannot be loaded

[CS-1052](https://hpc-gridware.atlassian.net/browse/CS-1052) -sync options: adapt qstat schema file

[CS-1063](https://hpc-gridware.atlassian.net/browse/CS-1063) Install OmniOs in  our build lab

[CS-1064](https://hpc-gridware.atlassian.net/browse/CS-1064) Install OpenIndiana in our lab

[CS-1065](https://hpc-gridware.atlassian.net/browse/CS-1065) 3rd\_party build fails on illumos

[CS-1066](https://hpc-gridware.atlassian.net/browse/CS-1066) qmake build fails on illumos

[CS-1067](https://hpc-gridware.atlassian.net/browse/CS-1067) Add illumos setup to developers guide

[CS-1068](https://hpc-gridware.atlassian.net/browse/CS-1068) Add the osol-amd64 platform to testsuite.

[CS-1077](https://hpc-gridware.atlassian.net/browse/CS-1077) add advance reservation requested by job to the assignment structure

[CS-1100](https://hpc-gridware.atlassian.net/browse/CS-1100) Enhance throughput test to check performance with enabled gdi\_request\_limits

[CS-1107](https://hpc-gridware.atlassian.net/browse/CS-1107) unnecessary actions are done for jobs which are skipped as the category is rejected

[CS-1108](https://hpc-gridware.atlassian.net/browse/CS-1108) during scheduling we evaluate for every job if it could get a resource reservation

[CS-1121](https://hpc-gridware.atlassian.net/browse/CS-1121) add test scenarios for the submit clients -dept switch

[CS-1122](https://hpc-gridware.atlassian.net/browse/CS-1122) test scenario for multi department view

[CS-1127](https://hpc-gridware.atlassian.net/browse/CS-1127) -dept switch not accepted by qsh

[CS-1199](https://hpc-gridware.atlassian.net/browse/CS-1199) Installer does not distinguish between client, execution and master specific binaries

[CS-1200](https://hpc-gridware.atlassian.net/browse/CS-1200) Installer provides darwin\_template for services which is incomplete and not needed anymore

[CS-1201](https://hpc-gridware.atlassian.net/browse/CS-1201) Service installation on non-supported architectures does not provide a meaningful message

[CS-1213](https://hpc-gridware.atlassian.net/browse/CS-1213) Performance: Normalization of job priority can be done during job add/mod

[CS-1214](https://hpc-gridware.atlassian.net/browse/CS-1214) Load formula should be evaluated in worker when changed

[CS-1215](https://hpc-gridware.atlassian.net/browse/CS-1215) Remove windows specific code from installer

[CS-1216](https://hpc-gridware.atlassian.net/browse/CS-1216) Remove TS windows hooks

[CS-1222](https://hpc-gridware.atlassian.net/browse/CS-1222) rename sge\_uu32 to sge\_u32

[CS-1224](https://hpc-gridware.atlassian.net/browse/CS-1224) Evaluate effort to parallelize report handling

[CS-1229](https://hpc-gridware.atlassian.net/browse/CS-1229) make rocky 9 available in test environment

[CS-1238](https://hpc-gridware.atlassian.net/browse/CS-1238) Split schedulers policy module into two separate modules \(usage\+decay and ticket calculation\)

[CS-1243](https://hpc-gridware.atlassian.net/browse/CS-1243) Setup new server that will be used for LDAP service 

[CS-1244](https://hpc-gridware.atlassian.net/browse/CS-1244) Update GPU Server and driver for the installed GPUs

[CS-1245](https://hpc-gridware.atlassian.net/browse/CS-1245) Make the latest CUDA toolkit available on all GPU machines

[CS-1246](https://hpc-gridware.atlassian.net/browse/CS-1246) Install or update DCGM on all GPU hosts

[CS-1247](https://hpc-gridware.atlassian.net/browse/CS-1247) Compile a test application using CUDA

[CS-1248](https://hpc-gridware.atlassian.net/browse/CS-1248) Create test that checks munge setup of configured TS hosts

[CS-1250](https://hpc-gridware.atlassian.net/browse/CS-1250) Setup fakturama on the fakturama host for the fakturama user

[CS-1264](https://hpc-gridware.atlassian.net/browse/CS-1264) Clone and update Solaris 11 VM

[CS-1285](https://hpc-gridware.atlassian.net/browse/CS-1285) Split binding specific code in test suite and add modules that allow to implement new tests for scheduler binding

[CS-1287](https://hpc-gridware.atlassian.net/browse/CS-1287) Binding tests do not check the assigned socket/core IDs of binding jobs

[CS-1374](https://hpc-gridware.atlassian.net/browse/CS-1374) finalize specification \(user perspective\)

[CS-1394](https://hpc-gridware.atlassian.net/browse/CS-1394) Add start\_time of array jobs tasks to qstat -j <jid>

[CS-1395](https://hpc-gridware.atlassian.net/browse/CS-1395) Cleanup of job states and show states also in qstat -j <jid> output

[CS-1396](https://hpc-gridware.atlassian.net/browse/CS-1396) Show granted host information in qstat -j <jid> output

[CS-1404](https://hpc-gridware.atlassian.net/browse/CS-1404) Show granted queues in qstat -j <jid> output

[CS-1410](https://hpc-gridware.atlassian.net/browse/CS-1410) Show priority in qstat -j <jid> output even if it is the base priority

[CS-1416](https://hpc-gridware.atlassian.net/browse/CS-1416) make sure that valgrind.supp for false positives is adapted to the current code

[CS-1417](https://hpc-gridware.atlassian.net/browse/CS-1417) find and fix memory leaks which lead to rapid growth of sge\_qmaster \(as of July 2025\)

[CS-1418](https://hpc-gridware.atlassian.net/browse/CS-1418) do memory testing with full testsuite runs on master branch \(August 2025\)

### Bug

[CS-430](https://hpc-gridware.atlassian.net/browse/CS-430) booking of resources into advance reservations needs to distinguish between host and queue resources

[CS-634](https://hpc-gridware.atlassian.net/browse/CS-634) a job requesting a pty fails on freebsd

[CS-671](https://hpc-gridware.atlassian.net/browse/CS-671) qrsh truncates the command line and/or output at 927 characters

[CS-722](https://hpc-gridware.atlassian.net/browse/CS-722) env\_list in qstat should show NONE if not set

[CS-911](https://hpc-gridware.atlassian.net/browse/CS-911) Sharetree changes are not sent to all data stores

[CS-912](https://hpc-gridware.atlassian.net/browse/CS-912) Changed scheduler configuration is not send to all data stores

[CS-921](https://hpc-gridware.atlassian.net/browse/CS-921) behavior of qalter is unclear regarding the modification of command arguments

[CS-924](https://hpc-gridware.atlassian.net/browse/CS-924) remove GDI requests in scheduler that gets its data via events.

[CS-942](https://hpc-gridware.atlassian.net/browse/CS-942) root filesystem on GCP master host is too small for running 50.000 jobs

[CS-948](https://hpc-gridware.atlassian.net/browse/CS-948) As an admin I want to configure exclusive host consumable in complex configuration for reserving complete nodes

[CS-959](https://hpc-gridware.atlassian.net/browse/CS-959) sge\_qmaster shutdown takes too long

[CS-981](https://hpc-gridware.atlassian.net/browse/CS-981) start of advance reservation can be slightly delayed

[CS-990](https://hpc-gridware.atlassian.net/browse/CS-990) Active reservations for parallel jobs will cause core of master during restart

[CS-992](https://hpc-gridware.atlassian.net/browse/CS-992) Scheduler makes no reservation for jobs after execd or sometimes qmaster restart

[CS-994](https://hpc-gridware.atlassian.net/browse/CS-994) Instead of reserving slots for a PE job it is started immediately

[CS-997](https://hpc-gridware.atlassian.net/browse/CS-997) Output shows negative timestamps for resource bookings

[CS-998](https://hpc-gridware.atlassian.net/browse/CS-998) Scheduler creates unnecessary resource schedules that cost performance

[CS-1007](https://hpc-gridware.atlassian.net/browse/CS-1007) Repeated operation during reservation assignment costs >10% performance in scheduler.

[CS-1008](https://hpc-gridware.atlassian.net/browse/CS-1008) Unnecessary pattern matching operations in scheduler can cost up to 5% dispatch performance

[CS-1019](https://hpc-gridware.atlassian.net/browse/CS-1019) sge\_execd logs errors when running tightly integrated parallel jobs

[CS-1024](https://hpc-gridware.atlassian.net/browse/CS-1024) qdel reports jobs as "is already in deletion" although qstat shows that job in 'qw' state

[CS-1046](https://hpc-gridware.atlassian.net/browse/CS-1046) update docker image for github build-on-push action

[CS-1049](https://hpc-gridware.atlassian.net/browse/CS-1049) Fix MPI templates to contain ignore slave task requests on master host

[CS-1053](https://hpc-gridware.atlassian.net/browse/CS-1053) with pe setting ign\_sreq\_on\_mhost=true job having a slave request on a globally defined resource are not scheduled

[CS-1054](https://hpc-gridware.atlassian.net/browse/CS-1054) with pe setting ign\_sreq\_on\_mhost=true a globally defined resource which is requested for slave scope is not booked into the resource diagram

[CS-1055](https://hpc-gridware.atlassian.net/browse/CS-1055) sge prefix in man pages is not correctly replaced in certain man page files

[CS-1085](https://hpc-gridware.atlassian.net/browse/CS-1085) BDB build error on lx-riscv64 after OS update.

[CS-1089](https://hpc-gridware.atlassian.net/browse/CS-1089) testsuite reports "UNEXPECTED-FORMAT-SPECIFIER" at startup

[CS-1096](https://hpc-gridware.atlassian.net/browse/CS-1096) USE\_QSUB\_GID functionality fails on FreeBSD 14

[CS-1097](https://hpc-gridware.atlassian.net/browse/CS-1097) Core of qmaster

[CS-1098](https://hpc-gridware.atlassian.net/browse/CS-1098) Tests that use simulated hosts fail if they are executed on FreeBSD

[CS-1099](https://hpc-gridware.atlassian.net/browse/CS-1099) process monitoring of TS fails if executed on FreeBSD

[CS-1131](https://hpc-gridware.atlassian.net/browse/CS-1131) wallclock time reported for tasks of a tightly integrated parallel job is incorrect

[CS-1132](https://hpc-gridware.atlassian.net/browse/CS-1132) reserved usage is not calculated for rss

[CS-1139](https://hpc-gridware.atlassian.net/browse/CS-1139) job deletion via JAPI/DRMAA fails if job ID exceeds INT\_MAX

[CS-1140](https://hpc-gridware.atlassian.net/browse/CS-1140) termination of event client via JAPI fails if event client ID exceeds INT\_MAX

[CS-1141](https://hpc-gridware.atlassian.net/browse/CS-1141) MacOS build broken due to unavailability of getgrouplist\(\)

[CS-1142](https://hpc-gridware.atlassian.net/browse/CS-1142) when starting up a 9.1.0 sge\_qmaster it logs "received invalid event group 4"

[CS-1162](https://hpc-gridware.atlassian.net/browse/CS-1162) jobs with high job ids \(> 2^31\) have an incorrect key in berkeleydb spooling

[CS-1163](https://hpc-gridware.atlassian.net/browse/CS-1163) when a queue is signalled then additional invalid entries are created in the berkeleydb spooling database

[CS-1190](https://hpc-gridware.atlassian.net/browse/CS-1190) Mirror does not process MOD and DEL events for categories correctly

[CS-1193](https://hpc-gridware.atlassian.net/browse/CS-1193) uninstalling an sge\_execd on the master host fails with "denied: can't delete master host "ubuntu-24-amd64-1" from admin host list"

[CS-1194](https://hpc-gridware.atlassian.net/browse/CS-1194) man pages contain incorrect paths in the FILES section

[CS-1204](https://hpc-gridware.atlassian.net/browse/CS-1204) In the "Cluster Queue Configuration" the \+Add PE button does not work correctly

[CS-1227](https://hpc-gridware.atlassian.net/browse/CS-1227) \(Auto\)installer fails to install OCS 9.0.5 packages on Rocky Linux 9

[CS-1228](https://hpc-gridware.atlassian.net/browse/CS-1228) unsuspend on threshold fails to set QU\_last\_suspend\_threshold\_ckeck

[CS-1267](https://hpc-gridware.atlassian.net/browse/CS-1267) broken build on Solaris 11 after update

[CS-1269](https://hpc-gridware.atlassian.net/browse/CS-1269) error reason reported in error file and shown by  qstat -j jobid is truncated

[CS-1281](https://hpc-gridware.atlassian.net/browse/CS-1281) testsuite fails in post-check tests when option "interactive ssh" is used

[CS-1283](https://hpc-gridware.atlassian.net/browse/CS-1283) Build description with munge in developers guide is not complete

[CS-1286](https://hpc-gridware.atlassian.net/browse/CS-1286) Core binding \(striding strategy\) not applied although cores might be available.

[CS-1288](https://hpc-gridware.atlassian.net/browse/CS-1288) Core binding \(explicit strategy\) does not reserve cores correctly

[CS-1311](https://hpc-gridware.atlassian.net/browse/CS-1311) Error message in autoinstaller: MakeUserKs command not found

[CS-1323](https://hpc-gridware.atlassian.net/browse/CS-1323) code writing add\_grp\_id to shepherd config file is duplicated and looks incorrect

[CS-1325](https://hpc-gridware.atlassian.net/browse/CS-1325) possible race condition between calling StartTransientUnit and waiting for the corresponding job to finish

[CS-1341](https://hpc-gridware.atlassian.net/browse/CS-1341) installation guide mentions scheduler profiles which do not exist

[CS-1387](https://hpc-gridware.atlassian.net/browse/CS-1387) qrsh takes up to 1s to terminate after job end

[CS-1422](https://hpc-gridware.atlassian.net/browse/CS-1422) endless loop in protocol between sge\_qmaster and sge\_execd in certain job failure situations

[CS-1424](https://hpc-gridware.atlassian.net/browse/CS-1424) qmod -sj on own job fails on submit only host

[CS-1425](https://hpc-gridware.atlassian.net/browse/CS-1425) backup/restore does not handle $SGE\_ROOT/$SGE\_CELL/slice\_name

[CS-1429](https://hpc-gridware.atlassian.net/browse/CS-1429) sge\_qmaster can segfault on qdel -f

[CS-1430](https://hpc-gridware.atlassian.net/browse/CS-1430) running tightly integrated parallel jobs leaves systemd slices

[CS-1432](https://hpc-gridware.atlassian.net/browse/CS-1432) auto install does not create slice\_name file

[CS-1433](https://hpc-gridware.atlassian.net/browse/CS-1433) systemd support is initialized despite slice\_name file missing

[CS-1435](https://hpc-gridware.atlassian.net/browse/CS-1435) rescheduling of jobs requires manager rights, documented is "manager or operator rights"

[CS-1436](https://hpc-gridware.atlassian.net/browse/CS-1436) qmod man pages says it requires manager or operator privileges to rerun a job, but a job owner may rerun his own jobs as well

[CS-1441](https://hpc-gridware.atlassian.net/browse/CS-1441) on CentOS 10 sge\_shepherd seems to crash when a qrsh job finishes

[CS-1443](https://hpc-gridware.atlassian.net/browse/CS-1443) prolog/epilog and pe\_start/pe\_stop procedures are started within the job systemd scope

[CS-1450](https://hpc-gridware.atlassian.net/browse/CS-1450) usage attribute iow is not shown by qstat -j job\_id and always 0 in accounting

[CS-1451](https://hpc-gridware.atlassian.net/browse/CS-1451) option -out of examples/jobsbin/<arch>/work is broken

[CS-1457](https://hpc-gridware.atlassian.net/browse/CS-1457) usage collection in sge\_execd \(PDC / Systemd\) is called too often

[//]: # (Each file has to end with two empty lines)

