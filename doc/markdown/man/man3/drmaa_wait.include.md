# NAME

drmaa_synchronize, drmaa_wait, drmaa_wifexited, drmaa_wexitstatus,
drmaa_wifsignaled, drmaa_wtermsig, drmaa_wcoredump, drmaa_wifaborted -
Waiting for jobs to finish

# SYNOPSIS

**#include "drmaa.h"**

    int drmaa_synchronize(
    const char *job_ids[],
    signed long timeout,
    int dispose,
    char *error_diagnosis,
    size_t error_diag_len

**);**

    int drmaa_wait(
    const char *job_id,
    char *job_id_out,
    size_t job_id_out_len,
    int *stat,
    signed long timeout,
    drmaa_attr_values_t **rusage,
    char *error_diagnosis,
    size_t error_diagnois_len

**);**

    int drmaa_wifaborted(
    int *aborted,
    int stat,
    char *error_diagnosis,
    size_t error_diag_len

**);**

    int drmaa_wifexited(
    int *exited,
    int stat,
    char *error_diagnosis,
    size_t error_diag_len

**);**

    int drmaa_wifsignaled(
    int *signaled,
    int stat,
    char *error_diagnosis,
    size_t error_diag_len

**);**

    int drmaa_wcoredump(
    int *core_dumped,
    int stat,
    char *error_diagnosis,
    size_t error_diag_len

**);**

    int drmaa_wexitstatus(
    int *exit_status,
    int stat,
    char *error_diagnosis,
    size_t error_diag_len

**);**

    int drmaa_wtermsig(
    char *signal,
    size_t signal_len,
    int stat,
    char *error_diagnosis,
    size_t error_diag_len

**);**

# DESCRIPTION

The drmaa_synchronize() function blocks the calling thread until all
jobs specified in *job_ids*** ** have failed or finished execution. If
*job_ids*** contains 'DRMAA_JOB_IDS_SESSION_ALL', then this ** function
waits for all jobs submitted during this DRMAA session. The *job_ids***
pointer array** must be *NULL*** terminated. **

To prevent blocking indefinitely in this call, the caller may use the
*timeout***, specifying ** how many seconds to wait for this call to
complete before timing out. The special value DRMAA_TIMEOUT_WAIT_FOREVER
can be used to wait indefinitely for a result. The special value
DRMAA_TIMEOUT_NO_WAIT can be used to return immediately. If the call
exits before *timeout*** seconds, all the specified jobs have completed
or** the calling thread received an interrupt. In both cases, the return
code is DRMAA_ERRNO_EXIT_TIMEOUT.

The *dispose*** parameter specifies how to treat reaping information. **
If '0' is passed to this parameter, job finish information will still be
available when *drmaa_wait*(3) is used. If '1' is passed,
*drmaa_wait*(3) will be unable to access this job's finish information.

## drmaa_wait()

The drmaa_wait() function blocks the calling thread until a job fails or
finishes execution. This routine is modeled on the *wait4*(3) routine.
If the special string 'DRMAA_JOB_IDS_SESSION_ANY' is passed as
*job_id***, this routine ** will wait for any job from the session.
Otherwise the *job_id*** must be the job identifier** of a job or array
job task that was submitted during the session.

To prevent blocking indefinitely in this call, the caller may use
*timeout***, specifying ** how many seconds to wait for this call to
complete before timing out. The special value DRMAA_TIMEOUT_WAIT_FOREVER
can be to wait indefinitely for a result. The special value
DRMAA_TIMEOUT_NO_WAIT can be used to return immediately. If the call
exits before *timeout*** seconds have passed, all the specified jobs
have completed or** the calling thread received an interrupt. In both
cases, the return code is DRMAA_ERRNO_EXIT_TIMEOUT.

The routine reaps jobs on a successful call, so any subsequent calls to
*drmaa_wait*(3) will fail returning a DRMAA_ERRNO_INVALID_JOB error,
meaning that the job has already been reaped. This error is the same as
if the job were unknown. Returning due to an elapsed timeout or an
interrupt does not cause the job information to be reaped. This means
that, in this case, it is possible to issue *drmaa_wait*(3) multiple
times for the same *job_id***. **

If *job_id_out*** is not a null pointer, then on return from a
successful ** *drmaa_wait*(3) call, up to *job_id_out_len*** characters
from the job id of the failed ** or finished job are returned.

