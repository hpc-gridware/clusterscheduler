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
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *   Copyright: 2001 by Sun Microsystems, Inc.
 *
 *   All Rights Reserved.
 *
 *  Portions of this code are Copyright 2011 Univa Inc.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <cctype>

#include <termios.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#if defined(DARWIN)
#  include <sys/ttycom.h>
#  include <sys/ioctl.h>
#elif defined(FREEBSD) || defined(NETBSD)
#  include <sys/ioctl.h>
#endif

#include "uti/sge_io.h"
#include "uti/sge_pty.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

#include "sge_ijs_comm.h"
#include "sge_ijs_threads.h"
#include "sge_client_ijs.h"
#include "ijs/sge_ijs_lib.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/cull/sge_all_listsL.h"

/* module variables */
static char *g_hostname  = nullptr;
static int  g_exit_status = 0; // set by worker thread, read by main thread
static int  g_nostdin     = 0; // set by main thread, read by worker thread
static int  g_noshell     = 0; // set by main thread, read by worker thread
static int  g_is_rsh      = 0; // set by main thread, read by worker thread
static unsigned int g_pid = 0; // set by main thread, read by worker thread
static int  g_raw_mode_state = 0; // set by main thread, read by worker thread
static int  g_suspend_remote = 0; // set by main thread, read by worker thread
static COMM_HANDLE *g_comm_handle = nullptr;
static int g_wakeup_pipe[2] = { -1, -1 }; // pipe to wake up worker thread */
static bool g_client_connected = false;  // set to true by commlib_to_tty thread once the client (sge_shepherd) connected
static bool g_do_exit = false;           // set by the run_ijs_server (the parent thread) to tell the worker threads to shutdown
static volatile bool g_escape_disconnect = false; ///< set by tty_to_commlib when escape+. is detected
static char g_escape_char = '~';                  ///< configurable IJS escape char; '\0' = disabled

/*
 * static volatile sig_atomic_t received_window_change_signal = 1;
 * Flag to indicate that we have received a window change signal which has
 * not yet been processed.  This will cause a message indicating the new
 * window size to be sent to the server a little later.  This is volatile
 * because this is updated in a signal handler.
 */
static volatile sig_atomic_t received_window_change_signal = 1;
static volatile sig_atomic_t received_broken_pipe_signal = 0;
static volatile sig_atomic_t quit_pending; /* Set non-zero to quit the loop. */

volatile sig_atomic_t received_signal = 0;

/* X11 forwarding state — set by run_ijs_server before worker threads start */
#define X11_MAX_CONNS 64
static bool             g_forward_x11 = false;         ///< true when -X was given
static char             g_x11_display[256] = "";       ///< client's DISPLAY (e.g. ":0.0")
static char             g_x11_cookie_hex[33] = "";     ///< real MIT-MAGIC-COOKIE-1, pre-fetched before threads start
static int              g_x11_fds[X11_MAX_CONNS];      ///< per-conn_id fd to real X server (-1 = unused)
static pthread_mutex_t  g_x11_mutex = PTHREAD_MUTEX_INITIALIZER;

/****** window_change_handler **************************************************
*  NAME
*     window_change_handler() -- handler for the window changed signal
*
*  SYNOPSIS
*     static void window_change_handler(int sig)
*
*  FUNCTION
*     Signal handler for the window change signal (SIGWINCH).  This just sets a
*     flag indicating that the window has changed.
*
*  INPUTS
*     int sig - number of the received signal
*
*  RESULT
*     void - no result
*
*  NOTES
*    MT-NOTE: window_change_handler() is not MT safe, because it uses
*             received_window_change_signal
*
*  SEE ALSO
*******************************************************************************/
static void window_change_handler(int sig)
{
   /* Do not use DPRINTF in a signal handler! */
   received_window_change_signal = 1;
   signal(SIGWINCH, window_change_handler);
}

/****** broken_pipe_handler ****************************************************
*  NAME
*     broken_pipe_handler() -- handler for the broken pipe signal
*
*  SYNOPSIS
*     static void broken_pipe_handler(int sig)
*
*  FUNCTION
*     Handler for the SIGPIPE signal.
*
*  INPUTS
*     int sig - number of the received signal
*
*  RESULT
*     void - no result
*
*  NOTES
*
*  SEE ALSO
*******************************************************************************/
static void broken_pipe_handler(int sig)
{
   received_broken_pipe_signal = 1;
   signal(SIGPIPE, broken_pipe_handler);
}

/****** signal_handler() *******************************************************
*  NAME
*     signal_handler() -- handler for quit signals
*
*  SYNOPSIS
*     static void signal_handler(int sig)
*
*  FUNCTION
*     Handler for all signals that quit the program. These signals are trapped
*     in order to restore the terminal modes.
*
*  INPUTS
*     int sig - number of the received signal
*
*  RESULT
*     void - no result
*
*  NOTES
*
*  SEE ALSO
*******************************************************************************/
void signal_handler(int sig)
{
   received_signal = sig;
   quit_pending = 1;
}

/****** set_signal_handlers() **************************************************
*  NAME
*     set_signal_handlers() -- set all signal handlers
*
*  SYNOPSIS
*     static void set_signal_handlers()
*
*  FUNCTION
*     Sets all signal handlers. Doesn't overwrite SIG_IGN and therefore
*     matches the behaviour of rsh.
*
*  RESULT
*     void - no result
*
*  NOTES
*
*  SEE ALSO
*******************************************************************************/
void set_signal_handlers()
{
   struct sigaction old_handler, new_handler;
   memset(&old_handler, 0, sizeof(old_handler));
   memset(&new_handler, 0, sizeof(new_handler));

   /* Is SIGHUP necessary?
    * Yes: termio(7I) says:
    * "When a modem disconnect is detected, a SIGHUP signal is sent
    *  to the terminal's controlling process.
    *  Unless other arrangements have  been  made,  these  signals
    *  cause  the  process  to  terminate. If  SIGHUP is ignored or
    *  caught, any subsequent  read  returns  with  an  end-of-file
    *  indication until the terminal is closed."
    */
   sigaction(SIGHUP, nullptr, &old_handler);
   if (old_handler.sa_handler != SIG_IGN) {
      new_handler.sa_handler = signal_handler;
      sigaddset(&new_handler.sa_mask, SIGHUP);
      new_handler.sa_flags = SA_RESTART;
      sigaction(SIGHUP, &new_handler, nullptr);
   }

   sigaction(SIGINT, nullptr, &old_handler);
   if (old_handler.sa_handler != SIG_IGN) {
      new_handler.sa_handler = signal_handler;
      sigaddset(&new_handler.sa_mask, SIGINT);
      new_handler.sa_flags = SA_RESTART;
      sigaction(SIGINT, &new_handler, nullptr);
   }

   sigaction(SIGCONT, nullptr, &old_handler);
   if (old_handler.sa_handler != SIG_IGN) {
      new_handler.sa_handler = signal_handler;
      sigaddset(&new_handler.sa_mask, SIGCONT);
      new_handler.sa_flags = SA_RESTART;
      sigaction(SIGCONT, &new_handler, nullptr);
   }

   sigaction(SIGQUIT, nullptr, &old_handler);
   if (old_handler.sa_handler != SIG_IGN) {
      new_handler.sa_handler = signal_handler;
      sigaddset(&new_handler.sa_mask, SIGQUIT);
      new_handler.sa_flags = SA_RESTART;
      sigaction(SIGQUIT, &new_handler, nullptr);
   }

   sigaction(SIGTERM, nullptr, &old_handler);
   if (old_handler.sa_handler != SIG_IGN) {
      new_handler.sa_handler = signal_handler;
      sigaddset(&new_handler.sa_mask, SIGTERM);
      new_handler.sa_flags = SA_RESTART;
      sigaction(SIGTERM, &new_handler, nullptr);
   }

   new_handler.sa_handler = window_change_handler;
   sigaddset(&new_handler.sa_mask, SIGWINCH);
   new_handler.sa_flags = SA_RESTART;
   sigaction(SIGWINCH, &new_handler, nullptr);

   new_handler.sa_handler = broken_pipe_handler;
   sigaddset(&new_handler.sa_mask, SIGPIPE);
   new_handler.sa_flags = SA_RESTART;
   sigaction(SIGPIPE, &new_handler, nullptr);
}

