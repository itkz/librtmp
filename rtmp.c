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

#if defined(__WIN32__) || defined(WIN32)
#include <mmsystem.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>


#include "rtmp.h"
#include "rtmp_packet.h"
#include "amf_packet.h"
#include "data_rw.h"


static rtmp_server_client_t *get_new_server_client(rtmp_server_t *s);
static int rtmp_server_client_set_will_send_buffer(
    rtmp_server_client_t *rc, unsigned char *data, size_t size);
static void rtmp_server_client_delete_received_buffer(
    rtmp_server_client_t *rsc, size_t size);
static rtmp_result_t rtmp_server_client_send_and_recv(
    rtmp_server_client_t *rsc);
static rtmp_result_t rtmp_server_client_send_packet(
    rtmp_server_client_t *rsc, rtmp_packet_t *packet);
static void rtmp_server_client_process_packet(
    rtmp_server_client_t *rsc, rtmp_packet_t *packet);
static void rtmp_server_client_free(
    rtmp_server_t *rs, rtmp_server_client_t *rsc);

static void rtmp_server_client_handshake_first(
    rtmp_server_client_t *server_client);
static void rtmp_server_client_handshake_second(
    rtmp_server_client_t *server_client);
static void rtmp_server_client_get_packet(
    rtmp_server_client_t *server_client);


rtmp_server_t *rtmp_server_create(unsigned short port_number)
{
    rtmp_server_t *rtmp_server;
    int ret;

    rtmp_server = (rtmp_server_t*)malloc(sizeof(rtmp_server_t));
    if (rtmp_server == NULL) {
        return NULL;
    }
    rtmp_server->client_pool = NULL;
    rtmp_server->client_working = NULL;
    rtmp_server->stand_by_socket = -1;

    rtmp_server->conn_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (rtmp_server->conn_sock == -1) {
        rtmp_server_free(rtmp_server);
        return NULL;
    }
    rtmp_server->stand_by_socket = 1;

    rtmp_server->conn_sockaddr.sin_family = AF_INET;
    rtmp_server->conn_sockaddr.sin_addr.s_addr = INADDR_ANY;
    rtmp_server->conn_sockaddr.sin_port = htons(port_number);
    ret = bind(
        rtmp_server->conn_sock,
        (struct sockaddr*)&(rtmp_server->conn_sockaddr),
        sizeof(rtmp_server->conn_sockaddr));
    if (ret == -1) {
        rtmp_server_free(rtmp_server);
        return NULL;
    }

    ret = listen(rtmp_server->conn_sock, 10);
    if (ret == -1) {
        rtmp_server_free(rtmp_server);
        return NULL;
    }

    return rtmp_server;
}


void rtmp_server_process_message(rtmp_server_t *rs)
{
    rtmp_server_client_t *rsc;
    rtmp_server_client_t *next;
    int client_sock;
#ifdef __MINGW32__
    int addrlen;
#else
    socklen_t addrlen;
#endif
    fd_set fdset;
    int ret;
    struct timeval timeout;
    rtmp_result_t result;

    FD_ZERO(&fdset);
    FD_SET((unsigned int)rs->conn_sock, &fdset);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    ret = select(rs->conn_sock + 1, &fdset, NULL, NULL, &timeout);
    if (ret == 1) {
        rsc = get_new_server_client(rs);

        addrlen = sizeof(rs->conn_sockaddr);
        client_sock = accept(
            rs->conn_sock,
            (struct sockaddr*)&(rs->conn_sockaddr),
            &addrlen);
        if (client_sock == -1) {
            return;
        }

        rsc->conn_sock = client_sock;
        rsc->prev = NULL;
        rsc->next = rs->client_working;
        if (rs->client_working == NULL) {
            rs->client_working = rsc;
        } else {
            rs->client_working->prev = rsc;
            rs->client_working = rsc;
        }
    }

    rsc = rs->client_working;
    while (rsc) {
        result = rtmp_server_client_send_and_recv(rsc);
        rsc->process_message(rsc);
        if (result == RTMP_ERROR_DISCONNECTED) {
#ifdef DEBUG
        printf("client disconnected\n");
#endif
            next = rsc->next;
            rtmp_server_client_free(rs, rsc);
            rsc = next;
        } else {
            rsc = rsc->next;
        }
    }
}


static rtmp_result_t rtmp_server_client_send_and_recv(rtmp_server_client_t *rsc)
{
    fd_set fdset;
    int ret;
    struct timeval timeout;
    int received_size;
    int sent_size;

    FD_ZERO(&fdset);
    FD_SET(rsc->conn_sock, &fdset);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    ret = select(rsc->conn_sock + 1, &fdset, NULL, NULL, &timeout);
    if (ret == 1) {
        received_size = recv(
            rsc->conn_sock,
            (void*)(rsc->received_buffer + rsc->received_size),
            RTMP_BUFFER_SIZE - rsc->received_size, 0);
        if (received_size == -1) {
            return RTMP_ERROR_DISCONNECTED;
        }
#ifdef DEBUG
        if (received_size > 0) {
            printf("received: %d\n", received_size);
        }
#endif
        rsc->received_size += received_size;
    }

    rsc->process_message(rsc);

    if (rsc->will_send_size > 0) {
        FD_ZERO(&fdset);
        FD_SET(rsc->conn_sock, &fdset);
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
        ret = select(rsc->conn_sock + 1, NULL, &fdset, NULL, &timeout);
        if (ret == 1) {
            sent_size = send(
                rsc->conn_sock,
                rsc->will_send_buffer,
                rsc->will_send_size, 0);
            if (sent_size == -1) {
                return RTMP_ERROR_DISCONNECTED;
            }
#ifdef DEBUG
            if (sent_size > 0) {
                printf("sent: %d\n", sent_size);
            }
#endif
            if (rsc->will_send_size - sent_size > 0) {
                memmove(
                    rsc->will_send_buffer,
                    rsc->will_send_buffer + sent_size,
                    rsc->will_send_size - sent_size);
            }
            rsc->will_send_size -= sent_size;
        }
    }

    return RTMP_SUCCESS;
}


