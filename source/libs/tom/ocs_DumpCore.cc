/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

#include "tom/ocs_DumpCore.h"

#include <cctype>
#include <cerrno>
#include <exception>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

/* do not compile in monitoring code */
#ifndef NO_SGE_COMPILE_DEBUG
#define NO_SGE_COMPILE_DEBUG
#endif

#include "uti/sge_dstring.h"
#include "uti/sge_io.h"
#include "uti/sge_log.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"

#include "tom/detail/ocs_ListP.h"
#include "tom/detail/ocs_MultitypeP.h"
#include "tom/msg_tom.h"
#include "tom/ocs_Descr.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_ListCore.h"
#include "tom/ocs_Model.h"
#include "tom/ocs_PackCore.h"

namespace ocs::tom {

   namespace {

      /// buffer size for one line read back from a dump
      constexpr int ReadLineLength = MAX_STRING_SIZE;
      /// the string one nesting level is indented by
      constexpr const char *IndentString = "   ";

      /**
       * @brief Throw the failure of a dump or of reading one
       * @param what what failed, the message of the ListCoreError
       */
      [[noreturn, gnu::cold]] void fail(const char *what) {
         throw ListCoreError(what);
      }

      /**
       * @brief Run a reader and report a failure in it as a failure of the caller, as the old readers did when
       *        they returned
       * @param what what failed, the message of the ListCoreError that replaces the reader's
       * @param read the reader
       * @return what the reader returns
       * @throw ListCoreError with @p what if the reader throws one
       */
      template <typename Read>
      auto relabel(const char *what, Read read) -> decltype(read()) {
         try {
            return read();
         } catch (const ListCoreError &) {
            fail(what);
         }
      }

      /** @brief Frees a descriptor allocated with sge_malloc() */
      struct MallocFree {
         /**
          * @brief Free it
          * @param p the memory
          */
         void operator()(void *p) const noexcept {
            free(p);
         }
      };

      /** @brief Frees an element */
      struct ElemFree {
         /**
          * @brief Free it
          * @param ep the element
          */
         void operator()(ocs::tom::ElementData *ep) const noexcept {
            free_elem(ep);
         }
      };

      /** @brief Frees a list */
      struct ListFree {
         /**
          * @brief Free it
          * @param lp the list
          */
         void operator()(ocs::tom::ListData *lp) const noexcept {
            free_list(lp);
         }
      };

      using OwnedChars = std::unique_ptr<char, MallocFree>;     ///< a string allocated with malloc
      /// a descriptor allocated with sge_malloc
      using OwnedDescr = std::unique_ptr<ocs::tom::FieldDescr, MallocFree>;
      using OwnedElem = std::unique_ptr<ocs::tom::ElementData, ElemFree>;   ///< an element being read
      using OwnedList = std::unique_ptr<ocs::tom::ListData, ListFree>;       ///< a list being read

      /**
       * @brief The indentation of a nesting level
       * @param indent the level
       * @return three blanks per level
       */
      std::string indentation(const int indent) {
         std::string space;
         for (int i = 0; i < indent; i++) {
            space += IndentString;
         }
         return space;
      }

      int write_descr(FILE *fp, const ocs::tom::FieldDescr *dp, int indent);
      int write_elem(FILE *fp, const ocs::tom::ElementData *ep, int indent);
      int write_object(FILE *fp, const ocs::tom::ElementData *ep, int indent);
      int write_list(FILE *fp, const ocs::tom::ListData *lp, int indent);
      ocs::tom::ListData *read_list(FILE *fp, const char *name, const ocs::tom::FieldDescr *dp);
      ocs::tom::ElementData *read_object(FILE *fp);

      // ---------------------------------------------------------------- writers
      //
      // The writers keep the result of before: the outcome of the last write
      // only, so that a failed write in the middle still closes the block.

      /**
       * @brief Write a descriptor
       * @param fp the stream
       * @param dp the descriptor
       * @param indent the nesting level
       * @return 0, or -1 if the last write failed
       * @throw ListCoreError if the stream or the descriptor is null
       */
      int write_descr(FILE *fp, const ocs::tom::FieldDescr *dp, const int indent) {
         const std::string space = indentation(indent);
         if (fp == nullptr) {
            fail("dumping a descriptor to no stream");
         }
         int ret = fprintf(fp, "%s{ /* DESCR BEGIN */\n", space.c_str());
         if (dp == nullptr) {
            fail("dumping a null descriptor");
         }
         ret = fprintf(fp, "%s/* NUMBER OF DESCR FIELDS */ %d\n", space.c_str(), lCountPresent(dp));
         for (const auto f : ocs::tom::present_fields(dp)) {
            if (ret == EOF) {
               break;
            }
            ret = fprintf(fp, "%s/* %-20.20s */ { %d, %d }\n", space.c_str(), ocs::tom::field_name(dp[f.pos].nm),
                          dp[f.pos].nm, dp[f.pos].mt);
         }
         ret = fprintf(fp, "%s} /* DESCR END */\n", space.c_str());
         return ret == EOF ? -1 : 0;
      }

