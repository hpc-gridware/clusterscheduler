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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "cull/pack.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/types.h>
#include <netinet/in.h>
#include <rpc/rpc.h>
#include <rpc/types.h>

#if defined(LINUXRISCV64)
#  include <rpc/xdr.h>
#endif

#if defined(SOLARIS) || defined(DARWIN)
#define htobe64(x) htonll(x)
#define be64toh(x) ntohll(x)
#endif

/* do not compile in monitoring code */
#ifndef NO_SGE_COMPILE_DEBUG
#define NO_SGE_COMPILE_DEBUG
#endif

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"

#include "cull/msg_cull.h"

#include "basis_types.h"

#if 0
#   undef PACK_LAYER
#   define PACK_LAYER BASIS_LAYER
#endif


/****** cull/pack/--CULL_Packing ***************************************
*
*  NAME
*     CULL_Packing -- platform independent exchange format
*
*  FUNCTION
*     The cull packing functions provide a framework for a 
*     platform independent data representation.
*
*     Data is written into a packbuffer. Individual data words
*     are written in network byte order.
*
*     Data in the packbuffer can be compressed.
*
*  NOTES
*     Other platform independent formats, like XML, should be 
*     implemented.
*
*  SEE ALSO
*     cull/pack/-Versioncontrol
****************************************************************************
*/

/* -------------------------------

   get chunk_size

 */
int pack_get_chunk() {
   return CHUNK;
}

/****** cull/pack/init_packbuffer() *******************************************
*  NAME
*     init_packbuffer() -- initialize packing buffer
*
*  SYNOPSIS
*     int init_packbuffer(sge_pack_buffer *pb, int initial_size, 
*                         bool just_count, bool with_auth_info)
*
*  FUNCTION
*     Initialize a packing buffer.
*     Allocates the necessary memory. If more memory is needed during the use
*     of the packbuffer, it will be reallocated increasing the size by 
*     chunk_size (see function pack_set_chunk).
*
*     Since version 6.0, version information is provided in the packbuffer and 
*     is included in sent messages.
*     For best possible backward interoperability with former versions, an 
*     integer with value 0 is padded before the version information as first 
*     word in the packbuffer. This triggeres error handling in former versions.
*
*     Functions using packing buffers in GDI or related code should use the 
*     function ocs::gdi::Client::sge_gdi_packet_get_pb_size() to find the correct
*     "initial_size".  
*
*  INPUTS
*     sge_pack_buffer *pb - the packbuffer to initialize
*     int initial_size    - the amount of memory to be allocated at 
*                           initialization.
*                           If a value of 0 is given as initial_size, a size 
*                           of chunk_size (global variable, see function 
*                           pack_set_chunk) will be used.
*     bool just_count     - if true, no memory will be allocated and the
*                           "just_count" property of the packbuffer will 
*                           be set.
*     bool with_auth_info - if true, the packbuffer will be initialized with
*                           authentication information (user and group information)
*                           This is the case for all pack buffers which are used for
*                           communication between components.
*                           For spooling operations no auth_info is needed.
*
*  RESULT
*     int - PACK_SUCCESS on success
*           PACK_ENOMEM  if memory allocation fails
*           PACK_FORMAT  if no valid packbuffer is passed
*
*  NOTES
*     MT-NOTE: init_packbuffer() is MT safe (assumptions)
*  
*  SEE ALSO
*     cull/pack/-Packing-typedefs
*     cull/pack/pack_set_chunk()
*     gdi/request_internal/sge_gdi_packet_get_pb_size()
*******************************************************************************/
int
init_packbuffer(sge_pack_buffer *pb, size_t initial_size, bool just_count, bool with_auth_info) {
   DENTER(PACK_LAYER);

   if (pb == nullptr) {
      ERROR(MSG_CULL_ERRORININITPACKBUFFER_S, MSG_CULL_PACK_FORMAT);
      DRETURN(PACK_FORMAT);
   }

   // clear all data
   memset(pb, 0, sizeof(sge_pack_buffer));

   if (!just_count) {
      if (initial_size == 0) {
         initial_size = CHUNK;
      } else {
         initial_size += 2 * INTSIZE;  /* space for version information */
      }

      // if auth_info is requested then get it from the component module
      // and add its length to the initial_size
      if (with_auth_info) {
         pb->auth_info = component_get_auth_info();
         if (pb->auth_info == nullptr) {
            // ERROR printing has been done in component_get_auth_info()
            DRETURN(PACK_AUTHINFO);
         }
      }
      if (pb->auth_info != nullptr) {
         initial_size += strlen(pb->auth_info) + 1;
      } else {
         initial_size += 1;
      }

      pb->head_ptr = sge_malloc(initial_size);
      if (pb->head_ptr == nullptr) {
         ERROR(MSG_CULL_NOTENOUGHMEMORY_D, (int)initial_size);
         DRETURN(PACK_ENOMEM);
      }
      pb->cur_ptr = pb->head_ptr;
      pb->mem_size = initial_size;

      pb->cur_ptr = pb->head_ptr;
      pb->mem_size = initial_size;

      pb->bytes_used = 0;
      pb->just_count = false;
      pb->version = CULL_VERSION;
      packint(pb, 0);              /* pad 0 byte -> error handing in former versions */
      packint(pb, pb->version);      /* version information is included in buffer      */
      packstr(pb, pb->auth_info);
   } else {
      pb->just_count = true;
   }

   DRETURN(PACK_SUCCESS);
}

