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

/** @brief Current time as microseconds since the epoch
 * @return the current time in the 64 bit microsecond representation
 */
uint64_t sge_get_gmt64();

/** @brief Convert seconds since the epoch to the 64 bit microsecond form
 * @param timestamp seconds since the epoch
 * @return the same instant in microseconds
 */
constexpr uint64_t sge_gmt32_to_gmt64(uint32_t timestamp) {
   uint64_t ret = timestamp;
   return ret * 1000000;
}

/** @brief Convert the 64 bit microsecond form to whole seconds
 * @param timestamp the instant in microseconds
 * @return seconds since the epoch, truncated
 */
uint32_t sge_gmt64_to_gmt32(uint64_t timestamp);
/** @brief Convert the 64 bit microsecond form to a `time_t`
 * @param timestamp the instant in microseconds
 * @return the same instant as a `time_t`
 */
time_t sge_gmt64_to_time_t(uint64_t timestamp);
/** @brief Convert the 64 bit microsecond form to a `struct timespec`
 * @param timestamp the instant in microseconds
 * @param[out] tm receives the same instant
 */
void sge_gmt64_to_timespec(uint64_t timestamp, struct timespec &tm);
/** @brief Convert the 64 bit microsecond form to fractional seconds
 * @param timestamp the instant in microseconds
 * @return seconds, with the sub second part preserved
 */
double sge_gmt64_to_gmt32_double(uint64_t timestamp);
/** @brief Convert a `time_t` to the 64 bit microsecond form
 * @param timestamp the instant as a `time_t`
 * @return the same instant in microseconds
 */
uint64_t sge_time_t_to_gmt64(time_t timestamp);

/** @brief Format a timestamp, choosing the layout explicitly
 * @param timestamp the instant to format
 * @param dstr buffer receiving the text
 * @param is_xml use the XML date layout instead of the human readable one
 * @param with_micro include the microsecond part
 * @return the formatted text held by the buffer
 */
const char *sge_ctime64(uint64_t timestamp, dstring *dstr, bool is_xml, bool with_micro);
/** @brief Format a timestamp in the default human readable layout
 * @param timestamp the instant to format
 * @param dstr buffer receiving the text
 * @return the formatted text held by the buffer
 */
const char *sge_ctime64(uint64_t timestamp, dstring *dstr);
/** @brief Format a timestamp in the short layout
 * @param timestamp the instant to format
 * @param dstr buffer receiving the text
 * @return the formatted text held by the buffer
 */
const char *sge_ctime64_short(uint64_t timestamp, dstring *dstr);
/** @brief Format a timestamp in the XML layout
 * @param timestamp the instant to format
 * @param dstr buffer receiving the text
 * @return the formatted text held by the buffer
 */
const char *sge_ctime64_xml(uint64_t timestamp, dstring *dstr);
/** @brief Format a timestamp as date and time without the sub second part
 * @param timestamp the instant to format
 * @param dstr buffer receiving the text
 * @return the formatted text held by the buffer
 */
const char *sge_ctime64_date_time(uint64_t timestamp, dstring *dstr);

/** @brief Append a formatted timestamp to a buffer
 * @param i the instant in microseconds
 * @param buffer buffer to append to
 * @param is_xml use the XML date layout
 * @return the formatted text held by the buffer
 */
const char *append_time(uint64_t i, dstring *buffer, bool is_xml);
/** @overload */
const char *append_time(time_t i, dstring *buffer, bool is_xml);

uint64_t duration_add_offset(uint64_t duration, uint64_t offset);

void sge_usleep(int);