/****** client_check_window_change() *******************************************
*  NAME
*     client_check_window_change() -- check if window size was change and
*                                     submit changes to pty
*
*  SYNOPSIS
*     static void client_check_window_change(COMM_HANDLE *handle)
*
*  FUNCTION
*     Checks if the window size of the terminal window was changed.
*     If the size was changed, submits the new window size to the
*     pty.
*     The actual change is detected by a signal (on Unix), this function
*     just checks the according flag.
*
*  INPUTS
*     COMM_HANDLE *handle - pointer to the commlib handle
*
*  RESULT
*     void - no result
*
*  NOTES
*     MT-NOTE: client_check_window_change() is MT-safe (see comment in code)
*
*  SEE ALSO
*     window_change_handler()
*******************************************************************************/
static void client_check_window_change(COMM_HANDLE *handle)
{
   struct winsize ws;
   char           buf[200];
   dstring        err_msg = DSTRING_INIT;

   DENTER(TOP_LAYER);

   if (received_window_change_signal) {
      /*
       * here we can have a race condition between the two working threads,
       * but it doesn't matter - in the worst case, the new window size gets
       * submitted two times.
       */
      received_window_change_signal = 0;
      if (ioctl(fileno(stdin), TIOCGWINSZ, &ws) >= 0) {
         DPRINTF("sending WINDOW_SIZE_CTRL_MSG with new window size: %d, %d, %d, %d to shepherd\n",
                 ws.ws_row, ws.ws_col, ws.ws_xpixel, ws.ws_ypixel);

         snprintf(buf, sizeof(buf), "WS %d %d %d %d", ws.ws_row, ws.ws_col, ws.ws_xpixel, ws.ws_ypixel);
         comm_write_message(handle, g_hostname, COMM_CLIENT, 1, (unsigned char*)buf, strlen(buf),
                            WINDOW_SIZE_CTRL_MSG, &err_msg);
      } else {
         DPRINTF("client_check_windows_change: ioctl() failed! sending dummy WINDOW_SIZE_CTRL_MSG to fullfill protocol.\n");
         snprintf(buf, sizeof(buf), "WS 60 80 480 640");
         comm_write_message(handle, g_hostname, COMM_CLIENT, 1, (unsigned char*)buf, strlen(buf), WINDOW_SIZE_CTRL_MSG, &err_msg);
      }
   }
   sge_dstring_free(&err_msg);
   DRETURN_VOID;
}

/**
 * @brief Retrieve the MIT-MAGIC-COOKIE-1 for a DISPLAY.
 *
 * Runs "xauth list <display>" and parses the output to extract the
 * MIT-MAGIC-COOKIE-1.  The 32-character hex string is written into
 * @p cookie_hex.  Must be called in the main thread before worker threads
 * start to avoid popen()/waitpid() races with different SIGCHLD masks.
 *
 * @param display         X11 display string (e.g. ":0" or "host:10.0").
 * @param cookie_hex      Caller-supplied buffer; must be at least 33 bytes.
 * @param cookie_hex_size Size of @p cookie_hex in bytes.
 * @return true if a 32-character MIT-MAGIC-COOKIE-1 was found and written;
 *         false otherwise (xauth not installed, display not listed, or buffer
 *         too small).
 * @note MT-NOTE: not MT-safe (calls popen()).
 */
static bool x11_get_cookie(const char *display, char *cookie_hex, size_t cookie_hex_size) {
   DENTER(TOP_LAYER);

   char cmd[512];
   snprintf(cmd, sizeof(cmd), "xauth list %.200s 2>/dev/null", display);
   FILE *fp = popen(cmd, "r");
   if (!fp) {
      DRETURN(false);
   }

   bool found = false;
   char line[512];
   while (fgets(line, sizeof(line), fp)) {
      const char *p = strstr(line, "MIT-MAGIC-COOKIE-1");
      if (p != nullptr) {
         p += strlen("MIT-MAGIC-COOKIE-1");
         while (*p == ' ' || *p == '\t') {
            p++;
         }
         size_t n = 0;
         while (n < cookie_hex_size - 1 && isxdigit((unsigned char)p[n])) {
            cookie_hex[n] = p[n];
            n++;
         }
         cookie_hex[n] = '\0';
         if (n == 32) {
            found = true;
            break;
         }
      }
   }
   pclose(fp);
   DRETURN(found);
}

/**
 * @brief Connect to the real X server identified by a display string.
 *
 * Supports both Unix-domain displays (":N[.S]" → /tmp/.X11-unix/XN) and
 * TCP displays ("host:N[.S]" → port 6000+N).  Called by the tty_to_commlib
 * thread when a new X11 connection arrives on the proxy socket (X11_OPEN_MSG
 * from the shepherd).
 *
 * @param display X11 display string, e.g. ":0", ":0.0", or "myhost:1.0".
 * @return Connected socket file descriptor, or -1 on failure.
 * @note MT-NOTE: MT-safe.
 */
static int x11_connect_to_server(const char *display) {
   DENTER(TOP_LAYER);

   int fd = -1;
   if (display[0] == ':') {
      // Unix domain socket: /tmp/.X11-unix/XN
      int display_num = atoi(display + 1);
      struct sockaddr_un addr{};
      memset(&addr, 0, sizeof(addr));
      addr.sun_family = AF_UNIX;
      snprintf(addr.sun_path, sizeof(addr.sun_path), "/tmp/.X11-unix/X%d", display_num);
      fd = socket(AF_UNIX, SOCK_STREAM, 0);
      if (fd >= 0 && connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
         close(fd);
         fd = -1;
      }
   } else {
      // TCP: host:N[.S] → port 6000+N
      const char *colon = strrchr(display, ':');
      if (colon != nullptr) {
         char host[256] = "";
         int host_len = static_cast<int>(colon - display);
         if (host_len > 0 && host_len < static_cast<int>(sizeof(host))) {
            memcpy(host, display, host_len);
            host[host_len] = '\0';
         } else {
            snprintf(host, sizeof(host), "localhost");
         }
         int display_num = atoi(colon + 1);
         char port_str[16];
         snprintf(port_str, sizeof(port_str), "%d", 6000 + display_num);
         struct addrinfo hints, *res = nullptr;
         memset(&hints, 0, sizeof(hints));
         hints.ai_family   = AF_UNSPEC;
         hints.ai_socktype = SOCK_STREAM;
         if (getaddrinfo(host, port_str, &hints, &res) == 0 && res != nullptr) {
            fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
            if (fd >= 0 && connect(fd, res->ai_addr, res->ai_addrlen) != 0) {
               close(fd);
               fd = -1;
            }
            freeaddrinfo(res);
         }
      }
   }
   DRETURN(fd);
}