/**************************************************************
 initialize packing buffer out of a normal character buffer
 set read/write pointer to the beginning

 NOTES
    MT-NOTE: init_packbuffer_from_buffer() is MT safe (assumptions)
 **************************************************************/
int
init_packbuffer_from_buffer(sge_pack_buffer *pb, char *buf, u_long32 buflen, bool with_auth_info) {
   DENTER(PACK_LAYER);

   if (!pb && !buf) {
      DRETURN(PACK_FORMAT);
   }

   memset(pb, 0, sizeof(sge_pack_buffer));

   pb->head_ptr = buf;
   pb->cur_ptr = buf;
   pb->mem_size = buflen;
   pb->bytes_used = 0;

   /* check cull version (only if buffer contains any data) */
   if (buflen > 0) {
      int ret;

      u_long32 pad;
      if ((ret = unpackint(pb, &pad)) != PACK_SUCCESS) {
         DRETURN(ret);
      }

      u_long32 version;
      if ((ret = unpackint(pb, &version)) != PACK_SUCCESS) {
         DRETURN(ret);
      }

      char *auth_info;
      if ((ret = unpackstr(pb, &auth_info)) != PACK_SUCCESS) {
         DRETURN(ret);
      }
      pb->auth_info = auth_info;

      if (pad != 0 || version != CULL_VERSION) {
         ERROR(MSG_CULL_PACK_WRONG_VERSION_XX, (unsigned int) version, CULL_VERSION);
         DRETURN(PACK_VERSION);
      }

      pb->version = version;

      // verify auth_info
      if (with_auth_info) {
         DSTRING_STATIC(error_dstr, MAX_STRING_SIZE);
         if (!component_parse_auth_info(&error_dstr, pb->auth_info, &pb->uid, pb->username, MAX_USER_GROUP,
            &pb->gid, pb->groupname, MAX_USER_GROUP, &pb->grp_amount, &pb->grp_array)) {
            ERROR(SFNMAX, sge_dstring_get_string(&error_dstr));
            DRETURN(PACK_AUTHINFO);
         }
      }
   } else {
      pb->version = CULL_VERSION;
   }

   DRETURN(PACK_SUCCESS);
}

/**************************************************************/
/* MT-NOTE: clear_packbuffer() is MT safe */
void clear_packbuffer(sge_pack_buffer *pb) {
   if (pb != nullptr) {
      sge_free(&pb->head_ptr);
      sge_free(&pb->auth_info);
      sge_free(&pb->grp_array);
   }
}

/*************************************************************
 look whether pb is filled
 *************************************************************/
int pb_filled(
        sge_pack_buffer *pb
) {
   /* do we have more bytes used than the version information? */
   return (pb_used(pb) > (2 * INTSIZE));
}

