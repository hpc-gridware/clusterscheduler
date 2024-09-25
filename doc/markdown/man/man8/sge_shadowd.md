---
title: sge_shadowd
section: 8
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

`xxqs_name_sxx_shadowd` - xxQS_NAMExx shadow master daemon

# SYNOPSIS

`xxqs_name_sxx_shadowd`

# DESCRIPTION

`xxqs_name_sxx_shadowd` is a "light weight" process which can be run on so-called shadow master hosts in a 
xxQS_NAMExx cluster to detect failure of the current xxQS_NAMExx master daemon, xxqs_name_sxx_qmaster(8),
and to start-up a new xxqs_name_sxx_qmaster(8) on the host on which the `xxqs_name_sxx_shadowd` runs. 
If multiple shadow daemons are active in a cluster, they run a protocol which ensures that only one of them
will start-up a new master daemon.

The hosts suitable for being used as shadow master hosts must have shared root read/write access to the directory
\$xxQS_NAME_Sxx_ROOT/\$xxQS_NAME_Sxx_CELL/common as well as to the master daemon spool directory (by default
\$xxQS_NAME_Sxx_ROOT/\$xxQS_NAME_Sxx_CELL/spool/qmaster). The names of the shadow master hosts need to be contained 
in the file \$xxQS_NAME_Sxx_ROOT/\$xQS_NAME_Sxx_CELL/common/shadow_masters.

# RESTRICTIONS

`xxqs_name_sxx_shadowd` may only be started by root.

# ENVIRONMENT VARIABLES

For a complete list of common environment variables used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

xxQS_NAME_Sxx_DELAY_TIME  
This variable controls the interval in which `xxqs_name_sxx_shadowd` pauses if a takeover bid fails. This value is 
used only when there are multiple `xxqs_name_sxx_shadowd` instances, and they are contending to be the master. 
The default is 600 seconds.

xxQS_NAME_Sxx_CHECK_INTERVAL  
This variable controls the interval in which the `xxqs_name_sxx_shadowd` checks the heartbeat file (60 seconds 
by default).

xxQS_NAME_Sxx_GET_ACTIVE_INTERVAL  
This variable controls the interval when a `xxqs_name_sxx_shadowd` instance tries to take over when the heartbeat 
file has not changed. The default is 240 seconds.

# FILES

For a complete list of files used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

## <xxqs_name_sxx_root>/<cell>/common
Default configuration directory
    
## <xxqs_name_sxx_root>/<cell>/common/shadow_masters
Shadow master hostname file.
    
## <xxqs_name_sxx_root>/<cell>/spool/qmaster
Default master daemon spool directory
    
## <xxqs_name_sxx_root>/<cell>/spool/qmaster/heartbeat
The heartbeat file.

# SEE ALSO

xxqs_name_sxx_intro(1), xxqs_name_sxx_conf(5), xxqs_name_sxx_qmaster(8), 

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
