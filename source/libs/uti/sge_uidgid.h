#pragma once
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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <sys/types.h>
#include <grp.h>
#include <pwd.h>

#include "sge_dstring.h"
#include "sge_unistd.h"

#define SGE_SUPERUSER_UID 0
#define SGE_SUPERUSER_GID 0

#ifndef MAX_NIS_RETRIES
#  define MAX_NIS_RETRIES 10
#endif

bool
sge_is_start_user_superuser();

int
sge_set_admin_username(const char *username, char *err_str, size_t err_str_size);

const char *
get_admin_user_name();

int
sge_switch2admin_user();

int
sge_switch2start_user();

bool
sge_has_admin_user();

int
sge_user2uid(const char *user, uid_t *puid, gid_t *pgid, int retries);

int
sge_group2gid(const char *gname, gid_t *gidp, int retries);

int
sge_uid2user(uid_t uid, char *dst, size_t sz, int retries);

int
sge_gid2group(gid_t gid, char *dst, size_t sz, int retries);

int
sge_gid2group(gid_t gid, gid_t *last_gid, char **group_name_p, int retries);

int
sge_add_group(gid_t newgid, char *err_str, size_t err_str_size, bool skip_silently);

int
sge_set_uid_gid_addgrp(const char *user, const char *intermediate_user, int min_gid, int min_uid, int add_grp,
                       char *err_str, size_t err_str_size, int use_qsub_gid, gid_t qsub_gid, bool skip_silently);

struct passwd *
sge_getpwnam_r(const char *name, struct passwd *pw, char *buffer, size_t bufsize);

struct group *
sge_getgrgid_r(gid_t gid, struct group *pg, char *buffer, size_t bufffer_size, int retries);

bool
sge_is_user_superuser(const char *name);

/* getting buffer sizes for getpwnam_r etc. */
int
get_group_buffer_size();

int
get_pw_buffer_size();
