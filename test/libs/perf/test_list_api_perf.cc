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

/**
 * @file
 * @brief Performance of the list API, one source for both release lines
 *
 * The same catalogue of operations is measured with the old list library CULL (9.1) and with the typed object
 * model TOM (9.2). Every operation is a function of the same name in both variants, works on the same data and
 * does the same work; only the calls differ. The variant is chosen at compile time: TOM if OCS_PERF_API_TOM is
 * defined or, without OCS_PERF_API_CULL, if the header tom/ocs_Element.h exists; CULL otherwise.
 *
 * TOM is measured in two forms, chosen with `-mode typed|raw|both` (default both):
 * - typed: the members of the typed handles (ElementPtr<T>, Element<T>, List<T>, ...), fields at compile time
 * - raw: the forms on raw pointers and raw handles with run-time checked fields: the free forms of
 *   tom/ocs_FreeAccess.h and tom/ocs_Element.h on lListElem * / lList *, the raw API of tom/ocs_Raw.h, and where
 *   only a function with a run-time field number or sort order exists, the core functions of tom/ocs_*Core.h
 * The CULL variant accepts the switch and ignores it.
 *
 * Output on stdout, one CSV line per operation and form: `api,operation,calls,ns_per_op` with api `cull`,
 * `tom-typed` or `tom-raw`. ns_per_op is the measured time divided by the calls. Operations that need fresh data
 * per call (sorting, splitting, adding to a sub-list, ...) time only the operation, not the preparation.
 * Compare two or three such files with the lab script gcs-list-api-perf-compare.
 */

#if defined(OCS_PERF_API_TOM) || (!defined(OCS_PERF_API_CULL) && __has_include("tom/ocs_Element.h"))
#define OCS_PERF_TOM 1
#else
#define OCS_PERF_TOM 0
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

#if OCS_PERF_TOM
#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_ListCore.h"
#include "tom/ocs_ListOpsCore.h"
#include "tom/ocs_Model.h"
#include "tom/ocs_Pack.h"
#include "tom/ocs_PackCore.h"
#include "tom/ocs_Raw.h"
#include "tom/ocs_Select.h"
#include "tom/ocs_SelectCore.h"
#include "tom/ocs_Sort.h"
#include "tom/ocs_SortCore.h"
#else
#include "cull/cull.h"
#include "sgeobj/cull/sge_all_listsL.h"
#endif
#include "uti/sge_stdlib.h"

// ---------------------------------------------------------------------------------------------------------------
// harness

/// the time each operation is measured for
static constexpr double target_ns = 3.0e8;

/// the time the calibration loop of an operation must reach, which also warms it up
static constexpr double calibration_ns = 2.0e7;

/// the wall clock time after which an operation with preparation per round stops, even below target_ns
static constexpr double max_wall_ns = 4.0e9;

/// the form an operation is measured in
enum class Form {
   Cull,    ///< the old list library (9.1)
   Typed,   ///< TOM, members of the typed handles
   Raw      ///< TOM, forms on raw pointers and raw handles with run-time checked fields
};

/// the api column of the current form
static const char *g_api = "cull";

/// consumer of every value read, so that the compiler cannot drop the work
static std::uint64_t g_sink = 0;

using Clock = std::chrono::steady_clock;

/**
 * @brief The nanoseconds since a point in time
 * @param start the point in time
 * @return the nanoseconds since @p start
 */
static double elapsed_ns(const Clock::time_point start) {
   return std::chrono::duration<double, std::nano>(Clock::now() - start).count();
}

/**
 * @brief Print the CSV line of an operation
 * @param op the name of the operation
 * @param calls the number of calls measured
 * @param ns_per_op the nanoseconds per call
 */
static void report(const std::string &op, const std::uint64_t calls, const double ns_per_op) {
   printf("%s,%s,%" PRIu64 ",%.2f\n", g_api, op.c_str(), calls, ns_per_op);
   fflush(stdout);
}

/**
 * @brief Measure an operation that needs no preparation per call
 *
 * The number of calls is doubled until a loop takes calibration_ns, which also warms caches and branch
 * predictors up; then the operation runs as often as fits into target_ns. Each call ends in a compiler barrier,
 * so that no call is hoisted out of the loop or merged with the next one.
 *
 * @tparam Body the operation, called with the index of the call
 * @param op the name of the operation
 * @param body the operation
 */
template <typename Body>
static void run(const std::string &op, Body &&body) {
   std::uint64_t n = 1;
   double ns = 0.0;
   for (;;) {
      const auto start = Clock::now();
      for (std::uint64_t i = 0; i < n; i++) {
         body(i);
         std::atomic_signal_fence(std::memory_order_seq_cst);
      }
      ns = elapsed_ns(start);
      if (ns >= calibration_ns) {
         break;
      }
      n *= 2;
   }
   const auto calls = std::max<std::uint64_t>(1, static_cast<std::uint64_t>(static_cast<double>(n) * target_ns / ns));
   const auto start = Clock::now();
   for (std::uint64_t i = 0; i < calls; i++) {
      body(i);
      std::atomic_signal_fence(std::memory_order_seq_cst);
   }
   ns = elapsed_ns(start);
   report(op, calls, ns / static_cast<double>(calls));
}

/**
 * @brief Measure an operation that needs fresh data per round
 *
 * Each round prepares the data, times @p batch calls of the operation and cleans up; only the calls are timed.
 * Rounds are repeated until the timed part reaches calibration_ns (warm-up, not reported) and then target_ns,
 * or until max_wall_ns have passed.
 *
 * @tparam Setup the preparation of a round
 * @tparam Body the operation, called with the index of the call within the round
 * @tparam Teardown the cleanup of a round
 * @param op the name of the operation
 * @param batch the calls per round
 * @param setup the preparation of a round
 * @param body the operation
 * @param teardown the cleanup of a round
 */
template <typename Setup, typename Body, typename Teardown>
static void run_batched(const std::string &op, const std::size_t batch, Setup &&setup, Body &&body,
                        Teardown &&teardown) {
   const auto round = [&]() {
      setup();
      const auto start = Clock::now();
      for (std::size_t i = 0; i < batch; i++) {
         body(static_cast<std::uint64_t>(i));
         std::atomic_signal_fence(std::memory_order_seq_cst);
      }
      const double ns = elapsed_ns(start);
      teardown();
      return ns;
   };
   double warm = 0.0;
   const auto warm_start = Clock::now();
   while (warm < calibration_ns && elapsed_ns(warm_start) < max_wall_ns / 4) {
      warm += round();
   }
   double total = 0.0;
   std::uint64_t rounds = 0;
   const auto wall_start = Clock::now();
   while (total < target_ns && elapsed_ns(wall_start) < max_wall_ns) {
      total += round();
      rounds++;
   }
   const std::uint64_t calls = rounds * batch;
   report(op, calls, total / static_cast<double>(calls));
}

// ---------------------------------------------------------------------------------------------------------------
// data set, identical in both variants

/// the length of a short list, below the hash threshold of 16
static constexpr std::size_t short_n = 8;

/// the length of a long list, hashed
static constexpr std::size_t long_n = 1000;

/// the index of the key looked up in a short list
static constexpr std::size_t short_hit = 4;

/// the index of the key looked up in a long list
static constexpr std::size_t long_hit = 500;

/// the elements added per round to a short sub-list, so that it stays below 16
static constexpr std::size_t add_short = 7;

/// the elements added per round to a long sub-list
static constexpr std::size_t add_long = 100;

/// first task number of the keys added
static constexpr std::uint32_t new_u32 = 900000;

/// first time of the 64 bit keys of the data set
static constexpr std::uint64_t base_u64 = 5000000000ULL;

/// first time of the 64 bit keys added
static constexpr std::uint64_t new_u64 = 9000000000ULL;

/// a 32 bit key no element holds
static constexpr std::uint32_t miss_u32 = 0x7fffffff;

/// a 64 bit key no element holds
static constexpr std::uint64_t miss_u64 = 0x7fffffffffffffffULL;

/// a string key no element holds
static const char *const miss_str = "missing";

/// a host name no element holds
static const char *const miss_host = "missing.example.com";

/// the size of a list or sub-list
enum class Size {
   Short,   ///< short_n elements
   Long     ///< long_n elements
};

/// the key type of a lookup, add or delete
enum class Kind {
   Str,    ///< string key
   U32,    ///< 32 bit key
   U64,    ///< 64 bit key
   Host    ///< host key
};

static std::vector<std::string> g_names;       ///< name0000 ... (CE_name, JB_job_name, ST_name)
static std::vector<std::string> g_hosts;       ///< host0000.example.com ... (EH_name, QU_qhostname)
static std::vector<std::string> g_vars;        ///< VAR0000 ... (VA_variable)
static std::vector<std::string> g_owners;      ///< user0 ... user9 (JB_owner)
static std::vector<std::string> g_qnames;      ///< q00 ... q99 (QU_qname)
static std::vector<std::string> g_new_names;   ///< new0000 ... keys added
static std::vector<std::string> g_new_hosts;   ///< newhost0000.example.com ... host keys added

/**
 * @brief Fill the key tables
 */
static void init_keys() {
   char buf[64];
   for (std::size_t i = 0; i < long_n; i++) {
      snprintf(buf, sizeof(buf), "name%04zu", i);
      g_names.emplace_back(buf);
      snprintf(buf, sizeof(buf), "host%04zu.example.com", i);
      g_hosts.emplace_back(buf);
      snprintf(buf, sizeof(buf), "VAR%04zu", i);
      g_vars.emplace_back(buf);
      snprintf(buf, sizeof(buf), "new%04zu", i);
      g_new_names.emplace_back(buf);
      snprintf(buf, sizeof(buf), "newhost%04zu.example.com", i);
      g_new_hosts.emplace_back(buf);
   }
   for (std::size_t i = 0; i < 10; i++) {
      g_owners.emplace_back("user" + std::to_string(i));
   }
   for (std::size_t i = 0; i < 100; i++) {
      snprintf(buf, sizeof(buf), "q%02zu", i);
      g_qnames.emplace_back(buf);
   }
}

/**
 * @brief The index of the key looked up in a list of a size
 * @param size the size
 * @return short_hit or long_hit
 */
static std::size_t hit_of(const Size size) {
   return size == Size::Short ? short_hit : long_hit;
}

/**
 * @brief The elements added per round to a sub-list of a size
 * @param size the size
 * @return add_short or add_long
 */
static std::size_t add_of(const Size size) {
   return size == Size::Short ? add_short : add_long;
}

/**
 * @brief The name of a parametrised operation
 * @param base the operation family, e.g. "sub_find"
 * @param kind the key type
 * @param size the list size
 * @param suffix appended after an underscore, or nullptr
 * @return e.g. "sub_find_str_short_hit"
 */
static std::string op_name(const char *base, const Kind kind, const Size size, const char *suffix) {
   static const char *const kinds[] = {"str", "u32", "u64", "host"};
   std::string name = std::string(base) + "_" + kinds[static_cast<int>(kind)] + "_" +
                      (size == Size::Short ? "short" : "long");
   if (suffix != nullptr) {
      name += "_";
      name += suffix;
   }
   return name;
}

/**
 * @brief The priority of the job with an index: a fixed permutation, so a list of jobs is unsorted
 * @param i the index
 * @return the priority, 0 to 1023
 */
static std::uint32_t job_priority(const std::uint32_t i) {
   return (i * 7919U) % 1024U;
}

/**
 * @brief The submission time of the job with an index, unsorted
 * @param i the index
 * @return the time
 */
static std::uint64_t job_submission(const std::uint32_t i) {
   return 1700000000000ULL + (static_cast<std::uint64_t>(i) * 104729ULL) % 100000ULL;
}

/**
 * @brief The job name of the job with an index, a permutation of the names
 * @param i the index
 * @return the name
 */
static const char *job_name(const std::uint32_t i) {
   return g_names[(i * 577U) % long_n].c_str();
}

/**
 * @brief The owner of the job with an index
 * @param i the index
 * @return one of ten owners
 */
static const char *job_owner(const std::uint32_t i) {
   return g_owners[(i * 3U) % 10U].c_str();
}

// ---------------------------------------------------------------------------------------------------------------
// fixture

#if OCS_PERF_TOM

using ocs::tom::CE;
using ocs::tom::CK;
using ocs::tom::CQ;
using ocs::tom::EH;
using ocs::tom::Element;
using ocs::tom::ElementPtr;
using ocs::tom::JAT;
using ocs::tom::JB;
using ocs::tom::List;
using ocs::tom::ListPtr;
using ocs::tom::QU;
using ocs::tom::RDE;
using ocs::tom::RUE;
using ocs::tom::ST;
using ocs::tom::VA;

/// the key argument of a field as the raw forms take it: a number, or a C string for a string or a host field
template <auto F>
using Key = ocs::tom::KeyArgOf<ocs::tom::FieldTraits<F>::type>;

/**
 * @brief The key of a field as the typed members take it: a host name becomes an ocs::tom::Host
 * @tparam F the key field
 * @param key the key as the raw forms take it
 * @return the key as the typed members take it
 */
template <auto F>
static auto typed_key(const Key<F> key) {
   if constexpr (ocs::tom::FieldTraits<F>::type == ocs::tom::FieldType::Host) {
      return ocs::tom::Host(key);
   } else {
      return key;
   }
}

/// the data set, built once
struct Fixture {
   ListPtr<JB> jobs_short;    ///< short_n plain jobs
   ListPtr<JB> jobs_long;     ///< long_n plain jobs, unsorted priority, owner, name, submission time
   ListPtr<JB> jobs_100;      ///< 100 plain jobs
   ListPtr<JB> jobs_pack;     ///< 100 jobs with 10 env entries, 10 tasks and a checkpoint object each
   ListPtr<JAT> tasks_short;  ///< short_n tasks, JAT_task_number 1...
   ListPtr<JAT> tasks_long;   ///< long_n tasks
   ListPtr<RDE> rde_short;    ///< short_n diagram entries, RDE_time base_u64...
   ListPtr<RDE> rde_long;     ///< long_n diagram entries
   ListPtr<CE> ce_short;      ///< short_n complex entries, CE_name g_names
   ListPtr<CE> ce_long;       ///< long_n complex entries
   ListPtr<EH> eh_short;      ///< short_n hosts, EH_name g_hosts
   ListPtr<EH> eh_long;       ///< long_n hosts
   ListPtr<QU> qu_short;      ///< short_n queue instances on 2 hosts and 2 queue names
   ListPtr<QU> qu_long;       ///< long_n queue instances on 100 hosts and 100 queue names
   ListPtr<ST> st_dup;        ///< long_n strings, 100 distinct
   ListPtr<VA> va_add;        ///< short_n variables new0000 ... for lAddSubList()
   ElementPtr<JB> job;        ///< a job with 10 env entries, 10 tasks and a checkpoint object
   ElementPtr<JB> scratch_a;  ///< a job like job, changed by the operations
   ElementPtr<JB> scratch_b;  ///< a job like job, changed by the operations
   ElementPtr<JB> sub_short;  ///< a job with short_n env entries and tasks
   ElementPtr<JB> sub_long;   ///< a job with long_n env entries and tasks
   ElementPtr<RUE> rue_short; ///< a resource utilization with short_n diagram entries
   ElementPtr<RUE> rue_long;  ///< a resource utilization with long_n diagram entries
   ElementPtr<CQ> cq_short;   ///< a cluster queue with short_n queue instances on distinct hosts
   ElementPtr<CQ> cq_long;    ///< a cluster queue with long_n queue instances on distinct hosts
   ElementPtr<EH> host;       ///< a host
   ElementPtr<CE> ce;         ///< a complex entry
   ElementPtr<QU> qu;         ///< a queue instance
};

/**
 * @brief Set the plain fields of a job
 * @param job the job
 * @param i the index of the job
 */
static void fill_job(const Element<JB> job, const std::uint32_t i) {
   job.set<JB_job_number>(i + 1);
   job.set<JB_job_name>(job_name(i));
   job.set<JB_owner>(job_owner(i));
   job.set<JB_project>("project");
   job.set<JB_priority>(job_priority(i));
   job.set<JB_submission_time>(job_submission(i));
   job.set<JB_urg>(i * 0.5);
   job.set<JB_notify>((i % 2) != 0);
}

/**
 * @brief Create a job, with sub-lists and a checkpoint object if asked for
 * @param i the index of the job
 * @param n_env the env entries, VA_variable g_vars; with any a checkpoint object is added
 * @param n_tasks the tasks, JAT_task_number 1...
 * @return the job
 */
static ElementPtr<JB> make_job(const std::uint32_t i, const std::size_t n_env, const std::size_t n_tasks) {
   auto job = ElementPtr<JB>::create();
   fill_job(*job, i);
   if (n_env > 0) {
      const List<VA> env = job->get_or_create<JB_env_list>("env");
      for (std::size_t k = 0; k < n_env; k++) {
         env.emplace_back<VA_variable>(g_vars[k].c_str()).set<VA_value>("value");
      }
      auto ckpt = ElementPtr<CK>::create();
      ckpt->set<CK_name>("ckpt");
      job->set<JB_checkpoint_object>(std::move(ckpt));
   }
   if (n_tasks > 0) {
      const List<JAT> tasks = job->get_or_create<JB_ja_tasks>("tasks");
      for (std::size_t k = 0; k < n_tasks; k++) {
         tasks.emplace_back<JAT_task_number>(static_cast<std::uint32_t>(k + 1));
      }
   }
   return job;
}

