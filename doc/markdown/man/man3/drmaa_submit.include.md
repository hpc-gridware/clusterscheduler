# NAME

drmaa_run_job, drmaa_run_bulk_jobs, drmaa_get_next_job_id,
drmaa_get_num_job_ids, drmaa_release_job_ids - Job submission

# SYNOPSIS

**#include "drmaa.h"**

    int drmaa_run_job(
    char *job_id,
    size_t job_id_len,
    drmaa_job_template_t *jt,
    char *error_diagnosis, 
    size_t error_diag_len

**);**

    int drmaa_run_bulk_jobs(
    drmaa_job_ids_t **jobids,
    drmaa_job_template_t *jt,
    int start,
    int end,
    int incr,
    char *error_diagnosis,
    size_t error_diag_len

**);**

    int drmaa_get_next_job_id(
    drmaa_job_ids_t* values,
    char *value,
    int value_len

**);**

    int drmaa_get_num_job_ids(
    drmaa_job_ids_t* values,
    int *size

);

    void drmaa_release_job_ids(
    drmaa_job_ids_t* values

);

# DESCRIPTION

drmaa_run_job() submits an xxQS_NAMExx job with attributes defined in
the DRMAA job template, *jt***. On success up to ***job_id_len*** bytes
of the job identifier are ** returned into the buffer, *job_id***.**

## drmaa_run_bulk_jobs()

The drmaa_run_bulk_jobs() submits a xxQS_NAMExx array job very much as
if the *qsub*(1) option \`-t *start***-***end***:***incr***' had been
used along with the additional** attributes defined in the DRMAA job
template, *jt***. The same constraints regarding value ranges are also
in effect** for the parameters *start***, ***end***, and ***incr*** as
for** *qsub*(5) -t. On success a DRMAA job id string vector containing
job identifiers for each array job task is returned into *jobids***. **
The job identifiers in the job id string vector can be extracted using
*drmaa_get_next_job_id*(3). The number of identifiers in the job id
string vector can be determined using *drmaa_get_num_job_ids*(3). Note
that this function is only available in the 1.0 implementation. The
caller is responsible for releasing the job id string vector returned
into *jobids*** using** *drmaa_release_job_ids*(3).

## drmaa_get_next_job_id()

Each time drmaa_get_next_job_id() is called it returns into the buffer,
*value***, up to ***value_len*** ** bytes of the next entry stored in
the DRMAA job id string vector, *values***.** Once the job ids list has
been exhausted, DRMAA_ERRNO_NO_MORE_ELEMENTS is returned.

## drmaa_get_num_job_ids()

The drmaa_get_num_job_ids() returns into *size*** the number of
entries** in the DRMAA job ids string vector. This function is only
available in the 1.0 implementation.

## drmaa_release_job_ids()

The drmaa_release_attr_job_id() function releases all resources
associated with the DRMAA job id string vector, *values***. This
operation has no effect on the actual xxQS_NAMExx array job ** tasks.

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

The drmaa_run_job(), drmaa_run_bulk_jobs(), and drmaa_get_next_job_id()
will fail if:

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

The drmaa_run_job() and drmaa_run_bulk_jobs() functions will fail if:

## DRMAA_ERRNO_TRY_LATER 

The DRM system indicated that it is too busy to accept the job. A retry
may succeed, however.

## DRMAA_ERRNO_DENIED_BY_DRM

The DRM system rejected the job. The job will never be accepted due to
DRM configuration or job template settings.

The drmaa_get_next_job_id() will fail if:

## DRMAA_ERRNO_INVALID_ATTRIBUTE_VALUE

When there are no more entries in the vector.

# SEE ALSO

*drmaa_attributes*(3), *drmaa_jobtemplate*(3).
