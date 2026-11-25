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
 *
 * CS-3137: the start time (`a`) and the deadline (`dl`) were sent to the JSV with `tm_mon`, which counts the
 * months from 0, so the JSV saw the previous month. The tests check the text sent and that the values passed
 * back unchanged keep the job's timestamps.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

#include <sys/wait.h>
#include <unistd.h>

#include "cull/cull.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_jsv.h"
#include "sgeobj/sge_jsv_script.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "uti/sge_time.h"

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
 * @param old_job the job the STARTED line sends the parameters of (JSV_old_job), nullptr for none
 * @param sent receives what was sent to the JSV, if not nullptr
 * @return the text of the first error answer, "<none>" if there was none
 */
static std::string
communicate(const char *out, const char *err, bool *accepted, lListElem *job = nullptr, int *errors = nullptr,
            const char *context = JSV_CONTEXT_CLIENT, lListElem *old_job = nullptr, std::string *sent = nullptr) {
   lListElem *jsv = lCreateElem(JSV_Type);
   lSetString(jsv, JSV_name, "test");
   lSetString(jsv, JSV_context, context);
   lSetString(jsv, JSV_command, "/no/such/jsv");
   lSetString(jsv, JSV_pid, "-1");

   FILE *fp_in = sent != nullptr ? tmpfile() : fopen("/dev/null", "w");
   FILE *fp_out = make_stream(out);
   FILE *fp_err = make_stream(err);
   if (fp_in == nullptr || fp_out == nullptr || fp_err == nullptr) {
      printf("cannot open the streams of the test JSV\n");
      exit(1);
   }
   lSetRef(jsv, JSV_in, fp_in);
   lSetRef(jsv, JSV_out, fp_out);
   lSetRef(jsv, JSV_err, fp_err);
   lSetRef(jsv, JSV_new_job, job);
   lSetRef(jsv, JSV_old_job, old_job);

   lList *alp = nullptr;
   jsv_do_communication(jsv, &alp);
   *accepted = lGetBool(jsv, JSV_accept);

   std::string text = "<none>";
   int error_count = 0;
   const lListElem *answer;
   for_each_ep(answer, alp) {
      if (lGetUlong(answer, AN_quality) == ANSWER_QUALITY_ERROR) {
         if (error_count++ == 0) {
            text = lGetString(answer, AN_text) != nullptr ? lGetString(answer, AN_text) : "";
         }
      }
   }
   if (errors != nullptr) {
      *errors = error_count;
   }
   lFreeList(&alp);

   lSetRef(jsv, JSV_in, nullptr);
   lSetRef(jsv, JSV_out, nullptr);
   lSetRef(jsv, JSV_err, nullptr);
   lSetRef(jsv, JSV_new_job, nullptr);
   lSetRef(jsv, JSV_old_job, nullptr);
   lFreeElem(&jsv);
   if (sent != nullptr) {
      sent->clear();
      fflush(fp_in);
      rewind(fp_in);
      char buffer[1024];
      while (fgets(buffer, sizeof(buffer), fp_in) != nullptr) {
         *sent += buffer;
      }
   }
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

   lListElem *job = lCreateElem(JB_Type);
   accepted = false;
   text = communicate(("PARAM A " + message + "\nRESULT STATE ACCEPT\n").c_str(), "", &accepted, job);
   const char *account = lGetString(job, JB_account);
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
   lFreeElem(&job);
}

/**
 * @brief Empty lines and leading whitespace between the lines are skipped as before (CS-3208)
 */
