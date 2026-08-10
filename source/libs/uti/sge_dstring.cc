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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Implementation of the dynamically growing string buffer
 */

#include <limits>
#include <cstring>
#include <cstdio>

#include <uti/ocs_Pattern.h>

/* do not compile in monitoring code */
#ifndef NO_SGE_COMPILE_DEBUG
/// suppresses the monitoring code of the RMON macros in this file
#define NO_SGE_COMPILE_DEBUG
#endif

#include "symbols.h"
#include "uti/sge_dstring.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_stdlib.h"

#include <sge_log.h>

/// growth step in bytes when a dynamic buffer has to be enlarged
#define REALLOC_CHUNK   1024

/// debug layer used by every DENTER/DPRINTF in this file
#define DSTRING_LAYER BASIS_LAYER

/** @brief Either #sge_dstring_copy_string or #sge_dstring_append, selected by the caller
 *
 * Lets `sge_dstring_vsprintf_copy_append()` serve both the copying and the
 * appending sprintf variants with one implementation.
 */
typedef const char *(*sge_dstring_copy_append_f)(dstring *sb, const char *a);

static const char *
sge_dstring_vsprintf_copy_append(dstring *sb,
                                 sge_dstring_copy_append_f function,
                                 const char *format,
                                 va_list ap) {
   const char *ret = nullptr;

   if (sb != nullptr && format != nullptr && function != nullptr) {
      char static_buffer[BUFSIZ];
      int vsnprintf_ret;
      va_list ap_copy;

      va_copy(ap_copy, ap);
      vsnprintf_ret = vsnprintf(static_buffer, BUFSIZ, format, ap_copy);
      va_end(ap_copy);

      /*
       * We have to handle three cases here:
       *    1) If the function returns -1 then vsprintf does not follow 
       *       the C99 standard. We have to increase the buffer until
       *       all parameters fit into the buffer.
       *    2) The function returns a value >BUFSIZE. This indicates
       *       that the function follows the C99 standard. 
       *       vsnprintf_ret is the number of characters which would
       *       have been written to the buffer if it where large enough.
       *       We have to create a buffer of this size.
       *    3) If the return value is >0 and <BUFSIZ than vsprintf
       *       was successful. We do not need a dyn_buffer.
       */
      if (vsnprintf_ret == -1) {
         size_t dyn_size = 2 * BUFSIZ;
         char *dyn_buffer = sge_malloc(dyn_size);

         while (vsnprintf_ret == -1 && dyn_buffer != nullptr) {
            va_copy(ap_copy, ap);
            vsnprintf_ret = vsnprintf(dyn_buffer, dyn_size, format, ap_copy);
            va_end(ap_copy);

            if (vsnprintf_ret == -1) {
               dyn_size *= 2;
               dyn_buffer = (char *)sge_realloc(dyn_buffer, dyn_size, 0);
            }
         }
         if (dyn_buffer != nullptr) {
            ret = function(sb, dyn_buffer);
            sge_free(&dyn_buffer);
         } else {
            /* error: no memory */
            ret = nullptr;
         }
      } else if (vsnprintf_ret > BUFSIZ) {
         char *dyn_buffer = nullptr;

         dyn_buffer = sge_malloc((vsnprintf_ret + 1) * sizeof(char));
         if (dyn_buffer != nullptr) {
            va_copy(ap_copy, ap);
            vsnprintf(dyn_buffer, vsnprintf_ret + 1, format, ap_copy);
            va_end(ap_copy);

            ret = function(sb, dyn_buffer);
            sge_free(&dyn_buffer);
         } else {
            /* error: no memory */
            ret = nullptr;
         }
      } else {
         ret = function(sb, static_buffer);
      }
   }
   return ret;
}