static rtmp_server_client_t *get_new_server_client(rtmp_server_t *rs)
{
    rtmp_server_client_t *rsc;

    if (rs->client_pool == NULL) {
        rsc = (rtmp_server_client_t*)malloc(sizeof(rtmp_server_client_t));
        if (rsc == NULL) {
            return NULL;
        }
    } else {
        rsc = rs->client_pool;
        rs->client_pool = rsc->next;
        if (rs->client_pool) {
            rs->client_pool->prev = NULL;
        }
    }


    rsc->received_size = 0;
    rsc->will_send_size = 0;
    rsc->amf_chunk_size = DEFAULT_AMF_CHUNK_SIZE;
    rsc->process_message = rtmp_server_client_handshake_first;

    return rsc;
}


static int rtmp_server_client_set_will_send_buffer(
    rtmp_server_client_t *rsc, unsigned char *data, size_t size)
{
    if (rsc->will_send_size + size > RTMP_BUFFER_SIZE) {
        return RTMP_ERROR_BUFFER_OVERFLOW;
    }
    memmove(
        rsc->will_send_buffer + rsc->will_send_size,
        data, size);
    rsc->will_send_size += size;
    return RTMP_SUCCESS;
}


static void rtmp_server_client_delete_received_buffer(
    rtmp_server_client_t *rsc, size_t size)
{
    if (size >= rsc->received_size) {
        rsc->received_size = 0;
    } else {
        memmove(
            rsc->received_buffer,
            rsc->received_buffer + size,
            rsc->received_size - size);
        rsc->received_size -= size;
    }
}


void rtmp_server_client_handshake_first(rtmp_server_client_t *rsc)
{
    unsigned char magic[] = {0x03};
    int i;
#if defined(__WIN32__) || defined(WIN32)
    DWORD now;
#else
    unsigned long now;
#endif

    if (rsc->received_size >= (1 + RTMP_HANDSHAKE_SIZE)) {
        rtmp_server_client_set_will_send_buffer(
            rsc, magic, 1);
#if defined(__WIN32__) || defined(WIN32)
        now = timeGetTime();
#else
        now = time(NULL) * 1000;
#endif
        write_le32int(rsc->handshake, (int)now);
        write_le32int(rsc->handshake + 4, 0);
        for (i = 8; i < RTMP_HANDSHAKE_SIZE; ++i) {
            rsc->handshake[i] = (unsigned char)(rand() % 256);
        }
        rtmp_server_client_set_will_send_buffer(
            rsc,
            rsc->handshake,
            RTMP_HANDSHAKE_SIZE);
        rtmp_server_client_set_will_send_buffer(
            rsc,
            rsc->received_buffer + 1,
            RTMP_HANDSHAKE_SIZE);
        rtmp_server_client_delete_received_buffer(
            rsc,
            1 + RTMP_HANDSHAKE_SIZE);
#ifdef DEBUG
        printf("handshake 1\n");
#endif
        rsc->process_message = rtmp_server_client_handshake_second;
    }
}


void rtmp_server_client_handshake_second(rtmp_server_client_t *rsc)
{
    unsigned char *client_signature;
    unsigned char *response;

    if (rsc->received_size >= RTMP_HANDSHAKE_SIZE) {
        client_signature = rsc->received_buffer;
        response = rsc->received_buffer;
#ifdef DEBUG
        if (memcmp(rsc->handshake, response, RTMP_HANDSHAKE_SIZE) == 0) {
            printf("handshake response OK!\n");
        }
#endif
        rtmp_server_client_delete_received_buffer(
            rsc, RTMP_HANDSHAKE_SIZE);
#ifdef DEBUG
        printf("handshake 2\n");
#endif
        rsc->data = rtmp_packet_create();
        rsc->process_message = rtmp_server_client_get_packet;
        rtmp_server_client_send_server_bandwidth(rsc);
        rtmp_server_client_send_client_bandwidth(rsc);
        rtmp_server_client_send_ping(rsc);
    }
}


