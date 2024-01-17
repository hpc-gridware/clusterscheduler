---
title: qping
section: 1
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

`qping` - check application status of xxQS_NAMExx daemons.

# SYNTAX

`qping` \[`-help`\] \[`-noalias`\] \[`-ssl`\|`-tcp`\]
    \[
        \[\[`-i` *interval*\] \[`-info`\] \[`-f`\]\] 
            \| 
        \[\[`-dump_tag` *tag* \[*param*\]\] \[`-dump`\] \[`-nonewline`\]\] 
    \] *host* *port* *name* *id*

# DESCRIPTION

`Qping` is used to validate the runtime status of a xxQS_NAMExx service daemon. The current xxQS_NAMExx 
implementation allows one to query the xxQS_NAME_Sxx_QMASTER daemon and any running xxQS_NAME_Sxx_EXECD daemon.
The `qping` command is used to send a SIM (Status Information Message) to the destination daemon. 
The communication layer of the specified daemon will respond with a SIRM (Status Information Response Message) which
contains status information about the consulted daemon.

The `qping -dump` and `-dump_tag` options allowing an administrator to observe the communication protocol data 
flow of a xxQS_NAMExx service daemon. The `-dump` switch must be started as user root on the same host where the 
observed daemon is running.

# OPTIONS

## -f

Show full status information on each ping interval.

* *First output Line:* The first output line shows the date and time of the request.
* *SIRM version:* Internal version number of the SIRM (Status Information Response Message)
* *SIRM message id:* Current message id for this connection
* *start time:* Start time of daemon in following format: *MM/DD/YYYY HH:MM:SS*
* *run time \[s\]:* Run time in seconds since *start time*
* *messages in read buffer:* Nr. of buffered messages in communication buffer. The messages are buffered for the 
  application (daemon). When this number grows too large the daemon is not able to handle all messages sent to it.
* *messages in write buffer:* Nr. of buffered messages in the communication write buffer. The messages are sent from 
  the application (daemon) to the connected clients, but the communication layer wasn't able to send the messages yet. 
  If this number grows too large, the communication layer isn't able to send them as fast as the application (daemon) 
  wants the messages to be sent.
*nr. of connected clients:* This is the number of actual connected clients to this daemon. This also implies the 
  current `qping` connection.
* *status:* The status value of the daemon. This value depends on the application which reply to the `qping` request. 
  If the application does not provide any information the status is 99999. Here are the possible status information 
  values for the xxQS_NAMExx daemons:

  **qmaster**:
  * *0* There is no unusual timing situation.
  * *1* One or more threads has reached warning timeout. This may happen when at least one thread does not increment 
    his time stamp for a not usual long time. A possible reason for this is a high workload for this thread.
  * *2* One or more threads has reached error timeout. This may happen when at least one thread has not incremented 
    his time stamp for longer than 10 minutes.
  * *3* The time measurement is not initialized.

  **execd**:
  * *0* There is no unusual timing situation.
  * *1* Dispatcher has reached warning timeout. This may happen when the dispatcher does not increment his time stamp 
    for an unusual long time. A possible reason for this is a high workload.
  * *2* Dispatcher has reached error timeout. This may happen when the dispatcher has not incremented his time stamp 
    for longer than 10 minutes.
  * *3* The time measurement is not initialized.
  
* *info:* Status message of the daemon. This value depends on the application which reply to the `qping` request. 
  If the application does not provide any information the info message is "not available". Here are the possible 
  status information values for the xxQS_NAMExx daemons:

  **qmaster**:

  The info message contains information about the *xxqs_name_sxx_qmaster(8) threads followed by a thread state and 
  time information. Each time when one of the known threads pass through their main loop the time information is 
  updated. Since the *xxqs_name_sxx_qmaster(8) has two message threads every message thread updates the time. 
  This means the timeout for the message thread (MT) can only occur when no message thread is active anymore:

    > *THREAD_NAME*: *THREAD_STATE* (*THREAD_TIME*)

  * *THREAD_NAME*:
    * MAIN: Main thread 
    * signaler: Signal thread 
    * event_master: Event master thread 
    * timer: Timer thread 
    * worker: Worker thread
    * listener: Listener thread
    * scheduler: Scheduler thread
    
    The thread names will be followed by a 3-digit number.
  
  * *THREAD_STATE*:
    * R: Running
    * W: Warning
    * E: Error
    
  * *THREAD_TIME*:
    Time since last timestamp updating.

  After the dispatcher information follows an additional information string which describes the complete application status.

  **execd**:

  The info message contains information for the execd job dispatcher: 
  
  >dispatcher: *STATE* (*TIME*)

  * *STATE*:
    * R: Running
    * W: Warning
    * E: Error

  * *TIME*:

    Time since last timestamp updating.

    After the thread information follows an additional information string which describes the application status.

  * *Monitor:* 
    If available, displays statistics on a thread. The data for each thread is displayed in one line. 
    The format of this line can be changed at any time. Only the master implements the monitoring.