      /**
       * @brief Write an element
       * @param fp the stream
       * @param ep the element
       * @param indent the nesting level
       * @return 0, or -1 if the last write failed
       * @throw ListCoreError if the stream or the element is null
       */
      int write_elem(FILE *fp, const ocs::tom::ElementData *ep, const int indent) {
         const std::string sp = indentation(indent);
         const char *space = sp.c_str();
         if (fp == nullptr) {
            fail("dumping an element to no stream");
         }
         if (ep == nullptr) {
            fail("dumping a null element");
         }

         dstring dstr = DSTRING_INIT;
         int ret = fprintf(fp, "%s{ \n", space);
         ret = 0;
         for (const auto f : ocs::tom::present_fields(ep->descr)) {
            if (ret == EOF) {
               break;
            }
            const ocs::tom::FieldValue &value = ocs::tom::detail::value(ep, f.pos);
            const char *name = ocs::tom::field_name(ep->descr[f.pos].nm);
            switch (mt_get_type(ep->descr[f.pos].mt)) {
               case MT_INT32:
                  ret = fprintf(fp, "%s/* %-20.20s */ %d\n", space, name, value.i);
                  break;
               case MT_UINT32:
                  ret = fprintf(fp, "%s/* %-20.20s */ " sge_u32 "\n", space, name, value.ul);
                  break;
               case MT_UINT64:
                  ret = fprintf(fp, "%s/* %-20.20s */ " sge_u64 "\n", space, name, value.ul64);
                  break;
               case MT_STRING: {
                  // a " inside the string is escaped
                  // CS-3171: the reentrant tokenizer, sge_strtok() keeps its state in static variables
                  struct saved_vars_s *context = nullptr;
                  const char *tok;
                  if ((tok = sge_strtok_r(value.str, "\"", &context)) != nullptr) {
                     sge_dstring_append(&dstr, tok);
                     while ((tok = sge_strtok_r(nullptr, "\"", &context)) != nullptr) {
                        sge_dstring_append(&dstr, "\\\"");
                        sge_dstring_append(&dstr, tok);
                     }
                  }
                  sge_free_saved_vars(context);
                  const char *str = sge_dstring_get_string(&dstr);
                  ret = fprintf(fp, "%s/* %-20.20s */ \"%s\"\n", space, name, str != nullptr ? str : "");
                  sge_dstring_clear(&dstr);
                  break;
               }
               case MT_HOST:
                  ret = fprintf(fp, "%s/* %-20.20s */ \"%s\"\n", space, name, value.host != nullptr ? value.host : "");
                  break;
               case MT_DOUBLE:
                  ret = fprintf(fp, "%s/* %-20.20s */ %f\n", space, name, value.db);
                  break;
               case MT_INT64:
                  ret = fprintf(fp, "%s/* %-20.20s */%ld \n", space, name, value.l);
                  break;
               case MT_BOOL:
                  ret = fprintf(fp, "%s/* %-20.20s */ %d\n", space, name, value.b);
                  break;
               case MT_REF:
                  ret = fprintf(fp, "%s/* %-20.20s */ %ld\n", space, name, reinterpret_cast<long>(value.ref));
                  break;
               case MT_OBJECT:
                  if (value.obj == nullptr) {
                     ret = fprintf(fp, "%s/* %-20.20s */ none\n", space, name);
                  } else {
                     ret = fprintf(fp, "%s/* %-20.20s */ object\n", space, name);
                     if (ret != EOF) {
                        ret = write_object(fp, value.obj, indent + 1);
                     }
                  }
                  break;
               case MT_LIST:
                  if (value.glp == nullptr) {
                     ret = fprintf(fp, "%s/* %-20.20s */ empty\n", space, name);
                  } else {
                     ret = fprintf(fp, "%s/* %-20.20s */ full\n", space, name);
                     if (ret != EOF) {
                        ret = write_list(fp, value.glp, indent + 1);
                     }
                  }
                  break;
               default:
                  break;
            }
         }
         sge_dstring_free(&dstr);
         ret = fprintf(fp, "%s}\n", space);
         return ret == EOF ? -1 : 0;
      }

