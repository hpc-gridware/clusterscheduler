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
 *  Portions of this code are Copyright 2011 Univa Inc.
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <algorithm>
#include <cstdio>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <unistd.h>

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <pwd.h>

#include <termios.h>
#if defined(DARWIN)
#  include <sys/ttycom.h>
#  include <sys/ioctl.h>
#elif defined(FREEBSD) || defined(NETBSD)
#  include <sys/ioctl.h>
#endif

#include "uti/sge_stdlib.h"
#include "uti/sge_dstring.h"
#include "uti/sge_pty.h"
#include "uti/sge_io.h"
#include "uti/sge_unistd.h"
#include "uti/sge_signal.h"
#include "uti/config_file.h"

#include "sge_ijs_comm.h"
#include "sge_ijs_threads.h"
#include <cinttypes>
#include "err_trace.h"
#include "shepherd.h"

#define RESPONSE_MSG_TIMEOUT 120

#define COMM_SERVER "qrsh_ijs"
#define COMM_CLIENT "shepherd_ijs"

/*
 * Compile with EXTENSIVE_TRACING defined to get lots of trace messages from
 * the two worker threads.
 * @note Some tests will fail when compiled with extensive tracing, e.g.,
 * the qrsh_terminate and qrsh_suspend check_functions of the qrsh test.
 */
#undef EXTENSIVE_TRACING
#if 0
#define EXTENSIVE_TRACING
#endif

static ijs_fds_t     *g_p_ijs_fds         = nullptr;
static COMM_HANDLE   *g_comm_handle       = nullptr;
static THREAD_HANDLE g_thread_main;
static int           g_raised_event        = 0;
static int           g_job_pid             = 0;
static int           g_ijs_reconnect_timeout = 0;   ///< grace period seconds before SIGKILL on unexpected disconnect; 0 = disabled
static cl_framework_t g_ijs_framework      = CL_CT_TCP;   ///< commlib framework copied from parent_loop, needed for reconnect attempts
static char          *g_job_owner          = nullptr;     ///< job owner copied from parent_loop, needed for commlib auth on reconnect

static char          *g_hostname           = nullptr;
extern int           received_signal; /* defined in shepherd.c */

/* X11 forwarding state (set by commlib_to_pty on X11_AUTH_MSG, read by pty_to_commlib) */
#define X11_MAX_CONNS  64
static int              g_x11_listen_fd                  = -1;   ///< Unix socket accepting X11 from job
static int              g_x11_client_fds[X11_MAX_CONNS]; ///< per-conn_id fd toward job X client (-1=unused)
static int              g_x11_display_num                = -1;   ///< display number N for :N.0
static int              g_x11_next_conn_id               = 0;    ///< round-robin conn_id allocator
static char             g_x11_socket_path[108]           = "";   ///< path to /tmp/.X11-unix/XN (for cleanup)
static pthread_mutex_t  g_x11_mutex                      = PTHREAD_MUTEX_INITIALIZER;

/*
 * static functions
 */
/****** trace_buf() **********************************************************
*  NAME
*     trace_buf() -- writes contents of a buffer partially to the trace file
*
*  SYNOPSIS
*     int trace_buf(const char *buffer, int length, const char *format, ...)
*
*  FUNCTION
*     Writes the contents of a buffer, preceeded by a formatted string,
*     partially to the trace file. I.e. it first writes the formatted
*     string to the trace file, replacing the printf placeholders with
*     the variables provided in ..., and then adds the first 99 or "length"
*     characters (which ever is smaller), encapsulated by hyphens, to
*     the formatted string.
*
*  INPUTS
*     const char *buffer - the buffer that is to be written to the trace file
*     int        length  - length of the content of the buffer
*     const char *format -
*     int  fd          - file descriptor to read from
*     char *pbuf       - working buffer, must be of size BUFSIZE
*     int  *buf_bytes  - number of bytes already in the buffer
*
*  OUTPUTS
*     int *buf_bytes   - number of bytes in the buffer
*
*  RESULT
*     int - >0: OK, number of bytes read from the fd
*           =0: EINTR or EAGAIN occurred, just call select() and read again
*           -1: error occcured, connection was closed
*
*  NOTES
*     MT-NOTE:
*
*  SEE ALSO
*******************************************************************************/
#ifdef EXTENSIVE_TRACING
// This function will output commlib logging to the shepherd trace file
// when EXTENSIVE_TRACING is enabled.
static int
shepherd_log_list_flush_list(cl_raw_list_t* list_p) {

   if (list_p == nullptr) {
      return CL_RETVAL_LOG_NO_LOGLIST;
   }

   int ret_val;
   if ((ret_val = cl_raw_list_lock(list_p)) != CL_RETVAL_OK) {
      return ret_val;
   }

   // Log all entries of the log list via shepherd_trace().
   cl_log_list_elem_t *elem = nullptr;
   while ((elem = cl_log_list_get_first_elem(list_p)) != nullptr) {
      if (elem->log_parameter == nullptr) {
         shepherd_trace("COMMLIB|%s|%s|%s",
                        cl_thread_convert_state_id(elem->log_thread_state),
                        cl_log_list_convert_type_id(elem->log_type),
                        elem->log_message);
      } else {
         shepherd_trace("COMMLIB|%s|%s|%s %s",
                        cl_thread_convert_state_id(elem->log_thread_state),
                        cl_log_list_convert_type_id(elem->log_type),
                        elem->log_message, elem->log_parameter);
      }

      cl_log_list_del_log(list_p);
   }

   return cl_raw_list_unlock(list_p);
}

static int trace_buf(const char *buffer, int length, const char *format, ...)
{
   int         ret;
   va_list     ap;
   dstring     message = DSTRING_INIT;
   char        tmpbuf[100];

   if (length > 0) {
      snprintf(tmpbuf, std::min(99,length), "%s", buffer);
   } else {
      strcpy(tmpbuf, "");
   }

   va_start(ap, format);

   sge_dstring_vsprintf(&message, format, ap);
   sge_dstring_append(&message, "\"");
   sge_dstring_append(&message, tmpbuf);
   sge_dstring_append(&message, "\"");
   ret = shepherd_trace("%s", sge_dstring_get_string(&message));

   sge_dstring_free(&message);
   va_end(ap);

   return ret;
}
#endif

/****** append_to_buf() ******************************************************
*  NAME
*     append_to_buf() -- copies data from pty (or pipe) to a buffer
*
*  SYNOPSIS
*     int append_to_buf(int fd, char *pbuf, int *buf_bytes)
*
*  FUNCTION
*     Reads data from the pty or pipe and appends it to the given buffer.
*
*  INPUTS
*     int  fd          - file descriptor to read from
*     char *pbuf       - working buffer, must be of size BUFSIZE
*     int  *buf_bytes  - number of bytes already in the buffer
*
*  OUTPUTS
*     int *buf_bytes   - number of bytes in the buffer
*
*  RESULT
*     int - >0: OK, number of bytes read from the fd
*           =0: EINTR or EAGAIN occurred, just call select() and read again
*           -1: error occcured, connection was closed
*
*  NOTES
*     MT-NOTE:
*
*  SEE ALSO
*******************************************************************************/
static int append_to_buf(int fd, char *pbuf, int *buf_bytes)
{
   int nread = 0;

   if (fd >= 0) {
      nread = read(fd, &pbuf[*buf_bytes], BUFSIZE-1-(*buf_bytes));

      if (nread < 0 && (errno == EINTR || errno == EAGAIN)) {
         nread = 0;
      } else if (nread <= 0) {
         nread = -1;
      } else {
         *buf_bytes += nread;
      }
   }
   return nread;
}