static void rtmp_server_client_process_packet(
    rtmp_server_client_t *rsc, rtmp_packet_t *packet)
{
    rtmp_packet_inner_amf_t *inner_amf;
    amf_packet_t *amf;
    char *command;
    double number;
    char *code;
    char *level;

    switch (packet->data_type) {
    case RTMP_DATATYPE_CHUNK_SIZE:
        break;
    case RTMP_DATATYPE_BYTES_READ:
        break;
    case RTMP_DATATYPE_PING:
        break;
    case RTMP_DATATYPE_SERVER_BW:
        break;
    case RTMP_DATATYPE_CLIENT_BW:
        break;
    case RTMP_DATATYPE_AUDIO_DATA:
        break;
    case RTMP_DATATYPE_VIDEO_DATA:
        break;
    case RTMP_DATATYPE_MESSAGE:
        break;
    case RTMP_DATATYPE_NOTIFY:
        inner_amf = packet->inner_amf_packets;
        amf = inner_amf->amf;
        if (amf->datatype != AMF_DATATYPE_STRING) {
            break;
        }
        command = amf->string.value;
#ifdef DEBUG
        printf("notify command: %s\n", command);
#endif
        rtmp_packet_retrieve_status_info(packet, &code, &level);
        if (code == NULL || level == NULL) {
            break;
        }
#ifdef DEBUG
        printf("code: %s\n", code);
        printf("level: %s\n", level);
#endif
        /* FIXME: add event */
        break;
    case RTMP_DATATYPE_SHARED_OBJECT:
        break;
    case RTMP_DATATYPE_INVOKE:
        inner_amf = packet->inner_amf_packets;
        amf = inner_amf->amf;
        if (amf->datatype != AMF_DATATYPE_STRING) {
            break;
        }
        command = amf->string.value;
        amf = inner_amf->next->amf;
        if (amf->datatype != AMF_DATATYPE_NUMBER) {
            break;
        }
        number = amf->number.value;
#ifdef DEBUG
        printf("invoke command: %s\n", command);
#endif
        if (strcmp(command, "_result") == 0) {
            rtmp_packet_retrieve_status_info(packet, &code, &level);
            if (code == NULL || level == NULL) {
                break;
            }
#ifdef DEBUG
            printf("code: %s\n", code);
            printf("level: %s\n", level);
#endif
            /* FIXME: add event */
        } else if (strcmp(command, "connect") == 0) {
//            rsc->amf_chunk_size = 4096;
            rtmp_server_client_send_chunk_size(rsc);
            rtmp_server_client_send_connect_result(rsc, number);
        } else if (strcmp(command, "createStream") == 0) {
            rtmp_server_client_send_create_stream_result(rsc, number);
        } else if (strcmp(command, "play") == 0) {
            rtmp_server_client_send_play_result_success(rsc, number);
	}
        break;
    default:
        break;
    }
}


static rtmp_result_t rtmp_server_client_send_packet(
    rtmp_server_client_t *rsc, rtmp_packet_t *packet)
{
    rtmp_result_t result;
    size_t packet_size;

    unsigned char fuck[1024];
    result = rtmp_packet_serialize(
        packet,
        fuck,
        1024,
        rsc->amf_chunk_size,
        &packet_size);
    if (result == RTMP_SUCCESS) {
        rtmp_server_client_set_will_send_buffer(
            rsc, fuck, packet_size);
    }

    return RTMP_SUCCESS;
}


void rtmp_server_client_send_server_bandwidth(
    rtmp_server_client_t *rsc)
{
    rtmp_packet_t *rtmp_packet;

    rtmp_packet = (rtmp_packet_t*)rsc->data;
    rtmp_packet_cleanup(rtmp_packet);
    rtmp_packet->object_id = 2;
    rtmp_packet->timer = 0;
    rtmp_packet->data_type = RTMP_DATATYPE_SERVER_BW;
    rtmp_packet->stream_id = 0;
    rtmp_packet->body_type = RTMP_BODY_TYPE_DATA;
    rtmp_packet_allocate_body_data(rtmp_packet, 4);
    rtmp_packet->body_data[0] = 0x00;
    rtmp_packet->body_data[1] = 0x26;
    rtmp_packet->body_data[2] = 0x25;
    rtmp_packet->body_data[3] = 0xA0;

    rtmp_server_client_send_packet(rsc, rtmp_packet);
}


void rtmp_server_client_send_client_bandwidth(
    rtmp_server_client_t *rsc)
{
    rtmp_packet_t *rtmp_packet;

    rtmp_packet = (rtmp_packet_t*)rsc->data;
    rtmp_packet_cleanup(rtmp_packet);
    rtmp_packet->object_id = 2;
    rtmp_packet->timer = 0;
    rtmp_packet->data_type = RTMP_DATATYPE_CLIENT_BW;
    rtmp_packet->stream_id = 0;
    rtmp_packet->body_type = RTMP_BODY_TYPE_DATA;
    rtmp_packet_allocate_body_data(rtmp_packet, 5);
    rtmp_packet->body_data[0] = 0x00;
    rtmp_packet->body_data[1] = 0x26;
    rtmp_packet->body_data[2] = 0x25;
    rtmp_packet->body_data[3] = 0xA0;
    rtmp_packet->body_data[4] = 0x02;

    rtmp_server_client_send_packet(rsc, rtmp_packet);
}


void rtmp_server_client_send_ping(rtmp_server_client_t *rsc)
{
    rtmp_packet_t *rtmp_packet;

    rtmp_packet = (rtmp_packet_t*)rsc->data;
    rtmp_packet_cleanup(rtmp_packet);
    rtmp_packet->object_id = 2;
    rtmp_packet->timer = 0;
    rtmp_packet->data_type = RTMP_DATATYPE_PING;
    rtmp_packet->stream_id = 0;
    rtmp_packet->body_type = RTMP_BODY_TYPE_DATA;
    rtmp_packet_allocate_body_data(rtmp_packet, 6);
    rtmp_packet->body_data[0] = 0x00;
    rtmp_packet->body_data[1] = 0x00;
    rtmp_packet->body_data[2] = 0x00;
    rtmp_packet->body_data[3] = 0x00;
    rtmp_packet->body_data[4] = 0x00;
    rtmp_packet->body_data[5] = 0x00;

    rtmp_server_client_send_packet(rsc, rtmp_packet);
}