## -help

Prints a list of all options.

## -i *interval*

Set `qping` interval time.

The default interval time is one second. `Qping` will send a SIM (Status Information Message) on each interval time.

## -info

Show full status information (see `-f` for more information) and exit. The exit value 0 indicates no error. 
On errors qping returns with 1.

## -noalias

Ignore host_aliases file, which is located at *\<xxqs_name_sxx_root>/\<cell>/common/host_aliases. If this option is
used it is not necessary to set any xxQS_NAMExx environment variable.

## -ssl

This option can be used to specify an SSL (Secure Socket Layer) configuration. The `qping` will use the configuration 
to connect to services running SSL. If the SGE settings file is not sourced, you have to use the `-noalias` option to 
bypass the need for the *SGE_ROOT* environment variable. The following environment variables are used to specify your 
certificates: 

* SSL_CA_CERT_FILE - CA certificate file
* SSL_CERT_FILE - certificates file SSL_KEY_FILE - key file
* SSL_RAND_FILE - rand file

## -tcp

This option is used to select TCP/IP as the protocol used to connect to other services.

## -nonewline

Dump output will not have a linebreak within a message and binary messages are not unpacked.

## -dump

This option allows an administrator to observe the communication protocol data flow of a xxQS_NAMExx service daemon. 
The `qping -dump` instruction must be started as root and on the same host where the observed daemon is running.

The output is written to stdout. The environment variable *SGE_QPING_OUTPUT_FORMAT* can be set to hide columns, 
set a default column width or to set a hostname output format. The value of the environment variable can be set 
to any combination of the following specifiers separated by a space character:

| Value  | Description                                                     |
|:-------|:----------------------------------------------------------------|
| h:X    | hide column X                                                   |
| s:X    | show column X                                                   |
| w:X:Y  | set width of column X to Y                                      |
| hn:X"  | set hostname output parameter X. X values are "long" or "short" |

Start `qping -help` to see which columns are available.

## -dump_tag *tag* \[*param*\]

This option has the same meaning as `-dump`, but can provide more information by specifying the debug level and 
message types `qping` should print:

`-dump_tag` *ALL* *debug_level*

This option shows all possible debug messages (APP+MSG) for the debug levels, *ERROR*, *WARNING*, *INFO*, *DEBUG*
and *DPRINTF*. The contacted service must support this kind of debugging. This option is not currently implemented.

`-dump_tag` *APP* *debug_level*

This option shows only application debug messages for the debug levels, *ERROR*, *WARNING*, *INFO*, *DEBUG* and 
*DPRINTF*. The contacted service must support this kind of debugging. This option is not currently implemented.

`-dump_tag` *MSG*

This option has the same behavior as the `-dump` option.

## *host*

Host where daemon is running.

## *port*

Port which daemon has bound (used xxqs_name_sxx_qmaster/xxqs_name_sxx_execd port number).

## *name*

Name of communication endpoint (*qmaster* or *execd*). A communication endpoint is a triplet of 
hostname/endpoint name/endpoint id (e.g. *hostA/qmaster/1* or *hostB/qstat/4*).

## *id*

Id of communication endpoint ("1" for daemons)

# EXAMPLES

```
>qping master_host 31116 qmaster
08/24/2004 16:41:15 endpoint master_host/qmaster/1 at port 31116 is up since 365761 seconds
08/24/2004 16:41:16 endpoint master_host/qmaster/1 at port 31116 is up since 365762 seconds
08/24/2004 16:41:17 endpoint master_host/qmaster/1 at port 31116 is up since 365763 seconds

> qping -info master_host 31116 qmaster 1
08/24/2004 16:42:47:
SIRM version:             0.1
SIRM message id:          1
start time:               08/20/2004 11:05:14 (1092992714)
run time [s]:             365853
messages in read buffer:  0
messages in write buffer: 0
nr. of connected clients: 4
status:                   0
info:                     ok

> qping -info execd_host 31117 execd 1
08/24/2004 16:43:45:
SIRM version:             0.1
SIRM message id:          1
start time:               08/20/2004 11:06:13 (1092992773)
run time [s]:             365852
messages in read buffer:  0
messages in write buffer: 0
nr. of connected clients: 2
status:                   0
info:                     ok
```

# ENVIRONMENTAL VARIABLES

For a complete list of common environment variables used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

# FILES

For a complete list of files used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

# SEE ALSO

xxqs_name_sxx_intro(1), xxqs_name_sxx_host_aliases(5), xxqs_name_sxx_qmaster(8), xxqs_name_sxx_execd(8).

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
