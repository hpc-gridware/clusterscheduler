---
title: sge_qtask
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

xxqs_name_sxx_qtask - file format of the qtask file.

# DESCRIPTION

A *qtask* file defines which commands are submitted to xxQS_NAMExx for
remote execution by *qtcsh*(1). The *qtask* file optionally may contain
*qrsh*(1) command-line parameters. These parameters are passed to the
*qrsh*(1) command being used by *qtcsh* to submit the commands.

A cluster global *qtask* file defining cluster wide defaults and a user
specific *qtask* file eventually overriding and enhancing those
definitions are supported. The cluster global file resides at
\<xxqs_name_sxx_root>/\<cell/common/qtask, while the user specific file
can be found at \~/.qtask. An exclamation mark preceding command
definitions in the cluster global can be used by the administrator to
deny overriding of such commands by users.

# FORMAT

The principle format of the *qtask* file is that of a tabulated list.
Each line starting with a '#' character is a comment line. Each line
despite comment lines defines a command to be started remotely.

Definition starts with the command name that must match exactly the name
as typed in a *qtcsh*(1) command-line. Pathnames are not allowed in
*qtask* files. Hence absolute or relative pathnames in *qtcsh*(1)
command-lines always lead to local execution even if the commands itself
are the same as defined in the *qtask* files.

The command name can be followed by an arbitrary number of *qrsh*(1)
option arguments which are passed on to *qrsh*(1) by *qtcsh*(1).

An exclamation mark prefixing the command in the cluster global *qtask*
file prevents overriding by the user supplied *qtask* file.

# EXAMPLES

The following *qtask* file

    netscape -l a=sol-sparc64 -v DISPLAY=myhost:0
    grep -l h=filesurfer
    verilog -l veri_lic=1

designates the applications netscape, grep and verilog for interactive
remote execution through xxQS_NAMExx. Netscape is requested to run only
on Solaris64 architectures with the DISPLAY environment variable set to
'myhost:0', grep only runs on the host named 'filesurfer' and verilog
requests availability of a verilog license in order to get executed
remotely.

# SEE ALSO

*xxqs_name_sxx_intro*(1), *qtcsh*(1), *qrsh*(1).

# COPYRIGHT

See *xxqs_name_sxx_intro*(1) for a full statement of rights and
permissions.
