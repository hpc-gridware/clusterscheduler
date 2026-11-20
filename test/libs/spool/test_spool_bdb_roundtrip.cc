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
 * @brief Round trip of objects through the Berkeley DB spooling backend
 *
 * Creates a Berkeley DB spooling context in a fresh temporary directory, the way spoolinit does it (startup
 * without a check, then the init maintenance command creates the databases), writes objects of every shape the
 * backend treats specially and reads them back through the public spooling API:
 *
 * - a complex entry, bound in a list, and written twice under the same key;
 * - a userset with a sub-list, written as a free element (the borrow-into-a-temporary-list path);
 * - a cluster queue with two queue instances, which the backend stores as separate records and joins on reading;
 * - an array job with two array tasks and a parallel task, which the backend stores as separate records.
 *
 * For every record the bytes stored under the expected key are compared with an independent packing of the
 * expected object, so a change in the key layout, in the written bytes or in what is split off a record shows up.
 * The objects read back are compared field by field with object_has_differences(), and the objects written are
 * checked to be unchanged and to own the same sub-lists afterwards.
 *
 * The validation callbacks of the rule are replaced by counting no-ops: the default ones need the master lists of
 * a running qmaster (exec hosts, complexes) and initialise fields that are not spooled, which would make a field
 * by field comparison meaningless.
 *
 * Environment: no qmaster and no installed SGE_ROOT. Hashing a host field consults ocs::Bootstrap, so a
 * throwaway bootstrap is installed below the temporary directory, as test_spool_golden does.
 */

#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <unistd.h>

#include "uti/ocs_Bootstrap.h"
#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/ocs_CEntry.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_attr.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_userset.h"

#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_PackCore.h"
#include "tom/ocs_Raw.h"

#include "spool/sge_spooling.h"
#include "spool/ocs_spool_duplicates.h"
#include "spool/berkeleydb/sge_bdb_types.h"
#include "spool/berkeleydb/sge_spooling_berkeleydb.h"

namespace tom = ocs::tom;

