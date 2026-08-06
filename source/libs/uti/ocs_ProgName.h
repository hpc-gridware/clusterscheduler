#pragma once
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

/** @file
 * @brief The canonical names of the cluster's programs
 */

#include <array>
#include <string_view>
#include <optional>
#include <cstddef>
#include <string>

// For those applications that should be handled with sge_options the entry has to be before ALL_OPT
/// The canonical program names, as an X-macro list: `X(enumerator, name)`
#define PROG_NAME_LIST(X)            \
   X(UNKNOWN_APP,   "unknown")       \
   X(QALTER,        "qalter")        \
   X(QCONF,         "qconf")         \
   X(QDEL,          "qdel")          \
   X(QHOLD,         "qhold")         \
   X(QMASTER,       "qmaster")       \
   X(QMOD,          "qmod")          \
   X(QRESUB,        "qresub")        \
   X(QRLS,          "qrls")          \
   X(QSELECT,       "qselect")       \
   X(QSH,           "qsh")           \
   X(QRSH,          "qrsh")          \
   X(QLOGIN,        "qlogin")        \
   X(QSTAT,         "qstat")         \
   X(QSUB,          "qsub")          \
   X(EXECD,         "execd")         \
   X(QEVENT,        "qevent")        \
   X(QRSUB,         "qrsub")         \
   X(QRDEL,         "qrdel")         \
   X(QRSTAT,        "qrstat")        \
   X(UNUSED_CONST,  "unknown")       \
   X(ALL_OPT,       "unknown")       \
   X(SCHEDD,        "schedd")        \
   X(QACCT,         "qacct")         \
   X(SHADOWD,       "shadowd")       \
   X(QHOST,         "qhost")         \
   X(SPOOLDEFAULTS, "spoolinit")     \
   X(JAPI,          "japi")          \
   X(DRMAA,         "drmaa")         \
   X(QPING,         "qping")         \
   X(QQUOTA,        "qquota")        \
   X(SGE_SHARE_MON, "sge_share_mon") \
   X(PYTHON_CLIENT, "python_client") \
   X(QMON,          "qmon")

/// One enumerator per entry of #PROG_NAME_LIST, in that order
enum ProgName {
/// @cond   transient X-macro expansion, undefined again below
#define X(name, str) name,
/// @endcond
   PROG_NAME_LIST(X)
#undef X
   PROGNAME_COUNT ///< number of program names, and the size of #prognames
};

/// The name of each #ProgName, indexed by the enumerator
constexpr std::array<std::string_view, PROGNAME_COUNT> prognames = {
/// @cond   transient X-macro expansion, undefined again below
#define X(name, str) str,
/// @endcond
   PROG_NAME_LIST(X)
#undef X
};

/**
 * @brief The name of a program
 *
 * @param p the program
 * @return its name, or an empty view when @p p is out of range
 */
constexpr std::string_view to_string_view(const ProgName p) {
   const auto idx = static_cast<std::size_t>(p);
   return idx < prognames.size() ? prognames[idx] : std::string_view{};
}

/**
 * @brief The name of a program, as a C string
 *
 * Out of range gives nullptr, mirroring #to_string_view above and `to_cstr()`
 * in `ocs_ThreadName.h`. Indexing the array directly aborted on
 * #PROGNAME_COUNT in a build with `_GLIBCXX_ASSERTIONS`, and read past the
 * array without it.
 *
 * @param p the program
 * @return its name, or nullptr when @p p is out of range; the pointer refers
 *         to static storage, do not free it
 */
constexpr const char *to_cstr(const ProgName p) {
   return to_string_view(p).data();
}

/**
 * @brief The name of a program, as a `std::string`
 *
 * @param p the program
 * @return its name, copied into a new string
 */
constexpr std::string to_string(const ProgName p) {
   return std::string(to_string_view(p));
}

/**
 * @brief Look a program up by name
 *
 * @param s the name to look for, matched exactly
 * @return the matching program, or an empty optional when the name is unknown
 */
constexpr std::optional<ProgName> from_string_to_ProgName(const std::string_view s) {
   for (std::size_t i = 0; i < prognames.size(); ++i) {
      if (prognames[i] == s) {
         return static_cast<ProgName>(i);
      }
   }
   return std::nullopt;
}