/**
 * @brief Create a list of elements, each filled before it is appended
 * @tparam T the object type
 * @tparam Fill the filling, called with the element and its index
 * @param name the name of the list
 * @param n the number of elements
 * @param fill the filling
 * @return the list
 */
template <ocs::tom::ObjectType T, typename Fill>
static ListPtr<T> make_list(const char *name, const std::size_t n, Fill &&fill) {
   auto list = ListPtr<T>::create(name);
   for (std::size_t i = 0; i < n; i++) {
      auto ep = ElementPtr<T>::create();
      fill(*ep, static_cast<std::uint32_t>(i));
      list->push_back(std::move(ep));
   }
   return list;
}

/**
 * @brief Create a list of jobs
 * @param n the number of jobs
 * @param n_env the env entries per job
 * @param n_tasks the tasks per job
 * @return the list
 */
static ListPtr<JB> make_jobs(const std::size_t n, const std::size_t n_env, const std::size_t n_tasks) {
   auto list = ListPtr<JB>::create("jobs");
   for (std::size_t i = 0; i < n; i++) {
      list->push_back(make_job(static_cast<std::uint32_t>(i), n_env, n_tasks));
   }
   return list;
}

/**
 * @brief Create a resource utilization with diagram entries
 * @param n the number of diagram entries, RDE_time base_u64...
 * @return the resource utilization
 */
static ElementPtr<RUE> make_rue(const std::size_t n) {
   auto rue = ElementPtr<RUE>::create();
   rue->set<RUE_name>("slots");
   const List<RDE> rdes = rue->get_or_create<RUE_utilized>("utilized");
   for (std::size_t k = 0; k < n; k++) {
      rdes.emplace_back<RDE_time>(base_u64 + k).set<RDE_amount>(1.0);
   }
   return rue;
}

/**
 * @brief Create a cluster queue with queue instances on distinct hosts
 * @param n the number of queue instances, QU_qhostname g_hosts
 * @return the cluster queue
 */
static ElementPtr<CQ> make_cq(const std::size_t n) {
   auto cq = ElementPtr<CQ>::create();
   cq->set<CQ_name>("all.q");
   const List<QU> qis = cq->get_or_create<CQ_qinstances>("qinstances");
   for (std::size_t k = 0; k < n; k++) {
      qis.emplace_back<QU_qhostname>(ocs::tom::Host(g_hosts[k].c_str())).set<QU_qname>("all.q");
   }
   return cq;
}

/**
 * @brief Build the data set
 * @param f the fixture to fill
 */
static void build_fixture(Fixture &f) {
   f.jobs_short = make_jobs(short_n, 0, 0);
   f.jobs_long = make_jobs(long_n, 0, 0);
   f.jobs_100 = make_jobs(100, 0, 0);
   f.jobs_pack = make_jobs(100, 10, 10);
   const auto fill_task = [](const Element<JAT> e, const std::uint32_t i) { e.set<JAT_task_number>(i + 1); };
   f.tasks_short = make_list<JAT>("tasks", short_n, fill_task);
   f.tasks_long = make_list<JAT>("tasks", long_n, fill_task);
   const auto fill_rde = [](const Element<RDE> e, const std::uint32_t i) {
      e.set<RDE_time>(base_u64 + i);
      e.set<RDE_amount>(1.0);
   };
   f.rde_short = make_list<RDE>("rde", short_n, fill_rde);
   f.rde_long = make_list<RDE>("rde", long_n, fill_rde);
   const auto fill_ce = [](const Element<CE> e, const std::uint32_t i) {
      e.set<CE_name>(g_names[i].c_str());
      e.set<CE_doubleval>(1.0);
   };
   f.ce_short = make_list<CE>("ce", short_n, fill_ce);
   f.ce_long = make_list<CE>("ce", long_n, fill_ce);
   const auto fill_eh = [](const Element<EH> e, const std::uint32_t i) {
      e.set<EH_name>(ocs::tom::Host(g_hosts[i].c_str()));
   };
   f.eh_short = make_list<EH>("eh", short_n, fill_eh);
   f.eh_long = make_list<EH>("eh", long_n, fill_eh);
   f.qu_short = make_list<QU>("qu", short_n, [](const Element<QU> e, const std::uint32_t i) {
      e.set<QU_qhostname>(ocs::tom::Host(g_hosts[i % 2].c_str()));
      e.set<QU_qname>(g_qnames[i % 2].c_str());
   });
   f.qu_long = make_list<QU>("qu", long_n, [](const Element<QU> e, const std::uint32_t i) {
      e.set<QU_qhostname>(ocs::tom::Host(g_hosts[i % 100].c_str()));
      e.set<QU_qname>(g_qnames[i % 100].c_str());
   });
   f.st_dup = make_list<ST>("st", long_n, [](const Element<ST> e, const std::uint32_t i) {
      e.set<ST_name>(g_names[(i * 7U) % 100U].c_str());
   });
   f.va_add = make_list<VA>("add", short_n, [](const Element<VA> e, const std::uint32_t i) {
      e.set<VA_variable>(g_new_names[i].c_str());
      e.set<VA_value>("value");
   });
   f.job = make_job(0, 10, 10);
   f.scratch_a = make_job(1, 10, 10);
   f.scratch_b = make_job(2, 10, 10);
   f.sub_short = make_job(0, short_n, short_n);
   f.sub_long = make_job(0, long_n, long_n);
   f.rue_short = make_rue(short_n);
   f.rue_long = make_rue(long_n);
   f.cq_short = make_cq(short_n);
   f.cq_long = make_cq(long_n);
   f.host = ElementPtr<EH>::create();
   f.host->set<EH_name>(ocs::tom::Host(g_hosts[0].c_str()));
   f.ce = ElementPtr<CE>::create();
   f.ce->set<CE_name>(g_names[0].c_str());
   f.ce->set<CE_doubleval>(1.0);
   f.qu = ElementPtr<QU>::create();
   f.qu->set<QU_qhostname>(ocs::tom::Host(g_hosts[0].c_str()));
}

/**
 * @brief Free the data set
 * @param f the fixture, freed by its owners
 */
static void free_fixture(Fixture &f) {
   static_cast<void>(f);
}

#else

/// the data set, built once
struct Fixture {
   lList *jobs_short;      ///< short_n plain jobs
   lList *jobs_long;       ///< long_n plain jobs, unsorted priority, owner, name, submission time
   lList *jobs_100;        ///< 100 plain jobs
   lList *jobs_pack;       ///< 100 jobs with 10 env entries, 10 tasks and a checkpoint object each
   lList *tasks_short;     ///< short_n tasks, JAT_task_number 1...
   lList *tasks_long;      ///< long_n tasks
   lList *rde_short;       ///< short_n diagram entries, RDE_time base_u64...
   lList *rde_long;        ///< long_n diagram entries
   lList *ce_short;        ///< short_n complex entries, CE_name g_names
   lList *ce_long;         ///< long_n complex entries
   lList *eh_short;        ///< short_n hosts, EH_name g_hosts
   lList *eh_long;         ///< long_n hosts
   lList *qu_short;        ///< short_n queue instances on 2 hosts and 2 queue names
   lList *qu_long;         ///< long_n queue instances on 100 hosts and 100 queue names
   lList *st_dup;          ///< long_n strings, 100 distinct
   lList *va_add;          ///< short_n variables new0000 ... for lAddSubList()
   lListElem *job;         ///< a job with 10 env entries, 10 tasks and a checkpoint object
   lListElem *scratch_a;   ///< a job like job, changed by the operations
   lListElem *scratch_b;   ///< a job like job, changed by the operations
   lListElem *sub_short;   ///< a job with short_n env entries and tasks
   lListElem *sub_long;    ///< a job with long_n env entries and tasks
   lListElem *rue_short;   ///< a resource utilization with short_n diagram entries
   lListElem *rue_long;    ///< a resource utilization with long_n diagram entries
   lListElem *cq_short;    ///< a cluster queue with short_n queue instances on distinct hosts
   lListElem *cq_long;     ///< a cluster queue with long_n queue instances on distinct hosts
   lListElem *host;        ///< a host
   lListElem *ce;          ///< a complex entry
   lListElem *qu;          ///< a queue instance
};

/**
 * @brief Set the plain fields of a job
 * @param job the job
 * @param i the index of the job
 */
static void fill_job(lListElem *job, const std::uint32_t i) {
   lSetUlong(job, JB_job_number, i + 1);
   lSetString(job, JB_job_name, job_name(i));
   lSetString(job, JB_owner, job_owner(i));
   lSetString(job, JB_project, "project");
   lSetUlong(job, JB_priority, job_priority(i));
   lSetUlong64(job, JB_submission_time, job_submission(i));
   lSetDouble(job, JB_urg, i * 0.5);
   lSetBool(job, JB_notify, (i % 2) != 0);
}

/**
 * @brief Create a job, with sub-lists and a checkpoint object if asked for
 * @param i the index of the job
 * @param n_env the env entries, VA_variable g_vars; with any a checkpoint object is added
 * @param n_tasks the tasks, JAT_task_number 1...
 * @return the job
 */
static lListElem *make_job(const std::uint32_t i, const std::size_t n_env, const std::size_t n_tasks) {
   lListElem *job = lCreateElem(JB_Type);
   fill_job(job, i);
   if (n_env > 0) {
      lList *env = lCreateList("env", VA_Type);
      for (std::size_t k = 0; k < n_env; k++) {
         lListElem *ep = lCreateElem(VA_Type);
         lSetString(ep, VA_variable, g_vars[k].c_str());
         lSetString(ep, VA_value, "value");
         lAppendElem(env, ep);
      }
      lSetList(job, JB_env_list, env);
      lListElem *ckpt = lCreateElem(CK_Type);
      lSetString(ckpt, CK_name, "ckpt");
      lSetObject(job, JB_checkpoint_object, ckpt);
   }
   if (n_tasks > 0) {
      lList *tasks = lCreateList("tasks", JAT_Type);
      for (std::size_t k = 0; k < n_tasks; k++) {
         lListElem *ep = lCreateElem(JAT_Type);
         lSetUlong(ep, JAT_task_number, static_cast<u_long32>(k + 1));
         lAppendElem(tasks, ep);
      }
      lSetList(job, JB_ja_tasks, tasks);
   }
   return job;
}

/**
 * @brief Create a list of elements, each filled before it is appended
 * @tparam Fill the filling, called with the element and its index
 * @param name the name of the list
 * @param descr the descriptor of the elements
 * @param n the number of elements
 * @param fill the filling
 * @return the list
 */
template <typename Fill>
static lList *make_list(const char *name, const lDescr *descr, const std::size_t n, Fill &&fill) {
   lList *list = lCreateList(name, descr);
   for (std::size_t i = 0; i < n; i++) {
      lListElem *ep = lCreateElem(descr);
      fill(ep, static_cast<std::uint32_t>(i));
      lAppendElem(list, ep);
   }
   return list;
}

/**
 * @brief Create a list of jobs
 * @param n the number of jobs
 * @param n_env the env entries per job
 * @param n_tasks the tasks per job
 * @return the list
 */
static lList *make_jobs(const std::size_t n, const std::size_t n_env, const std::size_t n_tasks) {
   lList *list = lCreateList("jobs", JB_Type);
   for (std::size_t i = 0; i < n; i++) {
      lAppendElem(list, make_job(static_cast<std::uint32_t>(i), n_env, n_tasks));
   }
   return list;
}

/**
 * @brief Create a resource utilization with diagram entries
 * @param n the number of diagram entries, RDE_time base_u64...
 * @return the resource utilization
 */
static lListElem *make_rue(const std::size_t n) {
   lListElem *rue = lCreateElem(RUE_Type);
   lSetString(rue, RUE_name, "slots");
   lSetList(rue, RUE_utilized, make_list("utilized", RDE_Type, n, [](lListElem *ep, const std::uint32_t k) {
               lSetUlong64(ep, RDE_time, base_u64 + k);
               lSetDouble(ep, RDE_amount, 1.0);
            }));
   return rue;
}

/**
 * @brief Create a cluster queue with queue instances on distinct hosts
 * @param n the number of queue instances, QU_qhostname g_hosts
 * @return the cluster queue
 */
static lListElem *make_cq(const std::size_t n) {
   lListElem *cq = lCreateElem(CQ_Type);
   lSetString(cq, CQ_name, "all.q");
   lSetList(cq, CQ_qinstances, make_list("qinstances", QU_Type, n, [](lListElem *ep, const std::uint32_t k) {
               lSetHost(ep, QU_qhostname, g_hosts[k].c_str());
               lSetString(ep, QU_qname, "all.q");
            }));
   return cq;
}

/**
 * @brief Build the data set
 * @param f the fixture to fill
 */
static void build_fixture(Fixture &f) {
   f.jobs_short = make_jobs(short_n, 0, 0);
   f.jobs_long = make_jobs(long_n, 0, 0);
   f.jobs_100 = make_jobs(100, 0, 0);
   f.jobs_pack = make_jobs(100, 10, 10);
   const auto fill_task = [](lListElem *ep, const std::uint32_t i) { lSetUlong(ep, JAT_task_number, i + 1); };
   f.tasks_short = make_list("tasks", JAT_Type, short_n, fill_task);
   f.tasks_long = make_list("tasks", JAT_Type, long_n, fill_task);
   const auto fill_rde = [](lListElem *ep, const std::uint32_t i) {
      lSetUlong64(ep, RDE_time, base_u64 + i);
      lSetDouble(ep, RDE_amount, 1.0);
   };
   f.rde_short = make_list("rde", RDE_Type, short_n, fill_rde);
   f.rde_long = make_list("rde", RDE_Type, long_n, fill_rde);
   const auto fill_ce = [](lListElem *ep, const std::uint32_t i) {
      lSetString(ep, CE_name, g_names[i].c_str());
      lSetDouble(ep, CE_doubleval, 1.0);
   };
   f.ce_short = make_list("ce", CE_Type, short_n, fill_ce);
   f.ce_long = make_list("ce", CE_Type, long_n, fill_ce);
   const auto fill_eh = [](lListElem *ep, const std::uint32_t i) { lSetHost(ep, EH_name, g_hosts[i].c_str()); };
   f.eh_short = make_list("eh", EH_Type, short_n, fill_eh);
   f.eh_long = make_list("eh", EH_Type, long_n, fill_eh);
   f.qu_short = make_list("qu", QU_Type, short_n, [](lListElem *ep, const std::uint32_t i) {
      lSetHost(ep, QU_qhostname, g_hosts[i % 2].c_str());
      lSetString(ep, QU_qname, g_qnames[i % 2].c_str());
   });
   f.qu_long = make_list("qu", QU_Type, long_n, [](lListElem *ep, const std::uint32_t i) {
      lSetHost(ep, QU_qhostname, g_hosts[i % 100].c_str());
      lSetString(ep, QU_qname, g_qnames[i % 100].c_str());
   });
   f.st_dup = make_list("st", ST_Type, long_n, [](lListElem *ep, const std::uint32_t i) {
      lSetString(ep, ST_name, g_names[(i * 7U) % 100U].c_str());
   });
   f.va_add = make_list("add", VA_Type, short_n, [](lListElem *ep, const std::uint32_t i) {
      lSetString(ep, VA_variable, g_new_names[i].c_str());
      lSetString(ep, VA_value, "value");
   });
   f.job = make_job(0, 10, 10);
   f.scratch_a = make_job(1, 10, 10);
   f.scratch_b = make_job(2, 10, 10);
   f.sub_short = make_job(0, short_n, short_n);
   f.sub_long = make_job(0, long_n, long_n);
   f.rue_short = make_rue(short_n);
   f.rue_long = make_rue(long_n);
   f.cq_short = make_cq(short_n);
   f.cq_long = make_cq(long_n);
   f.host = lCreateElem(EH_Type);
   lSetHost(f.host, EH_name, g_hosts[0].c_str());
   f.ce = lCreateElem(CE_Type);
   lSetString(f.ce, CE_name, g_names[0].c_str());
   lSetDouble(f.ce, CE_doubleval, 1.0);
   f.qu = lCreateElem(QU_Type);
   lSetHost(f.qu, QU_qhostname, g_hosts[0].c_str());
}

/**
 * @brief Free the data set
 * @param f the fixture
 */
static void free_fixture(Fixture &f) {
   for (lList **lpp : {&f.jobs_short, &f.jobs_long, &f.jobs_100, &f.jobs_pack, &f.tasks_short, &f.tasks_long,
                       &f.rde_short, &f.rde_long, &f.ce_short, &f.ce_long, &f.eh_short, &f.eh_long, &f.qu_short,
                       &f.qu_long, &f.st_dup, &f.va_add}) {
      lFreeList(lpp);
   }
   for (lListElem **epp : {&f.job, &f.scratch_a, &f.scratch_b, &f.sub_short, &f.sub_long, &f.rue_short,
                           &f.rue_long, &f.cq_short, &f.cq_long, &f.host, &f.ce, &f.qu}) {
      lFreeElem(epp);
   }
}