namespace {

// ---------------------------------------------------------------------------------------------------------------
// test infrastructure

int g_failures = 0;          ///< number of failed checks
int g_known = 0;             ///< number of checks that hit a known deviation of the product
int g_validated = 0;         ///< calls of the object validation callback
int g_validated_lists = 0;   ///< calls of the list validation callback

/// the pack flags of the backend (pack_part in sge_bdb.cc)
constexpr int PACK_PART = CULL_SPOOL | CULL_SUBLIST | CULL_SPOOL_PROJECT | CULL_SPOOL_USER;

constexpr uint32_t JOB_ID = 4711;          ///< the job number of the array job
constexpr const char *QUEUE = "bdb.q";     ///< the cluster queue name
constexpr const char *HOST1 = "bdbhost1";  ///< the host of the first queue instance
constexpr const char *HOST2 = "bdbhost2";  ///< the host of the second queue instance
constexpr const char *PE_TASK = "1.bdbhost1";   ///< the id of the parallel task

/** @def CHECK
 * @brief Assert one condition and record the result
 *
 * @param id the test number, printed as `[SPOOL-BDB-nn]`
 * @param label what the check is about
 * @param expr the condition that must hold
 */
#define CHECK(id, label, expr) \
   do { \
      if (!(expr)) { \
         printf("FAIL  [SPOOL-BDB-%02d] %s\n", (id), (label)); \
         ++g_failures; \
      } else { \
         printf("ok    [SPOOL-BDB-%02d] %s\n", (id), (label)); \
      } \
   } while (0)

/**
 * @brief Print the error messages of an answer list and tell whether it holds an error
 * @param alp the answer list, emptied afterwards
 * @param what the call the answers are about
 * @return true when the list holds no error
 */
bool answers_ok(tom::ListPtr<tom::AN> &alp, const char *what) {
   const bool ok = !answer_list_has_error(&alp);
   if (!ok) {
      for (const auto an : *alp) {
         const char *text = an.get<AN_text>();
         printf("      %s: %s\n", what, text != nullptr ? text : "");
      }
   }
   alp.reset();
   return ok;
}

/**
 * @brief Two elements hold the same fields with the same values, sub-lists included
 * @param a the first element
 * @param b the second element
 * @return true when object_has_differences() finds no difference
 */
bool same(const tom::ConstRawElement a, const tom::ConstRawElement b) {
   tom::ListPtr<tom::AN> alp;
   return !object_has_differences(a, &alp, b);
}

/**
 * @brief The object validation callback of the rule: counts the calls and accepts every object
 * @param answer_list unused
 * @param type unused
 * @param rule unused
 * @param object unused
 * @param object_type unused
 * @return true
 */
bool count_validate([[maybe_unused]] tom::ListPtr<tom::AN> *answer_list, [[maybe_unused]] const lListElem *type,
                    [[maybe_unused]] const lListElem *rule, [[maybe_unused]] lListElem *object,
                    [[maybe_unused]] const sge_object_type object_type) {
   ++g_validated;
   return true;
}

/**
 * @brief The list validation callback of the rule: counts the calls and accepts every list
 * @param answer_list unused
 * @param type unused
 * @param rule unused
 * @param object_type unused
 * @return true
 */
bool count_validate_list([[maybe_unused]] tom::ListPtr<tom::AN> *answer_list, [[maybe_unused]] const lListElem *type,
                         [[maybe_unused]] const lListElem *rule, [[maybe_unused]] const sge_object_type object_type) {
   ++g_validated_lists;
   return true;
}

// ---------------------------------------------------------------------------------------------------------------
// database access

/**
 * @brief The record key of an object as the backend builds it: the object type name, a colon and the key
 * @param object_type the object type
 * @param key the key that follows the colon
 * @return the record key
 */
std::string db_key(const sge_object_type object_type, const std::string &key) {
   return std::string(object_type_get_name(object_type)) + ":" + key;
}

/**
 * @brief The job part of a record key, as the backend formats the numbers
 * @param job_id the job number
 * @return the job number right aligned in ten characters
 */
std::string job_part(const uint32_t job_id) {
   char buffer[32];
   snprintf(buffer, sizeof(buffer), "%10" PRIu32, job_id);
   return buffer;
}

/**
 * @brief The job and array task part of a record key, as the backend formats the numbers
 * @param job_id the job number
 * @param ja_task_id the array task number
 * @return both numbers right aligned in ten characters, separated by a dot
 */
std::string task_part(const uint32_t job_id, const uint32_t ja_task_id) {
   char buffer[32];
   snprintf(buffer, sizeof(buffer), "%10" PRIu32 ".%10" PRIu32, job_id, ja_task_id);
   return buffer;
}

/**
 * @brief The record stored under a key holds exactly the packing of an element
 *
 * The element is packed independently with the flags of the backend, so that a change of the key, of the object
 * written or of the way it is packed (bound or free, with or without a sub-list) shows up.
 *
 * @param info the database handle of the rule
 * @param database the database the record is expected in
 * @param key the full record key
 * @param expected the element the record must hold, bound in a list as the backend writes every element
 * @return true when the record exists and its bytes are those of the packed element
 */
bool record_has_bytes(const bdb_info info, const bdb_database database, const std::string &key,
                      const lListElem *expected) {
   sge_pack_buffer pb;
   if (tom::try_init_buffer(&pb, 8192, false, false) != PACK_SUCCESS) {
      printf("      cannot initialise a pack buffer\n");
      return false;
   }
   bool ok = true;
   try {
      tom::pack_elem(&pb, expected, nullptr, PACK_PART);
   } catch (const tom::PackException &) {
      printf("      cannot pack the expected object of %s\n", key.c_str());
      ok = false;
   }

   DB *db = bdb_get_db(info, database);
   if (db == nullptr) {
      printf("      database %d is not open\n", static_cast<int>(database));
      ok = false;
   }
   if (ok) {
      DBT key_dbt;
      DBT data_dbt;
      memset(&key_dbt, 0, sizeof(key_dbt));
      memset(&data_dbt, 0, sizeof(data_dbt));
      key_dbt.data = const_cast<char *>(key.c_str());
      key_dbt.size = static_cast<uint32_t>(key.size() + 1);
      data_dbt.flags = DB_DBT_MALLOC;
      const int dbret = db->get(db, nullptr, &key_dbt, &data_dbt, 0);
      if (dbret != 0) {
         printf("      no record under %s: %s\n", key.c_str(), db_strerror(dbret));
         ok = false;
      } else {
         if (data_dbt.size != pb.bytes_used || memcmp(data_dbt.data, pb.head_ptr, pb.bytes_used) != 0) {
            printf("      record %s: %u bytes stored, %zu bytes expected, or the bytes differ\n", key.c_str(),
                   data_dbt.size, pb.bytes_used);
            ok = false;
         }
         free(data_dbt.data);
      }
   }
   tom::clear_buffer(&pb);
   return ok;
}

/**
 * @brief The record stored under a key holds exactly the packing of an element, which is bound for the packing
 * @tparam T the object type
 * @param info the database handle of the rule
 * @param database the database the record is expected in
 * @param key the full record key
 * @param expected the expected element, free; it is appended to a list of its own
 * @return true when the record exists and its bytes are those of the packed element
 */
template <typename T>
bool record_matches(const bdb_info info, const bdb_database database, const std::string &key,
                    tom::ElementPtr<T> &&expected) {
   auto list = tom::ListPtr<T>::create("expected");
   const auto bound = list->push_back(std::move(expected));
   return record_has_bytes(info, database, key, bound.raw());
}

/**
 * @brief The keys of all records whose key starts with a prefix, in database order
 * @param context the spooling context
 * @param prefix the key prefix, e.g. `JATASK:`
 * @return the keys
 */
std::vector<std::string> read_keys(const lListElem *context, const std::string &prefix) {
   tom::ListPtr<tom::AN> alp;
   lList *lp = nullptr;
   spool_read_keys(&alp, context, &lp, prefix.c_str());
   answers_ok(alp, "spool_read_keys");
   const auto keys = tom::ListPtr<tom::STU>::from_typed(lp);
   std::vector<std::string> result;
   for (const auto key : keys.get()) {
      result.emplace_back(key.get<STU_name>());
   }
   return result;
}

/**
 * @brief Read the list of one object type through spool_read_list()
 * @tparam T the object type
 * @param context the spooling context
 * @param object_type the object type
 * @param[out] ok receives whether the read succeeded without an error answer
 * @return the list read, empty if no record matched
 */
template <typename T>
tom::ListPtr<T> read_list(const lListElem *context, const sge_object_type object_type, bool *ok) {
   tom::ListPtr<tom::AN> alp;
   lList *lp = nullptr;
   const bool read = spool_read_list(&alp, context, &lp, object_type);
   *ok = answers_ok(alp, "spool_read_list") && read;
   // every record is unpacked with the descriptor of the object type
   return tom::ListPtr<T>::from_typed(lp);
}

/**
 * @brief Write an object through spool_write_object()
 * @param context the spooling context
 * @param object the object
 * @param key the key handed to the framework
 * @param object_type the object type
 * @return true when the write succeeded without an error answer
 */
bool write_ok(const lListElem *context, const tom::ConstRawElement object, const char *key,
           const sge_object_type object_type) {
   tom::ListPtr<tom::AN> alp;
   const bool written = spool_write_object(&alp, context, object.raw(), key, object_type);
   return answers_ok(alp, "spool_write_object") && written;
}

/**
 * @brief Delete an object through spool_delete_object()
 * @param context the spooling context
 * @param object_type the object type
 * @param key the key handed to the framework
 * @param quiet do not print the error answers, for a delete that is expected to fail
 * @return true when the delete succeeded
 */
bool delete_ok(const lListElem *context, const sge_object_type object_type, const char *key, const bool quiet = false) {
   tom::ListPtr<tom::AN> alp;
   const bool deleted = spool_delete_object(&alp, context, object_type, key);
   if (quiet) {
      alp.reset();
      return deleted;
   }
   return answers_ok(alp, "spool_delete_object") && deleted;
}

// ---------------------------------------------------------------------------------------------------------------
// objects

/**
 * @brief A complex entry, every spooled field set
 * @param defaultval the default value, so that two versions differ
 * @return the complex entry, free
 */
tom::ElementPtr<tom::CE> make_centry(const char *defaultval) {
   auto ce = tom::ElementPtr<tom::CE>::create();
   ce->set<CE_name>("bdb_mem");
   ce->set<CE_shortcut>("bm");
   ce->set<CE_valtype>(static_cast<uint32_t>(ocs::CEntry::Type::MEM));
   ce->set<CE_relop>(static_cast<uint32_t>(CMPLXLE_OP));
   ce->set<CE_requestable>(static_cast<uint32_t>(REQU_YES));
   ce->set<CE_consumable>(static_cast<uint32_t>(CONSUMABLE_JOB));
   ce->set<CE_defaultval>(defaultval);
   ce->set<CE_urgency_weight>("100");
   return ce;
}

/**
 * @brief An access list with three entries
 * @return the userset, free
 */
tom::ElementPtr<tom::US> make_userset() {
   auto us = tom::ElementPtr<tom::US>::create();
   us->set<US_name>("bdb_acl");
   us->set<US_type>(static_cast<uint32_t>(US_ACL | US_DEPT));
   us->set<US_fshare>(12);
   us->set<US_oticket>(34);
   auto entries = tom::ListPtr<tom::UE>::create("entries");
   for (const char *name : {"alice", "bob", "@staff"}) {
      auto entry = tom::ElementPtr<tom::UE>::create();
      entry->set<UE_name>(name);
      entries->push_back(std::move(entry));
   }
   us->set<US_entries>(std::move(entries));
   return us;
}

/**
 * @brief A queue instance of QUEUE, only spooled fields set
 * @param host the host of the instance
 * @param version the instance version, so that two instances differ beyond the host
 * @return the queue instance, free
 */
tom::ElementPtr<tom::QU> make_qinstance(const char *host, const uint32_t version) {
   auto qu = tom::ElementPtr<tom::QU>::create();
   qu->set<QU_qname>(QUEUE);
   qu->set<QU_qhostname>(ocs::tom::Host{host});
   qu->set<QU_state>(version);
   qu->set<QU_version>(version);
   return qu;
}

/**
 * @brief A cluster queue with two attributes and two queue instances
 *
 * CQ_hostlist is not set: it carries no spool flag, so the backend does not write it.
 *
 * @return the cluster queue, free
 */
tom::ElementPtr<tom::CQ> make_cqueue() {
   auto cq = tom::ElementPtr<tom::CQ>::create();
   cq->set<CQ_name>(QUEUE);

   auto seq_no = tom::ListPtr<tom::AULNG>::create("seq_no");
   auto def = tom::ElementPtr<tom::AULNG>::create();
   def->set<AULNG_href>(ocs::tom::Host{HOSTREF_DEFAULT});
   def->set<AULNG_value>(3);
   seq_no->push_back(std::move(def));
   cq->set<CQ_seq_no>(std::move(seq_no));

   auto slots = tom::ListPtr<tom::AULNG>::create("job_slots");
   auto slots_def = tom::ElementPtr<tom::AULNG>::create();
   slots_def->set<AULNG_href>(ocs::tom::Host{HOSTREF_DEFAULT});
   slots_def->set<AULNG_value>(4);
   slots->push_back(std::move(slots_def));
   auto slots_host = tom::ElementPtr<tom::AULNG>::create();
   slots_host->set<AULNG_href>(ocs::tom::Host{HOST2});
   slots_host->set<AULNG_value>(8);
   slots->push_back(std::move(slots_host));
   cq->set<CQ_job_slots>(std::move(slots));

   auto qinstances = tom::ListPtr<tom::QU>::create("qinstances");
   qinstances->push_back(make_qinstance(HOST1, 1));
   qinstances->push_back(make_qinstance(HOST2, 2));
   cq->set<CQ_qinstances>(std::move(qinstances));
   return cq;
}

/**
 * @brief An array job with the tasks 1 and 2, task 1 running a parallel task
 * @return the job, free
 */
tom::ElementPtr<tom::JB> make_job() {
   auto job = tom::ElementPtr<tom::JB>::create();
   job->set<JB_job_number>(JOB_ID);
   job->set<JB_job_name>("bdb_roundtrip");
   job->set<JB_owner>("bdbuser");
   job->set<JB_uid>(1234);
   job->set<JB_submission_time>(static_cast<uint64_t>(1767225600000000));

   auto structure = tom::ListPtr<tom::RN>::create("ja_structure");
   auto range = tom::ElementPtr<tom::RN>::create();
   range->set<RN_min>(1);
   range->set<RN_max>(2);
   range->set<RN_step>(1);
   structure->push_back(std::move(range));
   job->set<JB_ja_structure>(std::move(structure));

   auto tasks = tom::ListPtr<tom::JAT>::create("ja_tasks");
   for (uint32_t task_id = 1; task_id <= 2; task_id++) {
      auto task = tom::ElementPtr<tom::JAT>::create();
      task->set<JAT_task_number>(task_id);
      task->set<JAT_status>(static_cast<uint32_t>(task_id == 1 ? JRUNNING : JIDLE));
      task->set<JAT_master_queue>(task_id == 1 ? "bdb.q@bdbhost1" : nullptr);
      if (task_id == 1) {
         auto pe_tasks = tom::ListPtr<tom::PET>::create("pe_tasks");
         auto pe_task = tom::ElementPtr<tom::PET>::create();
         pe_task->set<PET_id>(PE_TASK);
         pe_task->set<PET_name>("bdb_pe_task");
         pe_task->set<PET_status>(static_cast<uint32_t>(JRUNNING));
         pe_tasks->push_back(std::move(pe_task));
         task->set<JAT_task_list>(std::move(pe_tasks));
      }
      tasks->push_back(std::move(task));
   }
   job->set<JB_ja_tasks>(std::move(tasks));
   return job;
}

// ---------------------------------------------------------------------------------------------------------------
// tests

/**
 * @brief A bound complex entry: write, record bytes, read back, write the same key again
 * @param context the spooling context
 * @param info the database handle of the rule
 */
void test_centry(const lListElem *context, const bdb_info info) {
   printf("\n--- bound element with a unique key (complex entry) ---\n");

   auto list = tom::ListPtr<tom::CE>::create("centries");
   const auto ce = list->push_back(make_centry("1G"));
   const auto before = ce.clone();

   CHECK(4, "a bound complex entry is written", write_ok(context, ce, "bdb_mem", SGE_TYPE_CENTRY));
   CHECK(5, "the bound complex entry stays bound and unchanged", !tom::is_free(ce) && same(ce, *before));
   CHECK(6, "the record COMPLEX_ENTRY:bdb_mem holds the packed complex entry",
         record_matches(info, BDB_CONFIG_DB, db_key(SGE_TYPE_CENTRY, "bdb_mem"), ce.clone()));

   // the same key again, with another value: the record is replaced
   auto second_list = tom::ListPtr<tom::CE>::create("centries");
   const auto second = second_list->push_back(make_centry("2G"));
   CHECK(7, "the same key is written a second time", write_ok(context, second, "bdb_mem", SGE_TYPE_CENTRY));
   CHECK(8, "one key COMPLEX_ENTRY:bdb_mem after two writes",
         read_keys(context, db_key(SGE_TYPE_CENTRY, "")) ==
            std::vector<std::string>{db_key(SGE_TYPE_CENTRY, "bdb_mem")});

   const int validated = g_validated;
   const int validated_lists = g_validated_lists;
   bool ok = false;
   const auto read = read_list<tom::CE>(context, SGE_TYPE_CENTRY, &ok);
   CHECK(9, "the complex entries are read back", ok && read && read->size() == 1);
   CHECK(10, "the complex entry read back is the second version",
         ok && read && read->size() == 1 && same(read->front(), second));
   CHECK(11, "each object and the list were handed to the validation once",
         g_validated == validated + 1 && g_validated_lists == validated_lists + 1);

   tom::ListPtr<tom::AN> alp;
   auto single = tom::RawElementPtr(spool_read_object(&alp, context, SGE_TYPE_CENTRY,
                                                      db_key(SGE_TYPE_CENTRY, "bdb_mem").c_str()));
   CHECK(12, "spool_read_object() reads the complex entry by its record key",
         answers_ok(alp, "spool_read_object") && single && same(single.get(), second));

   // a second record whose object holds the same name is filtered out on reading and recorded as a duplicate
   ocs::spool::clear_duplicate_objects();
   CHECK(13, "the complex entry is written under a second key", write_ok(context, second, "bdb_alias",
                                                                       SGE_TYPE_CENTRY));
   const auto filtered = read_list<tom::CE>(context, SGE_TYPE_CENTRY, &ok);
   const auto &duplicates = ocs::spool::duplicate_objects();
   CHECK(14, "two records with the same name are read as one complex entry, the second is recorded",
         ok && filtered && filtered->size() == 1 && duplicates.size() == 1 &&
            duplicates.front().first_spool_key == db_key(SGE_TYPE_CENTRY, "bdb_alias") &&
            duplicates.front().second_spool_key == db_key(SGE_TYPE_CENTRY, "bdb_mem"));
   ocs::spool::clear_duplicate_objects();
   CHECK(15, "the second key is deleted", delete_ok(context, SGE_TYPE_CENTRY, "bdb_alias"));
}

/**
 * @brief A free userset with a sub-list: write, record bytes, read back, delete
 * @param context the spooling context
 * @param info the database handle of the rule
 */
void test_userset(const lListElem *context, const bdb_info info) {
   printf("\n--- free element with a sub-list (userset) ---\n");

   const auto us = make_userset();
   const auto before = us->clone();
   const lList *entries = us->get<US_entries>().raw();

   CHECK(16, "a free userset is written", write_ok(context, *us, "bdb_acl", SGE_TYPE_USERSET));
   CHECK(17, "the free userset is free again after the write", tom::is_free(*us));
   CHECK(18, "the free userset is unchanged and keeps its entry list",
         same(*us, *before) && us->get<US_entries>().raw() == entries);
   // a free element is written in the format of a bound one
   CHECK(19, "the record USERSET:bdb_acl holds the userset packed as a bound element",
         record_matches(info, BDB_CONFIG_DB, db_key(SGE_TYPE_USERSET, "bdb_acl"), us->clone()));

   bool ok = false;
   const auto read = read_list<tom::US>(context, SGE_TYPE_USERSET, &ok);
   CHECK(20, "the userset is read back with its entries",
         ok && read && read->size() == 1 && same(read->front(), *us) &&
            read->front().get<US_entries>().size() == 3);

   CHECK(21, "the userset is deleted", delete_ok(context, SGE_TYPE_USERSET, "bdb_acl"));
   const auto after = read_list<tom::US>(context, SGE_TYPE_USERSET, &ok);
   CHECK(22, "no userset is read after the delete", ok && (!after || after->size() == 0));
   CHECK(23, "no key USERSET: is left", read_keys(context, db_key(SGE_TYPE_USERSET, "")).empty());
   // the delete of a missing key reports the error, but spool_delete_object() returns the result of ending the
   // (aborted) transaction, which succeeds: true with an error answer, as before the conversion
   {
      tom::ListPtr<tom::AN> missing_alp;
      const bool missing_deleted = spool_delete_object(&missing_alp, context, SGE_TYPE_USERSET, "bdb_acl");
      CHECK(24, "deleting a key that does not exist answers an error (the call itself returns true)",
            missing_deleted && answer_list_has_error(&missing_alp));
   }
}

/**
 * @brief A cluster queue with queue instances: separate records, joined on reading, deleted together
 * @param context the spooling context
 * @param info the database handle of the rule
 */
void test_cqueue(const lListElem *context, const bdb_info info) {
   printf("\n--- cluster queue with queue instances ---\n");

   const auto cq = make_cqueue();
   const auto before = cq->clone();
   const lList *qinstances = cq->get<CQ_qinstances>().raw();

   CHECK(25, "the cluster queue is written", write_ok(context, *cq, QUEUE, SGE_TYPE_CQUEUE));
   CHECK(26, "the cluster queue is unchanged and keeps its queue instance list",
         same(*cq, *before) && cq->get<CQ_qinstances>().raw() == qinstances);
   auto expected = cq->clone();
   static_cast<void>(expected->exchange<CQ_qinstances>(tom::ListPtr<tom::QU>()));
   CHECK(27, "the record CQUEUE:bdb.q holds the cluster queue without its queue instances",
         record_matches(info, BDB_CONFIG_DB, db_key(SGE_TYPE_CQUEUE, QUEUE), std::move(expected)));
   CHECK(28, "writing the cluster queue writes no queue instance",
         read_keys(context, db_key(SGE_TYPE_QINSTANCE, "")).empty());

   bool all_written = true;
   bool all_bytes = true;
   std::vector<std::string> expected_keys;
   for (const auto qu : cq->get<CQ_qinstances>()) {
      const std::string key = std::string(QUEUE) + "/" + static_cast<const char *>(qu.get<QU_qhostname>());
      all_written = write_ok(context, qu, key.c_str(), SGE_TYPE_QINSTANCE) && all_written;
      expected_keys.push_back(db_key(SGE_TYPE_QINSTANCE, key));
      all_bytes = record_matches(info, BDB_CONFIG_DB, expected_keys.back(), qu.clone()) && all_bytes;
   }
   CHECK(29, "both queue instances are written", all_written);
   CHECK(30, "the records QINSTANCE:bdb.q/<host> hold the packed queue instances", all_bytes);
   CHECK(31, "the queue instance keys are QINSTANCE:bdb.q/bdbhost1 and QINSTANCE:bdb.q/bdbhost2",
         read_keys(context, db_key(SGE_TYPE_QINSTANCE, "")) == expected_keys);

   const int validated = g_validated;
   bool ok = false;
   const auto read = read_list<tom::CQ>(context, SGE_TYPE_CQUEUE, &ok);
   CHECK(32, "the cluster queue is read back", ok && read && read->size() == 1);
   CHECK(33, "the cluster queue read back holds both queue instances, in host order, unchanged",
         ok && read && read->size() == 1 && read->front().get<CQ_qinstances>().size() == 2 &&
            same(read->front(), *cq));
   CHECK(34, "only the cluster queue is handed to the validation", g_validated == validated + 1);

   CHECK(35, "the cluster queue is deleted", delete_ok(context, SGE_TYPE_CQUEUE, QUEUE));
   CHECK(36, "deleting the cluster queue deletes its queue instances",
         read_keys(context, db_key(SGE_TYPE_CQUEUE, "")).empty() &&
            read_keys(context, db_key(SGE_TYPE_QINSTANCE, "")).empty());
}

/**
 * @brief An array job with array tasks and a parallel task: separate records, joined on reading, deleted
 * @param context the spooling context
 * @param info the database handle of the rule
 */
void test_job(const lListElem *context, const bdb_info info) {
   printf("\n--- array job with array tasks and a parallel task ---\n");

   const auto job = make_job();
   const auto before = job->clone();
   const auto tasks = job->get<JB_ja_tasks>();
   const auto task1 = tasks.front();
   const auto task2 = task1.next();
   const auto pe_task = task1.get<JAT_task_list>().front();
   const lList *tasks_raw = tasks.raw();
   const lList *pe_tasks_raw = task1.get<JAT_task_list>().raw();

   // the key <job>.<task> writes the job and that array task
   const std::string key1 = std::to_string(JOB_ID) + ".1";
   CHECK(37, "the job is written with array task 1", write_ok(context, *job, key1.c_str(), SGE_TYPE_JOB));
   CHECK(38, "the job is unchanged and keeps its array task and parallel task lists",
         same(*job, *before) && job->get<JB_ja_tasks>().raw() == tasks_raw &&
            task1.get<JAT_task_list>().raw() == pe_tasks_raw);

   auto expected_job = job->clone();
   static_cast<void>(expected_job->exchange<JB_ja_tasks>(tom::ListPtr<tom::JAT>()));
   CHECK(39, "the record JOB:<job> holds the job without its array tasks",
         record_matches(info, BDB_JOB_DB, db_key(SGE_TYPE_JOB, job_part(JOB_ID)), std::move(expected_job)));
   auto expected_task1 = task1.clone();
   static_cast<void>(expected_task1->exchange<JAT_task_list>(tom::ListPtr<tom::PET>()));
   CHECK(40, "the record JATASK:<job>.<1> holds array task 1 without its parallel tasks",
         record_matches(info, BDB_JOB_DB, db_key(SGE_TYPE_JATASK, task_part(JOB_ID, 1)), std::move(expected_task1)));

   const std::string key2 = std::to_string(JOB_ID) + ".2";
   CHECK(41, "array task 2 is written on its own", write_ok(context, task2, key2.c_str(), SGE_TYPE_JATASK));
   CHECK(42, "the record JATASK:<job>.<2> holds array task 2",
         record_matches(info, BDB_JOB_DB, db_key(SGE_TYPE_JATASK, task_part(JOB_ID, 2)), task2.clone()));

   const std::string pe_key = key1 + " " + PE_TASK;
   const std::string pe_db_key = db_key(SGE_TYPE_PETASK, task_part(JOB_ID, 1) + " " + PE_TASK);
   CHECK(43, "the parallel task is written", write_ok(context, pe_task, pe_key.c_str(), SGE_TYPE_PETASK));
   CHECK(44, "the record PETASK:<job>.<1> <id> holds the parallel task",
         record_matches(info, BDB_JOB_DB, pe_db_key, pe_task.clone()));
   const std::vector<std::string> job_keys{db_key(SGE_TYPE_JOB, job_part(JOB_ID))};
   const std::vector<std::string> task_keys{db_key(SGE_TYPE_JATASK, task_part(JOB_ID, 1)),
                                            db_key(SGE_TYPE_JATASK, task_part(JOB_ID, 2))};
   const std::vector<std::string> pe_keys{pe_db_key};
   CHECK(45, "the job database holds one job, two array tasks and one parallel task, keyed as expected",
         read_keys(context, db_key(SGE_TYPE_JOB, "")) == job_keys &&
            read_keys(context, db_key(SGE_TYPE_JATASK, "")) == task_keys &&
            read_keys(context, db_key(SGE_TYPE_PETASK, "")) == pe_keys);

   bool ok = false;
   const auto read = read_list<tom::JB>(context, SGE_TYPE_JOB, &ok);
   CHECK(46, "the job is read back with both array tasks",
         ok && read && read->size() == 1 && read->front().get<JB_ja_tasks>().size() == 2);
   if (ok && read && read->size() == 1) {
      const auto read_task1 = read->front().get<JB_ja_tasks>().front();
      if (read_task1 && read_task1.get<JAT_task_list>().size() == 0) {
         // known: the parallel tasks are written under "PETASK:<job>.<task> <id>" but looked for under the prefix
         // "PETASK:<job>.<task>." - a mismatch as old as the backend, so they are never read back
         printf("KNOWN [SPOOL-BDB-47] the parallel task is not read back (key written with a blank, read with a "
                "dot)\n");
         ++g_known;
         auto expected = job->clone();
         static_cast<void>(expected->get<JB_ja_tasks>().front().exchange<JAT_task_list>(tom::ListPtr<tom::PET>()));
         CHECK(48, "the job read back equals the job written, apart from the parallel task",
               same(read->front(), *expected));
      } else {
         printf("FAIL  [SPOOL-BDB-47] the parallel task is read back now: the known deviation is gone, update "
                "this test\n");
         ++g_failures;
         CHECK(48, "the job read back equals the job written", same(read->front(), *job));
      }
   }

   CHECK(49, "array task 2 is deleted", delete_ok(context, SGE_TYPE_JATASK, key2.c_str()));
   const auto after_task = read_list<tom::JB>(context, SGE_TYPE_JOB, &ok);
   CHECK(50, "the job is read back with array task 1 only",
         ok && after_task && after_task->size() == 1 && after_task->front().get<JB_ja_tasks>().size() == 1 &&
            after_task->front().get<JB_ja_tasks>().front().get<JAT_task_number>() == 1);

   CHECK(51, "the job is deleted", delete_ok(context, SGE_TYPE_JOB, std::to_string(JOB_ID).c_str()));
   CHECK(52, "deleting the job deletes its array tasks and parallel tasks",
         read_keys(context, db_key(SGE_TYPE_JOB, "")).empty() &&
            read_keys(context, db_key(SGE_TYPE_JATASK, "")).empty() &&
            read_keys(context, db_key(SGE_TYPE_PETASK, "")).empty());
}

/**
 * @brief Install a throwaway bootstrap file and point SGE_ROOT/SGE_CELL at it
 *
 * Hashing a host field consults ocs::Bootstrap, which reads $SGE_ROOT/$SGE_CELL/common/bootstrap and exits when
 * it cannot; the values only have to parse (the same trick test_spool_golden uses).
 *
 * @param root the directory to create it in
 * @return true when it was created
 */
bool setup_bootstrap(const std::string &root) {
   std::error_code error;
   std::filesystem::create_directories(root + "/default/common", error);
   std::ofstream out(root + "/default/common/bootstrap");
   out << "admin_user        none\n"
          "default_domain    none\n"
          "ignore_fqdn       true\n"
          "spooling_method   berkeleydb\n"
          "spooling_lib      none\n"
          "spooling_params   none\n"
          "binary_path       none\n"
          "qmaster_spool_dir none\n"
          "security_mode     none\n";
   if (!out.good()) {
      return false;
   }
   setenv("SGE_ROOT", root.c_str(), 1);
   setenv("SGE_CELL", "default", 1);
   return true;
}

} // namespace

