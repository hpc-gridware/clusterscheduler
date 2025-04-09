/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025 HPC-Gridware GmbH
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

#include <math.h>
#include <mpi.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include <unistd.h>

long clock_ticks = 0;

// commands to be sent from master rank to the worker ranks
typedef enum {
   CMD_INIT,      // initialize
   CMD_WORK,      // do one work unit
   CMD_USAGE,     // report usage
   CMD_EXIT       // exit
} Commands;

// convert command to string for debugging
const char *command_to_string(Commands cmd) {
   switch (cmd) {
      case CMD_INIT:
         return "CMD_INIT";
      case CMD_WORK:
         return "CMD_WORK";
      case CMD_USAGE:
         return "CMD_USAGE";
      case CMD_EXIT:
         return "CMD_EXIT";
      default:
         return "UNKNOWN";
   }
}

// usage fields
typedef enum {
   USAGE_UTIME = 0,     // user CPU time in seconds
   USAGE_STIME,         // system CPU time in seconds
   USAGE_MAXRSS,        // maximum resident set size in kB
   USAGE_NUM_FIELDS
} UsageFields;

// get usage information via getrusage() and store it in the usage array
static void get_usage(double *usage) {
   struct rusage rusage;
   getrusage(RUSAGE_SELF, &rusage);
   usage[USAGE_UTIME] = rusage.ru_utime.tv_sec + rusage.ru_utime.tv_usec / 1000000.0;
   usage[USAGE_STIME] = rusage.ru_stime.tv_sec + rusage.ru_stime.tv_usec / 1000000.0;
   usage[USAGE_MAXRSS] = rusage.ru_maxrss;
}

// information we gather about every rank of the MPI job
typedef struct {
   int rank;                           // MPI rank
   char *name;                         // OCS pe_task_id (if available), or MPI processor name
   int work_done;                      // number of work units done
   double usage[USAGE_NUM_FIELDS];     // usage information
} RankInfo;

// create and initialize the rank info
static RankInfo *
create_rank_info(int world_size) {
   RankInfo *rank_info = calloc(world_size + 1, sizeof(RankInfo));
   return rank_info;
}

// delete the rank info
static void
delete_rank_info(RankInfo **info, int world_size) {
   for (int i = 0; i < world_size; i++) {
      free((*info)[i].name);
   }
   free(*info);
   *info = NULL;
}

// dump the rank info to stdout, e.g.
//    Rank |           Processor |     work |    utime |    stime |   maxrss
//---------+---------------------+----------+----------+----------+---------
//       0 |              master |        0 |   11.677 |   23.331 |    10244
//---------+---------------------+----------+----------+----------+---------
//       1 |     rocky-8-amd64-1 |       35 |    6.392 |    0.030 |    10184
//---------+---------------------+----------+----------+----------+---------
//       2 | 1.ubuntu-24-amd64-1 |       34 |    8.268 |    0.619 |    10112
//---------+---------------------+----------+----------+----------+---------
//       3 | 1.ubuntu-22-amd64-1 |       35 |    7.024 |    0.032 |     9608
//---------+---------------------+----------+----------+----------+---------
static void
dump_info_stdout(RankInfo *rank_info, int world_size) {
   int max_name_len = 0;
   for (int i = 0; i < world_size; i++) {
      int name_len = strlen(rank_info[i].name);
      if (name_len > max_name_len) {
         max_name_len = name_len;
      }
   }
   char separator[256];
   sprintf(separator, "%*s-+-", 8, "--------");
   for (int i = 0; i < max_name_len; i++) {
      strcat(separator, "-");
   }
   sprintf(separator + strlen(separator), "-+-%*s-+-%*s-+-%*s-+-%*s", 8, "--------", 8, "--------", 8, "--------", 8, "--------");

   printf("%*s | %*s | %*s | %*s | %*s | %*s\n", 8, "Rank", max_name_len, "Processor", 8, "work", 8, "utime", 8, "stime", 8, "maxrss");
   printf("%s\n", separator);
   for (int i = 0; i < world_size; i++) {
      printf("%8d | %*s | %8d | %8.3f | %8.3f | %8.0f\n", rank_info[i].rank, max_name_len, rank_info[i].name, rank_info[i].work_done, rank_info[i].usage[USAGE_UTIME], rank_info[i].usage[USAGE_STIME], rank_info[i].usage[USAGE_MAXRSS]);
      printf("%s\n", separator);
   }
}