      /**
       * @brief Write a sub-object with its descriptor
       * @param fp the stream
       * @param ep the object
       * @param indent the nesting level
       * @return 0, or -1 if the last write failed
       * @throw ListCoreError if the stream or the element is null
       */
      int write_object(FILE *fp, const ocs::tom::ElementData *ep, const int indent) {
         const std::string space = indentation(indent);
         if (fp == nullptr) {
            fail("dumping an object to no stream");
         }
         if (ep == nullptr) {
            fail("dumping a null object");
         }
         fprintf(fp, "%s{ /* OBJECT BEGIN */\n", space.c_str());
         write_descr(fp, ep->descr, indent);
         write_elem(fp, ep, indent);
         const int ret = fprintf(fp, "%s} /* OBJECT END */\n", space.c_str());
         return ret == EOF ? -1 : 0;
      }

      /**
       * @brief Write a list with its name, its descriptor and its elements
       * @param fp the stream
       * @param lp the list
       * @param indent the nesting level
       * @return 0, or -1 if the last write failed
       * @throw ListCoreError if the stream or the list is null
       */
      int write_list(FILE *fp, const ocs::tom::ListData *lp, const int indent) {
         const std::string space = indentation(indent);
         if (fp == nullptr) {
            fail("dumping a list to no stream");
         }
         if (lp == nullptr) {
            fail("dumping a null list");
         }
         fprintf(fp, "%s{ /* LIST BEGIN */\n", space.c_str());
         fprintf(fp, "%s/* LISTNAME               */ \"%s\"\n", space.c_str(), list_name(lp));
         fprintf(fp, "%s/* NUMBER OF ELEMENTS     */ " sge_u32 "\n", space.c_str(), lp->nelem);
         int ret = write_descr(fp, lp->descr, indent);
         for (const ocs::tom::ElementData *ep = lp->first; ep != nullptr && ret != EOF; ep = ep->next) {
            ret = write_elem(fp, ep, indent);
         }
         ret = fprintf(fp, "%s} /* LIST END */\n", space.c_str());
         return ret == EOF ? -1 : 0;
      }

      // ---------------------------------------------------------------- readers
      //
      // Every reader throws a ListCoreError naming what failed; a caller that
      // reported a failure of its own when the reader returned an error does so
      // through relabel(), so that the message names the outermost reader.

      /**
       * @brief Blank out the comments of a line
       * @param s the line
       * @return false if a comment is not closed
       */
      bool blank_comments(char *s) {
         char *t;
         while ((t = strstr(s, "/*")) != nullptr) {
            char *p = strstr(t + 2, "*/");
            if (p == nullptr) {
               return false;
            }
            while (t < p + 2) {
               *t++ = ' ';
            }
         }
         return true;
      }

      /**
       * @brief Read a line with its comments blanked out
       * @param fp the stream
       * @param line receives the line
       * @param max_line the size of @p line
       * @throw ListCoreError if the stream is null, no line can be read or a comment is not closed
       */
      void read_line(FILE *fp, char *line, const int max_line) {
         if (fp == nullptr) {
            fail("reading a line from no stream");
         }
         if (fgets(line, max_line, fp) == nullptr) {
            fail("reading a line");
         }
         if (!blank_comments(line)) {
            fail("reading a line with a comment that is not closed");
         }
      }

      /**
       * @brief Read a line and tell whether it opens a block
       * @param fp the stream
       * @return true if the line holds a `{`
       * @throw ListCoreError if no line can be read
       */
      bool read_bra(FILE *fp) {
         char s[ReadLineLength + 1];
         relabel("reading the opening of a block", [&] { read_line(fp, s, ReadLineLength); });
         return strstr(s, "{") != nullptr;
      }

      /**
       * @brief Read a line and tell whether it closes a block
       * @param fp the stream
       * @return true if the line holds a `}`
       * @throw ListCoreError if no line can be read
       */
      bool read_ket(FILE *fp) {
         char s[ReadLineLength + 1];
         relabel("reading the closing of a block", [&] { read_line(fp, s, ReadLineLength); });
         return strstr(s, "}") != nullptr;
      }

      /**
       * @brief Whether the next line opens a block; a line that cannot be read does not
       * @param fp the stream
       * @return true if it does
       */
      bool opens(FILE *fp) {
         try {
            return read_bra(fp);
         } catch (const ListCoreError &) {
            return false;
         }
      }

      /**
       * @brief Whether the next line closes a block; a line that cannot be read does not
       * @param fp the stream
       * @return true if it does
       */
      bool closes(FILE *fp) {
         try {
            return read_ket(fp);
         } catch (const ListCoreError &) {
            return false;
         }
      }