/**
 * @brief Entry point: runs every check in a fresh temporary directory, which is removed at the end
 * @param argc unused
 * @param argv unused
 * @return 0 when every check passed, 1 otherwise
 */
int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_spool_bdb_roundtrip");

   char scratch_template[] = "/tmp/test_spool_bdb_roundtrip.XXXXXX";
   const char *scratch = mkdtemp(scratch_template);
   if (scratch == nullptr) {
      printf("FAIL - cannot create a temporary directory: %s\n", strerror(errno));
      DRETURN(1);
   }
   if (!setup_bootstrap(std::string(scratch) + "/sge_root")) {
      printf("FAIL - cannot create the throwaway bootstrap\n");
      DRETURN(1);
   }
   // read the bootstrap while errors still reach stderr
   ocs::Bootstrap::get_ignore_fqdn();
   component_set_daemonized(true);
   tom::install_model(tom::gcs_model());

   const std::string db_dir = std::string(scratch) + "/bdb";
   std::error_code error;
   std::filesystem::create_directories(db_dir, error);

   // the context as spoolinit sets it up: startup without a check, then the init command creates the databases
   tom::ListPtr<tom::AN> alp;
   auto context = tom::ElementPtr<tom::SPC>::from_typed(spool_berkeleydb_create_context(&alp, db_dir.c_str()));
   CHECK(1, "a Berkeley DB spooling context is created", answers_ok(alp, "create context") && context);
   if (!context) {
      std::filesystem::remove_all(scratch, error);
      DRETURN(1);
   }
   lListElem *ctx = context.get().raw();
   const auto rule = context->get<SPC_rules>().front();
   const auto info = static_cast<bdb_info>(rule.get<SPR_clientdata>());
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
   rule.set<SPR_validate_func>(reinterpret_cast<void *>(count_validate));
   rule.set<SPR_validate_list_func>(reinterpret_cast<void *>(count_validate_list));
#pragma GCC diagnostic pop

   const bool started = spool_startup_context(&alp, ctx, false);
   CHECK(2, "the context starts up in the temporary directory", answers_ok(alp, "startup") && started);
   const bool initialised = spool_maintain_context(&alp, ctx, SPM_init, nullptr);
   CHECK(3, "the databases are created", answers_ok(alp, "init") && initialised);

   if (started && initialised) {
      test_centry(ctx, info);
      test_userset(ctx, info);
      test_cqueue(ctx, info);
      test_job(ctx, info);
   }

   spool_free_context(&alp, std::move(context));
   alp.reset();
   if (std::filesystem::remove_all(scratch, error) == 0) {
      printf("note: the temporary directory %s was not removed\n", scratch);
   }

   printf("\n%s - %d failure(s), %d known deviation(s)\n", g_failures == 0 ? "PASS" : "FAIL", g_failures, g_known);
   DRETURN(g_failures == 0 ? 0 : 1);
}
