---
title: jsv_script_interface
section: 3
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

`jsv_is_param`(), `jsv_get_param`(), `jsv_add_param`(), `jsv_mod_param`(), `jsv_del_param`(), `jsv_sub_is_param`(), 
`jsv_sub_get_param`(), `jsv_sub_add_param`(), `jsv_sub_del_param`(), `jsv_is_env`(), `jsv_get_env`(), `jsv_add_env`(), 
`jsv_mod_env`(), `jsv_del_env`(), `jsv_accept`(), `jsv_correct`(), `jsv_reject`(), `jsv_reject_wait`(), 
`jsv_show_params`(), `jsv_show_envs`(), `jsv_log_info`(), `jsv_log_warning`(),
`jsv_log_error`(), `jsv_main`() - xxQS_NAMExx Job Submission Verifier Scripting Interface

# SYNOPSIS

During the communication with client and server components, the function `jsv_main`() triggers two callback functions 
for each job that should verify each incoming job in the xxQS_NAMExx system. First `jsv_on_start`() is called 
and later on `jsv_on_verify`().

`jsv_on_start`() can be used to initialize certain things that might be needed for the verification process. 
`jsv_on_verify`() does the verification process of the job itself.

The function `jsv_send_env`() can be called in `jsv_on_start`() so that the job environment is available for
the verification step in `jsv_on_verify`().

The following function can only be used in `jsv_on_verify`().  Simple job parameters can be accessed/modified with: 
`jsv_is_param`(), `jsv_get_param`(), `jsv_set_param`() and `jsv_del_param`().

List based job parameters can be accessed with: `jsv_sub_is_param`(), `jsv_sub_get_param`(), `jsv_sub_add_param`() 
and `jsv_sub_del_param`()

If the environment was requested with `jsv_send_env`() in `jsv_on_start`() then the environment can be 
accessed/modified with the following commands: `jsv_is_env`(), `jsv_get_env`(), `jsv_add_env`(), `jsv_mod_env`() 
and `jsv_del_env`().

Jobs can be accepted/rejected with the following: `jsv_accept`(), `jsv_correct`(), `jsv_reject`() and `jsv_reject_wait`()

The following functions send messages to the calling component of a JSV that will either appear on the stdout stream 
of the client or in the master message file. This is especially useful when new JSV scripts should be tested:
`jsv_show_params`(), `jsv_show_envs`(), `jsv_log_info`(), `jsv_log_warning`() and `jsv_log_error`()

When the verification of a jobs is done the `jsv_on_start`() has to call one and only one of following functions before
`jsv_on_start`() has to return: `jsv_accept`(), `jsv_correct`(), `jsv_reject`() or `jsv_reject_wait`()

The function `jsv_clear_params`() and `jsv_clear_envs`() can be used to clear data from a previous job verification.

# DESCRIPTION

The functions documented here implement the server side of the JSV protocol as it is described in the 
man page sge_jsv(1).

These script functions are available in Bourne shell and Perl scripts after sourcing/including the files 
*jsv\_include.sh* or *JSV.pm*. The files and corresponding JSV script templates are located in 
the directory *$xxQS_NAME_Sxx\_ROOT/util/resources/jsv*.

## jsv_clear_params()

This function clears all received job parameters that were stored during the last job verification process.

## jsv_clear_envs()

This function clears all received job environment variables that were stored during the last job verification process.

## jsv_show_params()

A call of this function reports all known job parameters to the counterpart of this script (client or master 
daemon thread). This parameters will be reported as info messages and appear either in the stdout stream of the 
client or in the message file of the master process.

## jsv_show_envs()

A call of this function reports all known job environment variables to the counterpart of this script (client or 
master daemon thread). They will be reported as info messages and appear in the stdout stream of the client or in the 
message file of the master process.

## jsv_is_param(*param_name*)

This function returns whether a particular job parameter is available for the job currently being
checked. Either a *true* or *false* string is returned. The availability/non-availability of a job 
parameter does not imply that the corresponding command line switch was used/not used.

The following values are allowed for *param_name*. Corresponding `qsub`/`qrsh`/`qsh`/... switches next to the 
parameter name are mentioned only if they are different from the command line switches. Job parameters written
in capital letters are pseudo parameters.

Find additional information in qsub(1) man page describing the availability and value format.

