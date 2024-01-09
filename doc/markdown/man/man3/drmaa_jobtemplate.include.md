# NAME

drmaa_allocate_job_template, drmaa_delete_job_template,
drmaa_set_attribute, drmaa_get_attribute, drmaa_set_vector_attribute,
drmaa_get_vector_attribute, drmaa_get_next_attr_value,
drmaa_get_num_attr_values, drmaa_release_attr_values - xxQS_NAMExx DRMAA
job template handling

# SYNOPSIS

**#include "drmaa.h"**

    int drmaa_allocate_job_template(
    drmaa_job_template_t **jt,
    char *error_diagnosis,
    size_t error_diag_len

**);**

    int drmaa_delete_job_template(
    drmaa_job_template_t *jt,
    char *error_diagnosis,
    size_t error_diag_len

**);**

    int drmaa_set_attribute(
    drmaa_job_template_t *jt,
    const char *name,
    const char *value,
    char *error_diagnosis,
    size_t error_diag_len

**);**

    int drmaa_get_attribute(
    drmaa_job_template_t *jt,
    const char *name,
    char *value,
    size_t value_len,
    char *error_diagnosis,
    size_t error_diag_len

**);**

    int drmaa_set_vector_attribute(
    drmaa_job_template_t *jt,
    const char *name,
    const char *value[],
    char *error_diagnosis,
    size_t error_diag_len

**);**

    int drmaa_get_vector_attribute(
    drmaa_job_template_t *jt,
    const char *name,
    drmaa_attr_values_t **values,
    char *error_diagnosis,
    size_t error_diag_len

**);**

    int drmaa_get_next_attr_value(
    drmaa_attr_values_t* values,
    char *value,
    int value_len

**);**

    int drmaa_get_num_attr_values(
    drmaa_attr_values_t* values,
    int *size

);

    void drmaa_release_attr_values(
    drmaa_attr_values_t* values

**);**

# DESCRIPTION

The drmaa_allocate_job_template() function allocates a new DRMAA job
template into *jt***. DRMAA job templates describe specifics of jobs
that are submitted** using *drmaa_run_job*(3) and
*drmaa_run_bulk_jobs*(3).

## drmaa_delete_job_template()

The drmaa_delete_job_template() function releases all resources
associated with the DRMAA job template *jt***. Jobs that were submitted
using the job ** template are not affected.

## drmaa_set_attribute()

The drmaa_set_attribute() function stores the *value*** under ***name***
** for the given DRMAA job template, *jt*** . Only non-vector attributes
may be** passed.

## drmaa_get_attribute()

The drmaa_get_attribute() function returns into *value*** up to
***value_len*** ** bytes from the string stored for the non-vector
attribute, *name***, in the** DRMAA job template, *jt***.**

## drmaa_set_vector_attribute()

The drmaa_set_vector_attribute() function stores the strings in
*value*** under ** *name*** in the list of vector attributes for the
given DRMAA job template, ** *jt*** . Only vector attributes may be
passed. The ***value*** pointer array ** must be *NULL*** terminated.**

## drmaa_get_vector_attribute()

The drmaa_get_vector_attribute() function returns into *values*** a
DRMAA attribute ** string vector containing all string values stored in
the vector attribute, *name***. ** The values in the DRMAA values string
vector can be extracted using *drmaa_get_next_attr_value*(3). The
caller is responsible for releasing the DRMAA values string vector
returned into *values*** using ** *drmaa_release_attr_values*(3).

## drmaa_get_next_attr_value()

Each time drmaa_get_next_attr_value() is called it returns into
*value*** up to ***value_len*** ** bytes of the next entry stored in the
DRMAA values string vector, *values***.** Once the values list has been
exhausted, DRMAA_ERRNO_NO_MORE_ELEMENTS is returned.

## drmaa_get_num_attr_values()

The drmaa_get_num_attr_values() returns into *size*** the number of
entries** in the DRMAA values string vector. This function is only
available in the 1.0 implementation.

## drmaa_release_attr_values()

The drmaa_release_attr_values() function releases all resources
associated with the DRMAA values string vector, *values***.**

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

Upon successful completion, drmaa_allocate_job_template(),
drmaa_delete_job_template(), drmaa_set_attribute(),
drmaa_get_attribute(), drmaa_set_vector_attribute(),
drmaa_get_vector_attribute(), and drmaa_get_next_attr_value() return
DRMAA_ERRNO_SUCCESS. Other values indicate an error. Up to
*error_diag_len*** characters of error related diagnosis ** information
is then provided in the buffer *error_diagnosis***.**

# ERRORS

The drmaa_allocate_job_template(), drmaa_delete_job_template(),
drmaa_set_attribute(), drmaa_get_attribute(),
drmaa_set_vector_attribute(), drmaa_get_vector_attribute(), and
drmaa_get_next_attr_value() functions will fail if:

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

The drmaa_set_attribute() and drmaa_set_vector_attribute() will fail if:

## DRMAA_ERRNO_INVALID_ATTRIBUTE_FORMAT

The format for the attribute value is invalid.

## DRMAA_ERRNO_INVALID_ATTRIBUTE_VALUE

The value for the attribute is invalid.

## DRMAA_ERRNO_CONFLICTING_ATTRIBUTE_VALUES

The value of this attribute is conflicting with a previously set
attributes.

The drmaa_get_attribute() and drmaa_get_vector_attribute() will fail if:

## DRMAA_ERRNO_INVALID_ATTRIBUTE_VALUE

The specified attribute is not set in the DRMAA job template.

The drmaa_get_next_attr_value() will fail if:

## DRMAA_ERRNO_INVALID_ATTRIBUTE_VALUE

When there are no more entries in the vector.

# SEE ALSO

*drmaa_submit*(3)and *drmaa_attributes*(3).
