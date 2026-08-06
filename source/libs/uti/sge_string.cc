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
 * @brief Implementation of the string helpers
 */
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <pthread.h>
#include <fnmatch.h>
#include <cerrno>

#include "uti/msg_utilib.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"

/// true when character @p c is one of the delimiters in @p delimitor
#define IS_DELIMITOR(c, delimitor) \
   (delimitor?(strchr(delimitor, c)?1:0):isspace(c))

/**
 * @brief Get basename for path
 *
 * Determines the basename for a path like string - the last field
 * of a string where fields are separated by a fixed one character
 * delimiter.
 *
 * @code
 * sge_basename("/usr/local/bin/flex", '/'); returns "flex"
 * @endcode
 *
 * @param name contains the input string (path)
 * @param delim delimiter
 *
 * @return pointer to base of name after the last delimter nullptr if "name" is nullptr or zero length string or delimiter is the last character in "name"
 *
 * @note MT-NOTE: sge_basename() is MT safe
 */
const char *sge_basename(const char *name, int delim) {
   char *cp;

   DENTER(BASIS_LAYER);

   if (!name) {
      DRETURN(nullptr);
   }
   if (name[0] == '\0') {
      DRETURN(nullptr);
   }

   cp = strrchr((char *)name, delim);
   if (!cp) {
      DRETURN(name);
   } else {
      cp++;
      if (*cp == '\0') {
         DRETURN(nullptr);
      } else {
         DRETURN(cp);
      }
   }
}

/**
 * @brief Determine the jobname of a command-line string.
 *
 * The jobname is everything up to the first ';', then up to the first
 * whitespace, then the basename. Examples:
 *   "cd /home/me/5five; hostname" -> "cd"; "cat /tmp/5five" -> "cat";
 *   "bla;blub" -> "bla"; "a b" -> "a".
 * MT-NOTE: not MT safe (uses sge_strtok()).
 *
 * @param[in] name command-line input string
 * @return pointer to the jobname, or nullptr if @p name is nullptr or empty
 *
 * @see sge_basename(), sge_strtok()
 */
const char *sge_jobname(const char *name) {

   const char *cp = nullptr;

   DENTER(BASIS_LAYER);
   if (name && name[0] != '\0') {
      cp = sge_strtok(name, ";");
      // cp aliases sge_strtok()'s internal static buffer; re-tokenizing it is
      // safe because sge_strtok() copies with memmove() (resolves CS-347,
      // CS-2362).
      cp = sge_strtok(cp, " ");
      cp = sge_basename(cp, '/');
   }

   DRETURN(cp);
}

/**
 * @brief Return first part of string up to deliminator
 *
 * The function will return a malloced string containing the first
 * part of 'name' up to, but not including deliminator. nullptr will
 * be returned if 'name' is nullptr or a zero length string or if
 * 'delim' is the first character in 'name'
 *
 * @param name string
 * @param delim deliminator
 *
 * @return malloced string
 *
 * @note This routine is called "dirname" in opposite to "basename"
 *       but is mostly used to strip off the domainname of a FQDN
 *       MT-NOTE: sge_dirname() is MT safe
 */
char *sge_dirname(const char *name, int delim) {
   char *cp, *cp2;

   DENTER(BASIS_LAYER);

   if (!name) {
      DRETURN(nullptr);
   }

   if (name[0] == '\0' || name[0] == delim) {
      DRETURN(nullptr);
   }

   cp = strchr((char *)name, delim);

   if (!cp) {                   /* no occurence of delim */
      cp2 = strdup(name);
      DRETURN(cp2);
   } else {
      if ((cp2 = sge_malloc((cp - name) + 1)) == nullptr) {
         DRETURN(nullptr);
      } else {
         strncpy(cp2, name, cp - name);
         cp2[cp - name] = '\0';
         DRETURN(cp2);
      }
   }
}

/**
 * @brief Replacement for strtok() using an internal static buffer.
 *
 * Tokenizes @p str on any character in @p delimitor (isspace() if none given)
 * and returns the first token; pass nullptr as @p str on subsequent calls to
 * get the next token. The input is copied into an internal static buffer with
 * memmove(), so it is safe to pass a pointer that aliases that buffer (as
 * sge_jobname() does when re-tokenizing a previous result — CS-347 / CS-2362).
 * MT-NOTE: not MT safe, use sge_strtok_r() instead.
 *
 * @param[in] str       string to tokenize, or nullptr to continue the previous
 * @param[in] delimitor delimiter characters (nullptr selects whitespace)
 * @return the first/next token of @p str, or nullptr when exhausted
 *
 * @see sge_strtok_r()
 */