| param_name  | command line switch/description    |
|:------------|:-----------------------------------|
| a           |                                    |
| ac          | combination of -ac, -sc, -dc       |
| ar          |                                    |
| A           |                                    |
| b           |                                    |
| c           |                                    |
| ckpt        |                                    |
| cwd         |                                    |
| display     |                                    |
| dl          |                                    |
| e           |                                    |
| h           |                                    |
| hold_jid    |                                    |
| hold_jid_ad |                                    |
| i           |                                    |
| l_hard      | -l or -hard followed by -l         |
| l_soft      | -soft followed by -l               |
| j           |                                    |
| js          |                                    |
| m           |                                    |
| M           |                                    |
| masterq     |                                    |
| N           |                                    |
| notify      |                                    |
| now         |                                    |
| N           |                                    |
| o           |                                    |
| ot          |                                    |
| P           |                                    |
| pe          |                                    |
| q_hard      | -q or -hard followed by -q         |
| q_soft      | -soft followed by -q               |
| R           |                                    |
| r           |                                    |
| shell       |                                    |
| S           |                                    |
| t           |                                    |
| w           |                                    |
| wd          |                                    |
| CLIENT      |                                    |
| CONTEXT     |                                    |
| GROUP       |                                    |
| VERSION     |                                    |
| JOB_ID      |                                    |
| SCRIPT      |                                    |
| CMDARGS     |                                    |
| CMDARG<i>   | where <i> is a non-negative number |
| USER        |                                    |

The function returns the string *true* if the parameter (*param_name*) exists in the job currently being verified. 
If it does not exist *false* will be returned.

## jsv_get_param(*param_name*)

This function returns the value of a specific job parameter (*param_name*).

This value is only available if the function `jsv_is_param`() returns *true*. Otherwise, an empty string is returned.

Find a list of allowed parameter names in the section for the function `jsv_is_param`().

## jsv_set_param(*param_name*, *param_value*)

This function changes the job parameter *param_name* to the value *param_value.*

If *param_value* is an empty string then the corresponding job parameter will be deleted similar to the function 
`jsv_del_param`(). As a result the job parameter is not available as if the corresponding command line switch 
was not specified during job submission.

For boolean parameters that only accept the values *yes* or *no* it is not allowed to pass an empty string as 
*param_value*. Also for the parameters with the *param_name* *c* and *m* it is not allowed to use empty strings. 

## jsv_del_param(*param_name*)

This function deletes the job parameter *param_name.*

Find a list of allowed parameter names in the section for the function`jsv_is_param`().

## jsv_sub_is_param(*param_name*, *variable_name*)

Some job parameters are lists that can contain multiple variables with an optional value.

This function returns *true* if a job parameters list contains a variable and *false* otherwise. *false* might also 
indicate that the parameter list itself is not available. Use the function
`jsv_is_param`() to check if the parameter list is not available.

The following parameters are list parameters. The second columns describes corresponding variable names to be used. 
The third column contains a dash ('*-*') if there is no *variable_value* allowed when the functions* 
`jsv_sub_add_param`() or it indicated that `jsv_sub_get_param`() will return always an empty string. 
A question mark ('*?*') shows that the value is optional.

| param_name | variable_name                   | variable_value |
|:-----------|:--------------------------------|:---------------|
| ac         | job context variable name       | -              |
| hold_jid   | job identifier                  | -              |
| l_hard     | complex attribute name          | ?              |
| l_soft     | complex attribute name          | ?              |
| M          | mail address                    | -              |
| masterq    | cluster queue or qinstance name | -              |
| q_hard     | cluster queue or qinstance name | -              |
| q_soft     | cluster queue or qinstance_name | -              |

## jsv_sub_get_param(*param_name*, *variable_name*)

Some job parameters are lists that can contain multiple variables with an optional value.

This function returns the value of a variable (*variable_name*). For sub list elements that have no value an empty 
string will be returned.

Find a list of allowed parameter names (*param_name*) and variable names (*variable_name*) in the section for the 
function `jsv_sub_is_param`().

## jsv_sub_add_param(*param_name*, *variable_name*, *variable_value*)

Some job parameters are list that can contain multiple variables with an
optional value.

This function either adds a new variable with a new value or it modifies
the value if the variable is already in the list parameter.
*variable_value* is optional. In that case, the variable has no value.

Find a list of allowed parameter names (*param_name*) and variable
names (*variable_name*) in the section for the function
`jsv_sub_is_param`().

