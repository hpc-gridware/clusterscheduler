---
title: sge_qmaster
section: 8
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

xxqs_name_sxx_qmaster - xxQS_NAMExx master control daemon

# SYNOPSIS

**xxqs_name_sxx_qmaster** \[ **-help** \]

# DESCRIPTION

*xxqs_name_sxx_qmaster* controls the overall xxQS_NAMExx behavior in a
cluster.

# OPTIONS

-help  
Prints a listing of all options.

# ENVIRONMENTAL VARIABLES

xxQS_NAME_Sxx_ROOT  
Specifies the location of the xxQS_NAMExx standard configuration files.

xxQS_NAME_Sxx_CELL  
If set, specifies the default xxQS_NAMExx cell. To address a xxQS_NAMExx
cell *xxqs_name_sxx_qmaster* uses (in the order of precedence):

> The name of the cell specified in the environment variable
> xxQS_NAME_Sxx_CELL, if it is set.
>
> The name of the default cell, i.e. **default**.

xxQS_NAME_Sxx_DEBUG_LEVEL  
If set, specifies that debug information should be written to stderr. In
addition the level of detail in which debug information is generated is
defined.

xxQS_NAME_Sxx_QMASTER_PORT  
If set, specifies the tcp port on which *xxqs_name_sxx_qmaster* (8) is
expected to listen for communication requests. Most installations will
use a services map entry for the service "sge_qmaster" instead to define
that port.

xxQS_NAME_Sxx_EXECD_PORT  
If set, specifies the tcp port on which *xxqs_name_sxx_execd* (8) is
expected to listen for communication requests. Most installations will
use a services map entry for the service "sge_execd" instead to define
that port.

# RESTRICTIONS

*xxqs_name_sxx_qmaster* is usually started from root on the master or
shadow master machines of the cluster (refer to the *xxQS_NAMExx
Installation and Administration Guide* for more information about the
configuration of shadow master hosts). If started by a normal user, a
master spool directory must be used to which the user has read/write
access. In this case only jobs being submitted by that same user are
handled correctly by the system.

# FILES

    <xxqs_name_sxx_root>/<cell>/common/configuration
    	xxQS_NAMExx global configuration
    <xxqs_name_sxx_root>/<cell>/common/local_conf/<host>
    	xxQS_NAMExx host specific configuration
    <xxqs_name_sxx_root>/<cell>/common/qmaster_args
    	xxqs_name_sxx_qmaster argument file
    <xxqs_name_sxx_root>/<cell>/spool
    	Default master spool directory

# SEE ALSO

*xxqs_name_sxx_intro* (1), *xxqs_name_sxx_conf* (5),
*xxqs_name_sxx_execd* (8), *xxqs_name_sxx_shadowd* (8), *xxQS_NAMExx
Installation and Administration Guide*

# COPYRIGHT

See *xxqs_name_sxx_intro* (1) for a full statement of rights and
permissions.
