---
title: sge_diagnostics
section: 1
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

xxqs_name_sxx_diagnostics - xxQS_NAMExx diagnostics documentation

# DESCRIPTION

This document describes how to collect diagnostic information for xxQS_NAMExx installations.
It is intended to be used by system administrators and support personnel to gather relevant information about the xxQS_NAMExx installation and its current state.

## Error Codes reported in the failed state of jobs

The `failed` attribute of both `qstat -j <job_id>` and `qacct -j <job_id>` commands can contain error codes that
indicate the reason for a job failure.

Depending on the error code the job or the queue instance may be set into error state.

The reason for a queue error state can be queried via `qstat -explain E`.  
The error state can be cleared via `qmod -cq <queue_name>`.

The reason for a job error state can be queried via `qstat -j <job_id>`.   
The error state can be cleared via `qmod -cj <job_id>`.

The following table lists the error codes and their meaning:

|  Code | Name / Meaning                                                                                                                                                                                                      |
|------:|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
|   `0` | `STATUS_OK`: Job ran through and exited normally                                                                                                                                                                    |
|   `1` | `SSTATE_FAILURE_BEFORE_JOB`: `sge_execd`  cannot start the job. The job or queue instances may be set into error state and further information will be available via `qstat -j <job_id>` and/or `qstat -explain E`. |
|   `2` | `ESSTATE_NO_SHEPHERD`: `sge_shepherd` cannot be executed, see `sge_execd` messages file for details.                                                                                                                |
|   `3` | `SSTATE_NO_CONFIG`: `sge_execd` could not write the `sge_shepherd` config file.                                                                                                                                     |
|    `4` | `SSTATE_NO_PID`: `sge_shepherd` did not write its `pid` file (poss. as it crashed), see the `sge_shepherd` trace file for details.                                                                                  |
|    `5` | `SSTATE_READ_CONFIG`: `sge_shepherd` cannot read its `config` file.                                                                                                                                                 |
|    `6` | `SSTATE_PROCSET_NOTSET`: On Solaris: `sge_shepherd` could not create a processor set.                                                                                                                               |
|    `7` | `SSTATE_BEFORE_PROLOG`: `sge_shepherd` could not start a prolog.                                                                                                                                                    |
|    `8` | `SSTATE_PROLOG_FAILED`: A prolog was started by `sge_shepherd` but failed.                                                                                                                                          |
|    `9` | `SSTATE_BEFORE_PESTART`: `sge_shepherd` could not start a PE start procedure.                                                                                                                                       |
|   `10` | `SSTATE_PESTART_FAILED`: A PE start procedure was started by `sge_shepherd` but failed.                                                                                                                             |
|   `11` | `SSTATE_BEFORE_JOB`: `sge_shepherd` could  not start the job. More information can be found in the `sge_shepherd` trace file.                                                                                       |
|   `12` | `SSTATE_BEFORE_PESTOP`: `sge_shepherd` could not start a PE stop procedure.                                                                                                                                         |
|   `13` | `SSTATE_PESTOP_FAILED`: A PE stop procedure was started by `sge_shepherd` but failed.                                                                                                                               |
|   `14` | `SSTATE_BEFORE_EPILOG`: `sge_shepherd` could not start an epilog.                                                                                                                                                   |
|   `15` | `SSTATE_EPILOG_FAILED`: An epilog was started by `sge_shepherd` but failed.                                                                                                                                         |
|   `16` | `SSTATE_PROCSET_NOTFREED`: On Solaris: `sge_shepherd` could not release a previously created processor set.                                                                                                         |
|   `17` | `ESSTATE_DIED_THRU_SIGNAL`: The job died through a signal.                                                                                                                                                          |
|   `18` | `ESSTATE_SHEPHERD_EXIT`: `sge_shepherd` exited with exit status > 0.                                                                                                                                                |
|   `19` | `ESSTATE_NO_EXITSTATUS`: `sge_shepherd` didn't write its `exit_status` file - possibly crashed before exiting regularly.                                                                                            |
|   `20` | `ESSTATE_UNEXP_ERRORFILE`: The `sge_shepherd` `error` file couldn't be read.                                                                                                                                        |
|   `21` | `ESSTATE_UNKNOWN_JOB`: `sge_execd` got a message from `sge_qmaster` about a job it doesn't know about.                                                                                                              |
|   `22` | `ESSTATE_EXECD_LOST_RUNNING`: Job removed manually.                                                                                                                                                                 |
|   `23` | `ESSTATE_PTF_CANT_GET_PIDS`: PTF can't get information for certain pids.                                                                                                                                            |
|   `24` | `SSTATE_MIGRATE`: The job was checkpointed for migration.                                                                                                                                                           |
|   `25` | `SSTATE_AGAIN`: The job shall be re-started.                                                                                                                                                                        |
|   `26` | `SSTATE_OPEN_OUTPUT`: Error, input, or output file couldn't be opened by `sge_shepherd`.                                                                                                                            |
|   `27` | `SSTATE_NO_SHELL`: The requested shell could not be found by `sge_shepherd`.                                                                                                                                        |
|   `28` | `SSTATE_NO_CWD`: `sge_shepherd` cannot change directory to the requested job directory.                                                                                                                             |
|   `29` | `SSTATE_AFS_PROBLEM`: AFS setup failed.                                                                                                                                                                             |
|   `30` | `SSTATE_APPERROR`: The job exited with exit_status 100 (application error)                                                                                                                                          |
|   `36` | `SSTATE_CHECK_DAEMON_CONFIG`: The daemon for an interactive job could not be found (if `rsh_daemon`, `rlogin_daemon`, `qlogin_daemon` is configured to a daemon path, instead of `builtin`)                         |
|   `37` | `SSTATE_QMASTER_ENFORCED_LIMIT: `sge_qmaster` enforced killing the job due to a limit.                                                                                                                              |
|   `38` | `SSTATE_ADD_GRP_SET_ERROR`: `sge_shepherd` cannot attach the additional group id to the `sge_shepherd` child process becoming the job.                                                                              |
|  `100` | `SSTATE_FAILURE_AFTER_JOB`: The job ran through, but no `usage` file was written by `sge_shepherd`.                                                                                                                 |


More details about errors reported by `sge_execd` can be found in the `sge_execd` messages file.   
For errors reported by `sge_shepherd` please check the `sge_shepherd` trace file or the error mail (if requested at job submission) or administrator mail (if configured in the global configuration).

## Administrator Mail

The administrator mail features a mail message for each failed job.
The mail message contains

* general information
  * about the job, e.g., job owner, queue, start time and end time (if available)
  * about the error, e.g., `failed in prolog:2025-12-02 16:45:32.632767 [6001:104882]: execvp(/no/such/prolog, "/no/such/prolog") failed: No such file or directory`
  * actions taken due to the error, e.g., `Job 4 caused action: Queue "all.q@<hostname>" set to ERROR`
* the shepherd `trace` file
* the shepherd `error` file
* the `pe_hostfile`

The administrator mail address can be configured in the global configuration file (see `sge_conf.5`).

For most of the error codes the administrator mail is sent for every failed job.

For specific configuration-related failures (prolog and epilog configuration), the administrator mail is sent only once for the first failed job. It will be sent again if the configuration is changed (either a local or global configuration, or a queue is changed).

For a few error codes no administrator mail is sent.

The following table lists the error codes and the frequency of sending. See [Error Codes reported in the failed state of jobs](#error-codes-reported-in-the-failed-state-of-jobs) for a list of the error codes and their meaning.

| Code                       | Frequency |
|:---------------------------|:----------|
| SSTATE_FAILURE_BEFORE_JOB  | NEVER     |
| ESSTATE_NO_SHEPHERD        | NEVER     |
| ESSTATE_NO_CONFIG          | ALWAYS    |
| ESSTATE_NO_PID             | ALWAYS    |
| SSTATE_PROCSET_NOT_SET     | ALWAYS    |
| SSTATE_BEFORE_PROLOG       | ALWAYS    |
| SSTATE_PROLOG_FAILED       | ONCE      |
| SSTATE_BEFORE_PESTART      | ALWAYS    |
| SSTATE_PESTART_FAILED      | ALWAYS    |
| SSTATE_BEFORE_JOB          | ALWAYS    |
| SSTATE_BEFORE_PESTOP       | ALWAYS    |
| SSTATE_PESTOP_FAILED       | ALWAYS    |
| SSTATE_BEFORE_EPILOG       | ALWAYS    |
| SSTATE_EPILOG_FAILED       | ONCE      |
| SSTATE_PROCSET_NOTFREED    | ALWAYS    |
| ESSTATE_DIED_THRU_SIGNAL   | ALWAYS    |
| ESSTATE_SHEPHERD_EXIT      | ALWAYS    |
| ESSTATE_NO_EXITSTATUS      | ALWAYS    |
| ESSTATE_UNEXP_ERRORFILE    | ALWAYS    |
| ESSTATE_UNKNOWN_JOB        | ALWAYS    |
| ESSTATE_EXECD_LOST_RUNNING | NEVER     |
| ESSTATE_PTF_CANT_GET_PIDS  | NEVER     |
| SSTATE_MIGRATE             | NEVER     |
| SSTATE_AGAIN               | NEVER     |
| SSTATE_OPEN_OUTPUT         | ALWAYS    |
| SSTATE_NO_SHELL            | ALWAYS    |
| SSTATE_NO_CWD              | ALWAYS    |
| SSTATE_AFS_PROBLEM         | ALWAYS    |
| SSTATE_APPERROR            | ALWAYS    |
| SSTATE_CHECK_DAEMON_CONFIG | NEVER     |

# ENVIRONMENTAL VARIABLES

For a complete list of common environment variables used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

# FILES

The `sge_shepherd` trace file is located in `<sge_shepherd_spool_dir>/active_jobs/<job_id>.<array_task_id>/trace` (where `<array_task_id>` is `1` for non-array jobs).   
Set the `execd_params` attribute `KEEP_ACTIVE` to keep the active job directories after job termination. See xxqs_name_sxx_conf(5) for details.

The `sge_execd` messages file is located in the `sge_execd` spool directory.

# SEE ALSO

xxqs_name_sxx_conf(5), xxqs_name_sxx_execd(8), xxqs_name_sxx_shepherd(8)

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