static void
sge_dstring_allocate(dstring *sb, size_t request) {
   /* always request multiples of REALLOC_CHUNK */
   size_t chunks = request / REALLOC_CHUNK + 1;
   request = chunks * REALLOC_CHUNK;

   /* set new size */
   sb->size += request;

   /* allocate memory */
   if (sb->s != nullptr) {
      sb->s = (char *)sge_realloc(sb->s, sb->size * sizeof(char), 1);
   } else {
      sb->s = sge_malloc(sb->size * sizeof(char));
      SGE_ASSERT(sb->s != nullptr);
      sb->s[0] = '\0';
   }
}

/** @brief Initialise a dstring with a preallocated dynamic buffer
 *
 * Use instead of #DSTRING_INIT when the final size is roughly known, to avoid
 * repeated reallocation while the string grows.
 *
 * @param[out] sb the dstring to initialise
 * @param size number of bytes to preallocate
 * @return @p sb, or nullptr when @p sb is nullptr or the allocation failed
 *
 * @note MT-NOTE: sge_dstring_init_dynamic() is MT safe
 * @see #sge_dstring_init, #sge_dstring_free
 */
dstring *sge_dstring_init_dynamic(dstring *sb, size_t size) {
   memset(sb, 0, sizeof(dstring));
   if (sb != nullptr && size > 0) {
      sge_dstring_allocate(sb, size);
      return sb;
   }
   return nullptr;
}


/**
 * @brief Strcat() for dstring's
 *
 * Append 'a' after 'sb'
 *
 * @param sb dynamic string
 * @param a string
 *
 * @return result string
 *
 * @note MT-NOTE: sge_dstring_append() is MT safe
 */
const char *sge_dstring_append(dstring *sb, const char *a) {
   size_t len;  /* length of string a */

   DENTER(DSTRING_LAYER);

   if (sb == nullptr || a == nullptr) {
      DRETURN(nullptr);
   }

   len = strlen(a);

   if (sb->is_static) {
      // @todo: what about the 0 byte?
      if ((sb->length + len) > sb->size)
         len = sb->size - sb->length;

      strncat(sb->s + sb->length, a, len);
      sb->length += len;
   } else {
      size_t required;

      /* only allow to append a string with length 0
         for memory allocation */
      if (len == 0 && sb->s != nullptr) {
         DRETURN(sb->s);
      }

      required = len + sb->length + 1;

      if (required > sb->size) {
         sge_dstring_allocate(sb, required - sb->size);
      }

      strcat(sb->s + sb->length, a);
      sb->length += len;
   }

   DRETURN(sb->s);
}

/**
* @brief 
*
* @param sb
* @param a
* @param n
*
* @return 
*/
const char *sge_dstring_nappend(dstring *sb, const char *a, size_t n) {
   DENTER(DSTRING_LAYER);

   if (sb == nullptr || a == nullptr) {
      DRETURN(0);
   }

   if (sb->is_static) {
      if ((sb->length + n) > sb->size)
         n = sb->size - sb->length;

      strncat(sb->s + sb->length, a, n);
      sb->length += n;
   } else {
      size_t required;

      /* only allow to append a string with length 0 for memory allocation */
      if (n == 0 && sb->s != nullptr) {
         DRETURN(sb->s);
      }

      required = n + sb->length + 1;

      if (required > sb->size) {
         sge_dstring_allocate(sb, required - sb->size);
      }

      strncat(sb->s + sb->length, a, n);
      sb->length += n;
   }

   DRETURN(sb->s);
}

/** @brief Append a single character
 *
 * @param sb the dstring to append to
 * @param a the character to append
 * @return the resulting string, or nullptr when @p sb is nullptr
 *
 * @note MT-NOTE: sge_dstring_append_char() is MT safe
 * @see #sge_dstring_append
 */
