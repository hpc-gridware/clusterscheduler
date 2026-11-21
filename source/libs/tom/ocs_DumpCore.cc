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

#include "tom/detail/ocs_ErrnoP.h"
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
       * @brief Set the error number and throw it
       * @param lerrno the error number
       * @param what what failed
       */
      [[noreturn, gnu::cold]] void fail(const int lerrno, const char *what) {
         LERROR(lerrno);
         throw ListCoreError(lerrno, what);
      }

      /**
       * @brief Run a reader and give a failure in it the error number of the caller, as the old readers did when
       *        they returned
       * @param lerrno the error number of the caller
       * @param what what failed, for the message
       * @param read the reader
       * @return what the reader returns
       */
      template <typename Read>
      auto relabel(const int lerrno, const char *what, Read read) -> decltype(read()) {
         try {
            return read();
         } catch (const ListCoreError &) {
            fail(lerrno, what);
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
         void operator()(lListElem *ep) const noexcept {
            free_elem(ep);
         }
      };

      /** @brief Frees a list */
      struct ListFree {
         /**
          * @brief Free it
          * @param lp the list
          */
         void operator()(lList *lp) const noexcept {
            free_list(lp);
         }
      };

      using OwnedChars = std::unique_ptr<char, MallocFree>;     ///< a string allocated with malloc
      using OwnedDescr = std::unique_ptr<lDescr, MallocFree>;   ///< a descriptor allocated with sge_malloc
      using OwnedElem = std::unique_ptr<lListElem, ElemFree>;   ///< an element being read
      using OwnedList = std::unique_ptr<lList, ListFree>;       ///< a list being read

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

      int write_descr(FILE *fp, const lDescr *dp, int indent);
      int write_elem(FILE *fp, const lListElem *ep, int indent);
      int write_object(FILE *fp, const lListElem *ep, int indent);
      int write_list(FILE *fp, const lList *lp, int indent);
      lList *read_list(FILE *fp, const char *name, const lDescr *dp);
      lListElem *read_object(FILE *fp);

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
       * @throw ListCoreError with LEFILENULL or LEDESCRNULL
       */
      int write_descr(FILE *fp, const lDescr *dp, const int indent) {
         const std::string space = indentation(indent);
         if (fp == nullptr) {
            fail(LEFILENULL, "dumping a descriptor to no stream");
         }
         int ret = fprintf(fp, "%s{ /* DESCR BEGIN */\n", space.c_str());
         if (dp == nullptr) {
            fail(LEDESCRNULL, "dumping a null descriptor");
         }
         ret = fprintf(fp, "%s/* NUMBER OF DESCR FIELDS */ %d\n", space.c_str(), lCountPresent(dp));
         for (const auto f : ocs_cull_present_fields(dp)) {
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
       * @throw ListCoreError with LEFILENULL or LEELEMNULL
       */
      int write_elem(FILE *fp, const lListElem *ep, const int indent) {
         const std::string sp = indentation(indent);
         const char *space = sp.c_str();
         if (fp == nullptr) {
            fail(LEFILENULL, "dumping an element to no stream");
         }
         if (ep == nullptr) {
            fail(LEELEMNULL, "dumping a null element");
         }

         dstring dstr = DSTRING_INIT;
         int ret = fprintf(fp, "%s{ \n", space);
         ret = 0;
         for (const auto f : ocs_cull_present_fields(ep->descr)) {
            if (ret == EOF) {
               break;
            }
            const lMultiType &value = ocs_cull_value(ep, f.pos);
            const char *name = ocs::tom::field_name(ep->descr[f.pos].nm);
            switch (mt_get_type(ep->descr[f.pos].mt)) {
               case lIntT:
                  ret = fprintf(fp, "%s/* %-20.20s */ %d\n", space, name, value.i);
                  break;
               case lUlongT:
                  ret = fprintf(fp, "%s/* %-20.20s */ " sge_u32 "\n", space, name, value.ul);
                  break;
               case lUlong64T:
                  ret = fprintf(fp, "%s/* %-20.20s */ " sge_u64 "\n", space, name, value.ul64);
                  break;
               case lStringT: {
                  // a " inside the string is escaped
                  char *tok;
                  if ((tok = sge_strtok(value.str, "\"")) != nullptr) {
                     sge_dstring_append(&dstr, tok);
                     while ((tok = sge_strtok(nullptr, "\"")) != nullptr) {
                        sge_dstring_append(&dstr, "\\\"");
                        sge_dstring_append(&dstr, tok);
                     }
                  }
                  const char *str = sge_dstring_get_string(&dstr);
                  ret = fprintf(fp, "%s/* %-20.20s */ \"%s\"\n", space, name, str != nullptr ? str : "");
                  sge_dstring_clear(&dstr);
                  break;
               }
               case lHostT:
                  ret = fprintf(fp, "%s/* %-20.20s */ \"%s\"\n", space, name, value.host != nullptr ? value.host : "");
                  break;
               case lDoubleT:
                  ret = fprintf(fp, "%s/* %-20.20s */ %f\n", space, name, value.db);
                  break;
               case lLongT:
                  ret = fprintf(fp, "%s/* %-20.20s */%ld \n", space, name, value.l);
                  break;
               case lBoolT:
                  ret = fprintf(fp, "%s/* %-20.20s */ %d\n", space, name, value.b);
                  break;
               case lRefT:
                  ret = fprintf(fp, "%s/* %-20.20s */ %ld\n", space, name, reinterpret_cast<long>(value.ref));
                  break;
               case lObjectT:
                  if (value.obj == nullptr) {
                     ret = fprintf(fp, "%s/* %-20.20s */ none\n", space, name);
                  } else {
                     ret = fprintf(fp, "%s/* %-20.20s */ object\n", space, name);
                     if (ret != EOF) {
                        ret = write_object(fp, value.obj, indent + 1);
                     }
                  }
                  break;
               case lListT:
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
       * @throw ListCoreError with LEFILENULL or LEELEMNULL
       */
      int write_object(FILE *fp, const lListElem *ep, const int indent) {
         const std::string space = indentation(indent);
         if (fp == nullptr) {
            fail(LEFILENULL, "dumping an object to no stream");
         }
         if (ep == nullptr) {
            fail(LEELEMNULL, "dumping a null object");
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
       * @throw ListCoreError with LEFILENULL or LELISTNULL
       */
      int write_list(FILE *fp, const lList *lp, const int indent) {
         const std::string space = indentation(indent);
         if (fp == nullptr) {
            fail(LEFILENULL, "dumping a list to no stream");
         }
         if (lp == nullptr) {
            fail(LELISTNULL, "dumping a null list");
         }
         fprintf(fp, "%s{ /* LIST BEGIN */\n", space.c_str());
         if (lp->listname == nullptr) {
            // as lGetListName() reported it
            LERROR(LENULLSTRING);
         }
         fprintf(fp, "%s/* LISTNAME               */ \"%s\"\n", space.c_str(), list_name(lp));
         fprintf(fp, "%s/* NUMBER OF ELEMENTS     */ " sge_u32 "\n", space.c_str(), lp->nelem);
         int ret = write_descr(fp, lp->descr, indent);
         for (const lListElem *ep = lp->first; ep != nullptr && ret != EOF; ep = ep->next) {
            ret = write_elem(fp, ep, indent);
         }
         ret = fprintf(fp, "%s} /* LIST END */\n", space.c_str());
         return ret == EOF ? -1 : 0;
      }

      // ---------------------------------------------------------------- readers
      //
      // Every reader throws a ListCoreError after setting the error number of
      // before; a caller that set an error number of its own when the reader
      // returned an error does so through relabel(), so that the last error
      // number set is that of before.

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
       * @throw ListCoreError with LEFILENULL, LEFGETS or LESPACECOMMENT
       */
      void read_line(FILE *fp, char *line, const int max_line) {
         if (fp == nullptr) {
            fail(LEFILENULL, "reading a line from no stream");
         }
         if (fgets(line, max_line, fp) == nullptr) {
            fail(LEFGETS, "reading a line");
         }
         if (!blank_comments(line)) {
            fail(LESPACECOMMENT, "reading a line with a comment that is not closed");
         }
      }

      /**
       * @brief Read a line and tell whether it opens a block
       * @param fp the stream
       * @return true if the line holds a `{`
       * @throw ListCoreError with LEFGETLINE
       */
      bool read_bra(FILE *fp) {
         char s[ReadLineLength + 1];
         relabel(LEFGETLINE, "reading the opening of a block", [&] { read_line(fp, s, ReadLineLength); });
         return strstr(s, "{") != nullptr;
      }

      /**
       * @brief Read a line and tell whether it closes a block
       * @param fp the stream
       * @return true if the line holds a `}`
       * @throw ListCoreError with LEFGETLINE
       */
      bool read_ket(FILE *fp) {
         char s[ReadLineLength + 1];
         relabel(LEFGETLINE, "reading the closing of a block", [&] { read_line(fp, s, ReadLineLength); });
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
       * @throw ListCoreError with LEFILENULL, LEFGETLINE or LESSCANF
       */
      template <typename V>
      void read_number(FILE *fp, const char *format, V *value) {
         char s[ReadLineLength + 1];
         if (fp == nullptr) {
            fail(LEFILENULL, "reading a value from no stream");
         }
         relabel(LEFGETLINE, "reading a value", [&] { read_line(fp, s, ReadLineLength); });
         if (sscanf(s, format, value) != 1) {
            fail(LESSCANF, "reading a value");
         }
      }

      /**
       * @brief Read a descriptor entry `{ nm, mt }`; the slot offset is 0
       * @param fp the stream
       * @param dp receives the entry
       * @throw ListCoreError with LEFILENULL, LEFGETLINE, LESSCANF or LESYNTAX
       */
      void read_descr_entry(FILE *fp, lDescr *dp) {
         char s[ReadLineLength + 1];
         if (fp == nullptr) {
            fail(LEFILENULL, "reading a descriptor entry from no stream");
         }
         relabel(LEFGETLINE, "reading a descriptor entry", [&] { read_line(fp, s, ReadLineLength); });
         int mt;
         int nm;
         char bra[2];
         char comma[2];
         char ket[2];
         // this form of sscanf skips the white space at the beginning
         if (sscanf(s, "%1s %d %1s %d %1s", bra, &nm, comma, &mt, ket) != 5) {
            fail(LESSCANF, "reading a descriptor entry");
         }
         if (bra[0] != '{' || comma[0] != ',' || ket[0] != '}') {
            fail(LESYNTAX, "reading a descriptor entry");
         }
         dp->nm = nm;
         dp->mt = mt;
         dp->off = 0;
      }

      /**
       * @brief Read a quoted string; a string divided by a newline continues on the next lines
       * @param fp the stream
       * @return the string, allocated with malloc; an empty string for `""`
       * @throw ListCoreError with LEFILENULL, LEFGETLINE, LESYNTAX or LESTRDUP
       */
      char *read_string(FILE *fp) {
         char line[ReadLineLength + 1];
         if (fp == nullptr) {
            fail(LEFILENULL, "reading a string from no stream");
         }
         relabel(LEFGETLINE, "reading a string", [&] { read_line(fp, line, ReadLineLength); });
         const char *s = line;
         while (isspace(static_cast<int>(*s))) {
            s++;
         }
         if (*s++ != '"') {
            fail(LESYNTAX, "reading a string that is not quoted");
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
               relabel(LEFGETLINE, "reading the rest of a string", [&] { read_line(fp, line, ReadLineLength); });
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
            fail(LESTRDUP, "reading a string");
         }
         return str;
      }

      /**
       * @brief Read a quoted host name on one line
       * @param fp the stream
       * @return the name, allocated with malloc
       * @throw ListCoreError with LEFILENULL, LEFGETLINE, LESYNTAX or LESTRDUP
       */
      char *read_host(FILE *fp) {
         char line[ReadLineLength + 1];
         if (fp == nullptr) {
            fail(LEFILENULL, "reading a host name from no stream");
         }
         relabel(LEFGETLINE, "reading a host name", [&] { read_line(fp, line, ReadLineLength); });
         const char *s = line;
         while (isspace(static_cast<int>(*s))) {
            s++;
         }
         if (*s++ != '"') {
            fail(LESYNTAX, "reading a host name that is not quoted");
         }
         int i;
         for (i = 0; s[i] != '\0' && s[i] != '"'; i++) {
         }
         if (s[i] != '"') {
            fail(LESYNTAX, "reading a host name that is not closed");
         }
         char *host = strndup(s, i);
         if (host == nullptr) {
            fail(LESTRDUP, "reading a host name");
         }
         return host;
      }

      /**
       * @brief Read a sub-list: `empty`, or a list
       * @param fp the stream
       * @return the list, or nullptr for `empty`
       * @throw ListCoreError with LEFILENULL, LEFGETLINE or LEUNDUMPLIST
       */
      lList *read_sub_list(FILE *fp) {
         char s[ReadLineLength + 1];
         if (fp == nullptr) {
            fail(LEFILENULL, "reading a sub-list from no stream");
         }
         relabel(LEFGETLINE, "reading a sub-list", [&] { read_line(fp, s, ReadLineLength); });
         if (strstr(s, "empty") != nullptr) {
            return nullptr;
         }
         return relabel(LEUNDUMPLIST, "reading a sub-list", [&] { return read_list(fp, nullptr, nullptr); });
      }

      /**
       * @brief Read a sub-object: `none`, or `object` and the object
       * @param fp the stream
       * @return the object, or nullptr for `none`
       * @throw ListCoreError with LEFILENULL, LEFGETLINE, LESYNTAX or LEUNDUMPELEM
       */
      lListElem *read_sub_object(FILE *fp) {
         char s[ReadLineLength + 1];
         if (fp == nullptr) {
            fail(LEFILENULL, "reading a sub-object from no stream");
         }
         relabel(LEFGETLINE, "reading a sub-object", [&] { read_line(fp, s, ReadLineLength); });
         if (strstr(s, "none") != nullptr) {
            return nullptr;
         }
         if (strstr(s, "object") == nullptr) {
            fail(LESYNTAX, "reading a sub-object");
         }
         lListElem *ep = relabel(LEUNDUMPELEM, "reading a sub-object", [&] { return read_object(fp); });
         elem_set_object(ep);
         return ep;
      }

      /**
       * @brief Read the values of an element
       * @param fp the stream
       * @param ep the element, with its descriptor
       * @throw ListCoreError with the error number of the value that failed
       */
      void read_values(FILE *fp, lListElem *ep) {
         const lDescr *dp = ep->descr;
         for (const auto f : ocs_cull_present_fields(dp)) {
            lMultiType &value = ep->cont[f.slot];
            switch (mt_get_type(dp[f.pos].mt)) {
               case lIntT:
                  read_number(fp, "%d", &(value.i));
                  break;
               case lUlongT:
                  read_number(fp, sge_u32, &(value.ul));
                  break;
               case lUlong64T:
                  read_number(fp, sge_u64, &(value.ul64));
                  break;
               case lStringT:
               case lHostT: {
                  OwnedChars str(mt_get_type(dp[f.pos].mt) == lStringT ? read_string(fp) : read_host(fp));
                  store_string(ep, f.pos, str.get());
                  break;
               }
               case lDoubleT:
                  read_number(fp, "%lf", &(value.db));
                  break;
               case lLongT:
                  read_number(fp, "%ld", &(value.l));
                  break;
               case lBoolT: {
                  int i = 0;
                  read_number(fp, "%d", &i);
                  value.b = i;
                  break;
               }
               case lRefT: {
                  // a reference is not read back, but its line is skipped
                  lUlong dummy;
                  read_number(fp, sge_u32, &dummy);
                  value.ref = nullptr;
                  break;
               }
               case lObjectT:
                  value.obj = read_sub_object(fp);
                  break;
               case lListT:
                  value.glp = read_sub_list(fp);
                  break;
               default:
                  unknownType("lUndumpElemFp");
                  fail(LEFIELDREAD, "reading a value of an unknown type");
            }
         }
      }

      /**
       * @brief Read a descriptor
       * @param fp the stream
       * @return the descriptor, allocated with sge_malloc
       * @throw ListCoreError with LEFILENULL, LESYNTAX, LEFIELDREAD, LEFGETDESCR or LEMALLOC
       */
      lDescr *read_descr(FILE *fp) {
         if (fp == nullptr) {
            fail(LEFILENULL, "reading a descriptor from no stream");
         }
         if (!opens(fp)) {
            printf("bra is missing\n");
            fail(LESYNTAX, "reading a descriptor");
         }
         int n = 0;
         try {
            read_number(fp, "%d", &n);
         } catch (const ListCoreError &) {
            printf("reading integer from dump file failed\n");
            fail(LEFIELDREAD, "reading the number of fields of a descriptor");
         }
         if (n < 0) {
            fail(LEMALLOC, "reading a descriptor with a negative number of fields");
         }
         std::vector<lDescr> dp(static_cast<std::size_t>(n) + 1);
         for (int i = 0; i < n; i++) {
            relabel(LEFGETDESCR, "reading a descriptor entry", [&] { read_descr_entry(fp, &(dp[i])); });
         }
         dp[n].nm = NoName;
         dp[n].mt = lEndT;
         dp[n].off = 0;
         if (!closes(fp)) {
            printf("ket is missing");
            fail(LESYNTAX, "reading a descriptor");
         }

         // the full length of the type the fields belong to; they are listed in type order, as the values follow
         int first = 0;
         const lDescr *type = n > 0 ? ocs_cull_descr_static_containing(dp[0].nm, &first) : nullptr;
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
            fail(LEFGETDESCR, "reading a descriptor of no known type");
         }
         auto *out = reinterpret_cast<lDescr *>(sge_malloc(sizeof(lDescr) * (type_n + 1)));
         if (out == nullptr) {
            fail(LEMALLOC, "reading a descriptor");
         }
         ocs_cull_descr_layout(type, keep, out);
         return out;
      }

      /**
       * @brief Read an element
       * @param fp the stream
       * @param dp the descriptor
       * @return the element
       * @throw ListCoreError with LEFILENULL, LEDESCRNULL, LECREATEELEM, LECOUNTDESCR, LESYNTAX or LEFIELDREAD
       */
      lListElem *read_elem(FILE *fp, const lDescr *dp) {
         if (fp == nullptr) {
            fail(LEFILENULL, "reading an element from no stream");
         }
         if (dp == nullptr) {
            fail(LEDESCRNULL, "reading an element without a descriptor");
         }
         OwnedElem ep;
         try {
            ep.reset(create_elem(dp));
         } catch (const std::exception &) {
            fail(LECREATEELEM, "reading an element");
         }
         if (lCountPresent(dp) <= 0) {
            fail(LECOUNTDESCR, "reading an element without fields");
         }
         if (!opens(fp)) {
            printf("bra is missing\n");
            fail(LESYNTAX, "reading an element");
         }
         relabel(LEFIELDREAD, "reading the values of an element", [&] { read_values(fp, ep.get()); });
         if (!closes(fp)) {
            printf("ket is missing\n");
            fail(LESYNTAX, "reading an element");
         }
         return ep.release();
      }

      /**
       * @brief Read a sub-object with its descriptor
       * @param fp the stream
       * @return the object
       * @throw ListCoreError with LEFILENULL, LESYNTAX, LEFGETDESCR, LECOUNTDESCR or LEUNDUMPELEM
       */
      lListElem *read_object(FILE *fp) {
         if (fp == nullptr) {
            fail(LEFILENULL, "reading an object from no stream");
         }
         if (!opens(fp)) {
            printf("bra is missing\n");
            fail(LESYNTAX, "reading an object");
         }
         OwnedDescr dp(relabel(LEFGETDESCR, "reading the descriptor of an object", [&] { return read_descr(fp); }));
         if (descr_length(dp.get()) <= 0) {
            fail(LECOUNTDESCR, "reading an object without fields");
         }
         OwnedElem ep(relabel(LEUNDUMPELEM, "reading an object", [&] { return read_elem(fp, dp.get()); }));
         if (!closes(fp)) {
            printf("ket is missing\n");
            fail(LESYNTAX, "reading an object");
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
      lList *read_list(FILE *fp, const char *name, const lDescr *dp) {
         if (fp == nullptr) {
            fail(LEFILENULL, "reading a list from no stream");
         }
         if (!opens(fp)) {
            printf("bra is missing\n");
            fail(LESYNTAX, "reading a list");
         }
         OwnedChars oldname;
         try {
            oldname.reset(read_string(fp));
         } catch (const ListCoreError &) {
            printf("fGetString failed\n");
            fail(LEFIELDREAD, "reading the name of a list");
         }
         int nelem = 0;
         try {
            read_number(fp, "%d", &nelem);
         } catch (const ListCoreError &) {
            printf("fGetInt failed\n");
            fail(LEFIELDREAD, "reading the number of elements of a list");
         }
         OwnedDescr fdp(relabel(LEFGETDESCR, "reading the descriptor of a list", [&] { return read_descr(fp); }));
         if (dp == nullptr) {
            // the descriptor of the text
            dp = fdp.get();
         }

         OwnedList lp;
         try {
            lp.reset(create_list(name != nullptr ? name : oldname.get(), dp, true));
         } catch (const std::exception &) {
            fail(LECREATELIST, "reading a list");
         }
         const int n = descr_length(dp);
         if (n <= 0) {
            fail(LECOUNTDESCR, "reading a list without fields");
         }

         // the field of the text each field of the list takes its value from, -1 for none
         std::vector<int> found(n, -1);
         for (int j = 0; fdp.get()[j].nm != NoName; j++) {
            const lDescr &from = fdp.get()[j];
            if (from.off == OCS_CULL_ABSENT) {
               continue;
            }
            for (int i = 0; i < n; i++) {
               if (dp[i].nm == from.nm && dp[i].off != OCS_CULL_ABSENT &&
                   (dp[i].mt & ~(CULL_DESCR_INTERNED | CULL_IS_REDUCED)) == (from.mt & ~CULL_IS_REDUCED)) {
                  found[i] = j;
                  break;
               }
            }
         }

         for (int k = 0; k < nelem; k++) {
            OwnedElem fep(relabel(LEUNDUMPELEM, "reading an element of a list", [&] { return read_elem(fp, fdp.get()); }));
            OwnedElem ep;
            try {
               ep.reset(create_elem(dp));
            } catch (const std::exception &) {
               fail(LECREATEELEM, "reading an element of a list");
            }
            for (int i = 0; i < n; i++) {
               if (found[i] != -1) {
                  try {
                     copy_field(fep.get(), ep.get(), found[i], i, true, nullptr, false);
                  } catch (const std::exception &) {
                     fail(LECOPYSWITCH, "copying a field of an element of a list");
                  }
               }
            }
            try {
               append_elem(lp.get(), ep.get());
            } catch (const std::exception &) {
               fail(LEAPPENDELEM, "appending an element to a list");
            }
            static_cast<void>(ep.release());
         }

         if (!closes(fp)) {
            printf("ket is missing\n");
            fail(LESYNTAX, "reading a list");
         }
         return lp.release();
      }

      /**
       * @brief Throw the error of a writer whose last write failed, without an error number, as before
       * @param what what failed
       */
      [[noreturn, gnu::cold]] void write_failed(const char *what) {
         throw ListCoreError(0, what);
      }

   } // namespace

   void dump_descr(FILE *fp, const lDescr *descr, const int indent) {
      if (write_descr(fp, descr, indent) != 0) {
         write_failed("dumping a descriptor");
      }
   }

   void dump_elem(FILE *fp, const lListElem *ep, const int indent) {
      if (write_elem(fp, ep, indent) != 0) {
         write_failed("dumping an element");
      }
   }

   void dump_object(FILE *fp, const lListElem *ep, const int indent) {
      if (write_object(fp, ep, indent) != 0) {
         write_failed("dumping an object");
      }
   }

   void dump_list(FILE *fp, const lList *lp, const int indent) {
      if (write_list(fp, lp, indent) != 0) {
         write_failed("dumping a list");
      }
   }

   void dump_elem_to(const char *path, const lListElem *ep, const int indent) {
      FILE *fp = fopen(path, "w");
      if (fp == nullptr) {
         fail(LEOPEN, "opening a file to dump into");
      }
      int ret;
      try {
         ret = write_elem(fp, ep, indent);
      } catch (...) {
         fclose(fp);
         throw;
      }
      if (fclose(fp) != 0) {
         fail(LECLOSE, "closing a file dumped into");
      }
      if (ret != 0) {
         write_failed("dumping an element");
      }
   }

   lDescr *undump_descr(FILE *fp) {
      return read_descr(fp);
   }

   lListElem *undump_elem(FILE *fp, const lDescr *descr) {
      return read_elem(fp, descr);
   }

   lListElem *undump_object(FILE *fp) {
      return read_object(fp);
   }

   lList *undump_list(FILE *fp, const char *name, const lDescr *descr) {
      return read_list(fp, name, descr);
   }

   lListElem *undump_elem_from(const char *path, const lDescr *descr) {
      FILE *fp = fopen(path, "r");
      if (fp == nullptr) {
         fail(LEOPEN, "opening a file to read a dump from");
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

   void write_elem_to_disk(const lListElem *ep, const char *prefix, const char *name, const char *obj_name) {
      DENTER(TOP_LAYER);

      if (prefix == nullptr && name == nullptr) {
         ERROR(SFNMAX, MSG_CULL_NOPREFIXANDNOFILENAMEINWRITEELMTODISK);
         throw ocs::RuntimeException(MSG_CULL_NOPREFIXANDNOFILENAMEINWRITEELMTODISK);
      }

      sge_pack_buffer pb{};
      BufferGuard guard{&pb};
      try {
         init_buffer(&pb, 8192, false, false);
         pack_elem(&pb, ep, nullptr, 0);
      } catch (const PackException &e) {
         switch (e.code()) {
            case PACK_ENOMEM:
               ERROR(MSG_CULL_NOTENOUGHMEMORYFORPACKINGXY_SS, obj_name, name ? name : "null");
               break;
            case PACK_FORMAT:
               ERROR(MSG_CULL_FORMATERRORWHILEPACKINGXY_SS, obj_name, name ? name : "null");
               break;
            default:
               ERROR(MSG_CULL_UNEXPECTEDERRORWHILEPACKINGXY_SS, obj_name, name ? name : "null");
               break;
         }
         throw;
      }

      const std::string filename = element_file(prefix, name);
      SpoolingProfile profile;
      const int fd = SGE_OPEN3(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
      if (fd < 0) {
         CRITICAL(MSG_CULL_CANTOPENXFORWRITINGOFYZ_SSS, filename.c_str(), obj_name, strerror(errno));
         throw ocs::RuntimeException("cannot open " + filename + " for writing");
      }
      if (sge_writenbytes(fd, pb.head_ptr, pb.bytes_used) != static_cast<int>(pb.bytes_used)) {
         CRITICAL(MSG_CULL_CANTWRITEXTOFILEY_SS, obj_name, filename.c_str());
         close(fd);
         throw ocs::RuntimeException("cannot write " + filename);
      }
      close(fd);

      DRETURN_VOID;
   }

   lListElem *read_elem_from_disk(const char *prefix, const char *name, const lDescr *descr, const char *obj_name) {
      DENTER(TOP_LAYER);

      if (prefix == nullptr && name == nullptr) {
         ERROR(SFNMAX, MSG_CULL_NOPREFIXANDNOFILENAMEINREADELEMFROMDISK);
         throw ocs::RuntimeException(MSG_CULL_NOPREFIXANDNOFILENAMEINREADELEMFROMDISK);
      }
      const std::string filename = element_file(prefix, name);

      // the size of the file
      SGE_STRUCT_STAT statbuf;
      if (SGE_STAT(filename.c_str(), &statbuf) == -1) {
         CRITICAL(MSG_CULL_CANTGETFILESTATFORXFILEY_SS, obj_name, filename.c_str());
         throw ocs::RuntimeException("cannot stat " + filename);
      }
      if (statbuf.st_size == 0) {
         CRITICAL(MSG_CULL_XFILEYHASZEROSIYE_SS, obj_name, filename.c_str());
         throw ocs::RuntimeException(filename + " is empty");
      }
      const auto size = static_cast<std::size_t>(statbuf.st_size);
      OwnedChars buf(static_cast<SGE_OFF_T>(size) == statbuf.st_size ? sge_malloc(size) : nullptr);
      if (buf == nullptr) {
         CRITICAL(SFNMAX, MSG_CULL_LEMALLOC);
         throw ocs::RuntimeException(MSG_CULL_LEMALLOC);
      }

      const int fd = SGE_OPEN2(filename.c_str(), O_RDONLY);
      if (fd < 0) {
         CRITICAL(MSG_CULL_CANTREADXFROMFILEY_SS, obj_name, filename.c_str());
         throw ocs::RuntimeException("cannot open " + filename + " for reading");
      }
      if (sge_readnbytes(fd, buf.get(), size) != static_cast<int>(size)) {
         CRITICAL(MSG_CULL_ERRORREADINGXINFILEY_SS, obj_name, filename.c_str());
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
         ERROR(MSG_CULL_ERRORININITPACKBUFFER_S, pack_error_text(e.code()));
      }
      lListElem *ep = nullptr;
      try {
         ep = unpack_elem(&pb, descr, 0);
      } catch (const PackException &e) {
         switch (e.code()) {
            case PACK_ENOMEM:
               ERROR(MSG_CULL_NOTENOUGHMEMORYFORUNPACKINGXY_SS, obj_name, filename.c_str());
               break;
            case PACK_FORMAT:
               ERROR(MSG_CULL_FORMATERRORWHILEUNPACKINGXY_SS, obj_name, filename.c_str());
               break;
            case PACK_BADARG:
               ERROR(MSG_CULL_BADARGUMENTWHILEUNPACKINGXY_SS, obj_name, filename.c_str());
               break;
            default:
               ERROR(MSG_CULL_UNEXPECTEDERRORWHILEUNPACKINGXY_SS, obj_name, filename.c_str());
               break;
         }
         throw;
      }

      DRETURN(ep);
   }

} // namespace ocs::tom
