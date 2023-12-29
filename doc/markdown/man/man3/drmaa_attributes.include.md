# NAME

drmaa_get_attribute_names, drmaa_get_vector_attribute_names,
drmaa_get_next_attr_name, drmaa_get_num_attr_names,
drmaa_release_attr_names - DRMAA job template attributes

# SYNOPSIS

**#include "drmaa.h"**

    int drmaa_get_attribute_names(
    drmaa_attr_names_t **values,
    char *error_diagnosis,
    size_t error_diag_len

);

    int drmaa_get_vector_attribute_names(
    drmaa_attr_names_t **values,
    char *error_diagnosis,
    size_t error_diag_len

);

    int drmaa_get_next_attr_name(
    drmaa_attr_names_t* values,
    char *value,
    int value_len

);

    int drmaa_get_next_attr_value(
    drmaa_attr_values_t* values,
    char *value,
    int value_len

);

    int drmaa_get_num_attr_names(
    drmaa_attr_names_t* values,
    int *size

);

    void drmaa_release_attr_names(
    drmaa_attr_names_t* values

);

# DESCRIPTION

The drmaa_get_attribute_names() function returns into *values*** a DRMAA
names** string vector containing the set of supported non-vector DRMAA
job template attribute names. The set includes supported DRMAA reserved
attribute names and xxQS_NAMExx native attribute names. The names in the
names string vector can be extracted using
*drmaa_get_next_attr_name* (3). The number of names in the names string
vector can be determined using *drmaa_get_num_attr_names* (3). Note that
this function is only available in the 1.0 implementation. The caller is
responsible for releasing the names string vector returned into
*values*** using ** *drmaa_release_attr_names* (3). Use
*drmaa_set_attribute* (3) and *drmaa_get_attribute* (3) for setting and
inspecting non-vector attributes.

## drmaa_get_vector_attribute_names()

The drmaa_get_vector_attribute_names() function returns into *values***
a DRMAA names** string vector containing the set of supported vector
DRMAA job template attribute names. The set includes supported DRMAA
reserved attribute names and xxQS_NAMExx native attribute names. The
names in the names string vector can be extracted using
*drmaa_get_next_attr_name* (3). The caller is responsible for releasing
the names string vector returned into *values*** using **
*drmaa_release_attr_names* (3). Use *drmaa_set_vector_attribute* (3) and
*drmaa_get_vector_attribute* (3) for setting and inspecting vector
attributes.

## drmaa_get_next_attr_name()

Each time drmaa_get_next_attr_name() is called it returns into the
buffer, *value***, up to ***value_len*** ** bytes of the next entry
stored in the DRMAA names string vector, *values***.** Once the names
list has been exhausted, DRMAA_ERRNO_NO_MORE_ELEMENTS is returned.

## drmaa_get_num_attr_names()

The drmaa_get_num_attr_names() returns into *size*** the number of
entries** in the DRMAA names string vector. This function is only
available in the 1.0 implementation.

## drmaa_release_attr_names()

The drmaa_release_attr_names() function releases all resources
associated with the DRMAA names string vector, *values***.**

## Attribute Priorities

DRMAA job template attributes can be set from six different sources. In
order of precedence, from lowest to highest, these are: options set by
DRMAA automatically by default, options set in the *sge_request* (5)
file(s), options set in the script file, options set by the
drmaa_job_category attribute, options set by the
drmaa_native_specification attribute, and options set through other
DRMAA attributes.

By default DRMAA sets four options for all jobs. These are "-p 0", "-b
yes", "-shell no", and "-w e". This means that by default, all jobs will
have priority 0, all jobs will be treated as binary, i.e. no scripts
args will be parsed, all jobs will be executed without a wrapper shell,
and jobs which are unschedulable will cause a submit error.

The *sge_request* (5) file, found in the
$xxQS_NAME_Sxx_ROOT/$xxQS_NAME_Sxx_CELL/common directory, may contain
options to be applied to all jobs. The .sge_request file found in the
user's home directory or the current working directory may also contain
options to be applied to certain jobs. See *sge_request* (5) for more
information.