static void
test_line_separation() {
   printf("\n--- empty lines and whitespace ---\n");
   lListElem *job = lCreateElem(JB_Type);
   bool accepted = false;

   std::string text = communicate("\n\n  PARAM A acct\n\n \t\n   PARAM N name\n\nRESULT STATE ACCEPT\n\n", "",
                                  &accepted, job);
   const char *account = lGetString(job, JB_account);
   const char *name = lGetString(job, JB_job_name);
   CHECK(15, "empty lines: the job is accepted without an error", accepted && text == "<none>");
   CHECK(16, "empty lines: both PARAM lines are applied",
         account != nullptr && strcmp(account, "acct") == 0 && name != nullptr && strcmp(name, "name") == 0);

   accepted = true;
   text = communicate("RESULT STATE ACCEPT\n", "\n\n  first error\n", &accepted);
   CHECK(17, "empty lines on stderr: the error line is read",
         !accepted && text.find("first error") != std::string::npos);
   lFreeElem(&job);
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
      lListElem *job = lCreateElem(JB_Type);
      const std::string text = communicate(out, "", &accepted, job, nullptr, context);
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
   const std::string send_error = "got \"\" as object for SEND command";
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
 * @brief The seconds since the epoch of a local date and time
 * @param year the year
 * @param month the month, 1 for January
 * @param day the day of the month
 * @param hour the hour
 * @param minute the minute
 * @param second the second
 * @return the time
 */
static time_t
local_time(const int year, const int month, const int day, const int hour, const int minute, const int second) {
   struct tm tm{};
   tm.tm_year = year - 1900;
   tm.tm_mon = month - 1;
   tm.tm_mday = day;
   tm.tm_hour = hour;
   tm.tm_min = minute;
   tm.tm_sec = second;
   tm.tm_isdst = -1;
   return mktime(&tm);
}

/**
 * @brief The start time and the deadline reach the JSV with the right month and come back unchanged (CS-3137)
 */
static void
test_date_params() {
   printf("\n--- PARAM a / dl sent to the JSV ---\n");
   lListElem *old_job = lCreateElem(JB_Type);
   lSetString(old_job, JB_owner, "user");
   lSetString(old_job, JB_group, "group");
   const time_t start = local_time(2026, 12, 1, 12, 0, 0);
   const time_t deadline = local_time(2027, 1, 15, 8, 30, 5);
   // both are stored as microseconds since the epoch
   lSetUlong64(old_job, JB_execution_time, sge_time_t_to_gmt64(start));
   lSetUlong64(old_job, JB_deadline, sge_time_t_to_gmt64(deadline));

   bool accepted = false;
   std::string sent;
   std::string text = communicate("STARTED\nRESULT STATE ACCEPT\n", "", &accepted, nullptr, nullptr,
                                  JSV_CONTEXT_CLIENT, old_job, &sent);
   CHECK(43, "STARTED: the job is accepted without an error", accepted && text == "<none>");
   const bool a_ok = sent.find("PARAM a 202612011200.00\n") != std::string::npos;
   const bool dl_ok = sent.find("PARAM dl 202701150830.05\n") != std::string::npos;
   if (!a_ok || !dl_ok) {
      printf("      sent:\n%s", sent.c_str());
   }
   CHECK(44, "the start time is sent with its month, December as 12", a_ok);
   CHECK(45, "the deadline is sent with its month, January as 01", dl_ok);

   // a JSV that passes both values back unchanged leaves the job unchanged
   lListElem *job = lCreateElem(JB_Type);
   text = communicate("PARAM a 202612011200.00\nPARAM dl 202701150830.05\nRESULT STATE ACCEPT\n", "", &accepted,
                      job);
   CHECK(46, "the values passed back are accepted", accepted && text == "<none>");
   CHECK(47, "the start time passed back is the one sent",
         lGetUlong64(job, JB_execution_time) == sge_time_t_to_gmt64(start));
   CHECK(48, "the deadline passed back is the one sent",
         lGetUlong64(job, JB_deadline) == sge_time_t_to_gmt64(deadline));
   lFreeElem(&job);
   lFreeElem(&old_job);
}

/**
 * @brief Entry point
 * @return 0 when every check passed, 1 otherwise
 */
int main(int /*argc*/, char * /*argv*/[]) {
   lInit(nmv);
   // the client context takes the timeout from the environment, not from the qmaster configuration
   setenv("SGE_JSV_TIMEOUT", "5", 1);

   test_reject_message();
   test_stderr_line();
   test_long_lines();
   test_line_separation();
   test_incomplete_lines();
   test_date_params();

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