const char *sge_dstring_append_char(dstring *sb, const char a) {
   DENTER(DSTRING_LAYER);

   if (sb == nullptr) {
      DRETURN(nullptr);
   }

   if (a == '\0') {
      DRETURN(nullptr);
   }

   if (sb->is_static) {
      // @todo: what about the 0 byte?
      if (sb->length < sb->size) {
         sb->s[sb->length++] = a;
         sb->s[sb->length] = '\0';
      }
   } else {
      size_t required = sb->length + 1 + 1;

      if (required > sb->size) {
         sge_dstring_allocate(sb, required - sb->size);
      }

      sb->s[sb->length++] = a;
      sb->s[sb->length] = '\0';
   }

   DRETURN(sb->s);
}

/** @brief Append the symbolic form of a mail option bitmask
 *
 * Renders @p mailopt the way `qsub -m` expects it, appending one symbol per set
 * flag - abort, beginning, end, suspend - or the "no mail" symbol when none is
 * set.
 *
 * @param sb the dstring to append to
 * @param mailopt bitmask of `MAIL_AT_*` flags
 * @return the resulting string
 *
 * @note MT-NOTE: sge_dstring_append_mailopt() is MT safe
 */
const char *sge_dstring_append_mailopt(dstring *sb, uint32_t mailopt) {
   DENTER(DSTRING_LAYER);

   if ((MAIL_AT_ABORT | mailopt) == mailopt) {
      sge_dstring_append_char(sb, MAIL_AT_ABORT_SYM);
   }
   if ((MAIL_AT_BEGINNING | mailopt) == mailopt) {
      sge_dstring_append_char(sb, MAIL_AT_BEGINNING_SYM);
   }
   if ((MAIL_AT_EXIT | mailopt) == mailopt) {
      sge_dstring_append_char(sb, MAIL_AT_EXIT_SYM);
   }
   if ((NO_MAIL | mailopt) == mailopt) {
      sge_dstring_append_char(sb, NO_MAIL_SYM);
   }
   if ((MAIL_AT_SUSPENSION | mailopt) == mailopt) {
      sge_dstring_append_char(sb, MAIL_AT_SUSPENSION_SYM);
   }

   DRETURN(sb->s);
}

/**
 * @brief Strcat() for dstring's
 *
 * Append 'a' after 'sb'
 *
 * @param sb dynamic string
 * @param a string
 *
 * @return result string
 *
 * @note MT-NOTE: sge_dstring_append_dstring() is MT safe
 */
const char *sge_dstring_append_dstring(dstring *sb, const dstring *a) {
   return sge_dstring_append(sb, sge_dstring_get_string(a));
}

/**
 * @brief Sprintf() for dstring's
 *
 * see sprintf()
 *
 * @param sb dynamic string
 * @param format format string ...                - additional parameters
 *
 * @return result string
 *
 * @note MT-NOTE: sge_dstring_sprintf() is MT safe
 */
const char *sge_dstring_sprintf(dstring *sb, const char *format, ...) {
   const char *ret = nullptr;

   if (sb != nullptr) {
      if (format != nullptr) {
         va_list ap;

         va_start(ap, format);
         ret = sge_dstring_vsprintf_copy_append(sb, sge_dstring_copy_string, format, ap);
         va_end(ap);
      } else {
         ret = sb->s;
      }
   }

   return ret;
}

/**
 * @brief Vsprintf() for dstring's
 *
 * see vsprintf()
 *
 * @param sb dynamic string
 * @param format format string
 * @param ap argument list
 *
 * @return result string
 *
 * @note MT-NOTE: sge_dstring_vsprintf() is MT safe
 */
const char *sge_dstring_vsprintf(dstring *sb, const char *format, va_list ap) {
   const char *ret = nullptr;

   if (sb != nullptr) {
      if (format != nullptr) {
         ret = sge_dstring_vsprintf_copy_append(sb, sge_dstring_copy_string,
                                                format, ap);
      } else {
         ret = sb->s;
      }
   }
   return ret;
}