If the *sge_request* (5) file contains "-b no" or if the
drmaa_native_specification attribute is set and contains "-b no", the
script file will be parsed for in-line arguments. Otherwise, no scripts
args will be interpreted. See *qsub* (1) for more information.

If the drmaa_job_category attribute is set, and the category it points
to exists in one of the *qtask* (5) files, the options associated with
that category will be applied to the job template. See *qtask* (5) and
the drmaa_job_category attribute below for more information.

If the drmaa_native_specification attribute is set, all options
contained therein will be applied to the job template. See the
drmaa_native_specification below for more information.

Other DRMAA attributes will override any previous settings. For example,
if the sge_request file contains "-j y", but the drmaa_join_files
attribute is set to "n", the ultimate result is that the input and
output files will remain separate.

For various reasons, some options are silently ignored by DRMAA. Setting
any of these options will have no effect. The ignored options are: -cwd,
-help, -sync, -t, -verify, -w w, and -w v. The -cwd option can be
re-enabled by setting the environment variable, SGE_DRMAA_ALLOW_CWD.
However, the -cwd option is not thread safe and should not be used in a
multi-threaded context.

## Attribute Correlations

The following DRMAA attributes correspond to the following *qsub* (1)
options:

>     DRMAA Attribute                  qsub Option
>     -------------------------------------------------------
>     drmaa_remote_command             script file
>     drmaa_v_argv                     script file args
>     drmaa_js_state = "drmaa_hold"    -h
>     drmaa_v_env                      -v
>     drmaa_wd = $PWD                  -cwd
>     drmaa_job_category               (qtsch qtask)*
>     drmaa_native_specification       ALL*
>     drmaa_v_email                    -M
>     drmaa_block_email = "1"          -m n
>     drmaa_start_time                 -a
>     drmaa_job_name                   -N
>     drmaa_input_path                 -i
>     drmaa_output_path                -o
>     drmaa_error_path                 -e
>     drmaa_join_files                 -j
>     drmaa_transfer_files             (prolog and epilog)*
>
> \* See the individual attribute description below

# DRMAA JOB TEMPLATE ATTRIBUTES

## drmaa_remote_command - "\<remote_command>"

Specifies the remote command to execute. The *remote_command*** must be
the path of an ** executable that is available at the job's execution
host. If the path is relative, it is assumed to be relative to the
working directory, usually set through the drmaa_wd attribute. If
working directory is not set, the path is assumed to be relative to the
user's home directory.

The file pointed to by remote_command may either be an executable binary
or an executable script. If a script, it must include the path to the
shell in a #! line at the beginning of the script. By default, the
remote command will be executed directly, as by *exec* (2). To have the
remote command executed in a shell, such as to preserve environment
settings, use the drmaa_native_specification attribute to include the
"-shell yes" option. Jobs which are executed by a wrapper shell fail
differently from jobs which are executed directly. When a job which
contains a user error, such as an invalid path to the executable, is
executed by a wrapper shell, the job will execute successfully, but exit
with a return code of 1. When a job which contains such an error is
executed directly, it will enter the DRMAA_PS_FAILED state upon
execution.

## drmaa_js_state - "{drmaa_hold\|drmaa_active}"

Specifies the job state at submission. The string values 'drmaa_hold'
and 'drmaa_active' are supported. When 'drmaa_active' is used the job is
submitted in a runnable state. When 'drmaa_hold' is used the job is
submitted in user hold state (either DRMAA_PS_USER_ON_HOLD or
DRMAA_PS_USER_SYSTEM_ON_HOLD). This attribute is largely equivalent to
the *qsub* (1) submit option '-h'.

## drmaa_wd - "\<directory_name>"

