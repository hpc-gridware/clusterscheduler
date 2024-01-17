---
title: qquota
section: 1
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

`qquota` - shows current usage of xxQS_NAMExx resource quotas

# SYNTAX

`qquota` \[ `-h` *wc_host*\|*wc_hostgroup*,...\] \[`-help`\] \[`-l` *resource_name*,...\] \[`-u` *wc_user*,...\] 
\[`-P` *wc_project,...*\] \[`-pe` *wc_pe_name,...*\] \[`-q` *wc_cqueue*,...\] \[`-xml`\]

# DESCRIPTION

`qquota` shows the current xxQS_NAMExx resource quota sets. Only resource quota sets with a positive usage count or 
a static limit are printed.

Selection options allow you to filter for specific hosts, cluster queues, projects, parallel environments (pe), 
resources or users. Without any option `qquota` will display a list of all resource quota sets for the calling user.

# OPTIONS

## -h *wc_host* \| *wc_hostgroup*,...  
Display only resource quota sets that matches with the hosts in the
given wildcard host or host group list. Find the definition of *wc_host* and *wc_hostgroup* in xxqs_name_sxx_types(1).

## -help  
Prints a listing of all options.

## -l *resource_name*,...  
Display only resource quota sets being matched with the resources in the given resource list.

## -u *wc_user*,...  
Display only resource quota sets being matched with the users in the given wildcard user list. Find the definition 
of *wc_user* in xxqs_name_sxx_types*(1).

## -P *wc_project*,...  
Display only resource quota sets being matched with the projects in the given wildcard project list. Find the 
definition of *wc_project* in xxqs_name_sxx_types(1).

## -pe *wc_pe_name*,...  
Display only resource quota sets being matched with the parallel environments (pe) in the given wildcard pe list. 
Find the definition of *wc_pe_name* in xxqs_name_sxx_types(1).

## -q *wc_cqueue*,...  
Display only resource quota sets being matched with the queues in the given wildcard cluster queue list. 
Find the definition of *wc_cqueue* in xxqs_name_sxx_types(1).

## -xml  
This option can be used with all other options and changes the output to XML. The schema used is referenced in the 
XML output. The output is printed to stdout.

# OUTPUT FORMATS

A line is printed for every resource quota with a positive usage count  or a static resource. The line consists of

- the resource quota - rule set name/rule name or number of rule in ruleset
- the limit - resource name, the number of used and available entities of that resource
- the effective resource quota set filter

# ENVIRONMENTAL VARIABLES

For a complete list of common environment variables used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

# FILES

For a complete list of files used by all xxQS_NAMExx commands, see xxqs_name_sxx_intro(1).

## $HOME/.xxqs_name_sxx_qquota	
user `qquota` default options

# SEE ALSO

xxqs_name_sxx_intro(1), qalter(1), qconf(1), qhold(1), qmod(1), qstat(1), qsub(1), xxqs_name_sxx_queue_conf(5),
xxqs_name_sxx_execd(8), xxqs_name_sxx_qmaster(8), xxqs_name_sxx_shepherd(8).

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