/*************************************************************
 look for the number of bytes that are unused
 i.e. that are not yet consumed
 *************************************************************/
int pb_unused(
        sge_pack_buffer *pb
) {
   return (pb->mem_size - pb->bytes_used);
}

/*************************************************************
 look for the number of bytes that are used
 *************************************************************/
int pb_used(
        sge_pack_buffer *pb
) {
   return pb->bytes_used;
}

/* --------------------------------------------------------- */
/*                                                           */
/*            low level functions for packing                */
/*                                                           */
/* --------------------------------------------------------- */

/*
   return values:
   PACK_SUCCESS
   PACK_ENOMEM
   PACK_FORMAT
 */
int packint(sge_pack_buffer *pb, u_long32 i) {
   u_long32 J = 0;

   DENTER(PACK_LAYER);

   if (!pb->just_count) {
      if (pb->bytes_used + INTSIZE > pb->mem_size) {
         DPRINTF("realloc(%d + %d)\n", pb->mem_size, CHUNK);
         pb->mem_size += CHUNK;
         pb->head_ptr = (char *) sge_realloc(pb->head_ptr, pb->mem_size, 0);
         if (!pb->head_ptr) {
            DRETURN(PACK_ENOMEM);
         }
         pb->cur_ptr = &(pb->head_ptr[pb->bytes_used]);
      }

      /* copy in packing buffer */
      J = htonl(i);
      memcpy(pb->cur_ptr, (((char *) &J) + INTOFF), INTSIZE);
      pb->cur_ptr = &(pb->cur_ptr[INTSIZE]);
   }
   pb->bytes_used += INTSIZE;

   DRETURN(PACK_SUCCESS);
}

int repackint(sge_pack_buffer *pb, u_long32 i) {
   u_long32 J = 0;

   DENTER(PACK_LAYER);

   if (!pb->just_count) {
      J = htonl(i);
      memcpy(pb->cur_ptr, (((char *) &J) + INTOFF), INTSIZE);
      pb->cur_ptr = &(pb->cur_ptr[INTSIZE]);
   }

   DRETURN(PACK_SUCCESS);
}

int packint64(sge_pack_buffer *pb, u_long64 i) {
   u_long64 J = 0;

   DENTER(PACK_LAYER);

   if (!pb->just_count) {
      if (pb->bytes_used + (INTSIZE * 2) > pb->mem_size) {
         DPRINTF("realloc(%d + %d)\n", pb->mem_size, CHUNK);
         pb->mem_size += CHUNK;
         pb->head_ptr = (char *) sge_realloc(pb->head_ptr, pb->mem_size, 0);
         if (!pb->head_ptr) {
            DRETURN(PACK_ENOMEM);
         }
         pb->cur_ptr = &(pb->head_ptr[pb->bytes_used]);
      }

      /* copy in packing buffer */
      J = htobe64(i);
      memcpy(pb->cur_ptr, (((char *) &J) + INTOFF), (INTSIZE * 2));
      pb->cur_ptr = &(pb->cur_ptr[(INTSIZE * 2)]);
   }
   pb->bytes_used += (INTSIZE * 2);

   DRETURN(PACK_SUCCESS);
}

#define DOUBLESIZE 8

/*
   return values:
   PACK_SUCCESS
   PACK_ENOMEM
   PACK_FORMAT
 */
int packdouble(sge_pack_buffer *pb, double d) {
/* CygWin does not know RPC u. XDR */
   char buf[32];
   XDR xdrs;

   DENTER(PACK_LAYER);

   if (!pb->just_count) {
      if (pb->bytes_used + DOUBLESIZE > pb->mem_size) {
         DPRINTF("realloc(%d + %d)\n", pb->mem_size, CHUNK);
         pb->mem_size += CHUNK;
         pb->head_ptr = (char *) sge_realloc(pb->head_ptr, pb->mem_size, 0);
         if (!pb->head_ptr) {
            DRETURN(PACK_ENOMEM);
         }
         pb->cur_ptr = &(pb->head_ptr[pb->bytes_used]);
      }

      /* copy in packing buffer */
      xdrmem_create(&xdrs, (caddr_t) buf, sizeof(buf), XDR_ENCODE);

      if (!(xdr_double(&xdrs, &d))) {
         DPRINTF("error - XDR of double failed\n");
         xdr_destroy(&xdrs);
         DRETURN(PACK_FORMAT);
      }

      if (xdr_getpos(&xdrs) != DOUBLESIZE) {
         DPRINTF("error - size of XDRed double is %d\n", xdr_getpos(&xdrs));
         xdr_destroy(&xdrs);
         DRETURN(PACK_FORMAT);
      }

      memcpy(pb->cur_ptr, buf, DOUBLESIZE);
      pb->cur_ptr = &(pb->cur_ptr[DOUBLESIZE]);

      xdr_destroy(&xdrs);
   }
   pb->bytes_used += DOUBLESIZE;

   DRETURN(PACK_SUCCESS);
}