/****** send_buf() ***********************************************************
*  NAME
*     send_buf() -- sends the content of the buffer over the commlib
*
*  SYNOPSIS
*     int send_buf(char *pbuf, int buf_bytes, int message_type)
*
*  FUNCTION
*     Sends the content of the buffer over to the commlib to the receiver.
*
*  INPUTS
*     char *pbuf        - buffer to send
*     int  buf_bytes   - number of bytes in the buffer to send
*     int  message_tye - type of the message that is to be sent over
*                        commlib. Can be STDOUT_DATA_MSG or
*                        STDERR_DATA_MSG, depending on where the data
*                        came from (pty and stdout = STDOUT_DATA_MSG,
*                        stderr = STDERR_DATA_MSG)
*
*  RESULT
*     int - 0: OK
*           1: could'nt write all data
*
*  NOTES
*     MT-NOTE:
*
*  SEE ALSO
*******************************************************************************/
static int send_buf(char *pbuf, unsigned long buf_bytes, int message_type)
{
   int ret = 0;
   DSTRING_STATIC(err_msg, MAX_STRING_SIZE);

   if (comm_write_message(g_comm_handle, g_hostname,
      COMM_SERVER, 1, (unsigned char*)pbuf,
      (unsigned long)buf_bytes, message_type, &err_msg) != buf_bytes) {
      shepherd_trace("couldn't write all data: %s",
                     sge_dstring_get_string(&err_msg));
      ret = 1;
   } else {
#ifdef EXTENSIVE_TRACING
      shepherd_trace("successfully wrote all data: %s",
                     sge_dstring_get_string(&err_msg));
#endif
   }

   return ret;
}

/**
 * @brief Parse reconnect.info written by execd into host/port/token.
 *
 * Format (one key=value per line):
 *   host=<hostname>
 *   port=<integer>
 *   token=<one-time token>
 *
 * @param path        Path to reconnect.info (relative to active_jobs/<jobid>).
 * @param host        Output buffer for hostname.
 * @param host_size   Size of host buffer.
 * @param port        Output for parsed port number.
 * @param token       Output buffer for token.
 * @param token_size  Size of token buffer.
 * @return 0 on success (all three fields parsed); -1 on missing field or I/O error.
 */
static int parse_reconnect_info(const char *path,
                                char *host, size_t host_size,
                                int *port,
                                char *token, size_t token_size) {
   FILE *fp = fopen(path, "r");
   if (fp == nullptr) {
      return -1;
   }

   bool have_host = false;
   bool have_port = false;
   bool have_token = false;
   char line[512];

   while (fgets(line, sizeof(line), fp) != nullptr) {
      char *nl = strchr(line, '\n');
      if (nl != nullptr) {
         *nl = '\0';
      }

      if (strncmp(line, "host=", 5) == 0) {
         strncpy(host, line + 5, host_size - 1);
         host[host_size - 1] = '\0';
         have_host = (strlen(host) > 0);
      } else if (strncmp(line, "port=", 5) == 0) {
         *port = atoi(line + 5);
         have_port = (*port > 0);
      } else if (strncmp(line, "token=", 6) == 0) {
         strncpy(token, line + 6, token_size - 1);
         token[token_size - 1] = '\0';
         have_token = (strlen(token) > 0);
      }
   }

   fclose(fp);
   return (have_host && have_port && have_token) ? 0 : -1;
}

/**
 * @brief Create an X11 proxy display on the execution host.
 *
 * Sets up the server side of the X11 forwarding tunnel for the interactive job:
 *
 * 1. Finds a free display number N >= 10 and binds a Unix-domain listen
 *    socket at /tmp/.X11-unix/XN (the proxy display).
 * 2. Converts the 32-character hex MIT-MAGIC-COOKIE-1 into 16 binary bytes
 *    and appends a FamilyWild Xau record to the job owner's ~/.Xauthority so
 *    X clients can authenticate against the proxy display.
 * 3. Appends "DISPLAY=:N.0" and "XAUTHORITY=<path>" to ./environment so the
 *    job process inherits them via sge_set_environment().
 *
 * Must be called in the commlib_to_pty thread when X11_AUTH_MSG is received,
 * BEFORE SETTINGS_CTRL_MSG unblocks the child — TCP ordering guarantees that
 * the child sees DISPLAY in its environment before it runs the user's command.
 * The listen fd is stored in module-level g_x11_listen_fd for use by
 * pty_to_commlib when accepting incoming X11 connections.
 *
 * @param cookie_hex 32-character hex MIT-MAGIC-COOKIE-1 forwarded from the
 *                   qrsh client via X11_AUTH_MSG.
 * @return true on success; false if no free display number was found.
 * @note MT-NOTE: not MT-safe (modifies g_x11_listen_fd, g_x11_display_num,
 *       g_x11_socket_path).  Uses geteuid() — not getuid() — to resolve the
 *       job owner's home directory; getuid() returns 0 in the shepherd parent
 *       which runs setuid root.
 */
static bool setup_x11_forwarding(const char *cookie_hex) {
   shepherd_trace("setup_x11_forwarding: starting with cookie_hex length %zu", strlen(cookie_hex));

   // Ensure /tmp/.X11-unix exists
   mkdir("/tmp/.X11-unix", 01777);

   // Find a free display number and bind a Unix socket
   bool found = false;
   for (int n = 10; n < 100 && !found; n++) {
      char sock_path[108];
      snprintf(sock_path, sizeof(sock_path), "/tmp/.X11-unix/X%d", n);

      struct sockaddr_un addr;
      memset(&addr, 0, sizeof(addr));
      addr.sun_family = AF_UNIX;
      strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

      int fd = socket(AF_UNIX, SOCK_STREAM, 0);
      if (fd < 0) {
         continue;
      }
      // Remove any stale socket from a previous run
      unlink(sock_path);
      if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0 &&
          listen(fd, 16) == 0) {
         chmod(sock_path, 0600);
         g_x11_listen_fd  = fd;
         g_x11_display_num = n;
         snprintf(g_x11_socket_path, sizeof(g_x11_socket_path), "%s", sock_path);
         found = true;
         shepherd_trace("setup_x11_forwarding: bound display :%d on %s (fd=%d)", n, sock_path, fd);
      } else {
         close(fd);
      }
   }
   if (!found) {
      shepherd_trace("setup_x11_forwarding: no free display number found");
      return false;
   }

   // Use euid (job owner), not uid (which is root in the shepherd parent process).
   struct passwd *pw = getpwuid(geteuid());
   const char *home = (pw != nullptr) ? pw->pw_dir : "/tmp";
   char xauth_path[512];
   snprintf(xauth_path, sizeof(xauth_path), "%s/.Xauthority", home);

   // Convert 32-char hex cookie to 16 binary bytes and write an Xau entry directly.
   // Avoids calling system()/xauth in a multithreaded context where waitpid races can deadlock.
   uint8_t cookie_bin[16];
   bool cookie_ok = true;
   for (int ci = 0; ci < 16 && cookie_ok; ci++) {
      unsigned int bval = 0;
      if (sscanf(cookie_hex + 2 * ci, "%02x", &bval) != 1) {
         cookie_ok = false;
      }
      cookie_bin[ci] = static_cast<uint8_t>(bval);
   }
   if (cookie_ok) {
      // Append a FamilyWild Xau record — matches any host/display for this cookie.
      FILE *xaf = fopen(xauth_path, "ab");
      if (xaf != nullptr) {
         const char *auth_name = "MIT-MAGIC-COOKIE-1";
         char display_str[16];
         snprintf(display_str, sizeof(display_str), "%d", g_x11_display_num);
         uint16_t display_len = static_cast<uint16_t>(strlen(display_str));
         uint16_t name_len    = static_cast<uint16_t>(strlen(auth_name));
         // Xau big-endian helper
         auto w16 = [&](uint16_t v) {
            uint8_t b[2] = {static_cast<uint8_t>(v >> 8), static_cast<uint8_t>(v & 0xFF)};
            fwrite(b, 1, 2, xaf);
         };
         w16(0xFFFF);        // FamilyWild
         w16(0);             // address length = 0
         w16(display_len);   // display number length
         fwrite(display_str, 1, display_len, xaf);
         w16(name_len);      // auth name length
         fwrite(auth_name, 1, name_len, xaf);
         w16(16);            // auth data length = 16 bytes
         fwrite(cookie_bin, 1, 16, xaf);
         fclose(xaf);
         shepherd_trace("setup_x11_forwarding: wrote Xau entry to %s for display :%d",
                        xauth_path, g_x11_display_num);
      } else {
         shepherd_trace("setup_x11_forwarding: cannot open %s for writing: %s",
                        xauth_path, strerror(errno));
      }
   } else {
      shepherd_trace("setup_x11_forwarding: cookie_hex parse failed");
   }

   // Append DISPLAY and XAUTHORITY to ./environment so the job picks them up.
   FILE *fp = fopen("./environment", "a");
   if (fp != nullptr) {
      fprintf(fp, "DISPLAY=:%d.0\n", g_x11_display_num);
      fprintf(fp, "XAUTHORITY=%s\n", xauth_path);
      fclose(fp);
      shepherd_trace("setup_x11_forwarding: appended DISPLAY=:%d.0 to ./environment",
                     g_x11_display_num);
   } else {
      shepherd_trace("setup_x11_forwarding: could not open ./environment for appending");
   }

   return true;
}