      /**
       * @brief Read a value line with sscanf()
       * @param fp the stream
       * @param format the format, with one conversion
       * @param value receives the value
       * @throw ListCoreError if the stream is null or the value cannot be read
       */
      template <typename V>
      void read_number(FILE *fp, const char *format, V *value) {
         char s[ReadLineLength + 1];
         if (fp == nullptr) {
            fail("reading a value from no stream");
         }
         relabel("reading a value", [&] { read_line(fp, s, ReadLineLength); });
         if (sscanf(s, format, value) != 1) {
            fail("reading a value");
         }
      }

      /**
       * @brief Read a descriptor entry `{ nm, mt }`; the slot offset is 0
       * @param fp the stream
       * @param dp receives the entry
       * @throw ListCoreError if the stream is null or the entry cannot be read
       */
      void read_descr_entry(FILE *fp, ocs::tom::FieldDescr *dp) {
         char s[ReadLineLength + 1];
         if (fp == nullptr) {
            fail("reading a descriptor entry from no stream");
         }
         relabel("reading a descriptor entry", [&] { read_line(fp, s, ReadLineLength); });
         int mt;
         int nm;
         char bra[2];
         char comma[2];
         char ket[2];
         // this form of sscanf skips the white space at the beginning
         if (sscanf(s, "%1s %d %1s %d %1s", bra, &nm, comma, &mt, ket) != 5) {
            fail("reading a descriptor entry");
         }
         if (bra[0] != '{' || comma[0] != ',' || ket[0] != '}') {
            fail("reading a descriptor entry");
         }
         dp->nm = nm;
         dp->mt = mt;
         dp->off = 0;
      }

      /**
       * @brief Read a quoted string; a string divided by a newline continues on the next lines
       * @param fp the stream
       * @return the string, allocated with malloc; an empty string for `""`
       * @throw ListCoreError if the stream is null, the text cannot be read or there is no memory
       */
      char *read_string(FILE *fp) {
         char line[ReadLineLength + 1];
         if (fp == nullptr) {
            fail("reading a string from no stream");
         }
         relabel("reading a string", [&] { read_line(fp, line, ReadLineLength); });
         const char *s = line;
         while (isspace(static_cast<int>(*s))) {
            s++;
         }
         if (*s++ != '"') {
            fail("reading a string that is not quoted");
         }
         std::string sp;
         int i;
         for (i = 0; s[i] != '\0' && s[i] != '"'; i++) {
            if (s[i] == '\\') {
               i++;
            }
            sp += s[i];
         }
         if (s[i] != '"') {
            // the string is divided by a newline
            bool done = false;
            while (!done) {
               relabel("reading the rest of a string", [&] { read_line(fp, line, ReadLineLength); });
               s = line;
               int j;
               for (j = 0; s[j] != '\0' && s[j] != '"'; j++) {
                  sp += s[j];
               }
               done = s[j] == '"';
            }
         }
         char *str = strdup(sp.c_str());
         if (str == nullptr) {
            fail("reading a string");
         }
         return str;
      }

      /**
       * @brief Read a quoted host name on one line
       * @param fp the stream
       * @return the name, allocated with malloc
       * @throw ListCoreError if the stream is null, the text cannot be read or there is no memory
       */
      char *read_host(FILE *fp) {
         char line[ReadLineLength + 1];
         if (fp == nullptr) {
            fail("reading a host name from no stream");
         }
         relabel("reading a host name", [&] { read_line(fp, line, ReadLineLength); });
         const char *s = line;
         while (isspace(static_cast<int>(*s))) {
            s++;
         }
         if (*s++ != '"') {
            fail("reading a host name that is not quoted");
         }
         int i;
         for (i = 0; s[i] != '\0' && s[i] != '"'; i++) {
         }
         if (s[i] != '"') {
            fail("reading a host name that is not closed");
         }
         char *host = strndup(s, i);
         if (host == nullptr) {
            fail("reading a host name");
         }
         return host;
      }

      /**
       * @brief Read a sub-list: `empty`, or a list
       * @param fp the stream
       * @return the list, or nullptr for `empty`
       * @throw ListCoreError if the stream is null or the sub-list cannot be read
       */
      ocs::tom::ListData *read_sub_list(FILE *fp) {
         char s[ReadLineLength + 1];
         if (fp == nullptr) {
            fail("reading a sub-list from no stream");
         }
         relabel("reading a sub-list", [&] { read_line(fp, s, ReadLineLength); });
         if (strstr(s, "empty") != nullptr) {
            return nullptr;
         }
         return relabel("reading a sub-list", [&] { return read_list(fp, nullptr, nullptr); });
      }

