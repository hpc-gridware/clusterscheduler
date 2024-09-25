---
title: sge_qmaster
section: 8
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

`xxqs_name_sxx_qmaster` - xxQS_NAMExx master control daemon

# SYNOPSIS

`xxqs_name_sxx_qmaster` \[ `-help` \]

# DESCRIPTION

`xxqs_name_sxx_qmaster` controls the overall xxQS_NAMExx behavior in a cluster.

# OPTIONS

## -help  
Prints a listing of all options.

# ENVIRONMENTAL VARIABLES

For a complete list of common environment variables used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

# RESTRICTIONS

`xxqs_name_sxx_qmaster` is usually started from root on the master or shadow master machines of the cluster 
If started by a normal user, a master spool directory must be used to which the user has read/write
access. In this case only jobs being submitted by that same user are handled correctly by the system.

# FILES

For a complete list of files used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

## <xxqs_name_sxx_root>/<cell>/common/configuration
xxQS_NAMExx global configuration

## <xxqs_name_sxx_root>/<cell>/common/local_conf/<host>
xxQS_NAMExx host specific configuration

## <xxqs_name_sxx_root>/<cell>/common/qmaster_args
xxqs_name_sxx_qmaster argument file

## <xxqs_name_sxx_root>/<cell>/spool
Default master spool directory

# SEE ALSO

xxqs_name_sxx_intro(1), xxqs_name_sxx_conf(5), xxqs_name_sxx_execd(8), xxqs_name_sxx_shadowd(8)

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
