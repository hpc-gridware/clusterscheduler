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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Time formatting, parsing and monotonic time sources
 */

#include <sys/time.h>
#include <unistd.h>

#include <cinttypes>
#include "uti/sge_dstring.h"

uint64_t sge_get_gmt64();

/** @brief Convert seconds since the epoch to the 64 bit microsecond form
 * @param timestamp seconds since the epoch
 * @return the same instant in microseconds
 */
constexpr uint64_t sge_gmt32_to_gmt64(uint32_t timestamp) {
   uint64_t ret = timestamp;
   return ret * 1000000;
}

uint32_t sge_gmt64_to_gmt32(uint64_t timestamp);
time_t sge_gmt64_to_time_t(uint64_t timestamp);
void sge_gmt64_to_timespec(uint64_t timestamp, struct timespec &tm);
double sge_gmt64_to_gmt32_double(uint64_t timestamp);
uint64_t sge_time_t_to_gmt64(time_t timestamp);

const char *sge_ctime64(uint64_t timestamp, dstring *dstr, bool is_xml, bool with_micro);
const char *sge_ctime64(uint64_t timestamp, dstring *dstr);
const char *sge_ctime64_short(uint64_t timestamp, dstring *dstr);
const char *sge_ctime64_xml(uint64_t timestamp, dstring *dstr);
const char *sge_ctime64_date_time(uint64_t timestamp, dstring *dstr);

const char *append_time(uint64_t i, dstring *buffer, bool is_xml);
const char *append_time(time_t i, dstring *buffer, bool is_xml);

uint64_t duration_add_offset(uint64_t duration, uint64_t offset);

void sge_usleep(int);