void rtmp_server_client_send_chunk_size(
    rtmp_server_client_t *rsc)
{
    rtmp_packet_t *rtmp_packet;

    rtmp_packet = (rtmp_packet_t*)rsc->data;
    rtmp_packet_cleanup(rtmp_packet);
    rtmp_packet->object_id = 2;
    rtmp_packet->timer = 0;
    rtmp_packet->data_type = RTMP_DATATYPE_PING;
    rtmp_packet->stream_id = 0;
    rtmp_packet->body_type = RTMP_BODY_TYPE_DATA;
    rtmp_packet_allocate_body_data(rtmp_packet, 4);
    write_be32int(rtmp_packet->body_data, rsc->amf_chunk_size);

    rtmp_server_client_send_packet(rsc, rtmp_packet);
}


void rtmp_server_client_send_connect_result(
    rtmp_server_client_t *rsc, double number)
{
    rtmp_packet_t *rtmp_packet;
    amf_packet_t *amf_object;

    rtmp_packet = (rtmp_packet_t*)rsc->data;
    rtmp_packet_cleanup(rtmp_packet);
    rtmp_packet->object_id = 3;
    rtmp_packet->timer = 0;
    rtmp_packet->data_type = RTMP_DATATYPE_INVOKE;
    rtmp_packet->stream_id = 0;
    rtmp_packet->body_type = RTMP_BODY_TYPE_AMF;

    rtmp_packet_add_amf(
        rtmp_packet,
        amf_packet_create_string("_result"));
    rtmp_packet_add_amf(
        rtmp_packet,
        amf_packet_create_number(number));

    amf_object = amf_packet_create_object();
    amf_packet_add_property_to_object(
        amf_object, "fmsver", amf_packet_create_string("librtmp 0.1"));
    amf_packet_add_property_to_object(
        amf_object, "capabilities", amf_packet_create_number(31));
    rtmp_packet_add_amf(rtmp_packet, amf_object);

    amf_object = amf_packet_create_object();
    amf_packet_add_property_to_object(
        amf_object, "level", amf_packet_create_string("status"));
    amf_packet_add_property_to_object(
        amf_object, "code", amf_packet_create_string("NetConnection.Connect.Success"));
    amf_packet_add_property_to_object(
        amf_object, "description", amf_packet_create_string("Connection succeeded."));
    amf_packet_add_property_to_object(
        amf_object, "clientid", amf_packet_create_number(313639155));
    /* FIXME: increment client id */
    amf_packet_add_property_to_object(
        amf_object, "objectEncoding", amf_packet_create_number(0));
    rtmp_packet_add_amf(rtmp_packet, amf_object);

    rtmp_server_client_send_packet(rsc, rtmp_packet);
}


void rtmp_server_client_send_create_stream_result(
    rtmp_server_client_t *rsc, double number)
{
    rtmp_packet_t *rtmp_packet;

    rtmp_packet = (rtmp_packet_t*)rsc->data;
    rtmp_packet_cleanup(rtmp_packet);
    rtmp_packet->object_id = 3;
    rtmp_packet->timer = 0;
    rtmp_packet->data_type = RTMP_DATATYPE_INVOKE;
    rtmp_packet->stream_id = 0;
    rtmp_packet->body_type = RTMP_BODY_TYPE_AMF;

    rtmp_packet_add_amf(
        rtmp_packet,
        amf_packet_create_string("_result"));
    rtmp_packet_add_amf(
        rtmp_packet,
        amf_packet_create_number(number));
    rtmp_packet_add_amf(
        rtmp_packet,
        amf_packet_create_null());
    rtmp_packet_add_amf(
        rtmp_packet,
        amf_packet_create_number(15125));
    /* FIXME: What's this number */

    rtmp_server_client_send_packet(rsc, rtmp_packet);
}


void rtmp_server_client_send_play_result_success(
    rtmp_server_client_t *rsc, double number)
{
    rtmp_packet_t *rtmp_packet;
    amf_packet_t *amf_object;

    rtmp_packet = (rtmp_packet_t*)rsc->data;
    rtmp_packet_cleanup(rtmp_packet);
    rtmp_packet->object_id = 5;
    rtmp_packet->timer = 0;
    rtmp_packet->data_type = RTMP_DATATYPE_INVOKE;
    rtmp_packet->stream_id = 1; /* FIXME */
    rtmp_packet->body_type = RTMP_BODY_TYPE_AMF;

    rtmp_packet_add_amf(
        rtmp_packet,
        amf_packet_create_string("onStatus"));
    rtmp_packet_add_amf(
        rtmp_packet,
        amf_packet_create_number(number));
    rtmp_packet_add_amf(
        rtmp_packet,
        amf_packet_create_null());

    amf_object = amf_packet_create_object();
    amf_packet_add_property_to_object(
        amf_object, "code", amf_packet_create_string("NetStream.Play.Start"));
    amf_packet_add_property_to_object(
        amf_object, "level", amf_packet_create_string("status"));
    amf_packet_add_property_to_object(
        amf_object, "description", amf_packet_create_string(""));
    rtmp_packet_add_amf(rtmp_packet, amf_object);

    rtmp_server_client_send_packet(rsc, rtmp_packet);
}