      /**
       * @brief Read a sub-object: `none`, or `object` and the object
       * @param fp the stream
       * @return the object, or nullptr for `none`
       * @throw ListCoreError if the stream is null or the sub-object cannot be read
       */
      ocs::tom::ElementData *read_sub_object(FILE *fp) {
         char s[ReadLineLength + 1];
         if (fp == nullptr) {
            fail("reading a sub-object from no stream");
         }
         relabel("reading a sub-object", [&] { read_line(fp, s, ReadLineLength); });
         if (strstr(s, "none") != nullptr) {
            return nullptr;
         }
         if (strstr(s, "object") == nullptr) {
            fail("reading a sub-object");
         }
         ocs::tom::ElementData *ep = relabel("reading a sub-object", [&] { return read_object(fp); });
         elem_set_object(ep);
         return ep;
      }

      /**
       * @brief Read the values of an element
       * @param fp the stream
       * @param ep the element, with its descriptor
       * @throw ListCoreError if a value cannot be read
       */
      void read_values(FILE *fp, ocs::tom::ElementData *ep) {
         const ocs::tom::FieldDescr *dp = ep->descr;
         for (const auto f : ocs::tom::present_fields(dp)) {
            ocs::tom::FieldValue &value = ep->cont[f.slot];
            switch (mt_get_type(dp[f.pos].mt)) {
               case MT_INT32:
                  read_number(fp, "%d", &(value.i));
                  break;
               case MT_UINT32:
                  read_number(fp, sge_u32, &(value.ul));
                  break;
               case MT_UINT64:
                  read_number(fp, sge_u64, &(value.ul64));
                  break;
               case MT_STRING:
               case MT_HOST: {
                  OwnedChars str(mt_get_type(dp[f.pos].mt) == MT_STRING ? read_string(fp) : read_host(fp));
                  store_string(ep, f.pos, str.get());
                  break;
               }
               case MT_DOUBLE:
                  read_number(fp, "%lf", &(value.db));
                  break;
               case MT_INT64:
                  read_number(fp, "%ld", &(value.l));
                  break;
               case MT_BOOL: {
                  int i = 0;
                  read_number(fp, "%d", &i);
                  value.b = i;
                  break;
               }
               case MT_REF: {
                  // a reference is not read back, but its line is skipped
                  uint32_t dummy;
                  read_number(fp, sge_u32, &dummy);
                  value.ref = nullptr;
                  break;
               }
               case MT_OBJECT:
                  value.obj = read_sub_object(fp);
                  break;
               case MT_LIST:
                  value.glp = read_sub_list(fp);
                  break;
               default:
                  fail("reading a value of an unknown type");
            }
         }
      }

      /**
       * @brief Read a descriptor
       * @param fp the stream
       * @return the descriptor, allocated with sge_malloc
       * @throw ListCoreError if the stream is null, the text cannot be read or there is no memory
       */
      ocs::tom::FieldDescr *read_descr(FILE *fp) {
         if (fp == nullptr) {
            fail("reading a descriptor from no stream");
         }
         if (!opens(fp)) {
            printf("bra is missing\n");
            fail("reading a descriptor");
         }
         int n = 0;
         try {
            read_number(fp, "%d", &n);
         } catch (const ListCoreError &) {
            printf("reading integer from dump file failed\n");
            fail("reading the number of fields of a descriptor");
         }
         if (n < 0) {
            fail("reading a descriptor with a negative number of fields");
         }
         std::vector<ocs::tom::FieldDescr> dp(static_cast<std::size_t>(n) + 1);
         for (int i = 0; i < n; i++) {
            relabel("reading a descriptor entry", [&] { read_descr_entry(fp, &(dp[i])); });
         }
         dp[n].nm = NoName;
         dp[n].mt = MT_END;
         dp[n].off = 0;
         if (!closes(fp)) {
            printf("ket is missing");
            fail("reading a descriptor");
         }

         // the full length of the type the fields belong to; they are listed in type order, as the values follow
         int first = 0;
         const ocs::tom::FieldDescr *type = n > 0 ? ocs::tom::descr_static_containing(dp[0].nm, &first) : nullptr;
         const int type_n = type != nullptr ? descr_length(type) : 0;
         std::vector<bool> keep(type_n, false);
         bool known = type != nullptr;
         int last = -1;
         for (int i = 0; known && i < n; i++) {
            const int pos = dp[i].nm - type[0].nm;
            known = pos > last && pos < type_n && type[pos].nm == dp[i].nm;
            if (known) {
               keep[pos] = true;
               last = pos;
            }
         }
         if (!known) {
            fail("reading a descriptor of no known type");
         }
         auto *out = ocs::uti::c_alloc<ocs::tom::FieldDescr>(type_n + 1);
         ocs::tom::descr_layout(type, keep, out);
         return out;
      }

