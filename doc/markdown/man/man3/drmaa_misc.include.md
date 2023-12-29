# NAME

drmaa_strerror, drmaa_get_contact, drmaa_version, drmaa_get_DRM_system -
Miscellaneous DRMAA functions.

# SYNOPSIS

**#include "drmaa.h"**

    const char *drmaa_strerror(
    int drmaa_errno

**);**

    int drmaa_get_contact(
    char *contact,
    size_t contact_len,
    char *error_diagnosis,
    size_t error_diag_len

**);**

    int drmaa_version(
    unsigned int *major,
    unsigned int *minor,
    char *error_diagnosis,
    size_t error_diag_len

**);**

    int drmaa_get_DRM_system(
    char *drm_system,
    size_t drm_system_len,
    char *error_diagnosis,
    size_t error_diag_len

**);**

    int drmaa_get_DRMAA_implementation(
    char *drm_impl,
    size_t drm_impl_len,
    char *error_diagnosis,
    size_t error_diag_len

**);**

# DESCRIPTION

The drmaa_strerror() function returns a message text associated with the
DRMAA error number, *drmaa_errno***. For invalid DRMAA error codes
\`NULL' is returned.**

## drmaa_get_contact()

The drmaa_get_contact() returns an opaque string containing contact
information related to the current DRMAA session to be used with the
*drmaa_init* (3) function. The opaque string contains the information
required by drmaa_init() to reconnect to the current session instead of
creating a new session. *drmaa_init* (3) function.

The drmaa_get_contact() function returns the same value before and after
*drmaa_init* (3) is called.

## drmaa_version()

The drmaa_version() function returns into the integers pointed to by
*major*** ** and *minor***, the major and minor version numbers of the
DRMAA library.** For a DRMAA 1.0 compliant implementation \`1' and \`0'
will be returned in *major*** and ***minor***,** respectively.

## drmaa_get_DRM_system()

The drmaa_get_DRM_system() function returns into *drm_system*** up to **
*drm_system_len*** characters of a string containing xxQS_NAMExx product
and ** version information.

The drmaa_get_DRM_system() function returns the same value before and
after *drmaa_init* (3) is called.

## drmaa_get_DRMAA_implementation()

The drmaa_get_DRMAA_implementation() function returns into
*drm_system*** up to ** *drm_system_len*** characters of a string
containing the xxQS_NAMExx DRMAA** implementation version information.
In the current implementation, the drmaa_get_DRMAA_implementation()
function returns the same result as the drmaa_get_DRM_system() function.

The drmaa_get_DRMAA_implementation() function returns the same value
before and after *drmaa_init* (3) is called.

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

Upon successful completion, drmaa_get_contact(), drmaa_version(), and
drmaa_get_DRM_system() return DRMAA_ERRNO_SUCCESS. Other values indicate
an error. Up to *error_diag_len*** characters of error related diagnosis
** information is then provided in the buffer *error_diagnosis***. **

# ERRORS

The drmaa_get_contact(), drmaa_version(), drmaa_get_DRM_system(), and
drmaa_get_DRMAA_implementation() will fail if:

## DRMAA_ERRNO_INTERNAL_ERROR

Unexpected or internal DRMAA error, like system call failure, etc.

## DRMAA_ERRNO_DRM_COMMUNICATION_FAILURE

Could not contact DRM system for this request.

## DRMAA_ERRNO_AUTH_FAILURE

The specified request is not processed successfully due to authorization
failure.

## DRMAA_ERRNO_INVALID_ARGUMENT

The input value for an argument is invalid.

## DRMAA_ERRNO_NO_MEMORY

Failed allocating memory.

The drmaa_get_contact() and drmaa_get_DRM_system() will fail if:

## DRMAA_ERRNO_NO_ACTIVE_SESSION

Failed because there is no active session.

# SEE ALSO

*drmaa_session* (3).