/****** pty_to_commlib() *******************************************************
*  NAME
*     pty_to_commlib() -- pty_to_commlib thread entry point and main loop
*
*  SYNOPSIS
*     void* pty_to_commlib(void *t_conf)
*
*  FUNCTION
*     Entry point and main loop of the pty_to_commlib thread.
*     Reads data from the pty and writes it to the commlib.
*
*  INPUTS
*     void *t_conf - pointer to cl_thread_settings_t struct of the thread
*
*  RESULT
*     void* - always nullptr
*
*  NOTES
*     MT-NOTE:
*
*  SEE ALSO
*******************************************************************************/
static void* pty_to_commlib(void *t_conf)
{
   int                  do_exit = 0;
   int                  fd_max = 0;
   int                  ret;
   fd_set               read_fds;
   struct timeval       timeout;
   char                 *stdout_buf = nullptr;
   char                 *stderr_buf = nullptr;
   int                  stdout_bytes = 0;
   int                  stderr_bytes = 0;
   bool                 b_select_timeout = false;
   DSTRING_STATIC(err_msg, MAX_STRING_SIZE);

   /* Report to thread lib that this thread starts up now */
   thread_func_startup(t_conf);

   /* Allocate working buffers, BUFSIZE = 64k */
   stdout_buf = sge_malloc(BUFSIZE);
   stderr_buf = sge_malloc(BUFSIZE);

   /* The main loop of this thread */
   while (do_exit == 0) {
      /* Fill fd_set for select */
      FD_ZERO(&read_fds);

      if (g_p_ijs_fds->pty_master != -1) {
         FD_SET(g_p_ijs_fds->pty_master, &read_fds);
      }
      if (g_p_ijs_fds->pipe_out != -1) {
         FD_SET(g_p_ijs_fds->pipe_out, &read_fds);
      }
      if (g_p_ijs_fds->pipe_err != -1) {
         FD_SET(g_p_ijs_fds->pipe_err, &read_fds);
      }
      fd_max = std::max(g_p_ijs_fds->pty_master, g_p_ijs_fds->pipe_out);
      fd_max = std::max(fd_max, g_p_ijs_fds->pipe_err);

      // Add X11 listen socket and any active X11 client connection fds
      int local_x11_client_fds[X11_MAX_CONNS];
      pthread_mutex_lock(&g_x11_mutex);
      int x11_listen = g_x11_listen_fd;
      memcpy(local_x11_client_fds, g_x11_client_fds, sizeof(local_x11_client_fds));
      pthread_mutex_unlock(&g_x11_mutex);
      if (x11_listen >= 0) {
         FD_SET(x11_listen, &read_fds);
         if (x11_listen > fd_max) {
            fd_max = x11_listen;
         }
      }
      for (int xi = 0; xi < X11_MAX_CONNS; xi++) {
         if (local_x11_client_fds[xi] >= 0) {
            FD_SET(local_x11_client_fds[xi], &read_fds);
            if (local_x11_client_fds[xi] > fd_max) {
               fd_max = local_x11_client_fds[xi];
            }
         }
      }

#ifdef EXTENSIVE_TRACING
      shepherd_trace("pty_to_commlib: g_p_ijs_fds->pty_master = %d, "
                     "g_p_ijs_fds->pipe_out = %d, "
                     "g_p_ijs_fds->pipe_err = %d, fd_max = %d",
                     g_p_ijs_fds->pty_master, g_p_ijs_fds->pipe_out,
                     g_p_ijs_fds->pipe_err, fd_max);
#endif
      /* Fill timeout struct for select */
      b_select_timeout = false;
      if (do_exit == 1) {
         /* If we know that we have to exit, don't wait for data,
          * just peek into the fds and read all data available from the buffers. */
         timeout.tv_sec  = 0;
         timeout.tv_usec = 0;
      } else {
         if ((stdout_bytes > 0 && stdout_bytes < 256)
             || cl_com_messages_in_send_queue(g_comm_handle) > 0) {
            /*
             * If we just received few data, wait another 10 milliseconds if new
             * data arrives to avoid sending data over the network in too small junks.
             * Also retry to send the messages that are still in the queue ASAP.
             */
            timeout.tv_sec  = 0;
            timeout.tv_usec = 10000;
         } else {
            /* Standard timeout is one second */
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
         }
      }
#ifdef EXTENSIVE_TRACING
      shepherd_trace("pty_to_commlib: doing select() with %d seconds, "
                     "%d usec timeout", timeout.tv_sec, timeout.tv_usec);
#endif
      /* Wait blocking for data from pty or pipe */
      errno = 0;
      ret = select(fd_max+1, &read_fds, nullptr, nullptr, &timeout);
      thread_testcancel(t_conf);
/* This is a workaround for Darwin, where thread_testcancel() doesn't work.
 * TODO: Find the reason why it doesn't work and remove the workaround
 */
      if (g_raised_event > 0) {
         do_exit = 1;
      }
#ifdef EXTENSIVE_TRACING
      shepherd_trace("pty_to_commlib: select() returned %d", ret);
#endif
      if (ret < 0) {
         /* select error */
#ifdef EXTENSIVE_TRACING
         shepherd_trace("pty_to_commlib: select() returned %d, reason: %d, %s", ret, errno, strerror(errno));
#endif
         if (errno == EINTR) {
            /* If we have a buffer, send it now (as we don't want to care about
             * how long we actually waited), then just continue, the top of the
             * loop will handle signals.
             * b_select_timeout tells the bottom of the loop to send the buffer.
             */

            /* ATTENTION: Don't call shepherd_trace() here, because on some
             * architectures it causes another EINTR, leading to a infinte loop.
             */
            b_select_timeout = true;
         } else {
            shepherd_trace("pty_to_commlib: select() error -> exiting");
            do_exit = 1;
         }
      } else if (ret == 0) {
         /* timeout, if we have a buffer, send it now */
#ifdef EXTENSIVE_TRACING
         shepherd_trace("pty_to_commlib: select() timeout");
#endif
         b_select_timeout = true;
      } else {
         /* at least one fd is ready to read from */
         ret = 0;
         /* now we can be sure that our child has started the job,
          * we can close the pipe_to_child now
          */
         if (g_p_ijs_fds->pty_master != -1 && FD_ISSET(g_p_ijs_fds->pty_master, &read_fds)) {
#ifdef EXTENSIVE_TRACING
            shepherd_trace("pty_to_commlib: reading from ptym");
#endif
            ret = append_to_buf(g_p_ijs_fds->pty_master, stdout_buf, &stdout_bytes);
#ifdef EXTENSIVE_TRACING
            trace_buf(stdout_buf, ret, "pty_to_commlib: appended %d bytes, stdout_buf = ",
                      ret, stdout_buf);
#endif
         }
         if (ret >= 0 && g_p_ijs_fds->pipe_out != -1
             && FD_ISSET(g_p_ijs_fds->pipe_out, &read_fds)) {
#ifdef EXTENSIVE_TRACING
            shepherd_trace("pty_to_commlib: reading from pipe_out");
#endif
            ret = append_to_buf(g_p_ijs_fds->pipe_out, stdout_buf, &stdout_bytes);

#ifdef EXTENSIVE_TRACING
            trace_buf(stdout_buf, ret, "pty_to_commlib: appended %d bytes, stdout_buf = ",
                      ret, stdout_buf);
#endif
         }
         if (ret < 0) {
            shepherd_trace("pty_to_commlib: STDOUT was closed. Our child seems to have exited -> exiting");
            do_exit = 1;
         }
         if (g_p_ijs_fds->pipe_err != -1
             && FD_ISSET(g_p_ijs_fds->pipe_err, &read_fds)) {
#ifdef EXTENSIVE_TRACING
            shepherd_trace("pty_to_commlib: reading from pipe_err");
#endif
            ret = append_to_buf(g_p_ijs_fds->pipe_err, stderr_buf, &stderr_bytes);
#ifdef EXTENSIVE_TRACING
            trace_buf(stderr_buf, ret, "pty_to_commlib: appended %d bytes, stderr_buf = ",
                      ret, stderr_buf);
#endif
         }
         if (ret < 0) {
            /* A fd was closed, likely our child has exited, we can exit, too. */
            shepherd_trace("pty_to_commlib: STDERR was closed. Our child seems to have exited -> exiting");
            do_exit = 1;
         } else if (ret > 0) {
            if (g_p_ijs_fds->pipe_to_child != -1) {
               shepherd_trace("pty_to_commlib: closing pipe to child");
               close(g_p_ijs_fds->pipe_to_child);
               g_p_ijs_fds->pipe_to_child = -1;
            }
         }

         // Handle new X11 connections from the job (accept on listen socket)
         if (x11_listen >= 0 && FD_ISSET(x11_listen, &read_fds)) {
            int new_fd = accept(x11_listen, nullptr, nullptr);
            if (new_fd >= 0) {
               // Find an unused conn_id (round-robin search)
               int conn_id = -1;
               pthread_mutex_lock(&g_x11_mutex);
               for (int attempt = 0; attempt < X11_MAX_CONNS; attempt++) {
                  int cid = (g_x11_next_conn_id + attempt) % X11_MAX_CONNS;
                  if (g_x11_client_fds[cid] < 0) {
                     g_x11_client_fds[cid] = new_fd;
                     g_x11_next_conn_id = (cid + 1) % X11_MAX_CONNS;
                     conn_id = cid;
                     break;
                  }
               }
               pthread_mutex_unlock(&g_x11_mutex);
               if (conn_id >= 0) {
                  // Tell client about the new connection so it can connect to the real X server
                  char open_msg[2];
                  open_msg[0] = static_cast<char>((conn_id >> 8) & 0xFF);
                  open_msg[1] = static_cast<char>(conn_id & 0xFF);
                  DSTRING_STATIC(x11_err, MAX_STRING_SIZE);
                  comm_write_message(g_comm_handle, g_hostname, COMM_SERVER, 1,
                                     (unsigned char *)open_msg, 2, X11_OPEN_MSG, &x11_err);
                  shepherd_trace("pty_to_commlib: X11 connection accepted, conn_id %d fd %d", conn_id, new_fd);
               } else {
                  shepherd_trace("pty_to_commlib: X11_MAX_CONNS reached, refusing X11 connection");
                  close(new_fd);
               }
            }
         }

         // Relay data from X11 job clients to the qrsh client (→ real X server)
         static char x11_buf[2 + BUFSIZE];
         for (int xi = 0; xi < X11_MAX_CONNS; xi++) {
            if (local_x11_client_fds[xi] >= 0 && FD_ISSET(local_x11_client_fds[xi], &read_fds)) {
               int nread = read(local_x11_client_fds[xi], x11_buf + 2, BUFSIZE - 1);
               if (nread <= 0) {
                  shepherd_trace("pty_to_commlib: X11 client conn %d closed by job", xi);
                  x11_buf[0] = static_cast<char>((xi >> 8) & 0xFF);
                  x11_buf[1] = static_cast<char>(xi & 0xFF);
                  DSTRING_STATIC(x11_err, MAX_STRING_SIZE);
                  comm_write_message(g_comm_handle, g_hostname, COMM_SERVER, 1,
                                     (unsigned char *)x11_buf, 2, X11_CLOSE_MSG, &x11_err);
                  pthread_mutex_lock(&g_x11_mutex);
                  close(g_x11_client_fds[xi]);
                  g_x11_client_fds[xi] = -1;
                  pthread_mutex_unlock(&g_x11_mutex);
               } else {
                  x11_buf[0] = static_cast<char>((xi >> 8) & 0xFF);
                  x11_buf[1] = static_cast<char>(xi & 0xFF);
                  DSTRING_STATIC(x11_err, MAX_STRING_SIZE);
                  comm_write_message(g_comm_handle, g_hostname, COMM_SERVER, 1,
                                     (unsigned char *)x11_buf, static_cast<unsigned long>(nread + 2),
                                     X11_DATA_MSG, &x11_err);
               }
            }
         }

      } // At least one file handle was ready to read.

      /* Always send stderr buffer immediately */
      if (stderr_bytes != 0) {
         ret = send_buf(stderr_buf, stderr_bytes, STDERR_DATA_MSG);
#ifdef EXTENSIVE_TRACING
         shepherd_trace("pty_to_commlib: send %d bytes of stderr data", stderr_bytes);
#endif
         if (ret == 0) {
            stderr_bytes = 0;
         } else if (g_ijs_reconnect_timeout > 0) {
            // CS-2144: reconnect is enabled; the failure is most likely commlib_to_pty's
            // counterpart detecting a client disconnect.  Preserve the buffered output and
            // wait — commlib_to_pty's polling loop may swap g_comm_handle and the next
            // send_buf will go through on the new connection.  If the grace period expires
            // without a reconnect, the job will be SIGKILL'd and the PTY will close, which
            // ends this thread via the normal STDOUT/STDERR closed path.
            shepherd_trace("pty_to_commlib: stderr send_buf failed in reconnect window; preserving %d bytes", stderr_bytes);
         } else {
            shepherd_trace("pty_to_commlib: send_buf() returned %d " "-> exiting", ret);
            do_exit = 1;
         }
      }
      /*
       * Send stdout_buf if there is enough data in it
       * OR if there was a select timeout (don't wait too long to send data)
       * OR if we will exit the loop now and there is data in the stdout_buf
       */
      if (stdout_bytes >= 256 ||
          (b_select_timeout && stdout_bytes > 0) ||
          (do_exit == 1 && stdout_bytes > 0)) {
#ifdef EXTENSIVE_TRACING
         shepherd_trace("pty_to_commlib: sending %d bytes of stdout data", stdout_bytes);
#endif
         ret = send_buf(stdout_buf, stdout_bytes, STDOUT_DATA_MSG);
         if (ret == 0) {
            stdout_bytes = 0;
         } else if (g_ijs_reconnect_timeout > 0) {
            // CS-2144: see matching comment in the stderr branch above — preserve the
            // buffered output so the next send goes through after a successful reconnect.
            shepherd_trace("pty_to_commlib: stdout send_buf failed in reconnect window; preserving %d bytes", stdout_bytes);
         } else {
            stdout_bytes = 0;
            shepherd_trace("pty_to_commlib: send_buf() failed -> exiting");
            do_exit = 1;
         }
         int flush_ret = comm_wait_for_all_messages_sent(g_comm_handle, &err_msg);
         if (flush_ret > 0) {
            // comm_flush_write_messages reported an error - log to trace file
            shepherd_trace("pty_to_commlib: comm_flush_write_messages() returned error %d: %s",
                           flush_ret, sge_dstring_get_string(&err_msg));

         } else if (flush_ret < 0) {
#ifdef EXTENSIVE_TRACING
            shepherd_trace("pty_to_commlib: comm_flush_write_messages() did %d retries", -flush_ret);
#endif
         } else {
#ifdef EXTENSIVE_TRACING
            shepherd_trace("pty_to_commlib: comm_flush_write_messages() succeeded without retries");
#endif
         }
      }
   }

#ifdef EXTENSIVE_TRACING
   shepherd_trace("pty_to_commlib: shutting down thread");
#endif
   sge_free(&stdout_buf);
   sge_free(&stderr_buf);

   // Close X11 listen socket and all X11 client connections
   pthread_mutex_lock(&g_x11_mutex);
   if (g_x11_listen_fd >= 0) {
      close(g_x11_listen_fd);
      g_x11_listen_fd = -1;
      if (g_x11_socket_path[0] != '\0') {
         unlink(g_x11_socket_path);
      }
   }
   for (int xi = 0; xi < X11_MAX_CONNS; xi++) {
      if (g_x11_client_fds[xi] >= 0) {
         close(g_x11_client_fds[xi]);
         g_x11_client_fds[xi] = -1;
      }
   }
   pthread_mutex_unlock(&g_x11_mutex);

   thread_func_cleanup(t_conf);

/* TODO: This could cause race conditions in the main thread, replace with pthread_condition */
/* HP: How can this cause race conditions? */
#ifdef EXTENSIVE_TRACING
   shepherd_trace("pty_to_commlib: raising event for main thread");
#endif
   g_raised_event = 2;
   thread_trigger_event(&g_thread_main);

#ifdef EXTENSIVE_TRACING
   shepherd_trace("pty_to_commlib: leaving pty_to_commlib thread");
#endif
   return nullptr;
}

