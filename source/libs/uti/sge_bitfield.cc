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

/** @file
 * @brief Implementation of the variable size bitfield, see @ref uti_bitfield
 */

#include <cstdio>
#include <cstring>

#include "uti/sge_bitfield.h"
#include "uti/sge_stdlib.h"

#include <sge_log.h>

/**
 * @brief Create a new bitfield
 *
 * Allocates and initializes the necessary memory.
 * It is in the responsibility of the caller to free the bitfield
 * once it is no longer needed.
 *
 * @param size size in bits
 *
 * @return a new bitfield or nullptr, if the creation of the bitfield failed
 *
 * @note MT-NOTE: sge_bitfield_new() is MT safe
 *
 * @see #sge_bitfield_free
 */
bitfield *
sge_bitfield_new(unsigned int size) {
   bitfield *bf;

   /* allocate storage for bitfield object */
   bf = (bitfield *) sge_malloc(sizeof(bitfield));
   SGE_ASSERT(bf != nullptr);
   if (bf != nullptr) {
      /* initialize bitfield, on errors, free bitfield */
      if (!sge_bitfield_init(bf, size)) {
         sge_free(&bf);
      }
   }

   return bf;
}

/**
 * @brief Initialize a bitfield object
 *
 * Initializes a bitfield object.
 * Storage for the bits is allocated if necessary (size of the bitfield is
 * bigger than the preallocated storage) and the size is stored.
 *
 * @param bf the bitfield to initialize
 * @param size the targeted size of the bitfield
 *
 * @return true on success, else false
 *
 * @note MT-NOTE: sge_bitfield_init() is MT safe
 */
bool
sge_bitfield_init(bitfield *bf, unsigned int size) {
   bool ret = true;

   if (bf == nullptr) {
      ret = false;
   } else {
      unsigned int char_size = sge_bitfield_get_size_bytes(size);

      /* malloc bitfield buffer only if char * has less bits than required */
      if (size <= fixed_bits) {
         /* clear buffer */
         bf->bf.dyn = (char *)nullptr;
      } else {
         bf->bf.dyn = sge_malloc(char_size);
         if (bf->bf.dyn == nullptr) {
            ret = false;
         } else {
            /* clear buffer */
            memset(bf->bf.dyn, 0, char_size);
         }
      }

      bf->size = size;
   }

   return ret;
}

/**
 * @brief Copies a bitfield into another one
 *
 * The memory has to be allocated before, and source and target has to have
 * the same size. Otherwise it will return false and does not copy anything.
 *
 * @param source source bitfield
 * @param target target bitfield
 *
 * @return false, if one of the bitfields is nullptr or the bitfield sizes are different
 *
 * @note MT-NOTE: sge_bitfield_copy() is MT safe
 */
bool
sge_bitfield_copy(const bitfield *source, bitfield *target) {
   bool ret = true;

   if (source == nullptr || target == nullptr) {
      ret = false;
   }

   if (ret && source->size != target->size) {
      ret = false;
   }
   if (ret) {
      unsigned int char_size = sge_bitfield_get_size_bytes(source->size);
      if (source->size <= fixed_bits) {
         target->bf.dyn = source->bf.dyn;
      } else {
         memcpy(target->bf.dyn, source->bf.dyn, char_size);
      }
   }

   return ret;
}


/**
 * @brief Copies a bitfield into another one
 *
 * The memory has to be allocated before, but the bitfields can have
 * different sizes.  If the source is longer than the target, only the bits
 * up to target's length are copied.
 *
 * @param source source bitfield
 * @param target target bitfield
 *
 * @return false, if one of the bitfields is nullptr
 *
 * @note MT-NOTE: sge_bitfield_bitwise_copy() is MT safe
 */
bool
sge_bitfield_bitwise_copy(const bitfield *source, bitfield *target) {
   bool ret = true;

   if (source == nullptr || target == nullptr) {
      ret = false;
   }

   if (ret) {
      unsigned int char_size = 0;
      const char *source_buffer = sge_bitfield_get_buffer(source);
      char *target_buffer = sge_bitfield_get_buffer(target);

      if (source->size > target->size) {
         /* This may result in the target getting a few more bits than it wants
          * (if target->size isn't a multiple of 8), but that shouldn't matter
          * because sge_bitfield_get() guards against accessing those extra
          * bits. */
         char_size = sge_bitfield_get_size_bytes(target->size);
      } else {
         char_size = sge_bitfield_get_size_bytes(source->size);
      }

      memcpy(target_buffer, source_buffer, char_size);
   }

   return ret;
}

/**
 * @brief Figures out if something was changed
 *
 * @param bf bitfield to analyze
 *
 * @return true, if the bitfield has a changed bit set.
 *
 * @note MT-NOTE: sge_bitfield_copy() is MT safe
 */
bool
sge_bitfield_changed(const bitfield *bf) {
   bool ret = false;

   if (bf != nullptr) {
      const char *buf = sge_bitfield_get_buffer(bf);
      unsigned int char_size = sge_bitfield_get_size_bytes(bf->size);
      unsigned int i;

      for (i = 0; i < char_size; i++) {
         if (buf[i] != 0) {
            ret = true;
            break;
         }
      }
   }

   return ret;
}

