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
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Manual test: a `qping -dump` style debug client
 *
 * Attaches to a service as #CL_COM_DEBUG_CLIENT_NAME and prints the message
 * stream it is then fed, one column per field of
 * #CL_DEBUG_MESSAGE_FORMAT_STRING.
 *
 * Usage: `test_debug_client <debug_level> <port> <host> <comp> <comp_id>`
 *
 * @note Not registered with ctest; run it by hand.
 */


#include <cstdio>
#include <cstring>
#include <sys/time.h>
#include <cstdlib>
#include <csignal>
#include <unistd.h>

#include "comm/lists/cl_lists.h"
#include "comm/cl_commlib.h"
#include <cinttypes>

/** @brief Note the signal so the client loop can leave
 * @param sig the signal that arrived
 */
void sighandler_client(int sig);

static int do_shutdown = 0;
static cl_com_handle_t *handle = nullptr;

#define ARGUMENT_COUNT 13   ///< Number of columns in the debug stream
char *cl_values[ARGUMENT_COUNT];   ///< Parsed fields of the debug line being printed
int cl_show[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};   ///< Which columns to print
int cl_alignment[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0};   ///< Whether each column is left or right aligned
size_t cl_column_width[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 40, 10};   ///< Width of each column
/** @brief Heading of each column */
const char *cl_description[] = {
        "time of debug output creation",
        "endpoint service name where debug client is connected",
        "message direction",
        "name of participating communication endpoint",
        "message data format",
        "message acknowledge type",
        "message tag information",
        "message id",
        "message response id",
        "message length",
        "time when message was sent/received",
        "commlib xml protocol output",
        "additional information"
};

/** @brief Short name of each column */
const char *cl_names[] = {
        "time",
        "local",
        "d.",
        "remote",
        "format",
        "ack type",
        "msg tag",
        "msg id",
        "msg rid",
        "msg len",
        "msg time",
        "msg xml protocol dump",
        "info"
};


void sighandler_client(
        int sig
) {
/*   thread_signal_receiver = pthread_self(); */
   if (sig == SIGPIPE) {
      return;
   }

   if (sig == SIGHUP) {
      return;
   }
   printf("do_shutdown\n");
   /* shutdown all sockets */
   do_shutdown = 1;
}

/** @brief Print a value padded to a column width
 * @param name the text
 * @param length the column width
 * @param c what to pad with
 * @param before pad on the left rather than the right
 */
void printf_fill_up(const char *name, int length, char c, int before) {
   int n = strlen(name);
   int i;

   if (before == 0) {
      printf("%c%s%c", c, name, c);
   }
   for (i = 0; i < (length - n); i++) {
      printf("%c", c);
   }
   if (before != 0) {
      printf("%c%s%c", c, name, c);
   }

}

/** @brief Render a timestamp the way the debug stream carries it
 * @param buffer the timestamp as received
 * @param dest receives the formatted time
 */
void convert_time(char *buffer, char *dest) {
   time_t i;
   char *help;
   char *help2;
   struct tm tm_buffer{};
   struct tm *tm;
   help = strtok(buffer, ".");
   help2 = strtok(nullptr, ".");
   i = atoi(help);
   tm = (struct tm *) localtime_r(&i, &tm_buffer);

#if 0
   sprintf(dest, "%04d%02d%02d%02d%02d.%02d",
           tm->tm_year+1900, tm->tm_mon + 1, tm->tm_mday,
           tm->tm_hour, tm->tm_min, tm->tm_sec);
#endif
   sprintf(dest, "%02d:%02d:%02d.%s", tm->tm_hour, tm->tm_min, tm->tm_sec, help2);
}

/** @brief Print one line of the debug stream as columns
 * @param buffer the raw line
 */