/**
 * @brief Run the IJS reconnect grace period: SIGSTOP the job, poll for reconnect.info,
 * attempt the handshake when it appears, swap g_comm_handle on success.
 *
 * Called from commlib_to_pty after the inner receive loop detected a client disconnect.
 * The polling loop runs at 1-second granularity for up to g_ijs_reconnect_timeout
 * seconds.  When reconnect.info appears the helper parses {host, port, token}, opens a
 * new commlib client connection to {host, port}, sends RECONNECT_REQUEST_MSG carrying
 * the token, and waits up to 5 seconds on the new handle for RECONNECT_ACCEPT_MSG.
 *
 * On a successful handshake:
 *   - the old g_comm_handle is shut down (it has been dead since the disconnect)
 *   - g_comm_handle and g_hostname are swapped to the new client
 *   - the job is SIGCONT'd (also thaws the cgroup that SIGSTOP froze)
 *   - returns true; the caller re-enters the inner receive loop on the new handle.
 *
 * On expiry or any handshake failure: returns false; the caller falls through to the
 * normal SIGCONT-then-SIGKILL path.
 */
static bool attempt_reconnect_grace_period() {
   shepherd_trace("commlib_to_pty: client disconnected, suspending job and polling for reconnect (timeout=%d s)",
                  g_ijs_reconnect_timeout);
   shepherd_signal_job(g_job_pid, SIGSTOP);

   const char *reconnect_info_path = "reconnect.info";
   const time_t deadline = time(nullptr) + g_ijs_reconnect_timeout;
   DSTRING_STATIC(err_msg, MAX_STRING_SIZE);

   while (time(nullptr) < deadline) {
      sleep(1);

      struct stat st{};
      if (stat(reconnect_info_path, &st) != 0) {
         continue;
      }

      shepherd_trace("commlib_to_pty: reconnect.info appeared, parsing");

      char client_host[256] = {0};
      int  client_port      = 0;
      char token[256]       = {0};
      if (parse_reconnect_info(reconnect_info_path, client_host, sizeof(client_host),
                               &client_port, token, sizeof(token)) != 0) {
         shepherd_trace("commlib_to_pty: reconnect.info malformed; unlinking and continuing to poll");
         unlink(reconnect_info_path);
         continue;
      }

      shepherd_trace("commlib_to_pty: attempting reconnect to %s:%d", client_host, client_port);

      COMM_HANDLE *new_handle = nullptr;
      int orc = comm_open_connection(false, g_ijs_framework, COMM_CLIENT, client_port,
                                     COMM_SERVER, client_host, g_job_owner,
                                     &new_handle, &err_msg);
      if (orc != COMM_RETVAL_OK) {
         shepherd_trace("commlib_to_pty: reconnect comm_open_connection failed: %s",
                        sge_dstring_get_string(&err_msg));
         unlink(reconnect_info_path);
         continue;
      }

      int wrc = (int)comm_write_message(new_handle, client_host, COMM_SERVER, 1,
                                        (unsigned char *)token, strlen(token) + 1,
                                        RECONNECT_REQUEST_MSG, &err_msg);
      if (wrc <= 0) {
         shepherd_trace("commlib_to_pty: reconnect comm_write_message failed: %s",
                        sge_dstring_get_string(&err_msg));
         comm_shutdown_connection(new_handle, COMM_SERVER, client_host, &err_msg);
         unlink(reconnect_info_path);
         continue;
      }

      shepherd_trace("commlib_to_pty: RECONNECT_REQUEST_MSG sent, awaiting ACK (5 s)");

      cl_com_set_synchron_receive_timeout(new_handle, 5);
      recv_message_t reply{};
      reply.cl_message = nullptr;
      reply.data       = nullptr;
      int rrc = comm_recv_message(new_handle, &reply, &err_msg);

      bool accepted = (rrc == COMM_RETVAL_OK && reply.type == RECONNECT_ACCEPT_MSG);
      const int reply_type = (rrc == COMM_RETVAL_OK) ? (int)reply.type : -1;
      comm_free_message(&reply, &err_msg);

      if (!accepted) {
         shepherd_trace("commlib_to_pty: handshake failed (rc=%d, msg_type=%d); closing new handle",
                        rrc, reply_type);
         comm_shutdown_connection(new_handle, COMM_SERVER, client_host, &err_msg);
         unlink(reconnect_info_path);
         continue;
      }

      shepherd_trace("commlib_to_pty: RECONNECT_ACCEPT_MSG received from %s:%d, swapping handle and resuming job",
                     client_host, client_port);
      // Tear down OLD g_comm_handle.  The old client is gone; this connection has been
      // dead since the disconnect that started the grace period.
      DSTRING_STATIC(old_err, MAX_STRING_SIZE);
      comm_shutdown_connection(g_comm_handle, COMM_SERVER, g_hostname, &old_err);

      g_comm_handle = new_handle;
      sge_free(&g_hostname);
      g_hostname = strdup(client_host);
      cl_com_set_synchron_receive_timeout(g_comm_handle, 1);

      // SIGCONT thaws the systemd cgroup and resumes the job.
      shepherd_signal_job(g_job_pid, SIGCONT);

      unlink(reconnect_info_path);
      return true;
   }

   shepherd_trace("commlib_to_pty: reconnect grace period expired");
   // SIGCONT to thaw the frozen cgroup so the caller's SIGKILL actually takes effect.
   shepherd_signal_job(g_job_pid, SIGCONT);
   return false;
}

