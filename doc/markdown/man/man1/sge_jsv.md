---
title: sge_jsv
section: 1
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

JSV - xxQS_NAMExx Job Submission Verifier

# DESCRIPTION

JSV is an abbreviation for Job Submission Verifier. A JSV is a script or binary that can be used to verify, modify or 
reject a job during the time of job submission.

JSVs will be triggered by submit clients like `qsub`, `qrsh` and `qsh` on submit hosts (Client JSV) or they verify 
incoming jobs on the master host (Server JSV) or both.

# CONFIGURATION

JSVs can be configured on various locations. Either a *jsv_url* can be provided with the `-jsv` submit parameter 
during job submission, a corresponding switch can be added to one of the *sge_request* files or a *jsv_url* can 
be configured in the global cluster configuration of the xxQS_NAMExx installation.

All defined JSV instances will be executed in following order:

1) qsub -jsv ... 
2) $cwd/.sge_request
3) $HOME/.sge_request 
4) $SGE_ROOT/$SGE_CELL/common/sge_request
5) Global configuration

The Client JSVs (1-3) can be defined by xxQS_NAMExx end users whereas the client JSV defined in the global 
sge_request file (4) and the server JSV (5) can only be defined by the xxQS_NAMExx administrators.

Due to the fact that (4) and (5) are defined and configured by xxQS_NAMExx administrators and because they are 
executed as last JSV instances in the sequence of JSV scripts, an administrator has an additional way to define 
certain policies for a cluster.

As soon as one JSV instance rejects a job the whole process of verification is stopped and the end user will get 
a corresponding error message that the submission of the job has failed.

If a JSV accepts a job or accepts a job after it applied several modifications then the following JSV instance will 
get the job parameters including all modifications as input for the verification process. This is done as long as 
either the job is accepted or rejected.

Find more information how to use Client JSVs in qsub(1) and for Server JSVs in sge_conf(5).

# LIFETIME