char *sge_strtok(const char *str, const char *delimitor) {
   char *cp;
   char *saved_cp;
   static char *static_cp = nullptr;
   static char *static_str = nullptr;
   static unsigned int alloc_len = 0;
   unsigned int n;
   bool done;

   DENTER(BASIS_LAYER);

   if (str) {
      n = strlen(str);
      if (static_str) {
         if (n > alloc_len) {
            /* need more memory */
            sge_free(&static_str);
            static_str = sge_malloc(n + 1);
            alloc_len = n;
         }
      } else {
         static_str = sge_malloc(n + 1);
         alloc_len = n;
      }
      SGE_ASSERT(static_str != nullptr);
      // memmove, not strcpy: str may alias static_str (e.g. sge_jobname()
      // re-tokenizes a previous result), and strcpy with overlapping src/dst is
      // undefined behaviour (CS-347, CS-2362, CWE-628). n == strlen(str).
      memmove(static_str, str, n + 1);
      saved_cp = static_str;
   } else {
      saved_cp = static_cp;
   }

   /* seek first character which is no '\0' and no delimitor */
   done = false;
   while (!done) {

      /* found end of string */
      if (saved_cp == nullptr || *saved_cp == '\0') {
         DRETURN(nullptr);
      }

      /* eat white spaces */
      if (!IS_DELIMITOR((int) saved_cp[0], delimitor)) {
         done = true;
         break;
      }

      saved_cp++;
   }

   /* seek end of string given by '\0' or delimitor */
   cp = saved_cp;
   done = false;
   while (!done) {
      if (!cp[0]) {
         static_cp = cp;

         DRETURN(saved_cp);
      }

      /* test if we found a delimitor */
      if (IS_DELIMITOR((int) cp[0], delimitor)) {
         cp[0] = '\0';
         cp++;
         static_cp = cp;

         DRETURN(saved_cp);
      }
      cp++;
   }

   DRETURN(nullptr);
}

/** @brief Appends characters from src to dst.
 *
 * Also terminates the dst string with '\0'.
 * Returns the size required for successfully completing the operation.
 *
 * @param dst destination
 * @param src source string (must be '\0' terminated)
 * @param dstsize size of source string
 */
void
sge_strlcat(char *dst, const char *src, size_t dstsize) {
   // early exit if input parameters are incorrect
   if (dst == NULL || src == NULL || src[0] == '\0' || dstsize == 0) {
      return;
   }

   // find end of dst
   size_t dst_idx = 0;
   for (dst_idx = 0; (dst[dst_idx] != '\0') && (dst_idx < dstsize - 1); dst_idx++) {
      ;
   }

   // copy until no space left or source ends
   char cur;
   for (size_t src_idx = 0; ((cur=src[src_idx]) != '\0') && (dst_idx < dstsize - 1); src_idx++, dst_idx++) {
      dst[dst_idx] = cur;
   }
   dst[dst_idx] = '\0';
}

/**
 * @brief Sge strlcpy implementation
 *
 * copies "dstsize"-1 characters from from "src" to "dst" and terminates
 * the src string with '\0'- Returns the size of the "src" string.
 *
 * @param dst destination
 * @param src source string (must be '\0' terminated)
 * @param dstsize size of source string
 *
 * @return strlen of src, not dst !!!
 *
 * @note MT-NOTE: sge_strlcpy() is MT safe
 */
size_t sge_strlcpy(char *dst, const char *src, size_t dstsize) {
   size_t index = 0;
   if (dst == nullptr) {
      return 0;
   }
   if (src == nullptr) {
      dst[0] = '\0';
      return 0;
   }
   for (index = 0; (src[index] != '\0') && (index < dstsize - 1); index++) {
      dst[index] = src[index];
   }
   dst[index] = '\0';
   while (src[index] != '\0') {
      index++;
   }
   return index;
}

/**
 * @brief Reentrant version of strtok()
 *
 * Reentrant version of sge_strtok. When 'str' is not nullptr,
 * '*context'has to be nullptr. If 'str' is nullptr, '*context'
 * must not be nullptr. The caller is responsible for freeing
 * '*context' with sge_free_saved_vars().
 * If no delimitor is given isspace() is used.
 *
 * @param str str which should be tokenized
 * @param delimitor delimitor string
 * @param context context
 *
 * @return first/next token
 *
 * @note MT-NOTE: sge_strtok_r() is MT safe
 *
 * @see #sge_strtok, #sge_free_saved_vars
 */
