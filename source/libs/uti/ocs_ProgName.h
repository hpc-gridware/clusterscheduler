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

#include <array>
#include <string_view>
#include <optional>
#include <cstddef>
#include <string>

// For those applications that should be handled with sge_options the entry has to be before ALL_OPT
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

enum ProgName {
#define X(name, str) name,
   PROG_NAME_LIST(X)
#undef X
   PROGNAME_COUNT
};

constexpr std::array<std::string_view, PROGNAME_COUNT> prognames = {
#define X(name, str) str,
   PROG_NAME_LIST(X)
#undef X
};

constexpr const char *to_cstr(const ProgName p) {
   return prognames[static_cast<std::size_t>(p)].data();
}

constexpr std::string_view to_string_view(const ProgName p) {
   const auto idx = static_cast<std::size_t>(p);
   return idx < prognames.size() ? prognames[idx] : std::string_view{};
}

constexpr std::string to_string(const ProgName p) {
   return std::string(to_string_view(p));
}

constexpr std::optional<ProgName> from_string_to_ProgName(const std::string_view s) {
   for (std::size_t i = 0; i < prognames.size(); ++i) {
      if (prognames[i] == s) {
         return static_cast<ProgName>(i);
      }
   }
   return std::nullopt;
}
