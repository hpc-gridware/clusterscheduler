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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <sys/time.h>
#include <unistd.h>

#include "basis_types.h"
#include "uti/sge_dstring.h"

u_long64 sge_get_gmt64();

constexpr u_long64 sge_gmt32_to_gmt64(u_long32 timestamp) {
   u_long64 ret = timestamp;
   return ret * 1000000;
}

u_long32 sge_gmt64_to_gmt32(u_long64 timestamp);
time_t sge_gmt64_to_time_t(u_long64 timestamp);
void sge_gmt64_to_timespec(u_long64 timestamp, struct timespec &tm);
double sge_gmt64_to_gmt32_double(u_long64 timestamp);
u_long64 sge_time_t_to_gmt64(time_t timestamp);

const char *sge_ctime64(u_long64 timestamp, dstring *dstr, bool is_xml, bool with_micro);
const char *sge_ctime64(u_long64 timestamp, dstring *dstr);
const char *sge_ctime64_short(u_long64 timestamp, dstring *dstr);
const char *sge_ctime64_xml(u_long64 timestamp, dstring *dstr);
const char *sge_ctime64_date_time(u_long64 timestamp, dstring *dstr);

const char *append_time(u_long64 i, dstring *buffer, bool is_xml);
const char *append_time(time_t i, dstring *buffer, bool is_xml);

u_long64 duration_add_offset(u_long64 duration, u_long64 offset);

void sge_usleep(int);