If *stat*** is not a null pointer, then on return from a successful **
*drmaa_wait*(3) call, the status of the job is stored in the integer
pointed to by *stat***.** *stat*** indicates whether job failed or
finished and other information. The ** information encoded in the
integer value can be accessed via *drmaa_wifaborted*(3)
*drmaa_wifexited*(3) *drmaa_wifsignaled*(3) *drmaa_wcoredump*(3)
*drmaa_wexitstatus*(3) *drmaa_wtermsig*(3).

If *rusage*** is not a null pointer, then on return from a successful**
*drmaa_wait*(3) call, a summary of the resources used by the terminated
job is returned in form of a DRMAA values string vector. The entries in
the DRMAA values string vector can be extracted using
*drmaa_get_next_attr_value*(3). Each string returned by
*drmaa_get_next_attr_value*(3) will be of the format \<name>=\<value>,
where \<name> and \<value> specify name and amount of resources consumed
by the job, respectively. See *accounting*(5) for an explanation of the
resource information.

## drmaa_wifaborted()

The drmaa_wifaborted() function evaluates into the integer pointed to by
*aborted* a non-zero value if *stat*** was returned from a job that
ended before entering the ** running state.

## drmaa_wifexited()

The drmaa_wifexited() function evaluates into the integer pointed to by
*exited*** a ** non-zero value if *stat*** was returned from a job that
terminated normally. A ** zero value can also indicate that although the
job has terminated normally, an exit status is not available, or that it
is not known whether the job terminated normally. In both cases
*drmaa_wexitstatus*(3) will not provide exit status information. A
non-zero value returned in *exited*** ** indicates more detailed
diagnosis can be provided by means of *drmaa_wifsignaled*(3),
*drmaa_wtermsig*(3) and *drmaa_wcoredump*(3).

## drmaa_wifsignaled()

The drmaa_wifsignaled() function evaluates into the integer pointed to
by *signaled*** ** a non-zero value if *stat*** was returned for a job
that terminated due to the receipt of a ** signal. A zero value can also
indicate that although the job has terminated due to the receipt of a
signal, the signal is not available, or it is not known whether the job
terminated due to the receipt of a signal. In both cases
*drmaa_wtermsig*(3) will not provide signal information. A non-zero
value returned in *signaled*** ** indicates signal information can be
retrieved by means of *drmaa_wtermsig*(3).

## drmaa_wcoredump()

If *drmaa_wifsignaled*(3) returned a non-zero value in the *signaled***
parameter, the drmaa_wcoredump() function evaluates into the ** integer
pointed to by *core_dumped*** a non-zero value if a core image of the
terminated ** job was created.

## drmaa_wexitstatus()

If *drmaa_wifexited*(3) returned a non-zero value in the *exited***
parameter, the drmaa_wexitstatus() function evaluates into the** integer
pointed to by *exit_code*** the exit code that the job passed to **
*exit*(2) or the value that the child process returned from main.

## drmaa_wtermsig()

If *drmaa_wifsignaled*(3) returned a non-zero value in the *signaled***
parameter, the drmaa_wtermsig() function evaluates ** into *signal*** up
to ***signal_len*** characters of a string representation of the signal
** that caused the termination of the job. For signals declared by
POSIX.1, the symbolic names are returned (e.g., SIGABRT, SIGALRM). For
signals not declared by POSIX, any other string may be returned.

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
If set, specifies the tcp port on which *xxqs_name_sxx_qmaster*(8) is
expected to listen for communication requests. Most installations will
use a services map entry instead to define that port.

# RETURN VALUES

Upon successful completion, drmaa_run_job(), drmaa_run_bulk_jobs(), and
drmaa_get_next_job_id() return DRMAA_ERRNO_SUCCESS. Other values
indicate an error. Up to *error_diag_len*** characters of error related
diagnosis ** information is then provided in the buffer
*error_diagnosis***.**

# ERRORS

The drmaa_synchronize(), drmaa_wait(), drmaa_wifexited(),
drmaa_wexitstatus(), drmaa_wifsignaled(), drmaa_wtermsig(),
drmaa_wcoredump(), and drmaa_wifaborted() will fail if:

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

The drmaa_synchronize() and drmaa_wait() functions will fail if:

## DRMAA_ERRNO_EXIT_TIMEOUT

Time-out condition.

## DRMAA_ERRNO_INVALID_JOB

The job specified by the does not exist.

The drmaa_wait() will fail if:

## DRMAA_ERRNO_NO_RUSAGE

This error code is returned by drmaa_wait() when a job has finished but
no rusage and stat data could be provided.

# SEE ALSO

*drmaa_submit*(3).