static void
wakeup_tty_to_commlib_thread(const char *source) {
   DENTER(TOP_LAYER);

   // write a byte into the wakeup pipe to wake up the tty_to_commlib thread
   if (g_wakeup_pipe[1] != -1) {
      DPRINTF("%s: writing wakeup byte to pipe\n", source);
      if (write(g_wakeup_pipe[1], "x", 1) < 0) {
         DPRINTF("%s: write() to wakeup pipe failed: %s\n", source, strerror(errno));
      }
   } else {
      DPRINTF("%s: g_wakeup_pipe[1] is -1, not writing wakeup byte\n", source);
   }

   DRETURN_VOID;
}

/****** tty_to_commlib() *******************************************************
*  NAME
*     tty_to_commlib() -- tty_to_commlib thread entry point and main loop
*
*  SYNOPSIS
*     void* tty_to_commlib(void *t_conf)
*
*  FUNCTION
*     Entry point and main loop of the tty_to_commlib thread.
*     Reads data from the tty and writes it to the commlib.
*
*  INPUTS
*     void *t_conf - pointer to cl_thread_settings_t struct of the thread
*
*  RESULT
*     void* - always nullptr
*
*  NOTES
*     MT-NOTE: tty_to_commlib is MT-safe ?
*
*  SEE ALSO
*******************************************************************************/
/**
 * @brief Filter SSH-style escape sequences from a TTY input buffer.
 *
 * Scans buf[0..len-1] byte by byte. escape_char is only treated as the escape
 * character when it immediately follows a newline/CR or at session start.
 * Recognised sequences (E = escape_char):
 *   E.   disconnect (sets *p_disconnect; caller stops forwarding)
 *   EE   send literal E
 *   EX   unrecognised: forward E and X unchanged
 *
 * When escape_char == '\0', all bytes are forwarded unmodified (disabled).
 * Modifies buf in-place; returns the number of bytes to forward.
 * *p_at_line_start and *p_pending_escape must be preserved across calls.
 *
 * @param buf              Input/output buffer (modified in-place).
 * @param len              Number of valid bytes in buf.
 * @param escape_char      Configured escape character; '\0' disables processing.
 * @param p_at_line_start  In/out: true when prev forwarded byte was '\n'/'\r' or session start.
 * @param p_pending_escape In/out: true when escape_char is pending at line start.
 * @param p_disconnect     Out: set to true when E. is detected.
 * @return                 Number of bytes to forward from buf.
 */
static int tty_escape_process(unsigned char *buf, int len, unsigned char escape_char,
                               bool *p_at_line_start, bool *p_pending_escape,
                               bool *p_disconnect)
{
   *p_disconnect = false;
   if (escape_char == '\0') {
      return len;
   }
   int out = 0;
   for (int i = 0; i < len; i++) {
      unsigned char c = buf[i];
      if (*p_pending_escape) {
         *p_pending_escape = false;
         if (c == '.') {
            *p_disconnect = true;
            return out;
         } else if (c == escape_char) {
            buf[out++] = escape_char;        // EE -> forward single escape char
            *p_at_line_start = false;
         } else {
            buf[out++] = escape_char;        // unrecognised EX -> forward both bytes
            buf[out++] = c;
            *p_at_line_start = (c == '\n' || c == '\r');
         }
      } else if (*p_at_line_start && c == escape_char) {
         *p_pending_escape = true;           // hold escape char; next byte decides
         *p_at_line_start = false;
      } else {
         buf[out++] = c;
         *p_at_line_start = (c == '\n' || c == '\r');
      }
   }
   return out;
}