char *sge_strtok_r(const char *str, const char *delimitor,
                   struct saved_vars_s **context) {
   char *cp;
   char *saved_cp;
   struct saved_vars_s *saved;
   bool done;

   DENTER(BASIS_LAYER);

   if (str != nullptr) {
      if (*context != nullptr) {
         ERROR(SFNMAX, MSG_POINTER_INVALIDSTRTOKCALL);
      }
      *context = (struct saved_vars_s *) sge_malloc(sizeof(struct saved_vars_s));
      memset(*context, 0, sizeof(struct saved_vars_s));
      saved = *context;

      saved->static_str = sge_malloc(strlen(str) + 1);

      strcpy(saved->static_str, str);
      saved_cp = saved->static_str;
   } else {
      if (*context == nullptr) {
         ERROR(SFNMAX, MSG_POINTER_INVALIDSTRTOKCALL1);
         DRETURN(nullptr);
      }
      saved = *context;
      saved_cp = saved->static_cp;
   }

   /* seek first character which is no '\0' and no delimitor */
   done = false;
   while (!done) {

      /* found end of string */
      if (saved_cp == nullptr || *saved_cp == '\0') {
         DRETURN(nullptr);
      }

      /* eat white spaces */
      if (!IS_DELIMITOR((int) saved_cp[0], delimitor)) {
         done = true;
         break;
      }

      saved_cp++;
   }

   /* seek end of string given by '\0' or delimitor */
   cp = saved_cp;
   done = false;
   while (!done) {
      if (!cp[0]) {
         saved->static_cp = cp;

         DRETURN(saved_cp);
      }

      /* test if we found a delimitor */
      if (IS_DELIMITOR((int) cp[0], delimitor)) {
         cp[0] = '\0';
         cp++;
         saved->static_cp = cp;

         DRETURN(saved_cp);
      }
      cp++;
   }

   DRETURN(nullptr);
}

/**
 * @brief Free 'context' of sge_strtok_r()
 *
 * Free 'context' of sge_strtok_r()
 *
 * @param context parser state returned by #sge_strtok_r; nullptr is allowed
 *
 * @note MT-NOTE: sge_free_saved_vars() is MT safe
 *
 * @see #sge_strtok_r
 */
void sge_free_saved_vars(struct saved_vars_s *context) {
   if (context) {
      if (context->static_str) {
         sge_free(&(context->static_str));
      }
      sge_free(&context);
   }
}

/**
 * @brief Replacement for strdup()
 *
 * Duplicate string 's'. "Use" 'old_str' buffer.
 *
 * @param old_str buffer (will be freed)
 * @param s string
 *
 * @return malloced string
 *
 * @note MT-NOTE: sge_strdup() is MT safe
 */
char *sge_strdup(char *old_str, const char *s) {
   char *ret = nullptr;

   /* 
    * target (old_str) and source (s) might point to the same object!
    * therefore free old_str only after the dup
    */
   if (s != nullptr) {
      int n = strlen(s);
      ret = sge_malloc(n + 1);
      if (ret != nullptr) {
         strcpy(ret, s);
      }
   }

   /* free and nullptr the old_str pointer */
   sge_free(&old_str);

   return ret;
}

/**
 * @brief Strip blanks from string
 *
 * Strip all blanks contained in a string. The string is used
 * both as source and drain for the necessary copy operations.
 * The string is '\0' terminated afterwards.
 *
 * @param str pointer to string to be condensed
 *
 * @note MT-NOTE: sge_strip_blanks() is MT safe
 */
void sge_strip_blanks(char *str) {
   char *cp = str;

   DENTER(BASIS_LAYER);

   if (!str) {
      DRETURN_VOID;
   }

   while (*str) {
      if (*str != ' ') {
         if (cp != str)
            *cp = *str;
         cp++;
      }
      str++;
   };
   *cp = '\0';

   DRETURN_VOID;
}

/**
 * @brief Strip trailing spaces and tabs from a C string.
 *
 * Removes trailing space and tab characters from @p str by writing '\0' over
 * them. Newlines and other whitespace are not touched. MT-safe.
 *
 * @param str string to be modified (modified in place)
 */
