/*
    librtmp
    Copyright (C) 2009 ITOYANAGI Kazunori

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    ITOYANAGI Kazunori
    kazunori@itoyanagi.name
*/

#ifndef _rtmp_H_
#define _rtmp_H_


#ifdef MACOS_OPENTRANSPORT
#include <OpenTransport.h>
#include <OpenTptInternet.h>
#else
#if defined(__WIN32__) || defined(WIN32)
#define __USE_W32_SOCKETS
#include <windows.h>
#ifdef __CYGWIN__
#include <netinet/in.h>
#endif
#else /* UNIX */
#ifdef __OS2__
#include <types.h>
#include <sys/ioctl.h>
#endif
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#ifndef __BEOS__
#include <arpa/inet.h>
#endif
#ifdef linux /* FIXME: what other platforms have this? */
#include <netinet/tcp.h>
#endif
#include <netdb.h>
#include <sys/socket.h>
#endif /* WIN32 */
#endif /* Open Transport */


/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

#define RTMP_HANDSHAKE_SIZE 1536
#define RTMP_BUFFER_SIZE 4096


typedef struct rtmp_server_client_t rtmp_server_client_t;

struct rtmp_server_client_t
{
    unsigned int conn_sock;
    struct sockaddr_in conn_sockaddr;
    unsigned char received_buffer[RTMP_BUFFER_SIZE];
    size_t received_size;
    unsigned char will_send_buffer[RTMP_BUFFER_SIZE];
    size_t will_send_size;
    size_t amf_chunk_size;
    void *data;
    void (*process_message)(rtmp_server_client_t *rsc);
    unsigned char handshake[RTMP_HANDSHAKE_SIZE];
    rtmp_server_client_t *prev;
    rtmp_server_client_t *next;
};

typedef struct rtmp_server_t rtmp_server_t;

struct rtmp_server_t
{
    int conn_sock;
    struct sockaddr_in conn_sockaddr;
    int stand_by_socket;
    rtmp_server_client_t *client_working;
    rtmp_server_client_t *client_pool;
};


extern rtmp_server_t *rtmp_server_create(unsigned short port_number);
extern void rtmp_server_process_message(rtmp_server_t *rs);
extern void rtmp_server_free(rtmp_server_t *rs);


#define DEFAULT_AMF_CHUNK_SIZE 128


typedef enum rtmp_result rtmp_result_t;

enum rtmp_result
{
    RTMP_SUCCESS,
    RTMP_ERROR_UNKNOWN,
    RTMP_ERROR_BUFFER_OVERFLOW,
    RTMP_ERROR_BROKEN_PACKET,
    RTMP_ERROR_DIVIDED_PACKET,
    RTMP_ERROR_MEMORY_ALLOCATION,
    RTMP_ERROR_LACKED_MEMORY,
    RTMP_ERROR_DISCONNECTED,
};


typedef struct rtmp_event_t rtmp_event_t;

struct rtmp_event_t
{
    char *code;
    char *level;
    rtmp_event_t *next;
};

typedef struct rtmp_client_t rtmp_client_t;

struct rtmp_client_t
{
    int conn_sock;
    unsigned char received_buffer[RTMP_BUFFER_SIZE];
    size_t received_size;
    unsigned char will_send_buffer[RTMP_BUFFER_SIZE];
    size_t will_send_size;
    void (*process_message)(rtmp_client_t *client);
    void *data;
    char *url;
    char *protocol;
    char *host;
    int port_number;
    char *path;
    size_t amf_chunk_size;
    unsigned char handshake[RTMP_HANDSHAKE_SIZE];
    long message_number;
    rtmp_event_t *events;
};


rtmp_client_t *rtmp_client_create(const char *url);
extern void rtmp_client_free(rtmp_client_t *client);

extern rtmp_event_t *rtmp_client_get_event(rtmp_client_t *client);
extern void rtmp_client_delete_event(rtmp_client_t *client);

extern void rtmp_client_connect(
    rtmp_client_t *client
    /* FIXME: take URL */);
extern void rtmp_client_create_stream(rtmp_client_t *client);
extern void rtmp_client_play(rtmp_client_t *client, const char *file_name);
extern void rtmp_server_client_send_server_bandwidth(rtmp_server_client_t *rsc);
extern void rtmp_server_client_send_client_bandwidth(rtmp_server_client_t *rsc);
extern void rtmp_server_client_send_ping(rtmp_server_client_t *rsc);
extern void rtmp_server_client_send_chunk_size(rtmp_server_client_t *rsc);
extern void rtmp_server_client_send_connect_result(
   rtmp_server_client_t *rsc, double number);
extern void rtmp_server_client_send_create_stream_result(
    rtmp_server_client_t *rsc, double number);
extern void rtmp_server_client_send_play_result_error(
    rtmp_server_client_t *rsc, double number);
extern void rtmp_server_client_send_play_result_success(
    rtmp_server_client_t *rsc, double number);

extern void rtmp_client_process_message(rtmp_client_t *client);


/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif


#endif