/**
 * @brief Sprintf() and append for dstring's
 *
 * See sprintf()
 * The string created by sprintf is appended already existing
 * contents of the dstring.
 *
 * @param sb dynamic string
 * @param format format string ...                - additional parameters
 *
 * @return result string
 *
 * @note MT-NOTE: sge_dstring_sprintf_append() is MT safe
 */
const char *sge_dstring_sprintf_append(dstring *sb, const char *format, ...) {
   const char *ret = nullptr;

   if (sb != nullptr) {
      if (format != nullptr) {
         va_list ap;

         va_start(ap, format);
         ret = sge_dstring_vsprintf_copy_append(sb, sge_dstring_append, format, ap);
         va_end(ap);
      } else {
         ret = sb->s;
      }
   }
   return ret;
}

/**
 * @brief Copy string into dstring
 *
 * Copy string into dstring
 *
 * @param sb destination dstring
 * @param str source string
 *
 * @return result string
 *
 * @note MT-NOTE: sge_dstring_copy_string() is MT safe
 */
const char *sge_dstring_copy_string(dstring *sb, const char *str) {
   DENTER(DSTRING_LAYER);

   const char *ret = nullptr;

   if (sb != nullptr) {
      sge_dstring_clear(sb);
      ret = sge_dstring_append(sb, str);
   }

   DRETURN(ret);
}

/**
 * @brief Strcpy() for dstrings's
 *
 * strcpy() for dstrings's
 *
 * @param sb1 destination dstring
 * @param sb2 source dstring
 *
 * @return result string buffer
 *
 * @note MT-NOTE: sge_dstring_copy_dstring() is MT safe
 */
const char *sge_dstring_copy_dstring(dstring *sb1, const dstring *sb2) {
   DENTER(DSTRING_LAYER);

   const char *ret = nullptr;

   if (sb1 != nullptr) {
      sge_dstring_clear(sb1);
      ret = sge_dstring_append(sb1, sge_dstring_get_string(sb2));
   }

   DRETURN(ret);
}

/**
 * @brief Sge_free() for dstring's
 *
 * Frees a dynamically allocated string
 *
 * @param sb dynamic string
 *
 * @note MT-NOTE: sge_dstring_free() is MT safe
 */
void sge_dstring_free(dstring *sb) {
   if (sb != nullptr && !sb->is_static && sb->s != nullptr) {
      sge_free(&(sb->s));
      sb->size = 0;
      sb->length = 0;
   }
}

/**
 * @brief Empty a dstring
 *
 * Set a dstring to an empty string.
 *
 * @param sb dynamic string
 *
 * @note MT-NOTE: sge_dstring_clear() is MT safe
 */
void sge_dstring_clear(dstring *sb) {
   if (sb == nullptr)
      return;

   if (sb->s != nullptr) {
      sb->s[0] = '\0';
   }

   sb->length = 0;
}

/**
 * @brief Returns string buffer
 *
 * Returns a pointer to the buffer where the string is stored.
 * The pointer is not valid until doomsday. The next
 * sge_dstring_* call may make it invalid.
 *
 * @param sb the dstring to query
 *
 * @return pointer to string buffer
 *
 * @note MT-NOTE: sge_dstring_get_string() is MT safe
 */
const char *sge_dstring_get_string(const dstring *sb) {
   return (sb != nullptr) ? sb->s : nullptr;
}

/** @brief Returns a pointer to the string buffer of a dstring
 *
 * The content of the buffer can be adjusted by the caller as
 * long as size of the buffer (returned by sge_dstring_get_size()) is not exceeded.
 *
 * @param sb pointer to dstring
 * @returns pointer to the string buffer of a dstring
 */
char *sge_dstring_get_string_rw(dstring *sb) {
   return (sb != nullptr) ? sb->s : nullptr;
}

/** @brief Returns the max size (buffer size) of a dstring
 *
 * @param sb pointer to dstring
 * @returns size of the dstring
 */
size_t sge_dstring_get_size(const dstring *sb) {
   return (sb != nullptr) ? sb->size : 0;
}

