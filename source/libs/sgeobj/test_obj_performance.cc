#include <cstdio>
#include <cstdlib>

#include <chrono>

#include "basis_types.h"
#include "uti/sge_dstring.h"
#include "uti/sge_unistd.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_job.h"

/*
 * @todo: make it a C++ program
 *    - make the random array a class, e.g. derived from std::list
 *       - constructor taking the size and initializing the random values
 *       - load and save methods
 *       - it should inherit iterators or extended for syntax from std::list
 *    - c++ way to generate random data
 */

int num_jobs = 10;
u_long32 *random_array = nullptr;
lList *master_job_list = nullptr;

static bool
load_rand_file(const char *filename)
{
   bool ret = true;
   char line[100];
   FILE *fp;

   fp = fopen(filename, "r");
   if (fp == nullptr) {
      fprintf(stderr, "error opening file %s for reading\n", filename);
      ret = false;
   } else {
      random_array = (u_long32 *)malloc(num_jobs * sizeof(u_long32));
      for (int i = 0; i < num_jobs; i++) {
         char *str;
         if ((str = fgets(line, sizeof(line), fp)) != nullptr) {
            random_array[i] = SGE_STRTOU_LONG32(line);
         } else {
            fprintf(stderr, "reading line %d of %s failed\n", i + 1, filename);
            break;
         }
      }

      fclose(fp);
   }

   return ret;
}

static bool
create_rand_file(const char *filename)
{
   bool ret = true;
   FILE *fp;

   srand(time(0));

   // fill in a sequence of job ids into an array
   random_array = (u_long32 *)malloc(num_jobs * sizeof(u_long32));
   for (int i = 0; i < num_jobs; i++) {
      random_array[i] = i + 1;
   }

   // shuffle
   for (int i = 0; i < num_jobs; i++) {
      int other = rand() % num_jobs;
      int tmp = random_array[i];
      random_array[i] = random_array[other];
      random_array[other] = tmp;
   }

   fp = fopen(filename, "w");
   if (fp == nullptr) {
      fprintf(stderr, "error opening file %s for writing\n", filename);
      ret = false;
   } else {
      for (int i = 0; i < num_jobs; i++) {
         fprintf(fp, sge_uu32"\n", random_array[i]);
      }
      fclose(fp);
   }

   return ret;
}

static bool
load_or_create_rand_file()
{
   DSTRING_STATIC(filename_dstr, 100);
   const char *filename_str;
   SGE_STRUCT_STAT stat_buf{0};

   filename_str = sge_dstring_sprintf(&filename_dstr, "test_performance_rand_%d.txt", num_jobs);

   if (SGE_STAT(filename_str, &stat_buf) == 0) {
      // file exists, read contents
      printf("random file %s exists, reading it\n", filename_str);
      return load_rand_file(filename_str);
   } else {
      // file does not exist, create it
      printf("random file %s does not yet exist, creating it\n", filename_str);
      return create_rand_file(filename_str);
   }
}

static bool
create_jobs()
{
   bool ret = true;
   typedef struct {
      const char *user;
      const char *group;
      const int uid;
      const int gid;
   } user;
   static user users[] = {
      { "bilbo", "hobbit", 100, 100 },
      { "frodo", "hobbit", 101, 100 },
      { "meriadoc", "hobbit", 102, 100 },
      { "peregrin", "hobbit", 103, 100 },
      { "samweis", "hobbit", 104, 100 },
      { "gollum", "hobbit", 105, 100 },
      { "fredegar", "hobbit", 106, 100 },
      { "durin", "dwarf", 107, 101 },
      { "dain", "dwarf", 108, 101 },
      { "gloin", "dwarf", 109, 101 },
      { "gimli", "dwarf", 110, 101 },
      { "balin", "dwarf", 111, 101 },
      { "dwalin", "dwarf", 112, 101 },
      { "fili", "dwarf", 113, 101 },
      { "kili", "dwarf", 114, 101 },
      { "ugluk", "ork", 115, 102 },
      { "grischnakh", "ork", 116, 102 },
      { "azog", "ork", 117, 102 },
      { "bolg", "ork", 118, 102 }
   };
   const int num_users = 19;

   auto time_start = std::chrono::high_resolution_clock::now();
   for (int i = 0; i < num_jobs; i++) {
      lListElem *ep = lAddElemUlong(&master_job_list, JB_job_number, i + 1, JB_Type);
      int idx = i % num_users;
      lSetString(ep, JB_owner, users[idx].user);
      lSetString(ep, JB_group, users[idx].group);
      lSetUlong(ep, JB_uid, users[idx].uid);
      lSetUlong(ep, JB_gid, users[idx].gid);
   }
   auto time_end = std::chrono::high_resolution_clock::now();
   auto millisecs = std::chrono::duration_cast<std::chrono::milliseconds>(time_end - time_start);
   printf("adding %d jobs took %.3f s\n", num_jobs, millisecs.count() / 1000.0);

   return ret;
}

static bool
update_jobs()
{
   bool ret = true;
   int i;

   auto time_start = std::chrono::high_resolution_clock::now();
   for (i = 0; i < num_jobs; i++) {
      u_long32 job_id = random_array[i];
      lListElem *job = lGetElemUlongRW(master_job_list, JB_job_number, job_id);
      if (job == nullptr) {
         fprintf(stderr, "didn't find job " sge_uu32 " in job list\n", job_id);
         ret = false;
         break;
      } else {
         lAddUlong(job, JB_version, 1);
         lSetString(job, JB_script_file, "/bin/sleep");
         lSetUlong(job, JB_jobshare, i);
         lSetDouble(job, JB_urg, job_id % 100);
         // add some sub objects, e.g. a gdil
      }
   }
   auto time_end = std::chrono::high_resolution_clock::now();
   auto millisecs = std::chrono::duration_cast<std::chrono::milliseconds>(time_end - time_start);
   printf("updating %d jobs took %.3f s\n", num_jobs, millisecs.count() / 1000.0);

   return ret;
}

static bool
delete_jobs()
{
   bool ret = true;
   int i;

   auto time_start = std::chrono::high_resolution_clock::now();
   for (i = 0; i < num_jobs; i++) {
      u_long32 job_id = random_array[i];
      lListElem *job = lGetElemUlongRW(master_job_list, JB_job_number, job_id);
      if (job == nullptr) {
         fprintf(stderr, "didn't find job " sge_uu32 " in job list\n", job_id);
         ret = false;
         break;
      } else {
         lRemoveElem(master_job_list, &job);
      }
   }
   lFreeList(&master_job_list);

   auto time_end = std::chrono::high_resolution_clock::now();
   auto millisecs = std::chrono::duration_cast<std::chrono::milliseconds>(time_end - time_start);
   printf("deleting %d jobs took %.3f s\n", num_jobs, millisecs.count() / 1000.0);


   return ret;
}

int main(int argc, const char *argv[])
{
   if (argc > 1) {
      num_jobs = atoi(argv[1]);
   }

   lInit(nmv);

   printf("Testing with %d jobs\n", num_jobs);

   if (!load_or_create_rand_file()) {
      return EXIT_FAILURE;
   }

   if (!create_jobs()) {
      return EXIT_FAILURE;
   }

   if (!update_jobs()) {
      return EXIT_FAILURE;
   }

   if (!delete_jobs()) {
      return EXIT_FAILURE;
   }

   return EXIT_SUCCESS;
}