void rtmp_server_client_send_play_result_error(
    rtmp_server_client_t *rsc, double number)
{
    rtmp_packet_t *rtmp_packet;
    amf_packet_t *amf_object;

    rtmp_packet = (rtmp_packet_t*)rsc->data;
    rtmp_packet_cleanup(rtmp_packet);
    rtmp_packet->object_id = 3;
    rtmp_packet->timer = 0;
    rtmp_packet->data_type = RTMP_DATATYPE_INVOKE;
    rtmp_packet->stream_id = 0; /* FIXME: 8byte header */
    rtmp_packet->body_type = RTMP_BODY_TYPE_AMF;

    rtmp_packet_add_amf(
        rtmp_packet,
        amf_packet_create_string("onStatus"));
    rtmp_packet_add_amf(
        rtmp_packet,
        amf_packet_create_number(number));
    rtmp_packet_add_amf(
        rtmp_packet,
        amf_packet_create_null());
/* Flash Player crashes when this code is available
    rtmp_packet_add_amf(
        rtmp_packet,
        amf_packet_create_number(15125));
*/

    amf_object = amf_packet_create_object();
    amf_packet_add_property_to_object(
        amf_object, "level", amf_packet_create_string("error"));
    amf_packet_add_property_to_object(
        amf_object, "code", amf_packet_create_string("NetStream.Play.StreamNotFound"));
    amf_packet_add_property_to_object(
        amf_object, "description", amf_packet_create_string("Failed to play test.mp4; stream not found."));
    amf_packet_add_property_to_object(
        amf_object, "clientid", amf_packet_create_number(313639155));
    /* FIXME: increment client id */
    amf_packet_add_property_to_object(
        amf_object, "details", amf_packet_create_string("test.mp4"));
    rtmp_packet_add_amf(rtmp_packet, amf_object);

    rtmp_server_client_send_packet(rsc, rtmp_packet);
}


static void rtmp_server_client_get_packet(rtmp_server_client_t *rsc)
{
    rtmp_result_t ret;
    size_t packet_size;
    rtmp_packet_t *packet;

    packet = (rtmp_packet_t*)rsc->data;
    ret = rtmp_packet_analyze_data(
        packet,
        rsc->received_buffer, rsc->received_size,
        rsc->amf_chunk_size,
        &packet_size);
    if (ret == RTMP_SUCCESS) {
        rtmp_server_client_delete_received_buffer(rsc, packet_size);
        rtmp_server_client_process_packet(rsc, packet);
    }
}


static void rtmp_server_client_free(rtmp_server_t *rs, rtmp_server_client_t *rsc)
{
    if (rsc->prev) {
        rsc->prev->next = rsc->next;
    } else {
        rs->client_working = rsc->next;
        if (rsc->next) {
            rsc->next->prev = NULL;
        }
    }
    if (rsc->next) {
        rsc->next->prev = rsc->prev;
    } else {
        if (rsc->prev) {
            rsc->prev->next = NULL;
        }
    }
    if (rsc->data) {
        rtmp_packet_free((rtmp_packet_t*)rsc->data);
    }
#ifdef __USE_W32_SOCKETS
        closesocket(rsc->conn_sock);
        WSACleanup();
#else
        close(rsc->conn_sock);
#endif
    free(rsc);
}


void rtmp_server_free(rtmp_server_t *rs)
{
    rtmp_server_client_t *rsc;
    rtmp_server_client_t *next;

    rsc = rs->client_working;
    while (rsc) {
        next = rsc->next;
        rtmp_server_client_free(rs, rsc);
        rsc = next;
    }
    rsc = rs->client_pool;
    while (rsc) {
        free(rsc);
    }
    if (rs->stand_by_socket) {
#ifdef __USE_W32_SOCKETS
        closesocket(rs->conn_sock);
        WSACleanup();
#else
        close(rs->conn_sock);
#endif
    }
    free(rs);
}














static void rtmp_client_parse_url(rtmp_client_t *rc, const char *url);
static void rtmp_client_parse_host_and_port_number(
    rtmp_client_t *rc, const char *host_and_port_number);

static void rtmp_client_handshake_first(rtmp_client_t *rc);
static void rtmp_client_handshake_second(rtmp_client_t *rc);
static void rtmp_client_get_packet(rtmp_client_t *rc);
static void rtmp_client_process_packet(
    rtmp_client_t *rc, rtmp_packet_t *packet);

static int rtmp_client_set_will_send_buffer(
    rtmp_client_t *rc, unsigned char *data, size_t size);
static void rtmp_client_delete_received_buffer(
    rtmp_client_t *rc, size_t size);
static rtmp_result_t rtmp_client_send_packet(
    rtmp_client_t *rc, rtmp_packet_t *packet);
static rtmp_result_t rtmp_client_add_event(
    rtmp_client_t *rc, char *code, char *level);


rtmp_client_t *rtmp_client_create(const char *url)
{
    rtmp_client_t *rc;
    int ret;
#ifdef __USE_W32_SOCKETS
    WSADATA data;
#endif
    struct sockaddr_in conn_sockaddr;

#ifdef __USE_W32_SOCKETS
    WSAStartup(MAKEWORD(2, 0), &data);
#endif

    srand((unsigned)time(NULL));

    rc = (rtmp_client_t*)malloc(sizeof(rtmp_client_t));
    if (rc == NULL) {
        return NULL;
    }

    rc->conn_sock = -1;

    rc->protocol = NULL;
    rc->host = NULL;
    rc->port_number = -1;
    rtmp_client_parse_url(rc, url);
    if (rc->url == NULL || rc->protocol == NULL || rc->host == NULL || rc->path == NULL) {
        rtmp_client_free(rc);
        return NULL;
    }

    memset(&conn_sockaddr, 0, sizeof(conn_sockaddr));
    conn_sockaddr.sin_family = AF_INET;
    conn_sockaddr.sin_addr.s_addr = inet_addr(rc->host);
    conn_sockaddr.sin_port = htons(rc->port_number);
    if (conn_sockaddr.sin_addr.s_addr == INADDR_NONE) {
        rtmp_client_free(rc);
        return NULL;
    }

    rc->conn_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (rc->conn_sock == -1) {
        rtmp_client_free(rc);
        return NULL;
    }

#ifdef __USE_W32_SOCKETS
    ret = connect(
        rc->conn_sock,
        (PSOCKADDR)&conn_sockaddr, sizeof(conn_sockaddr));
    if (ret == SOCKET_ERROR) {
        rtmp_client_free(rc);
        return NULL;
    }
#else
    ret = connect(
        rc->conn_sock, (const struct sockaddr*)&(conn_sockaddr),
        sizeof(conn_sockaddr));
    if (ret == -1) {
        rtmp_client_free(rc);
        return NULL;
    }
#endif

    rc->amf_chunk_size = DEFAULT_AMF_CHUNK_SIZE;
    rc->received_size = 0;
    rc->will_send_size = 0;
    rc->process_message = rtmp_client_handshake_first;
    rc->message_number = 0.0;
    rc->events = NULL;

    return rc;
}