#endif

// ---------------------------------------------------------------------------------------------------------------
// group: create and free

/**
 * @brief Create and free a job element (lCreateElem/lFreeElem)
 * @param f the fixture
 * @param form the form measured
 */
static void op_elem_create_free_jb(Fixture &f, const Form form) {
   static_cast<void>(f);
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      run("elem_create_free_jb", [&](std::uint64_t) {
         auto ep = ElementPtr<JB>::create();
         g_sink += ep ? 1 : 0;
      });
   } else {
      run("elem_create_free_jb", [&](std::uint64_t) {
         auto ep = ocs::tom::create_raw_elem(JB::descriptor.e);
         g_sink += ep ? 1 : 0;
      });
   }
#else
   static_cast<void>(form);
   run("elem_create_free_jb", [&](std::uint64_t) {
      lListElem *ep = lCreateElem(JB_Type);
      g_sink += ep != nullptr ? 1 : 0;
      lFreeElem(&ep);
   });
#endif
}

/**
 * @brief Create and free a complex entry element (lCreateElem/lFreeElem)
 * @param f the fixture
 * @param form the form measured
 */
static void op_elem_create_free_ce(Fixture &f, const Form form) {
   static_cast<void>(f);
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      run("elem_create_free_ce", [&](std::uint64_t) {
         auto ep = ElementPtr<CE>::create();
         g_sink += ep ? 1 : 0;
      });
   } else {
      run("elem_create_free_ce", [&](std::uint64_t) {
         auto ep = ocs::tom::create_raw_elem(CE::descriptor.e);
         g_sink += ep ? 1 : 0;
      });
   }
#else
   static_cast<void>(form);
   run("elem_create_free_ce", [&](std::uint64_t) {
      lListElem *ep = lCreateElem(CE_Type);
      g_sink += ep != nullptr ? 1 : 0;
      lFreeElem(&ep);
   });
#endif
}

/**
 * @brief Create and free an empty hashed job list (lCreateList/lFreeList)
 * @param f the fixture
 * @param form the form measured
 */
static void op_list_create_free(Fixture &f, const Form form) {
   static_cast<void>(f);
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      run("list_create_free", [&](std::uint64_t) {
         auto lp = ListPtr<JB>::create("jobs");
         g_sink += lp ? 1 : 0;
      });
   } else {
      run("list_create_free", [&](std::uint64_t) {
         auto lp = ocs::tom::create_raw_list(JB::descriptor.e, ocs::tom::Hashing::On, "jobs");
         g_sink += lp ? 1 : 0;
      });
   }
#else
   static_cast<void>(form);
   run("list_create_free", [&](std::uint64_t) {
      lList *lp = lCreateList("jobs", JB_Type);
      g_sink += lp != nullptr ? 1 : 0;
      lFreeList(&lp);
   });
#endif
}

/**
 * @brief Create a complex entry list, append 100 named elements, free it (lCreateElem/lSetString/lAppendElem)
 * @param f the fixture
 * @param form the form measured
 */
static void op_list_fill_free_100(Fixture &f, const Form form) {
   static_cast<void>(f);
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      run("list_fill_free_100", [&](std::uint64_t) {
         auto lp = ListPtr<CE>::create("ce");
         for (std::size_t k = 0; k < 100; k++) {
            auto ep = ElementPtr<CE>::create();
            ep->set<CE_name>(g_names[k].c_str());
            lp->push_back(std::move(ep));
         }
         g_sink += lp->size();
      });
   } else {
      run("list_fill_free_100", [&](std::uint64_t) {
         auto lp = ocs::tom::create_raw_list(CE::descriptor.e, ocs::tom::Hashing::On, "ce");
         for (std::size_t k = 0; k < 100; k++) {
            auto ep = ocs::tom::create_raw_elem(CE::descriptor.e);
            ocs::tom::set<CE_name>(ep.get().raw(), g_names[k].c_str());
            ocs::tom::append(lp.get().raw(), std::move(ep));
         }
         g_sink += ocs::tom::size(lp.get().raw());
      });
   }
#else
   static_cast<void>(form);
   run("list_fill_free_100", [&](std::uint64_t) {
      lList *lp = lCreateList("ce", CE_Type);
      for (std::size_t k = 0; k < 100; k++) {
         lListElem *ep = lCreateElem(CE_Type);
         lSetString(ep, CE_name, g_names[k].c_str());
         lAppendElem(lp, ep);
      }
      g_sink += lGetNumberOfElem(lp);
      lFreeList(&lp);
   });
#endif
}

// ---------------------------------------------------------------------------------------------------------------
// group: reading and writing fields

/**
 * @brief Read a 32 bit field (lGetUlong)
 * @param f the fixture
 * @param form the form measured
 */
static void op_get_u32(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<JB> job = *f.job;
      run("get_u32", [&](std::uint64_t) { g_sink += job.get<JB_job_number>(); });
   } else {
      const lListElem *ep = f.job.get().raw();
      run("get_u32", [&](std::uint64_t) { g_sink += ocs::tom::get<JB_job_number>(ep); });
   }
#else
   static_cast<void>(form);
   const lListElem *ep = f.job;
   run("get_u32", [&](std::uint64_t) { g_sink += lGetUlong(ep, JB_job_number); });
#endif
}

/**
 * @brief Write a 32 bit field that is not hashed (lSetUlong)
 * @param f the fixture
 * @param form the form measured
 */
static void op_set_u32(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<JB> job = *f.job;
      run("set_u32", [&](std::uint64_t i) { job.set<JB_priority>(static_cast<std::uint32_t>(i & 1023)); });
   } else {
      lListElem *ep = f.job.get().raw();
      run("set_u32", [&](std::uint64_t i) { ocs::tom::set<JB_priority>(ep, static_cast<std::uint32_t>(i & 1023)); });
   }
#else
   static_cast<void>(form);
   lListElem *ep = f.job;
   run("set_u32", [&](std::uint64_t i) { lSetUlong(ep, JB_priority, static_cast<u_long32>(i & 1023)); });
#endif
}

/**
 * @brief Write the hashed key of an element in a long list, which updates the hash table (lSetUlong)
 * @param f the fixture
 * @param form the form measured
 */
static void op_set_u32_hashed(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<JB> job = f.jobs_long->find_first<JB_job_number>(long_hit + 1);
      run("set_u32_hashed", [&](std::uint64_t i) {
         job.set<JB_job_number>(static_cast<std::uint32_t>(2000000 + (i & 1)));
      });
      job.set<JB_job_number>(long_hit + 1);
   } else {
      lListElem *ep = ocs::tom::find_first_rw<JB_job_number>(f.jobs_long.get().raw(), long_hit + 1);
      run("set_u32_hashed", [&](std::uint64_t i) {
         ocs::tom::set<JB_job_number>(ep, static_cast<std::uint32_t>(2000000 + (i & 1)));
      });
      ocs::tom::set<JB_job_number>(ep, static_cast<std::uint32_t>(long_hit + 1));
   }
#else
   static_cast<void>(form);
   lListElem *ep = lGetElemUlongRW(f.jobs_long, JB_job_number, long_hit + 1);
   run("set_u32_hashed", [&](std::uint64_t i) {
      lSetUlong(ep, JB_job_number, static_cast<u_long32>(2000000 + (i & 1)));
   });
   lSetUlong(ep, JB_job_number, long_hit + 1);
#endif
}

/**
 * @brief Read a 64 bit field (lGetUlong64)
 * @param f the fixture
 * @param form the form measured
 */
static void op_get_u64(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<JB> job = *f.job;
      run("get_u64", [&](std::uint64_t) { g_sink += job.get<JB_submission_time>(); });
   } else {
      const lListElem *ep = f.job.get().raw();
      run("get_u64", [&](std::uint64_t) { g_sink += ocs::tom::get<JB_submission_time>(ep); });
   }
#else
   static_cast<void>(form);
   const lListElem *ep = f.job;
   run("get_u64", [&](std::uint64_t) { g_sink += lGetUlong64(ep, JB_submission_time); });
#endif
}

/**
 * @brief Write a 64 bit field (lSetUlong64)
 * @param f the fixture
 * @param form the form measured
 */
static void op_set_u64(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<JB> job = *f.job;
      run("set_u64", [&](std::uint64_t i) { job.set<JB_submission_time>(i); });
   } else {
      lListElem *ep = f.job.get().raw();
      run("set_u64", [&](std::uint64_t i) { ocs::tom::set<JB_submission_time>(ep, i); });
   }
#else
   static_cast<void>(form);
   lListElem *ep = f.job;
   run("set_u64", [&](std::uint64_t i) { lSetUlong64(ep, JB_submission_time, i); });
#endif
}

/**
 * @brief Read a string field (lGetString)
 * @param f the fixture
 * @param form the form measured
 */
static void op_get_str(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<JB> job = *f.job;
      run("get_str", [&](std::uint64_t) { g_sink += job.get<JB_owner>()[0]; });
   } else {
      const lListElem *ep = f.job.get().raw();
      run("get_str", [&](std::uint64_t) { g_sink += ocs::tom::get<JB_owner>(ep)[0]; });
   }
#else
   static_cast<void>(form);
   const lListElem *ep = f.job;
   run("get_str", [&](std::uint64_t) { g_sink += lGetString(ep, JB_owner)[0]; });
#endif
}

/**
 * @brief Write a string field, alternating two values (lSetString)
 * @param f the fixture
 * @param form the form measured
 */
static void op_set_str(Fixture &f, const Form form) {
   const char *const values[2] = {g_owners[0].c_str(), g_owners[1].c_str()};
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<JB> job = *f.job;
      run("set_str", [&](std::uint64_t i) { job.set<JB_owner>(values[i & 1]); });
   } else {
      lListElem *ep = f.job.get().raw();
      run("set_str", [&](std::uint64_t i) { ocs::tom::set<JB_owner>(ep, values[i & 1]); });
   }
#else
   static_cast<void>(form);
   lListElem *ep = f.job;
   run("set_str", [&](std::uint64_t i) { lSetString(ep, JB_owner, values[i & 1]); });
#endif
}

/**
 * @brief Read a host field (lGetHost)
 * @param f the fixture
 * @param form the form measured
 */
static void op_get_host(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<EH> host = *f.host;
      run("get_host", [&](std::uint64_t) { g_sink += static_cast<const char *>(host.get<EH_name>())[0]; });
   } else {
      const lListElem *ep = f.host.get().raw();
      run("get_host", [&](std::uint64_t) { g_sink += static_cast<const char *>(ocs::tom::get<EH_name>(ep))[0]; });
   }
#else
   static_cast<void>(form);
   const lListElem *ep = f.host;
   run("get_host", [&](std::uint64_t) { g_sink += lGetHost(ep, EH_name)[0]; });
#endif
}

/**
 * @brief Write a host field, alternating two values (lSetHost)
 * @param f the fixture
 * @param form the form measured
 */
static void op_set_host(Fixture &f, const Form form) {
   const char *const values[2] = {g_hosts[1].c_str(), g_hosts[0].c_str()};
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<EH> host = *f.host;
      run("set_host", [&](std::uint64_t i) { host.set<EH_name>(ocs::tom::Host(values[i & 1])); });
   } else {
      lListElem *ep = f.host.get().raw();
      run("set_host", [&](std::uint64_t i) { ocs::tom::set<EH_name>(ep, values[i & 1]); });
   }
#else
   static_cast<void>(form);
   lListElem *ep = f.host;
   run("set_host", [&](std::uint64_t i) { lSetHost(ep, EH_name, values[i & 1]); });
#endif
}

/**
 * @brief Read a double field (lGetDouble)
 * @param f the fixture
 * @param form the form measured
 */
static void op_get_double(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<JB> job = *f.job;
      run("get_double", [&](std::uint64_t) { g_sink += static_cast<std::uint64_t>(job.get<JB_urg>()); });
   } else {
      const lListElem *ep = f.job.get().raw();
      run("get_double", [&](std::uint64_t) { g_sink += static_cast<std::uint64_t>(ocs::tom::get<JB_urg>(ep)); });
   }
#else
   static_cast<void>(form);
   const lListElem *ep = f.job;
   run("get_double", [&](std::uint64_t) { g_sink += static_cast<std::uint64_t>(lGetDouble(ep, JB_urg)); });
#endif
}

/**
 * @brief Write a double field (lSetDouble)
 * @param f the fixture
 * @param form the form measured
 */
static void op_set_double(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<JB> job = *f.job;
      run("set_double", [&](std::uint64_t i) { job.set<JB_urg>(static_cast<double>(i & 7)); });
   } else {
      lListElem *ep = f.job.get().raw();
      run("set_double", [&](std::uint64_t i) { ocs::tom::set<JB_urg>(ep, static_cast<double>(i & 7)); });
   }
#else
   static_cast<void>(form);
   lListElem *ep = f.job;
   run("set_double", [&](std::uint64_t i) { lSetDouble(ep, JB_urg, static_cast<double>(i & 7)); });
#endif
}

/**
 * @brief Read a bool field (lGetBool)
 * @param f the fixture
 * @param form the form measured
 */
static void op_get_bool(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<JB> job = *f.job;
      run("get_bool", [&](std::uint64_t) { g_sink += job.get<JB_notify>() ? 1 : 0; });
   } else {
      const lListElem *ep = f.job.get().raw();
      run("get_bool", [&](std::uint64_t) { g_sink += ocs::tom::get<JB_notify>(ep) ? 1 : 0; });
   }
#else
   static_cast<void>(form);
   const lListElem *ep = f.job;
   run("get_bool", [&](std::uint64_t) { g_sink += lGetBool(ep, JB_notify) ? 1 : 0; });
#endif
}

/**
 * @brief Write a bool field (lSetBool)
 * @param f the fixture
 * @param form the form measured
 */
static void op_set_bool(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<JB> job = *f.job;
      run("set_bool", [&](std::uint64_t i) { job.set<JB_notify>((i & 1) != 0); });
   } else {
      lListElem *ep = f.job.get().raw();
      run("set_bool", [&](std::uint64_t i) { ocs::tom::set<JB_notify>(ep, (i & 1) != 0); });
   }
#else
   static_cast<void>(form);
   lListElem *ep = f.job;
   run("set_bool", [&](std::uint64_t i) { lSetBool(ep, JB_notify, (i & 1) != 0); });
#endif
}

/**
 * @brief Read a list field (lGetList)
 * @param f the fixture
 * @param form the form measured
 */
static void op_get_list(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<JB> job = *f.job;
      run("get_list", [&](std::uint64_t) { g_sink += job.get<JB_env_list>() ? 1 : 0; });
   } else {
      const lListElem *ep = f.job.get().raw();
      run("get_list", [&](std::uint64_t) { g_sink += ocs::tom::get<JB_env_list>(ep) ? 1 : 0; });
   }
#else
   static_cast<void>(form);
   const lListElem *ep = f.job;
   run("get_list", [&](std::uint64_t) { g_sink += lGetList(ep, JB_env_list) != nullptr ? 1 : 0; });
#endif
}

/**
 * @brief Write a list field with a new empty list, freeing the one held (lCreateList/lSetList)
 * @param f the fixture
 * @param form the form measured
 */
static void op_set_list(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<JB> job = *f.scratch_a;
      run("set_list", [&](std::uint64_t) { job.set<JB_context>(ListPtr<VA>::create("context")); });
      job.set<JB_context>(nullptr);
   } else {
      lListElem *ep = f.scratch_a.get().raw();
      run("set_list", [&](std::uint64_t) {
         ocs::tom::set<JB_context>(ep, ocs::tom::create_raw_list(VA::descriptor.e, ocs::tom::Hashing::On,
                                                                 "context").release());
      });
      ocs::tom::set<JB_context>(ep, static_cast<lList *>(nullptr));
   }
#else
   static_cast<void>(form);
   lListElem *ep = f.scratch_a;
   run("set_list", [&](std::uint64_t) { lSetList(ep, JB_context, lCreateList("context", VA_Type)); });
   lSetList(ep, JB_context, nullptr);
#endif
}

/**
 * @brief Read an object field (lGetObject)
 * @param f the fixture
 * @param form the form measured
 */
static void op_get_object(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<JB> job = *f.job;
      run("get_object", [&](std::uint64_t) { g_sink += job.get<JB_checkpoint_object>() ? 1 : 0; });
   } else {
      const lListElem *ep = f.job.get().raw();
      run("get_object", [&](std::uint64_t) { g_sink += ocs::tom::get<JB_checkpoint_object>(ep) ? 1 : 0; });
   }
#else
   static_cast<void>(form);
   const lListElem *ep = f.job;
   run("get_object", [&](std::uint64_t) { g_sink += lGetObject(ep, JB_checkpoint_object) != nullptr ? 1 : 0; });
#endif
}

