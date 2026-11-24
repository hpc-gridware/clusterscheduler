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
 * @brief Unit tests for the JSV protocol handling in `libs/sgeobj`
 *
 * CS-3138: the message of a `RESULT STATE REJECT` / `REJECT_WAIT` line and the lines a JSV script writes
 * to stderr are text from the script. They were passed as the format of a printf-style function, so a
 * message containing a conversion specification was undefined behaviour - inside sge_qmaster for a
 * server JSV. The tests feed jsv_do_communication() such lines through files that stand in for the
 * pipes of a JSV script and check that the answer carries the text unchanged. Only `%%` and `%d` are
 * used: they show the text being interpreted as a format without crashing the unfixed code.
 *
 * CS-3208: every line of the script was read with `fscanf("%[^\n]\n")` into a fixed stack buffer of 10000
 * characters, so a longer line overflowed the buffer. The tests send lines of 20000 characters on stdout and
 * stderr and check that they arrive complete, and that empty lines and indentation are still skipped.
 *
 * CS-3139: the handlers of the protocol commands called strcmp() on tokens that are missing from an incomplete
 * line (`SEND`, `LOG`, `ENV ADD`, ...), which crashed qmaster (server JSV) or the client (client JSV). Each
 * such line is fed in a child process, so a crash is reported as a failure of that line instead of ending
 * the test.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <sys/wait.h>
#include <unistd.h>

#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_JSV.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_jsv.h"
#include "sgeobj/sge_jsv_script.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

// ---------------------------------------------------------------------------
// Test infrastructure
// ---------------------------------------------------------------------------

static int s_fail = 0;

/** @def CHECK
 * @brief Assert one condition and record the result
 *
 * Prints `ok`/`FAIL` with the test's id and label and counts the failure, so
 * a run reports every problem rather than stopping at the first.
 *
 * @param id the test number, printed as `[Tnn]`
 * @param label what the check is about, printed on failure
 * @param expr the condition that must hold
 */
#define CHECK(id, label, expr) \
   do { \
      if (!(expr)) { \
         printf("FAIL  [T%02d] %s\n", (id), (label)); \
         ++s_fail; \
      } else { \
         printf("ok    [T%02d] %s\n", (id), (label)); \
      } \
   } while (0)

/**
 * @brief Create a temporary file holding some text, positioned at its start
 * @param text the content of the file
 * @return the open file, nullptr on error
 */
static FILE *
make_stream(const char *text) {
   FILE *fp = tmpfile();
   if (fp != nullptr) {
      fputs(text, fp);
      rewind(fp);
   }
   return fp;
}

/**
 * @brief Run one JSV conversation and return the text of the first error answer
 *
 * The JSV's stdin goes to /dev/null, its stdout and stderr are temporary files with the given lines,
 * so jsv_do_communication() reads them as if a script had written them.
 *
 * @param out the lines the "script" writes to stdout
 * @param err the lines the "script" writes to stderr
 * @param accepted set to the JSV_accept state after the conversation
 * @param job the job the PARAM lines modify (JSV_new_job), nullptr for none
 * @param errors set to the number of error answers, if not nullptr
 * @param context the context of the JSV, JSV_CONTEXT_CLIENT or a qmaster thread name for a server JSV
 * @return the text of the first error answer, "<none>" if there was none
 */
static std::string
communicate(const char *out, const char *err, bool *accepted, ocs::tom::ElementData *job = nullptr,
            int *errors = nullptr, const char *context = JSV_CONTEXT_CLIENT) {
   auto jsv = ocs::tom::ElementPtr<ocs::tom::JSV>::create();
   jsv->set<JSV_name>("test");
   jsv->set<JSV_context>(context);
   jsv->set<JSV_command>("/no/such/jsv");
   jsv->set<JSV_pid>("-1");

   FILE *fp_in = fopen("/dev/null", "w");
   FILE *fp_out = make_stream(out);
   FILE *fp_err = make_stream(err);
   if (fp_in == nullptr || fp_out == nullptr || fp_err == nullptr) {
      printf("cannot open the streams of the test JSV\n");
      exit(1);
   }
   jsv->set<JSV_in>(fp_in);
   jsv->set<JSV_out>(fp_out);
   jsv->set<JSV_err>(fp_err);
   jsv->set<JSV_new_job>(job);

   ocs::tom::ListPtr<ocs::tom::AN> alp;
   jsv_do_communication(*jsv, &alp);
   *accepted = jsv->get<JSV_accept>();

   std::string text = "<none>";
   int error_count = 0;
   if (alp) {
      for (const auto answer : *alp) {
         if (answer.get<AN_quality>() == ANSWER_QUALITY_ERROR) {
            if (error_count++ == 0) {
               text = answer.get<AN_text>() != nullptr ? answer.get<AN_text>() : "";
            }
         }
      }
   }
   if (errors != nullptr) {
      *errors = error_count;
   }

   jsv->set<JSV_in>(nullptr);
   jsv->set<JSV_out>(nullptr);
   jsv->set<JSV_err>(nullptr);
   jsv->set<JSV_new_job>(nullptr);
   fclose(fp_in);
   fclose(fp_out);
   fclose(fp_err);
   return text;
}