## jsv_sub_del_param(*param_name*, *variable_name*)

Some job parameters are lists which can contain multiple variables with an optional value.

This function deletes a variable (*variable_name*) and if available the corresponding value. If *variable_name* 
is not available in the job parameters then the command will be ignored.

Find a list of allowed parameter names (*param_name*) and * variable  names (*variable_name*) in the section for 
the function `jsv_sub_is_param`().

## jsv_is_env(*variable_name*)

If the function returns *true*, then the job environment variable with the name *variable_name* exists in the job 
currently being verified and `jsv_get_env`() can be used to retrieve the value of that variable. If the function 
returns *false*, then the job environment variable does not exist.

## jsv_get_env(*variable_name*)

This function returns the value of a job environment variable (*variable_name*).

This variable has to be passed with the `qsub` command line switch *-v* or *-V* and it has to be enabled that 
environment variable data is passed to JSV scripts. Environment variable data is passed when the function 
`jsv_send_env`() is called in the callback function `jsv_on_start`().

If the variable does not exist or if environment variable information is not available then an empty string will 
be returned.

## jsv_add_env(*variable_name*, *variable_value*)

This function adds an environment variable to the set of variables that will be exported to the job, when it is 
started. As a result *variable_name* and *variable_value* become available, as if the *-v*
or *-V* was specified during job submission.

*variable_value* is optional. If there is an empty string passed then the variable is defined without value.

If *variable_name* already exists in the set of job environment variables, then the corresponding value will be 
replaced by *variable_value*, as if the function `jsv_mod_env`() was used. If an empty string is passed then the old 
value will be deleted.

To delete an environment variable the function `jsv_del_env`() has to be used.

## jsv_mod_env(*variable_name*, *variable_value*)

This function modifies an existing environment variable that is in the set of variables which will exported to the job, 
when it is started. As a result, the *variable_name* and *variable_value* will be available as if  the -v or -V was 
specified during job submission.

*variable_value* is optional. If there is an empty string passed then the variable is defined without value.

If *variable_name* does not already exist in the set of job environment variables, then the corresponding name and 
value will be added as if the function `jsv_add_env`() was used.

To delete a environment variable, use the function `jsv_del_env`().

## jsv_del_env(*variable_name*)

This function removes a job environment variable (*variable_name*) from the set of variables that will be exported to 
the job, when it is started.

If *variable_name* does not already exist in the set of job environment variables then the command is ignored.

To change the value of a variable use the function `jsv_mod_env`() to add a new value, call the 
function `jsv_add_env`().

## jsv_accept(*message*)

This function can only be used in `jsv_on_verify`(). After it has been* called, the function `jsv_on_verify`() 
has to return immediately after a call of this function.

A call to this function indicates that the job that is currently being verified should be accepted as it was initially 
provided. All job modifications that might have been applied in `jsv_on_verify`() before this function was called, 
are then ignored.

Instead of calling `jsv_accept`() in `jsv_on_verify`() also the functions `jsv_correct`(), `jsv_reject`() or
`jsv_reject_wait`() can be called, but only one of these functions can be used at a time.

## jsv_correct(*message*)

This function can only be used in `jsv_on_verify`(). After it has been called, the function `jsv_on_verify`() has 
to return immediately. 

A call to this function indicates that the job that is currently being verified has to be modified before it can be 
accepted. All job parameter modifications that were previously applied will be committed and the job will be accepted. 
'Accept' in that case means that the job will either be passed to the next JSV instance for modification or that it is 
passed to that component in the master daemon that adds it to the master data store when the last JSV instance has 
verified the job.

Instead of calling `jsv_correct`() in `jsv_on_verify`(), the functions `jsv_accept`(), `jsv_reject`() or
`jsv_reject_wait`() can be called, but only one of these functions can be used.

## jsv_reject(*message*)

This function can only be used in `jsv_on_verify`(). After it has been called the function `jsv_on_verify`() has to 
return immediately. 

The job that is currently being verified will be rejected. *message* will be passed to the client application that 
tried to submit the job. Command line clients like `qsub` will print that *message* to stdout to inform the user that 
the submission has failed.

`jsv_reject_wait`() should be called if the user may try to submit the job again. `jsv_reject_wait`() indicates that 
the verification process might be successful in the future.

Instead of calling `jsv_reject`() in `jsv_on_verify`() also the functions `jsv_accept`(), `jsv_correct`() or
`jsv_reject_wait`() can* be also called, but only one of these functions can be used.

