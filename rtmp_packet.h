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

#ifndef _rtmp_packet_H_
#define _rtmp_packet_H_

#include "rtmp.h"
#include "amf_packet.h"


/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif


#define HEADER_MAGIC_01 3
#define HEADER_MAGIC_04 2
#define HEADER_MAGIC_08 1
#define HEADER_MAGIC_12 0

typedef enum rtmp_datatype rtmp_datatype_t;

enum rtmp_datatype
{
    RTMP_DATATYPE_CHUNK_SIZE    = 0x01,
    RTMP_DATATYPE_UNKNOWN_0     = 0x02,
    RTMP_DATATYPE_BYTES_READ    = 0x03,
    RTMP_DATATYPE_PING          = 0x04,
    RTMP_DATATYPE_SERVER_BW     = 0x05,
    RTMP_DATATYPE_CLIENT_BW     = 0x06,
    RTMP_DATATYPE_UNKNOWN_1     = 0x07,
    RTMP_DATATYPE_AUDIO_DATA    = 0x08,
    RTMP_DATATYPE_VIDEO_DATA    = 0x09,
    RTMP_DATATYPE_UNKNOWN_2     = 0x0A,
    RTMP_DATATYPE_UNKNOWN_3     = 0x0B,
    RTMP_DATATYPE_UNKNOWN_4     = 0x0C,
    RTMP_DATATYPE_UNKNOWN_5     = 0x0D,
    RTMP_DATATYPE_UNKNOWN_6     = 0x0E,
    RTMP_DATATYPE_FLEX_STREAM   = 0x0F,
    RTMP_DATATYPE_FLEX_SHARED_OBJECT = 0x10,
    RTMP_DATATYPE_MESSAGE       = 0x11,
    RTMP_DATATYPE_NOTIFY        = 0x12,
    RTMP_DATATYPE_SHARED_OBJECT = 0x13,
    RTMP_DATATYPE_INVOKE        = 0x14,
    RTMP_DATATYPE_FLV_DATA      = 0x16,
};

typedef enum rtmp_body_type rtmp_body_type_t;

enum rtmp_body_type
{
    RTMP_BODY_TYPE_AMF,
    RTMP_BODY_TYPE_DATA
};

typedef struct rtmp_packet_inner_amf_t rtmp_packet_inner_amf_t;

struct rtmp_packet_inner_amf_t
{
    amf_packet_t *amf;
    rtmp_packet_inner_amf_t *next;
};

typedef struct rtmp_packet_t rtmp_packet_t;

struct rtmp_packet_t
{
    char object_id;
    long timer;
    rtmp_datatype_t data_type;
    long stream_id;
    rtmp_body_type_t body_type;
    rtmp_packet_inner_amf_t *inner_amf_packets;
    unsigned char *body_data;
    size_t body_data_length;
};


extern rtmp_packet_t *rtmp_packet_create(void);
extern void rtmp_packet_free(rtmp_packet_t *packet);

extern rtmp_result_t rtmp_packet_add_amf(
    rtmp_packet_t *packet,
    amf_packet_t *amf);
extern void rtmp_packet_cleanup(rtmp_packet_t *packet);

extern rtmp_result_t rtmp_packet_analyze_data(
    rtmp_packet_t *packet,
    unsigned char *data, size_t data_size,
    size_t amf_chunk_size,
    size_t *packet_size);

extern rtmp_result_t rtmp_packet_serialize(
    rtmp_packet_t *packet,
    unsigned char *output_buffer, size_t output_buffer_size,
    size_t amf_chunk_size,
    size_t *packet_size);

extern rtmp_result_t rtmp_packet_allocate_body_data(
    rtmp_packet_t *packet, size_t length);

extern void rtmp_packet_retrieve_status_info(
    rtmp_packet_t *packet, char **code, char **level);


/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif


#endif