/**
 * @brief Write an object field with a new element, freeing the one held (lCreateElem/lSetObject)
 * @param f the fixture
 * @param form the form measured
 */
static void op_set_object(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<JB> job = *f.scratch_a;
      run("set_object", [&](std::uint64_t) { job.set<JB_checkpoint_object>(ElementPtr<CK>::create()); });
   } else {
      lListElem *ep = f.scratch_a.get().raw();
      run("set_object", [&](std::uint64_t) {
         ocs::tom::set<JB_checkpoint_object>(ep, ocs::tom::create_raw_elem(CK::descriptor.e).release());
      });
   }
#else
   static_cast<void>(form);
   lListElem *ep = f.scratch_a;
   run("set_object", [&](std::uint64_t) { lSetObject(ep, JB_checkpoint_object, lCreateElem(CK_Type)); });
#endif
}

/**
 * @brief Add to a 32 bit field (lAddUlong)
 * @param f the fixture
 * @param form the form measured
 */
static void op_add_u32(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<JB> job = *f.job;
      run("add_u32", [&](std::uint64_t) { g_sink += job.add<JB_priority>(1U); });
   } else {
      lListElem *ep = f.job.get().raw();
      run("add_u32", [&](std::uint64_t) { g_sink += ocs::tom::add<JB_priority>(ep, 1U); });
   }
#else
   static_cast<void>(form);
   lListElem *ep = f.job;
   run("add_u32", [&](std::uint64_t) { g_sink += lAddUlong(ep, JB_priority, 1); });
#endif
}

/**
 * @brief Add to a double field (lAddDouble)
 * @param f the fixture
 * @param form the form measured
 */
static void op_add_double(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<CE> ce = *f.ce;
      run("add_double", [&](std::uint64_t) { g_sink += static_cast<std::uint64_t>(ce.add<CE_doubleval>(0.5)); });
   } else {
      lListElem *ep = f.ce.get().raw();
      run("add_double", [&](std::uint64_t) { g_sink += ocs::tom::add<CE_doubleval>(ep, 0.5); });
   }
#else
   static_cast<void>(form);
   lListElem *ep = f.ce;
   run("add_double", [&](std::uint64_t) { g_sink += lAddDouble(ep, CE_doubleval, 0.5); });
#endif
}

// ---------------------------------------------------------------------------------------------------------------
// group: copies

/**
 * @brief Copy and free a complex entry (lCopyElem)
 * @param f the fixture
 * @param form the form measured
 */
static void op_copy_elem_ce(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<CE> ce = *f.ce;
      run("copy_elem_ce", [&](std::uint64_t) {
         auto copy = ce.clone();
         g_sink += copy ? 1 : 0;
      });
   } else {
      const lListElem *ep = f.ce.get().raw();
      run("copy_elem_ce", [&](std::uint64_t) {
         auto copy = ocs::tom::clone(ep);
         g_sink += copy ? 1 : 0;
      });
   }
#else
   static_cast<void>(form);
   const lListElem *ep = f.ce;
   run("copy_elem_ce", [&](std::uint64_t) {
      lListElem *copy = lCopyElem(ep);
      g_sink += copy != nullptr ? 1 : 0;
      lFreeElem(&copy);
   });
#endif
}

/**
 * @brief Copy and free a job with 10 env entries, 10 tasks and a checkpoint object (lCopyElem)
 * @param f the fixture
 * @param form the form measured
 */
static void op_copy_elem_job(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<JB> job = *f.job;
      run("copy_elem_job", [&](std::uint64_t) {
         auto copy = job.clone();
         g_sink += copy ? 1 : 0;
      });
   } else {
      const lListElem *ep = f.job.get().raw();
      run("copy_elem_job", [&](std::uint64_t) {
         auto copy = ocs::tom::clone(ep);
         g_sink += copy ? 1 : 0;
      });
   }
#else
   static_cast<void>(form);
   const lListElem *ep = f.job;
   run("copy_elem_job", [&](std::uint64_t) {
      lListElem *copy = lCopyElem(ep);
      g_sink += copy != nullptr ? 1 : 0;
      lFreeElem(&copy);
   });
#endif
}

/**
 * @brief Copy and free a list (lCopyList)
 * @tparam ListType the list owner type of the variant
 * @param op the name of the operation
 * @param form the form measured
 * @param list the list to copy
 */
template <typename ListType>
static void copy_list_case(const std::string &op, const Form form, const ListType &list) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const auto typed = *list;
      run(op, [&](std::uint64_t) {
         auto copy = typed.clone("copy");
         g_sink += copy ? 1 : 0;
      });
   } else {
      const lList *lp = list.get().raw();
      run(op, [&](std::uint64_t) {
         auto copy = ocs::tom::clone(lp);
         g_sink += copy ? 1 : 0;
      });
   }
#else
   static_cast<void>(form);
   const lList *lp = list;
   run(op, [&](std::uint64_t) {
      lList *copy = lCopyList("copy", lp);
      g_sink += copy != nullptr ? 1 : 0;
      lFreeList(&copy);
   });
#endif
}

/**
 * @brief Copy a list of short_n complex entries (lCopyList)
 * @param f the fixture
 * @param form the form measured
 */
static void op_copy_list_short(Fixture &f, const Form form) {
   copy_list_case("copy_list_short", form, f.ce_short);
}

/**
 * @brief Copy a list of long_n complex entries (lCopyList)
 * @param f the fixture
 * @param form the form measured
 */
static void op_copy_list_long(Fixture &f, const Form form) {
   copy_list_case("copy_list_long", form, f.ce_long);
}

/**
 * @brief Copy a list of 100 jobs with sub-lists and objects (lCopyList)
 * @param f the fixture
 * @param form the form measured
 */
static void op_copy_list_jobs_100(Fixture &f, const Form form) {
   copy_list_case("copy_list_jobs_100", form, f.jobs_pack);
}

// ---------------------------------------------------------------------------------------------------------------
// group: list structure

/**
 * @brief Append an element to a long list and dechain it again (lAppendElem/lDechainElem)
 * @param f the fixture
 * @param form the form measured
 */
static void op_append_dechain(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const List<JB> list = *f.jobs_long;
      auto spare = ElementPtr<JB>::create();
      spare->set<JB_job_number>(999999);
      run("append_dechain", [&](std::uint64_t) {
         const Element<JB> e = list.push_back(std::move(spare));
         spare = list.extract(e);
      });
   } else {
      lList *lp = f.jobs_long.get().raw();
      auto spare = ocs::tom::create_raw_elem(JB::descriptor.e);
      ocs::tom::set<JB_job_number>(spare.get().raw(), std::uint32_t{999999});
      run("append_dechain", [&](std::uint64_t) {
         lListElem *ep = spare.get().raw();
         ocs::tom::append(lp, std::move(spare));
         spare = ocs::tom::take(lp, ep);
      });
   }
#else
   static_cast<void>(form);
   lList *lp = f.jobs_long;
   lListElem *spare = lCreateElem(JB_Type);
   lSetUlong(spare, JB_job_number, 999999);
   run("append_dechain", [&](std::uint64_t) {
      lAppendElem(lp, spare);
      spare = lDechainElem(lp, spare);
   });
   lFreeElem(&spare);
#endif
}

/**
 * @brief Insert an element at the front of a long list and dechain it again (lInsertElem/lDechainElem)
 * @param f the fixture
 * @param form the form measured
 */
static void op_insert_front_dechain(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const List<JB> list = *f.jobs_long;
      auto spare = ElementPtr<JB>::create();
      spare->set<JB_job_number>(999999);
      run("insert_front_dechain", [&](std::uint64_t) {
         const Element<JB> e = list.push_front(std::move(spare));
         spare = list.extract(e);
      });
   } else {
      lList *lp = f.jobs_long.get().raw();
      auto spare = ocs::tom::create_raw_elem(JB::descriptor.e);
      ocs::tom::set<JB_job_number>(spare.get().raw(), std::uint32_t{999999});
      run("insert_front_dechain", [&](std::uint64_t) {
         lListElem *ep = spare.get().raw();
         ocs::tom::insert_after(lp, nullptr, std::move(spare));
         spare = ocs::tom::take(lp, ep);
      });
   }
#else
   static_cast<void>(form);
   lList *lp = f.jobs_long;
   lListElem *spare = lCreateElem(JB_Type);
   lSetUlong(spare, JB_job_number, 999999);
   run("insert_front_dechain", [&](std::uint64_t) {
      lInsertElem(lp, nullptr, spare);
      spare = lDechainElem(lp, spare);
   });
   lFreeElem(&spare);
#endif
}

/**
 * @brief Insert an element in the middle of a long list and dechain it again (lInsertElem/lDechainElem)
 * @param f the fixture
 * @param form the form measured
 */
static void op_insert_middle_dechain(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const List<JB> list = *f.jobs_long;
      const Element<JB> after = list.find_first<JB_job_number>(long_hit);
      auto spare = ElementPtr<JB>::create();
      spare->set<JB_job_number>(999999);
      run("insert_middle_dechain", [&](std::uint64_t) {
         const Element<JB> e = list.insert_after(after, std::move(spare));
         spare = list.extract(e);
      });
   } else {
      lList *lp = f.jobs_long.get().raw();
      lListElem *after = ocs::tom::find_first_rw<JB_job_number>(lp, long_hit);
      auto spare = ocs::tom::create_raw_elem(JB::descriptor.e);
      ocs::tom::set<JB_job_number>(spare.get().raw(), std::uint32_t{999999});
      run("insert_middle_dechain", [&](std::uint64_t) {
         lListElem *ep = spare.get().raw();
         ocs::tom::insert_after(lp, after, std::move(spare));
         spare = ocs::tom::take(lp, ep);
      });
   }
#else
   static_cast<void>(form);
   lList *lp = f.jobs_long;
   lListElem *after = lGetElemUlongRW(lp, JB_job_number, long_hit);
   lListElem *spare = lCreateElem(JB_Type);
   lSetUlong(spare, JB_job_number, 999999);
   run("insert_middle_dechain", [&](std::uint64_t) {
      lInsertElem(lp, after, spare);
      spare = lDechainElem(lp, spare);
   });
   lFreeElem(&spare);
#endif
}

/**
 * @brief Create an element, append it to a long list and remove (free) it (lCreateElem/lAppendElem/lRemoveElem)
 * @param f the fixture
 * @param form the form measured
 */
static void op_append_remove(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const List<JB> list = *f.jobs_long;
      run("append_remove", [&](std::uint64_t) {
         const Element<JB> e = list.push_back(ElementPtr<JB>::create());
         list.erase(e);
      });
   } else {
      lList *lp = f.jobs_long.get().raw();
      run("append_remove", [&](std::uint64_t) {
         auto spare = ocs::tom::create_raw_elem(JB::descriptor.e);
         lListElem *ep = spare.get().raw();
         ocs::tom::append(lp, std::move(spare));
         ocs::tom::erase(lp, ep);
      });
   }
#else
   static_cast<void>(form);
   lList *lp = f.jobs_long;
   run("append_remove", [&](std::uint64_t) {
      lListElem *ep = lCreateElem(JB_Type);
      lAppendElem(lp, ep);
      lRemoveElem(lp, &ep);
   });
#endif
}

/**
 * @brief Walk a long list and read a field of each element; one call is one walk (for_each_ep)
 * @param f the fixture
 * @param form the form measured
 */
static void op_walk_1000(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const ocs::tom::ConstList<JB> list = *f.jobs_long;
      run("walk_1000", [&](std::uint64_t) {
         for (const auto job : list) {
            g_sink += job.get<JB_job_number>();
         }
      });
   } else {
      const ocs::tom::ConstRawList list(f.jobs_long.get().raw());
      run("walk_1000", [&](std::uint64_t) {
         for (const ocs::tom::ConstRawElement e : list) {
            g_sink += ocs::tom::get<JB_job_number>(e.raw());
         }
      });
   }
#else
   static_cast<void>(form);
   const lList *lp = f.jobs_long;
   run("walk_1000", [&](std::uint64_t) {
      const lListElem *ep;
      for_each_ep(ep, lp) {
         g_sink += lGetUlong(ep, JB_job_number);
      }
   });
#endif
}

/**
 * @brief The number of elements of a list (lGetNumberOfElem)
 * @param f the fixture
 * @param form the form measured
 */
static void op_size(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const List<JB> list = *f.jobs_long;
      run("size", [&](std::uint64_t) { g_sink += list.size(); });
   } else {
      const lList *lp = f.jobs_long.get().raw();
      run("size", [&](std::uint64_t) { g_sink += ocs::tom::size(lp); });
   }
#else
   static_cast<void>(form);
   const lList *lp = f.jobs_long;
   run("size", [&](std::uint64_t) { g_sink += lGetNumberOfElem(lp); });
#endif
}

// ---------------------------------------------------------------------------------------------------------------
// group: lookups in a list

#if OCS_PERF_TOM

/**
 * @brief Measure one lookup by key in a list
 * @tparam F the key field
 * @tparam T the object type
 * @param op the name of the operation
 * @param form the form measured
 * @param list the list
 * @param key the key
 */
template <auto F, ocs::tom::ObjectType T>
static void find_case(const std::string &op, const Form form, const List<T> list, const Key<F> key) {
   if (form == Form::Typed) {
      const auto typed = typed_key<F>(key);
      run(op, [&](std::uint64_t) { g_sink += list.template find_first<F>(typed) ? 1 : 0; });
   } else {
      const lList *lp = list.raw();
      run(op, [&](std::uint64_t) { g_sink += ocs::tom::find_first<F>(lp, key) ? 1 : 0; });
   }
}

/**
 * @brief Measure a walk over all elements with a key, first and next
 * @tparam F the key field
 * @tparam T the object type
 * @param op the name of the operation
 * @param form the form measured
 * @param list the list
 * @param key the key
 */
template <auto F, ocs::tom::ObjectType T>
static void find_all_case(const std::string &op, const Form form, const List<T> list, const Key<F> key) {
   if (form == Form::Typed) {
      const auto typed = typed_key<F>(key);
      run(op, [&](std::uint64_t) {
         for (const auto e : list.template find_all<F>(typed)) {
            g_sink += e ? 1 : 0;
         }
      });
   } else {
      const lList *lp = list.raw();
      run(op, [&](std::uint64_t) {
         const void *it = nullptr;
         for (auto e = ocs::tom::find_first<F>(lp, key, &it); e; e = ocs::tom::find_next<F>(lp, key, &it)) {
            g_sink += 1;
         }
      });
   }
}

/**
 * @brief Measure adding an element by key to a list and deleting it by key again
 * @tparam F the key field
 * @tparam T the object type
 * @param op the name of the operation
 * @param form the form measured
 * @param list the list
 * @param key the key, held by no element of the list
 */
template <auto F, ocs::tom::ObjectType T>
static void add_del_case(const std::string &op, const Form form, const List<T> list, const Key<F> key) {
   if (form == Form::Typed) {
      const auto typed = typed_key<F>(key);
      run(op, [&](std::uint64_t) {
         g_sink += list.template emplace_back<F>(typed) ? 1 : 0;
         g_sink += list.template erase<F>(typed);
      });
   } else {
      lList *lp = list.raw();
      run(op, [&](std::uint64_t) {
         g_sink += ocs::tom::emplace_back<F>(&lp, key) ? 1 : 0;
         g_sink += ocs::tom::erase<F>(&lp, key);
      });
   }
}

#endif

/**
 * @brief Look up one element by key in a list (lGetElemStr/lGetElemUlong/lGetElemUlong64/lGetElemHost)
 * @param f the fixture
 * @param form the form measured
 * @param kind the key type: CE_name, JAT_task_number, RDE_time (not hashed in either line), EH_name
 * @param size short (linear) or long (hashed, except RDE_time) list
 * @param hit true to look up a key in the middle of the list, false for a key no element holds
 */
static void op_find(Fixture &f, const Form form, const Kind kind, const Size size, const bool hit) {
   const std::string op = op_name("find", kind, size, hit ? "hit" : "miss");
   const std::size_t idx = hit_of(size);
   const bool s = size == Size::Short;
   const char *str = hit ? g_names[idx].c_str() : miss_str;
   const char *host = hit ? g_hosts[idx].c_str() : miss_host;
   const std::uint32_t u32 = hit ? static_cast<std::uint32_t>(idx + 1) : miss_u32;
   const std::uint64_t u64 = hit ? base_u64 + idx : miss_u64;
#if OCS_PERF_TOM
   switch (kind) {
      case Kind::Str:
         find_case<CE_name>(op, form, *(s ? f.ce_short : f.ce_long), str);
         break;
      case Kind::U32:
         find_case<JAT_task_number>(op, form, *(s ? f.tasks_short : f.tasks_long), u32);
         break;
      case Kind::U64:
         find_case<RDE_time>(op, form, *(s ? f.rde_short : f.rde_long), u64);
         break;
      case Kind::Host:
         find_case<EH_name>(op, form, *(s ? f.eh_short : f.eh_long), host);
         break;
   }
#else
   static_cast<void>(form);
   switch (kind) {
      case Kind::Str: {
         const lList *lp = s ? f.ce_short : f.ce_long;
         run(op, [&](std::uint64_t) { g_sink += lGetElemStr(lp, CE_name, str) != nullptr ? 1 : 0; });
         break;
      }
      case Kind::U32: {
         const lList *lp = s ? f.tasks_short : f.tasks_long;
         run(op, [&](std::uint64_t) { g_sink += lGetElemUlong(lp, JAT_task_number, u32) != nullptr ? 1 : 0; });
         break;
      }
      case Kind::U64: {
         const lList *lp = s ? f.rde_short : f.rde_long;
         run(op, [&](std::uint64_t) { g_sink += lGetElemUlong64(lp, RDE_time, u64) != nullptr ? 1 : 0; });
         break;
      }
      case Kind::Host: {
         const lList *lp = s ? f.eh_short : f.eh_long;
         run(op, [&](std::uint64_t) { g_sink += lGetElemHost(lp, EH_name, host) != nullptr ? 1 : 0; });
         break;
      }
   }
#endif
}