void rtmp_client_free(rtmp_client_t *rc)
{
    /* FIXME: clean up some memory */
    if (rc->conn_sock != -1) {
#ifdef __USE_W32_SOCKETS
        closesocket(rc->conn_sock);
        WSACleanup();
#else
        close(rc->conn_sock);
#endif
    }

    if (rc->url) {
        free(rc->url);
    }
    if (rc->protocol) {
        free(rc->protocol);
    }
    if (rc->host) {
        free(rc->host);
    }
    if (rc->path) {
        free(rc->path);
    }
}


static void rtmp_client_parse_url(rtmp_client_t *rc, const char *url)
{
    int i;
    int position;
    int length;
    char previous_charactor;
    char *host_and_port_number;

    for (i = 0; url[i]; ++i) {
        if (url[i] == ':') {
            if (i == 0) {
                return;
	    }
            rc->protocol = malloc(i + 1);
            if (rc->protocol == NULL) {
                return;
            }
            strncpy(rc->protocol, url, i);
#ifdef DEBUG
            printf("protocol: %s\n", rc->protocol);
#endif
            position = i;
            break;
        }
    }
    if (url[position] == '\0') {
        return;
    }
    if (rc->protocol == NULL) {
        return;
    }

    previous_charactor = url[position - 1];
    for (i = 0; url[position + i]; ++i) {
        if (previous_charactor == '/' && url[position + i] == '/') {
            position += i;
            break;
        }
        previous_charactor = url[position + i];
    }
    if (url[position] == '\0') {
        return;
    }
    position++;

    for (i = 0; url[position + i]; ++i) {
        if (url[position + i] == '/') {
            if (i == 0) {
                break;
            }
            host_and_port_number = malloc(i + 1);
            if (host_and_port_number == NULL) {
                return;
            }
            strncpy(host_and_port_number, url + position, i);
            rtmp_client_parse_host_and_port_number(
                rc, host_and_port_number);
            free(host_and_port_number);
            position += i;
            break;
        }
    }
    if (url[position] == '\0') {
        return;
    }
    position++;

    length = strlen(url + position);
    rc->path = malloc(length + 1);
    if (rc->path == NULL) {
        return;
    }
    strncpy(rc->path, url + position, length);
#ifdef DEBUG
    printf("path: %s\n", rc->path);
#endif
}


static void rtmp_client_parse_host_and_port_number(
    rtmp_client_t *rc, const char *host_and_port_number)
{
    int i;

    for (i = 0; host_and_port_number[i]; ++i) {
        if (host_and_port_number[i] == ':') {
            rc->host = malloc(i + 1);
            if (rc->host == NULL) {
                return;
            }
            strncpy(rc->host, host_and_port_number, i);
            break;
	}
    }
    if (rc->host == NULL) {
        rc->host = malloc(strlen(host_and_port_number) + 1);
        if (rc->host == NULL) {
            return;
        }
        strcpy(rc->host, host_and_port_number);
        rc->port_number = 1935;
    } else {
        rc->port_number = atoi(host_and_port_number + i + 1);
    }
#ifdef DEBUG
    printf("host: %s\n", rc->host);
    printf("port_number: %d\n", rc->port_number);
#endif
}


void rtmp_client_process_message(rtmp_client_t *rc)
{
    fd_set fdset;
    int ret;
    int received_size;
    int sent_size;
    struct timeval timeout;

    FD_ZERO(&fdset);
    FD_SET(rc->conn_sock, &fdset);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    ret = select(rc->conn_sock + 1, &fdset, NULL, NULL, &timeout);
    if (ret == 1) {
        received_size = recv(
            rc->conn_sock,
            rc->received_buffer + rc->received_size,
            RTMP_BUFFER_SIZE - rc->received_size, 0);
        /* FIXME: process finishing when recv returns -1 */
        if (received_size > 0) {
#ifdef DEBUG
            printf("received: %d\n", received_size);
#endif
            rc->received_size += received_size;
        }
    }

    rc->process_message(rc);

    if (rc->will_send_size > 0) {
        FD_ZERO(&fdset);
        FD_SET(rc->conn_sock, &fdset);
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
        ret = select(rc->conn_sock + 1, NULL, &fdset, NULL, &timeout);
        if (ret == 1) {
            sent_size = send(
                rc->conn_sock,
                rc->will_send_buffer,
                rc->will_send_size, 0);
#ifdef DEBUG
            if (sent_size > 0) {
                printf("sent: %d\n", sent_size);
            }
#endif
            if (sent_size != -1) {
                if (rc->will_send_size - sent_size > 0) {
                    memmove(
                        rc->will_send_buffer,
                        rc->will_send_buffer + sent_size,
                        rc->will_send_size - sent_size);
                }
                rc->will_send_size -= sent_size;
            }
        }
    }
}


