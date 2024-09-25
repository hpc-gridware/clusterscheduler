---
title: sge_execd
section: 8
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

`xxqs_name_sxx_execd` - xxQS_NAMExx job execution agent

# SYNOPSIS

`xxqs_name_sxx_execd` \[ `-help*` \]

# DESCRIPTION

`xxqs_name_sxx_execd` controls the xxQS_NAMExx queues local to the machine on which `xxqs_name_sxx_execd` is running 
and executes/controls the jobs sent from xxqs_name_sxx_qmaster(8) to be run on these queues.  

# OPTIONS

## -help

Prints a listing of all options.

# LOAD SENSORS

If a *load sensor* is configured for `xxqs_name_sxx_execd` via either the global host configuration or the 
execution-host-specific cluster configuration (See xxqs_name_sxx_conf(5)), the executable path of the load sensor 
is invoked by `xxqs_name_sxx_execd` on a regular basis and delivers one or multiple load figures for the execution 
host (e.g. users currently logged in) or the complete cluster (e.g. free disk space on a network wide scratch 
file system). The load sensor may be a script or a binary executable. In either case its handling of the STDIN and
STDOUT streams and its control flow must comply to the following rules:

The load sensor must be written as an infinite loop waiting at a certain point for input from STDIN. If the string 
"quit" is read from STDIN, the load sensor should exit. When an end-of-line is read from STDIN, a load
data retrieval cycle should start. The load sensor then performs whatever operation is necessary to compute the 
desired load figures. At the end of the cycle the load sensor writes the result to stdout. The
format is as follows:

-   A load value report starts with a line containing only either the word "start" or the word "begin".

-   Individual load values are separated by newlines.

-   Each load value report consists of three parts separated by colons (":") and containing no blanks.

-   The first part of a load value information is either the name of the host for which load is reported or the 
    special name "global".

-   The second part is the symbolic name of the load value as defined in the host or global complex list 
    (see xxqs_name_sxx_complex(5) for details). If a load value is reported for which no entry in the host or global
    complex list exists, the reported load value is not used.

-   The third part is the measured load value.

-   A load value report ends with a line with only the word "end".

# ENVIRONMENTAL VARIABLES

For a complete list of common environment variables used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

# RESTRICTIONS

`xxqs_name_sxx_execd` usually is started from root on each machine in the xxQS_NAMExx pool. If started by a 
normal user, a spool directory must be used to which the user has read/write access. In this case only jobs being 
submitted by that same user are handled correctly by the system.

# FILES

For a complete list of files used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

**sgepasswd contains a list of user names and their** corresponding
encrypted passwords. If available, the password file will be used by
**sge_execd. To change the contents ** of this file please use the
**sgepasswd command. It is not advised to change ** that file manually.

## <xxqs_name_sxx_root>/<cell>/common/configuration
xxQS_NAMExx global configuration

## <xxqs_name_sxx_root>/<cell>/common/local_conf/<host>
xxQS_NAMExx host specific configuration

## <xxqs_name_sxx_root>/<cell>/spool/<host>
Default execution host spool directory
    
## <xxqs_name_sxx_root>/<cell>/common/act_qmaster
xxQS_NAMExx master host file

# SEE ALSO

xxqs_name_sxx_intro(1), xxqs_name_sxx_conf(5), xxqs_name_sxx_complex(5), xxqs_name_sxx_qmaster(8).

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