      /**
       * @brief Read an element
       * @param fp the stream
       * @param dp the descriptor
       * @return the element
       * @throw ListCoreError if the stream or the descriptor is null, the text cannot be read or there is no memory
       */
      ocs::tom::ElementData *read_elem(FILE *fp, const ocs::tom::FieldDescr *dp) {
         if (fp == nullptr) {
            fail("reading an element from no stream");
         }
         if (dp == nullptr) {
            fail("reading an element without a descriptor");
         }
         OwnedElem ep;
         try {
            ep.reset(create_elem(dp));
         } catch (const std::exception &) {
            fail("reading an element");
         }
         if (lCountPresent(dp) <= 0) {
            fail("reading an element without fields");
         }
         if (!opens(fp)) {
            printf("bra is missing\n");
            fail("reading an element");
         }
         relabel("reading the values of an element", [&] { read_values(fp, ep.get()); });
         if (!closes(fp)) {
            printf("ket is missing\n");
            fail("reading an element");
         }
         return ep.release();
      }

      /**
       * @brief Read a sub-object with its descriptor
       * @param fp the stream
       * @return the object
       * @throw ListCoreError if the stream is null, the text cannot be read or there is no memory
       */
      ocs::tom::ElementData *read_object(FILE *fp) {
         if (fp == nullptr) {
            fail("reading an object from no stream");
         }
         if (!opens(fp)) {
            printf("bra is missing\n");
            fail("reading an object");
         }
         OwnedDescr dp(relabel("reading the descriptor of an object", [&] { return read_descr(fp); }));
         if (descr_length(dp.get()) <= 0) {
            fail("reading an object without fields");
         }
         OwnedElem ep(relabel("reading an object", [&] { return read_elem(fp, dp.get()); }));
         if (!closes(fp)) {
            printf("ket is missing\n");
            fail("reading an object");
         }
         return ep.release();
      }

      /**
       * @brief Read a list
       * @param fp the stream
       * @param name the name of the list, or nullptr for the one in the text
       * @param dp the descriptor of the list, or nullptr for the one in the text
       * @return the list
       * @throw ListCoreError as undump_list()
       */
      ocs::tom::ListData *read_list(FILE *fp, const char *name, const ocs::tom::FieldDescr *dp) {
         if (fp == nullptr) {
            fail("reading a list from no stream");
         }
         if (!opens(fp)) {
            printf("bra is missing\n");
            fail("reading a list");
         }
         OwnedChars oldname;
         try {
            oldname.reset(read_string(fp));
         } catch (const ListCoreError &) {
            printf("fGetString failed\n");
            fail("reading the name of a list");
         }
         int nelem = 0;
         try {
            read_number(fp, "%d", &nelem);
         } catch (const ListCoreError &) {
            printf("fGetInt failed\n");
            fail("reading the number of elements of a list");
         }
         OwnedDescr fdp(relabel("reading the descriptor of a list", [&] { return read_descr(fp); }));
         if (dp == nullptr) {
            // the descriptor of the text
            dp = fdp.get();
         }

         OwnedList lp;
         try {
            lp.reset(create_list(name != nullptr ? name : oldname.get(), dp, true));
         } catch (const std::exception &) {
            fail("reading a list");
         }
         const int n = descr_length(dp);
         if (n <= 0) {
            fail("reading a list without fields");
         }

         // the field of the text each field of the list takes its value from, -1 for none
         std::vector<int> found(n, -1);
         for (int j = 0; fdp.get()[j].nm != NoName; j++) {
            const ocs::tom::FieldDescr &from = fdp.get()[j];
            if (from.off == TOM_ABSENT) {
               continue;
            }
            for (int i = 0; i < n; i++) {
               if (dp[i].nm == from.nm && dp[i].off != TOM_ABSENT &&
                   (dp[i].mt & ~(MT_DESCR_INTERNED | MT_IS_REDUCED)) == (from.mt & ~MT_IS_REDUCED)) {
                  found[i] = j;
                  break;
               }
            }
         }

         for (int k = 0; k < nelem; k++) {
            OwnedElem fep(relabel("reading an element of a list", [&] { return read_elem(fp, fdp.get()); }));
            OwnedElem ep;
            try {
               ep.reset(create_elem(dp));
            } catch (const std::exception &) {
               fail("reading an element of a list");
            }
            for (int i = 0; i < n; i++) {
               if (found[i] != -1) {
                  try {
                     copy_field(fep.get(), ep.get(), found[i], i, true, nullptr, false);
                  } catch (const std::exception &) {
                     fail("copying a field of an element of a list");
                  }
               }
            }
            try {
               append_elem(lp.get(), ep.get());
            } catch (const std::exception &) {
               fail("appending an element to a list");
            }
            static_cast<void>(ep.release());
         }

         if (!closes(fp)) {
            printf("ket is missing\n");
            fail("reading a list");
         }
         return lp.release();
      }

