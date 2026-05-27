# Full List of Fixes

## v9.1.2

### [CS-1129](https://hpc-gridware.atlassian.net/browse/CS-1129) Provide a UI for a modern qmon replacement for cluster configuration

- [CS-1969](https://hpc-gridware.atlassian.net/browse/CS-1969) Add copy / move operations in Qontrol Share Tree editor
- [CS-2069](https://hpc-gridware.atlassian.net/browse/CS-2069) qontrol: add ETag/If-Match optimistic concurrency to queue configurations
- [CS-2070](https://hpc-gridware.atlassian.net/browse/CS-2070) qontrol: add ETag/If-Match optimistic concurrency to global configuration (version-aware)
- [CS-2071](https://hpc-gridware.atlassian.net/browse/CS-2071) qontrol: add ETag/If-Match optimistic concurrency to scheduler configuration
- [CS-2072](https://hpc-gridware.atlassian.net/browse/CS-2072) qontrol: add ETag/If-Match optimistic concurrency to resource quota sets

### [CS-1575](https://hpc-gridware.atlassian.net/browse/CS-1575) improvements and fixes to encryption of messages between commands and daemons

- [CS-1858](https://hpc-gridware.atlassian.net/browse/CS-1858) with TLS encryption enabled profiling messages sometimes start with some binary data

### [CS-174](https://hpc-gridware.atlassian.net/browse/CS-174) add wallclock, rss, maxrss to job usage

- [CS-184](https://hpc-gridware.atlassian.net/browse/CS-184) add wallclock time, rss and maxrss to dbwriter

### [CS-1986](https://hpc-gridware.atlassian.net/browse/CS-1986) Add additional missing information to the dbwriter database.

- [CS-2147](https://hpc-gridware.atlassian.net/browse/CS-2147) Add all usage values which are available in qacct to the dbwriter sge_job_usage table

### [CS-2159](https://hpc-gridware.atlassian.net/browse/CS-2159) IJS — documentation and test-coverage improvements

- [CS-2165](https://hpc-gridware.atlassian.net/browse/CS-2165) Re-enable qrsh -pty + stdin-redirect test scenarios (CS-1759 follow-up)

### [CS-335](https://hpc-gridware.atlassian.net/browse/CS-335) dbwriter: bring-up and stabilization

- [CS-1334](https://hpc-gridware.atlassian.net/browse/CS-1334) add online usage to the dbwriter database
- [CS-1948](https://hpc-gridware.atlassian.net/browse/CS-1948) dbwriter creates daily derived values of the current day at startup
- [CS-1983](https://hpc-gridware.atlassian.net/browse/CS-1983) Make dbwriter work with current Java versions
- [CS-1984](https://hpc-gridware.atlassian.net/browse/CS-1984) In dbwriter the fields for job_number, task_number and ar_number cannot hold ids > 2³¹
- [CS-2009](https://hpc-gridware.atlassian.net/browse/CS-2009) dbwriter duplicates records in the sge_job_requests table due to intermediate accounting records
- [CS-2015](https://hpc-gridware.atlassian.net/browse/CS-2015) replace deprecated boxed type instantiation in dbwriter by factory methods
- [CS-2055](https://hpc-gridware.atlassian.net/browse/CS-2055) remove code from dbwriter handling the (no longer existing) statistics file
- [CS-2065](https://hpc-gridware.atlassian.net/browse/CS-2065) With the mysql JDBC sgedbwriter logs an info message
- [CS-2067](https://hpc-gridware.atlassian.net/browse/CS-2067) Document how dbwriter is updated
- [CS-2215](https://hpc-gridware.atlassian.net/browse/CS-2215) dbwriter JobManager calls usageManager.setParentManager twice, never sets requestManager's parent
- [CS-2227](https://hpc-gridware.atlassian.net/browse/CS-2227) dbwriter: package as a single shaded uber-jar for safe patch delivery
- [CS-2244](https://hpc-gridware.atlassian.net/browse/CS-2244) dbwriter: online_usage records create orphaned sparse sge_job rows for PE tasks
- [CS-2245](https://hpc-gridware.atlassian.net/browse/CS-2245) JSONL ar_acct record omits ar_number, so dbwriter never fills sge_ar_usage
- [CS-2247](https://hpc-gridware.atlassian.net/browse/CS-2247) dbwriter: logs AdvanceReservationResource.splitError when running advance reservations
- [CS-2249](https://hpc-gridware.atlassian.net/browse/CS-2249) JSONL reporting omits pe_taskid sentinel "NONE" for non-PE jobs
- [CS-338](https://hpc-gridware.atlassian.net/browse/CS-338) make dbwriter process the json format

### [CS-822](https://hpc-gridware.atlassian.net/browse/CS-822) Provide Support for AI Workloads

- [CS-2209](https://hpc-gridware.atlassian.net/browse/CS-2209) Add host resource overview to cli qontrol mcp server

### Bug

- [CS-1385](https://hpc-gridware.atlassian.net/browse/CS-1385) sharetree usage booking is storing too much data in user/project objects
- [CS-1597](https://hpc-gridware.atlassian.net/browse/CS-1597) only static load values shall be written to spooling
- [CS-1759](https://hpc-gridware.atlassian.net/browse/CS-1759) qrsh -pty yes fails when characters are sent to stdin at startup time
- [CS-1963](https://hpc-gridware.atlassian.net/browse/CS-1963) testsuite tests calling qrsh -inherit fail in a setup with TLS encryption and long host names
- [CS-1964](https://hpc-gridware.atlassian.net/browse/CS-1964) commlib error messages in a TLS setup can contain "(null)"
- [CS-2068](https://hpc-gridware.atlassian.net/browse/CS-2068) testsuite dbwriter build as part of a full product build sometimes fails as it uses the wrong Java version
- [CS-2111](https://hpc-gridware.atlassian.net/browse/CS-2111) qlogin / qrsh without command: 100% CPU in idle builtin IJS session
- [CS-2157](https://hpc-gridware.atlassian.net/browse/CS-2157) Requests are rejected when hostname resolving is not consistent regarding short vs. long host names
- [CS-2228](https://hpc-gridware.atlassian.net/browse/CS-2228) jdrmaa build on CentOS 8 creates jdrmaa.jar instead of jdrmaa-1.0.jar
- [CS-2230](https://hpc-gridware.atlassian.net/browse/CS-2230) qrsh client tty_to_commlib spins at 100% CPU due to undrained wakeup pipe
- [CS-2231](https://hpc-gridware.atlassian.net/browse/CS-2231) sge_shepherd spins at 100% CPU after PTY POLLHUP in non-builtin IJS mode (GH #81)
- [CS-2243](https://hpc-gridware.atlassian.net/browse/CS-2243) testsuite framework function wait_for_end_of_all_jobs only waits for jobs of the testsuite user

### Improvement

- [CS-1333](https://hpc-gridware.atlassian.net/browse/CS-1333) add reporting of online usage to reporting file
- [CS-1955](https://hpc-gridware.atlassian.net/browse/CS-1955) commlib resolving error message(s) should contain all available information
- [CS-1968](https://hpc-gridware.atlassian.net/browse/CS-1968) remove logging "Got telnet client name from global/local config: builtin" from qlogin client output
- [CS-1970](https://hpc-gridware.atlassian.net/browse/CS-1970) in scheduler use pre-parsed allocation rules where possible
- [CS-1985](https://hpc-gridware.atlassian.net/browse/CS-1985) In accounting records add the information who deleted a job.
- [CS-2014](https://hpc-gridware.atlassian.net/browse/CS-2014) normalize requests in the job category
- [CS-2216](https://hpc-gridware.atlassian.net/browse/CS-2216) dbwriter: unify RecordManager construction for JobManager sub-managers
- [CS-2254](https://hpc-gridware.atlassian.net/browse/CS-2254) Harden MCP analysis tools: efficiency, pattern truncation, rolling windows, ctx cancellation

### New Feature

- [CS-2013](https://hpc-gridware.atlassian.net/browse/CS-2013) Add port_range to global configuration to restrict ephemeral ports for GCS/OCS commands
- [CS-2146](https://hpc-gridware.atlassian.net/browse/CS-2146) qacct should not print keys (like submit cmd) without values when old accounting file is used
- [CS-2251](https://hpc-gridware.atlassian.net/browse/CS-2251) Add pending_tasks to qstat -j output for better array job tasks insights

## v9.1.1

### [CS-1129](https://hpc-gridware.atlassian.net/browse/CS-1129) Provide a UI for a modern qmon replacement for cluster configuration

- [CS-1857](https://hpc-gridware.atlassian.net/browse/CS-1857) Add sharetree support to Qontrol
- [CS-1886](https://hpc-gridware.atlassian.net/browse/CS-1886) Add operations JSONL operations log history to Qontrol interface to track changes done with UI
- [CS-1933](https://hpc-gridware.atlassian.net/browse/CS-1933) Add MCP server to Qontrol to provide tools for read only cluster analysis

### [CS-217](https://hpc-gridware.atlassian.net/browse/CS-217) Provide an observability solution so that GCS can be integrated in any ops/management dashboards

- [CS-1864](https://hpc-gridware.atlassian.net/browse/CS-1864) Improve qtelemetry section in Admin Guide

### [CS-335](https://hpc-gridware.atlassian.net/browse/CS-335) dbwriter: bring-up and stabilization

- [CS-1489](https://hpc-gridware.atlassian.net/browse/CS-1489) test dbwriter with MySQL
- [CS-1924](https://hpc-gridware.atlassian.net/browse/CS-1924) run dbwriter as systemd services
- [CS-1940](https://hpc-gridware.atlassian.net/browse/CS-1940) dbwriter reports an error when parsing accounting records around midnight
- [CS-1943](https://hpc-gridware.atlassian.net/browse/CS-1943) dbwriter doesn't create the daily derived d_jobs_finished values for hosts
- [CS-1949](https://hpc-gridware.atlassian.net/browse/CS-1949) inst_dbwriter prints "Welcome to the @@GRID_ENGINE_NAME@@ ARCo dbwriter module installation"
- [CS-1950](https://hpc-gridware.atlassian.net/browse/CS-1950) inst_dbwriter is not executable
- [CS-336](https://hpc-gridware.atlassian.net/browse/CS-336) provide a Maven build for dbwriter

### [CS-767](https://hpc-gridware.atlassian.net/browse/CS-767) provide means to run performance tests on cloud resources

- [CS-1869](https://hpc-gridware.atlassian.net/browse/CS-1869) make sure that throughput test results can be re-read and reports can be re-generated
- [CS-1870](https://hpc-gridware.atlassian.net/browse/CS-1870) make testsuite create an overview page comparing and linking multiple performance test results
- [CS-1875](https://hpc-gridware.atlassian.net/browse/CS-1875) speed up testsuite cluster shutdown

### Bug

- [CS-1812](https://hpc-gridware.atlassian.net/browse/CS-1812) remove dbwriter logging "0 lines marked as erroneous, these will be skipped"
- [CS-1834](https://hpc-gridware.atlassian.net/browse/CS-1834) PE round robin allocation rule does not respect the masterq and scope switch
- [CS-1850](https://hpc-gridware.atlassian.net/browse/CS-1850) if master and slave queue requests are overlapping slave tasks can be scheduled to queue instances which do not match the slave queue request
- [CS-1859](https://hpc-gridware.atlassian.net/browse/CS-1859) with profiling enabled qmaster messages file contains empty lines
- [CS-1861](https://hpc-gridware.atlassian.net/browse/CS-1861) Munge installation in the GCP deployment seems to be incomplete
- [CS-1863](https://hpc-gridware.atlassian.net/browse/CS-1863) GDI multi call (as done by cetrain client apps) can cause core of client application
- [CS-1867](https://hpc-gridware.atlassian.net/browse/CS-1867) qtelemetry fails parsing qhost
- [CS-1871](https://hpc-gridware.atlassian.net/browse/CS-1871) testsuite framework function ge_has_feature sometimes returns an incorrect result
- [CS-1872](https://hpc-gridware.atlassian.net/browse/CS-1872) Malformed qstat -j output for job arrays
- [CS-1894](https://hpc-gridware.atlassian.net/browse/CS-1894) Runtime crash due to mixed compiler/runtime libraries when using non-system toolchain (xlx-amd64)
- [CS-1904](https://hpc-gridware.atlassian.net/browse/CS-1904) loadcheck shows product name as OGE
- [CS-1905](https://hpc-gridware.atlassian.net/browse/CS-1905) Zombie jobs are not displayed
- [CS-1922](https://hpc-gridware.atlassian.net/browse/CS-1922) shepherd_cmd does not work with qrsh jobs

### Improvement

- [CS-1176](https://hpc-gridware.atlassian.net/browse/CS-1176) Installation on xlx-amd64 fails due to core of qping
- [CS-1860](https://hpc-gridware.atlassian.net/browse/CS-1860) remove extra string copy operation in logging to messages files
- [CS-1887](https://hpc-gridware.atlassian.net/browse/CS-1887) Add support for linux kernel release 7.*
- [CS-1893](https://hpc-gridware.atlassian.net/browse/CS-1893) testsuite test tight_integration shall ignore non test related warning messages
- [CS-1918](https://hpc-gridware.atlassian.net/browse/CS-1918) Improve logging of lmstat requests in license-manager for better traceability
- [CS-1919](https://hpc-gridware.atlassian.net/browse/CS-1919) Make license-manager lmstat data caching timeout configurable
- [CS-1923](https://hpc-gridware.atlassian.net/browse/CS-1923) qgpu requires a flag which influences CUDA_VISIBLE_DEVICES numbering scheme for device isolation
- [CS-1928](https://hpc-gridware.atlassian.net/browse/CS-1928) Provide an example of enforcing GPU device isolation using a shepherd wrapper script
- [CS-1960](https://hpc-gridware.atlassian.net/browse/CS-1960) Add license aliasing / pooling feature description to admin guide
- [CS-1961](https://hpc-gridware.atlassian.net/browse/CS-1961) Add license aliasing / pooling feature description to man page
- [CS-612](https://hpc-gridware.atlassian.net/browse/CS-612) allow to overwrite the PE allocation rule on the submission command line

### New Feature

- [CS-1900](https://hpc-gridware.atlassian.net/browse/CS-1900) Add license feature aggregation supporting multiple license servers to license-manager
- [CS-1917](https://hpc-gridware.atlassian.net/browse/CS-1917) license-manager should have a configurable timeout for individual license server requests

### Task

- [CS-1878](https://hpc-gridware.atlassian.net/browse/CS-1878) in testsuite reports containing tables allow to set the background color per individual cell
- [CS-1954](https://hpc-gridware.atlassian.net/browse/CS-1954) Add management of single users to User Management in Qontrol

## v9.1.0

### [CS-1105](https://hpc-gridware.atlassian.net/browse/CS-1105) Go API Interface Updates

- [CS-1106](https://hpc-gridware.atlassian.net/browse/CS-1106) Go Interface: Add gdi_request_limits from global config in qconf v91 types
- [CS-1109](https://hpc-gridware.atlassian.net/browse/CS-1109) Go Interface: Add mail_tag from global conf
- [CS-1113](https://hpc-gridware.atlassian.net/browse/CS-1113) Go Interface: Consider fix CS-761 for v9.1

### [CS-1129](https://hpc-gridware.atlassian.net/browse/CS-1129) Provide a UI for a modern qmon replacement for cluster configuration

- [CS-1149](https://hpc-gridware.atlassian.net/browse/CS-1149) Add basic support for certificates in the UI backend to encrypt traffic
- [CS-1203](https://hpc-gridware.atlassian.net/browse/CS-1203) Fully implement "Add Host Group" in Host Management in qcontrol
- [CS-1206](https://hpc-gridware.atlassian.net/browse/CS-1206) Delete User Set configuration confirmation modal displays text twice
- [CS-1829](https://hpc-gridware.atlassian.net/browse/CS-1829) Add "Clone" functionality for queues, PEs, user sets, projects, quotas and calendars in Qontrol
- [CS-1833](https://hpc-gridware.atlassian.net/browse/CS-1833) In complex configuration in qontrol an exclusive consumable for a bool can not be selected

### [CS-1388](https://hpc-gridware.atlassian.net/browse/CS-1388) improvements and fixes to systemd and cgroups integration

- [CS-1753](https://hpc-gridware.atlassian.net/browse/CS-1753) side-by-side installation creates incorrect files for systemd

### [CS-1550](https://hpc-gridware.atlassian.net/browse/CS-1550) Scheduler Binding Improvements

- [CS-106](https://hpc-gridware.atlassian.net/browse/CS-106) create a test binary that detects and outputs its binding
- [CS-1784](https://hpc-gridware.atlassian.net/browse/CS-1784) Optimize scheduler binding performance for PE jobs on Epic compute nodes

### [CS-1575](https://hpc-gridware.atlassian.net/browse/CS-1575) improvements and fixes to encryption of messages between commands and daemons

- [CS-1855](https://hpc-gridware.atlassian.net/browse/CS-1855) re-creating certificates sometimes is not done

### [CS-1626](https://hpc-gridware.atlassian.net/browse/CS-1626) CRA Part1: Compliance Foundation

- [CS-1628](https://hpc-gridware.atlassian.net/browse/CS-1628) Establish vulnerability reporting process
- [CS-1629](https://hpc-gridware.atlassian.net/browse/CS-1629) Publish security.txt on Web Server
- [CS-1630](https://hpc-gridware.atlassian.net/browse/CS-1630) Document internal vulnerability response process
- [CS-1631](https://hpc-gridware.atlassian.net/browse/CS-1631) Define 24h incident reporting process (ENISA)
- [CS-1632](https://hpc-gridware.atlassian.net/browse/CS-1632) Assign security incident responsible personnel
- [CS-1634](https://hpc-gridware.atlassian.net/browse/CS-1634) Introduce periodic security reviews
- [CS-1636](https://hpc-gridware.atlassian.net/browse/CS-1636) Create STRIDE threat model
- [CS-1637](https://hpc-gridware.atlassian.net/browse/CS-1637) Define security governance
- [CS-1643](https://hpc-gridware.atlassian.net/browse/CS-1643) Add SECURITY.md

### [CS-1627](https://hpc-gridware.atlassian.net/browse/CS-1627) CRA Part2: Full Compliance

- [CS-1659](https://hpc-gridware.atlassian.net/browse/CS-1659) Define patch policy

### [CS-1699](https://hpc-gridware.atlassian.net/browse/CS-1699) Improvements of JSV

- [CS-59](https://hpc-gridware.atlassian.net/browse/CS-59) Add a Python JSV implementation.

### [CS-204](https://hpc-gridware.atlassian.net/browse/CS-204) Add scheduler categories to qmaster

- [CS-1160](https://hpc-gridware.atlassian.net/browse/CS-1160) Create TS module that parses client output of category specific commands
- [CS-211](https://hpc-gridware.atlassian.net/browse/CS-211) Create new test scenarios for categories

### [CS-31](https://hpc-gridware.atlassian.net/browse/CS-31) Create product manuals

- [CS-224](https://hpc-gridware.atlassian.net/browse/CS-224) Add GDI limit section to admin guide

### [CS-640](https://hpc-gridware.atlassian.net/browse/CS-640) RQS verifications, fixes and improvements

- [CS-1515](https://hpc-gridware.atlassian.net/browse/CS-1515) Create a RQS performance test

### [CS-767](https://hpc-gridware.atlassian.net/browse/CS-767) provide means to run performance tests on cloud resources

- [CS-1755](https://hpc-gridware.atlassian.net/browse/CS-1755) make sure that the current GCP deployment works for performance testing
- [CS-1847](https://hpc-gridware.atlassian.net/browse/CS-1847) improve performance of functions setting/resetting host slots for binding
- [CS-771](https://hpc-gridware.atlassian.net/browse/CS-771) make commlib performance test run in the cloud environment
- [CS-772](https://hpc-gridware.atlassian.net/browse/CS-772) make max_dyn_ec test run in the cloud environment

### Bug

- [CS-1101](https://hpc-gridware.atlassian.net/browse/CS-1101) Create an upgrade procedure for 9.1
- [CS-1282](https://hpc-gridware.atlassian.net/browse/CS-1282) Build on Ubuntu 24.04 fails due to default dependencies of hwloc to other libs like opencl or nvml
- [CS-162](https://hpc-gridware.atlassian.net/browse/CS-162) enable jemalloc for lx-riscv64 which failed with previous versions
- [CS-1714](https://hpc-gridware.atlassian.net/browse/CS-1714) in job info mails for aborted jobs the reason for the abort is truncated
- [CS-1735](https://hpc-gridware.atlassian.net/browse/CS-1735) qrsh doing massive output reports a commlib read error and truncates output
- [CS-1749](https://hpc-gridware.atlassian.net/browse/CS-1749) qrsh -V does not handle multi line environment variables (and shell functions)
- [CS-1756](https://hpc-gridware.atlassian.net/browse/CS-1756) Adding an execd in Qontrol tries to add it twice
- [CS-1757](https://hpc-gridware.atlassian.net/browse/CS-1757) When adding a new hostgroup with Qontrol host selections of former host group is preset
- [CS-1760](https://hpc-gridware.atlassian.net/browse/CS-1760) with reporting disabled there is increased spooling activity at midnight for 10 minutes
- [CS-1761](https://hpc-gridware.atlassian.net/browse/CS-1761) Debug checks are active even in a Release build
- [CS-1769](https://hpc-gridware.atlassian.net/browse/CS-1769) When adding a userset in Qontrol the entries are space separated and not comma separated in the backend
- [CS-1773](https://hpc-gridware.atlassian.net/browse/CS-1773) sge_execd crash in specific job error situation on a systemd host with no toplevel systemd slice configured
- [CS-1774](https://hpc-gridware.atlassian.net/browse/CS-1774) verify if toplevel systemd slice name is always created (installation, backup, restore, ...)
- [CS-1775](https://hpc-gridware.atlassian.net/browse/CS-1775) Simulated exec hosts do not inherit topology from parent
- [CS-1776](https://hpc-gridware.atlassian.net/browse/CS-1776) Deleting a manager or operator in Qontrol fails with a JSON error message
- [CS-1778](https://hpc-gridware.atlassian.net/browse/CS-1778) the pseudo terminal slave device created for jobs with option -pty y has  wrong ownership
- [CS-1781](https://hpc-gridware.atlassian.net/browse/CS-1781) Omit timestamp in gperf file names so that testsuite can generate pdf's automatically
- [CS-1783](https://hpc-gridware.atlassian.net/browse/CS-1783) Avoid characteristics updates from topology strings if they are not needed to improve performance
- [CS-1785](https://hpc-gridware.atlassian.net/browse/CS-1785) Stabilize sort order of different core types so that faster cores appear first
- [CS-1788](https://hpc-gridware.atlassian.net/browse/CS-1788) When adding a hostgroup to a hostgroup in Qontrol the hostgroup list is not loaded properly
- [CS-1791](https://hpc-gridware.atlassian.net/browse/CS-1791) Update Admin Guide with new qgpu parameters
- [CS-1793](https://hpc-gridware.atlassian.net/browse/CS-1793) qmaster ignores SIGHUB signal when running in debug mode which can cause freeze of component
- [CS-1800](https://hpc-gridware.atlassian.net/browse/CS-1800) mk_dist does not handle tar errors which can result in incomplete packages
- [CS-1801](https://hpc-gridware.atlassian.net/browse/CS-1801) In queue configuration dialog the allowed user list and denied user lists select dialog is not working
- [CS-1802](https://hpc-gridware.atlassian.net/browse/CS-1802) qevent reports incorrect numbers for jobs_running and jobs_registered
- [CS-1803](https://hpc-gridware.atlassian.net/browse/CS-1803) throughput test scenario mixed_qstat runs endlessly
- [CS-1811](https://hpc-gridware.atlassian.net/browse/CS-1811) topology in use missing in qhost -F output
- [CS-1818](https://hpc-gridware.atlassian.net/browse/CS-1818) Qontrol: Displaying userset in User Management does not display where the list is used
- [CS-1826](https://hpc-gridware.atlassian.net/browse/CS-1826) qsub -terse switch is ignored when embedded in a script file
- [CS-1828](https://hpc-gridware.atlassian.net/browse/CS-1828) clustermq does not terminate pending worker in case R workload has already been processed
- [CS-1837](https://hpc-gridware.atlassian.net/browse/CS-1837) Solaris SMF related scripts in $SGE_ROOT/util/sgeSMF are not executable
- [CS-1843](https://hpc-gridware.atlassian.net/browse/CS-1843) installation with munge enabled fails since munge update
- [CS-1849](https://hpc-gridware.atlassian.net/browse/CS-1849) testsuite auto-install fails when started with option install_rc
- [CS-1854](https://hpc-gridware.atlassian.net/browse/CS-1854) When modifying a resource quota set in Qontrol it deletes other resource quota sets
- [CS-427](https://hpc-gridware.atlassian.net/browse/CS-427) qhost -F prints capacity of INT or RSMAP resources as double, not as integer

### Improvement

- [CS-1471](https://hpc-gridware.atlassian.net/browse/CS-1471) Install Debian Trixie in our test lab environment.
- [CS-1590](https://hpc-gridware.atlassian.net/browse/CS-1590) improve sge_resource_quota.5 man page, esp. regarding expansion of filters
- [CS-1739](https://hpc-gridware.atlassian.net/browse/CS-1739) cleanup in builtin interactive job support: do not call cl_commlib_trigger()
- [CS-1780](https://hpc-gridware.atlassian.net/browse/CS-1780) Allow filtering of components debug output by thread name
- [CS-1782](https://hpc-gridware.atlassian.net/browse/CS-1782) Improve binding performance in scheduler by replacing the characteristics-value-type
- [CS-1794](https://hpc-gridware.atlassian.net/browse/CS-1794) Improve debug support for qmaster in performance test environment
- [CS-1798](https://hpc-gridware.atlassian.net/browse/CS-1798) Optimization: Certain host name operations unnecessarily normalize names
- [CS-1804](https://hpc-gridware.atlassian.net/browse/CS-1804) Optimization: Improve code related to pattern matching by introducing lookup tabels
- [CS-1805](https://hpc-gridware.atlassian.net/browse/CS-1805) Optimization: Improve RQS performance by optimizing low level core functions
- [CS-1827](https://hpc-gridware.atlassian.net/browse/CS-1827) Support adjustments of clustermq template parameters
- [CS-1836](https://hpc-gridware.atlassian.net/browse/CS-1836) testsuite shall allow empty shadowd_hosts list

### New Feature

- [CS-1730](https://hpc-gridware.atlassian.net/browse/CS-1730) Final QA tasks for new scheduler based binding.
- [CS-1762](https://hpc-gridware.atlassian.net/browse/CS-1762) Add gdi_request_limits to global configuration in Qontrol in version 9.1
- [CS-1763](https://hpc-gridware.atlassian.net/browse/CS-1763) Add binding_params to global configuration in Qontrol for version 9.1
- [CS-1795](https://hpc-gridware.atlassian.net/browse/CS-1795) Add 9.1.0 features (core binding, mail tag, gdi request limits) to global configuration dialog and execd params
- [CS-1796](https://hpc-gridware.atlassian.net/browse/CS-1796) Qontrol needs to detect GCS version 9.0.* and 9.1.*  and use different backend library versions
- [CS-1797](https://hpc-gridware.atlassian.net/browse/CS-1797) Add Qontrol to 9.1beta2 and future 9.1.* packages
- [CS-1821](https://hpc-gridware.atlassian.net/browse/CS-1821) Add PE templates to Qontrol so that admin can easily select and install

### Task

- [CS-1731](https://hpc-gridware.atlassian.net/browse/CS-1731) Update all lab hosts before 9.1.0beta1 build
- [CS-1743](https://hpc-gridware.atlassian.net/browse/CS-1743) do memory testing on master branch for the 9.1.0 release
- [CS-1746](https://hpc-gridware.atlassian.net/browse/CS-1746) schedule file does not show entry for host for jobs started in an AR
- [CS-1752](https://hpc-gridware.atlassian.net/browse/CS-1752) Remove other product names from sge_conf page where possible
- [CS-1758](https://hpc-gridware.atlassian.net/browse/CS-1758) remove dependency on libtirp
- [CS-1786](https://hpc-gridware.atlassian.net/browse/CS-1786) Create sperf_binding test to measure performance of binding in scheduler with Google Performance Tools
- [CS-1822](https://hpc-gridware.atlassian.net/browse/CS-1822) Make a full TS run with Ubuntu 26.04 as master and preferred exec host
- [CS-1823](https://hpc-gridware.atlassian.net/browse/CS-1823) Add Ubuntu 26.04 to lab environment and document OCS/GCS build requirements
- [CS-1830](https://hpc-gridware.atlassian.net/browse/CS-1830) Add RHEL 10 to lab environment and document OCS/GCS build requirements
- [CS-1831](https://hpc-gridware.atlassian.net/browse/CS-1831) Add ALMA 10 to lab environment and document OCS/GCS build requirements
- [CS-1832](https://hpc-gridware.atlassian.net/browse/CS-1832) Add Rocky 10 to lab environment and document OCS/GCS build requirements


[//]: # (Each file has to end with two empty lines)