// dump the rank info to a CSV file
const char *csv_filename = NULL;
static void
dump_info_csv(RankInfo *rank_info, int world_size, int restarted_from_checkpoint) {
   if (csv_filename != NULL) {
      // when we restart from a checkpoint, we append to the existing file
      FILE *file = fopen(csv_filename, restarted_from_checkpoint ? "a" : "w");
      if (file != NULL) {
         // print header
         if (!restarted_from_checkpoint) {
            fprintf(file, "\"Rank\",\"Processor\",\"work\", \"utime\",\"stime\",\"maxrss\"\n");
         }
         // print data
         for (int i = 0; i < world_size; i++) {
            fprintf(file, "%d,\"%s\",%d,%0.3f,%0.3f,%.0f\n", rank_info[i].rank, rank_info[i].name, rank_info[i].work_done, rank_info[i].usage[USAGE_UTIME], rank_info[i].usage[USAGE_STIME], rank_info[i].usage[USAGE_MAXRSS]);
         }
         fclose(file);
      }
   }
}

// dump the rank info to stdout and optionally a CSV file
static void
dump_info(RankInfo *rank_info, int world_size, int restarted_from_checkpoint) {
   dump_info_stdout(rank_info, world_size);
   dump_info_csv(rank_info, world_size, restarted_from_checkpoint);
}

// verbose output
// enable with command line option -verbose
int verbose=0;
static void
log_verbose(const char *format, ...) {
   if (verbose) {
      va_list args;
      va_start(args, format);
      vprintf(format, args);
      va_end(args);
   }
}

// print MPI error message and abort
static void
print_error_and_abort(int error_code) {
   char error_string[MPI_MAX_ERROR_STRING];
   int length_of_error_string;
   MPI_Error_string(error_code, error_string, &length_of_error_string);
   fprintf(stderr, "MPI error %d: %s\n", error_code, error_string);
   MPI_Abort(MPI_COMM_WORLD, error_code);
}

// If we want to distribute settings, e.g. from command line options, we can use MPI_Bcast:
// MPI_Bcast(&work_time, 1, MPI_INT, 0, MPI_COMM_WORLD);

// MPI send with error checking
static void
mpi_send_and_check(void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm) {
   int error_code = MPI_Send(buf, count, datatype, dest, tag, comm);
   if (error_code != MPI_SUCCESS) {
      print_error_and_abort(error_code);
   }
}

// MPI receive with error checking
// @todo receive with timeout
static void
mpi_receive_and_check(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status) {
   int error_code = MPI_Recv(buf, count, datatype, source, tag, comm, status);
   if (error_code != MPI_SUCCESS) {
      print_error_and_abort(error_code);
   }
}