Specifies the directory name where the job will be executed. A
'$drmaa_hd_ph$' placeholder at the beginning of the *directory_name***
** denotes the remaining string portion as a relative directory name
that is resolved relative to the job user's home directory at the
execution host. When the DRMAA job template is used for bulk job
submission (see also *drmaa_run_bulk_job* (3)) the '$drmaa_incr_ph$'
placeholder can be used at any position within *directory_name*** ** to
cause a substitution with the parametric job's index. The
*directory_name*** must be specified in a syntax that is common at the
host ** where the job is executed. If set to a relative path and no
placeholder is used, a path relative to the user's home directory is
assumed. If not set, the working directory will default to the user's
home directory. If set and the given directory does not exist the job
will enter the DRMAA_PS_FAILED state when run.

Note that the working directory path is the path on the execution host.
If binary mode is disabled, an attempt to find the job script will be
made, relative to the working directory path. That means that the path
to the script must be the same on both the submission and execution
hosts.

## drmaa_job_name - "\<job_name>"

Specifies the job's name. Setting the job name is equivalent to use of
*qsub* (1) submit option '-N' with *job_name*** as option argument. **

## drmaa_input_path - "\[\<hostname>\]:\<file_path>"

Specifies the standard input of the job. Unless set elsewhere, if not
explicitly set in the job template, the job is started with an empty
input stream. If the standard input is set it specifies the network path
of the job's input stream file.

When the 'drmaa_transfer_files' job template attribute is supported and
contains the character 'i', the input file will be fetched by
xxQS_NAMExx from the specified host or from the submit host if no
*hostname*** is specified. When the 'drmaa_transfer_files' job template
attribute is not ** supported or does not contain the character 'i', the
input file is always expected at the host where the job is executed
regardless of any *hostname*** specified. **

If the DRMAA job template will be used for bulk job submission, (See
also *drmaa_run_bulk_job* (3)) the '$drmaa_incr_ph$' placeholder can be
used at any position within *file_path*** to cause a substitution with
the parametric job's index. A '$drmaa_hd_ph$' ** placeholder at the
beginning of *file_path*** denotes the remaining portion of the **
*file_path*** as a relative file specification resolved relative to the
job user's home directory ** at the host where the file is located. A
'$drmaa_wd_ph$' placeholder at the beginning of *file_path* denotes the
remaining portion of the *file_path*** as a relative file specification
resolved relative ** to the job's working directory at the host where
the file is located. The *file_path*** must be specified ** in a syntax
that is common at the host where the file is located. If set and the
file can't be read the job enters the state DRMAA_PS_FAILED.

## drmaa_output_path - "\[\<hostname>\]:\<file_path>"

Specifies the standard output of the job. If not explicitly set in the
job template, the whereabouts of the job's output stream is not defined.
If set, this attribute specifies the network path of the job's output
stream file.

When the 'drmaa_transfer_files' job template attribute is supported and
contains the character 'o', the output file will be transferred by
xxQS_NAMExx to the specified host or to the submit host if no
*hostname*** is specified. When the 'drmaa_transfer_files' job template
attribute is not supported or ** does not contain the character 'o', the
output file is always kept at the host where the job is executed
regardless of any *hostname*** specified. **

If the DRMAA job template will be used for of bulk job submission (see
also *drmaa_run_bulk_job* (3)) the '$drmaa_incr_ph$' placeholder can be
used at any position within the *file_path* to cause a substitution with
the parametric job's index. A '$drmaa_hd_ph$' placeholder at the
beginning of *file_path*** denotes the remaining portion of the
***file_path*** as a relative file specification ** resolved relative to
the job user's home directory at the host where the file is located. A
'$drmaa_wd_ph$' placeholder at the beginning of the *file_path***
denotes the remaining portion of ***file_path*** as a ** relative file
specification resolved relative to the job's working directory at the
host where the file is located. The *file_path*** must be specified in a
syntax that is common at the host where the file ** is located. If set
and the file can't be written before execution the job enters the state
DRMAA_PS_FAILED.

## drmaa_error_path - "\[\<hostname>\]:\<file_path>"

Specifies the standard error of the job. If not explicitly set in the
job template, the whereabouts of the job's error stream is not defined.
If set, this attribute specifies the network path of the job's error
stream file.