void print_line(char *buffer) {
   int i = 0;
   size_t max_name_length = 0;
   static int show_header = 1;
   char time[512];
   char msg_time[512];

   for (i = 0; i < ARGUMENT_COUNT; i++) {
      if (max_name_length < strlen(cl_names[i])) {
         max_name_length = strlen(cl_names[i]);
      }
   }


   i = 0;
   cl_values[i++] = strtok(buffer, "\t");
   while ((cl_values[i++] = strtok(nullptr, "\t\n")));


   convert_time(cl_values[0], time);
   cl_values[0] = msg_time;

   convert_time(cl_values[10], msg_time);
   cl_values[10] = msg_time;


   for (i = 0; i < ARGUMENT_COUNT; i++) {
      if (cl_column_width[i] < strlen(cl_values[i])) {
         cl_column_width[i] = strlen(cl_values[i]);
      }
      if (cl_column_width[i] < strlen(cl_names[i])) {
         cl_column_width[i] = strlen(cl_names[i]);
      }
   }

   if (show_header == 1) {
      for (i = 0; i < ARGUMENT_COUNT; i++) {
         if (cl_show[i]) {
            printf_fill_up(cl_names[i], cl_column_width[i], ' ', cl_alignment[i]);
            printf("|");
         }
      }
      printf("\n");
      for (i = 0; i < ARGUMENT_COUNT; i++) {
         if (cl_show[i]) {
            printf_fill_up("", cl_column_width[i], '-', cl_alignment[i]);
            printf("|");
         }
      }
      printf("\n");

      show_header = 0;
   }
   for (i = 0; i < ARGUMENT_COUNT; i++) {
      if (cl_show[i]) {
         printf_fill_up(cl_values[i], cl_column_width[i], ' ', cl_alignment[i]);
         printf("|");
      }
   }
   printf("\n");

}

/** @brief Run the test
 * @param argc argument count
 * @param argv arguments
 * @return 0 on success, 1 on a usage error or failure
 */
extern int main(int argc, char **argv) {
   struct sigaction sa;
   int port;
   char line_buffer[8192];
   int line_index = 0;


   if (argc != 6) {
      printf("syntax: debug_level port host comp comp_id\n");
      exit(1);
   }


   /* setup signalhandling */
   memset(&sa, 0, sizeof(sa));
   sa.sa_handler = sighandler_client;  /* one handler for all signals */
   sigemptyset(&sa.sa_mask);
   sigaction(SIGINT, &sa, nullptr);
   sigaction(SIGTERM, &sa, nullptr);
   sigaction(SIGHUP, &sa, nullptr);
   sigaction(SIGPIPE, &sa, nullptr);


   printf("startup commlib ...\n");
   cl_com_setup_commlib(CL_NO_THREAD, (cl_log_t) atoi(argv[1]), nullptr);

   printf("setting up handle for connect port %d\n", atoi(argv[2]));
   handle = cl_com_create_handle(nullptr, CL_CT_TCP, CL_CM_CT_STREAM, false, atoi(argv[2]), /* CL_TCP_DEFAULT*/
                                 CL_TCP_RESERVED_PORT, "debug_client", 0, 1, 0);
   if (handle == nullptr) {
      printf("could not get handle\n");
      exit(1);
   }

   printf("local hostname is \"%s\"\n", handle->local->comp_host);
   printf("local component is \"%s\"\n", handle->local->comp_name);
   printf("local component id is \"%ld\"\n", handle->local->comp_id);

   cl_com_get_connect_port(handle, &port);
   printf("connecting to port \"%d\" on host \"%s\"\n", port, argv[3]);

   while (do_shutdown == 0) {
      int retval = 0;
      cl_com_message_t *message = nullptr;
      cl_com_endpoint_t *sender = nullptr;

      cl_commlib_trigger(handle, 1);
      retval = cl_commlib_receive_message(handle, nullptr, nullptr, 0,      /* handle, comp_host, comp_name , comp_id, */
                                          false, 0,                 /* syncron, response_mid */
                                          &message, &sender);
      if (retval != CL_RETVAL_OK) {
         if (retval == CL_RETVAL_CONNECTION_NOT_FOUND) {
            printf("open connection to \"%s/%s/%d\" ...\n", argv[3], argv[4], atoi(argv[5]));
            retval = cl_commlib_open_connection(handle, argv[3], argv[4], atoi(argv[5]));
         }
      } else {
         for (unsigned long i = 0; i < message->message_length; i++) {
            line_buffer[line_index] = message->message[i];

            switch (line_buffer[line_index]) {
               case 0:
                  /* ignore string end information */
                  break;
               case '\n': {
                  line_index++;
                  line_buffer[line_index] = 0;
                  print_line(line_buffer);
                  line_index = 0;
                  break;
               }
               default:
                  line_index++;
            }
         }
         fflush(stdout);
         cl_com_free_message(&message);
         cl_com_free_endpoint(&sender);
      }
   }

   printf("shutdown commlib ...\n");
   cl_com_cleanup_commlib();

   printf("main done\n");
   return 0;
}