// ---------------------------------------------------------------------------
// jsv_do_communication()
// ---------------------------------------------------------------------------

/**
 * @brief The message of a rejecting RESULT line reaches the answer unchanged (CS-3138)
 */
static void
test_reject_message() {
   printf("\n--- RESULT STATE REJECT / REJECT_WAIT ---\n");
   bool accepted = true;

   std::string text = communicate("RESULT STATE REJECT 100%% done %d\n", "", &accepted);
   printf("      answer: \"%s\"\n", text.c_str());
   CHECK(1, "REJECT: the job is not accepted", !accepted);
   CHECK(2, "REJECT: the message is not used as a format", text == "100%% done %d");

   accepted = true;
   text = communicate("RESULT STATE REJECT_WAIT try again at 50%% %d\n", "", &accepted);
   printf("      answer: \"%s\"\n", text.c_str());
   CHECK(3, "REJECT_WAIT: the job is not accepted", !accepted);
   CHECK(4, "REJECT_WAIT: the message is not used as a format", text == "try again at 50%% %d");

   accepted = true;
   text = communicate("RESULT STATE REJECT\n", "", &accepted);
   CHECK(5, "REJECT without a message: the default message", text == MSG_JSV_REJECTED);

   accepted = false;
   text = communicate("RESULT STATE ACCEPT\n", "", &accepted);
   CHECK(6, "ACCEPT: the job is accepted without an error", accepted && text == "<none>");
}

/**
 * @brief A line the script writes to stderr reaches the answer unchanged (CS-3138)
 */
static void
test_stderr_line() {
   printf("\n--- stderr of the JSV ---\n");
   bool accepted = true;

   const std::string text = communicate("RESULT STATE ACCEPT\n", "error at 50%% %d\n", &accepted);
   printf("      answer: \"%s\"\n", text.c_str());
   CHECK(7, "a stderr line rejects the job", !accepted);
   CHECK(8, "the stderr line is not used as a format", text.find("error at 50%% %d") != std::string::npos);
}

/**
 * @brief A text of 'length' characters that shows where it was cut
 * @param length the number of characters
 * @return "0123456789" repeated, ending with "END"
 */
static std::string
long_text(const size_t length) {
   std::string text;
   while (text.size() < length - 3) {
      text.push_back(static_cast<char>('0' + text.size() % 10));
   }
   return text + "END";
}

/**
 * @brief Lines longer than the former 10000 character buffer arrive complete (CS-3208)
 */
static void
test_long_lines() {
   printf("\n--- lines longer than 10000 characters ---\n");
   const std::string message = long_text(20000);
   bool accepted = true;

   std::string text = communicate(("RESULT STATE REJECT " + message + "\n").c_str(), "", &accepted);
   printf("      answer: %zu characters\n", text.size());
   CHECK(9, "long REJECT: the job is not accepted", !accepted);
   CHECK(10, "long REJECT: the message arrives complete", text == message);

   auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   accepted = false;
   text = communicate(("PARAM A " + message + "\nRESULT STATE ACCEPT\n").c_str(), "", &accepted, job.get().raw());
   const char *account = job->get<JB_account>();
   CHECK(11, "long PARAM: the job is accepted without an error", accepted && text == "<none>");
   CHECK(12, "long PARAM: the value arrives complete", account != nullptr && message == account);

   accepted = true;
   int errors = 0;
   text = communicate("RESULT STATE ACCEPT\n", (message + "\n").c_str(), &accepted, nullptr, &errors);
   printf("      answer: %zu characters, %d error answer(s)\n", text.size(), errors);
   CHECK(13, "long stderr line: the job is rejected", !accepted);
   // the answer is the error log message, which the log buffer cuts to its size
   CHECK(14, "long stderr line: the line is read as one line",
         errors == 1 && text.find(message.substr(0, 4000)) != std::string::npos);
}

/**
 * @brief Empty lines and leading whitespace between the lines are skipped as before (CS-3208)
 */
static void
test_line_separation() {
   printf("\n--- empty lines and whitespace ---\n");
   auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   bool accepted = false;

   std::string text = communicate("\n\n  PARAM A acct\n\n \t\n   PARAM N name\n\nRESULT STATE ACCEPT\n\n", "",
                                  &accepted, job.get().raw());
   const char *account = job->get<JB_account>();
   const char *name = job->get<JB_job_name>();
   CHECK(15, "empty lines: the job is accepted without an error", accepted && text == "<none>");
   CHECK(16, "empty lines: both PARAM lines are applied",
         account != nullptr && strcmp(account, "acct") == 0 && name != nullptr && strcmp(name, "name") == 0);

   accepted = true;
   text = communicate("RESULT STATE ACCEPT\n", "\n\n  first error\n", &accepted);
   CHECK(17, "empty lines on stderr: the error line is read",
         !accepted && text.find("first error") != std::string::npos);
}

