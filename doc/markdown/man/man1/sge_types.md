---
title: sge_types
section: 1
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

sge_types - xxQS_NAMExx type descriptions

# DESCRIPTION

The xxQS_NAMExx user interface consists of several programs and files. Some command-line switches and several file 
attributes are types. The syntax for these types is explained in this page.

# OBJECT TYPES

These types are used for defining xxQS_NAMExx configuration:

## *calendar_name*

A calendar name is the name of a xxQS_NAMExx calendar described in xxqs_name_sxx_calendar_conf(5).

    calendar_name := object_name

## *ckpt_name*

A *ckpt_name* is the name of a xxQS_NAMExx checkpointing interface described in xxqs_name_sxx_checkpoint(5).

    ckpt_name := object_name

## *complex_name*

A complex name is the name of a xxQS_NAMExx resource attribute described in xxqs_name_sxx_complex(5).

    complex_name := object_name

## *department_name*

A department name is the name of a xxQS_NAMExx department described in xxqs_name_sxx_access_list(5).

    department_name := object_name

## *host_identifier*

A host identifier can be either a host name or a host group name.

    host_identifier  := host_name | hostgroup_name

## *hostgroup_name*

A host group name is the name of a xxQS_NAMExx host group described in xxqs_name_sxx_hostgroup(5). Note, to allow 
host group names easily be differed from host names a "@" prefix is used.

    hostgroup_name := '@' object_name

## *host_name*

A host name is the official name of a host node. Host names with a domain specification such as 
"jira.hpc-gridware.com" are called fully-qualified host names, whereas host names like "jira" are
called short host names. Note, there are the install time parameters *default_domain* and *ignore_fqdn* 
(see xxqs_name_sxx_bootstrap(5)) which affect how xxQS_NAMExx deals with host names in general.

## *jsv_url*

The *jsv_url* has the following format:

    jsv_url := jsv_client_url | jsv_server_url
    jsv_server_url := [ type ':' ] \[ user '@' \] path
    jsv_client_url := [ type ':' ] path
    type := 'script'

At the moment only the *type* *script* is allowed. This means that *path* is either the path to a script or to a 
binary application which will be used to instantiate a JSV process. The *type* is optional till other *types* are 
supported by xxQS_NAMExx.

Specifying a *user* is only allowed for server JSVs. Client JSVs will automatically be started as submit user and 
server JSVs as admin user if not other specified.

The *path* has always to be the absolute path to a binary or application.

## *memory_specifier*

Memory specifiers are positive decimal, hexadecimal or octal integer constants which may be followed by a multiplier 
letter. Valid multiplier letters are k, K, m, M, g and G, where k means multiply the value by 1000, K multiply by 1024, 
m multiply by 1000\*1000, M multiply by 1024\*1024, g multiply by 1000\*1000\*1000 and G multiply by 1024\*1024\*1024. 
If no multiplier is present, the value is just counted in bytes.

## *pe_name*

A *pe_name* is the name of a xxQS_NAMExx parallel environment described in xxqs_name_sxx_pe(5).

    pe_name := object_name

## *project_name*

A project name is the name of a xxQS_NAMExx project described in xxqs_name_sxx_project(5).

    project_name := object_name

## *queue_name*

A queue name is the name of a xxQS_NAMExx queue described in xxqs_name_sxx_queue_conf(5).

    queue_name := object_name

## *time_specifier*

A time specifier either consists of a positive decimal, hexadecimal or octal integer constant, in which case the 
value is interpreted to be in seconds, or is built by 3 decimal integer numbers separated by colon signs where the 
first number counts the hours, the second the minutes and the third the seconds. If a number would be zero it can be 
left out but the separating colon must remain (e.g. 1:0:1 = 1::1 means 1 hour and 1 second).

## *user_name*

A username can be the name of a login(1) user or of the xxQS_NAMExx user object described in xxqs_name_sxx_user(5).

    user_name := object_name

## *userset_name*

A user set name is the name of an xxQS_NAMExx access list or department described in xxqs_name_sxx_access_list(5).

    userset_name := object_name

## *object_name*

An object name is a sequence of up to 512 ASCII characters except "\\n", "\\t", "\\r", " ", "/", ":", "'", "\\", 
"\[", "\]", "{", "}", "\|", "(", ")", "@", "%", "," or the " character itself.

# MATCHING TYPES

These types are used for matching xxQS_NAMExx configuration:

## **expression**

A wildcard expression is a regular boolean expression that consists of one or more patterns joined by boolean 
operators. When a wildcard expression is used, the following definition applies:

    expression	= ["!"] ["("] valExp [")"] [ AND_OR expression ]*
    valExp	= pattern | expression
    AND_OR	= "&" | "|"