/* ---------------------------------------------------------

   return values:
   PACK_SUCCESS
   PACK_ENOMEM
   PACK_FORMAT

 */
int packstr(sge_pack_buffer *pb, const char *str) {
   DENTER(PACK_LAYER);

   /* determine string length */
   if (str == nullptr) {
      if (!pb->just_count) {
         /* is realloc necessary */
         if ((pb->bytes_used + 1) > pb->mem_size) {

            /* realloc */
            DPRINTF("realloc(%d + %d)\n", pb->mem_size, CHUNK);
            pb->mem_size += CHUNK;
            pb->head_ptr = (char *) sge_realloc(pb->head_ptr, pb->mem_size, 0);
            if (!pb->head_ptr) {
               DRETURN(PACK_ENOMEM);
            }

            /* update cur_ptr */
            pb->cur_ptr = &(pb->head_ptr[pb->bytes_used]);
         }
         pb->cur_ptr[0] = '\0';
         /* update cur_ptr & bytes_packed */
         pb->cur_ptr = &(pb->cur_ptr[1]);
      }
      pb->bytes_used++;
   } else {
      size_t n = strlen(str) + 1;

      if (!pb->just_count) {
         /* is realloc necessary */
         if ((pb->bytes_used + n) > pb->mem_size) {
            /* realloc */
            DPRINTF("realloc(%d + %d)\n", pb->mem_size, CHUNK);
            while ((pb->bytes_used + n) > pb->mem_size)
               pb->mem_size += CHUNK;
            pb->head_ptr = (char *) sge_realloc(pb->head_ptr, pb->mem_size, 0);
            if (!pb->head_ptr) {
               DRETURN(PACK_ENOMEM);
            }

            /* update cur_ptr */
            pb->cur_ptr = &(pb->head_ptr[pb->bytes_used]);
         }
         memcpy(pb->cur_ptr, str, n);
         /* update cur_ptr & bytes_packed */
         pb->cur_ptr = &(pb->cur_ptr[n]);
      }
      pb->bytes_used += n;
   }

   DRETURN(PACK_SUCCESS);
}

/* ---------------------------------------------------------

   return values:
   PACK_SUCCESS
   PACK_ENOMEM
   PACK_FORMAT

 */
int packbuf(
        sge_pack_buffer *pb,
        const char *buf_ptr,
        u_long32 buf_size
) {

   DENTER(PACK_LAYER);

   if (!pb->just_count) {
      /* is realloc necessary */
      if (buf_size + (u_long32) pb->bytes_used > (u_long32) pb->mem_size) {
         /* realloc */
         DPRINTF("realloc(%d + %d)\n", pb->mem_size, CHUNK);
         pb->mem_size += CHUNK;
         pb->head_ptr = (char *) sge_realloc(pb->head_ptr, pb->mem_size, 0);
         if (!(pb->head_ptr)) {
            DRETURN(PACK_ENOMEM);
         }

         /* update cur_ptr */
         pb->cur_ptr = &(pb->head_ptr[pb->bytes_used]);
      }

      /* copy in packing buffer */
      memcpy(pb->cur_ptr, buf_ptr, buf_size);
      /* update cur_ptr & bytes_packed */
      pb->cur_ptr = &(pb->cur_ptr[buf_size]);
   }
   pb->bytes_used += buf_size;

   DRETURN(PACK_SUCCESS);
}


