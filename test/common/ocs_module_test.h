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

/**
 * @file ocs_module_test.h
 * @brief Shared frame of the module tests: CHECK, scenarios, describe and report.
 *
 * A module test declares its scenarios by hand -- id, title, what it verifies,
 * the steps, the pass criterion, optional catalog test cases and the CHECK ids
 * that belong to it -- and hands them to ocs::module_test::run(), which runs
 * each scenario's body in order.
 *
 * CHECK(id, label, expr) prints the same lines as the per-file macros it
 * replaces ("ok    [Tnn] label" / "FAIL  [Tnn] label"), and run() prints the
 * same footer ("PASS - 0 failure(s)") and returns 0 or 1, so callers that only
 * look at the output or the exit status see no difference.
 *
 * Options, removed from the arguments before the test sees them (args()):
 *
 *   --describe            print the scenarios as a catalog fragment
 *                         (testsuite doc/reports/format.md, section 3), run nothing
 *   --report=<file>       append a record stream (format.md, section 1.5) to <file>
 *   --report=-            write the record lines to stdout, each prefixed with
 *                         "##tsrec ", for a caller that collects the output
 *   --report-run=<id>     run id of the stream (default TS_REPORT_RUN_ID, else
 *                         the start time)
 *   --report-attempt=<n>  attempt order (default TS_REPORT_ATTEMPT, else 0)
 *   --report-label=<l>    attempt label (default TS_REPORT_ATTEMPT_LABEL, else
 *                         "interactive")
 *
 * Drift guard: a CHECK outside any scenario, a CHECK id a scenario does not
 * declare, and a declared id that never ran are each reported as a failure
 * (an error with origin "drift"), so the descriptor cannot quietly diverge
 * from the code. A scenario that calls unsupported() is exempt from the last
 * rule.
 */

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <set>
#include <utility>
#include <string>
#include <vector>

#include <pwd.h>
#include <unistd.h>

namespace ocs::module_test {

   /** @brief A catalog test case a scenario relates to. */
   struct TestCase {
      std::string id;       ///< catalog id, e.g. ENG-03
      std::string relation; ///< covers or prepares
   };

   /** @brief One scenario of a module test, written by hand next to its body. */
   struct Scenario {
      std::string id;                   ///< unique id, e.g. ULONG-01
      std::string title;                ///< one line
      std::string verifies;             ///< what the scenario verifies
      std::vector<std::string> steps;   ///< the steps, in order
      std::string pass;                 ///< the pass criterion
      std::vector<TestCase> test_cases; ///< optional catalog test cases
      std::vector<int> checks;          ///< the CHECK ids that belong to it
      std::function<void()> body;       ///< runs the scenario
   };

   /** @brief The state of one module test run. */
   struct State {
      std::string name;                ///< binary name, the unit's check
      std::string path;                ///< argv[0]
      std::vector<std::string> args;   ///< the arguments the test sees
      FILE *out = nullptr;             ///< record sink, nullptr if not recording
      bool prefix = false;             ///< prefix each record line with "##tsrec "
      int failures = 0;                ///< failed CHECKs plus drift errors
      const Scenario *current = nullptr;
      std::set<int> ran;               ///< CHECK ids run in the current scenario
      bool current_unsupported = false;
      bool current_failed = false;
      bool started = false;            ///< the stream head is written
      std::vector<std::pair<std::string, std::string>> early; ///< (type, fields) from before run()
   };

   /**
    * @brief The state of this process.
    * @return the one State instance
    */
   inline State &state() {
      static State s;
      return s;
   }

   /**
    * @brief The arguments left for the test after the frame's own options.
    * @return the arguments, argv[0] excluded
    */
   inline const std::vector<std::string> &args() {
      return state().args;
   }

   /**
    * @brief Quote a string as a JSON string literal.
    * @param s the string
    * @return the literal, quotes included
    */
   inline std::string json(const std::string &s) {
      std::string r = "\"";
      for (unsigned char c : s) {
         switch (c) {
            case '"': r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n"; break;
            case '\r': r += "\\r"; break;
            case '\t': r += "\\t"; break;
            default:
               if (c < 0x20) {
                  char buf[8];
                  snprintf(buf, sizeof(buf), "\\u%04x", c);
                  r += buf;
               } else {
                  r += static_cast<char>(c);
               }
         }
      }
      return r + "\"";
   }

   /**
    * @brief A list of strings as a JSON array.
    * @param items the strings
    * @return the array literal
    */
   inline std::string json(const std::vector<std::string> &items) {
      std::string r = "[";
      for (size_t i = 0; i < items.size(); ++i) {
         r += (i > 0 ? ", " : "") + json(items[i]);
      }
      return r + "]";
   }