static int rtmp_client_set_will_send_buffer(
    rtmp_client_t *rc, unsigned char *data, size_t size)
{
    if (rc->will_send_size + size > RTMP_BUFFER_SIZE) {
        return RTMP_ERROR_BUFFER_OVERFLOW;
    }
    memmove(
        rc->will_send_buffer + rc->will_send_size,
        data, size);
    rc->will_send_size += size;
    return RTMP_SUCCESS;
}


static void rtmp_client_delete_received_buffer(
    rtmp_client_t *rc, size_t size)
{
    if (size >= rc->received_size) {
        rc->received_size = 0;
    } else {
        memmove(
            rc->received_buffer,
            rc->received_buffer + size,
            rc->received_size - size);
        rc->received_size -= size;
    }
}


static void rtmp_client_handshake_first(rtmp_client_t *rc)
{
    unsigned char magic[] = {0x03};
    int i;
#if defined(__WIN32__) || defined(WIN32)
    DWORD now;
#else
    unsigned long now;
#endif

    rtmp_client_set_will_send_buffer(rc, magic, 1);

#if defined(__WIN32__) || defined(WIN32)
    now = timeGetTime();
#else
    now = time(NULL) * 1000;
#endif
    write_le32int(rc->handshake, (int)now);
    write_le32int(rc->handshake + 4, 0);
    for (i = 8; i < RTMP_HANDSHAKE_SIZE; ++i) {
        rc->handshake[i] = (unsigned char)(rand() % 256);
    }
    rtmp_client_set_will_send_buffer(
        rc, rc->handshake, RTMP_HANDSHAKE_SIZE);
#ifdef DEBUG
    printf("handshake 1\n");
#endif
    rc->process_message = rtmp_client_handshake_second;
}


static void rtmp_client_handshake_second(rtmp_client_t *rc)
{
    unsigned char *server_signature;
    unsigned char *response;

    if (rc->received_size >= (1 + RTMP_HANDSHAKE_SIZE * 2)) {
        server_signature = rc->received_buffer + 1;
        response = rc->received_buffer + 1 + RTMP_HANDSHAKE_SIZE;
        rtmp_client_set_will_send_buffer(
            rc, server_signature, RTMP_HANDSHAKE_SIZE);
#ifdef DEBUG
        if (memcmp(rc->handshake, response, RTMP_HANDSHAKE_SIZE) == 0) {
            printf("handshake response OK!\n");
        }
#endif
        rtmp_client_delete_received_buffer(
            rc, 1 + RTMP_HANDSHAKE_SIZE * 2);
#ifdef DEBUG
        printf("handshake 2\n");
#endif
        rc->data = rtmp_packet_create();
        rc->process_message = rtmp_client_get_packet;
        rtmp_client_connect(rc);
    }
}


static void rtmp_client_get_packet(rtmp_client_t *rc)
{
    rtmp_result_t ret;
    size_t packet_size;
    rtmp_packet_t *packet;

    packet = (rtmp_packet_t*)rc->data;
    ret = rtmp_packet_analyze_data(
        packet,
        rc->received_buffer, rc->received_size,
        rc->amf_chunk_size,
        &packet_size);
    if (ret == RTMP_SUCCESS) {
        rtmp_client_delete_received_buffer(rc, packet_size);
        rtmp_client_process_packet(rc, packet);
    }
}


void rtmp_client_process_packet(
    rtmp_client_t *rc, rtmp_packet_t *packet)
{
    rtmp_packet_inner_amf_t *inner_amf;
    amf_packet_t *amf;
    char *command;
    char *code;
    char *level;

    switch (packet->data_type) {
    case RTMP_DATATYPE_CHUNK_SIZE:
        rc->amf_chunk_size = read_be32int(packet->body_data);
        break;
    case RTMP_DATATYPE_BYTES_READ:
        break;
    case RTMP_DATATYPE_PING:
        break;
    case RTMP_DATATYPE_SERVER_BW:
        break;
    case RTMP_DATATYPE_CLIENT_BW:
        break;
    case RTMP_DATATYPE_AUDIO_DATA:
        break;
    case RTMP_DATATYPE_VIDEO_DATA:
        break;
    case RTMP_DATATYPE_MESSAGE:
        break;
    case RTMP_DATATYPE_NOTIFY:
        inner_amf = packet->inner_amf_packets;
        amf = inner_amf->amf;
        if (amf->datatype != AMF_DATATYPE_STRING) {
            break;
        }
        command = amf->string.value;
#ifdef DEBUG
        printf("command: %s\n", command);
#endif
        rtmp_packet_retrieve_status_info(packet, &code, &level);
        if (code == NULL || level == NULL) {
            break;
        }
#ifdef DEBUG
        printf("code: %s\n", code);
        printf("level: %s\n", level);
#endif
        rtmp_client_add_event(rc, code, level);
        break;
    case RTMP_DATATYPE_SHARED_OBJECT:
        break;
    case RTMP_DATATYPE_INVOKE:
        inner_amf = packet->inner_amf_packets;
        amf = inner_amf->amf;
        if (amf->datatype != AMF_DATATYPE_STRING) {
            break;
        }
        command = amf->string.value;
#ifdef DEBUG
        printf("command: %s\n", command);
#endif
        if (strcmp(command, "_result") == 0) {
            rtmp_packet_retrieve_status_info(packet, &code, &level);
            if (code == NULL || level == NULL) {
                break;
            }
#ifdef DEBUG
            printf("code: %s\n", code);
            printf("level: %s\n", level);
#endif
            rtmp_client_add_event(rc, code, level);
        }
        break;
    default:
        break;
    }
}