      /**
       * @brief Throw the error of a writer whose last write failed
       * @param what what failed
       */
      [[noreturn, gnu::cold]] void write_failed(const char *what) {
         throw ListCoreError(what);
      }

   } // namespace

   void dump_descr(FILE *fp, const ocs::tom::FieldDescr *descr, const int indent) {
      if (write_descr(fp, descr, indent) != 0) {
         write_failed("dumping a descriptor");
      }
   }

   void dump_elem(FILE *fp, const ocs::tom::ElementData *ep, const int indent) {
      if (write_elem(fp, ep, indent) != 0) {
         write_failed("dumping an element");
      }
   }

   void dump_object(FILE *fp, const ocs::tom::ElementData *ep, const int indent) {
      if (write_object(fp, ep, indent) != 0) {
         write_failed("dumping an object");
      }
   }

   void dump_list(FILE *fp, const ocs::tom::ListData *lp, const int indent) {
      if (write_list(fp, lp, indent) != 0) {
         write_failed("dumping a list");
      }
   }

   void dump_elem_to(const char *path, const ocs::tom::ElementData *ep, const int indent) {
      FILE *fp = fopen(path, "w");
      if (fp == nullptr) {
         fail("opening a file to dump into");
      }
      int ret;
      try {
         ret = write_elem(fp, ep, indent);
      } catch (...) {
         fclose(fp);
         throw;
      }
      if (fclose(fp) != 0) {
         fail("closing a file dumped into");
      }
      if (ret != 0) {
         write_failed("dumping an element");
      }
   }

   ocs::tom::FieldDescr *undump_descr(FILE *fp) {
      return read_descr(fp);
   }

   ocs::tom::ElementData *undump_elem(FILE *fp, const ocs::tom::FieldDescr *descr) {
      return read_elem(fp, descr);
   }

   ocs::tom::ElementData *undump_object(FILE *fp) {
      return read_object(fp);
   }

   ocs::tom::ListData *undump_list(FILE *fp, const char *name, const ocs::tom::FieldDescr *descr) {
      return read_list(fp, name, descr);
   }

   ocs::tom::ElementData *undump_elem_from(const char *path, const ocs::tom::FieldDescr *descr) {
      FILE *fp = fopen(path, "r");
      if (fp == nullptr) {
         fail("opening a file to read a dump from");
      }
      // unlike before, the file is closed again
      std::unique_ptr<FILE, int (*)(FILE *)> closer(fp, fclose);
      return read_elem(fp, descr);
   }

   // ---------------------------------------------------------------- packed element files

   namespace {

      /**
       * @brief The file an element is written to or read from: prefix/name, or the one of the two given
       * @param prefix the directory, or nullptr
       * @param name the file name, or nullptr
       * @return the path
       */
      std::string element_file(const char *prefix, const char *name) {
         if (prefix != nullptr && name != nullptr) {
            return std::string(prefix) + "/" + name;
         }
         return prefix != nullptr ? prefix : name;
      }

      /**
       * @brief Time the I/O of the spooling for SGE_PROF_SPOOLINGIO, also when it throws
       */
      class SpoolingProfile {
      public:
         /** @brief Start the measurement */
         SpoolingProfile() {
            PROF_START_MEASUREMENT(SGE_PROF_SPOOLINGIO);
         }

         /** @brief Stop the measurement */
         ~SpoolingProfile() {
            PROF_STOP_MEASUREMENT(SGE_PROF_SPOOLINGIO);
         }

         SpoolingProfile(const SpoolingProfile &) = delete;
         SpoolingProfile &operator=(const SpoolingProfile &) = delete;
      };

      /** @brief Clears a pack buffer when it goes out of scope */
      struct BufferGuard {
         sge_pack_buffer *pb;   ///< the buffer

         /** @brief Clear it */
         ~BufferGuard() {
            clear_buffer(pb);
         }
      };

   } // namespace