When the 'drmaa_transfer_files' job template attribute is supported and
contains the character 'e', the output file will be transferred by
xxQS_NAMExx to the specified host or to the submit host if no
*hostname*** is specified. When the 'drmaa_transfer_files' job template
attribute is not supported ** or does not contain the character 'e', the
error file is always kept at the host where the job is executed
regardless of any *hostname*** specified. **

If the DRMAA job template will be used for of bulk job submission (see
also *drmaa_run_bulk_job* (3)) the '$drmaa_incr_ph$' placeholder can be
used at any position within the *file_path* to cause a substitution with
the parametric job's index. A '$drmaa_hd_ph$' placeholder at the
beginning of the *file_path*** denotes the remaining portion of the
***file_path*** as a** relative file specification resolved relative to
the job user's home directory at the host where the file is located. A
'$drmaa_wd_ph$' placeholder at the beginning of the *file_path***
denotes the remaining portion of the ***file_path*** as a** relative
file specification resolved relative to the job's working directory at
the host where the file is located. The *file_path*** must be specified
in a** syntax that is common at the host where the file is located. If
set and the file can't be written before execution the job enters the
state DRMAA_PS_FAILED. The attribute name is drmaa_error_path.

## drmaa_join_files - "{y\|n}"

Specifies if the job's error stream should be intermixed with the output
stream. If not explicitly set in the job template the attribute defaults
to 'n'. Either 'y' or 'n' can be specified. If 'y' is specified
xxQS_NAMExx will ignore the value of the 'drmaa_error_path' job template
attribute and intermix the standard error stream with the standard
output stream as specified with 'drmaa_output_path'.

## drmaa_v\_argv - "argv1 argv2 ..."

Specifies the arguments to the job.

## drmaa_job_category - "\<category>"

Specifies the DRMAA job category. The *category*** string is used ** by
xxQS_NAMExx as a reference into the *qtask* (5) file. Certain *qsub* (1)
options used in the referenced qtask file line are applied to the job
template before submission to allow site-specific resolving of resources
and/or policies. The cluster qtask file, the local qtask file, and the
user qtask file are searched. Job settings resulting from job template
category are overridden by settings resulting from the job template
drmaa_native_specification attribute as well as by explicit DRMAA job
template settings.

In order to avoid collisions with command names in the qtask files, it
is recommended that DRMAA job category names take the form:
\<category_name>.cat.

The options -help, -sync, -t, -verify, and -w w\|v are ignored. The -cwd
option is ignored unless the $SGE_DRMAA_ALLOW_CWD environment variable
is set.

## drmaa_native_specification - "\<native_specification>"

Specifies xxQS_NAMExx native *qsub* (1) options which will be
interpreted as part of the DRMAA job template. All options available to
*qsub* (1) command may be used in the *native_specification***, except
for -help, -sync,** -t, -verify, and -w w\|v. The -cwd option may only
be used if the SGE_DRMAA_ALLOW_CWD environment variable is set. This is
because the current parsing algorithm for -cwd is not thread-safe.
Options set in the *native* specification** will be overridden by the
corresponding DRMAA attributes. See** *qsub* (1) for more information on
qsub options.

## drmaa_v\_env - "\<name1>=\<value1> \<name2>=\<value2> ...

Specifies the job environment. Each environment *value*** defines the
remote ** environment. The *value*** overrides the remote environment
values if there ** is a collision.

## drmaa_v\_email - "\<email1> \<email2> ...

Specifies e-mail addresses that are used to report the job completion
and status.

## drmaa_block_email - "{0\|1}"

Specifies whether e-mail sending shall blocked or not. By default email
is not sent. If, however, a setting in a cluster or user settings file
or the

email in association with job events, the 'drmaa_block_email' attribute
will override that setting, causing no email to be sent.

## drmaa_start_time - "\[\[\[\[CC\]YY/\]MM/\]DD\] hh:mm\[:ss\] \[{-\|+}UU:uu\]"