void *tty_to_commlib(void *t_conf) {
   DENTER(TOP_LAYER);

   char pbuf[BUFSIZE];
   // @todo err_msg is passed into / filled into in several functions, but it is never output anywhere
   DSTRING_STATIC(err_msg, MAX_STRING_SIZE);

   thread_func_startup(t_conf);

   /*
    * allocate working buffer
    */
   bool do_exit = false;
   bool at_line_start = true;   // true at session start; reset after each '\n' or '\r'
   bool pending_escape = false; // true when escape_char at line start is pending next char
   while (!do_exit) {
      // This thread should only start its processing once the commlib_to_tty thread received a
      // REGISTER_CTRL_MSG from the sge_shepherd and replied to it
      // We wait on the wakeup_pipe for a byte sent from the commlib_to_tty thread.
      fd_set read_fds;
      FD_ZERO(&read_fds);
      if (g_nostdin == 0 && g_client_connected) {
         /* wait for input on tty */
         FD_SET(STDIN_FILENO, &read_fds);
      }
      if (g_wakeup_pipe[0] != -1) {
         /* wait for input on wakeup pipe */
         FD_SET(g_wakeup_pipe[0], &read_fds);
      }

      // Snapshot X11 fds and add them to the fd_set so data from X clients reaches the job.
      int local_x11_fds[X11_MAX_CONNS];
      int fd_max = std::max(STDIN_FILENO, g_wakeup_pipe[0]);
      if (g_forward_x11) {
         pthread_mutex_lock(&g_x11_mutex);
         memcpy(local_x11_fds, g_x11_fds, sizeof(local_x11_fds));
         pthread_mutex_unlock(&g_x11_mutex);
         for (int xi = 0; xi < X11_MAX_CONNS; xi++) {
            if (local_x11_fds[xi] >= 0) {
               FD_SET(local_x11_fds[xi], &read_fds);
               if (local_x11_fds[xi] > fd_max) {
                  fd_max = local_x11_fds[xi];
               }
            }
         }
      }

      struct timeval       timeout;
      timeout.tv_sec  = 1;
      timeout.tv_usec = 0;

      if (received_signal == SIGCONT) {
         received_signal = 0;
         if (continue_handler (g_comm_handle, g_hostname) == 1) {
            do_exit = true;
            continue;
         }
         if (g_raw_mode_state == 1) {
            /* restore raw-mode after SIGCONT */
            if (terminal_enter_raw_mode () != 0) {
               DPRINTF("tty_to_commlib: couldn't enter raw mode for pty\n");
               do_exit = true;
               continue;
            }
         }
      }

      DPRINTF("tty_to_commlib: Waiting in select() for data\n");
      int ret = select(fd_max + 1, &read_fds, nullptr, nullptr, &timeout);
      DPRINTF("tty_to_commlib: select returned %d\n", ret);

      thread_testcancel(t_conf);

      // Check for window changed events and send them to the client - but only if a client is connected.
      if (g_client_connected) {
         client_check_window_change(g_comm_handle);
      }

      if (received_signal == SIGHUP ||
          received_signal == SIGINT ||
          received_signal == SIGQUIT ||
          received_signal == SIGTERM) {
         /* If we receive one of these signals, we must terminate */
         do_exit = true;
         continue;
      }

      if (ret > 0) {
         dstring dbuf = DSTRING_INIT;

         if (g_nostdin == 1) {
            /* We should never get here if STDIN is closed */
            DPRINTF("tty_to_commlib: STDIN ready to read while it should be closed!!!\n");
         }
         if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            DPRINTF("tty_to_commlib: trying to read() from stdin\n");
            int nread = read(STDIN_FILENO, pbuf, BUFSIZE - 1);
            pbuf[nread] = '\0';
            sge_dstring_append(&dbuf, pbuf);
            DPRINTF("tty_to_commlib: nread = %d\n", nread);

            if (nread < 0 && (errno == EINTR || errno == EAGAIN)) {
               DPRINTF("tty_to_commlib: EINTR or EAGAIN\n");
               /* do nothing */
            } else if (nread <= 0) {
               do_exit = true;
            } else {
               // filter escape sequences before forwarding to the remote shell
               bool escape_disconnect = false;
               int nforward = tty_escape_process(
                  (unsigned char *)pbuf, nread, (unsigned char)g_escape_char,
                  &at_line_start, &pending_escape, &escape_disconnect);
               if (escape_disconnect) {
                  // escape+. detected: restore terminal immediately so the user gets their shell back
                  const char disconnect_msg[] = "\r\n~.\r\n";
                  sge_writenbytes(STDOUT_FILENO, disconnect_msg, sizeof(disconnect_msg) - 1);
                  terminal_leave_raw_mode();
                  g_escape_disconnect = true;
                  do_exit = true;
               } else if (nforward > 0) {
                  DPRINTF("tty_to_commlib: writing to commlib: %d bytes\n", nforward);
                  if (suspend_handler(g_comm_handle, g_hostname, g_is_rsh, g_suspend_remote, g_pid, &dbuf) == 1) {
                     if (comm_write_message(g_comm_handle, g_hostname,
                                            COMM_CLIENT, 1, (unsigned char *) pbuf,
                                            (unsigned long) nforward, STDIN_DATA_MSG, &err_msg) != (unsigned long) nforward) {
                        DPRINTF("tty_to_commlib: couldn't write all data\n");
                     } else {
                        DPRINTF("tty_to_commlib: data successfully written\n");
                     }
                  }
                  comm_wait_for_all_messages_sent(g_comm_handle, &err_msg);
               }
            }
         } else if (FD_ISSET(g_wakeup_pipe[0], &read_fds)) {
            // Drain the byte written by wakeup_tty_to_commlib_thread() so that select()
            // does not keep returning immediately. Two callers write to this pipe:
            //   1. commlib_to_tty (~line 625) when shepherd completes the REGISTER handshake
            //      — g_client_connected is set but g_do_exit is still false at that point
            //   2. run_ijs_server (~line 821) when it sets g_do_exit = true for shutdown
            //   3. commlib_to_tty when a new X11 connection was opened (X11_OPEN_MSG)
            char wakeup_byte;
            if (read(g_wakeup_pipe[0], &wakeup_byte, 1) < 0) {
               DPRINTF("tty_to_commlib: read() from wakeup pipe failed: %s\n", strerror(errno));
            }
            DPRINTF("tty_to_commlib: wakeup pipe was triggered\n");
            if (g_do_exit) {
               DPRINTF("tty_to_commlib: exit was requested\n");
               do_exit = true;
            }
         }

         // relay data from X11 connections (real X server) back to shepherd
         if (g_forward_x11) {
            static char x11_buf[2 + BUFSIZE];
            for (int xi = 0; xi < X11_MAX_CONNS && !do_exit; xi++) {
               if (local_x11_fds[xi] >= 0 && FD_ISSET(local_x11_fds[xi], &read_fds)) {
                  int nread = read(local_x11_fds[xi], x11_buf + 2, BUFSIZE - 1);
                  if (nread <= 0) {
                     // X server closed this connection
                     DPRINTF("tty_to_commlib: X11 conn %d closed by X server\n", xi);
                     x11_buf[0] = static_cast<char>((xi >> 8) & 0xFF);
                     x11_buf[1] = static_cast<char>(xi & 0xFF);
                     comm_write_message(g_comm_handle, g_hostname, COMM_CLIENT, 1,
                                        (unsigned char *)x11_buf, 2, X11_CLOSE_MSG, &err_msg);
                     pthread_mutex_lock(&g_x11_mutex);
                     close(g_x11_fds[xi]);
                     g_x11_fds[xi] = -1;
                     pthread_mutex_unlock(&g_x11_mutex);
                  } else {
                     // relay X11 data to shepherd
                     x11_buf[0] = static_cast<char>((xi >> 8) & 0xFF);
                     x11_buf[1] = static_cast<char>(xi & 0xFF);
                     DPRINTF("tty_to_commlib: relaying %d bytes from X11 conn %d\n", nread, xi);
                     comm_write_message(g_comm_handle, g_hostname, COMM_CLIENT, 1,
                                        (unsigned char *)x11_buf, static_cast<unsigned long>(nread + 2),
                                        X11_DATA_MSG, &err_msg);
                  }
               }
            }
         }

         sge_dstring_free(&dbuf);
      } else {
         /*
          * We got either a select timeout or a select error. In both cases,
          * it's a good chance to check if our client is still alive.
          */
         DPRINTF("tty_to_commlib: Checking if client is still alive\n");
         if (comm_get_connection_count(g_comm_handle, &err_msg) == 0) {
            DPRINTF("tty_to_commlib: Client is not alive! -> exiting.\n");
            do_exit = true;
         } else {
            DPRINTF("tty_to_commlib: Client is still alive\n");
         }
      }
   } /* while (!do_exit) */

   /* Send STDIN_CLOSE_MSG to the shepherd. That causes the shepherd to close its filedescriptor, also. */
   if (comm_write_message(g_comm_handle, g_hostname, COMM_CLIENT, 1, (unsigned char *) " ",
                      1, STDIN_CLOSE_MSG, &err_msg) != 1) {
      DPRINTF("tty_to_commlib: couldn't write STDIN_CLOSE_MSG\n");
   } else {
      DPRINTF("tty_to_commlib: STDIN_CLOSE_MSG successfully written\n");
   }

   /* clean up */
   thread_func_cleanup(t_conf);

   DPRINTF("tty_to_commlib: exiting tty_to_commlib thread!\n");
   DRETURN(nullptr);
}

/**
 * @brief Entry point and main loop of the commlib_to_tty thread.
 *
 * Reads data from the commlib and writes it to the tty (stdout/stderr of the
 * job).  Also handles X11 forwarding: on REGISTER_CTRL_MSG it sends the
 * pre-fetched MIT-MAGIC-COOKIE-1 to the shepherd (X11_AUTH_MSG); on
 * X11_OPEN_MSG it connects to the real X server and registers the channel; on
 * X11_DATA_MSG it forwards X protocol bytes to the appropriate channel; on
 * X11_CLOSE_MSG it tears down the corresponding X11 connection.
 *
 * @param t_conf Pointer to cl_thread_settings_t struct of the thread.
 * @return Always nullptr.
 * @note MT-NOTE: MT-safe.
 */