/**
 * @brief Walk all elements with a non-unique key, first and next (lGetElemStrFirst/Next, lGetElemHostFirst/Next)
 * @param f the fixture
 * @param form the form measured
 * @param kind Str (QU_qname) or Host (QU_qhostname)
 * @param size short (4 matches of 8) or long (10 matches of 1000, hashed)
 */
static void op_find_all(Fixture &f, const Form form, const Kind kind, const Size size) {
   const std::string op = op_name("find_all", kind, size, nullptr);
   const bool s = size == Size::Short;
   const char *str = g_qnames[1].c_str();
   const char *host = g_hosts[1].c_str();
#if OCS_PERF_TOM
   if (kind == Kind::Str) {
      find_all_case<QU_qname>(op, form, *(s ? f.qu_short : f.qu_long), str);
   } else {
      find_all_case<QU_qhostname>(op, form, *(s ? f.qu_short : f.qu_long), host);
   }
#else
   static_cast<void>(form);
   const lList *lp = s ? f.qu_short : f.qu_long;
   if (kind == Kind::Str) {
      run(op, [&](std::uint64_t) {
         const void *it = nullptr;
         for (const lListElem *ep = lGetElemStrFirst(lp, QU_qname, str, &it); ep != nullptr;
              ep = lGetElemStrNext(lp, QU_qname, str, &it)) {
            g_sink += 1;
         }
      });
   } else {
      run(op, [&](std::uint64_t) {
         const void *it = nullptr;
         for (const lListElem *ep = lGetElemHostFirst(lp, QU_qhostname, host, &it); ep != nullptr;
              ep = lGetElemHostNext(lp, QU_qhostname, host, &it)) {
            g_sink += 1;
         }
      });
   }
#endif
}

/**
 * @brief Fill an empty hashed list with long_n elements by key and look one up, which in TOM builds the hash table
 *
 * CULL keeps the hash table up to date on every append; TOM builds it on the first lookup. Both are in the time.
 *
 * @param f the fixture
 * @param form the form measured
 */
static void op_hash_build_1000(Fixture &f, const Form form) {
   static_cast<void>(f);
   const char *key = g_names[long_hit].c_str();
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      ListPtr<CE> lp;
      run_batched("hash_build_1000", 1, [&] { lp = ListPtr<CE>::create("ce"); },
                  [&](std::uint64_t) {
                     const List<CE> list = *lp;
                     for (std::size_t k = 0; k < long_n; k++) {
                        list.emplace_back<CE_name>(g_names[k].c_str());
                     }
                     g_sink += list.find_first<CE_name>(key) ? 1 : 0;
                  },
                  [&] { lp.reset(); });
   } else {
      lList *lp = nullptr;
      run_batched("hash_build_1000", 1,
                  [&] { lp = ocs::tom::create_raw_list(CE::descriptor.e, ocs::tom::Hashing::On, "ce").release(); },
                  [&](std::uint64_t) {
                     for (std::size_t k = 0; k < long_n; k++) {
                        ocs::tom::emplace_back<CE_name>(&lp, g_names[k].c_str());
                     }
                     g_sink += ocs::tom::find_first<CE_name>(lp, key) ? 1 : 0;
                  },
                  [&] { ocs::tom::free_raw_list(lp); });
   }
#else
   static_cast<void>(form);
   lList *lp = nullptr;
   run_batched("hash_build_1000", 1, [&] { lp = lCreateList("ce", CE_Type); },
               [&](std::uint64_t) {
                  for (std::size_t k = 0; k < long_n; k++) {
                     lAddElemStr(&lp, CE_name, g_names[k].c_str(), CE_Type);
                  }
                  g_sink += lGetElemStr(lp, CE_name, key) != nullptr ? 1 : 0;
               },
               [&] { lFreeList(&lp); });
#endif
}

// ---------------------------------------------------------------------------------------------------------------
// group: adding and deleting by key

/**
 * @brief Add an element by key to a list and delete it by key (lAddElem<Type>/lDelElem<Type>)
 * @param f the fixture
 * @param form the form measured
 * @param kind the key type: CE_name, JAT_task_number, RDE_time, EH_name
 * @param size short or long list
 */
static void op_add_del(Fixture &f, const Form form, const Kind kind, const Size size) {
   const std::string op = op_name("add_del", kind, size, nullptr);
   const bool s = size == Size::Short;
   const char *str = g_new_names[0].c_str();
   const char *host = g_new_hosts[0].c_str();
#if OCS_PERF_TOM
   switch (kind) {
      case Kind::Str:
         add_del_case<CE_name>(op, form, *(s ? f.ce_short : f.ce_long), str);
         break;
      case Kind::U32:
         add_del_case<JAT_task_number>(op, form, *(s ? f.tasks_short : f.tasks_long), new_u32);
         break;
      case Kind::U64:
         add_del_case<RDE_time>(op, form, *(s ? f.rde_short : f.rde_long), new_u64);
         break;
      case Kind::Host:
         add_del_case<EH_name>(op, form, *(s ? f.eh_short : f.eh_long), host);
         break;
   }
#else
   static_cast<void>(form);
   switch (kind) {
      case Kind::Str: {
         lList *lp = s ? f.ce_short : f.ce_long;
         run(op, [&](std::uint64_t) {
            g_sink += lAddElemStr(&lp, CE_name, str, CE_Type) != nullptr ? 1 : 0;
            g_sink += lDelElemStr(&lp, CE_name, str);
         });
         break;
      }
      case Kind::U32: {
         lList *lp = s ? f.tasks_short : f.tasks_long;
         run(op, [&](std::uint64_t) {
            g_sink += lAddElemUlong(&lp, JAT_task_number, new_u32, JAT_Type) != nullptr ? 1 : 0;
            g_sink += lDelElemUlong(&lp, JAT_task_number, new_u32);
         });
         break;
      }
      case Kind::U64: {
         lList *lp = s ? f.rde_short : f.rde_long;
         run(op, [&](std::uint64_t) {
            g_sink += lAddElemUlong64(&lp, RDE_time, new_u64, RDE_Type) != nullptr ? 1 : 0;
            g_sink += lDelElemUlong64(&lp, RDE_time, new_u64);
         });
         break;
      }
      case Kind::Host: {
         lList *lp = s ? f.eh_short : f.eh_long;
         run(op, [&](std::uint64_t) {
            g_sink += lAddElemHost(&lp, EH_name, host, EH_Type) != nullptr ? 1 : 0;
            g_sink += lDelElemHost(&lp, EH_name, host);
         });
         break;
      }
   }
#endif
}

// ---------------------------------------------------------------------------------------------------------------
// group: sub-lists, the functions addressed by two fields (a list field and a field of its elements)

#if OCS_PERF_TOM

/**
 * @brief Measure one lookup by key in a sub-list
 * @tparam LF the list field
 * @tparam F the key field of the sub-type
 * @tparam T the object type holding the list field
 * @param op the name of the operation
 * @param form the form measured
 * @param elem the element holding the sub-list
 * @param key the key
 */
template <auto LF, auto F, ocs::tom::ObjectType T>
static void sub_find_case(const std::string &op, const Form form, const Element<T> elem, const Key<F> key) {
   if (form == Form::Typed) {
      run(op, [&](std::uint64_t) { g_sink += elem.template find_first_in<LF, F>(key) ? 1 : 0; });
   } else {
      const lListElem *ep = elem.raw();
      run(op, [&](std::uint64_t) { g_sink += ocs::tom::find_first_in<LF, F>(ep, key) ? 1 : 0; });
   }
}

/**
 * @brief Measure adding elements by key to a sub-list; they are deleted again outside the time
 * @tparam LF the list field
 * @tparam F the key field of the sub-type
 * @tparam T the object type holding the list field
 * @tparam KeyAt the key of the i-th element added
 * @param op the name of the operation
 * @param form the form measured
 * @param elem the element holding the sub-list
 * @param batch the elements added per round
 * @param key_at the key of the i-th element added
 */
template <auto LF, auto F, ocs::tom::ObjectType T, typename KeyAt>
static void sub_add_case(const std::string &op, const Form form, const Element<T> elem, const std::size_t batch,
                         KeyAt &&key_at) {
   if (form == Form::Typed) {
      run_batched(op, batch, [] {},
                  [&](std::uint64_t i) { g_sink += elem.template emplace_back_in<LF, F>(key_at(i)) ? 1 : 0; },
                  [&] {
                     for (std::size_t i = 0; i < batch; i++) {
                        g_sink += elem.template erase_in<LF, F>(key_at(i));
                     }
                  });
   } else {
      lListElem *ep = elem.raw();
      run_batched(op, batch, [] {},
                  [&](std::uint64_t i) { g_sink += ocs::tom::emplace_back_in<LF, F>(ep, key_at(i)) ? 1 : 0; },
                  [&] {
                     for (std::size_t i = 0; i < batch; i++) {
                        g_sink += ocs::tom::erase_in<LF, F>(ep, key_at(i));
                     }
                  });
   }
}

/**
 * @brief Measure deleting elements by key from a sub-list; they are added outside the time
 * @tparam LF the list field
 * @tparam F the key field of the sub-type
 * @tparam T the object type holding the list field
 * @tparam KeyAt the key of the i-th element deleted
 * @param op the name of the operation
 * @param form the form measured
 * @param elem the element holding the sub-list
 * @param batch the elements deleted per round
 * @param key_at the key of the i-th element deleted
 */
template <auto LF, auto F, ocs::tom::ObjectType T, typename KeyAt>
static void sub_del_case(const std::string &op, const Form form, const Element<T> elem, const std::size_t batch,
                         KeyAt &&key_at) {
   if (form == Form::Typed) {
      run_batched(op, batch,
                  [&] {
                     for (std::size_t i = 0; i < batch; i++) {
                        g_sink += elem.template emplace_back_in<LF, F>(key_at(i)) ? 1 : 0;
                     }
                  },
                  [&](std::uint64_t i) { g_sink += elem.template erase_in<LF, F>(key_at(i)); }, [] {});
   } else {
      lListElem *ep = elem.raw();
      run_batched(op, batch,
                  [&] {
                     for (std::size_t i = 0; i < batch; i++) {
                        g_sink += ocs::tom::emplace_back_in<LF, F>(ep, key_at(i)) ? 1 : 0;
                     }
                  },
                  [&](std::uint64_t i) { g_sink += ocs::tom::erase_in<LF, F>(ep, key_at(i)); }, [] {});
   }
}

#endif

/**
 * @brief Look up one element by key in a sub-list (lGetSubStr/lGetSubUlong/lGetSubUlong64/lGetSubHost)
 * @param f the fixture
 * @param form the form measured
 * @param kind Str: JB_env_list/VA_variable, U32: JB_ja_tasks/JAT_task_number, U64: RUE_utilized/RDE_time (not
 *        hashed), Host: CQ_qinstances/QU_qhostname
 * @param size short (linear) or long (hashed) sub-list
 * @param hit true to look up a key in the middle of the sub-list, false for a key no element holds
 */
static void op_sub_find(Fixture &f, const Form form, const Kind kind, const Size size, const bool hit) {
   const std::string op = op_name("sub_find", kind, size, hit ? "hit" : "miss");
   const std::size_t idx = hit_of(size);
   const bool s = size == Size::Short;
   const char *str = hit ? g_vars[idx].c_str() : miss_str;
   const char *host = hit ? g_hosts[idx].c_str() : miss_host;
   const std::uint32_t u32 = hit ? static_cast<std::uint32_t>(idx + 1) : miss_u32;
   const std::uint64_t u64 = hit ? base_u64 + idx : miss_u64;
#if OCS_PERF_TOM
   switch (kind) {
      case Kind::Str:
         sub_find_case<JB_env_list, VA_variable>(op, form, *(s ? f.sub_short : f.sub_long), str);
         break;
      case Kind::U32:
         sub_find_case<JB_ja_tasks, JAT_task_number>(op, form, *(s ? f.sub_short : f.sub_long), u32);
         break;
      case Kind::U64:
         sub_find_case<RUE_utilized, RDE_time>(op, form, *(s ? f.rue_short : f.rue_long), u64);
         break;
      case Kind::Host:
         sub_find_case<CQ_qinstances, QU_qhostname>(op, form, *(s ? f.cq_short : f.cq_long), host);
         break;
   }
#else
   static_cast<void>(form);
   switch (kind) {
      case Kind::Str: {
         const lListElem *ep = s ? f.sub_short : f.sub_long;
         run(op, [&](std::uint64_t) { g_sink += lGetSubStr(ep, VA_variable, str, JB_env_list) != nullptr ? 1 : 0; });
         break;
      }
      case Kind::U32: {
         const lListElem *ep = s ? f.sub_short : f.sub_long;
         run(op, [&](std::uint64_t) {
            g_sink += lGetSubUlong(ep, JAT_task_number, u32, JB_ja_tasks) != nullptr ? 1 : 0;
         });
         break;
      }
      case Kind::U64: {
         const lListElem *ep = s ? f.rue_short : f.rue_long;
         run(op, [&](std::uint64_t) { g_sink += lGetSubUlong64(ep, RDE_time, u64, RUE_utilized) != nullptr ? 1 : 0; });
         break;
      }
      case Kind::Host: {
         const lListElem *ep = s ? f.cq_short : f.cq_long;
         run(op, [&](std::uint64_t) {
            g_sink += lGetSubHost(ep, QU_qhostname, host, CQ_qinstances) != nullptr ? 1 : 0;
         });
         break;
      }
   }
#endif
}

/**
 * @brief Add elements by key to a sub-list (lAddSubStr/lAddSubUlong/lAddSubUlong64/lAddSubHost)
 *
 * Per round add_short (short) or add_long (long) elements are added and timed, then deleted outside the time.
 *
 * @param f the fixture
 * @param form the form measured
 * @param kind the sub-list and key, see op_sub_find()
 * @param size short or long sub-list
 */
static void op_sub_add(Fixture &f, const Form form, const Kind kind, const Size size) {
   const std::string op = op_name("sub_add", kind, size, nullptr);
   const std::size_t batch = add_of(size);
   const bool s = size == Size::Short;
   const auto str_at = [](std::uint64_t i) { return g_new_names[i].c_str(); };
   const auto host_at = [](std::uint64_t i) { return g_new_hosts[i].c_str(); };
   const auto u32_at = [](std::uint64_t i) { return static_cast<std::uint32_t>(new_u32 + i); };
   const auto u64_at = [](std::uint64_t i) { return new_u64 + i; };
#if OCS_PERF_TOM
   switch (kind) {
      case Kind::Str:
         sub_add_case<JB_env_list, VA_variable>(op, form, *(s ? f.sub_short : f.sub_long), batch, str_at);
         break;
      case Kind::U32:
         sub_add_case<JB_ja_tasks, JAT_task_number>(op, form, *(s ? f.sub_short : f.sub_long), batch, u32_at);
         break;
      case Kind::U64:
         sub_add_case<RUE_utilized, RDE_time>(op, form, *(s ? f.rue_short : f.rue_long), batch, u64_at);
         break;
      case Kind::Host:
         sub_add_case<CQ_qinstances, QU_qhostname>(op, form, *(s ? f.cq_short : f.cq_long), batch, host_at);
         break;
   }
#else
   static_cast<void>(form);
   switch (kind) {
      case Kind::Str: {
         lListElem *ep = s ? f.sub_short : f.sub_long;
         run_batched(op, batch, [] {},
                     [&](std::uint64_t i) {
                        g_sink += lAddSubStr(ep, VA_variable, str_at(i), JB_env_list, VA_Type) != nullptr ? 1 : 0;
                     },
                     [&] {
                        for (std::size_t i = 0; i < batch; i++) {
                           g_sink += lDelSubStr(ep, VA_variable, str_at(i), JB_env_list);
                        }
                     });
         break;
      }
      case Kind::U32: {
         lListElem *ep = s ? f.sub_short : f.sub_long;
         run_batched(op, batch, [] {},
                     [&](std::uint64_t i) {
                        lListElem *added = lAddSubUlong(ep, JAT_task_number, u32_at(i), JB_ja_tasks, JAT_Type);
                        g_sink += added != nullptr ? 1 : 0;
                     },
                     [&] {
                        for (std::size_t i = 0; i < batch; i++) {
                           g_sink += lDelSubUlong(ep, JAT_task_number, u32_at(i), JB_ja_tasks);
                        }
                     });
         break;
      }
      case Kind::U64: {
         lListElem *ep = s ? f.rue_short : f.rue_long;
         run_batched(op, batch, [] {},
                     [&](std::uint64_t i) {
                        g_sink += lAddSubUlong64(ep, RDE_time, u64_at(i), RUE_utilized, RDE_Type) != nullptr ? 1 : 0;
                     },
                     [&] {
                        for (std::size_t i = 0; i < batch; i++) {
                           g_sink += lDelSubUlong64(ep, RDE_time, u64_at(i), RUE_utilized);
                        }
                     });
         break;
      }
      case Kind::Host: {
         // CULL has no lDelSubHost(); the cleanup outside the time deletes through the list reference
         lListElem *ep = s ? f.cq_short : f.cq_long;
         run_batched(op, batch, [] {},
                     [&](std::uint64_t i) {
                        g_sink += lAddSubHost(ep, QU_qhostname, host_at(i), CQ_qinstances, QU_Type) != nullptr ? 1 : 0;
                     },
                     [&] {
                        for (std::size_t i = 0; i < batch; i++) {
                           g_sink += lDelElemHost(lGetListRef(ep, CQ_qinstances), QU_qhostname, host_at(i));
                        }
                     });
         break;
      }
   }
#endif
}