/* --------------------------------------------------------- */
/*                                                           */
/*            low level functions for unpacking              */
/*                                                           */
/* --------------------------------------------------------- */

/* ---------------------------------------------------------

   return values:
   PACK_SUCCESS
   (PACK_ENOMEM)
   PACK_FORMAT

 */
int unpackint(sge_pack_buffer *pb, u_long32 *ip) {
   DENTER(PACK_LAYER);

   /* are there enough bytes ? */
   if (pb->bytes_used + INTSIZE > pb->mem_size) {
      *ip = 0;
      DRETURN(PACK_FORMAT);
   }

   /* copy integer */
   memset(ip, 0, sizeof(u_long32));
   memcpy(((char *) ip) + INTOFF, pb->cur_ptr, INTSIZE);
   *ip = ntohl(*ip);

   /* update cur_ptr & bytes_unpacked */
   pb->cur_ptr = &(pb->cur_ptr[INTSIZE]);
   pb->bytes_used += INTSIZE;

   DRETURN(PACK_SUCCESS);
}

/* ---------------------------------------------------------

   return values:
   PACK_SUCCESS
   (PACK_ENOMEM)
   PACK_FORMAT

 */
int unpackint64(sge_pack_buffer *pb, u_long64 *ip) {
   DENTER(PACK_LAYER);

   /* are there enough bytes ? */
   if (pb->bytes_used + (INTSIZE * 2) > pb->mem_size) {
      *ip = 0;
      DRETURN(PACK_FORMAT);
   }

   /* copy integer */
   memset(ip, 0, sizeof(u_long64));
   memcpy(((char *) ip) + INTOFF, pb->cur_ptr, (INTSIZE * 2));
   *ip = be64toh(*ip);

   /* update cur_ptr & bytes_unpacked */
   pb->cur_ptr = &(pb->cur_ptr[(INTSIZE * 2)]);
   pb->bytes_used += (INTSIZE * 2);

   DRETURN(PACK_SUCCESS);
}

/* ---------------------------------------------------------

   return values:
   PACK_SUCCESS
   (PACK_ENOMEM)
   PACK_FORMAT

 */
int unpackdouble(sge_pack_buffer *pb, double *dp) {
   XDR xdrs;
   char buf[32];

   DENTER(PACK_LAYER);

   /* are there enough bytes ? */
   if (pb->bytes_used + DOUBLESIZE > pb->mem_size) {
      *dp = 0;
      DRETURN(PACK_FORMAT);
   }

   /* copy double */

   /* CygWin does not know RPC u. XDR */
#if !defined(WIN32)                   /* XDR not called */
   memcpy(buf, pb->cur_ptr, DOUBLESIZE);
   xdrmem_create(&xdrs, buf, DOUBLESIZE, XDR_DECODE);
   if (!(xdr_double(&xdrs, dp))) {
      *dp = 0;
      DPRINTF("error unpacking XDRed double\n");
      xdr_destroy(&xdrs);
      DRETURN(PACK_FORMAT);
   }
#endif /* WIN32 */

   /* update cur_ptr & bytes_unpacked */
   pb->cur_ptr = &(pb->cur_ptr[DOUBLESIZE]);
   pb->bytes_used += DOUBLESIZE;

#if !defined(WIN32)                   /* XDR not called */
   xdr_destroy(&xdrs);
#endif /* WIN32 */

   DRETURN(PACK_SUCCESS);
}

/* ---------------------------------------------------------

   return values:
   PACK_SUCCESS
   PACK_ENOMEM
   PACK_FORMAT
 */