void* commlib_to_tty(void *t_conf)
{
   recv_message_t       recv_mess;
   dstring              err_msg = DSTRING_INIT;
   int                  ret = 0, do_exit = 0;

   DENTER(TOP_LAYER);
   thread_func_startup(t_conf);

   while (do_exit == 0) {
      /*
       * wait blocking for a message from commlib
       */
      recv_mess.cl_message = nullptr;
      recv_mess.data = nullptr;

      DPRINTF("commlib_to_tty: recv_message()\n");
      ret = comm_recv_message(g_comm_handle, &recv_mess, &err_msg);
      if (ret != COMM_RETVAL_OK) {
         /* check if we are still connected to anybody. */
         /* if not - exit. */
         DPRINTF("commlib_to_tty: error receiving message: %s\n", sge_dstring_get_string(&err_msg));
         if (comm_get_connection_count(g_comm_handle, &err_msg) == 0) {
            DPRINTF("commlib_to_tty: no endpoint found\n");
            do_exit = 1;
            continue;
         }
      }
      DPRINTF("commlib_to_tty: received a message\n");

      thread_testcancel(t_conf);

      // exit if tty_to_commlib triggered an escape disconnect
      if (g_escape_disconnect) {
         DPRINTF("commlib_to_tty: escape disconnect requested, exiting\n");
         do_exit = 1;
         continue;
      }

      if (received_signal == SIGHUP ||
          received_signal == SIGINT ||
          received_signal == SIGQUIT ||
          received_signal == SIGTERM) {
         /* If we receive one of these signals, we must terminate */
         DPRINTF("commlib_to_tty: shutting down because of signal %d\n", received_signal);
         do_exit = 1;
         continue;
      }

      DPRINTF("'parsing' message\n");
      /*
       * 'parse' message
       * A 1 byte prefix tells us what kind of message it is.
       * See sge_ijs_comm.h for message types.
       */
      if (recv_mess.cl_message != nullptr) {
         char buf[100];
         switch (recv_mess.type) {
            case STDOUT_DATA_MSG:
               /* copy recv_mess.data to buf to append '\0' */
               if (DPRINTF_IS_ACTIVE) {
                  memcpy(buf, recv_mess.data, std::min(99ul, recv_mess.cl_message->message_length - 1));
                  buf[std::min(99ul, recv_mess.cl_message->message_length - 1)] = 0;
                  DPRINTF("commlib_to_tty: received stdout message, writing to tty.\n");
                  DPRINTF("commlib_to_tty: message is: %s\n", buf);
               }
/* TODO: If it's not possible to write all data to the tty, retry blocking
 *       until all data was written. The commlib must block then, too.
 */
               if (sge_writenbytes(STDOUT_FILENO, recv_mess.data,
                          (int)(recv_mess.cl_message->message_length-1))
                       != (int)(recv_mess.cl_message->message_length-1)) {
                  DPRINTF("commlib_to_tty: sge_writenbytes() error\n");
               }
               break;
            case STDERR_DATA_MSG:
               DPRINTF("commlib_to_tty: received stderr message, writing to tty.\n");
/* TODO: If it's not possible to write all data to the tty, retry blocking
 *       until all data was written. The commlib must block then, too.
 */
               if (sge_writenbytes(STDERR_FILENO, recv_mess.data,
                          (int)(recv_mess.cl_message->message_length-1))
                       != (int)(recv_mess.cl_message->message_length-1)) {
                  DPRINTF("commlib_to_tty: sge_writenbytes() error\n");
               }
               break;
            case WINDOW_SIZE_CTRL_MSG:
               /* control message */
               /* we don't expect a control message */
               DPRINTF(("commlib_to_tty: received window size message! "
                        "This was unexpected!\n"));
               break;
            case REGISTER_CTRL_MSG:
               /* control message */
               /* A client registered with us. The commlib write thread
                * will send the WINDOW_SIZE_CTRL_MSG
                * (and perhaps some data messages),  which is already in the
                * send_messages list of the connection, to the client.
                */
               DPRINTF("commlib_to_tty: received register message!\n");
               // Before sending SETTINGS_CTRL_MSG, send X11_AUTH_MSG if X11 forwarding is
               // requested. TCP ordering guarantees that the shepherd receives X11_AUTH_MSG
               // first and sets up the X11 proxy display before processing SETTINGS_CTRL_MSG.
               // The cookie was pre-fetched in run_ijs_server() before threads started.
               if (g_forward_x11 && g_x11_cookie_hex[0] != '\0') {
                  DPRINTF("commlib_to_tty: sending X11_AUTH_MSG\n");
                  comm_write_message(g_comm_handle, g_hostname, COMM_CLIENT, 1,
                                     (unsigned char *)g_x11_cookie_hex, strlen(g_x11_cookie_hex) + 1,
                                     X11_AUTH_MSG, &err_msg);
               }
               /* Send the settings in response */
               snprintf(buf, sizeof(buf), "noshell = %d", g_noshell);
               ret = (int)comm_write_message(g_comm_handle, g_hostname, COMM_CLIENT, 1,
                  (unsigned char*)buf, strlen(buf)+1, SETTINGS_CTRL_MSG, &err_msg);
               DPRINTF("commlib_to_tty: sent SETTINGS_CTRL_MSG, ret = %d\n", ret);
               // Wait for the messages to really have left commlib.
               comm_wait_for_all_messages_sent(g_comm_handle, &err_msg);
               // Now we can wake up the tty_to_commlib thread, and it can start reading from stdin.
               g_client_connected = true;
               wakeup_tty_to_commlib_thread("commlib_to_tty");
               break;
            case X11_OPEN_MSG: {
               // Shepherd accepted a new X11 connection from the job; open a matching
               // connection to the real X server and record it under the given conn_id.
               if (recv_mess.cl_message->message_length < 3) {
                  DPRINTF("commlib_to_tty: X11_OPEN_MSG too short\n");
                  break;
               }
               int conn_id = (static_cast<unsigned char>(recv_mess.data[0]) << 8)
                           | static_cast<unsigned char>(recv_mess.data[1]);
               if (conn_id < 0 || conn_id >= X11_MAX_CONNS) {
                  DPRINTF("commlib_to_tty: X11_OPEN_MSG conn_id %d out of range\n", conn_id);
                  break;
               }
               int x11_fd = x11_connect_to_server(g_x11_display);
               DPRINTF("commlib_to_tty: X11_OPEN_MSG conn_id %d -> fd %d\n", conn_id, x11_fd);
               if (x11_fd < 0) {
                  // Cannot reach real X server — tell shepherd to close xterm's connection.
                  DPRINTF("commlib_to_tty: X11_OPEN_MSG: cannot connect to X server, sending CLOSE\n");
                  char close_buf[2];
                  close_buf[0] = static_cast<char>((conn_id >> 8) & 0xFF);
                  close_buf[1] = static_cast<char>(conn_id & 0xFF);
                  comm_write_message(g_comm_handle, g_hostname, COMM_CLIENT, 1,
                                     (unsigned char *)close_buf, 2, X11_CLOSE_MSG, &err_msg);
                  break;
               }
               pthread_mutex_lock(&g_x11_mutex);
               if (g_x11_fds[conn_id] >= 0) {
                  close(g_x11_fds[conn_id]);
               }
               g_x11_fds[conn_id] = x11_fd;
               pthread_mutex_unlock(&g_x11_mutex);
               // Wake up tty_to_commlib so it picks up the new fd in its select loop.
               wakeup_tty_to_commlib_thread("commlib_to_tty X11_OPEN");
               break;
            }
            case X11_DATA_MSG: {
               // Data from shepherd's X11 relay (job → real X server direction).
               if (recv_mess.cl_message->message_length < 3) {
                  DPRINTF("commlib_to_tty: X11_DATA_MSG too short\n");
                  break;
               }
               int conn_id = (static_cast<unsigned char>(recv_mess.data[0]) << 8)
                           | static_cast<unsigned char>(recv_mess.data[1]);
               if (conn_id < 0 || conn_id >= X11_MAX_CONNS) {
                  DPRINTF("commlib_to_tty: X11_DATA_MSG conn_id %d out of range\n", conn_id);
                  break;
               }
               pthread_mutex_lock(&g_x11_mutex);
               int fd = g_x11_fds[conn_id];
               pthread_mutex_unlock(&g_x11_mutex);
               if (fd >= 0) {
                  int data_len = static_cast<int>(recv_mess.cl_message->message_length) - 3;
                  DPRINTF("commlib_to_tty: X11_DATA_MSG conn_id %d, %d bytes\n", conn_id, data_len);
                  sge_writenbytes(fd, recv_mess.data + 2, data_len);
               }
               break;
            }
            case X11_CLOSE_MSG: {
               // Shepherd closed an X11 connection (job side closed).
               if (recv_mess.cl_message->message_length < 3) {
                  DPRINTF("commlib_to_tty: X11_CLOSE_MSG too short\n");
                  break;
               }
               int conn_id = (static_cast<unsigned char>(recv_mess.data[0]) << 8)
                           | static_cast<unsigned char>(recv_mess.data[1]);
               if (conn_id < 0 || conn_id >= X11_MAX_CONNS) {
                  DPRINTF("commlib_to_tty: X11_CLOSE_MSG conn_id %d out of range\n", conn_id);
                  break;
               }
               DPRINTF("commlib_to_tty: X11_CLOSE_MSG conn_id %d\n", conn_id);
               pthread_mutex_lock(&g_x11_mutex);
               if (g_x11_fds[conn_id] >= 0) {
                  close(g_x11_fds[conn_id]);
                  g_x11_fds[conn_id] = -1;
               }
               pthread_mutex_unlock(&g_x11_mutex);
               break;
            }
            case UNREGISTER_CTRL_MSG:
               /* control message */
               /* the client wants to quit, as this is the last message the client
                * sends, we can be sure to have received all messages from the
                * client. We answer with a UNREGISTER_RESPONSE_CTRL_MSG so
                * the client knows that it can quit now. We can quit, also.
                */
               DPRINTF("commlib_to_tty: received unregister message!\n");
               DPRINTF("commlib_to_tty: writing UNREGISTER_RESPONSE_CTRL_MSG\n");

               /* copy recv_mess.data to buf to append '\0' */
               memcpy(buf, recv_mess.data, std::min(99ul, recv_mess.cl_message->message_length - 1));
               buf[std::min(99ul, recv_mess.cl_message->message_length - 1)] = 0;

               /* the UNREGISTER_CTRL_MSG contains the exit status of the
                * qrsh_starter in case of qrsh <command> and the exit status
                * of the shell for qlogin/qrsh <no command>.
                * If the job was signalled, the exit code is 128+signal.
                */
               sscanf(buf, "%d", &g_exit_status);
               comm_write_message(g_comm_handle, g_hostname, COMM_CLIENT, 1,
                  (unsigned char*)" ", 1, UNREGISTER_RESPONSE_CTRL_MSG, &err_msg);

               DPRINTF("commlib_to_tty: received exit_status from shepherd: %d\n", g_exit_status);
               comm_wait_for_all_messages_sent(g_comm_handle, &err_msg);
               do_exit = 1;
#if 0
               cl_log_list_set_log_level(cl_com_get_log_list(), CL_LOG_OFF);
               cl_com_set_error_func(nullptr);
#endif
               break;
         }
      }
      comm_free_message(&recv_mess, &err_msg);
   }

   thread_func_cleanup(t_conf);
   DPRINTF("commlib_to_tty: exiting commlib_to_tty thread!\n");
   sge_dstring_free(&err_msg);
   DRETURN(nullptr);
}

