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
 * @brief What happens when a broken sd-bus connection is torn down?
 *
 * A manual test for the teardown behaviour that ocs::uti::Systemd depends on. It mirrors the
 * class: libsystemd is opened with dlopen and its entry points are resolved with dlsym, so the
 * test exercises the same calls in the same order as the destructor does.
 *
 * The test opens a connection to the system bus, reads a property to show that the connection
 * works, breaks the connection in one of three ways, reads the property again and then drops the
 * connection. Each step is reported, so the output shows which error the broken connection
 * produces and whether libsystemd can still be asked about its state.
 *
 * This is deliberately a C file with no dependency on our libraries: it compiles with nothing but
 * a C compiler and the systemd headers, which makes it usable on a customer-like host where no
 * C++ compiler is installed.
 *
 *     gcc -O0 -g -Wall -o bus_teardown bus_teardown.c -ldl
 *     ./bus_teardown <teardown> [<reset>]
 *
 * Needs a system bus and has to run as root, like the daemons which use the class.
 *
 * teardown:
 *    flush   sd_bus_flush_close_unref(), what the destructor did before CS-2552
 *    unref   sd_bus_unref(), no I/O on the connection
 *    auto    flush when sd_bus_is_open() says the connection is open, unref otherwise
 *    guard   as auto, but refuse to drop a connection whose file descriptor is not ours any more,
 *            which is what the destructor does now
 *
 * reset:
 *    shutdown   shutdown() on the connection's socket - what a peer reset looks like (default)
 *    close      close() the connection's file descriptor behind libsystemd's back
 *    reuse      close() it and let an unrelated file take the number
 *
 * Expected, verified on Rocky 8 (systemd 239) and Ubuntu 24 (systemd 255):
 *
 *    shutdown   the read fails with -ECONNRESET and sd_bus_is_open() reports 0 afterwards.
 *               Every teardown is clean.
 *    close      the read fails with -EBADF but sd_bus_is_open() still reports 1, because
 *               libsystemd never saw a transport error. flush, unref and auto all abort the
 *               process in libsystemd's safe_close(), guard reports the lost descriptor instead.
 *    reuse      as close, but flush and unref silently close the unrelated file rather than
 *               aborting. Only guard notices.
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <systemd/sd-bus.h>

typedef int (*open_system_t)(sd_bus **);
typedef sd_bus *(*unref_t)(sd_bus *);
typedef int (*is_open_t)(sd_bus *);
typedef int (*get_fd_t)(sd_bus *);
typedef int (*get_property_t)(sd_bus *, const char *, const char *, const char *,
                              const char *, sd_bus_error *, sd_bus_message **, const char *);
typedef int (*msg_read_t)(sd_bus_message *, const char *, ...);
typedef sd_bus_message *(*msg_unref_t)(sd_bus_message *);
typedef void (*error_free_t)(sd_bus_error *);

static open_system_t f_open;
static unref_t f_unref;
static unref_t f_flush_close_unref;
static is_open_t f_is_open;
static is_open_t f_is_ready;
static get_fd_t f_get_fd;
static get_property_t f_get_property;
static msg_read_t f_msg_read;
static msg_unref_t f_msg_unref;
static error_free_t f_error_free;

static void *
sym(void *handle, const char *name) {
   void *ret = dlsym(handle, name);

   if (ret == NULL) {
      fprintf(stderr, "cannot resolve %s: %s\n", name, dlerror());
      exit(2);
   }

   return ret;
}

/* read the systemd Version property - the same call Systemd::initialize() makes */
static int
read_version(sd_bus *bus, char **value) {
   sd_bus_error error = SD_BUS_ERROR_NULL;
   sd_bus_message *m = NULL;

   int ret = f_get_property(bus, "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
                            "org.freedesktop.systemd1.Manager", "Version", &error, &m, "s");
   if (ret >= 0) {
      const char *str = NULL;

      ret = f_msg_read(m, "s", &str);
      if (ret >= 0 && str != NULL && value != NULL) {
         *value = strdup(str);
      }
   }
   if (m != NULL) {
      f_msg_unref(m);
   }
   f_error_free(&error);

   return ret;
}

