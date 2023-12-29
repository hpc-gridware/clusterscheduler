# NAME

drmaa_init, drmaa_exit - Start/finish xxQS_NAMExx DRMAA session

# SYNOPSIS

**#include "drmaa.h"**

    int drmaa_init(
    const char *contact,
    char *error_diagnosis,
    size_t error_diag_len

**);**

    int drmaa_exit(
    char *error_diagnosis,
    size_t error_diag_len

**);**

# DESCRIPTION

## drmaa_init()

The drmaa_init() function initializes the xxQS_NAMExx DRMAA API library
for all threads of the process and creates a new DRMAA session. This
routine must be called once before any other DRMAA call, except for
*drmaa_version* (3), *drmaa_get_DRM_system* (3), and
*drmaa_get_DRMAA_implementation* (3). Except for the above listed
functions, no DRMAA functions may be called before the drmaa_init()
function *completes***. Any DRMAA function which is called** before the
drmaa_init() function completes will return a
DRMAA_ERRNO_NO_ACTIVE_SESSION error. *Contact*** is an implementation
dependent string which may be used to ** specify which xxQS_NAMExx cell
to use. If *contact*** ** is NULL, the default xxQS_NAMExx cell will be
used. In the 1.0 implementation *contact*** may have the following
value: ***session=\<session_id>***. To** determine the session_id, the
*drmaa_get_contact* (3) function should be called after the session has
already been initialized. By passing the *session=\<session_id>***
string to the drmaa_init() function,** instead of creating a new
session, DRMAA will attempt to reconnect to the session indicated by the
*session_id***. The result of reconnecting to a** previous session is
that all jobs previously submitted in that session **that** are still
running** will be available in the DRMAA session. Note, however,** that
jobs which ended before the call to drmaa_init() may not be available or
may have no associated exit information or resource usage data.

## drmaa_exit()

The drmaa_exit() function closes the DRMAA session for all threads and
must be called before process termination. The drmaa_exit() function may
be called only once by a single thread in the process and may only be
called after the drmaa_init() function has completed. Any DRMAA
function, other than *drmaa_init* (3), which is called after the
drmaa_exit() function completes will return a
DRMAA_ERRNO_NO_ACTIVE_SESSION error.

The drmaa_exit() function does necessary clean up of the DRMAA session
state, including unregistering from the *qmaster* (8). If the
drmaa_exit() function is not called, the qmaster will store events for
the DRMAA client until the connection times out, causing extra work for
the qmaster and consuming system resources.

Submitted jobs are not affected by the drmaa_exit() function.

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
If set, specifies the tcp port on which the xxqs_name_sxx_qmaster is
expected to listen for communication requests. Most installations will
use a services map entry instead to define that port.

# RETURN VALUES

Upon successful completion, drmaa_init() and drmaa_exit() return
DRMAA_ERRNO_SUCCESS. Other values indicate an error. Up to
*error_diag_len*** characters of error related diagnosis ** information
is then provided in the buffer *error_diagnosis***.**

# ERRORS

The drmaa_init() and drmaa_exit() functions will fail if:

## DRMAA_ERRNO_INTERNAL_ERROR

Unexpected or internal DRMAA error, like system call failure, etc.

## DRMAA_ERRNO_DRM_COMMUNICATION_FAILURE

Could not contact DRM system for this request.

## DRMAA_ERRNO_AUTH_FAILURE

The specified request is not processed successfully due to authorization
failure.

## DRMAA_ERRNO_INVALID_ARGUMENT

The input value for an argument is invalid.

The drmaa_init() will fail if:

## DRMAA_ERRNO_INVALID_CONTACT_STRING

Initialization failed due to invalid contact string.

## DRMAA_ERRNO_DEFAULT_CONTACT_STRING_ERROR

Could not use the default contact string to connect to DRM system.

## DRMAA_ERRNO_DRMS_INIT_FAILED

Initialization failed due to failure to init DRM system.

## DRMAA_ERRNO_ALREADY_ACTIVE_SESSION

Initialization failed due to existing DRMAA session.

## DRMAA_ERRNO_NO_MEMORY

Failed allocating memory.

The drmaa_exit() will fail if:

## DRMAA_ERRNO_NO_ACTIVE_SESSION

Failed because there is no active session.

## DRMAA_ERRNO_DRMS_EXIT_ERROR

DRM system disengagement failed.

# SEE ALSO

*drmaa_submit* (3).