   /**
    * @brief The current time, ISO 8601 with milliseconds and offset.
    * @return e.g. 2026-10-25T14:03:11.204+02:00
    */
   inline std::string now() {
      auto tp = std::chrono::system_clock::now();
      auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
      time_t secs = static_cast<time_t>(ms / 1000);
      struct tm tm_buf;
      localtime_r(&secs, &tm_buf);
      char base[32];
      char zone[8];
      strftime(base, sizeof(base), "%Y-%m-%dT%H:%M:%S", &tm_buf);
      strftime(zone, sizeof(zone), "%z", &tm_buf);
      char buf[64];
      snprintf(buf, sizeof(buf), "%s.%03d%.3s:%.2s", base, static_cast<int>(ms % 1000), zone, zone + 3);
      return buf;
   }

   /**
    * @brief Seconds since the epoch, with fractions.
    * @return the time
    */
   inline double seconds() {
      auto tp = std::chrono::system_clock::now();
      return std::chrono::duration<double>(tp.time_since_epoch()).count();
   }

   /**
    * @brief Write one record line, if recording.
    * @param type the line type
    * @param fields the further fields, already JSON, without braces, starting with ", "
    *               or empty
    * @return nothing
    */
   inline void record(const char *type, const std::string &fields) {
      State &s = state();
      if (!s.started && strcmp(type, "invocation") != 0 && strcmp(type, "selection") != 0 &&
          strcmp(type, "unit_start") != 0) {
         s.early.emplace_back(type, fields); // a CHECK before run(), written once the stream is open
         return;
      }
      if (s.out == nullptr) {
         return;
      }
      std::string line = "{\"type\": " + json(type) + ", \"t\": " + json(now()) + fields + "}";
      fprintf(s.out, "%s%s\n", s.prefix ? "##tsrec " : "", line.c_str());
      fflush(s.out);
   }

   /**
    * @brief Record an error of the current scenario and count it as a failure.
    * @param origin the error origin, assertion or drift
    * @param message the message
    * @return nothing
    */
   inline void error(const char *origin, const std::string &message) {
      State &s = state();
      ++s.failures;
      s.current_failed = true;
      std::string function = s.current != nullptr ? s.current->id : "";
      record("error", ", \"function\": " + json(function) + ", \"level\": \"SEVERE\", \"origin\": " +
                      json(origin) + ", \"message\": " + json(message));
   }

   /**
    * @brief Evaluate one check; what the CHECK macro calls.
    * @param id the check id, unique in the test
    * @param label what the check asserts
    * @param ok the result of the checked expression
    * @return nothing
    */
   inline void check(int id, const char *label, bool ok) {
      State &s = state();
      printf("%s [T%02d] %s\n", ok ? "ok   " : "FAIL ", id, label);
      fflush(stdout);
      std::string function = s.current != nullptr ? s.current->id : "";
      record("assertion", ", \"function\": " + json(function) + ", \"id\": " + std::to_string(id) +
                          ", \"label\": " + json(label) + ", \"result\": " + json(ok ? "ok" : "fail"));
      if (!ok) {
         char buf[32];
         snprintf(buf, sizeof(buf), "[T%02d] ", id);
         error("assertion", buf + std::string(label));
      }
      if (s.current == nullptr) {
         error("drift", "CHECK " + std::to_string(id) + " ran outside any scenario");
         return;
      }
      bool declared = false;
      for (int c : s.current->checks) {
         declared = declared || c == id;
      }
      if (!declared) {
         error("drift", "CHECK " + std::to_string(id) + " is not declared by scenario " + s.current->id);
      }
      s.ran.insert(id);
   }

   /**
    * @brief End the current scenario as unsupported: the environment cannot run it.
    *
    * Prints "SKIP: <reason>". The scenario's body must return right after;
    * its declared checks that did not run are no drift error.
    *
    * @param reason why the scenario cannot run here
    * @return nothing
    */
   inline void unsupported(const std::string &reason) {
      State &s = state();
      printf("SKIP: %s\n", reason.c_str());
      fflush(stdout);
      s.current_unsupported = true;
   }