// logic of rank 0, the master rank
// it sends commands to the worker ranks and collects the results
// - initialize the worker ranks
// - distribute work to the worker ranks
// - collect results from the worker ranks
// - collect usage information from the worker ranks
// - collect usage information from itself
// - tell the worker ranks to exit
// - dump the collected information
static void
rank_0(int world_size, int work_time) {
   printf("Initializing MPI job with %d ranks\n", world_size);
   // Rank 0 distributes work
   RankInfo *rank_info = create_rank_info(world_size);
   rank_info[0].rank = 0;
   rank_info[0].name = strdup("master");

   // initialize all worker ranks, they will send their processor name back
   for (int i = 1; i < world_size; i++) {
      int cmd = CMD_INIT;
      mpi_send_and_check(&cmd, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
      log_verbose("Rank 0 sent CMD_INIT to rank %d\n", i);
   }
   for (int i = 1; i < world_size; i++) {
      MPI_Status status;
      char processor_name[MPI_MAX_PROCESSOR_NAME];
      mpi_receive_and_check(&processor_name, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
      log_verbose("Rank 0 received processor name %s from rank %d\n", processor_name, status.MPI_SOURCE);
      rank_info[status.MPI_SOURCE].rank = status.MPI_SOURCE;
      rank_info[status.MPI_SOURCE].name = strdup(processor_name);
   }

   // read checkpoint if it exists
   int restarted_from_checkpoint = 0;
   FILE *checkpoint = fopen("checkpoint", "r");
   if (checkpoint != NULL) {
      int checkpoint_work_units;
      if (fscanf(checkpoint, "%d", &checkpoint_work_units) == 1) {
         work_time -= checkpoint_work_units;
         printf("Restarting from checkpoint with %d work units\n", work_time);
         restarted_from_checkpoint = 1;
      }
      fclose(checkpoint);
      unlink("checkpoint");
   }


   // while there is work to be done, send it to the workers
   printf("Distributing %d work units\n", work_time);
   int num_work_units = work_time;
   int done_work_units = 0;
   int sent_work_units = 0;

   // send the first work unit to each worker
   for (int i = 1; i < world_size; i++) {
      int cmd = CMD_WORK;
      mpi_send_and_check(&cmd, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
      log_verbose("Rank 0 sent CMD_WORK to rank %d\n", i);
      sent_work_units++;
   }
   while (done_work_units < num_work_units) {
      // wait for result from a worker and send it the next work unit
      MPI_Status status;
      int dummy;
      mpi_receive_and_check(&dummy, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
      log_verbose("Rank 0 received work result from rank %d\n", status.MPI_SOURCE);
      done_work_units++;
      rank_info[status.MPI_SOURCE].work_done++;
      printf("%8d done\r", done_work_units);

      if (sent_work_units < num_work_units) {
         int cmd = CMD_WORK;
         mpi_send_and_check(&cmd, 1, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD);
         log_verbose("Rank 0 sent CMD_WORK to rank %d\n", status.MPI_SOURCE);
         sent_work_units++;
      }

      // check if a file "abort" exists in the current working directory
      // if it does, write the sent_work_units as checkpoint
      // still wait for the results from the workers, but do not send more work
      struct stat statbuf;
      if (stat("abort", &statbuf) == 0) {
         printf("Aborting work\n");
         unlink("abort");
         // write a checkpoint
         FILE *checkpoint = fopen("checkpoint", "w");
         if (checkpoint != NULL) {
            fprintf(checkpoint, "%d\n", sent_work_units);
            fclose(checkpoint);
         }
         // stop when the work sent so far has been processed
         num_work_units = sent_work_units;
      }
   }

   // fetch usage from the workers
   printf("\nRetrieving usage\n");
   for (int i = 1; i < world_size; i++) {
      int cmd = CMD_USAGE;
      mpi_send_and_check(&cmd, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
      log_verbose("Rank 0 sent CMD_USAGE to rank %d\n", i);
   }
   for (int i = 1; i < world_size; i++) {
      MPI_Status status;
      double usage[USAGE_NUM_FIELDS];
      mpi_receive_and_check(usage, USAGE_NUM_FIELDS, MPI_DOUBLE, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
      log_verbose("Rank 0 received usage from rank %d\n", status.MPI_SOURCE);
      rank_info[status.MPI_SOURCE].usage[USAGE_UTIME] = usage[USAGE_UTIME];
      rank_info[status.MPI_SOURCE].usage[USAGE_STIME] = usage[USAGE_STIME];
      rank_info[status.MPI_SOURCE].usage[USAGE_MAXRSS] = usage[USAGE_MAXRSS];
   }

   // own usage
   double usage[USAGE_NUM_FIELDS];
   get_usage(usage);
   rank_info[0].usage[USAGE_UTIME] = usage[USAGE_UTIME];
   rank_info[0].usage[USAGE_STIME] = usage[USAGE_STIME];
   rank_info[0].usage[USAGE_MAXRSS] = usage[USAGE_MAXRSS];

   // tell all workers to exit
   printf("Shutting down MPI tasks\n");
   for (int i = 1; i < world_size; i++) {
      int cmd = CMD_EXIT;
      mpi_send_and_check(&cmd, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
      log_verbose("Rank 0 sent CMD_EXIT to rank %d\n", i);
   }

   // dump the collected information to stdout and optionally (option -o) to a CSV file
   dump_info(rank_info, world_size, restarted_from_checkpoint);
   delete_rank_info(&rank_info, world_size);
}

// initialize a worker rank
static void
rank_n_init() {
   char processor_name[MPI_MAX_PROCESSOR_NAME];
   memset(processor_name, 0, MPI_MAX_PROCESSOR_NAME);
   const char *pe_task_id = getenv("SGE_PE_TASK_ID");
   if (pe_task_id != NULL) {
      strncpy(processor_name, pe_task_id, MPI_MAX_PROCESSOR_NAME - 1);
      processor_name[MPI_MAX_PROCESSOR_NAME - 1] = '\0';
   } else {
      int name_len;
      MPI_Get_processor_name(processor_name, &name_len);
   }
   mpi_send_and_check(processor_name, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
}

// do work in a worker rank
static void
rank_n_work() {
   // do work for approximately one second
   clock_t end = times(NULL) + clock_ticks;
   int loops = 0;
   while (times(NULL) < end) {
      // burn some CPU time
      for (int i = 0; i < 10000; i++) {
         double x = i, y, z;
         y = sin(x);
         z = cos(y);
         x *= z;
      }
      // and wait a little bit, we don't want to overload the machines (test VMs)
      usleep(1000);
      loops++;
   }

   // send result to rank 0
   mpi_send_and_check(&loops, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
}

// get usage information via getrusage() and send it to rank 0
static void rank_n_usage() {
   double usage[USAGE_NUM_FIELDS];
   get_usage(usage);
   mpi_send_and_check(usage, USAGE_NUM_FIELDS, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD);
}

// logic of a worker rank
// process commands from rank 0
// and return a result
static void rank_n(int rank) {
   // Other ranks receive work
   int done = 0;
   while (!done) {
      int cmd;
      mpi_receive_and_check(&cmd, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      log_verbose("Rank %d received command %s from rank 0\n", rank, command_to_string((Commands)cmd));
      switch (cmd) {
         case CMD_INIT:
            rank_n_init();
            break;
         case CMD_WORK:
            rank_n_work();
            break;
         case CMD_USAGE:
            rank_n_usage();
            break;
         case CMD_EXIT:
            done = 1;
            break;
         default:
            fprintf(stderr, "Unknown command\n");
            break;
      }
   }
}

static void
usage(const char *argv0) {
   fprintf(stderr, "Usage: %s [-verbose] [-o csvfilename] [runtime in seconds]\n", argv0);
   fprintf(stderr, "  -verbose: verbose output\n");
   fprintf(stderr, "  -o csvfilename: write output to CSV file\n");
   fprintf(stderr, "  runtime in seconds, default is 60 seconds\n");
   MPI_Abort(MPI_COMM_WORLD, MPI_ERR_ARG);
   exit(1);
}

/**
 * Arguments: [runtime in seconds]
 */
int main(int argc, char** argv) {
   clock_ticks = sysconf(_SC_CLK_TCK);
   int work_time = 60; // in seconds of work to be done - it will be distributed over the workers
   int error_code;

   MPI_Init(&argc, &argv);
   //MPI_Errhandler_set(MPI_COMM_WORLD, MPI_ERRORS_RETURN);
   MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);

   int world_size;
   error_code = MPI_Comm_size(MPI_COMM_WORLD, &world_size);
   if (error_code != MPI_SUCCESS) {
      print_error_and_abort(error_code);
   }
   if (world_size < 2) {
      fprintf(stderr, "This program must be run with at least 2 processes\n");
      MPI_Abort(MPI_COMM_WORLD, MPI_ERR_ARG);
   }

   int world_rank;
   error_code = MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
   if (error_code != MPI_SUCCESS) {
      print_error_and_abort(error_code);
   }

   if (world_rank == 0) {
      // do command line parsing only in rank 0
      int idx = 1;
      while (idx < argc && argv[idx][0] == '-') {
         if (strcmp(argv[idx], "-verbose") == 0) {
            verbose = 1;
         } else if (strcmp(argv[idx], "-o") == 0) {
            if (idx + 1 < argc) {
               idx++;
               csv_filename = argv[idx];
            } else {
               usage(argv[0]);
            }
         } else {
            usage(argv[0]);
         }
         idx++;
      }
      if (argc > idx) {
         work_time = atoi(argv[idx]);
      }

      // Rank 0 distributes work
      rank_0(world_size, work_time);
   } else {
      rank_n(world_rank);
   }

   MPI_Finalize();
   if (world_rank == 0) {
      printf("testmpi finished successfully\n");
   }
   return 0;
}