/**
 * @brief Entry point and main loop of the commlib_to_pty thread.
 *
 * Reads data from the commlib and writes it to the pty (stdin of the job).
 * In addition to standard control messages (SETTINGS_CTRL_MSG, SUSPEND_CTRL_MSG,
 * WINDOW_SIZE_CTRL_MSG), handles X11 forwarding: X11_AUTH_MSG triggers
 * setup_x11_forwarding() to create the proxy display before the child is unblocked;
 * X11_DATA_MSG relays X protocol bytes from the client's X server to the job's X
 * client socket; X11_CLOSE_MSG tears down an X11 channel.
 *
 * Reconnect (CS-2143/CS-2144): the receive loop is wrapped in an outer loop so that
 * on disconnect the function can enter attempt_reconnect_grace_period() and, on
 * successful reconnect, re-enter the receive loop on the swapped g_comm_handle.
 *
 * @param t_conf Pointer to cl_thread_settings_t struct of the thread.
 * @return Always nullptr.
 * @note MT-NOTE: not MT-safe (modifies shared shepherd state).
 */
static void* commlib_to_pty(void *t_conf)
{
   recv_message_t       recv_mess;
   int                  b_was_connected = 0;
   bool                 b_sent_to_child = false;
   int                  ret;
   int                  do_exit = 0;
   int                  fd_write = -1;
   DSTRING_STATIC(err_msg, MAX_STRING_SIZE);

   /* report to thread lib that thread starts up now */
   thread_func_startup(t_conf);

   if (g_p_ijs_fds->pty_master != -1) {
      fd_write = g_p_ijs_fds->pty_master;
   } else if (g_p_ijs_fds->pipe_in != -1) {
      fd_write = g_p_ijs_fds->pipe_in;
   } else {
      do_exit = 1;
      shepherd_trace("commlib_to_pty: no valid handle for stdin available. Exiting!");
   }

   // Outer loop: each iteration is one "lifetime" of the receive loop on a given
   // commlib handle.  After a client disconnect we may enter the reconnect grace
   // period; if a new client successfully completes the handshake, g_comm_handle is
   // swapped and we re-enter the inner loop on the new connection.
   while (true) {
      // Set timeout for synchronous receiving of messages.  Re-applied each outer
      // iteration because g_comm_handle may have been swapped by the reconnect helper.
      cl_com_set_synchron_receive_timeout(g_comm_handle, 1);

      do_exit = 0;
   while (do_exit == 0) {
      // We wait synchronously (blocking) for a message from commlib, timeout is 1s.
      recv_mess.cl_message = nullptr;
      recv_mess.data       = nullptr;
      sge_dstring_clear(&err_msg);

#ifdef EXTENSIVE_TRACING
      shepherd_trace("commlib_to_pty: calling comm_recv_message() synchronously, timeout %d",
                     g_comm_handle->synchron_receive_timeout);
#endif

      ret = comm_recv_message(g_comm_handle, &recv_mess, &err_msg);

#ifdef EXTENSIVE_TRACING
      shepherd_trace("commlib_to_pty: comm_recv_message() returned %d, err_msg: %s",
                     ret, sge_dstring_get_string(&err_msg));
#endif

      /*
       * Check if the thread was canceled. Exit the thread if it was.
       * It shouldn't be necessary to do the check here, as the cancel state
       * of the thread is 1, i.e., the thread may be canceled at any time,
       * but this doesn't work on some architectures (Darwin, older Solaris).
       */
      thread_testcancel(t_conf);
      if (g_raised_event > 0) {
         do_exit = 1;
         continue;
      }

      if (ret != COMM_RETVAL_OK) {
         /* handle error cases */
         switch (ret) {
            case COMM_NO_SELECT_DESCRIPTORS:
               /*
                * As long as we're not connected, this return value is expected.
                * If we were already connected, it means the connection was closed.
                */
               if (b_was_connected == 1) {
                  shepherd_trace("commlib_to_pty: was connected, but lost connection -> exiting");
                  do_exit = 1;
               }
               break;
            case COMM_CONNECTION_NOT_FOUND:
               if (b_was_connected == 0) {
                  shepherd_trace("commlib_to_pty: our server is not running -> exiting. Error: %s", sge_dstring_get_string(&err_msg));
                  /*
                   * On some architectures (e.g. Darwin), the child doesn't recognize
                   * when the pipe gets closed, so we have to send the information
                   * that the parent will exit soon to the child.
                   * "noshell" may be 0 or 1, everything else indicates an error.
                   */
                  if (!b_sent_to_child) {
                     if (write(g_p_ijs_fds->pipe_to_child, "noshell = 9", 11) != 11) {
                        shepherd_trace("commlib_to_pty: error in communicating with child -> exiting");
                     } else {
                        b_sent_to_child = true;
                     }
                  }
               } else {
                  shepherd_trace("commlib_to_pty: was connected and still have "
                                 "selectors, but lost connection -> exiting. Error: %s", sge_dstring_get_string(&err_msg));
               }
               do_exit = 1;
               break;
            case COMM_INVALID_PARAMETER:
               shepherd_trace("commlib_to_pty: communication handle or "
                        "message buffer is invalid -> exiting");
               do_exit = 1;
               break;
            case COMM_CANT_TRIGGER:
               shepherd_trace("commlib_to_pty: can't trigger communication, likely the "
                              "communication was shut down by other thread -> exiting");
               shepherd_trace("commlib_to_pty: err_msg: %s",
                              sge_dstring_get_string(&err_msg));
               do_exit = 1;
               break;
            case COMM_CANT_RECEIVE_MESSAGE:
               if (check_client_alive(g_comm_handle,
                                      COMM_SERVER,
                                      g_hostname,
                                      &err_msg) != COMM_RETVAL_OK) {
                  shepherd_trace("commlib_to_pty: not connected any more -> exiting.");
                  do_exit = 1;
               } else {
#ifdef EXTENSIVE_TRACING
                  shepherd_trace("commlib_to_pty: can't receive message, reason: %s "
                                 "-> trying again",
                                 sge_dstring_get_string(&err_msg));
#endif
               }
               b_was_connected = 1;
               break;
            case COMM_GOT_TIMEOUT:
#ifdef EXTENSIVE_TRACING
               shepherd_trace("commlib_to_pty: got timeout -> trying again");
#endif
               b_was_connected = 1;
               break;
            case COMM_SELECT_INTERRUPT:
               /* Don't do tracing here */
               /* shepherd_trace("commlib_to_pty: interrupted select"); */
               b_was_connected = 1;
               break;
            case COMM_NO_MESSAGE_AVAILABLE:
#ifdef EXTENSIVE_TRACING
               shepherd_trace("commlib_to_pty: didn't receive a message within 1 s "
                  "timeout -> trying again");
#endif
               b_was_connected = 1;
               break;
            default:
               /* Unknown error, just try again */
#ifdef EXTENSIVE_TRACING
               shepherd_trace("commlib_to_pty: comm_recv_message() returned %d -> "
                              "trying again", ret);
#endif
               b_was_connected = 1;
               break;
         }
      } else {  /* if (ret == COMM_RETVAL_OK) */
         /* We received a message, 'parse' it */
         switch (recv_mess.type) {
            case STDIN_DATA_MSG:
               /* data message, write data to stdin of child */
#ifdef EXTENSIVE_TRACING
               shepherd_trace("commlib_to_pty: received data message");
               shepherd_trace("commlib_to_pty: writing data to stdin of child, "
                              "length = %d", recv_mess.cl_message->message_length-1);
#endif

               if (sge_writenbytes(fd_write,
                          recv_mess.data,
                          (int)(recv_mess.cl_message->message_length-1))
                       != (int)(recv_mess.cl_message->message_length-1)) {
                  shepherd_trace("commlib_to_pty: error writing to stdin of "
                                 "child: %d, %s", errno, strerror(errno));
               }
               b_was_connected = 1;
               break;

            case STDIN_CLOSE_MSG:
               /* If we receive a STDIN_CLOSE_MSG we have to close the filedescriptor!
                * This is needed for GE-3580
                */
               shepherd_trace("commlib_to_pty: received stdin_close message");
               SGE_CLOSE(fd_write);
               b_was_connected = 1;
               break;

            case SUSPEND_CTRL_MSG:
               shepherd_trace("commlib_to_pty: received suspend message, "
                   "suspend the process");
               shepherd_signal_job(g_job_pid, SIGSTOP);
               break;

            case UNSUSPEND_CTRL_MSG:
               shepherd_trace("commlib_to_pty: received unsuspend message, "
                   "awake the process");
               shepherd_signal_job(g_job_pid, SIGCONT);
               break;

            case WINDOW_SIZE_CTRL_MSG:
               /* control message, set size of pty */
               shepherd_trace("commlib_to_pty: received window size message, "
                  "changing window size");
               ioctl(fd_write, TIOCSWINSZ, &(recv_mess.ws));
               b_was_connected = 1;
               break;
            case SETTINGS_CTRL_MSG:
               /* control message */
               shepherd_trace("commlib_to_pty: received settings message");
               /* Forward the settings to the child process.
                * This is also tells the child process that it can start
                * the job 'in' the pty now.
                */
               shepherd_trace("commlib_to_pty: writing to child %d bytes: %s",
                              strlen(recv_mess.data), recv_mess.data);
               if (write(g_p_ijs_fds->pipe_to_child, recv_mess.data,
                         strlen(recv_mess.data)) != (ssize_t)strlen(recv_mess.data)) {
                  shepherd_trace("commlib_to_pty: error in communicating "
                     "with child -> exiting");
                  do_exit = 1;
               } else {
                  b_sent_to_child = true;
               }
               b_was_connected = 1;
               break;
            case X11_AUTH_MSG: {
               // Client sent the real MIT-MAGIC-COOKIE-1; set up X11 proxy display.
               // Must arrive BEFORE SETTINGS_CTRL_MSG so DISPLAY is in ./environment
               // when the child reads it (TCP ordering guarantees this).
               if (recv_mess.cl_message->message_length < 2) {
                  shepherd_trace("commlib_to_pty: X11_AUTH_MSG too short");
                  break;
               }
               char cookie_hex[33];
               unsigned long cookie_len = std::min(recv_mess.cl_message->message_length - 1,
                                                   static_cast<unsigned long>(32));
               memcpy(cookie_hex, recv_mess.data, cookie_len);
               cookie_hex[cookie_len] = '\0';
               shepherd_trace("commlib_to_pty: received X11_AUTH_MSG, setting up X11 forwarding");
               if (!setup_x11_forwarding(cookie_hex)) {
                  shepherd_trace("commlib_to_pty: X11 forwarding setup failed");
               }
               b_was_connected = 1;
               break;
            }
            case X11_DATA_MSG: {
               // Data from client's real X server toward the job's X client connection.
               if (recv_mess.cl_message->message_length < 3) {
                  shepherd_trace("commlib_to_pty: X11_DATA_MSG too short");
                  break;
               }
               int conn_id = (static_cast<unsigned char>(recv_mess.data[0]) << 8)
                           | static_cast<unsigned char>(recv_mess.data[1]);
               if (conn_id < 0 || conn_id >= X11_MAX_CONNS) {
                  shepherd_trace("commlib_to_pty: X11_DATA_MSG conn_id %d out of range", conn_id);
                  break;
               }
               pthread_mutex_lock(&g_x11_mutex);
               int x11_cfd = g_x11_client_fds[conn_id];
               pthread_mutex_unlock(&g_x11_mutex);
               if (x11_cfd >= 0) {
                  int data_len = static_cast<int>(recv_mess.cl_message->message_length) - 3;
                  sge_writenbytes(x11_cfd, recv_mess.data + 2, data_len);
               }
               b_was_connected = 1;
               break;
            }
            case X11_CLOSE_MSG: {
               // Client closed its connection to the real X server for this conn_id.
               if (recv_mess.cl_message->message_length < 3) {
                  shepherd_trace("commlib_to_pty: X11_CLOSE_MSG too short");
                  break;
               }
               int conn_id = (static_cast<unsigned char>(recv_mess.data[0]) << 8)
                           | static_cast<unsigned char>(recv_mess.data[1]);
               if (conn_id < 0 || conn_id >= X11_MAX_CONNS) {
                  shepherd_trace("commlib_to_pty: X11_CLOSE_MSG conn_id %d out of range", conn_id);
                  break;
               }
               shepherd_trace("commlib_to_pty: X11_CLOSE_MSG conn_id %d", conn_id);
               pthread_mutex_lock(&g_x11_mutex);
               if (g_x11_client_fds[conn_id] >= 0) {
                  close(g_x11_client_fds[conn_id]);
                  g_x11_client_fds[conn_id] = -1;
               }
               pthread_mutex_unlock(&g_x11_mutex);
               b_was_connected = 1;
               break;
            }
            case KEEPALIVE_MSG: {
               shepherd_trace("commlib_to_pty: received KEEPALIVE, sending ACK");
               unsigned char ack_buf[1] = {0};
               comm_write_message(g_comm_handle, g_hostname, COMM_SERVER, 1,
                                  ack_buf, 1, KEEPALIVE_ACK_MSG, &err_msg);
               b_was_connected = 1;
               break;
            }
            default:
               shepherd_trace("commlib_to_pty: received unknown message");
               break;
         }
      }
      comm_free_message(&recv_mess, &err_msg);
   }
   /*
    * When we get here, the inner receive loop exited.  Either the client disconnected
    * or the thread was asked to shut down.  If reconnect is enabled and we just lost a
    * client, run the grace-period helper; on success it swaps g_comm_handle and we
    * re-enter the outer loop, otherwise we fall through to SIGKILL.
    */
   if (g_ijs_reconnect_timeout > 0 && attempt_reconnect_grace_period()) {
      b_was_connected = 1;   // handshake completed; treat the new connection as live
      continue;              // back to the outer loop -> re-enter receive loop on new handle
   }
   break;
   }   // end of outer while(true) reconnect-capable loop

   /*
    * TODO: Use SIGINT if qrsh client was quit with Ctrl-C
    */
   shepherd_signal_job(g_job_pid, SIGKILL);

#ifdef EXTENSIVE_TRACING
   shepherd_trace("commlib_to_pty: leaving commlib_to_pty function");
#endif
   thread_func_cleanup(t_conf);
/* TODO: pthread_condition, see other thread*/
#ifdef EXTENSIVE_TRACING
   shepherd_trace("commlib_to_pty: raising event for main thread");
#endif
   g_raised_event = 3;
   thread_trigger_event(&g_thread_main);
#ifdef EXTENSIVE_TRACING
   shepherd_trace("commlib_to_pty: leaving commlib_to_pty thread");
#endif
   return nullptr;
}