/**
 * @brief Main loop of the commlib IJS server.
 *
 * Handles data transfer between the qrsh/qlogin client and the shepherd of
 * the interactive job.  Starts the tty_to_commlib and commlib_to_tty worker
 * threads and waits for them to finish.  When @p forward_x11 is true, the
 * MIT-MAGIC-COOKIE-1 is fetched in the main thread before the workers start
 * and the threads handle the X11 forwarding protocol between the client's X
 * server and the job's proxy display on the execution host.
 *
 * @param handle         Handle of the COMM server.
 * @param remote_host    Hostname of the execution host.
 * @param nostdin        Non-zero when the "-nostdin" flag was passed.
 * @param noshell        Non-zero when the "-noshell" flag was passed.
 * @param is_rsh         Non-zero for qrsh with a command line argument.
 * @param is_qlogin      Non-zero for qlogin or qrsh without a command.
 * @param force_pty      Ternary::Yes when the user forced pty via "-pty yes".
 * @param suspend_remote Ternary::Yes to suspend the remote process on Ctrl-Z.
 * @param forward_x11    If true, fetch the MIT-MAGIC-COOKIE-1 for $DISPLAY
 *                       and enable X11 forwarding to the job.
 * @param[out] p_exit_status Exit status of the remote command (128+signal if
 *                           killed by a signal).
 * @param[out] p_err_msg     Error description on failure.
 * @return 0 on success; non-zero error code otherwise:
 *         1 invalid parameter, 2 log list not initialized,
 *         3 terminal mode error, 4 tty_to_commlib thread error,
 *         5 commlib_to_tty thread error, 6 commlib shutdown error,
 *         7 terminal mode reset error.
 * @note MT-NOTE: not MT-safe.
 */