/**
 * @brief Strlen() for dstring's
 *
 * strlen() for dstring's
 *
 * @param sb the dstring to query
 *
 * @return string length
 *
 * @note MT-NOTE: sge_dstring_strlen() is MT safe
 */
size_t sge_dstring_strlen(const dstring *sb) {
   size_t ret = 0;

   if (sb != nullptr) {
      ret = sb->length;
   }

   return ret;
}

/**
 * @brief Remaining chars in dstring
 *
 * Returns number of chars remaining in dstrings.
 *
 * @param sb the dstring to query
 *
 * @return remaining chars
 *
 * @note MT-NOTE: sge_dstring_remaining() is MT safe
 */
size_t sge_dstring_remaining(const dstring *sb) {
   size_t ret = 0;

   if (sb != nullptr) {
      if (sb->is_static) {
         ret = sb->size - sb->length;
      } else {
         ret = std::numeric_limits<uint32_t>::max();
      }
   }

   return ret;
}

/** @brief Wrap a caller supplied buffer in a dstring
 *
 * The counterpart of #DSTRING_INIT for buffers the caller already owns. The
 * dstring starts out empty and static; it switches to an allocated buffer by
 * itself once the content no longer fits, so @p s does not have to be large
 * enough for the final result.
 *
 * One byte of @p size is reserved for the terminating NUL.
 *
 * Does nothing when @p sb or @p s is nullptr.
 *
 * @param[out] sb the dstring to initialise
 * @param s buffer to use, must stay alive as long as @p sb is used
 * @param size size of @p s in bytes
 *
 * @note MT-NOTE: sge_dstring_init() is MT safe
 * @see #DSTRING_STATIC, #sge_dstring_init_dynamic
 */
void sge_dstring_init(dstring *sb, char *s, size_t size) {
   if (sb != nullptr && s != nullptr) {
      sb->is_static = true;
      sb->length = 0;
      sb->size = size - 1;   /* leave space for trailing 0 */
      sb->s = s;
      sb->s[0] = '\0';
   }
}

/**
 * @brief Convert a uint32 to its binary string representation.
 *
 * Writes the binary representation of @p number into @p sb with no leading
 * zeros (e.g. 5 → "101", 8 → "1000"). @p number == 0 produces an empty string.
 *
 * @param sb     dstring to receive the result
 * @param number value to convert
 * @return pointer to the dstring's internal buffer
 */
const char *sge_dstring_ulong_to_binstring(dstring *sb, uint32_t number) {
   char buffer[33] = "                              ";
   int i = 31;

   while (number > 0) {
      if ((number % 2) > 0) {
         buffer[i] = '1';
      } else {
         buffer[i] = '0';
      }
      i--;
      number /= 2;
   }
   sge_strip_blanks(buffer);
   // copy verbatim, never as a printf format string (CS-2354, CWE-134); buffer
   // here only holds binary digits, but this keeps the safe-by-construction
   // pattern consistent with sge_tmpnam().
   sge_dstring_copy_string(sb, buffer);
   return sge_dstring_get_string(sb);
}

/**
 * @brief Split a dstring at the first occurrence of a delimiter character.
 *
 * Finds the first occurrence of @p character in @p string. Characters before
 * the delimiter are appended to @p before; characters after it are appended to
 * @p after. If @p character is not found, @p before is left unchanged and the
 * entire string is appended to @p after.
 * Always returns true; the return value does not indicate whether the delimiter
 * was found.
 *
 * @param string    source dstring
 * @param character delimiter character to split on
 * @param before    receives characters before the delimiter
 * @param after     receives characters after the delimiter
 * @return always true
 */