   /**
    * @brief Print the scenarios as a catalog fragment (format.md, section 3).
    * @param scenarios the scenarios
    * @return nothing
    */
   inline void describe(const std::vector<Scenario> &scenarios) {
      State &s = state();
      printf("{\"format\": \"ts-report/catalog/1\",\n");
      printf(" \"source\": {\"title\": %s, \"generated_at\": %s, \"files\": [{\"path\": %s}]},\n",
             json("module test " + s.name).c_str(), json(now()).c_str(), json(s.name).c_str());
      printf(" \"milestones\": [],\n \"test_cases\": [");
      for (size_t i = 0; i < scenarios.size(); ++i) {
         const Scenario &sc = scenarios[i];
         std::string relates = "[";
         for (size_t j = 0; j < sc.test_cases.size(); ++j) {
            relates += (j > 0 ? ", " : "") + std::string("{\"id\": ") + json(sc.test_cases[j].id) +
                       ", \"relation\": " + json(sc.test_cases[j].relation) + "}";
         }
         relates += "]";
         std::string checks = "[";
         for (size_t j = 0; j < sc.checks.size(); ++j) {
            checks += (j > 0 ? ", " : "") + std::to_string(sc.checks[j]);
         }
         checks += "]";
         printf("%s\n  {\"id\": %s, \"title\": %s, \"section\": %s, \"verifies\": %s, \"steps\": %s, "
                "\"pass\": %s, \"notes\": [], \"owner\": \"product\", \"source\": \"module_test\", "
                "\"criteria\": [], \"test_cases\": %s, \"checks\": %s}",
                i > 0 ? "," : "", json(sc.id).c_str(), json(sc.title).c_str(), json(s.name).c_str(),
                json(sc.verifies).c_str(), json(sc.steps).c_str(), json(sc.pass).c_str(), relates.c_str(),
                checks.c_str());
      }
      printf("],\n \"criteria\": [],\n \"unmapped\": []}\n");
   }

   /**
    * @brief The value of an option "--name=value", or of "--name value".
    * @param argv the arguments
    * @param i index of the current argument; advanced past a separate value
    * @param argc the number of arguments
    * @param name the option, e.g. "--report"
    * @param value receives the value
    * @return true if argv[i] is this option
    */
   inline bool option(char *argv[], int &i, int argc, const char *name, std::string &value) {
      size_t len = strlen(name);
      if (strncmp(argv[i], name, len) != 0) {
         return false;
      }
      if (argv[i][len] == '=') {
         value = argv[i] + len + 1;
         return true;
      }
      if (argv[i][len] == '\0' && i + 1 < argc) {
         value = argv[++i];
         return true;
      }
      return false;
   }

   /**
    * @brief An environment variable, or a default.
    * @param name the variable
    * @param fallback the value if unset or empty
    * @return the value
    */
   inline std::string env(const char *name, const std::string &fallback) {
      const char *v = getenv(name);
      return (v != nullptr && *v != '\0') ? v : fallback;
   }

   /**
    * @brief Write the invocation head: invocation, selection and unit_start.
    * @param run_id the run id
    * @param attempt the attempt order
    * @param label the attempt label
    * @param scenarios the scenarios
    * @return nothing
    */
   inline void record_head(const std::string &run_id, int attempt, const std::string &label,
                           const std::vector<Scenario> &scenarios) {
      State &s = state();
      char host[256] = "";
      gethostname(host, sizeof(host) - 1);
      char *dot = strchr(host, '.');
      if (dot != nullptr) {
         *dot = '\0'; // the short name, as the testsuite writes it
      }
      const struct passwd *pw = getpwuid(geteuid());
      std::string user = pw != nullptr ? pw->pw_name : std::to_string(geteuid());
      long long ms = static_cast<long long>(seconds() * 1000);
      std::string inv = std::string(host) + "-" + std::to_string(getpid()) + "-" + std::to_string(ms);
      std::string args;
      for (const auto &a : s.args) {
         args += (args.empty() ? "" : " ") + a;
      }
      record("invocation", ", \"format\": \"ts-report/record/1\", \"run_id\": " + json(run_id) +
                           ", \"invocation_id\": " + json(inv) + ", \"attempt\": " + std::to_string(attempt) +
                           ", \"attempt_label\": " + json(label) +
                           ", \"producer\": \"module_test\", \"provenance\": {\"ts_host\": " + json(host) +
                           ", \"ts_user\": " + json(user) + ", \"binary\": " + json(s.path) +
                           ", \"args\": " + json(args) + "}");
      std::string unit = "{\"check\": " + json(s.name) + ", \"level\": 0}";
      record("selection", ", \"source\": \"command_line\", \"units\": [" + unit + "]");
      std::vector<std::string> ids;
      std::string cases;
      for (const Scenario &sc : scenarios) {
         ids.push_back(sc.id);
         if (sc.test_cases.empty()) {
            continue;
         }
         std::string list;
         for (const TestCase &tc : sc.test_cases) {
            list += (list.empty() ? "" : ", ") + std::string("{\"id\": ") + json(tc.id) +
                    ", \"relation\": " + json(tc.relation) + "}";
         }
         cases += (cases.empty() ? "" : ", ") + json(sc.id) + ": [" + list + "]";
      }
      record("unit_start", ", \"check\": " + json(s.name) + ", \"check_path\": " + json(s.path) +
                           ", \"level\": 0, \"description\": " + json("module test " + s.name) +
                           ", \"categories\": [\"MODULE\"], \"functions\": " + json(ids) +
                           (cases.empty() ? "" : ", \"test_cases\": {" + cases + "}"));
   }