static void
report_state(sd_bus *bus, const char *when) {
   printf("%-19s sd_bus_is_open=%d sd_bus_is_ready=%d\n", when, f_is_open(bus), f_is_ready(bus));
   fflush(stdout);
}

static unsigned long long
inode_of(int fd) {
   struct stat st;

   if (fd < 0 || fstat(fd, &st) != 0) {
      return 0;
   }

   return (unsigned long long)st.st_ino;
}

int
main(int argc, char *argv[]) {
   const char *teardown = argc > 1 ? argv[1] : "guard";
   const char *reset = argc > 2 ? argv[2] : "shutdown";

   void *handle = dlopen("libsystemd.so.0", RTLD_LAZY);
   if (handle == NULL) {
      fprintf(stderr, "dlopen failed: %s\n", dlerror());
      return 2;
   }
   f_open = (open_system_t)sym(handle, "sd_bus_open_system");
   f_unref = (unref_t)sym(handle, "sd_bus_unref");
   f_flush_close_unref = (unref_t)sym(handle, "sd_bus_flush_close_unref");
   f_is_open = (is_open_t)sym(handle, "sd_bus_is_open");
   f_is_ready = (is_open_t)sym(handle, "sd_bus_is_ready");
   f_get_fd = (get_fd_t)sym(handle, "sd_bus_get_fd");
   f_get_property = (get_property_t)sym(handle, "sd_bus_get_property");
   f_msg_read = (msg_read_t)sym(handle, "sd_bus_message_read");
   f_msg_unref = (msg_unref_t)sym(handle, "sd_bus_message_unref");
   f_error_free = (error_free_t)sym(handle, "sd_bus_error_free");

   sd_bus *bus = NULL;
   int ret = f_open(&bus);
   if (ret < 0) {
      fprintf(stderr, "sd_bus_open_system: %d %s\n", ret, strerror(-ret));
      return 2;
   }
   printf("teardown %s, reset %s\n", teardown, reset);

   /* what Systemd::connect() remembers for the check in the destructor */
   int fd = f_get_fd(bus);
   unsigned long long inode = inode_of(fd);
   printf("connected           fd=%d inode=%llu\n", fd, inode);

   char *version = NULL;
   ret = read_version(bus, &version);
   printf("first read          ret=%d version=%s\n", ret, version != NULL ? version : "(none)");
   if (ret < 0) {
      fprintf(stderr, "the connection did not work in the first place - giving up\n");
      return 2;
   }
   report_state(bus, "before reset");

   if (strcmp(reset, "close") == 0 || strcmp(reset, "reuse") == 0) {
      if (close(fd) < 0) {
         fprintf(stderr, "close: %s\n", strerror(errno));
         return 2;
      }
      if (strcmp(reset, "reuse") == 0) {
         int other = open("/dev/null", O_RDONLY);

         printf("reused              fd=%d now refers to /dev/null\n", other);
      }
   } else {
      if (shutdown(fd, SHUT_RDWR) < 0) {
         fprintf(stderr, "shutdown: %s\n", strerror(errno));
         return 2;
      }
   }
   printf("reset               by %s\n", reset);

   ret = read_version(bus, NULL);
   printf("read after reset    ret=%d %s\n", ret, ret < 0 ? strerror(-ret) : "unexpectedly ok");
   report_state(bus, "after reset");

   printf("tearing down with %s ...\n", teardown);
   fflush(stdout);
   if (strcmp(teardown, "flush") == 0) {
      f_flush_close_unref(bus);
   } else if (strcmp(teardown, "unref") == 0) {
      f_unref(bus);
   } else if (strcmp(teardown, "guard") == 0 && inode != 0 && inode_of(fd) != inode) {
      printf("guard               the file descriptor is not ours any more"
             " - leaking the connection\n");
   } else if (f_is_open(bus) > 0) {
      f_flush_close_unref(bus);
   } else {
      f_unref(bus);
   }
   printf("teardown returned - no abort\n");

   return 0;
}
