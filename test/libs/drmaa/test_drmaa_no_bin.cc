/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 *
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 *
 *  Sun Microsystems Inc., March, 2001
 *
 *
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 *
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 *
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *   Copyright: 2001 by Sun Microsystems, Inc.
 *
 *   All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <cstdlib>
#include "drmaa.h"

#define WD "/tmp"

int handle_code(int code, char *msg);

int main(int argc, char **argv) {
    int ret = DRMAA_ERRNO_SUCCESS;
    char error[DRMAA_ERROR_STRING_BUFFER + 1];
    drmaa_job_template_t *jt = nullptr;
    char jobid[DRMAA_JOBNAME_BUFFER + 1];

    if (argc != 2) {
       printf("Usage: %s path_to_script\n", argv[0]);
       exit(1);
    }
    
    ret = drmaa_init("", error, DRMAA_ERROR_STRING_BUFFER);
    if (handle_code(ret, error) == 1) {
        exit(1);
    }
    
    ret = drmaa_allocate_job_template(&jt, error, DRMAA_ERROR_STRING_BUFFER);
    if (handle_code(ret, error) == 1) {
        return 1;
    }
    
    ret = drmaa_set_attribute(jt, DRMAA_REMOTE_COMMAND, argv[1], error,
    DRMAA_ERROR_STRING_BUFFER);
    if (handle_code(ret, error) == 1) {
        return 1;
    }
    
    ret = drmaa_set_attribute(jt, DRMAA_WD, WD, error,
    DRMAA_ERROR_STRING_BUFFER);
    if (handle_code(ret, error) == 1) {
        return 1;
    }
    
    ret = drmaa_set_attribute(jt, DRMAA_NATIVE_SPECIFICATION, "-b n -cwd", error,
    DRMAA_ERROR_STRING_BUFFER);
    if (handle_code(ret, error) == 1) {
        return 1;
    }
    
    ret = drmaa_run_job(jobid, DRMAA_JOBNAME_BUFFER, jt, error,
    DRMAA_ERROR_STRING_BUFFER);
    if (handle_code(ret, error) == 1) {
        return 1;
    }
    
    ret = drmaa_exit(error, DRMAA_ERROR_STRING_BUFFER);
    handle_code(ret, error);
    
    printf ("OK\n");
    
    return 0;
}

int handle_code(int code, char *msg) {
    if (code != DRMAA_ERRNO_SUCCESS) {
        printf("EXCEPTION: %s\n", msg);
        return 1;
    }
    else {
        return 0;
    }
}
