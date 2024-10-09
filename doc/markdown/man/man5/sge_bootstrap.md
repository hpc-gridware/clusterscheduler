---
title: sge_bootstrap
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

xxqs_name_sxx_bootstrap - xxQS_NAMExx bootstrap file

# DESCRIPTION

*bootstrap* contains parameters that are needed for the startup of xxQS_NAMExx components. It is created during the 
xxqs_name_sxx_qmaster installation. Modifying *bootstrap* in a running system is not supported.

# FORMAT

The paragraphs that follow provide brief descriptions of the individual parameters that compose the bootstrap 
configuration for a xxQS_NAMExx cluster:

## *admin_user*

Administrative user account used by xxQS_NAMExx for all internal file handling (status spooling, message logging, 
etc.). Can be used in cases where the root account does not have the corresponding file access permissions 
(e.g. on a shared file system without global root read/write access).

Being a parameter set at installation time changing *admin_user* in a running system is not supported. Changing it 
manually on a shut-down cluster is possible, but if access to the xxQS_NAMExx spooling area is
interrupted, this will result in unpredictable behavior.

The *admin_user* parameter has no default value, but instead it is defined during the master installation procedure.

## *default_domain*

Only needed if your xxQS_NAMExx cluster covers hosts belonging to more than a single DNS domain. In this case it can 
be used if your hostname resolving yields both qualified and unqualified hostnames for the hosts in one of the DNS 
domains. The value of *default_domain* is appended to the unqualified hostname to define a fully qualified hostname. 
The *default_domain* parameter will have no effect if *ignore_fqdn* is set to *true*.

Being a parameter set at installation time changing *default_domain* in a running system is not supported. The default 
for *default_domain* is *none*, in which case it will not be used.

## *ignore_fqdn*

Ignore fully qualified domain name component of hostnames. Should be set if all hosts belonging to a xxQS_NAMExx 
cluster are part of a single DNS domain. It is switched on if set to either *true* or *1*. Switching it on may solve 
problems with load reports due to different hostname resolutions across the cluster.

Being a parameter set at installation time changing *ignore_fqdn* in a running system is not supported. The default 
for *ignore_fqdn* is *true*.

## *spooling_method*

Defines how xxqs_name_sxx_qmaster(8) writes its configuration and the status information of a running cluster.

The available spooling methods are *berkeleydb* and *classic*.

## *spooling_lib*

The name of a shared library containing the *spooling_method* to be loaded at xxqs_name_sxx_qmaster(8) initialization 
time. The extension characterizing a shared library (.so, .sl, .dylib etc.) is not contained in *spooling_lib*.

If *spooling_method* was set to *berkeleydb* during installation, *spooling_lib* is set to *libspoolb*, if *classic* 
was chosen as *spooling_method*, *spooling_lib* is set to *libspoolc*.

Not all operating systems allow the dynamic loading of libraries. On these platforms a certain spooling method 
(default: berkeleydb) is compiled into the binaries and the parameter *spooling_lib* will be ignored.

## *spooling_params*

Defines parameters to the chosen spooling method.

Parameters that are needed to initialize the spooling framework, e.g. to open database files or to connect to a 
certain database server.

The spooling parameters value for spooling method *berkeleydb* is \[rpc_server:\]database directory, e.g.
/sge_local/default/spool/qmaster/spooldb for spooling to a local filesystem, or myhost:sge for spooling over a 
Berkeley DB RPC server.

For spooling method *classic* the spooling parameters take the form \<common_dir>;\<qmaster spool dir>, e.g.
/sge/default/common;/sge/default/spool/qmaster

## *binary_path*

The directory path where the xxQS_NAMExx binaries reside. It is used within xxQS_NAMExx components to locate and 
startup other xxQS_NAMExx programs.

The path name given here is searched for binaries as well as any directory below with a directory name equal to 
the current operating system architecture. Therefore, /usr/xxQS_NAME_Sxx/bin will work for all architectures, if the 
corresponding binaries are located in subdirectories named lx-amd64, lx-arm64, sol-amd64 etc.

The default location for the binary path is \<xxqs_name_sxx_root>/bin

## *qmaster_spool_dir*

The location where the master spool directory resides. Only the xxqs_name_sxx_qmaster(8) and xxqs_name_sxx_shadowd(8) 
need to have access to this directory. The master spool directory - in particular the job_scripts directory and the 
messages log file - may become quite large depending on the size of the cluster and the number of jobs. Be sure to
allocate enough disk space and regularly clean off the log files, e.g. via a cron(8) job.

Being a parameter set at installation time changing *qmaster_spool_dir* in a running system is not supported.

The default location for the master spool directory is \<xxqs_name_sxx_root>/\<cell>/spool/qmaster.

## *security_mode*

The security mode defines the set of security features the installed cluster is using.

Possible security mode settings are none, afs, dce, kerberos, csp. (no additional security, AFS, DCE, KERBEROS, 
CSP security model).

## *listener_threads*

The number of listener threads (allowed: 4-32, defaults set by installation). 

## *worker_threads*

The number of worker threads (allowed: 4-32, defaults set by installation). 

## *reader_threads*

The number of reader threads (allowed: 4-32, defaults set by installation). 

## *scheduler_threads*

The number of scheduler threads (allowed: 0-1, default set by installation: 1, off: 0). (see `qconf -kt/-at` option)

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