   /**
    * @brief Run a module test: parse the options, run every scenario, report.
    * @param argc the argument count of main
    * @param argv the argument vector of main
    * @param scenarios the scenarios, run in this order
    * @return the exit status for main: 0 if nothing failed, 1 otherwise, 2 on a usage error
    */
   inline int run(int argc, char *argv[], const std::vector<Scenario> &scenarios) {
      State &s = state();
      s.path = argc > 0 ? argv[0] : "module_test";
      const char *slash = strrchr(s.path.c_str(), '/');
      s.name = slash != nullptr ? slash + 1 : s.path;

      bool want_describe = false;
      std::string report;
      std::string run_id = env("TS_REPORT_RUN_ID", "");
      std::string attempt = env("TS_REPORT_ATTEMPT", "0");
      std::string label = env("TS_REPORT_ATTEMPT_LABEL", "interactive");
      for (int i = 1; i < argc; ++i) {
         if (strcmp(argv[i], "--describe") == 0) {
            want_describe = true;
         } else if (option(argv, i, argc, "--report-run", run_id) ||
                    option(argv, i, argc, "--report-attempt", attempt) ||
                    option(argv, i, argc, "--report-label", label) ||
                    option(argv, i, argc, "--report", report)) {
            continue;
         } else {
            s.args.push_back(argv[i]);
         }
      }

      if (want_describe) {
         describe(scenarios);
         return 0;
      }
      if (report == "-") {
         s.out = stdout;
         s.prefix = true;
      } else if (!report.empty()) {
         s.out = fopen(report.c_str(), "a");
         if (s.out == nullptr) {
            fprintf(stderr, "%s: cannot open report file %s: %s\n", s.name.c_str(), report.c_str(),
                    strerror(errno));
            return 2;
         }
      }
      if (run_id.empty()) {
         time_t t = time(nullptr);
         struct tm tm_buf;
         localtime_r(&t, &tm_buf);
         char buf[32];
         strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", &tm_buf);
         run_id = buf;
      }
      record_head(run_id, atoi(attempt.c_str()), label, scenarios);
      s.started = true;
      for (const auto &[type, fields] : s.early) {
         record(type.c_str(), fields);
      }
      s.early.clear();

      double unit_start = seconds();
      bool any_failed = false;
      bool all_unsupported = !scenarios.empty();
      for (size_t i = 0; i < scenarios.size(); ++i) {
         const Scenario &sc = scenarios[i];
         s.current = &sc;
         s.ran.clear();
         s.current_unsupported = false;
         s.current_failed = false;
         record("function_start", ", \"function\": " + json(sc.id) + ", \"index\": " + std::to_string(i + 1));
         double start = seconds();
         sc.body();
         if (!s.current_unsupported) {
            for (int c : sc.checks) {
               if (s.ran.count(c) == 0) {
                  error("drift", "CHECK " + std::to_string(c) + " is declared by scenario " + sc.id +
                                 " but never ran");
               }
            }
         }
         const char *verdict = s.current_failed ? "failed" : s.current_unsupported ? "unsupported" : "passed";
         any_failed = any_failed || s.current_failed;
         all_unsupported = all_unsupported && !s.current_failed && s.current_unsupported;
         char duration[32];
         snprintf(duration, sizeof(duration), "%.3f", seconds() - start);
         record("function_end", ", \"function\": " + json(sc.id) + ", \"verdict\": " + json(verdict) +
                                ", \"duration\": " + duration);
      }
      s.current = nullptr;

      int status = s.failures == 0 ? 0 : 1;
      printf("\n%s - %d failure(s)\n", status == 0 ? "PASS" : "FAIL", s.failures);
      fflush(stdout);
      // a failure outside the scenarios, e.g. a CHECK before run(), fails the unit too
      const char *unit_verdict = any_failed || status != 0 ? "failed" : all_unsupported ? "unsupported" : "passed";
      char duration[32];
      snprintf(duration, sizeof(duration), "%.3f", seconds() - unit_start);
      record("unit_end", ", \"check\": " + json(s.name) + ", \"level\": 0, \"verdict\": " + json(unit_verdict) +
                         ", \"duration\": " + duration);
      record("invocation_end", ", \"exit\": " + std::to_string(status));
      if (s.out != nullptr && s.out != stdout) {
         fclose(s.out);
      }
      s.out = nullptr;
      return status;
   }

} // namespace ocs::module_test

/**
 * @brief Check one expression and print the result line; records into the current scenario.
 * @param id the check id, unique in the test and declared by the scenario
 * @param label what the check asserts
 * @param expr the expression, evaluated once
 */
#define CHECK(id, label, expr) ::ocs::module_test::check((id), (label), static_cast<bool>(expr))