int unpackstr(sge_pack_buffer *pb, char **str) {
   u_long32 n;

   DENTER(PACK_LAYER);

   /* determine string length */
   if (!pb->cur_ptr[0]) {

      *str = nullptr;

      /* update cur_ptr & bytes_unpacked */
      pb->cur_ptr = &(pb->cur_ptr[1]);
      pb->bytes_used++;

      /* are there enough bytes ? */
      if (pb->bytes_used > pb->mem_size) {
         DRETURN(PACK_FORMAT);
      }

      DRETURN(PACK_SUCCESS);
   } else {
      n = strlen(pb->cur_ptr) + 1;

      /* are there enough bytes ? */
      if (n + pb->bytes_used > pb->mem_size) {
         DRETURN(PACK_FORMAT);
      }
      *str = strdup(pb->cur_ptr);
      if (!*str) {
         DRETURN(PACK_ENOMEM);
      }
      /* update cur_ptr & bytes_unpacked */
      pb->bytes_used += n;
      pb->cur_ptr = &(pb->cur_ptr[n]);
   }

   DRETURN(PACK_SUCCESS);
}

/* ---------------------------------------------------------

   return values:
   PACK_SUCCESS
   PACK_ENOMEM
   PACK_FORMAT
 */
int unpackbuf(sge_pack_buffer *pb, char **buf_ptr, int buf_size) {

   DENTER(PACK_LAYER);

   if (buf_size == 0) {
      DRETURN(PACK_SUCCESS);
   }

   /* are there enough bytes ? */
   if ((buf_size + pb->bytes_used) > pb->mem_size) {
      DRETURN(PACK_FORMAT);
   }

   /* copy buffer */
   *buf_ptr = sge_malloc(buf_size);
   if (!*buf_ptr) {
      DRETURN(PACK_ENOMEM);
   }
   memcpy(*buf_ptr, pb->cur_ptr, buf_size);
   /* update cur_ptr & bytes_unpacked */
   pb->cur_ptr = &(pb->cur_ptr[buf_size]);
   pb->bytes_used += buf_size;

   DRETURN(PACK_SUCCESS);
}

const char *cull_pack_strerror(int errnum) {
   switch (errnum) {
      case PACK_SUCCESS:
         return MSG_CULL_PACK_SUCCESS;
      case PACK_ENOMEM:
         return MSG_CULL_PACK_ENOMEM;
      case PACK_FORMAT:
         return MSG_CULL_PACK_FORMAT;
      case PACK_BADARG:
         return MSG_CULL_PACK_BADARG;
      case PACK_VERSION:
         return MSG_CULL_PACK_VERSION;
      case PACK_AUTHINFO:
         return MSG_CULL_PACK_AUTHINFO;
      default:
         /* JG: TODO: we should have some global error message for all strerror functions */
         return "";
   }
}

/****** cull/pack/pb_are_equivalent() *****************************************
*  NAME
*     pb_are_equivalent() -- check if both buffers are equivalent 
*
*  SYNOPSIS
*     bool pb_are_equivalent(sge_pack_buffer *pb1, sge_pack_buffer *pb2) 
*
*  FUNCTION
*     Check if size and content of both packbuffers is equivalent 
*
*  INPUTS
*     sge_pack_buffer *pb1 - packbuffer 
*     sge_pack_buffer *pb2 - packbuffer 
*
*  RESULT
*     bool - equivalent?
*        true  - yes
*        false - no
*******************************************************************************/
bool
pb_are_equivalent(sge_pack_buffer *pb1, sge_pack_buffer *pb2) {
   bool ret = true;

   if (pb1 != nullptr && pb2 != nullptr) {
      ret &= (pb1->bytes_used == pb2->bytes_used);
      ret &= (memcmp(pb1->head_ptr, pb2->head_ptr, pb1->bytes_used) == 0);
   }
   return ret;
}

/****** cull/pack/pb_print_to() ***********************************************
*  NAME
*     pb_print_to() -- Print content of packbuffer 
*
*  SYNOPSIS
*     void pb_print_to(sge_pack_buffer *pb, FILE* file) 
*
*  FUNCTION
*     Print content of packbuffer into file 
*
*  INPUTS
*     sge_pack_buffer *pb - packbuffer pointer 
*     bool only_header    - show only summary information 
*     FILE* file          - file stream (e.g. stderr) 
*
*  RESULT
*     void - NONE
*******************************************************************************/
void
pb_print_to(sge_pack_buffer *pb, bool only_header, FILE *file) {
   size_t i;

   fprintf(file, "head_ptr: %p\n", (void *)pb->head_ptr);
   fprintf(file, "cur_ptr: %p\n", (void *)pb->cur_ptr);
   fprintf(file, "mem_size: %d\n", (int) pb->mem_size);
   fprintf(file, "bytes_used: %d\n", (int) pb->bytes_used);
   fprintf(file, "buffer:\n");
   if (!only_header) {
      for (i = 0; i < pb->bytes_used; i++) {
         fprintf(file, "%3d ", pb->head_ptr[i]);
         if ((i + 1) % 15 == 0) {
            fprintf(file, "\n");
         }
      }
      fprintf(file, "\n");
   }
}