where:

    "!"	not operator -- negate the following pattern or expression 
    "&"	and operator -- logically and with the following expression
    "|"	or operator -- logically or with the following expression
    "("	open bracket -- begin an inner expression.
    ")"	close bracket -- end an inner expression. 
    "pattern"	see the pattern definition that's follow

The expression itself should be put inside quotes ('"') to ensure that clients receive the complete expression. e.g.

    "(lx*|sol*)&*64*" any string beginning with either "lx" or "sol" and containing "64"
    "rh_8*&!rh_8.1"   any string beginning with "rh_8", except "rh_8.1"

## *pattern*

When patterns are used the following definitions apply:

    "*"	matches any character and any number of characters (between 0 and inv).
    "?"	matches any character. It cannot be no character
    "."	is the character ".". It has no other meaning
    "\"	escape character. "\\" = "\", "\*" = "*", "\?" = "?"
    "[...]"	specifies an array or a range of allowed 
    	    characters for one character at a specific position.
            Character ranges may be specified using the a-z notation.
            The caret symbol (^) is not interpreted as a logical
            not; it is interpreted literally.

    For more details please see fnmatch(5)

The pattern itself should be put inside quotes ('\"') to ensure that clients receive the complete pattern.

## *range*

The task range specifier has the form

    n[-m[:s]][',' n[-m[:s]]]* 
    n[-m[:s]][' ' n[-m[:s]]]*

and thus consists of a comma or blank separated list of range specifiers n\[-m\[:s\]\]. The ranges are concatenated 
to the complete task id range. Each range may be a single number, a simple range of the form n-m or a range with a 
step size.

## *wc_ar*

The wildcard advance reservation (AR) specification is a placeholder for AR ids, AR names including AR name patterns. 
An AR id always references
one AR, while the name and pattern might reference multiple ARs.

    wc_ar := ar_id | ar_name | pattern

## *wc_ar_list*

The wildcard advance reservation (AR) list specification allows to reference multiple ARs with one command.

    wc_ar_list := wc_ar [ ',' wc_ar ]*

## *wc_host*

A wildcard host specification is a wildcard expression which might match one or more hosts used in the cluster. 
The first character of that string never begins with an at-character ('@'), even if the expression begins with a 
wildcard character. E.g.

    *	all hosts
    a*	all host beginning with an 'a'	

## *wc_hostgroup*

A wildcard host group specification is a wildcard expression which might match one or more host groups. The first 
character of that string is always an at-character ('@').

More information concerning host groups can be found in xxqs_name_sxx_hostgroup(5). E.g.

    @*	        all hostgroups in the cluster
    @solaris	the @solaris hostgroup

## *wc_job*

The wildcard job specification is a placeholder for job ids, job names including job name patterns. A job id always 
references one job, while the name and pattern might reference multiple jobs.

    wc_job := job-id | job-name | pattern

## *wc_job_range*

The wildcard job range specification allows to reference specific array tasks for one or multiple jobs. The job is 
referenced via wc_job and in addition gets a range specifier for the array tasks.

    wc_job_range := wc_job [ '-t' range ]

## *wc_job_list*

The wildcard job list specification allows to reference multiple jobs with one command.

    wc_job_list := wc_job [ , wc_job , ...]

## *wc_job_range_list*

The wildcard job range list (*wc_job_range_list*) is specified by one of the following forms:

    wc_job [ '-t' range ]  [',' wc_job [ '-t' range ] ]*

If present, the *task_range* restricts the effect of the operations to the array job task range specified as suffix 
to the job id (see the `-t` option to qsub(1) for further details on array jobs).

## *wc_qdomain*

    wc_qdomain := wc_cqueue "@" wc_hostgroup

A wildcard expression queue domain specification starts with a wildcard expression cluster queue name followed by
an at-character '@' and a wildcard expression host group specification.

*wc_qdomain* are used to address a group of queue instances. All queue instances residing on a hosts which is part 
of matching host groups will be addressed. Please note, that *wc_hostgroup* always begins with an
at-character. E.g.

    *@@*	    all queue instances whose underlying host is part of at least one hostgroup
    a*@@e*	    all queue instances begins with a whose underlying host is part of at least one hostgroup begin with e
    *@@solaris	all queue instances on hosts part of the @solaris hostgroup

## *wc_cqueue*