int
parent_loop(int job_pid, const char *childname, int timeout, ckpt_info_t *p_ckpt_info,
            ijs_fds_t *p_ijs_fds, const char *job_owner, const char *remote_host,
            int remote_port, cl_framework_t communication_framework, int *exit_status, struct rusage *rusage,
            dstring *err_msg)
{
   int               ret;
   THREAD_LIB_HANDLE *thread_lib_handle     = nullptr;
   THREAD_HANDLE     *thread_pty_to_commlib = nullptr;
   THREAD_HANDLE     *thread_commlib_to_pty = nullptr;
   cl_raw_list_t     *cl_com_log_list = nullptr;

   shepherd_trace("parent: starting parent loop with remote_host = %s, "
                  "remote_port = %d, job_owner = %s, fd_pty_master = %d, "
                  "fd_pipe_in = %d, fd_pipe_out = %d, "
                  "fd_pipe_err = %d, fd_pipe_to_child = %d",
                  remote_host, remote_port, job_owner, p_ijs_fds->pty_master,
                  p_ijs_fds->pipe_in, p_ijs_fds->pipe_out, p_ijs_fds->pipe_err,
                  p_ijs_fds->pipe_to_child);

   g_hostname  = strdup(remote_host);
   g_p_ijs_fds = p_ijs_fds;
   g_job_pid   = job_pid;
   g_ijs_framework = communication_framework;
   g_job_owner = strdup(job_owner);

   // Grace period before SIGKILL on unexpected client disconnect.  When > 0 the shepherd
   // SIGSTOPs the job and polls for a reconnect.info file written by execd up to this
   // many seconds before killing.
   {
      const char *v = get_conf_val("ijs_reconnect_timeout");
      g_ijs_reconnect_timeout = (v != nullptr) ? atoi(v) : 0;
   }

   // Initialize X11 forwarding state; setup happens lazily when X11_AUTH_MSG arrives.
   g_x11_listen_fd   = -1;
   g_x11_display_num = -1;
   g_x11_next_conn_id = 0;
   g_x11_socket_path[0] = '\0';
   for (int xi = 0; xi < X11_MAX_CONNS; xi++) {
      g_x11_client_fds[xi] = -1;
   }

   /*
    * Initialize err_msg, so it's never nullptr.
    */
   sge_dstring_sprintf(err_msg, "");


#ifdef EXTENSIVE_TRACING
   ret = comm_init_lib(err_msg, shepherd_log_list_flush_list);
#else
   ret = comm_init_lib(err_msg);
#endif
   if (ret != COMM_RETVAL_OK) {
      shepherd_trace("parent: init comm lib failed: %d", ret);
      return 1;
   }

   /*
    * Setup thread list.
    */
   ret = thread_init_lib(&thread_lib_handle);
   if (ret != CL_RETVAL_OK) {
      shepherd_trace("parent: init thread lib thread failed: %d", ret);
      return 1;
   }

   /*
    * Get log list of communication before a connection is opened.
    */
   cl_com_log_list = cl_com_get_log_list();

   /*
    * Register this main thread at the thread library, so it can
    * be triggered and create two worker threads.
    */
   ret = register_thread(cl_com_log_list, &g_thread_main, "main thread");
   if (ret != CL_RETVAL_OK) {
      shepherd_trace("parent: registering main thread failed: %d", ret);
      return 1;
   }

   /*
    * Open the connection port so we can connect to our server
    */
   shepherd_trace("parent: opening connection to qrsh/qlogin client");
   ret = comm_open_connection(false, communication_framework, COMM_CLIENT, remote_port,
                              COMM_SERVER, g_hostname, job_owner,
                              &g_comm_handle, err_msg);
   if (ret != COMM_RETVAL_OK) {
      shepherd_trace("parent: can't open commlib stream, err_msg = %s",
                     sge_dstring_get_string(err_msg));
      return 1;
   } else {
      shepherd_trace("parent: successfully opened connection to qrsh/qlogin client on host %s", g_hostname);
   }

   /*
    * register at qrsh/qlogin client, which is the server of the communication.
    * The answer of the server (a WINDOW_SIZE_CTRL_MSG) will be handled in the
    * commlib_to_pty thread.
    */
   shepherd_trace("parent: sending REGISTER_CTRL_MSG to qrsh/qlogin client");
   ret = (int)comm_write_message(g_comm_handle, g_hostname, COMM_SERVER, 1,
                      (unsigned char*)" ", 1, REGISTER_CTRL_MSG, err_msg);
   if (ret == 0) {
      /* No bytes written - error */
      shepherd_trace("parent: can't send REGISTER_CTRL_MSG, comm_write_message() "
                     "returned: %s", sge_dstring_get_string(err_msg));
   /* Don't exit here, the error handling is done in the commlib_to_tty-thread */
   /* Most likely, the qrsh client is not running, so it's not the shepherds fault,
    * we shouldn't return 1 (which leads to a shepherd_exit which sets the whole
    * queue in error!).
    */
   /*   return 1;*/
   }
#ifdef EXTENSIVE_TRACING
   else {
      shepherd_trace("parent: Sent %d bytes to qrsh client", ret);
   }
#endif

   {
      sigset_t old_sigmask;
      sge_thread_block_all_signals(&old_sigmask);

      shepherd_trace("parent: creating pty_to_commlib thread");
      ret = create_thread(thread_lib_handle,
                          &thread_pty_to_commlib,
                          cl_com_log_list,
                          "pty_to_commlib thread",
                          2,
                          pty_to_commlib);
      if (ret != CL_RETVAL_OK) {
         shepherd_trace("parent: creating pty_to_commlib thread failed: %d", ret);
      }

      shepherd_trace("parent: creating commlib_to_pty thread");
      ret = create_thread(thread_lib_handle,
                          &thread_commlib_to_pty,
                          cl_com_log_list,
                          "commlib_to_pty thread",
                          3,
                          commlib_to_pty);

      pthread_sigmask(SIG_SETMASK, &old_sigmask, nullptr);
   }

   if (ret != CL_RETVAL_OK) {
      shepherd_trace("parent: creating commlib_to_pty thread failed: %d", ret);
   }

   /* From here on, the two worker threads are doing all the work.
    * This main thread is just waiting until one of the to worker threads
    * wants to exit and sends an event to the main thread.
    * A worker thread wants to exit when either the communciation to
    * the server was shut down or the user application (likely the shell)
    * exited.
    * On some architectures, this thread awakens from the wait whenever
    * a signal arrives. Therefore we have to check if it was a signal
    * or a event that awoke this thread.
    */
   shepherd_trace("parent: created both worker threads, now waiting for jobs end");

   *exit_status = wait_my_child(job_pid, childname, timeout, p_ckpt_info, rusage, -1, -1);
   alarm(0);

   shepherd_trace("parent: wait_my_child returned exit_status = %d", *exit_status);
   shepherd_trace("parent:            rusage.ru_stime.tv_sec  = %d", rusage->ru_stime.tv_sec);
   shepherd_trace("parent:            rusage.ru_stime.tv_usec = %d", rusage->ru_stime.tv_usec);
   shepherd_trace("parent:            rusage.ru_utime.tv_sec  = %d", rusage->ru_utime.tv_sec);
   shepherd_trace("parent:            rusage.ru_utime.tv_usec = %d", rusage->ru_utime.tv_usec);

   /*
    * We are sure the job exited when we get here, but there could still be
    * some output in the buffers, so wait for the communication threads
    * to give them time to read, transmit and flush the buffers.
    */
   while (g_raised_event == 0) {
      ret = thread_wait_for_event(&g_thread_main, 0, 0);
   }
   shepherd_trace("parent: received event %d, g_raised_event = %d", ret, g_raised_event);

   /*
    * One of the worker threads sent an event, so shut down both threads now.
    * Shutdown the threads thread_pty_to_commlib and thread_commlib_to_pty
    */
   cl_raw_list_lock(thread_lib_handle);
   cl_thread_list_delete_thread_from_list(thread_lib_handle, thread_pty_to_commlib);
   cl_thread_list_delete_thread_from_list(thread_lib_handle, thread_commlib_to_pty);
   cl_raw_list_unlock(thread_lib_handle);

   shepherd_trace("parent: shutting down pty_to_commlib thread");
   cl_thread_shutdown(thread_pty_to_commlib);
   shepherd_trace("parent: shutting down commlib_to_pty thread");
   cl_thread_shutdown(thread_commlib_to_pty);

   // Workaround for CS-982 - the trigger_thread_condition below does *not* wake up commlib_to_pty thread.
   cl_com_ignore_timeouts(true);

   /*
    * This will wake up all threads waiting for a message
    */
#ifdef EXTENSIVE_TRACING
   shepherd_trace("parent: calling cl_thread_trigger_thread_condition()");
#endif
   cl_thread_trigger_thread_condition(g_comm_handle->app_condition, 1);
#ifdef EXTENSIVE_TRACING
   shepherd_trace("parent: after cl_thread_trigger_thread_condition()");
#endif

   close(g_p_ijs_fds->pty_master);

   close(g_p_ijs_fds->pipe_in);
   close(g_p_ijs_fds->pipe_out);
   close(g_p_ijs_fds->pipe_err);

   /*
    * Wait until threads have shut down and call cleanup functions
    */
   cl_thread_join(thread_pty_to_commlib);
   cl_thread_join(thread_commlib_to_pty);
   cl_thread_cleanup(thread_pty_to_commlib);
   cl_thread_cleanup(thread_commlib_to_pty);

   // Rollback workaround for CS-982
   cl_com_ignore_timeouts(false);


#if 0
{
struct timeb ts;
ftime(&ts);
shepherd_trace("+++++ timestamp: %d.%03d ++++", (int)ts.time, (int)ts.millitm);
}
#endif

   /* From here on, only the main thread is running */
   shepherd_trace("parent: thread_cleanup_lib()");
   thread_cleanup_lib(&thread_lib_handle);

   /* The communication will be freed in close_parent_loop() */
   sge_dstring_free(err_msg);
   shepherd_trace("parent: leaving main loop. From here on, only the main thread is running.");
   return 0;
}