/**
 * @brief Delete elements by key from a sub-list (lDelSubStr/lDelSubUlong/lDelSubUlong64; host see below)
 *
 * Per round add_short (short) or add_long (long) elements are added outside the time, then deleted and timed.
 * CULL has no lDelSubHost(); its host form is lDelElemHost() on lGetListRef(), the same work by other calls.
 *
 * @param f the fixture
 * @param form the form measured
 * @param kind the sub-list and key, see op_sub_find()
 * @param size short or long sub-list
 */
static void op_sub_del(Fixture &f, const Form form, const Kind kind, const Size size) {
   const std::string op = op_name("sub_del", kind, size, nullptr);
   const std::size_t batch = add_of(size);
   const bool s = size == Size::Short;
   const auto str_at = [](std::uint64_t i) { return g_new_names[i].c_str(); };
   const auto host_at = [](std::uint64_t i) { return g_new_hosts[i].c_str(); };
   const auto u32_at = [](std::uint64_t i) { return static_cast<std::uint32_t>(new_u32 + i); };
   const auto u64_at = [](std::uint64_t i) { return new_u64 + i; };
#if OCS_PERF_TOM
   switch (kind) {
      case Kind::Str:
         sub_del_case<JB_env_list, VA_variable>(op, form, *(s ? f.sub_short : f.sub_long), batch, str_at);
         break;
      case Kind::U32:
         sub_del_case<JB_ja_tasks, JAT_task_number>(op, form, *(s ? f.sub_short : f.sub_long), batch, u32_at);
         break;
      case Kind::U64:
         sub_del_case<RUE_utilized, RDE_time>(op, form, *(s ? f.rue_short : f.rue_long), batch, u64_at);
         break;
      case Kind::Host:
         sub_del_case<CQ_qinstances, QU_qhostname>(op, form, *(s ? f.cq_short : f.cq_long), batch, host_at);
         break;
   }
#else
   static_cast<void>(form);
   switch (kind) {
      case Kind::Str: {
         lListElem *ep = s ? f.sub_short : f.sub_long;
         run_batched(op, batch,
                     [&] {
                        for (std::size_t i = 0; i < batch; i++) {
                           lAddSubStr(ep, VA_variable, str_at(i), JB_env_list, VA_Type);
                        }
                     },
                     [&](std::uint64_t i) { g_sink += lDelSubStr(ep, VA_variable, str_at(i), JB_env_list); }, [] {});
         break;
      }
      case Kind::U32: {
         lListElem *ep = s ? f.sub_short : f.sub_long;
         run_batched(op, batch,
                     [&] {
                        for (std::size_t i = 0; i < batch; i++) {
                           lAddSubUlong(ep, JAT_task_number, u32_at(i), JB_ja_tasks, JAT_Type);
                        }
                     },
                     [&](std::uint64_t i) { g_sink += lDelSubUlong(ep, JAT_task_number, u32_at(i), JB_ja_tasks); },
                     [] {});
         break;
      }
      case Kind::U64: {
         lListElem *ep = s ? f.rue_short : f.rue_long;
         run_batched(op, batch,
                     [&] {
                        for (std::size_t i = 0; i < batch; i++) {
                           lAddSubUlong64(ep, RDE_time, u64_at(i), RUE_utilized, RDE_Type);
                        }
                     },
                     [&](std::uint64_t i) { g_sink += lDelSubUlong64(ep, RDE_time, u64_at(i), RUE_utilized); },
                     [] {});
         break;
      }
      case Kind::Host: {
         lListElem *ep = s ? f.cq_short : f.cq_long;
         run_batched(op, batch,
                     [&] {
                        for (std::size_t i = 0; i < batch; i++) {
                           lAddSubHost(ep, QU_qhostname, host_at(i), CQ_qinstances, QU_Type);
                        }
                     },
                     [&](std::uint64_t i) {
                        g_sink += lDelElemHost(lGetListRef(ep, CQ_qinstances), QU_qhostname, host_at(i));
                     },
                     [] {});
         break;
      }
   }
#endif
}

/**
 * @brief Append a list of short_n variables to the env sub-list of a job copy (lAddSubList)
 * @param f the fixture
 * @param form the form measured
 */
static void op_sub_add_list_short(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      ElementPtr<JB> job;
      ListPtr<VA> src;
      run_batched("sub_add_list_short", 1,
                  [&] {
                     job = f.sub_short->clone();
                     src = f.va_add->clone("src");
                  },
                  [&](std::uint64_t) { job->get_or_create<JB_env_list>("env").splice(std::move(src)); },
                  [&] { job.reset(); });
   } else {
      ocs::tom::RawElementPtr job;
      ocs::tom::RawListPtr src;
      run_batched("sub_add_list_short", 1,
                  [&] {
                     job = ocs::tom::clone(f.sub_short.get().raw());
                     src = ocs::tom::clone(f.va_add.get().raw());
                  },
                  [&](std::uint64_t) {
                     ocs::tom::splice(ocs::tom::get_or_create<JB_env_list>(job.get().raw(), "env"), std::move(src));
                  },
                  [&] { job.reset(); });
   }
#else
   static_cast<void>(form);
   lListElem *job = nullptr;
   lList *src = nullptr;
   run_batched("sub_add_list_short", 1,
               [&] {
                  job = lCopyElem(f.sub_short);
                  src = lCopyList("src", f.va_add);
               },
               [&](std::uint64_t) { g_sink += lAddSubList(job, JB_env_list, src) != nullptr ? 1 : 0; },
               [&] { lFreeElem(&job); });
#endif
}

// ---------------------------------------------------------------------------------------------------------------
// group: sorting, on a copy of the long job list made outside the time

#if OCS_PERF_TOM

/**
 * @brief Measure sorting a copy of the long job list
 * @tparam Typed the typed sort, called with the List<JB>
 * @param op the name of the operation
 * @param f the fixture
 * @param form the form measured
 * @param typed the typed sort
 * @param keys the sort order of the raw form, built per call as lPSortList() parses its format per call
 */
template <typename Typed>
static void sort_case(const std::string &op, Fixture &f, const Form form, Typed &&typed,
                      const std::vector<ocs::tom::SortKey> &keys) {
   if (form == Form::Typed) {
      ListPtr<JB> lp;
      run_batched(op, 1, [&] { lp = f.jobs_long->clone("sort"); }, [&](std::uint64_t) { typed(*lp); },
                  [&] { lp.reset(); });
   } else {
      ocs::tom::RawListPtr lp;
      run_batched(op, 1, [&] { lp = ocs::tom::clone(f.jobs_long.get().raw()); },
                  [&](std::uint64_t) {
                     lSortOrder *so = ocs::tom::make_sort_order(JB::descriptor.e, keys);
                     ocs::tom::sort_list(lp.get().raw(), so);
                     ocs::tom::free_sort_order(so);
                  },
                  [&] { lp.reset(); });
   }
}

#else

/**
 * @brief Measure sorting a copy of the long job list
 * @tparam Sort the sort, called with the list
 * @param op the name of the operation
 * @param f the fixture
 * @param sort the sort
 */
template <typename Sort>
static void sort_case(const std::string &op, Fixture &f, Sort &&sort) {
   lList *lp = nullptr;
   run_batched(op, 1, [&] { lp = lCopyList("sort", f.jobs_long); }, [&](std::uint64_t) { sort(lp); },
               [&] { lFreeList(&lp); });
}

#endif

/**
 * @brief Sort 1000 jobs by a 32 bit field ascending (lPSortList "%I+")
 * @param f the fixture
 * @param form the form measured
 */
static void op_sort_u32_asc(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   sort_case("sort_u32_asc", f, form, [](const List<JB> l) { l.sort<ocs::tom::asc<JB_priority>>(); },
             {{JB_priority, true}});
#else
   static_cast<void>(form);
   sort_case("sort_u32_asc", f, [](lList *lp) { lPSortList(lp, "%I+", JB_priority); });
#endif
}

/**
 * @brief Sort 1000 jobs by a 32 bit field descending (lPSortList "%I-")
 * @param f the fixture
 * @param form the form measured
 */
static void op_sort_u32_desc(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   sort_case("sort_u32_desc", f, form, [](const List<JB> l) { l.sort<ocs::tom::desc<JB_priority>>(); },
             {{JB_priority, false}});
#else
   static_cast<void>(form);
   sort_case("sort_u32_desc", f, [](lList *lp) { lPSortList(lp, "%I-", JB_priority); });
#endif
}

/**
 * @brief Sort 1000 jobs by a 64 bit field ascending (lPSortList "%I+")
 * @param f the fixture
 * @param form the form measured
 */
static void op_sort_u64_asc(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   sort_case("sort_u64_asc", f, form, [](const List<JB> l) { l.sort<ocs::tom::asc<JB_submission_time>>(); },
             {{JB_submission_time, true}});
#else
   static_cast<void>(form);
   sort_case("sort_u64_asc", f, [](lList *lp) { lPSortList(lp, "%I+", JB_submission_time); });
#endif
}

/**
 * @brief Sort 1000 jobs by a string field ascending (lPSortList "%I+")
 * @param f the fixture
 * @param form the form measured
 */
static void op_sort_str_asc(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   sort_case("sort_str_asc", f, form, [](const List<JB> l) { l.sort<ocs::tom::asc<JB_job_name>>(); },
             {{JB_job_name, true}});
#else
   static_cast<void>(form);
   sort_case("sort_str_asc", f, [](lList *lp) { lPSortList(lp, "%I+", JB_job_name); });
#endif
}

/**
 * @brief Sort 1000 jobs by a string field descending (lPSortList "%I-")
 * @param f the fixture
 * @param form the form measured
 */
static void op_sort_str_desc(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   sort_case("sort_str_desc", f, form, [](const List<JB> l) { l.sort<ocs::tom::desc<JB_job_name>>(); },
             {{JB_job_name, false}});
#else
   static_cast<void>(form);
   sort_case("sort_str_desc", f, [](lList *lp) { lPSortList(lp, "%I-", JB_job_name); });
#endif
}

/**
 * @brief Sort 1000 jobs by three keys: priority descending, owner and job number ascending (lPSortList)
 * @param f the fixture
 * @param form the form measured
 */
static void op_sort_3keys(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   sort_case("sort_3keys", f, form,
             [](const List<JB> l) {
                l.sort<ocs::tom::desc<JB_priority>, ocs::tom::asc<JB_owner>, ocs::tom::asc<JB_job_number>>();
             },
             {{JB_priority, false}, {JB_owner, true}, {JB_job_number, true}});
#else
   static_cast<void>(form);
   sort_case("sort_3keys", f,
             [](lList *lp) { lPSortList(lp, "%I- %I+ %I+", JB_priority, JB_owner, JB_job_number); });
#endif
}

/**
 * @brief Insert a job into the long list sorted by job number and dechain it again (lInsertSorted)
 *
 * The sort order is built once, outside the time.
 *
 * @param f the fixture
 * @param form the form measured
 */
static void op_insert_sorted(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const List<JB> list = *f.jobs_long;
      const auto order = ocs::tom::sort_order<ocs::tom::asc<JB_job_number>>();
      auto spare = ElementPtr<JB>::create();
      spare->set<JB_job_number>(long_hit);
      run("insert_sorted", [&](std::uint64_t) {
         const Element<JB> e = list.insert_sorted(order, std::move(spare));
         spare = list.extract(e);
      });
   } else {
      lList *lp = f.jobs_long.get().raw();
      const ocs::tom::SortKey keys[] = {{JB_job_number, true}};
      lSortOrder *so = ocs::tom::make_sort_order(JB::descriptor.e, keys);
      auto spare = ocs::tom::create_raw_elem(JB::descriptor.e);
      ocs::tom::set<JB_job_number>(spare.get().raw(), static_cast<std::uint32_t>(long_hit));
      run("insert_sorted", [&](std::uint64_t) {
         lListElem *ep = spare.release();
         ocs::tom::insert_sorted(lp, ep, so);
         spare = ocs::tom::take(lp, ep);
      });
      ocs::tom::free_sort_order(so);
   }
#else
   static_cast<void>(form);
   lList *lp = f.jobs_long;
   lSortOrder *so = lParseSortOrderVarArg(JB_Type, "%I+", JB_job_number);
   lListElem *spare = lCreateElem(JB_Type);
   lSetUlong(spare, JB_job_number, long_hit);
   run("insert_sorted", [&](std::uint64_t) {
      lInsertSorted(so, spare, lp);
      spare = lDechainElem(lp, spare);
   });
   lFreeElem(&spare);
   lFreeSortOrder(&so);
#endif
}

// ---------------------------------------------------------------------------------------------------------------
// group: conditions, field selections and select

/**
 * @brief Build and free a condition of two comparisons (lWhere "%T(%I == %s && %I > %u)")
 * @param f the fixture
 * @param form the form measured
 */
static void op_where_build(Fixture &f, const Form form) {
   static_cast<void>(f);
   const char *owner = "user3";
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      run("where_build", [&](std::uint64_t) {
         auto where = ocs::tom::field<JB_owner>() == owner && ocs::tom::field<JB_priority>() > 100U;
         g_sink += where.get() != nullptr ? 1 : 0;
      });
   } else {
      run("where_build", [&](std::uint64_t) {
         auto a = ocs::tom::where_compare(JB::descriptor.e, JB_owner, ocs::tom::CompareOp::Equal, owner);
         auto b = ocs::tom::where_compare(JB::descriptor.e, JB_priority, ocs::tom::CompareOp::Greater,
                                          std::uint32_t{100});
         ocs::tom::RawWherePtr where(ocs::tom::make_logic(ocs::tom::LogicOp::And, a.release(), b.release()));
         g_sink += where ? 1 : 0;
      });
   }
#else
   static_cast<void>(form);
   run("where_build", [&](std::uint64_t) {
      lCondition *where = lWhere("%T(%I == %s && %I > %u)", JB_Type, JB_owner, owner, JB_priority,
                                 static_cast<u_long32>(100));
      g_sink += where != nullptr ? 1 : 0;
      lFreeWhere(&where);
   });
#endif
}

/**
 * @brief Build and free a field selection of three fields (lWhat "%T(%I %I %I)")
 * @param f the fixture
 * @param form the form measured
 */
static void op_what_build(Fixture &f, const Form form) {
   static_cast<void>(f);
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      run("what_build", [&](std::uint64_t) {
         auto what = ocs::tom::what<JB_job_number, JB_owner, JB_priority>();
         g_sink += what.get() != nullptr ? 1 : 0;
      });
   } else {
      static const int fields[] = {JB_job_number, JB_owner, JB_priority};
      run("what_build", [&](std::uint64_t) {
         auto what = ocs::tom::what_of(JB::descriptor.e, fields);
         g_sink += what ? 1 : 0;
      });
   }
#else
   static_cast<void>(form);
   run("what_build", [&](std::uint64_t) {
      lEnumeration *what = lWhat("%T(%I %I %I)", JB_Type, JB_job_number, JB_owner, JB_priority);
      g_sink += what != nullptr ? 1 : 0;
      lFreeWhat(&what);
   });
#endif
}

#if OCS_PERF_TOM

/**
 * @brief The typed condition of the select operations: owner user3 and priority above 100
 * @return the condition
 */
static ocs::tom::Where<JB> typed_select_where() {
   return ocs::tom::field<JB_owner>() == static_cast<const char *>("user3") && ocs::tom::field<JB_priority>() > 100U;
}

/**
 * @brief The raw condition of the select operations: owner user3 and priority above 100
 * @return the condition
 */
