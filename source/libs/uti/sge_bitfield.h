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
 * @brief Variable size bitfields, and bit macros for native integer types
 */

#include <cstdio>

#include <cinttypes>

/** @defgroup uti_bitfield Bitfield
 * @brief A variable size bitfield implementation
 *
 * The size of a bitfield is fixed when it is created; individual bits can then
 * be set, read and cleared, and the contents printed to a file handle.
 *
 * Small bitfields cost no allocation at all: up to #fixed_bits bits are held
 * inline in #bitfield::bf, which saves both memory and, above all, the
 * allocation itself. Larger ones allocate.
 *
 * @note MT-NOTE: this module is MT safe
 * @{
 */

/** @name Bit macros for native integer types
 *
 * These operate on plain integers, **not** on #bitfield, and must not be mixed
 * with it.
 * @{
 */
#define ISSET(a, b)      ((a&b)==b)   ///< true when every bit of `a` is set in `b`
#define VALID(a, b)      ((a|b)==b)   ///< true when `a` sets no bit outside `b`
#define SETBIT(a, b)     (b=(a)|b);   ///< set the bits of `a` in `b`
#define CLEARBIT(a, b)   (b &= (~(a)));  ///< clear the bits of `a` in `b`
/** @} */

/** @brief A variable size bitfield
 *
 * Create one with #sge_bitfield_new, or initialise an existing struct with
 * #sge_bitfield_init. Either way it has to be released again, with
 * #sge_bitfield_free or #sge_bitfield_free_data respectively.
 */
typedef struct {
   unsigned int size;            ///< size of the bitfield in bits
   /** @brief Storage, inline for small bitfields and allocated for large ones */
   union {
      char fix[sizeof(char *)];  ///< inline buffer, used while `size` <= #fixed_bits
      char *dyn;                 ///< allocated buffer, used for larger bitfields
   } bf;
} bitfield;

bitfield *
sge_bitfield_new(unsigned int size);

bitfield *
sge_bitfield_free(bitfield *bf);

bool
sge_bitfield_init(bitfield *bf, unsigned int size);

bool
sge_bitfield_free_data(bitfield *bf);

bool
sge_bitfield_copy(const bitfield *source, bitfield *target);

bool
sge_bitfield_bitwise_copy(const bitfield *source, bitfield *target);

bool
sge_bitfield_set(bitfield *bf, unsigned int bit);

bool
sge_bitfield_get(const bitfield *bf, unsigned int bit);

bool
sge_bitfield_clear(bitfield *bf, unsigned int bit);

bool
sge_bitfield_reset(bitfield *source);

bool
sge_bitfield_changed(const bitfield *source);

void
sge_bitfield_print(const bitfield *bf, FILE *fd);

/// number of bits that fit into the inline buffer, so need no allocation
#define fixed_bits (sizeof(char *) * 8)
/// size of a bitfield in bits
#define sge_bitfield_get_size(bf) ((bf)->size)
/// bytes needed to hold `size` bits, rounded up
#define sge_bitfield_get_size_bytes(size) ((size) / 8 + (((size) % 8) > 0 ? 1 : 0))
/// buffer holding the bits, inline or allocated depending on the size
#define sge_bitfield_get_buffer(source) ((source)->size <= fixed_bits) ? (source)->bf.fix : (source)->bf.dyn

/** @} */