## jsv_reject_wait(*message*)

This function can only be used in `jsv_on_verify`(). After it has been called the function `jsv_on_verify`() has 
to return immediately. 

The job which is currently verified will be rejected. *message* will be passed to the client application, that 
tries to submit the job. Command line clients like `qsub` will print that *message* to stdout to inform the user that 
the submission has failed.

This function should be called if the user who tries to submit the job might have a chance to submit the job later. 
`jsv_reject`() indicates that the verified job will also be rejected in the future.

Instead of calling `jsv_reject_wait`() in `jsv_on_verify`() the functions `jsv_accept`(), `jsv_correct`() 
or `jsv_reject`() can be also called, but only one of these functions can be used.

## jsv_log_info(*message*)

This function sends an info *message* to the client or master daemon instance that started the JSV script.

For client JSVs, this means that the command line client will get the information and print it to the stdout stream. 
Server JSVs will print that *message* as an info message to the master daemon message file.

If *message* is missing then and empty line will be printed.

## jsv_log_error(*message*)

This function sends an error *message* to the client or master daemon instance that started the JSV script.

For client JSVs, this means that the command line client will get the information and print it to the stdout stream.
Server JSVs will print that *message* as an error message to the master daemon message file.

If *message* is missing then and empty line will be printed.

## jsv_log_warning(*message*)

This function sends a warning *message* to the client or master daemon instance that started the JSV script.

For client JSVs, this means that the command line client will get the information and print it to the stdout stream. 
Server JSVs will print that *message* as a warning message to the master daemon message file.

If *message* is missing then and empty line will be printed.

## jsv_send_env()

This function can only be used in `jsv_on_start`(). If it is used there, then the job environment information will be 
available in `jsv_on_verify`() for the next job that is scheduled to be verified.

This function must be called for the functions `jsv_show_envs`(), `jsv_is_env`(), `jsv_get_env`(), `jsv_add_env`() and
`jsv_mod_env`() to behave correctly.

Job environments might become very big (10K and more). This will slow down the executing component (submit client 
or master daemon thread). For this reason, job environment information is not passed to JSV scripts by default.

Please note also that the data in the job environment can't be verified by xxQS_NAMExx and might therefore contain 
data which could be misinterpreted in the script environment and cause security issues.

## jsv_main()

This function has to be called as main function in JSV scripts. It implements the JSV protocol and performs the 
communication with client and server components which might start JSV scripts.

This function does not return immediately. It returns only when the "*QUIT*" command is send by the client or 
server component.

During the communication with client and server components, this function triggers two callback functions for each 
job that should be verified. First `jsv_on_start`() and later on `jsv_on_verify`(). 

## jsv_on_start()

This is a callback function that has to be defined by the creator of a JSV script. It is called for every job short 
time before the verification process of a job starts.

Within this function `jsv_send_env`() can be called to request job environment information for the next job is 
scheduled to be verified.

After this function returns `jsv_on_verify`() will be called. This function does there verification process itself.

## jsv_on_verify()

This is a callback function that has to be defined by the creator of a JSV script. It is called for every job and 
when it returns then the job will either be accepted or rejected. Find implementation examples in the directory 
$SGE_ROOT/util/resources/jsv.

The logic of this function completely depends on the creator of this function. The creator has only to take care that 
one of the functions `jsv_accept`(), `jsv_reject`(), `jsv_reject_wait`() or `jsv_correct`() is called before the 
function returns.

# EXAMPLES

Find in the table below the returned values for the "...is..." and "...get..." functions when following 
job is submitted: 

    qsub -l mem=1G,mem2=200M ...

| function call                        | returned value     |
|:-------------------------------------|:-------------------|
| `jsv_is_param`(*l_hard*)             | "true"             |
| `jsv_get_param`(*l_hard*)            | "mem=1G,mem2=200M" |
| `jsv_sub_is_param`(*l_hard*,*mem*)   | "true"             |
| `jsv_sub_get_param`(*l_hard*,*mem*)  | "1G"               |
| `jsv_sub_get_param`(*l_hard*,*mem3*) | "false"            |
| `jsv_sub_get_param`(*l_hard*,*mem3*) | ""                 |

# SEE ALSO

xxqs_name_sxx_intro(1), qsub(1), sge_jsv(1)

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