/**
 * @brief Feed one incomplete line in a child process and check the answer (CS-3139)
 *
 * The child runs the conversation and exits with 0 if the answer is as expected. A crash of the child,
 * the defect of the unfixed code, is reported with its signal.
 *
 * @param id the test number
 * @param out the lines the "script" writes to stdout
 * @param context the context of the JSV, JSV_CONTEXT_CLIENT or a qmaster thread name for a server JSV
 * @param expect_accepted whether the job has to be accepted
 * @param expect_text the expected text of the first error answer, "<none>" for none
 */
static void
check_incomplete_line(const int id, const char *out, const char *context, const bool expect_accepted,
                      const std::string &expect_text) {
   std::string label = "\"";
   for (const char *p = out; *p != '\0'; ++p) {
      label += (*p == '\n') ? std::string("\\n") : std::string(1, *p);
   }
   label += std::string("\" (") + context + ")";

   fflush(stdout);
   const pid_t pid = fork();
   if (pid == 0) {
      bool accepted = !expect_accepted;
      auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
      const std::string text = communicate(out, "", &accepted, job.get().raw(), nullptr, context);
      printf("      %s: answer \"%s\", %s\n", label.c_str(), text.c_str(), accepted ? "accepted" : "rejected");
      fflush(stdout);
      _exit(accepted == expect_accepted && text == expect_text ? 0 : 1);
   }

   int status = 0;
   if (pid < 0 || waitpid(pid, &status, 0) != pid) {
      printf("cannot run the child process of the test\n");
      exit(1);
   }
   if (WIFSIGNALED(status)) {
      printf("      %s: crashed with signal %d\n", label.c_str(), WTERMSIG(status));
   }
   CHECK(id, label.c_str(), WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

/**
 * @brief Incomplete protocol lines are answered as an error and do not crash (CS-3139)
 */
static void
test_incomplete_lines() {
   printf("\n--- incomplete protocol lines ---\n");
   const auto client = JSV_CONTEXT_CLIENT;
   // in qmaster the context is the name of the thread
   const auto server = "worker";
   const std::string send_error = std::string("got \"\" as object for SEND command");
   const std::string env_error = "\"ENV <null> <null> <null>\" is invalid\n";
   const std::string env_add_error = "\"ENV ADD <null> <null>\" is invalid\n";

   // SEND without the subcommand is rejected like an unknown subcommand
   check_incomplete_line(18, "SEND\n", client, false, send_error);
   check_incomplete_line(19, "SEND\n", server, false, send_error);
   // LOG without the level is logged like an unknown level, the conversation goes on
   check_incomplete_line(20, "LOG\nRESULT STATE ACCEPT\n", server, true, "<none>");
   check_incomplete_line(21, "LOG\nRESULT STATE ACCEPT\n", client, true, "<none>");
   // ENV without the modification or the variable is invalid
   check_incomplete_line(22, "ENV\n", client, false, env_error);
   check_incomplete_line(23, "ENV ADD\n", client, false, env_add_error);
   check_incomplete_line(24, "ENV MOD\n", server, false, "\"ENV MOD <null> <null>\" is invalid\n");
   check_incomplete_line(25, "ENV DEL\n", server, false, "\"ENV DEL <null> <null>\" is invalid\n");
   // a test result without the expected value turns off the check instead of crashing the next line
   check_incomplete_line(26, "PARAM __JSV_TEST_RESULT\nPARAM A acct\nRESULT STATE ACCEPT\n", client, true,
                         "<none>");
   check_incomplete_line(27, "ENV ADD __JSV_TEST_RESULT\nENV ADD X y\nRESULT STATE ACCEPT\n", client, true,
                         "<none>");
   // PARAM without the name is ignored like an unknown parameter
   check_incomplete_line(28, "PARAM\nRESULT STATE ACCEPT\n", client, true, "<none>");
   // RESULT without the subcommand or the state
   check_incomplete_line(29, "RESULT\n", client, false, "JSV got unknown command \"\"");
   check_incomplete_line(30, "RESULT STATE\n", server, false, "JSV sent unknown result \"\"");
   check_incomplete_line(31, "RESULT STATE BOGUS text\n", client, false, "JSV sent unknown result \"BOGUS\"");
   // a line of blanks only never reaches the dispatcher, ocs_read_line() skips it
   check_incomplete_line(32, "   \n \t \nRESULT STATE ACCEPT\n", server, true, "<none>");
}

/**
 * @brief Entry point
 * @return 0 when every check passed, 1 otherwise
 */
int main(int /*argc*/, char * /*argv*/[]) {
   ocs::tom::install_model(ocs::tom::gcs_model());
   // the client context takes the timeout from the environment, not from the qmaster configuration
   setenv("SGE_JSV_TIMEOUT", "5", 1);

   test_reject_message();
   test_stderr_line();
   test_long_lines();
   test_line_separation();
   test_incomplete_lines();

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