rtmp_result_t rtmp_client_add_event(
    rtmp_client_t *rc, char *code, char *level)
{
    rtmp_event_t *event;
    rtmp_event_t *last_event;

    event = (rtmp_event_t*)malloc(sizeof(rtmp_event_t));
    event->code = (char*)malloc(strlen(code) + 1);
    strcpy(event->code, code);
    event->level = (char*)malloc(strlen(level) + 1);
    strcpy(event->level, level);
    event->next = NULL;
    if (rc->events == NULL) {
        rc->events = event;
    } else {
        last_event = rc->events;
        while (last_event->next != NULL) {
            last_event = last_event->next;
        }
        last_event->next = event;
    }

    return RTMP_SUCCESS;
}


rtmp_event_t *rtmp_client_get_event(rtmp_client_t *rc)
{
    return rc->events;
}


void rtmp_client_delete_event(rtmp_client_t *rc)
{
    rtmp_event_t *delete_event;
    rtmp_event_t *next_event;

    delete_event = rc->events;
    free(delete_event->code);
    free(delete_event->level);
    free(delete_event);

    rc->events = next_event;
}


rtmp_result_t rtmp_client_send_packet(
    rtmp_client_t *rc, rtmp_packet_t *packet)
{
    rtmp_result_t result;
    size_t packet_size;

    unsigned char fuck[1024];
    result = rtmp_packet_serialize(
        packet,
        fuck,
        1024,
        rc->amf_chunk_size,
        &packet_size);
    rtmp_client_set_will_send_buffer(
        rc, fuck, packet_size);

    return RTMP_SUCCESS;
}


void rtmp_client_connect(rtmp_client_t *rc)
{
    rtmp_packet_t *rtmp_packet;
    amf_packet_t *amf_object;

    rtmp_packet = (rtmp_packet_t*)rc->data;
    rtmp_packet_cleanup(rtmp_packet);
    rtmp_packet->data_type = RTMP_DATATYPE_INVOKE;
    rtmp_packet->object_id = 3;

    rc->message_number++;
    rtmp_packet_add_amf(
        rtmp_packet,
        amf_packet_create_string("connect"));
    rtmp_packet_add_amf(
        rtmp_packet,
        amf_packet_create_number((double)rc->message_number));

    amf_object = amf_packet_create_object();
    amf_packet_add_property_to_object(
        amf_object, "app", amf_packet_create_string(rc->path));
    amf_packet_add_property_to_object(
        amf_object, "flashVer", amf_packet_create_string("WIN 10,0,12,36"));
    amf_packet_add_property_to_object(
        amf_object, "swfUrl", amf_packet_create_undefined());
    amf_packet_add_property_to_object(
        amf_object, "tcUrl",
        amf_packet_create_string(rc->url));
    amf_packet_add_property_to_object(
        amf_object, "fpad", amf_packet_create_boolean(0));
    amf_packet_add_property_to_object(
        amf_object, "capabilities", amf_packet_create_number(15.0));
    amf_packet_add_property_to_object(
        amf_object, "audioCodecs", amf_packet_create_number(1639.0));
    amf_packet_add_property_to_object(
        amf_object, "videoCodecs", amf_packet_create_number(252.0));
    amf_packet_add_property_to_object(
        amf_object, "videoFunction", amf_packet_create_number(1.0));
    amf_packet_add_property_to_object(
        amf_object, "pageUrl", amf_packet_create_undefined());
    amf_packet_add_property_to_object(
        amf_object, "objectEncoding", amf_packet_create_number(0.0));
    rtmp_packet_add_amf(rtmp_packet, amf_object);

    rtmp_client_send_packet(rc, rtmp_packet);
}


void rtmp_client_create_stream(rtmp_client_t *rc)
{
    rtmp_packet_t *rtmp_packet;

    rtmp_packet = (rtmp_packet_t*)rc->data;
    rtmp_packet_cleanup(rtmp_packet);
    rtmp_packet->data_type = RTMP_DATATYPE_INVOKE;
    rtmp_packet->object_id = 3;

    rc->message_number++;
    rtmp_packet_add_amf(
        rtmp_packet,
        amf_packet_create_string("createStream"));
    rtmp_packet_add_amf(
        rtmp_packet,
        amf_packet_create_number((double)rc->message_number));
    rtmp_packet_add_amf(
        rtmp_packet,
        amf_packet_create_null());

    rtmp_client_send_packet(rc, rtmp_packet);
}


void rtmp_client_play(rtmp_client_t *rc, const char *file_name)
{
    rtmp_packet_t *rtmp_packet;

    rtmp_packet = (rtmp_packet_t*)rc->data;
    rtmp_packet_cleanup(rtmp_packet);
    rtmp_packet->data_type = RTMP_DATATYPE_INVOKE;
    rtmp_packet->object_id = 3;

    rc->message_number++;
    rtmp_packet_add_amf(
        rtmp_packet,
        amf_packet_create_string("play"));
    rtmp_packet_add_amf(
        rtmp_packet,
        amf_packet_create_number((double)rc->message_number));
    rtmp_packet_add_amf(
        rtmp_packet,
        amf_packet_create_null());
    rtmp_packet_add_amf(
        rtmp_packet,
        amf_packet_create_string(file_name));

    rtmp_client_send_packet(rc, rtmp_packet);
}