/**
 * @brief re-resolve and check user information
 *
 * The function can do multiple verifications on user data and optionally do re-resolving.
 * If local_uid_gid is true then we expect uid and gid of the request to match the uid and gid of the current
 * process. This is done to ensure daemon to daemon communication is coming from the right user.
 * If reresolve_user is true then user information will be re-resolved. If the re-resolved information differs from
 * the data in the request then it is corrected and an informational message is returned in the error dstring.
 * If reresolve_supp_grp is true the the supplementary groups will be re-resolved.
 * Errors happening during the operations described above will be reported in the error dstring
 * and false will be returned.
 *
 * @param pb pack buffer with usage information
 * @param error dstring for reporting errors
 * @param local_uid_gid only accept requests with the same uid and gid?
 * @param reresolve_user  re-resolve and correct the user and group name?
 * @param reresolve_supp_grp  re-resolve the supplementary group ids?
 * @return true if all was ok, possibly names were corrected, false on errors
 */
bool
cull_reresolve_check_user(sge_pack_buffer *pb, dstring *error, bool local_uid_gid, bool reresolve_user, bool reresolve_supp_grp) {
   DENTER(PACK_LAYER);
   bool ret = true;

   // check if the user (uid, gid) is the same as we are running with or root
   // this is required for communication between components like sge_qmaster and sge_execd
   if (local_uid_gid) {
      if ((pb->uid != component_get_uid() && pb->uid != 0) ||
          (pb->gid != component_get_gid() && pb->gid != 0)) {
         sge_dstring_sprintf(error, MSG_CULL_AUTHINFO_UIDGIDMISMATCH_UUUU,
            pb->uid, pb->gid, component_get_uid(), component_get_gid());
         ret = false;
      }
   }

   if (ret && reresolve_user) {
      char buffer[MAX_USER_GROUP]{};
      if (sge_uid2user(pb->uid, buffer, sizeof(buffer), MAX_NIS_RETRIES) != 0) {
         // sge_uid2user() calls ERROR in case of errors
         sge_dstring_sprintf(error, MSG_CULL_AUTHINFO_COULDNOTRESOLVEUSER_U, pb->uid);
         ret = false;
      } else {
         if (strcmp(pb->username, buffer) != 0) {
            sge_dstring_sprintf(error, MSG_CULL_AUTHINFO_CORRECTINGUSERNAME_SS, pb->username, buffer);
            sge_strlcpy(pb->username, buffer, MAX_USER_GROUP);
         }
      }
      if (sge_gid2group(pb->gid, buffer, sizeof(buffer), MAX_NIS_RETRIES) != 0) {
         // sge_gid2user() calls ERROR in case of errors
         sge_dstring_sprintf(error, MSG_CULL_AUTHINFO_COULDNOTRESOLVEGROUP_U, pb->gid);
         ret = false;
      } else {
         if (strcmp(pb->groupname, buffer) != 0) {
            sge_dstring_sprintf(error, MSG_CULL_AUTHINFO_CORRECTINGGROUPNAME_SS, pb->groupname, buffer);
            sge_strlcpy(pb->groupname, buffer, MAX_USER_GROUP);
         }
      }
   }

   if (ret && reresolve_supp_grp) {
      int amount;
      ocs_grp_elem_t *grp_array;
      if (ocs_get_groups(pb->username, pb->gid, &amount, &grp_array, error)) {
         sge_free(&pb->grp_array);
         pb->grp_amount = amount;
         pb->grp_array = grp_array;
      } else {
         ret = false;
      }
   }

   DRETURN(ret);
}