Specifies the earliest time when the job may be eligible to be run where

    CC is the first two digits of the year (century-1) 
    YY is the last two digits of the year 
    MM is the two digits of the month [01,12] 
    DD is the two digit day of the month [01,31] 
    hh is the two digit hour of the day [00,23] 
    mm is the two digit minute of the day [00,59] 
    ss is the two digit second of the minute [00,61] 
    UU is the two digit hours since (before) UTC 
    uu is the two digit minutes since (before) UTC 

If the optional UTC-offset is not specified, the offset associated with
the local timezone will be used. If the day (DD) is not specified, the
current day will be used unless the specified hour:mm:ss has already
elapsed, in which case the next day will be used. Similarly for month
(MM), year (YY), and century-1 (CC). Example: The time: Sep 3 4:47:27 PM
PDT 2002, could be represented as: 2002/09/03 16:47:27 -07:00.

## drmaa_transfer_files - "\[i\]\[o\]\[e\]"

Specifies, which of the standard I/O files (stdin, stdout and stderr)
are to be transferred to/from the execution host. If not set, defaults
to "". Any combination of 'e', 'i' and 'o' may be specified. See
drmaa_input_path, drmaa_output_path and drmaa_error_path for information
about how to specify the standard input file, standard output file and
standard error file. The file transfer mechanism itself must be
configured by the administrator (see *sge_conf* (5) ). When it is
configured, the administrator has to enable drmaa_transfer_files. If it
is not configured, "drmaa_transfer_files" is not enabled and can't be
used.

# ENVIRONMENTAL VARIABLES

SGE_DRMAA_ALLOW_CWD  
Enables the parsing of the -cwd option from the sge_request file(s), job
category, and/or the native specification attribute. This option is
disabled by default because the algorithm for parsing the -cwd option is
not thread-safe.

xxQS_NAME_Sxx_ROOT  
Specifies the location of the xxQS_NAMExx standard configuration files.

xxQS_NAME_Sxx_CELL  
If set, specifies the default xxQS_NAMExx cell to be used. To address a
xxQS_NAMExx cell xxQS_NAMExx uses (in the order of precedence):

> The name of the cell specified in the environment variable
> xxQS_NAME_Sxx_CELL, if it is set.
>
> The name of the default cell, i.e. **default.**

xxQS_NAME_Sxx_DEBUG_LEVEL  
If set, specifies that debug information should be written to stderr. In
addition the level of detail in which debug information is generated is
defined.

xxQS_NAME_Sxx_QMASTER_PORT  
If set, specifies the tcp port on which *xxqs_name_sxx_qmaster* (8) is
expected to listen for communication requests. Most installations will
use a services map entry instead to define that port.

# RETURN VALUES

Upon successful completion, drmaa_get_attribute_names(),
drmaa_get_vector_attribute_names(), and drmaa_get_next_attr_name()
return DRMAA_ERRNO_SUCCESS. Other values indicate an error. Up to
*error_diag_len*** characters of error related diagnosis ** information
is then provided in the buffer *error_diagnosis***.**

# ERRORS

The drmaa_get_attribute_names(), drmaa_get_vector_attribute_names(), and
drmaa_get_next_attr_name() functions will fail if:

## DRMAA_ERRNO_INTERNAL_ERROR

Unexpected or internal DRMAA error, like system call failure, etc.

## DRMAA_ERRNO_DRM_COMMUNICATION_FAILURE

Could not contact DRM system for this request.

## DRMAA_ERRNO_AUTH_FAILURE

The specified request is not processed successfully due to authorization
failure.

## DRMAA_ERRNO_INVALID_ARGUMENT

The input value for an argument is invalid.

## DRMAA_ERRNO_NO_ACTIVE_SESSION

Failed because there is no active session.

## DRMAA_ERRNO_NO_MEMORY

Failed allocating memory.

The drmaa_get_next_attr_name() will fail if:

## DRMAA_ERRNO_INVALID_ATTRIBUTE_VALUE

When there are no more entries in the vector.

# SEE ALSO

*drmaa_jobtemplate* (3)and *drmaa_submit* (3).