   void write_elem_to_disk(const ocs::tom::ElementData *ep, const char *prefix, const char *name,
                           const char *obj_name) {
      DENTER(TOP_LAYER);

      if (prefix == nullptr && name == nullptr) {
         ERROR(SFNMAX, MSG_TOM_WRITETODISK_NOPREFIXANDNOFILENAME);
         throw ocs::RuntimeException(MSG_TOM_WRITETODISK_NOPREFIXANDNOFILENAME);
      }

      sge_pack_buffer pb{};
      BufferGuard guard{&pb};
      try {
         init_buffer(&pb, 8192, false, false);
         pack_elem(&pb, ep, nullptr, 0);
      } catch (const PackException &e) {
         switch (e.code()) {
            case PACK_ENOMEM:
               ERROR(MSG_TOM_NOTENOUGHMEMORYFORPACKINGXY_SS, obj_name, name ? name : "null");
               break;
            case PACK_FORMAT:
               ERROR(MSG_TOM_FORMATERRORWHILEPACKINGXY_SS, obj_name, name ? name : "null");
               break;
            default:
               ERROR(MSG_TOM_UNEXPECTEDERRORWHILEPACKINGXY_SS, obj_name, name ? name : "null");
               break;
         }
         throw;
      }

      const std::string filename = element_file(prefix, name);
      SpoolingProfile profile;
      const int fd = SGE_OPEN3(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
      if (fd < 0) {
         CRITICAL(MSG_TOM_CANTOPENXFORWRITINGOFYZ_SSS, filename.c_str(), obj_name, strerror(errno));
         throw ocs::RuntimeException("cannot open " + filename + " for writing");
      }
      if (sge_writenbytes(fd, pb.head_ptr, pb.bytes_used) != static_cast<int>(pb.bytes_used)) {
         CRITICAL(MSG_TOM_CANTWRITEXTOFILEY_SS, obj_name, filename.c_str());
         close(fd);
         throw ocs::RuntimeException("cannot write " + filename);
      }
      close(fd);

      DRETURN_VOID;
   }

   ocs::tom::ElementData *read_elem_from_disk(const char *prefix, const char *name, const ocs::tom::FieldDescr *descr,
                                              const char *obj_name) {
      DENTER(TOP_LAYER);

      if (prefix == nullptr && name == nullptr) {
         ERROR(SFNMAX, MSG_TOM_READFROMDISK_NOPREFIXANDNOFILENAME);
         throw ocs::RuntimeException(MSG_TOM_READFROMDISK_NOPREFIXANDNOFILENAME);
      }
      const std::string filename = element_file(prefix, name);

      // the size of the file
      SGE_STRUCT_STAT statbuf;
      if (SGE_STAT(filename.c_str(), &statbuf) == -1) {
         CRITICAL(MSG_TOM_CANTGETFILESTATFORXFILEY_SS, obj_name, filename.c_str());
         throw ocs::RuntimeException("cannot stat " + filename);
      }
      if (statbuf.st_size == 0) {
         CRITICAL(MSG_TOM_XFILEYHASZEROSIYE_SS, obj_name, filename.c_str());
         throw ocs::RuntimeException(filename + " is empty");
      }
      const auto size = static_cast<std::size_t>(statbuf.st_size);
      OwnedChars buf(static_cast<SGE_OFF_T>(size) == statbuf.st_size ? sge_malloc(size) : nullptr);
      if (buf == nullptr) {
         CRITICAL(SFNMAX, MSG_TOM_MALLOCFAILURE);
         throw ocs::RuntimeException(MSG_TOM_MALLOCFAILURE);
      }

      const int fd = SGE_OPEN2(filename.c_str(), O_RDONLY);
      if (fd < 0) {
         CRITICAL(MSG_TOM_CANTREADXFROMFILEY_SS, obj_name, filename.c_str());
         throw ocs::RuntimeException("cannot open " + filename + " for reading");
      }
      if (sge_readnbytes(fd, buf.get(), size) != static_cast<int>(size)) {
         CRITICAL(MSG_TOM_ERRORREADINGXINFILEY_SS, obj_name, filename.c_str());
         close(fd);
         throw ocs::RuntimeException("cannot read " + filename);
      }
      close(fd);

      // the buffer owns the bytes from here on; as before, a header that cannot be read is logged and the element
      // is read nonetheless
      sge_pack_buffer pb{};
      BufferGuard guard{&pb};
      try {
         init_buffer_from(&pb, buf.release(), static_cast<std::uint32_t>(size), false);
      } catch (const PackException &e) {
         ERROR(MSG_TOM_ERRORININITPACKBUFFER_S, pack_error_text(e.code()));
      }
      ocs::tom::ElementData *ep = nullptr;
      try {
         ep = unpack_elem(&pb, descr, 0);
      } catch (const PackException &e) {
         switch (e.code()) {
            case PACK_ENOMEM:
               ERROR(MSG_TOM_NOTENOUGHMEMORYFORUNPACKINGXY_SS, obj_name, filename.c_str());
               break;
            case PACK_FORMAT:
               ERROR(MSG_TOM_FORMATERRORWHILEUNPACKINGXY_SS, obj_name, filename.c_str());
               break;
            case PACK_BADARG:
               ERROR(MSG_TOM_BADARGUMENTWHILEUNPACKINGXY_SS, obj_name, filename.c_str());
               break;
            default:
               ERROR(MSG_TOM_UNEXPECTEDERRORWHILEUNPACKINGXY_SS, obj_name, filename.c_str());
               break;
         }
         throw;
      }

      DRETURN(ep);
   }

} // namespace ocs::tom