static ocs::tom::RawWherePtr raw_select_where() {
   auto a = ocs::tom::where_compare(JB::descriptor.e, JB_owner, ocs::tom::CompareOp::Equal,
                                    static_cast<const char *>("user3"));
   auto b = ocs::tom::where_compare(JB::descriptor.e, JB_priority, ocs::tom::CompareOp::Greater, std::uint32_t{100});
   return ocs::tom::RawWherePtr(ocs::tom::make_logic(ocs::tom::LogicOp::And, a.release(), b.release()));
}

/// the fields of the raw selection of the select operations
static const int raw_select_fields[] = {JB_job_number, JB_owner, JB_priority};

#endif

/**
 * @brief Select 3 fields of the matching jobs of 1000 into a new list and free it (lSelect with where and what)
 * @param f the fixture
 * @param form the form measured
 */
static void op_select_1000(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const List<JB> list = *f.jobs_long;
      const auto where = typed_select_where();
      const auto what = ocs::tom::what<JB_job_number, JB_owner, JB_priority>();
      run("select_1000", [&](std::uint64_t) {
         auto result = list.select(where, what, "sel");
         g_sink += result->size();
      });
   } else {
      const lList *lp = f.jobs_long.get().raw();
      const auto where = raw_select_where();
      const auto what = ocs::tom::what_of(JB::descriptor.e, raw_select_fields);
      run("select_1000", [&](std::uint64_t) {
         auto result = ocs::tom::select(lp, where.get(), what.get());
         g_sink += ocs::tom::size(result.get().raw());
      });
   }
#else
   static_cast<void>(form);
   const lList *lp = f.jobs_long;
   lCondition *where = lWhere("%T(%I == %s && %I > %u)", JB_Type, JB_owner, "user3", JB_priority,
                              static_cast<u_long32>(100));
   lEnumeration *what = lWhat("%T(%I %I %I)", JB_Type, JB_job_number, JB_owner, JB_priority);
   run("select_1000", [&](std::uint64_t) {
      lList *result = lSelect("sel", lp, where, what);
      g_sink += lGetNumberOfElem(result);
      lFreeList(&result);
   });
   lFreeWhat(&what);
   lFreeWhere(&where);
#endif
}

/**
 * @brief Select all fields of the matching jobs of 1000 into a new list and free it (lSelect, what ALL)
 * @param f the fixture
 * @param form the form measured
 */
static void op_select_all_fields_1000(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const List<JB> list = *f.jobs_long;
      const auto where = typed_select_where();
      run("select_all_fields_1000", [&](std::uint64_t) {
         auto result = list.select(where, "sel");
         g_sink += result->size();
      });
   } else {
      const lList *lp = f.jobs_long.get().raw();
      const auto where = raw_select_where();
      const auto what = ocs::tom::what_all();
      run("select_all_fields_1000", [&](std::uint64_t) {
         auto result = ocs::tom::select(lp, where.get(), what.get());
         g_sink += ocs::tom::size(result.get().raw());
      });
   }
#else
   static_cast<void>(form);
   const lList *lp = f.jobs_long;
   lCondition *where = lWhere("%T(%I == %s && %I > %u)", JB_Type, JB_owner, "user3", JB_priority,
                              static_cast<u_long32>(100));
   lEnumeration *what = lWhat("%T(ALL)", JB_Type);
   run("select_all_fields_1000", [&](std::uint64_t) {
      lList *result = lSelect("sel", lp, where, what);
      g_sink += lGetNumberOfElem(result);
      lFreeList(&result);
   });
   lFreeWhat(&what);
   lFreeWhere(&where);
#endif
}

/**
 * @brief Select 3 fields of a job into a new element and free it (lSelectElemPack without buffer, no hash)
 * @param f the fixture
 * @param form the form measured
 */
static void op_select_elem(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<JB> job = *f.job;
      const auto what = ocs::tom::what<JB_job_number, JB_owner, JB_priority>();
      run("select_elem", [&](std::uint64_t) {
         auto result = job.select(what, ocs::tom::Hashing::Off);
         g_sink += result ? 1 : 0;
      });
   } else {
      const lListElem *ep = f.job.get().raw();
      const auto what = ocs::tom::what_of(JB::descriptor.e, raw_select_fields);
      run("select_elem", [&](std::uint64_t) {
         auto result = ocs::tom::select(ep, what.get(), ocs::tom::Hashing::Off);
         g_sink += result ? 1 : 0;
      });
   }
#else
   static_cast<void>(form);
   const lListElem *ep = f.job;
   lEnumeration *what = lWhat("%T(%I %I %I)", JB_Type, JB_job_number, JB_owner, JB_priority);
   run("select_elem", [&](std::uint64_t) {
      lListElem *result = lSelectElemPack(ep, nullptr, what, false, nullptr);
      g_sink += result != nullptr ? 1 : 0;
      lFreeElem(&result);
   });
   lFreeWhat(&what);
#endif
}

/**
 * @brief Reduce the job descriptor by a selection of 3 fields, create and free an element of it
 *        (lGetReducedDescr/lPartialDescr, lCreateElem)
 * @param f the fixture
 * @param form the form measured
 */
static void op_reduced_elem(Fixture &f, const Form form) {
   static_cast<void>(f);
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const auto what = ocs::tom::what<JB_job_number, JB_owner, JB_priority>();
      run("reduced_elem", [&](std::uint64_t) {
         auto ep = ElementPtr<JB>::create(what);
         g_sink += ep ? 1 : 0;
      });
   } else {
      const auto what = ocs::tom::what_of(JB::descriptor.e, raw_select_fields);
      run("reduced_elem", [&](std::uint64_t) {
         const lDescr *descr = ocs::tom::reduced_descr(JB::descriptor.e, what.get());
         auto ep = ocs::tom::create_raw_elem(descr);
         ocs::tom::release_descr(descr);
         g_sink += ep ? 1 : 0;
      });
   }
#else
   static_cast<void>(form);
   lEnumeration *what = lWhat("%T(%I %I %I)", JB_Type, JB_job_number, JB_owner, JB_priority);
   run("reduced_elem", [&](std::uint64_t) {
      lDescr *descr = lGetReducedDescr(JB_Type, what);
      lListElem *ep = lCreateElem(descr);
      g_sink += ep != nullptr ? 1 : 0;
      lFreeElem(&ep);
      sge_free(&descr);
   });
   lFreeWhat(&what);
#endif
}

/**
 * @brief Find the first job of 1000 that fulfils a condition, near the end (lFindFirstRW)
 * @param f the fixture
 * @param form the form measured
 */
static void op_find_if_1000(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const List<JB> list = *f.jobs_long;
      const auto where = ocs::tom::field<JB_job_number>() > 990U;
      run("find_if_1000", [&](std::uint64_t) { g_sink += list.find_if(where) ? 1 : 0; });
   } else {
      const lList *lp = f.jobs_long.get().raw();
      const auto where = ocs::tom::where_compare(JB::descriptor.e, JB_job_number, ocs::tom::CompareOp::Greater,
                                                 std::uint32_t{990});
      run("find_if_1000", [&](std::uint64_t) { g_sink += ocs::tom::find_first(lp, where.get()) != nullptr ? 1 : 0; });
   }
#else
   static_cast<void>(form);
   const lList *lp = f.jobs_long;
   lCondition *where = lWhere("%T(%I > %u)", JB_Type, JB_job_number, static_cast<u_long32>(990));
   run("find_if_1000", [&](std::uint64_t) { g_sink += lFindFirstRW(lp, where) != nullptr ? 1 : 0; });
   lFreeWhere(&where);
#endif
}

/**
 * @brief Test whether a job fulfils a condition of two comparisons (lCompare)
 * @param f the fixture
 * @param form the form measured
 */
static void op_matches(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<JB> job = *f.job;
      const auto where = typed_select_where();
      run("matches", [&](std::uint64_t) { g_sink += job.matches(where) ? 1 : 0; });
   } else {
      const lListElem *ep = f.job.get().raw();
      const auto where = raw_select_where();
      run("matches", [&](std::uint64_t) { g_sink += ocs::tom::matches(ep, where.get()) ? 1 : 0; });
   }
#else
   static_cast<void>(form);
   const lListElem *ep = f.job;
   lCondition *where = lWhere("%T(%I == %s && %I > %u)", JB_Type, JB_owner, "user3", JB_priority,
                              static_cast<u_long32>(100));
   run("matches", [&](std::uint64_t) { g_sink += lCompare(ep, where); });
   lFreeWhere(&where);
#endif
}

// ---------------------------------------------------------------------------------------------------------------
// group: moving many elements, on copies made outside the time

/**
 * @brief Split 1000 jobs by a condition into two lists (lSplit)
 * @param f the fixture
 * @param form the form measured
 */
static void op_split_1000(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const auto where = ocs::tom::field<JB_priority>() > 511U;
      ListPtr<JB> lp;
      ListPtr<JB> rest;
      run_batched("split_1000", 1, [&] { lp = f.jobs_long->clone("split"); },
                  [&](std::uint64_t) { rest = lp->split_off(where, "rest"); },
                  [&] {
                     lp.reset();
                     rest.reset();
                  });
   } else {
      const auto where = ocs::tom::where_compare(JB::descriptor.e, JB_priority, ocs::tom::CompareOp::Greater,
                                                 std::uint32_t{511});
      lList *lp = nullptr;
      lList *rest = nullptr;
      run_batched("split_1000", 1, [&] { lp = ocs::tom::clone(f.jobs_long.get().raw()).release(); },
                  [&](std::uint64_t) { ocs::tom::split_list(lp, &rest, "rest", where.get()); },
                  [&] {
                     ocs::tom::free_raw_list(lp);
                     ocs::tom::free_raw_list(rest);
                  });
   }
#else
   static_cast<void>(form);
   lCondition *where = lWhere("%T(%I > %u)", JB_Type, JB_priority, static_cast<u_long32>(511));
   lList *lp = nullptr;
   lList *rest = nullptr;
   run_batched("split_1000", 1, [&] { lp = lCopyList("split", f.jobs_long); },
               [&](std::uint64_t) { lSplit(&lp, &rest, "rest", where); },
               [&] {
                  lFreeList(&lp);
                  lFreeList(&rest);
               });
   lFreeWhere(&where);
#endif
}

/**
 * @brief Move all 100 jobs of one list to the end of another and free the emptied one (lAddList)
 * @param f the fixture
 * @param form the form measured
 */
static void op_add_list_100(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      ListPtr<JB> target;
      ListPtr<JB> source;
      run_batched("add_list_100", 1,
                  [&] {
                     target = f.jobs_100->clone("target");
                     source = f.jobs_100->clone("source");
                  },
                  [&](std::uint64_t) { target->splice(std::move(source)); }, [&] { target.reset(); });
   } else {
      ocs::tom::RawListPtr target;
      ocs::tom::RawListPtr source;
      run_batched("add_list_100", 1,
                  [&] {
                     target = ocs::tom::clone(f.jobs_100.get().raw());
                     source = ocs::tom::clone(f.jobs_100.get().raw());
                  },
                  [&](std::uint64_t) { ocs::tom::splice(target.get().raw(), std::move(source)); },
                  [&] { target.reset(); });
   }
#else
   static_cast<void>(form);
   lList *target = nullptr;
   lList *source = nullptr;
   run_batched("add_list_100", 1,
               [&] {
                  target = lCopyList("target", f.jobs_100);
                  source = lCopyList("source", f.jobs_100);
               },
               [&](std::uint64_t) { lAddList(target, &source); },
               [&] {
                  lFreeList(&target);
                  lFreeList(&source);
               });
#endif
}

/**
 * @brief Move all 100 jobs of one list to the end of another, keeping the emptied one (lAppendList)
 * @param f the fixture
 * @param form the form measured
 */
static void op_append_list_100(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      ListPtr<JB> target;
      ListPtr<JB> source;
      run_batched("append_list_100", 1,
                  [&] {
                     target = f.jobs_100->clone("target");
                     source = f.jobs_100->clone("source");
                  },
                  [&](std::uint64_t) { target->splice(*source); },
                  [&] {
                     target.reset();
                     source.reset();
                  });
   } else {
      ocs::tom::RawListPtr target;
      ocs::tom::RawListPtr source;
      run_batched("append_list_100", 1,
                  [&] {
                     target = ocs::tom::clone(f.jobs_100.get().raw());
                     source = ocs::tom::clone(f.jobs_100.get().raw());
                  },
                  [&](std::uint64_t) { ocs::tom::append_list(target.get().raw(), source.get().raw()); },
                  [&] {
                     target.reset();
                     source.reset();
                  });
   }
#else
   static_cast<void>(form);
   lList *target = nullptr;
   lList *source = nullptr;
   run_batched("append_list_100", 1,
               [&] {
                  target = lCopyList("target", f.jobs_100);
                  source = lCopyList("source", f.jobs_100);
               },
               [&](std::uint64_t) { lAppendList(target, source); },
               [&] {
                  lFreeList(&target);
                  lFreeList(&source);
               });
#endif
}

/**
 * @brief Take the second half of 1000 jobs out into a new list (lDechainList)
 * @param f the fixture
 * @param form the form measured
 */
static void op_dechain_list_1000(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      ListPtr<JB> lp;
      ListPtr<JB> tail;
      Element<JB> from;
      run_batched("dechain_list_1000", 1,
                  [&] {
                     lp = f.jobs_long->clone("list");
                     from = lp->find_first<JB_job_number>(long_hit + 1);
                  },
                  [&](std::uint64_t) { tail = lp->split_at(from, "tail"); },
                  [&] {
                     lp.reset();
                     tail.reset();
                  });
   } else {
      lList *lp = nullptr;
      lList *tail = nullptr;
      lListElem *from = nullptr;
      run_batched("dechain_list_1000", 1,
                  [&] {
                     lp = ocs::tom::clone(f.jobs_long.get().raw()).release();
                     from = ocs::tom::find_first_rw<JB_job_number>(lp, long_hit + 1);
                  },
                  [&](std::uint64_t) { tail = ocs::tom::split_from(lp, from, "tail"); },
                  [&] {
                     ocs::tom::free_raw_list(lp);
                     ocs::tom::free_raw_list(tail);
                  });
   }
#else
   static_cast<void>(form);
   lList *lp = nullptr;
   lList *tail = nullptr;
   lListElem *from = nullptr;
   run_batched("dechain_list_1000", 1,
               [&] {
                  lp = lCopyList("list", f.jobs_long);
                  from = lGetElemUlongRW(lp, JB_job_number, long_hit + 1);
               },
               [&](std::uint64_t) { lDechainList(lp, &tail, from); },
               [&] {
                  lFreeList(&lp);
                  lFreeList(&tail);
               });
#endif
}

/**
 * @brief Make 1000 strings with 100 distinct values unique (lUniqStr)
 * @param f the fixture
 * @param form the form measured
 */
static void op_uniq_str_1000(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      ListPtr<ST> lp;
      run_batched("uniq_str_1000", 1, [&] { lp = f.st_dup->clone("st"); },
                  [&](std::uint64_t) { lp->unique<ST_name>(); }, [&] { lp.reset(); });
   } else {
      ocs::tom::RawListPtr lp;
      run_batched("uniq_str_1000", 1, [&] { lp = ocs::tom::clone(f.st_dup.get().raw()); },
                  [&](std::uint64_t) { ocs::tom::unique_by_key(lp.get().raw(), ST_name); }, [&] { lp.reset(); });
   }
#else
   static_cast<void>(form);
   lList *lp = nullptr;
   run_batched("uniq_str_1000", 1, [&] { lp = lCopyList("st", f.st_dup); },
               [&](std::uint64_t) { lUniqStr(lp, ST_name); }, [&] { lFreeList(&lp); });
#endif
}

// ---------------------------------------------------------------------------------------------------------------
// group: list fields as a whole

/**
 * @brief Exchange the list of a list field with another list (lXchgList)
 * @param f the fixture
 * @param form the form measured
 */
static void op_xchg_list(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<JB> job = *f.scratch_b;
      auto other = ListPtr<VA>::create("other");
      run("xchg_list", [&](std::uint64_t) {
         auto old = job.exchange<JB_env_list>(std::move(other));
         other = std::move(old);
      });
   } else {
      lListElem *ep = f.scratch_b.get().raw();
      lList *other = ocs::tom::create_raw_list(VA::descriptor.e, ocs::tom::Hashing::On, "other").release();
      run("xchg_list", [&](std::uint64_t) { g_sink += ocs::tom::exchange<JB_env_list>(ep, &other); });
      ocs::tom::free_raw_list(other);
   }
#else
   static_cast<void>(form);
   lListElem *ep = f.scratch_b;
   lList *other = lCreateList("other", VA_Type);
   run("xchg_list", [&](std::uint64_t) { g_sink += lXchgList(ep, JB_env_list, &other); });
   lFreeList(&other);
#endif
}

/**
 * @brief Swap the lists of the list fields of two elements (lSwapList)
 * @param f the fixture
 * @param form the form measured
 */
static void op_swap_list(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<JB> a = *f.scratch_a;
      const Element<JB> b = *f.scratch_b;
      run("swap_list", [&](std::uint64_t) { a.swap<JB_ja_tasks, JB_ja_tasks>(b); });
   } else {
      lListElem *a = f.scratch_a.get().raw();
      lListElem *b = f.scratch_b.get().raw();
      run("swap_list", [&](std::uint64_t) { g_sink += ocs::tom::swap<JB_ja_tasks, JB_ja_tasks>(a, b); });
   }
