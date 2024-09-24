---
title: sge_host_aliases
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

xxqs_name_sxx_host_aliases - xxQS_NAMExx host aliases file format

# DESCRIPTION

All xxQS_NAMExx components use a hostname resolving service provided by the communication library to identify hosts 
via a unique hostname. The communication library itself references standard UNIX directory services
such as DNS, NIS and /etc/hosts to resolve hostnames. In rare cases these standard services cannot be setup cleanly 
and xxQS_NAMExx communication daemons running on different hosts are unable to automatically determine a unique 
hostname for one or all hosts which can be used on all hosts. In such situations a xxQS_NAMExx host aliases file
can be used to provide the communication daemons with a private and consistent hostname resolution database.

The location for the host aliases file is \<xxqs_name_sxx_root>/\<cell>/common/host_aliases.

# FORMAT

For each host a single line must be provided with a blank, comma or semicolon separated list of hostname aliases. 
The first alias is defined to be the *unique* hostname which will be used by all xxQS_NAMExx components using the
hostname aliasing service of the communication library.

# SEE ALSO

xxqs_name_sxx_intro(1)

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