int close_parent_loop(int exit_status)
{
   int     ret = 0;
   char    sz_exit_status[21];
   DSTRING_STATIC(err_msg, MAX_STRING_SIZE);

   /*
    * Send UNREGISTER_CTRL_MSG
    */
   snprintf(sz_exit_status, 20, "%d", exit_status);
   shepherd_trace("sending UNREGISTER_CTRL_MSG with exit_status = \"%s\"",
                  sz_exit_status);
   shepherd_trace("sending to host: %s",
                  g_hostname != nullptr ? g_hostname : "<null>");
   ret = (int)comm_write_message(g_comm_handle, g_hostname,
      COMM_SERVER, 1, (unsigned char*)sz_exit_status, strlen(sz_exit_status),
      UNREGISTER_CTRL_MSG, &err_msg);

   if (ret != (int)strlen(sz_exit_status)) {
      shepherd_trace("comm_write_message returned: %s",
                             sge_dstring_get_string(&err_msg));
      shepherd_trace("close_parent_loop: comm_write_message() returned %d "
                             "instead of %d!!!", ret, strlen(sz_exit_status));
   }

   /*
    * Wait for UNREGISTER_RESPONSE_CTRL_MSG
    */
   {
      int                  count = 0;
      recv_message_t       recv_mess;

      shepherd_trace("waiting for UNREGISTER_RESPONSE_CTRL_MSG");
      while (count < RESPONSE_MSG_TIMEOUT) {
         memset(&recv_mess, 0, sizeof(recv_message_t));
         ret = comm_recv_message(g_comm_handle, &recv_mess, &err_msg);
         count++;
         if (recv_mess.type == UNREGISTER_RESPONSE_CTRL_MSG) {
            shepherd_trace("Received UNREGISTER_RESPONSE_CTRL_MSG");
            comm_free_message(&recv_mess, &err_msg);
            break;
         } else if (ret == COMM_NO_MESSAGE_AVAILABLE) {
            /* trace this only every 10 loops (default: 1 loop = 1 s) */
            if (count%10 == 0) {
               shepherd_trace("still waiting for UNREGISTER_RESPONSE_CTRL_MSG");
            }
         } else if (ret == COMM_CONNECTION_NOT_FOUND) {
            shepherd_trace("client disconnected - break");
            break;
         } else {
            shepherd_trace("No connection or problem while waiting for message: %d", ret);
            break;
         }
         comm_free_message(&recv_mess, &err_msg);
      }
   }
   /* Now we are completely logged of from the server and can shut down */

   /*
    * Tell the communication to shut down immediately, don't wait for
    * the next timeout
    */
   shepherd_trace("parent: cl_com_ignore_timeouts");
   comm_ignore_timeouts(true, &err_msg);

   /*
    * Do cleanup
    */
   ret = comm_cleanup_lib(&err_msg);
   if (ret != COMM_RETVAL_OK) {
      shepherd_trace("parent: error in comm_cleanup_lib(): %d", ret);
   }

   sge_free(&g_hostname);
   shepherd_trace("parent: leaving close_parent_loop()");
   return 0;
}

