# NAME

drmaa_job_ps, drmaa_control, - Monitor and control jobs

# SYNOPSIS

**#include "drmaa.h"**

    int drmaa_job_ps(
    const char *job_id,
    int *remote_ps,
    char *error_diagnosis,
    size_t error_diag_len

**);**

    int drmaa_control(
    const char *jobid,
    int action,
    char *error_diagnosis,
    size_t error_diag_len

**);**

# DESCRIPTION

The drmaa_job_ps() function returns the status of the xxQS_NAMExx job
*job_id*** ** into the integer pointed to by *remote_ps***. Possible
return values are **

>     DRMAA_PS_UNDETERMINED        job status cannot be determined
>     DRMAA_PS_QUEUED_ACTIVE       job is queued and active
>     DRMAA_PS_SYSTEM_ON_HOLD      job is queued and in system hold
>     DRMAA_PS_USER_ON_HOLD        job is queued and in user hold
>     DRMAA_PS_USER_SYSTEM_ON_HOLD job is queued and in user and system hold
>     DRMAA_PS_RUNNING             job is running
>     DRMAA_PS_SYSTEM_SUSPENDED    job is system suspended
>     DRMAA_PS_USER_SUSPENDED      job is user suspended
>     DRMAA_PS_DONE                job finished normally
>     DRMAA_PS_FAILED              job finished, but failed

Jobs' user hold and user suspend states can be controlled via
*drmaa_control* (3). For affecting system hold and system suspend states
the appropriate xxQS_NAMExx interfaces must be used.

## drmaa_control()

The drmaa_control() function applies control operations on xxQS_NAMExx
jobs. *jobid*** may contain either an xxQS_NAMExx jobid or**
\`DRMAA_JOB_IDS_SESSION_ALL' to refer to all jobs submitted during the
DRMAA session opened using *drmaa_init* (3). Legal values for *action***
and their meanings are: **

>     DRMAA_CONTROL_SUSPEND        suspend the job 
>     DRMAA_CONTROL_RESUME         resume the job,
>     DRMAA_CONTROL_HOLD           put the job on-hold 
>     DRMAA_CONTROL_RELEASE        release the hold on the job
>     DRMAA_CONTROL_TERMINATE      kill the job

The DRMAA suspend/resume operations are equivalent to the use of \`-sj
\<jobid>' and \`-usj \<jobid>' options with xxQS_NAMExx *qmod* (1). The
DRMAA hold/release operations are equivalent to the use of xxQS_NAMExx
*qhold* (1) and *qrls* (1). The DRMAA terminate operation is equivalent
to the use of xxQS_NAMExx *qdel* (1). Only user hold and user suspend
can be controlled via *drmaa_control* (3). For affecting system hold and
system suspend states the appropriate xxQS_NAMExx interfaces must be
used.

# ENVIRONMENTAL VARIABLES

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

Upon successful completion, drmaa_job_ps(), and drmaa_control() return
DRMAA_ERRNO_SUCCESS. Other values indicate an error. Up to
*error_diag_len*** characters of error related diagnosis ** information
is then provided in the buffer, *error_diagnosis***.**

# ERRORS

The drmaa_job_ps(), and drmaa_control() will fail if:

## DRMAA_ERRNO_INTERNAL_ERROR

Unexpected or internal DRMAA error, like system call failure, etc.

## DRMAA_ERRNO_DRM_COMMUNICATION_FAILURE

Could not contact DRM system for this request.

## DRMAA_ERRNO_AUTH_FAILURE

The specified request was not processed successfully due to
authorization failure.

## DRMAA_ERRNO_INVALID_ARGUMENT

The input value for an argument is invalid.

## DRMAA_ERRNO_NO_ACTIVE_SESSION

Failed because there is no active session.

## DRMAA_ERRNO_NO_MEMORY

Failed allocating memory.

## DRMAA_ERRNO_INVALID_JOB

The specified job does not exist.

The drmaa_control() will fail if:

## DRMAA_ERRNO_RESUME_INCONSISTENT_STATE

The job is not suspended. The resume request will not be processed.

## DRMAA_ERRNO_SUSPEND_INCONSISTENT_STATE

The job is not running and thus cannot be suspended.

## DRMAA_ERRNO_HOLD_INCONSISTENT_STATE

The job cannot be moved to a hold state.

## DRMAA_ERRNO_RELEASE_INCONSISTENT_STATE

The job is not in a hold state.

# SEE ALSO

*drmaa_submit* (3)and *drmaa_wait* (3).