A Client or Server JSV is started as own UNIX process. This process communicates either with a xxQS_NAMExx client 
process or the master daemon by exchanging commands, job parameters and other data via *stdin*/*stdout* channels.

Client JSV instances are started by client applications before a job is sent to qmaster. This instance does the job 
verification for the job to be submitted. After that verification the JSV process is stopped.

Server JSV instances are started for each worker thread part of the qmaster process (for version 6.2 of xxQS_NAMExx 
this means that two processes are started). Each of those processes have to verify job parameters for multiple jobs 
as long as the master is running, the underlying JSV configuration is not changed and no error occurs.

# TIMEOUT

The timeout is a modifiable value that will measure the response time of either the client or server JSV. In the 
event that the response time of the JSV is longer than timeout value specified, this will result in the JSV being 
re-started. The server JSV timeout value is specified through the qmaster parameter jsv_timeout. The client JSV 
timeout value is set through the environment variable *SGE_JSV_TIMEOUT*. The default value is 10 seconds, and this 
value must be greater than 0. If the timeout has been reach, the JSV will only try to re-start once, if the timeout is
reached again an error will occur.

# THRESHOLD

The threshold value is defined as a qmaster parameter jsv_threshold. This value measures the time for a server job 
verification. If this time exceeds the defined threshold then additional logging will appear in the master message 
file at the INFO level. This value is specified in milliseconds and has a default value of 5000. If a value of 0 is 
defined then this means all jobs will be logged in the message file.

# PROTOCOL

After a JSV script or binary is started it will get commands through its *stdin* stream, and it has to respond with 
certain commands on the *stdout* stream. Data which is send via the **stderr** stream of a JSV instance is ignored. 
Each command which is sent to/by a JSV script has to be terminated by a new line character ('\\n') whereas new line
characters are not allowed in the whole command string itself.

In general commands which are exchanged between a JSV and client/qmaster have the following format. Commands and 
arguments are case sensitive. Find the EBNF command description below.

    command := command_name ' ' { argument ' ' } ;

A *command* starts with a *command_name* followed by a space character and a space separated list of *arguments*.

# PROTOCOL (JSV side)

Following *commands* have to be implemented by an JSV script so that it conforms to version 1.0 of the JSV protocol 
which was first implemented in xxQS_NAMExx 6.2u2:

    begin_command := 'BEGIN' ;  

After a JSV instance has received all *env_commands* and *param_commands* of a job which should be verified, 
the client/qmaster will trigger the verification process by sending one *begin_command*. After that it will wait for 
*param_commands* and *env_commands* which are sent back from the JSV instance to modify the job specification. 
As part of the verification process a JSV script or binary has to use the *result_command* to indicate that the
verification process is finished for a job.

    env_command := ENV ' ' modifier ' ' name ' ' value ;  
    modifier := 'ADD' \| 'MOD' \| 'DEL' ;

The *env_command* is an optional command which has only to be implemented by a JSV instance if the *send_data_command* 
is sent by this JSV before the *started_command* was sent. Only in that case the client or master will use one or 
multiple *env_commands* to pass the environment variables (*name* and *value*) to the JSV instance which would be 
exported to the job environment when the job would be started. Client and qmaster will only send *env_commands* with 
the modifier *ADD*.

JSV instances modify the set of environment variables by sending back *env_commands* and by using the *modifiers* 
*ADD*, *MOD* and *DEL*.

    param_command := 'PARAM' ' ' param_parameter ' ' value ;
    param_parameter := submit_parameter \| pseudo_parameter ;
    submit_parameter := 'b' \| 'masterq' \| ... ;
    pseudo_parameter := 'CLIENT' \| 'CMDARGS' \| ... ;

The *param_command* has two additional arguments which are separated by space characters. The first argument is either 
a *submit_parameter* as it is specified in qsub(1) or it is a *pseudo_parameters* as documented below. The second 
parameter is the *value* of the corresponding *param_parameter*.

Multiple *param_commands* will be sent to a JSV instance after the JSV has sent a *started_command*. The sum of all 
*param_commands* which is sent represents a job specification of that job which should be verified.

*submit_parameters* are for example *b* (similar to the `qsub -b` switch) or *master_q_hard* 
(similar to `qsub -scope master -hard -q` switch). Find a complete list of *submit_parameters* in the qsub(1) page. 
Please note that not in all cases the *param_parameter* name and the corresponding *value* format is equivalent with 
the `qsub` switch name and its argument format. E.g. the `qsub -pe` parameters will be available as a set of parameters 
with the name *pe_name*, *pe_min*, *pe_max* or the switch combination `-soft -l` (default `-scope global`) will be 
passed to JSV scripts as *global_l_soft* parameter. For details concerning this differences consult also the qsub(1) 
man page.

    start_command := 'START' ;  

The *start_command* has no additional arguments. This command indicates that a new job verification should be started. 
It is the first command which will be sent to JSV script after it has been started, and it will initiate each new 
job verification. A JSV instance might trash cached values which are still stored due to a previous job verification.
The application which send the *start_command* will wait for a *started_command* before it continues.

    quit_command := 'QUIT' ;  

The *quit_command* has no additional arguments. If this command is sent to a JSV instance then it should terminate 
itself immediately.

# PROTOCOL (client/qmaster side)

A JSV script or binary can send a set of commands to a client/qmaster process to indicate its state in the 
communication process, to change the job specification of a job which should be verified and to report
messages or errors. Below you can find the commands which are understood by the client/qmaster which will implement 
version 1.0 of the communication protocol which was first implemented in xxQS_NAMExx 6.2u2:

    error_command := 'ERROR' message ;  

Any time a JSV script encounters an error it might report it to the client/qmaster. If the error happens during a 
job verification the job which is currently verified will be rejected. The JSV binary or script will also be 
restarted before it gets a new verification task.

    log_command := 'LOG' log_level ;
    log_level := 'INFO' \| 'WARNING' \| 'ERROR'  

*log_commands* can be used whenever the client or qmaster expects input from a JSV instance. This command can be 
used in client JSVs to send information to the user submitting the job. In client JSVs all messages, independent of 
the *log_level* will be printed to the *stdout* stream of the used submit client. If a server JSV receives a 
*log_command* it will add the received message to the message file respecting the specified *log_level*. 
Please note that *message* might contain spaces but no new line characters.

    param_command (find definition above)  

By sending *param_commands* a JSV script can change the job specification of the job which should be verified. If a 
JSV instance later on sends a **result_command** which indicates that a JSV instance should be accepted with 
correction then the values provided with these *param_commands* will be used to modify the job before it is accepted
by the xxQS_NAMExx system.

    result_command := 'RESULT' result_type \[ message \] ;  
    result_type := 'ACCEPT' \| 'CORRECT' \| 'REJECT' \| 'REJECT_WAIT' ;

After the verification of a job is done a JSV script or binary has to send a *result_command* which indicates what 
should happen with the job. If the *result_type* is *ACCEPTED* the job will be accepted as it was initially 
submitted by the end user. All *param_commands* and *env_commands* which might have been sent before the 
*result_command* are ignored in this case. The *result_type* *CORRECT* indicates that the job should be accepted 
after all modifications sent via *param_commands* and *env_commands* are applied to the job. *REJECT* and 
*REJECT_WAIT* cause the client or qmaster instance to reject the job.

    send_data_command := 'SEND' data_name ;  
    data_name := 'ENV'; 

If a client/qmaster receives a *send_env_command* from a JSV instance before a *started_command* is sent, then it 
will not only pass job parameters with **param_commands** but also *env_commands* which provide the JSV with the 
information which environment variables would be exported to the job environment if the job is accepted and started
later on.

The job environment is not passed to JSV instances as default because the job environment of the end user might 
contain data which might be interpreted wrong in the JSV context and might therefore cause errors or security issues.

    started_command := 'STARTED' ; 

By sending the *started_command* a JSV instance indicates that it is ready to receive *param_commands* and 
*env_commands* for a new job verification. It will only receive *env_commands* if it sends a *send_data_command* 
before the *started_command*.

# PSEUDO PARAMETERS

CLIENT  
The corresponding value for the *CLIENT* parameter is either *qmaster* or the name of a submit client like `qsub`, 
`qsh`, `qrsh`, `qlogin` and so on. This parameter value can't be changed by JSV instances. It will always be sent 
as part of a job verification.

CMDARGS  
Number of arguments which will be passed to the job script or command when the job execution is started. It will 
always be sent as part of a job verification. If no arguments should be passed to the job script or
command it will have the value 0. This parameter can be changed by JSV instances. If the value of *CMDARGS* is bigger 
than the number of available *CMDARG\<id>* parameters then the missing parameters will be automatically passed as 
empty parameters to the job script.

CMDNAME  
Either the path to the script or the command name in case of binary submission. It will always be sent as part of 
a job verification.

CONTEXT  
Either *client* if the JSV which receives this *param_command* was started by a command-line client like `qsub`, 
`qsh`, ... or *master* if it was started by the qmaster process. It will always be sent as part of a job verification. 
Changing the value of this parameters is not possible within JSV instances.

GROUP  
Defines Primary group of the user which tries to submit the job which should be verified. This parameter cannot be
changed but is always sent as part of the verification process. The username is passed as parameters with the 
name *USER*.

JOB_ID  
Not available in the client context (see *CONTEXT*). Otherwise, it contains the job number of the job which will be 
submitted when the verification process is successful. *JOB_ID* is an optional parameter which can't be changed by 
JSV instances.

USER  
Username of the user which tries to submit the job which should be verified. Cannot be changed but is always sent as 
part of the verification process. The group name is passed as parameter with the name *GROUP*.

VERSION  
*VERSION* will always be sent as part of a job verification process and it will always be the first parameter 
which is sent. It will contain a version number of the format \<major>.\<minor>. In version 6.2u2 and
higher the value will be '**1.0**'. The value of this parameter can't be changed.

# EXAMPLE

Here is an example for the communication of a client with a JSV instance when following job is submitted:

    qsub -pe p 3 -hard -l a=1,b=5 -soft -l q=all.q $SGE_ROOT/examples/jobs/sleeper.sh

Data in the first column is sent from the client/qmaster to the JSV instance. That data contained in the second 
column is sent from the JSV script to the client/qmaster. New line characters which terminate each line in the 
communication protocol are omitted.

       START
                               SEND ENV
                               STARTED
       PARAM VERSION 1.0
       PARAM CONTEXT client
       PARAM CLIENT qsub
       PARAM USER ernst
       PARAM GROUP staff
       PARAM CMDNAME /sge_root/examples/jobs/sleeper.sh
       PARAM CMDARGS 1
       PARAM CMDARG0 12 
       PARAM global_l_hard a=1,b=5
       PARAM global_l_soft q=all.q
       PARAM M user@hostname
       PARAM N Sleeper
       PARAM o /dev/null
       PARAM pe_name pe1
       PARAM pe_min 3
       PARAM pe_max 3
       PARAM S /bin/sh
       BEGIN
                               RESULT STATE ACCEPT 

# SEE ALSO

xxqs_name_sxx_intro(1), qalter(1), qlogin(1), qmake(1), qrsh(1), qsh(1), qsub(1),

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