/**
 * @brief Clears a bitfield
 *
 * @param bf bitfield to reset
 *
 * @return false, if bf is nullptr
 *
 * @note MT-NOTE: sge_bitfield_copy() is MT safe
 */
bool
sge_bitfield_reset(bitfield *bf) {
   if (bf != nullptr) {
      if (bf->size > fixed_bits) {
         unsigned int char_size = sge_bitfield_get_size_bytes(bf->size);
         memset(bf->bf.dyn, 0, char_size);
      } else {
         bf->bf.dyn = (char *)nullptr;
      }

      return true;
   }

   return false;
}


/**
 * @brief Destroy a bitfield
 *
 * Destroys a bitfield. Frees all memory allocated by the bitfield.
 *
 * @param bf the bitfield to destroy
 *
 * @return nullptr
 *
 * @note MT-NOTE: sge_bitfield_free() is MT safe
 */
bitfield *sge_bitfield_free(bitfield *bf) {
   if (bf != nullptr) {
      if (bf->size > fixed_bits) {
         if (bf->bf.dyn != nullptr) {
            sge_free(&(bf->bf.dyn));
         }
      }
      sge_free(&bf);
   }

   return nullptr;
}

/**
 * @brief Free the bitfield data
 *
 * Frees the data part of a bitfield.
 * The bitfield itself is not freed.
 *
 * @param bf the bitfield to work on
 *
 * @return true on success, else false
 *
 * @note MT-NOTE: sge_bitfield_free_data() is MT safe
 */
bool
sge_bitfield_free_data(bitfield *bf) {
   bool ret = true;

   if (bf == nullptr) {
      ret = false;
   } else {
      if (bf->size > fixed_bits) {
         if (bf->bf.dyn != nullptr) {
            sge_free(&(bf->bf.dyn));
         }
      }
   }

   return ret;
}

/**
 * @brief Set a bit
 *
 * Sets a certain bit in a bitfield to 1.
 *
 * @param bf the bitfield to manipulate
 * @param bit the bit to set
 *
 * @return true on success, false on error
 *
 * @note MT-NOTE: sge_bitfield_set() is MT safe
 */
bool
sge_bitfield_set(bitfield *bf, unsigned int bit) {
   bool ret = true;

   if (bf == nullptr || bit >= bf->size) {
      ret = false;
   }

   if (ret) {
      char *buf = sge_bitfield_get_buffer(bf);
      unsigned int byte_offset = bit / 8;
      unsigned int bit_offset = bit % 8;

      buf[byte_offset] |= 1 << bit_offset;
   }

   return ret;
}

/**
 * @brief Read a bit
 *
 * Reads a certain bit of a bitfield and returns it's contents.
 *
 * @param bf the bitfield to read from
 * @param bit the bit to read
 *
 * @return false, if bit is not set (or input params invalid), true, if bit is set
 *
 * @note MT-NOTE: sge_bitfield_get() is MT safe
 */
bool
sge_bitfield_get(const bitfield *bf, unsigned int bit) {
   bool ret = false;

   if (bf != nullptr && bit < bf->size) {
      const char *buf = sge_bitfield_get_buffer(bf);
      unsigned int byte_offset = bit / 8;
      unsigned int bit_offset = bit % 8;

      if ((buf[byte_offset] & (1 << bit_offset)) > 0) {
         ret = true;
      }
   }

   return ret;
}

/**
 * @brief Clear a bit
 *
 * Clears a certain bit in a bitfield (sets its content to 0).
 *
 * @param bf the bitfield to manipulate
 * @param bit the bit to clear
 *
 * @return true on success, false on error
 *
 * @note MT-NOTE: sge_bitfield_clear() is MT safe
 */
bool
sge_bitfield_clear(bitfield *bf, unsigned int bit) {
   bool ret = true;

   if (bf == nullptr || bit >= bf->size) {
      ret = false;
   }

   if (ret) {
      char *buf = sge_bitfield_get_buffer(bf);
      unsigned int byte_offset = bit / 8;
      unsigned int bit_offset = bit % 8;

      buf[byte_offset] &= 0xff ^ (1 << bit_offset);
   }

   return ret;
}

/**
 * @brief Print contents of a bitfield
 *
 * Prints the contents of a bitfield.
 * For each bit one digit (0/1) is printed.
 * If nullptr is passed as file descriptor, output is sent to stdout.
 *
 * @param bf the bitfield to output
 * @param fd filehandle or nullptr
 *
 * @note MT-NOTE: sge_bitfield_print() is MT safe
 */
void sge_bitfield_print(const bitfield *bf, FILE *fd) {
   unsigned int i;

   if (bf == nullptr) {
      return;
   }

   if (fd == nullptr) {
      fd = stdout;
   }

   for (i = 0; i < bf->size; i++) {
      int value = sge_bitfield_get(bf, i) ? 1 : 0;
      fprintf(fd, "%d ", value);
   }
}