bool
sge_dstring_split(dstring *string, char character, dstring *before, dstring *after) {
   DENTER(DSTRING_LAYER);

   bool ret = true;

   if (string != nullptr && before != nullptr && after != nullptr) {
      const char *s = sge_dstring_get_string(string);
      const char *end = strchr(s, character);

      while (end != nullptr && s != end) {
         sge_dstring_append_char(before, *(s++));
      }
      if (*s == character) {
         s++;
      }
      sge_dstring_append(after, s);
   }
   DRETURN(ret);
}

/**
 * @brief Strip trailing spaces and tabs from a dstring.
 *
 * Removes trailing space and tab characters from the dstring's raw buffer.
 * Newlines and other whitespace are not touched. The dstring length field is
 * not updated after stripping; use sge_dstring_get_string() rather than
 * sge_dstring_strlen() to observe the result.
 *
 * @param string dstring to be modified
 */
void sge_dstring_strip_trailing_blanks(dstring *string) {
   DENTER(DSTRING_LAYER);
   if (string != nullptr) {
      char *s = string->s;

      if (s != nullptr) {
         sge_strip_trailing_blanks(s);
      }
   }
   DRETURN_VOID;
}

/** @brief Join an argument vector into one space separated string
 *
 * Arguments are appended in order, separated by single spaces. An argument is
 * wrapped in double quotes when it needs protecting, so the result can be shown
 * to a user or fed back to a shell without the word boundaries being lost.
 *
 * @param dstr the dstring to append to
 * @param argc number of entries in @p argv
 * @param argv the arguments to join
 * @param quote_whitespace quote arguments containing whitespace
 * @param quote_patterns quote arguments containing shell pattern characters
 * @return the resulting string
 *
 * @note MT-NOTE: sge_dstring_from_argv() is MT safe
 */
const char *
sge_dstring_from_argv(dstring *dstr, int argc, const char *argv[], bool quote_whitespace, bool quote_patterns) {
   bool first = true;
   for (int i = 0; i < argc; i++) {
      if (first) {
         first = false;
      } else {
         sge_dstring_append_char(dstr, ' ');
      }
      bool do_quote = false;
      if (quote_whitespace) {
         if (sge_has_whitespace(argv[i])) {
            do_quote = true;
         }
      }
      if (quote_patterns) {
         if (ocs::is_pattern(argv[i])) {
            do_quote = true;
         }
      }
      if (do_quote) {
         sge_dstring_append_char(dstr, '\'');
      }
      sge_dstring_append(dstr, argv[i]);
      if (do_quote) {
         sge_dstring_append_char(dstr, '\'');
      }
   }

   return sge_dstring_get_string(dstr);
}

/**
 * @brief Replacement for strerror
 *
 * Returns a string describing an error condition set by system
 * calls (errno).
 *
 * Wrapper arround strerror. Access to strerrror is serialized by the
 * use of a mutex variable to make strerror thread safe.
 *
 * @param errnum the errno to explain
 * @param buffer buffer into which the error message is written
 *
 * @return pointer to a string explaining errnum
 *
 * @note MT-NOTE: sge_strerror() is MT safe
 */
const char *
sge_strerror(int errnum, dstring *buffer) {
   static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
   const char *ret;

   pthread_mutex_lock(&mtx);
   ret = strerror(errnum);
   ret = sge_dstring_copy_string(buffer, ret);
   pthread_mutex_unlock(&mtx);

   return ret;
}

#if 0 /* EB: DEBUG: */
int main()
{
   char *s;
   dstring sb = DSTRING_INIT;    /* initialize */

   /*
    * change content
    */
   s = sge_dstring_append(&sb, "Trala");
   s = sge_dstring_append(&sb, " trolo");
   s = sge_dstring_append(&sb, " troet");
   s = sge_dstring_sprintf(&sb, "%d, %s, %f\n", 5, "rabarber ", 5.6);

   /*
    * use string
    */
   printf("%s\n", s);
   printf("%s\n", sge_dstring_get_string(&sb));

   /*
    * free the string when no longer needed
    */
   sge_dstring_free(&sb);
   return 0;
}
#endif