A wildcard expression cluster queue specification is a wildcard expression which might match one or more cluster 
queues used in the cluster. That string never contains an at-character ('@'), even if the expression begins with a 
wildcard character. E.g.

    *	        all cluster queues
    a*	        all cluster queues beginning with an 'a'
    a*&!adam	all cluster queues beginning with an 'a',but not adam

## *wc_qinstance*

    wc_qinstance := wc_cqueue "@" wc_host

A wildcard expression queue instance specification starts with a wildcard expression cluster queue name followed by 
an at-character '@' and a wildcard expression hostname.

*wc_qinstance* expressions are used to address a group of queue instances whose underlying hostname matches the 
given expression. Please note that the first character of *wc_host* does never match the at-character '@'. E.g.

    *@*	    all queue instances in the cluster
    *@b*	all queue instances whose hostname begins with a 'b'
    *@b*|c*	all queue instances whose hostname begins with a 'b' or 'c'

## *wc_queue*

    wc_queue := wc_cqueue | wc_qdomain | wc_qinstance

A wildcard queue expression might either be a wildcard expression cluster queue specification or a wildcard
expression queue domain specification or a wildcard expression queue instance specification. E.g.

    big_*1	    cluster queues which begin with "big_" and end with "1" 
    big_*&!*1	cluster queues which begin with "big_" ,but does not end with "1" 
    *@ampere	all qinstances residing on host ampere 

## *wc_queue_list*

    wc_queue_list := wc_queue ["," wc_queue ]*

Comma separated list of wc_queue elements. E.g.

    big, medium_\*@@sol\*, \*@ampere.hpc-gridware.com

## *wc_user*

A wildcard username pattern is either a wildcard username specification or a full username.

    wc_user := user_name \| pattern

## *wc_user_list*

A list of usernames.

    wc_user_list := wc_user [ ',' wc_user]*

## *wc_project*

A wildcard project name pattern is either a wildcard project name specification or a full project name.

    wc_project := project | pattern

## *wc_pe_name*

A wildcard parallel environment name pattern is either a wildcard pe name specification or a full pe name.

    wc_pe_name := pe_name | pattern

## *parallel_env* 

    n['-'[m]] | [-]m [',' ...] 

Parallel programming environment (PE) to select for an AR. The range descriptor behind the PE name specifies the 
number of parallel processes to be run. xxQS_NAMExx will allocate the appropriate resources as available. 
The xxqs_name_sxx_pe(5) manual page contains information about the definition of PEs and about how to obtain a 
list of currently valid PEs.

You can specify a PE name which uses the wildcard character, "\*". Thus, the request "pvm\*" will match any 
parallel environment with a name starting with the string "pvm". In the case of multiple parallel environments whose 
names match the name string, the parallel environment with the most available slots is chosen.

The range specification is a list of range expressions of the form "n-m", where n and m are positive, 
non-zero integers. The form "n" is equivalent to "n-n". The form "-m" is equivalent to "1-m". The form "n-"
is equivalent to "n-infinity". The range specification is processed as follows: The largest number of queues 
requested is checked first. If enough queues meeting the specified attribute list are available, all are reserved. 
If not, the next smaller number of queues is checked, and so forth.

## *date_time*

The *date_time* value must conform to 

    [[CC]]YY]MMDDhhmm[.SS] 

where:

    CC	denotes the century in 2 digits.
    YY	denotes the year in 2 digits.
    MM	denotes the month in 2 digits.
    DD	denotes the day in 2 digits.
    hh	denotes the hour in 2 digits.
    mm	denotes the minute in 2 digits.
    ss	denotes the seconds in 2 digits (default 00).

## *time*

The time value must conform to hh:mm:ss, or seconds where:

    hh      denotes the hour in 2 digits.
    mm      denotes the minute in 2 digits.
    ss      denotes the seconds in 2 digits (default 00).
    seconds is a number of seconds (is used for duration values)

If any of the optional date fields are omitted, the corresponding value of the current date is assumed. If CC is not 
specified, a YY of \< 70 means 20YY. Use of this option may cause unexpected results if the clocks of the
hosts in the xxQS_NAMExx pool are out of sync. Also, the proper behavior of this option very much depends on the 
correct setting of the appropriate timezone, e.g. in the TZ environment variable (see date(1) for details), when 
the xxQS_NAMExx daemons xxqs_name_sxx_qmaster(8) and xxqs_name_sxx_execd(8) are invoked.

## *name*

The *name* may be any arbitrary alphanumeric ASCII string, but may not contain 
"\\n", "\\t", "\\r", "/", ":", "@", "\\", "\*", or "?".

# SEE ALSO

qacct(1), qconf(1), qquota(1), qsub(1), qrsub(1)

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