#else
   static_cast<void>(form);
   lListElem *a = f.scratch_a;
   lListElem *b = f.scratch_b;
   run("swap_list", [&](std::uint64_t) { g_sink += lSwapList(a, JB_ja_tasks, b, JB_ja_tasks); });
#endif
}

/**
 * @brief Get the list of a list field that holds one (lGetOrCreateList)
 * @param f the fixture
 * @param form the form measured
 */
static void op_get_or_create_existing(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<JB> job = *f.job;
      run("get_or_create_existing", [&](std::uint64_t) {
         g_sink += job.get_or_create<JB_env_list>("env") ? 1 : 0;
      });
   } else {
      lListElem *ep = f.job.get().raw();
      run("get_or_create_existing", [&](std::uint64_t) {
         g_sink += ocs::tom::get_or_create<JB_env_list>(ep, "env") ? 1 : 0;
      });
   }
#else
   static_cast<void>(form);
   lListElem *ep = f.job;
   run("get_or_create_existing", [&](std::uint64_t) {
      g_sink += lGetOrCreateList(ep, JB_env_list, "env", VA_Type) != nullptr ? 1 : 0;
   });
#endif
}

/**
 * @brief Create the list of an empty list field and clear the field again (lGetOrCreateList/lSetList)
 * @param f the fixture
 * @param form the form measured
 */
static void op_get_or_create_new(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<JB> job = *f.scratch_a;
      run("get_or_create_new", [&](std::uint64_t) {
         g_sink += job.get_or_create<JB_context>("context") ? 1 : 0;
         job.set<JB_context>(nullptr);
      });
   } else {
      lListElem *ep = f.scratch_a.get().raw();
      run("get_or_create_new", [&](std::uint64_t) {
         g_sink += ocs::tom::get_or_create<JB_context>(ep, "context") ? 1 : 0;
         ocs::tom::set<JB_context>(ep, static_cast<lList *>(nullptr));
      });
   }
#else
   static_cast<void>(form);
   lListElem *ep = f.scratch_a;
   run("get_or_create_new", [&](std::uint64_t) {
      g_sink += lGetOrCreateList(ep, JB_context, "context", VA_Type) != nullptr ? 1 : 0;
      lSetList(ep, JB_context, nullptr);
   });
#endif
}

/**
 * @brief Get the reference of a list field (lGetListRef)
 * @param f the fixture
 * @param form the form measured
 */
static void op_list_ref(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<JB> job = *f.job;
      run("list_ref", [&](std::uint64_t) { g_sink += job.list_ref<JB_env_list>() != nullptr ? 1 : 0; });
   } else {
      const lListElem *ep = f.job.get().raw();
      run("list_ref", [&](std::uint64_t) { g_sink += ocs::tom::list_ref<JB_env_list>(ep) != nullptr ? 1 : 0; });
   }
#else
   static_cast<void>(form);
   const lListElem *ep = f.job;
   run("list_ref", [&](std::uint64_t) { g_sink += lGetListRef(ep, JB_env_list) != nullptr ? 1 : 0; });
#endif
}

// ---------------------------------------------------------------------------------------------------------------
// group: bit operations on a 32 bit field

/**
 * @brief Set bits of a 32 bit field (lOrUlongBitMask)
 * @param f the fixture
 * @param form the form measured
 */
static void op_bits_or(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<QU> qu = *f.qu;
      run("bits_or", [&](std::uint64_t i) {
         g_sink += qu.or_bits<QU_state>(static_cast<std::uint32_t>(1U << (i & 7)));
      });
   } else {
      lListElem *ep = f.qu.get().raw();
      run("bits_or", [&](std::uint64_t i) {
         g_sink += ocs::tom::or_bits<QU_state>(ep, static_cast<std::uint32_t>(1U << (i & 7)));
      });
   }
#else
   static_cast<void>(form);
   lListElem *ep = f.qu;
   run("bits_or", [&](std::uint64_t i) { g_sink += lOrUlongBitMask(ep, QU_state, 1U << (i & 7)); });
#endif
}

/**
 * @brief Keep bits of a 32 bit field (lAndUlongBitMask)
 * @param f the fixture
 * @param form the form measured
 */
static void op_bits_and(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<QU> qu = *f.qu;
      run("bits_and", [&](std::uint64_t) { g_sink += qu.and_bits<QU_state>(0xffU); });
   } else {
      lListElem *ep = f.qu.get().raw();
      run("bits_and", [&](std::uint64_t) { g_sink += ocs::tom::and_bits<QU_state>(ep, 0xffU); });
   }
#else
   static_cast<void>(form);
   lListElem *ep = f.qu;
   run("bits_and", [&](std::uint64_t) { g_sink += lAndUlongBitMask(ep, QU_state, 0xffU); });
#endif
}

/**
 * @brief Clear bits of a 32 bit field (lClearUlongBitMask)
 * @param f the fixture
 * @param form the form measured
 */
static void op_bits_clear(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<QU> qu = *f.qu;
      run("bits_clear", [&](std::uint64_t i) {
         g_sink += qu.clear_bits<QU_state>(static_cast<std::uint32_t>(1U << (i & 7)));
      });
   } else {
      lListElem *ep = f.qu.get().raw();
      run("bits_clear", [&](std::uint64_t i) {
         g_sink += ocs::tom::clear_bits<QU_state>(ep, static_cast<std::uint32_t>(1U << (i & 7)));
      });
   }
#else
   static_cast<void>(form);
   lListElem *ep = f.qu;
   run("bits_clear", [&](std::uint64_t i) { g_sink += lClearUlongBitMask(ep, QU_state, 1U << (i & 7)); });
#endif
}

/**
 * @brief Test bits of a 32 bit field (lMatchUlongBitMask)
 * @param f the fixture
 * @param form the form measured
 */
static void op_bits_match(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<QU> qu = *f.qu;
      run("bits_match", [&](std::uint64_t i) {
         g_sink += qu.has_any_bits<QU_state>(static_cast<std::uint32_t>(1U << (i & 7))) ? 1 : 0;
      });
   } else {
      const lListElem *ep = f.qu.get().raw();
      run("bits_match", [&](std::uint64_t i) {
         g_sink += ocs::tom::has_any_bits<QU_state>(ep, static_cast<std::uint32_t>(1U << (i & 7))) ? 1 : 0;
      });
   }
#else
   static_cast<void>(form);
   lListElem *ep = f.qu;
   run("bits_match", [&](std::uint64_t i) { g_sink += lMatchUlongBitMask(ep, QU_state, 1U << (i & 7)) ? 1 : 0; });
#endif
}

// ---------------------------------------------------------------------------------------------------------------
// group: packing; the buffers carry no sender information in either variant

/**
 * @brief Pack a list of 100 jobs with sub-lists and objects into a new buffer (cull_pack_list)
 * @param f the fixture
 * @param form the form measured
 */
static void op_pack_jobs_100(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const List<JB> list = *f.jobs_pack;
      run("pack_jobs_100", [&](std::uint64_t) {
         ocs::tom::PackBuffer pb(false);
         list.pack(pb.raw());
         g_sink += pb.size();
      });
   } else {
      const lList *lp = f.jobs_pack.get().raw();
      run("pack_jobs_100", [&](std::uint64_t) {
         sge_pack_buffer pb;
         ocs::tom::init_buffer(&pb, 0, false, false);
         ocs::tom::pack_list(&pb, lp, nullptr, 0);
         g_sink += pb.bytes_used;
         ocs::tom::clear_buffer(&pb);
      });
   }
#else
   static_cast<void>(form);
   const lList *lp = f.jobs_pack;
   run("pack_jobs_100", [&](std::uint64_t) {
      sge_pack_buffer pb;
      init_packbuffer(&pb, 0, false, false);
      cull_pack_list(&pb, lp);
      g_sink += pb.bytes_used;
      clear_packbuffer(&pb);
   });
#endif
}

/**
 * @brief Unpack a list of 100 jobs with sub-lists and objects from a copy of the packed bytes (cull_unpack_list)
 *
 * The bytes are packed once by the same variant and form; each call copies them into a buffer it owns, as the
 * reader of a message does.
 *
 * @param f the fixture
 * @param form the form measured
 */
static void op_unpack_jobs_100(Fixture &f, const Form form) {
   std::vector<char> bytes;
#if OCS_PERF_TOM
   {
      ocs::tom::PackBuffer pb(false);
      f.jobs_pack->pack(pb.raw());
      bytes.assign(pb.data(), pb.data() + pb.size());
   }
   if (form == Form::Typed) {
      run("unpack_jobs_100", [&](std::uint64_t) {
         ocs::tom::PackBuffer pb = ocs::tom::PackBuffer::reading(bytes.data(), bytes.size(), false);
         auto lp = ListPtr<JB>::unpack(pb.raw());
         g_sink += lp->size();
      });
   } else {
      run("unpack_jobs_100", [&](std::uint64_t) {
         char *buf = sge_malloc(bytes.size());
         memcpy(buf, bytes.data(), bytes.size());
         sge_pack_buffer pb;
         ocs::tom::init_buffer_from(&pb, buf, static_cast<std::uint32_t>(bytes.size()), false);
         auto lp = ocs::tom::unpack_raw_list(&pb);
         g_sink += ocs::tom::size(lp.get().raw());
         ocs::tom::clear_buffer(&pb);
      });
   }
#else
   static_cast<void>(form);
   {
      sge_pack_buffer pb;
      init_packbuffer(&pb, 0, false, false);
      cull_pack_list(&pb, f.jobs_pack);
      bytes.assign(pb.head_ptr, pb.head_ptr + pb.bytes_used);
      clear_packbuffer(&pb);
   }
   run("unpack_jobs_100", [&](std::uint64_t) {
      char *buf = sge_malloc(bytes.size());
      memcpy(buf, bytes.data(), bytes.size());
      sge_pack_buffer pb;
      init_packbuffer_from_buffer(&pb, buf, static_cast<u_long32>(bytes.size()), false);
      lList *lp = nullptr;
      cull_unpack_list(&pb, &lp);
      g_sink += lGetNumberOfElem(lp);
      lFreeList(&lp);
      clear_packbuffer(&pb);
   });
#endif
}

/**
 * @brief Pack a job with sub-lists and an object into a new buffer (cull_pack_elem)
 * @param f the fixture
 * @param form the form measured
 */
static void op_pack_elem_job(Fixture &f, const Form form) {
#if OCS_PERF_TOM
   if (form == Form::Typed) {
      const Element<JB> job = *f.job;
      run("pack_elem_job", [&](std::uint64_t) {
         ocs::tom::PackBuffer pb(false);
         job.pack(pb.raw());
         g_sink += pb.size();
      });
   } else {
      const lListElem *ep = f.job.get().raw();
      run("pack_elem_job", [&](std::uint64_t) {
         sge_pack_buffer pb;
         ocs::tom::init_buffer(&pb, 0, false, false);
         ocs::tom::pack_elem(&pb, ep, nullptr, 0);
         g_sink += pb.bytes_used;
         ocs::tom::clear_buffer(&pb);
      });
   }
#else
   static_cast<void>(form);
   const lListElem *ep = f.job;
   run("pack_elem_job", [&](std::uint64_t) {
      sge_pack_buffer pb;
      init_packbuffer(&pb, 0, false, false);
      cull_pack_elem(&pb, ep);
      g_sink += pb.bytes_used;
      clear_packbuffer(&pb);
   });
#endif
}

// ---------------------------------------------------------------------------------------------------------------
// catalogue and main

/// one measured operation: called once per form
using Operation = std::function<void(Fixture &, Form)>;

/**
 * @brief The catalogue of operations, in output order
 * @return the operations
 */
static std::vector<Operation> catalogue() {
   std::vector<Operation> ops = {
      op_elem_create_free_jb, op_elem_create_free_ce, op_list_create_free, op_list_fill_free_100,
      op_get_u32, op_set_u32, op_set_u32_hashed, op_get_u64, op_set_u64, op_get_str, op_set_str,
      op_get_host, op_set_host, op_get_double, op_set_double, op_get_bool, op_set_bool, op_get_list,
      op_set_list, op_get_object, op_set_object, op_add_u32, op_add_double,
      op_copy_elem_ce, op_copy_elem_job, op_copy_list_short, op_copy_list_long, op_copy_list_jobs_100,
      op_append_dechain, op_insert_front_dechain, op_insert_middle_dechain, op_append_remove, op_walk_1000,
      op_size};
   const Kind kinds[] = {Kind::Str, Kind::U32, Kind::U64, Kind::Host};
   const Size sizes[] = {Size::Short, Size::Long};
   for (const Kind kind : kinds) {
      for (const Size size : sizes) {
         for (const bool hit : {true, false}) {
            ops.emplace_back([kind, size, hit](Fixture &f, const Form form) { op_find(f, form, kind, size, hit); });
         }
      }
   }
   for (const Kind kind : {Kind::Str, Kind::Host}) {
      for (const Size size : sizes) {
         ops.emplace_back([kind, size](Fixture &f, const Form form) { op_find_all(f, form, kind, size); });
      }
   }
   ops.emplace_back(op_hash_build_1000);
   for (const Kind kind : kinds) {
      for (const Size size : sizes) {
         ops.emplace_back([kind, size](Fixture &f, const Form form) { op_add_del(f, form, kind, size); });
      }
   }
   for (const Kind kind : kinds) {
      for (const Size size : sizes) {
         for (const bool hit : {true, false}) {
            ops.emplace_back([kind, size, hit](Fixture &f, const Form form) {
               op_sub_find(f, form, kind, size, hit);
            });
         }
      }
   }
   for (const Kind kind : kinds) {
      for (const Size size : sizes) {
         ops.emplace_back([kind, size](Fixture &f, const Form form) { op_sub_add(f, form, kind, size); });
      }
   }
   for (const Kind kind : kinds) {
      for (const Size size : sizes) {
         ops.emplace_back([kind, size](Fixture &f, const Form form) { op_sub_del(f, form, kind, size); });
      }
   }
   const Operation rest[] = {
      op_sub_add_list_short,
      op_sort_u32_asc, op_sort_u32_desc, op_sort_u64_asc, op_sort_str_asc, op_sort_str_desc, op_sort_3keys,
      op_insert_sorted,
      op_where_build, op_what_build, op_select_1000, op_select_all_fields_1000, op_select_elem, op_reduced_elem,
      op_find_if_1000, op_matches,
      op_split_1000, op_add_list_100, op_append_list_100, op_dechain_list_1000, op_uniq_str_1000,
      op_xchg_list, op_swap_list, op_get_or_create_existing, op_get_or_create_new, op_list_ref,
      op_bits_or, op_bits_and, op_bits_clear, op_bits_match,
      op_pack_jobs_100, op_unpack_jobs_100, op_pack_elem_job};
   ops.insert(ops.end(), std::begin(rest), std::end(rest));
   return ops;
}

/**
 * @brief The api column of a form
 * @param form the form
 * @return "cull", "tom-typed" or "tom-raw"
 */
static const char *api_of(const Form form) {
   switch (form) {
      case Form::Typed:
         return "tom-typed";
      case Form::Raw:
         return "tom-raw";
      default:
         return "cull";
   }
}

/**
 * @brief Measure every operation of the catalogue in the forms asked for and print the CSV lines
 *
 * Options: `-mode typed|raw|both` selects the TOM forms (default both); the CULL variant ignores it.
 *
 * @param argc the number of arguments
 * @param argv the arguments
 * @return 0, or 1 for a wrong option
 */
int main(int argc, char *argv[]) {
   bool typed = true;
   bool raw = true;
   for (int i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-mode") == 0 && i + 1 < argc) {
         const char *mode = argv[++i];
         typed = strcmp(mode, "typed") == 0 || strcmp(mode, "both") == 0;
         raw = strcmp(mode, "raw") == 0 || strcmp(mode, "both") == 0;
         if (!typed && !raw) {
            fprintf(stderr, "usage: %s [-mode typed|raw|both]\n", argv[0]);
            return 1;
         }
      } else {
         fprintf(stderr, "usage: %s [-mode typed|raw|both]\n", argv[0]);
         return 1;
      }
   }

#if OCS_PERF_TOM
   ocs::tom::install_model(ocs::tom::gcs_model());
   std::vector<Form> forms;
   if (typed) {
      forms.push_back(Form::Typed);
   }
   if (raw) {
      forms.push_back(Form::Raw);
   }
#else
   lInit(nmv);
   const std::vector<Form> forms = {Form::Cull};
#endif

   init_keys();
   Fixture f{};
   build_fixture(f);

   printf("api,operation,calls,ns_per_op\n");
   for (const Operation &op : catalogue()) {
      for (const Form form : forms) {
         g_api = api_of(form);
         op(f, form);
      }
   }

   free_fixture(f);
   fprintf(stderr, "# sink %" PRIu64 "\n", g_sink);
   return 0;
}