void sge_strip_trailing_blanks(char *str) {
   DENTER(BASIS_LAYER);

   if (str != nullptr) {
      size_t length = strlen(str);
      if (length > 0) {
         while (str[length - 1] == ' ' || str[length - 1] == '\t') {
            str[length - 1] = '\0';
            length--;
         }
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Truncate slash at EOL
 *
 * Truncate slash from the end of the string
 *
 * @param str string to be modified
 *
 * @note MT-NOTE: sge_strip_slash_at_eol() is MT safe
 */
void sge_strip_slash_at_eol(char *str) {
   DENTER(BASIS_LAYER);

   if (str != nullptr) {
      size_t length = strlen(str);

      while (str[length - 1] == '/') {
         str[length - 1] = '\0';
         length--;
      }
   }
   DRETURN_VOID;
}


/**
 * @brief Trunc. a str according to a delimiter set
 *
 * Truncates a string according to a delimiter set. A copy of
 * the string truncated according to the delimiter set will be
 * returned.
 *
 * ATTENTION: The user is responsible for freeing the allocated
 * memory outside this routine. If not enough space could be
 * allocated, nullptr is returned.
 *
 * @param str string to be truncated
 * @param delim_pos A placeholder for the delimiting position in str on exit. If set on entry the position of the delimiter in the input string 'str' is returned. If no delimiting character in string was found, the address of the closing '\0' in 'str' is returned.
 * @param delim string containing delimiter characters
 *
 * @return Truncated copy of 'str' (Has to be freed by the caller!)
 *
 * @note MT-NOTE: sge_delim_str() is MT safe
 */
char *sge_delim_str(char *str, char **delim_pos, const char *delim) {
   char *cp = nullptr;
   char *tstr = nullptr;

   DENTER(BASIS_LAYER);

   /* we want it non-destructive --> we need a copy of str */
   if ((tstr = strdup(str)) == nullptr) {
      DRETURN(nullptr);
   }

   /* walk through str to find a character contained in delim or a
    * closing \0
    */

   cp = tstr;
   while (*cp) {
      if (strchr(delim, (int) *cp))     /* found */
         break;
      cp++;
   }

   /* cp now either points to a closing \0 or to a delim character */
   if (*cp) {                    /* if it points to a delim character */
      *cp = '\0';                /* terminate str with a \0 */
   }
   if (delim_pos) {              /* give back delimiting position for name */
      *delim_pos = str + strlen(tstr);
   }
   /* delim_pos either points to the delimiter or the closing \0 in str */

   DRETURN(tstr);
}

/**
 * @brief Like strcmp() but honours nullptr ptrs
 *
 * Like strcmp() apart from the handling of nullptr strings.
 * These are treated as being less than any not-nullptr strings.
 * Important for sorting lists where nullptr strings can occur.
 *
 * @param a 1st string
 * @param b 2nd string
 *
 * @return result 0 - strings are the same or both nullptr -1 - a < b or a is nullptr 1 - a > b or b is nullptr
 *
 * @note MT-NOTE: sge_strnullcmp() is MT safe
 */
int sge_strnullcmp(const char *a, const char *b) {
   if (!a && b) {
      return -1;
   }
   if (a && !b) {
      return 1;
   }
   if (!a && !b) {
      return 0;
   }
   return strcmp(a, b);
}

/**
 * @brief Like fnmatch
 *
 * Like fnmatch() apart from the handling of nullptr strings.
 * These are treated as being less than any not-nullptr strings.
 * Important for sorting lists where nullptr strings can occur.
 *
 * @param str string
 * @param pattern pattern to match
 *
 * @return result 0 - strings are the same or both nullptr -1 - a < b or a is nullptr 1 - a > b or b is nullptr
 *
 * @note MT-NOTE: fnmatch uses static variables, not MT safe
 */
int sge_patternnullcmp(const char *str, const char *pattern) {
   if (!str && pattern) {
      return -1;
   }
   if (str && !pattern) {
      return 1;
   }
   if (!str && !pattern) {
      return 0;
   }
   return fnmatch(pattern, str, 0);
}


/**
 * @brief Like strcasecmp() but honours nullptr ptrs
 *
 * Like strcasecmp() apart from the handling of nullptr strings.
 * These are treated as being less than any not-nullptr strings.
 * Important for sorting lists where nullptr strings can occur.
 *
 * @param a 1st string
 * @param b 2nd string
 *
 * @return result 0 - strings are the same minus case or both nullptr -1 - a < b or a is nullptr 1 - a > b or b is nullptr
 *
 * @note MT-NOTE: sge_strnullcasecmp() is MT safe
 */
int sge_strnullcasecmp(const char *a, const char *b) {
   if (!a && b)
      return -1;
   if (a && !b)
      return 1;
   if (!a && !b)
      return 0;
   return SGE_STRCASECMP(a, b);
}

/** @brief Does the string contain any whitespace?
 *
 * @param s the string to inspect
 * @return true when @p s contains at least one character `isspace()` accepts
 */
bool sge_has_whitespace(const char *s) {
   char c;
   while ((c = *s++)) {
      if (isspace(c)) {
         return true;
      }
   }
   return false;
}

/**
 * @brief Is string a integer value in characters?
 *
 * May we convert 'str' to int?
 *
 * @param str string
 *
 * @return result 0 - It is no integer 1 - It is a integer
 *
 * @note MT-NOTE: sge_strisint() is MT safe
 */
int sge_strisint(const char *str) {
   const char *cp = str;

   while (*cp) {
      if (!isdigit((int) *cp++)) {
         return 0;
      }
   }
   return 1;
}

/**
 * @brief Convert the first n to upper case
 *
 * Convert the first 'max_len' characters to upper case.
 *
 * @param buffer string
 * @param max_len number of chars
 *
 * @note MT-NOTE: sge_strtoupper() is MT safe
 */
void sge_strtoupper(char *buffer, int max_len) {
   DENTER(BASIS_LAYER);

   if (buffer != nullptr) {
      const int length = std::min(static_cast<int>(strlen(buffer)), max_len);
      for (int i = 0; i < length; i++) {
         buffer[i] = toupper(buffer[i]);
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Convert all upper character in the string to lower case
 *
 * sge_strtolower() for hostnames. Honours some configuration values:
 *
 * @param buffer string to be lowered, modified in place
 * @param max_len maximum number of characters to convert
 *
 * @see #sge_strtoupper
 */
void sge_strtolower(char *buffer, int max_len) {
   DENTER(BASIS_LAYER);
   if (buffer != nullptr) {
      int i;
      for (i = 0; buffer[i] != '\0' && i < max_len; i++) {
         buffer[i] = tolower(buffer[i]);
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Duplicate array of strings
 *
 * Copy list of character pointers including the strings these
 * pointers refer to. If 'n' is 0 strings are '\0'-delimited, if
 * 'n' is not 0 we use n as length of the strings.
 *
 * @param cpp pointer to array of strings
 * @param n '\0' terminated?
 *
 * @return copy of 'cpp'
 *
 * @note MT-NOTE: sge_stradup() is MT safe
 */
char **sge_stradup(char **cpp, int n) {
   int count = 0, len;
   char **cpp1, **cpp2, **cpp3;

   /* first count entries */
   cpp2 = cpp;
   while (*cpp2) {
      cpp2++;
      count++;
   }

   /* alloc space */
   cpp1 = (char **) sge_malloc((count + 1) * sizeof(char **));
   if (!cpp1)
      return nullptr;

   /* copy  */
   cpp2 = cpp;
   cpp3 = cpp1;
   while (*cpp2) {
      /* alloc space */
      if (n)
         len = n;
      else
         len = strlen(*cpp2) + 1;

      *cpp3 = (char *) sge_malloc(len);
      if (!(*cpp3)) {
         while ((--cpp3) >= cpp1) {
            sge_free(cpp3);
         }
         sge_free(&cpp1);
         return nullptr;
      }

      /* copy string */
      memcpy(*cpp3, *cpp2, len);
      cpp3++;
      cpp2++;
   }

   *cpp3 = nullptr;

   return cpp1;
}

/**
 * @brief Free list of character pointers
 *
 * Free list of character pointers
 *
 * @param cpp Pointer to array of string pointers
 *
 * @note MT-NOTE: sge_strafree() is MT safe
 */
void sge_strafree(char ***cpp) {
   if (cpp != nullptr && *cpp != nullptr) {
      char **cpp1 = *cpp;

      while (*cpp1 != nullptr) {
         sge_free(cpp1);
         cpp1++;
      }
      sge_free(cpp);
   }
}

/**
 * @brief Find string in string array
 *
 * Compare string with string field and return the pointer to
 * the matched character pointer. Compare exactly n chars
 * case insensitive.
 *
 * @param cp string to be found
 * @param cpp pointer to array of strings
 * @param n number of chars NOTES: MT-NOTE: sge_stramemncpy() is MT safe
 *
 * @return nullptr or pointer a string
 *
 * @note MT-NOTE: sge_stramemncpy() is MT safe
 */
char **sge_stramemncpy(const char *cp, char **cpp, int n) {
   while (*cpp) {
      if (!memcmp(*cpp, cp, n)) {
         return cpp;
      }
      cpp++;
   }
   return nullptr;
}

/**
 * @brief Find string in string array
 *
 * Compare string with string field and return the pointer to
 * the matched character pointer. Compare case sensitive.
 *
 * @param cp string
 * @param cpp pointer to array of strings
 *
 * @return nullptr or pointer a string
 *
 * @note MT-NOTE: sge_stracasecmp() is MT safe
 */
char **sge_stracasecmp(const char *cp, char **cpp) {
   while (*cpp) {
      if (!strcasecmp(*cpp, cp))
         return cpp;
      cpp++;
   }
   return nullptr;
}

/** @brief Print a nullptr terminated string array to stdout, one entry per line
 *
 * A debugging aid for the arrays returned by #string_list.
 *
 * @param stra the array to print, must be nullptr terminated
 */
void
stra_printf(char *stra[]) {
   int i = 0;

   while (stra[i] != nullptr) {
      fprintf(stdout, "%s\n", stra[i]);
      i++;
   }
}

/**
 * @brief Extract valid qstat options/paramers from qstat profile
 *
 * Parse string 'source_str' based on delimeter(s) 'delim' and store
 * resulting tokens in string array 'ret'. Supports comment lines.
 *
 * @param source_str File content of qstat profile as plain string.
 * @param delim Sequence of characters used to identify tokens and parameters.
 *
 * @return String array containing tokens found based on 'delim'.
 *
 * @note It is the caller's responsibilty to free dynamic memory allocated in this
 *       routine.
 *       MT-NOTE: stra_from_str() is MT safe.
 */
char **
stra_from_str(const char *source_str, const char *delim) {
   char **ret = nullptr;

   if (source_str != nullptr && delim != nullptr) {
      struct saved_vars_s *context1;
      struct saved_vars_s *context2;
      const char *token_1 = nullptr;
      const char *token_2 = nullptr;
      int number_of_tokens = 0;
      int index = 0;

      /*
       * Support of comment lines and multiple options per line
       * in qstat profiles requires two level parsing. First
       * level works on a per line basis (delimiter `\n') and
       * sorts out lines starting with comment sign '#'.
       *
       * The result of this process is passed to second
       * level parsing which scans for options and parameters
       * per line. Delimiters are ' ', '\n' and '\t'.
       *
       * We basically need to do this twice: first to determine
       * the number of valid tokens and then to populate the
       * string array.
       *
       */
      /*
       * Count tokens.
       */
      context1 = nullptr;
      token_1 = sge_strtok_r(source_str, "\n", &context1);
      while (token_1 != nullptr) {
         if (token_1[0] != '#') {
            context2 = nullptr;
            token_2 = sge_strtok_r(token_1, " \t", &context2);
            while (token_2 != nullptr) {
               token_2 = sge_strtok_r(nullptr, " \t", &context2);
               number_of_tokens++;
            }
            sge_free_saved_vars(context2);
         }
         token_1 = sge_strtok_r(nullptr, "\n", &context1);
      }
      sge_free_saved_vars(context1);

      /*
       * Note that we need to proceed from here even if we got
       * no valid options/parameters. This is because caller
       * expects a zero entry string array with stopper as minimum.
       */

      /* malloc array memory */
      ret = (char **) sge_malloc(sizeof(char *) * (number_of_tokens + 1));

      if (ret != nullptr) {
         /*
          * Allocate and populate string array.
          */
         index = 0;
         context1 = nullptr;
         token_1 = sge_strtok_r(source_str, "\n", &context1);
         while (token_1 != nullptr) {
            if (token_1[0] != '#') {
               context2 = nullptr;
               token_2 = sge_strtok_r(token_1, " \t", &context2);
               while (token_2 != nullptr) {
                  ret[index] = strdup(token_2);
                  token_2 = sge_strtok_r(nullptr, " \t", &context2);
                  index++;
               }
               sge_free_saved_vars(context2);
            }
            token_1 = sge_strtok_r(nullptr, "\n", &context1);
         }
         sge_free_saved_vars(context1);
         ret[index] = nullptr; /* Stopper */
      }
   }
   return ret;
}

/**
 * @brief Compresses sequences of slashes
 *
 * Compresses sequences of slashes in str to one slash
 *
 * @param str string (e.g. path)
 *
 * @note MT-NOTE: sge_compress_slashes() is MT safe
 */
void sge_compress_slashes(char *str) {
   char *p;
   int compressed = 0;
   DENTER(BASIS_LAYER);

   for (p = str; *p; p++) {
      while (*p == '/' && *(p + 1) == '/') {
         compressed = 1;
         *p = '\0';
         p++;
      }
      if (compressed) {
         strcat(str, p);
         compressed = 0;
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Strip quotes from string
 *
 * Strip quotes from "pstr".
 *
 * @param pstr string to be modified
 *
 * @note MT-NOTE: sge_strip_quotes() is MT safe
 */
void sge_strip_quotes(char **pstr) {
   char *cp = nullptr;
   char *cp2 = nullptr;

   DENTER(TOP_LAYER);

   if (!pstr) {
      DRETURN_VOID;
   }

   for (; *pstr; pstr++) {
      for (cp2 = cp = *pstr; *cp; cp++) {
         if ((*cp != '"') && (*cp != '\'')) {
            *cp2++ = *cp;
         }
      }

      *cp2 = '\0';
   }

   DRETURN_VOID;
}

/**
 * @brief Replacement for strlen()
 *
 * replacement for strlen
 *
 * @param str nullptr or pointer to string
 *
 * @return length of string or 0 if nullptr pointer
 *
 * @note MT-NOTE: sge_strlen() is MT safe
 */
size_t sge_strlen(const char *str) {
   size_t ret = 0;

   if (str != nullptr) {
      ret = strlen(str);
   }
   return ret;
}

/*
** problem: modifies input string,
** this is the most frequently used mode
** but allocating extra memory (as it was in
** sge_string2list) should also be possible
** problem: default delimiters should be possible
** note: there is a similar cull function lString2List
*/
/*
** NAME
**   string_list
** PARAMETER
**   str       -    string to be parsed
**   delis     -    string containing delimiters
**   pstr      -    nullptr or string array to return
** RETURN
**   nullptr      -    error
**   char **   -    pointer to an array of strings containing
**                  the string list
** EXTERNAL
**
** DESCRIPTION
**
** NOTES
**     MT-NOTE: string_list() is MT safe
**
*/
/** @brief Split a string into a nullptr terminated array of tokens
 *
 * Splits @p str at any of the delimiters in @p delis. Quoted sections are kept
 * together, so a delimiter inside quotes does not split.
 *
 * @p str is modified in place: the delimiters are overwritten with NUL and the
 * returned entries point into it, so @p str has to outlive the result.
 *
 * @param str string to split, modified in place
 * @param delis characters to split at
 * @param pstr caller supplied array to fill, or nullptr to have one allocated
 * @return the token array, nullptr terminated. When it was allocated the caller
 *         frees it, but not the entries, which point into @p str
 */
char **string_list(char *str, const char *delis, char **pstr) {
   unsigned int i = 0, j = 0;
   bool is_space = false;
   int found_first_quote = 0;
   char **head = nullptr;
   bool done;

   DENTER(BASIS_LAYER);

   if (str == nullptr) {
      DRETURN(nullptr);
   }

   /* skip heading delimiters */
   while (str[0] != '\0' && strchr(delis, str[0]) != nullptr) {
      str++;
   }

   /* at str end: str either was an empty string or only contained delimiters */
   if (str[0] == '\0') {
      DRETURN(nullptr);
   }

   /*
    * not more items than length of string is possible
    */
   if (pstr == nullptr) {
      head = (char **) sge_malloc((sizeof(void *)) * (strlen(str) + 1));
      if (head == nullptr) {
         DRETURN(nullptr);
      }
   } else {
      head = pstr;
   }

   done = false;
   while (!done) {
      while ((str[i] != '\0') && (strchr(delis, str[i]) != nullptr)) {
         i++;
      }

      if (str[i] == '\0') {
         done = true;
         break;
      }

      head[j] = &str[i];
      j++;
      /*
      ** parse one string
      */
      is_space = false;

      while ((str[i] != '\0') && !is_space) {
         if ((found_first_quote == 0) && (str[i] == '"')) {
            found_first_quote = 2;
         } else if ((found_first_quote == 0) && (str[i] == '\'')) {
            found_first_quote = 1;
         }

         i++;

         /* If we're inside quotes, we don't count spaces. */
         if (found_first_quote == 0) {
            is_space = (bool) (strchr(delis, str[i]) != nullptr);
         }

         if (((found_first_quote == 2) && (str[i] == '"')) ||
             ((found_first_quote == 1) && (str[i] == '\''))) {
            found_first_quote = 0;
         }
      }

      if (str[i] == '\0') {
         done = true;
         break;
      }

      str[i] = '\0';
      i++;
   }

   head[j] = nullptr;

   DRETURN(head);
}

/**
 * @brief Represents the given string a number
 *
 * This function returns true if the given string represents a number.
 *
 * @param string string to parse
 *
 * @return result true  - string represents a number false - string is not a number
 *
 * @note MT-NOTE: sge_str_is_number() is MT safe
 */
bool sge_str_is_number(const char *string) {
   bool ret = true;
   char *end = nullptr;
   double val;

   errno = 0;
   val = strtod(string, &end);

   if (errno == ERANGE) {
      if (val == 0) {
         /* underflow - TODO: do we count this as number? */
         ret = false;
      } else {
         /* overflow - TODO: do we count this as number? */
         ret = false;
      }
   }

   if (end == string) {
      /* no digits found */
      ret = false;
   } else if (*end != '\0') {
      /* additional characters after number found */
      ret = false;
   }

   return ret;
}

/**
 * @brief Replaces all occurences of old with new
 *
 * Replaces all occurences of old with new.
 * If old is part of the given string input, a new string is returned
 * where the replacement is done.
 *
 * @param input the input string
 * @param search the string to replace
 * @param replace the replacement string
 *
 * @return nullptr, if the input string didn't contain the pattern, else a newly allocated string containing the input string with replacements.
 *
 * @note MT-NOTE: sge_str_is_number() is MT safe
 *       It is the responsibility of the caller to free the returned string!
 */
const char *sge_replace_substring(const char *input, const char *search, const char *replace) {
   int to_replace = 0;
   int change, new_len;
   char *new_string = nullptr;
   char *return_string = nullptr;
   char *source = nullptr;
   char *source_string = nullptr;
   char *tail = nullptr;
   char *current_tail = nullptr;

   /*
    * Basic sanity checks first.
    */
   if (input == nullptr || search == nullptr || replace == nullptr) {
      return nullptr;
   }
   /*
    * Determine number for of substrings to replace. We are
    * careful NOT to overrun source string.
    */
   source = source_string = (char *) input;
   tail = source_string + strlen(source_string) - 1;
   while (source <= tail) {
      current_tail = source + strlen(search) - 1;
      if (current_tail > tail) {
         break;
      }
      if (memcmp(search, source, strlen(search)) == 0) {
         to_replace++;
      }
      source++;
   }
   if (to_replace == 0) {
      return nullptr;
   }
   /*
    * Calculate size of new string based on number of substrings to replace.
    */
   change = to_replace * (strlen(replace) - strlen(search));
   new_len = strlen(source_string) + change + 1;
   /*
    * Allocate new string and re-shuffle original string.
    */
   return_string = new_string = sge_malloc(new_len);
   if (new_string == nullptr) {
      return nullptr;
   }
   memset(new_string, 0x0, new_len);
   source = source_string;
   while (source <= tail) {
      current_tail = source + strlen(search) - 1;
      if (current_tail <= tail && memcmp(search, source, strlen(search)) == 0) {
         memcpy(new_string, replace, strlen(replace));
         new_string += strlen(replace);
         source += strlen(search);
      } else {
         *new_string = *source;
         source++;
         new_string++;
      }
   }
   return return_string;
}

/** @brief Move a substring to the front of its buffer
 *
 * Copies the string beginning at @p substr down to @p start, overwriting
 * whatever came before it, and terminates the result. Used to drop a prefix
 * without allocating.
 *
 * @param start where the text should end up, the beginning of the buffer
 * @param substr where the text currently begins; must be at or after @p start
 * @return @p start, or nullptr when either argument is nullptr or @p substr
 *         lies before @p start
 */
const char *sge_str_move_left(char *start, char *substr) {
   if (start == nullptr || substr == nullptr || substr < start) {
      return nullptr;
   } else if (substr > start){
      char *dst = start;
      char *src = substr;
      while (*src != '\0') {
         *dst++ = *src++;
      }
      *dst = '\0';
      return start;
   }
   return start;
}

/** @brief Reverse a string in place
 *
 * This function reverses the characters in the string pointed to by 'str'.
 * It does not allocate any new memory and modifies the original string.
 *
 * @param str Pointer to the string to be reversed. If str is nullptr, no action is taken.
 */
void
sge_str_reverse(char *str) {
   // Nothing to do
   if (!str) {
      return;
   }

   // Exchange characters from the start and end of the string
   int len = strlen(str);
   for (int i = 0; i < len / 2; i++) {
      char temp = str[i];
      str[i] = str[len - i - 1];
      str[len - i - 1] = temp;
   }
}

/****** uti/spool/sge_is_valid_filename2() ************************************
*  NAME
*     sge_is_valid_filename2() -- Verify file name.
*
*  SYNOPSIS
*     int sge_is_valid_filename2(const char *fname)
*
*  FUNCTION
*     Verify the applicability of a file name.
*     We dont like:
*        - names longer than 256 chars including '\0'
*        - blanks or other ugly chars
*     We like digits, chars and '_'.
*
*  INPUTS
*     const char *fname - filename
*
*  RESULT
*     int - result
*        0 - OK
*        1 - Invalid filename
*
*  NOTES
*     MT-NOTE: sge_is_valid_filename2() is MT safe
******************************************************************************/
int sge_is_valid_filename2(const char *fname) {
   int i = 0;

   /* dont allow "." ".." and "../tralla" */
   if (*fname == '.') {
      fname++;
      if (!*fname || (*fname == '.' && ((!*(fname + 1)) || (!*fname + 1 == '/')))) {
         return 1;
      }
   }
   while (*fname && i++ < 256) {
      if (!isalnum((int) *fname) && !(*fname == '_') && !(*fname == '.')) {
         return 1;
      }
      fname++;
   }
   if (i >= 256) {
      return 1;
   }
   return 0;
}

/****** uti/spool/sge_is_valid_filename() *************************************
*  NAME
*     sge_is_valid_filename() -- Check for a valid filename.
*
*  SYNOPSIS
*     int sge_is_valid_filename(const char *fname)
*
*  FUNCTION
*     Check for a valid filename. Filename can only
*     contain: 0-9a-zA-Z._-
*     '/' is not allowed.
*
*  INPUTS
*     const char *fname - filename
*
*  RESULT
*     int - result
*         0 - valid filename
*         1 - invalid filename
*
*  NOTES
*     MT-NOTE: sge_is_valid_filename() is MT safe
******************************************************************************/
int sge_is_valid_filename(const char *fname) {
   const char *cp = fname;

   if (!fname) {
      return 1;
   }
   while (*cp) {
      if (!isalnum((int) *cp) && !(strchr("._-", *cp))) {
         return 1;
      }
      cp++;
   }
   return 0;
}