int run_ijs_server(COMM_HANDLE *handle, const char *remote_host, int nostdin, int noshell,
                   int is_rsh, int is_qlogin, const ocs::Ternary force_pty,
                   const ocs::Ternary suspend_remote, int *p_exit_status, dstring *p_err_msg,
                   bool forward_x11, char escape_char)
{
   int               ret = 0, ret_val = 0;
   THREAD_HANDLE     *pthread_tty_to_commlib = nullptr;
   THREAD_HANDLE     *pthread_commlib_to_tty = nullptr;
   THREAD_LIB_HANDLE *thread_lib_handle = nullptr;
   cl_raw_list_t     *cl_com_log_list = nullptr;

   DENTER(TOP_LAYER);

   if (handle == nullptr || p_err_msg == nullptr || p_exit_status == nullptr || remote_host == nullptr) {
      return 1;
   }
   g_comm_handle = handle;
   g_hostname    = strdup(remote_host);

   cl_com_log_list = cl_com_get_log_list();
   if (cl_com_log_list == nullptr) {
      return 2;
   }

   g_nostdin = nostdin;
   g_noshell = noshell;
   g_pid = getpid();
   g_is_rsh = is_rsh;
   g_escape_char = escape_char;
   g_escape_disconnect = false;

   // Initialize X11 forwarding state before starting threads.
   g_forward_x11 = forward_x11;
   g_x11_cookie_hex[0] = '\0';
   for (int i = 0; i < X11_MAX_CONNS; i++) {
      g_x11_fds[i] = -1;
   }
   if (forward_x11) {
      const char *display = getenv("DISPLAY");
      if (display != nullptr) {
         snprintf(g_x11_display, sizeof(g_x11_display), "%s", display);
         // Fetch the cookie now, in the main thread, before worker threads start.
         // This avoids popen() being called from a worker thread where SIGCHLD
         // masks inherited from the parent may differ.
         if (!x11_get_cookie(g_x11_display, g_x11_cookie_hex, sizeof(g_x11_cookie_hex))) {
            g_x11_cookie_hex[0] = '\0';
            fprintf(stderr, "Warning: X11 forwarding requested but xauth failed for DISPLAY=%s; forwarding disabled\n",
                    g_x11_display);
         }
      } else {
         g_x11_display[0] = '\0';
         DPRINTF("run_ijs_server: -X given but DISPLAY is not set; X11 forwarding unavailable\n");
      }
   } else {
      g_x11_display[0] = '\0';
   }

   if (suspend_remote == ocs::Ternary::Unset || suspend_remote == ocs::Ternary::No) {
      g_suspend_remote = 0;
   } else {
      g_suspend_remote = 1;
   }

   /*
    * qrsh without command and qlogin both have is_rsh == 0 and is_qlogin == 1
    * qrsh with command and qsh don't need to set terminal mode.
    * If the user requested a pty we also have to set terminal mode.
    * But only if stdout is still connected to a tty and not redirected
    * to a file or a pipe.
    */
   if (isatty(STDOUT_FILENO) == 1 &&
      ((force_pty == ocs::Ternary::Unset && is_rsh == 0 && is_qlogin == 1) || force_pty == ocs::Ternary::No)) {
      /*
       * Set this terminal to raw mode, just output everything, don't interpret
       * it. Let the pty on the client side interpret the characters.
       */
      ret = terminal_enter_raw_mode();
      if (ret != 0) {
         sge_dstring_sprintf(p_err_msg, "can't set terminal to raw mode: %s (%d)",
            strerror(ret), ret);
         return 3;
      } else {
        g_raw_mode_state = 1;
      }
   }

   // Create a pipe to wake up the tty_to_commlib thread we start below.
   // It needs to be woken up in two situations:
   // 1. It shall only start reading from stdin and sending this data once a client (sge_shepherd)
   //    connected and did the first protocol steps. It waits for this state by reading from the wakeup pipe.
   // 2. Once the client is connected the tty_to_commlib thread waits for data from stdin and on the wakeup pipe.
   //    This allows us to wake it up (for termination) when the client disconnected.
   if (pipe(g_wakeup_pipe) < 0) {
      DPRINTF("tty_to_commlib: pipe() failed: %s\n", strerror(errno));
      // @todo we should return here, no use to continue
   }

   /*
    * Setup thread list and create two worker threads
    */
   thread_init_lib(&thread_lib_handle);
   /*
    * From here on, we have to clean up the list in case of errors, this is
    * why we "goto cleanup" in case of error.
    */

   DPRINTF("creating worker threads\n");
   DPRINTF("creating tty_to_commlib thread\n");
   ret = create_thread(thread_lib_handle, &pthread_tty_to_commlib, cl_com_log_list,
      "tty_to_commlib thread", 1, tty_to_commlib);
   if (ret != CL_RETVAL_OK) {
      sge_dstring_sprintf(p_err_msg, "can't create tty_to_commlib thread: %s",
         cl_get_error_text(ret));
      ret_val = 4;
      goto cleanup;
   }

   DPRINTF("creating commlib_to_tty thread\n");
   ret = create_thread(thread_lib_handle, &pthread_commlib_to_tty, cl_com_log_list,
      "commlib_to_tty thread", 1, commlib_to_tty);
   if (ret != CL_RETVAL_OK) {
      sge_dstring_sprintf(p_err_msg, "can't create commlib_to_tty thread: %s",
         cl_get_error_text(ret));
      ret_val = 5;
      goto cleanup;
   }

   /*
    * From here on, the two worker threads are doing all the work.
    * This main thread is just waiting until the client closes the
    * connection to us, which causes the commlib_to_tty thread to
    * exit. Then it closes the tty_to_commlib thread, too, and
    * cleans up everything.
    */
   DPRINTF("waiting for end of commlib_to_tty thread\n");
   thread_join(pthread_commlib_to_tty);

   DPRINTF("shutting down tty_to_commlib thread\n");
   g_do_exit = true;
   thread_shutdown(pthread_tty_to_commlib);

   wakeup_tty_to_commlib_thread("run_ijs_server");

   DPRINTF("waiting for end of tty_to_commlib thread\n");
   thread_join(pthread_tty_to_commlib);
   DPRINTF("tty_to_commlib thread terminated\n");
cleanup:
   /*
    * Set our terminal back to 'unraw' mode. Should be done automatically
    * by OS on process end, but we want to be sure.
    */
   ret = terminal_leave_raw_mode();
   DPRINTF("terminal_leave_raw_mode() returned %s (%d)\n", strerror(ret), ret);
   if (ret != 0) {
      sge_dstring_sprintf(p_err_msg, "error resetting terminal mode: %s (%d)", strerror(ret), ret);
      ret_val = 7;
   }

   *p_exit_status = g_exit_status;

   // Close any still-open X11 server connections.
   if (g_forward_x11) {
      pthread_mutex_lock(&g_x11_mutex);
      for (int i = 0; i < X11_MAX_CONNS; i++) {
         if (g_x11_fds[i] >= 0) {
            close(g_x11_fds[i]);
            g_x11_fds[i] = -1;
         }
      }
      pthread_mutex_unlock(&g_x11_mutex);
   }

   thread_cleanup_lib(&thread_lib_handle);
   DRETURN(ret_val);
}

/**
 * @brief Start the commlib server for builtin interactive job support (IJS).
 *
 * Opens the commlib server endpoint used for the connection between the
 * qrsh/qlogin client and the shepherd of the interactive job.  Over this
 * connection stdin/stdout/stderr and X11 forwarding data are transferred.
 *
 * When @p port_range is nullptr or empty the OS assigns an ephemeral port.
 * Otherwise the function iterates through the RN_Type range list (which may
 * contain multiple min-max[:step] sub-ranges from the mconf @c port_range
 * setting) and binds to the first available port, allowing administrators to
 * restrict qrsh to a firewall-friendly port range.
 *
 * @param communication_framework Communication framework; controls whether
 *                                 TLS (CSP) is used.
 * @param hostname                 Hostname of the execution host the shepherd
 *                                 will connect back to.
 * @param username                 Owner of the TLS certificates; used only
 *                                 when CSP mode is active, otherwise ignored.
 * @param port_range               RN_Type CULL range list of ports to try, or
 *                                 nullptr/empty for an OS-assigned ephemeral
 *                                 port.  Supports N-M and N-M:S (step) syntax.
 * @param[out] phandle             Receives the initialized COMM server handle.
 * @param[out] p_err_msg           Error description on failure.
 * @return 0 on success;
 *         1 if no connection could be opened (no port available in range);
 *         2 if connection parameters could not be set.
 * @note MT-NOTE: not MT-safe.
 */
int start_ijs_server(cl_framework_t communication_framework, const char *hostname,
                     const char* username, const lList *port_range,
                     COMM_HANDLE **phandle, dstring *p_err_msg)
{
   int  ret;
   int  ret_val   = 0;
   bool use_range = (port_range != nullptr && lGetNumberOfElem(port_range) > 0);

   DENTER(TOP_LAYER);
   DPRINTF("starting commlib server\n");

   if (!use_range) {
      // OS-assigned port
      ret = comm_open_connection(true, communication_framework, COMM_SERVER, 0, COMM_CLIENT,
                                 hostname, username, phandle, p_err_msg);
      if (ret != 0 || *phandle == nullptr) {
         ret_val = 1;
      }
   } else {
      // iterate through port_range; bind to first available port
      bool found = false;
      for (const lListElem *rep = lFirst(port_range); rep && !found; rep = lNext(rep)) {
         const uint32_t min  = lGetUlong(rep, RN_min);
         const uint32_t max  = lGetUlong(rep, RN_max);
         uint32_t step = lGetUlong(rep, RN_step);
         if (step == 0) {
            step = 1;
         }
         for (uint32_t candidate = min; candidate <= max && !found; candidate += step) {
            *phandle = nullptr;
            ret = comm_open_connection(true, communication_framework, COMM_SERVER,
                                       static_cast<int>(candidate), COMM_CLIENT,
                                       hostname, username, phandle, p_err_msg);
            if (ret == 0 && *phandle != nullptr) {
               found = true;
            }
         }
      }
      if (!found) {
         ret_val = 1;
      }
   }

   if (ret_val == 0) {
      ret = comm_set_connection_param(*phandle, HEARD_FROM_TIMEOUT, 0, p_err_msg);
      if (ret != 0) {
         ret_val = 2;
      }
   }

   DRETURN(ret_val);
}

/****** sge_client_ijs/stop_ijs_server() ***************************************
*  NAME
*     stop_ijs_server() -- stops the commlib server for the builtin
*                          interactive job support
*
*  SYNOPSIS
*     int stop_ijs_server(COMM_HANDLE **phandle, dstring *p_err_msg)
*
*  FUNCTION
*     Stops the commlib server for the commlib connection between the shepherd
*     of the interactive job (qrsh/qlogin) and the qrsh/qlogin command.
*     Over this connectin the stdin/stdout/stderr input/output is transferred.
*
*  INPUTS
*     COMM_HANDLE **phandle - Pointer to the COMM server handle. Gets set to
*                             nullptr in this function.
*     dstring *p_err_msg    - Contains the error reason in case of error.
*
*  RESULT
*     int - 0: OK
*           1: Invalid Parameter: phandle = nullptr
*           2: General error shutting down the COMM server,
*              see p_err_msg for details
*
*  NOTES
*     MT-NOTE: stop_ijs_server() is not MT safe
*
*  SEE ALSO
*     sge_client_ijs/start_ijs_server()
*     sge_client_ijs/run_ijs_server()
*     sge_client_ijs/force_ijs_server_shutdown()
*******************************************************************************/
int stop_ijs_server(COMM_HANDLE **phandle, dstring *p_err_msg)
{
   int ret = 0;

   DENTER(TOP_LAYER);

   if (phandle == nullptr) {
      ret = 1;
   } else if (*phandle != nullptr) {
      cl_com_set_error_func(nullptr);
#if 0
      cl_log_list_set_log_level(cl_com_get_log_list(), CL_LOG_OFF);
#endif
      cl_com_ignore_timeouts(true);
      DPRINTF("shut down the connection from our side\n");
      ret = cl_commlib_shutdown_handle(*phandle, false);
      if (ret != CL_RETVAL_OK) {
         sge_dstring_sprintf(p_err_msg, "error shutting down the connection: %s",
            cl_get_error_text(ret));
         ret = 2;
      }
      *phandle = nullptr;
   }
   DRETURN(ret);
}

/****** sge_client_ijs/force_ijs_server_shutdown() *****************************
*  NAME
*     force_ijs_server_shutdown() -- forces the commlib server for the builtin
*                                    interactive job support to shut down
*
*  SYNOPSIS
*     int force_ijs_server_shutdown(COMM_HANDLE **phandle, const char
*     *this_component, dstring *p_err_msg)
*
*  FUNCTION
*     Forces the commlib server for the builtin interactive job support to shut
*     down immediately and ensures it is shut down.
*
*  INPUTS
*     COMM_HANDLE **phandle      - Handle of the COMM connection, gets set to
*                                  nullptr in this function.
*     const char *this_component - Name of this component.
*     dstring *p_err_msg         - Contains the error reason in case of error.
*
*  RESULT
*     int - 0: OK
*           1: Invalid parameter: phandle == nullptr or *phandle == nullptr
*           2: Can't shut down connection, see p_err_msg for details
*
*  NOTES
*     MT-NOTE: force_ijs_server_shutdown() is not MT safe
*
*  SEE ALSO
*     sge_client_ijs/start_ijs_server()
*     sge_client_ijs/run_ijs_server()
*     sge_client_ijs/stop_ijs_server_shutdown()
*******************************************************************************/
int force_ijs_server_shutdown(COMM_HANDLE **phandle,
                              const char *this_component,
                              dstring *p_err_msg)
{
   int     ret;

   DENTER(TOP_LAYER);

   if (phandle == nullptr || *phandle == nullptr) {
      sge_dstring_sprintf(p_err_msg, "invalid connection handle");
      DPRINTF("invalid connection handle - nothing to shut down\n");
      DRETURN(1);
   }

   DPRINTF("connection is still alive\n");

   /* This will remove the handle */
   ret = comm_shutdown_connection(*phandle, COMM_CLIENT, g_hostname, p_err_msg);
   sge_free(&g_hostname);
   if (ret != COMM_RETVAL_OK) {
      DPRINTF("comm_shutdown_connection() failed: %s (%d)\n", sge_dstring_get_string(p_err_msg), ret);
      ret = 2;
   } else {
      DPRINTF("successfully shut down the connection\n");
   }
   *phandle = nullptr;

   DRETURN(ret);
}

