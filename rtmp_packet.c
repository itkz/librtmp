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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rtmp_packet.h"
#include "amf_packet.h"
#include "data_rw.h"


void rtmp_packet_retrieve_body(
    unsigned char *body_output_buffer,
    unsigned char *body_chunks, size_t rtmp_body_size,
    size_t amf_chunk_size, int chunk_delimiter_num);
static size_t rtmp_packet_insert_amf_chunk_header(
    unsigned char *amf_buffer,
    size_t amf_size,
    size_t amf_chunk_size,
    unsigned char *output_buffer);
static rtmp_result_t rtmp_packet_amf_analyze(
    rtmp_packet_t *packet,
    unsigned char *amf_packets_buffer, size_t rtmp_body_size);
static rtmp_result_t rtmp_packet_process_body(
    rtmp_packet_t *packet, unsigned char *body_buffer, size_t body_size);
static rtmp_result_t rtmp_packet_serialize_amf(
    rtmp_packet_t *packet,
    size_t *total_serialized_size,
    size_t amf_size, size_t amf_chunk_size, size_t header_size,
    unsigned char *output_buffer, size_t output_buffer_size);
static rtmp_result_t rtmp_packet_serialize_data(
    rtmp_packet_t *packet,
    size_t *total_serialized_size,
    size_t amf_chunk_size, size_t header_size,
    unsigned char *output_buffer, size_t output_buffer_size);


rtmp_packet_t *rtmp_packet_create(void)
{
    rtmp_packet_t *packet;

    packet = (rtmp_packet_t*)malloc(sizeof(rtmp_packet_t));
    packet->inner_amf_packets = NULL;
    packet->body_data = NULL;
    rtmp_packet_cleanup(packet);

    return packet;
}


void rtmp_packet_cleanup(rtmp_packet_t *packet)
{
    rtmp_packet_inner_amf_t *inner_amf;
    rtmp_packet_inner_amf_t *next;

    packet->object_id = 0;
    packet->timer = 0;
    packet->data_type = 0;
    packet->stream_id = 0;
    inner_amf = packet->inner_amf_packets;
    while (inner_amf) {
        next = inner_amf->next;
        amf_packet_free(inner_amf->amf);
        free(inner_amf);
        inner_amf = next;
    }
    packet->inner_amf_packets = NULL;
    if (packet->body_data) {
        free(packet->body_data);
        packet->body_data = NULL;
    }
}


void rtmp_packet_retrieve_body(
    unsigned char *body_output_buffer,
    unsigned char *body_chunks, size_t rtmp_body_size,
    size_t amf_chunk_size, int chunk_delimiter_num)
{
    size_t body_size_count;
    size_t body_moved_size;
    size_t rtmp_body_with_chunk_delimiter_size;

    body_size_count = 0;
    body_moved_size = 0;
    rtmp_body_with_chunk_delimiter_size =
        rtmp_body_size + chunk_delimiter_num;
    while (body_size_count < rtmp_body_with_chunk_delimiter_size) {
        size_t rest = rtmp_body_with_chunk_delimiter_size - body_size_count;
        if (rest < amf_chunk_size) {
            memmove(
                body_output_buffer + body_moved_size,
                body_chunks + body_size_count,
                rest);
        } else {
            memmove(
                body_output_buffer + body_moved_size,
                body_chunks + body_size_count,
                amf_chunk_size);
        }
        body_moved_size += amf_chunk_size;
        body_size_count += amf_chunk_size + 1; /* 1 is delimiter(0xC3) */
    }
}


rtmp_result_t rtmp_packet_analyze_data(
    rtmp_packet_t *packet,
    unsigned char *data, size_t data_size,
    size_t amf_chunk_size,
    size_t *packet_size)
{
    int header_size_magic;
    int header_size;
    size_t rtmp_body_size;
    unsigned char *body_chunks;
    rtmp_result_t amf_ret;
    int chunk_delimiter_num;
    size_t amf_size_count;
    unsigned char *body_buffer;

    if (data_size == 0) {
        *packet_size = 0;
        return RTMP_ERROR_UNKNOWN;
    }

    rtmp_packet_cleanup(packet);

#ifdef DEBUG
    printf("RTMP packet start\n");
#endif
    header_size_magic = data[0] >> 6;
#ifdef DEBUG
    printf("header_size_magic: %d\n", header_size_magic);
#endif
    packet->object_id = data[0] & 0x3F;
#ifdef DEBUG
    printf("object_id: %d\n", packet->object_id);
#endif
    if (header_size_magic == HEADER_MAGIC_01) {
        header_size = 1;
        *packet_size = 1;
        if (data_size < 1) {
            *packet_size = 0;
            return RTMP_ERROR_DIVIDED_PACKET;
        }
        return RTMP_SUCCESS;
    }
    if (data_size < 4) {
        *packet_size = 0;
        return RTMP_ERROR_DIVIDED_PACKET;
    }
    packet->timer = read_be24int(data + 1);
#ifdef DEBUG
    printf("timer: %ld\n", packet->timer);
#endif
    if (header_size_magic == HEADER_MAGIC_04) {
        header_size = 4;
        *packet_size = 4;
        return RTMP_SUCCESS;
    }
    if (data_size < 8) {
        *packet_size = 0;
        return RTMP_ERROR_DIVIDED_PACKET;
    }
    rtmp_body_size = read_be24int(data + 4);
#ifdef DEBUG
    printf("rtmp_body_size: %d\n", rtmp_body_size);
#endif
    chunk_delimiter_num = 0;
    amf_size_count = amf_chunk_size;
    while (amf_size_count < rtmp_body_size) {
        chunk_delimiter_num++;
        amf_size_count += amf_chunk_size;
    }
#ifdef DEBUG
    printf("chunk_delimiter_num: %d\n", chunk_delimiter_num);
#endif
    packet->data_type = data[7];
#ifdef DEBUG
    printf("data_type: %02x\n", packet->data_type);
#endif
    if (header_size_magic == HEADER_MAGIC_08) {
        header_size = 8;
        if (rtmp_body_size == 0) {
            *packet_size = 8;
            return RTMP_SUCCESS;
        }
        body_chunks = data + 8;
    } else if (header_size_magic == HEADER_MAGIC_12) {
        if (data_size < 12) {
            *packet_size = 0;
            return RTMP_ERROR_DIVIDED_PACKET;
        }
        header_size = 12;
        packet->stream_id = read_le32int(data + 8);
        if (rtmp_body_size == 0) {
            *packet_size = 12;
            return RTMP_SUCCESS;
        }
        body_chunks = data + 12;
    }
    if (header_size + rtmp_body_size > data_size) {
        *packet_size = 0;
        return RTMP_ERROR_BROKEN_PACKET;
    }
    *packet_size = header_size + rtmp_body_size + chunk_delimiter_num;

    body_buffer = (unsigned char*)malloc(rtmp_body_size);
    printf("allocated %d\n", rtmp_body_size);
    if (body_buffer == NULL) {
        return RTMP_ERROR_MEMORY_ALLOCATION;
    }
    rtmp_packet_retrieve_body(
        body_buffer, body_chunks, rtmp_body_size,
        amf_chunk_size, chunk_delimiter_num);
    amf_ret = rtmp_packet_process_body(packet, body_buffer, rtmp_body_size);

    if (amf_ret != RTMP_SUCCESS) {
        return amf_ret;
    }

#ifdef DEBUG
    printf("RTMP packet end\n");
#endif
    return RTMP_SUCCESS;
}


rtmp_result_t rtmp_packet_process_body(
    rtmp_packet_t *packet, unsigned char *body_buffer, size_t body_size)
{
    rtmp_result_t amf_ret;

    switch (packet->data_type) {
    case RTMP_DATATYPE_CHUNK_SIZE:
        packet->body_type = RTMP_BODY_TYPE_DATA;
        packet->body_data = body_buffer;
        packet->body_data_length = body_size;
        break;
    case RTMP_DATATYPE_UNKNOWN_0:
    case RTMP_DATATYPE_BYTES_READ:
    case RTMP_DATATYPE_PING:
    case RTMP_DATATYPE_SERVER_BW:
    case RTMP_DATATYPE_CLIENT_BW:
    case RTMP_DATATYPE_UNKNOWN_1:
    case RTMP_DATATYPE_AUDIO_DATA:
    case RTMP_DATATYPE_VIDEO_DATA:
    case RTMP_DATATYPE_UNKNOWN_2:
    case RTMP_DATATYPE_UNKNOWN_3:
    case RTMP_DATATYPE_UNKNOWN_4:
    case RTMP_DATATYPE_UNKNOWN_5:
    case RTMP_DATATYPE_UNKNOWN_6:
    case RTMP_DATATYPE_FLEX_STREAM:
    case RTMP_DATATYPE_FLEX_SHARED_OBJECT:
    case RTMP_DATATYPE_MESSAGE:
    case RTMP_DATATYPE_NOTIFY:
    case RTMP_DATATYPE_SHARED_OBJECT:
        packet->body_type = RTMP_BODY_TYPE_DATA;
        packet->body_data = body_buffer;
        packet->body_data_length = body_size;
        break;
    case RTMP_DATATYPE_INVOKE:
        amf_ret = rtmp_packet_amf_analyze(packet, body_buffer, body_size);
        free(body_buffer);
        if (amf_ret == RTMP_SUCCESS) {
            packet->body_type = RTMP_BODY_TYPE_AMF;
	}
        return amf_ret;
    case RTMP_DATATYPE_FLV_DATA:
        packet->body_type = RTMP_BODY_TYPE_DATA;
        packet->body_data = body_buffer;
        break;
    }
#ifdef DEBUG
    if (packet->body_type == RTMP_BODY_TYPE_DATA) {
        int i;
        printf("RTMP data start: %d\n", (int)packet->body_data_length);
        for (i = 0; i < (int)packet->body_data_length; ++i) {
	    printf("%02x ", packet->body_data[i]);
	}
	printf("\n");
    }
#endif
    return RTMP_SUCCESS;
}


rtmp_result_t rtmp_packet_amf_analyze(
    rtmp_packet_t *packet,
    unsigned char *amf_buffer, size_t amf_buffer_size)
{
    size_t buffer_position;
    amf_packet_t *amf;
    rtmp_packet_inner_amf_t *inner_amf;
    rtmp_packet_inner_amf_t *prev_inner_amf;
    size_t analyzed_amf_packet_size;

    buffer_position = 0;
    packet->inner_amf_packets = NULL;
    while (buffer_position < amf_buffer_size) {
        inner_amf = (rtmp_packet_inner_amf_t*)malloc(
            sizeof(rtmp_packet_inner_amf_t));
        if (inner_amf == NULL) {
            return RTMP_ERROR_MEMORY_ALLOCATION;
        }
        amf = amf_packet_analyze_data(
            amf_buffer + buffer_position,
            amf_buffer_size - buffer_position,
            &analyzed_amf_packet_size);
        buffer_position += analyzed_amf_packet_size;
        if (amf == NULL) {
            free(inner_amf);
            return RTMP_ERROR_BROKEN_PACKET;
        }
        inner_amf->amf = amf;
        inner_amf->next = NULL;
        if (packet->inner_amf_packets == NULL) {
            packet->inner_amf_packets = inner_amf;
        } else {
            prev_inner_amf->next = inner_amf;
        }
        prev_inner_amf = inner_amf;
    }

    return RTMP_SUCCESS;
}


rtmp_result_t rtmp_packet_serialize(
    rtmp_packet_t *packet,
    unsigned char *output_buffer, size_t output_buffer_size,
    size_t amf_chunk_size,
    size_t *packet_size)
{
    size_t header_size;
    size_t amf_size;
    size_t total_serialized_size;
    rtmp_packet_inner_amf_t *inner_amf;
    rtmp_result_t result;

    header_size = 12;

#ifdef DEBUG
    printf("RTMP packet serialize start\n");
#endif
    if (header_size > output_buffer_size) {
        *packet_size = 0;
        return RTMP_ERROR_LACKED_MEMORY;
    }

    output_buffer[0] = (HEADER_MAGIC_12 << 6) + packet->object_id;
    write_be24int(output_buffer + 1, packet->timer);

    if (packet->body_type == RTMP_BODY_TYPE_AMF) {
        amf_size = 0;
        inner_amf = packet->inner_amf_packets;
        while (inner_amf) {
            amf_size += amf_packet_get_size(inner_amf->amf);
            inner_amf = inner_amf->next;
        }
        write_be24int(output_buffer + 4, (int)amf_size);
    } else if (packet->body_type == RTMP_BODY_TYPE_DATA) {
        amf_size = 0;
        write_be24int(output_buffer + 4, (int)packet->body_data_length);
    }

#ifdef DEBUG
    printf("data_type: %02x\n", packet->data_type);
#endif

    output_buffer[7] = packet->data_type;
    write_le32int(output_buffer + 8, packet->stream_id);
    total_serialized_size = header_size;

    if (packet->body_type == RTMP_BODY_TYPE_AMF) {
        result = rtmp_packet_serialize_amf(
            packet,
            &total_serialized_size,
            amf_size, amf_chunk_size, header_size,
            output_buffer, output_buffer_size);
        if (result != RTMP_SUCCESS) {
            *packet_size = 0;
            return result;
        }
    } else if (packet->body_type == RTMP_BODY_TYPE_DATA) {
        result = rtmp_packet_serialize_data(
            packet,
            &total_serialized_size,
            amf_chunk_size, header_size,
            output_buffer, output_buffer_size);
        if (result != RTMP_SUCCESS) {
            *packet_size = 0;
            return result;
        }
    }

    *packet_size = total_serialized_size;
#ifdef DEBUG
    printf("RTMP packet serialize end\n");
#endif

    return RTMP_SUCCESS;
}


rtmp_result_t rtmp_packet_serialize_amf(
    rtmp_packet_t *packet,
    size_t *total_serialized_size,
    size_t amf_size, size_t amf_chunk_size, size_t header_size,
    unsigned char *output_buffer, size_t output_buffer_size)
{
    size_t amf_with_chunk_header_size;
    unsigned char *amf_buffer;
    size_t total_serialized_amf_size;
    size_t serialized_amf_size;
    int chunk_delimiter_num;
    rtmp_packet_inner_amf_t *inner_amf;

    chunk_delimiter_num = (int)(amf_size / amf_chunk_size);
    amf_with_chunk_header_size = amf_size + chunk_delimiter_num;
    if (header_size + amf_with_chunk_header_size > output_buffer_size) {
        return RTMP_ERROR_LACKED_MEMORY;
    }

    amf_buffer = (unsigned char*)malloc(amf_size);

#ifdef DEBUG
    printf("AMF serialize start\n");
#endif
    total_serialized_amf_size = 0;
    inner_amf = packet->inner_amf_packets;
    while (inner_amf) {
        serialized_amf_size = amf_packet_serialize(
            inner_amf->amf,
            amf_buffer + total_serialized_amf_size,
            amf_size - total_serialized_amf_size);
        if (serialized_amf_size == 0) {
#ifdef DEBUG
            printf("AMF serialize error!\n");
#endif
        }
        total_serialized_amf_size += serialized_amf_size;
        inner_amf = inner_amf->next;
    }
#ifdef DEBUG
    printf("AMF serialize end\n");
#endif

    *total_serialized_size += rtmp_packet_insert_amf_chunk_header(
        amf_buffer, amf_size,
        amf_chunk_size,
        output_buffer + *total_serialized_size);

    free(amf_buffer);

    return RTMP_SUCCESS;
}


rtmp_result_t rtmp_packet_serialize_data(
    rtmp_packet_t *packet,
    size_t *total_serialized_size,
    size_t amf_chunk_size, size_t header_size,
    unsigned char *output_buffer, size_t output_buffer_size)
{
    size_t amf_with_chunk_header_size;
    int chunk_delimiter_num;

    chunk_delimiter_num = (int)(packet->body_data_length / amf_chunk_size);
    amf_with_chunk_header_size = packet->body_data_length + chunk_delimiter_num;
    if (header_size + amf_with_chunk_header_size > output_buffer_size) {
        return RTMP_ERROR_LACKED_MEMORY;
    }

#ifdef DEBUG
    printf("RTMP data start\n");
    {
        int i;
        for (i = 0; i < (int)packet->body_data_length; ++i) {
	    printf("%02x ", packet->body_data[i]);
	}
	printf("\n");
    }
#endif
    *total_serialized_size += rtmp_packet_insert_amf_chunk_header(
        packet->body_data, packet->body_data_length,
        amf_chunk_size,
        output_buffer + *total_serialized_size);
#ifdef DEBUG
    printf("RTMP data end\n");
#endif

    return RTMP_SUCCESS;
}


size_t rtmp_packet_insert_amf_chunk_header(
    unsigned char *amf_buffer,
    size_t amf_size,
    size_t amf_chunk_size,
    unsigned char *output_buffer)
{
    size_t amf_buffer_count;
    size_t total_serialized_size;
    size_t remaining_chunk_size;

    total_serialized_size = 0;
    amf_buffer_count = 0;
    remaining_chunk_size = amf_size - amf_buffer_count;
    while (remaining_chunk_size > 0) {
        remaining_chunk_size = amf_size - amf_buffer_count;
        if (remaining_chunk_size > amf_chunk_size) {
            memmove(
                output_buffer + total_serialized_size,
                amf_buffer + amf_buffer_count,
                amf_chunk_size);
            amf_buffer_count += amf_chunk_size;
            total_serialized_size += amf_chunk_size;
            output_buffer[total_serialized_size] = 0xC3;
            total_serialized_size++;
        } else {
            memmove(
                output_buffer + total_serialized_size,
                amf_buffer + amf_buffer_count,
                remaining_chunk_size);
            amf_buffer_count += remaining_chunk_size;
            total_serialized_size += remaining_chunk_size;
        }
    }
    return total_serialized_size;
}


void rtmp_packet_free(rtmp_packet_t *packet)
{
    rtmp_packet_inner_amf_t *inner_amf;
    rtmp_packet_inner_amf_t *next;

    inner_amf = packet->inner_amf_packets;
    while (inner_amf) {
        next = inner_amf->next;
        amf_packet_free(inner_amf->amf);
        free(inner_amf);
        inner_amf = next;
    }
    if (packet->body_data) {
        free(packet->body_data);
    }
    free(packet);
}


rtmp_result_t rtmp_packet_add_amf(
    rtmp_packet_t *packet,
    amf_packet_t *amf)
{
    rtmp_packet_inner_amf_t *inner_amf;
    rtmp_packet_inner_amf_t *last_inner_amf;

    inner_amf = (rtmp_packet_inner_amf_t*)malloc(
        sizeof(rtmp_packet_inner_amf_t));
    if (inner_amf == NULL) {
        return RTMP_ERROR_MEMORY_ALLOCATION;
    }
    inner_amf->amf = amf;
    inner_amf->next = NULL;
    if (packet->inner_amf_packets == NULL) {
        packet->inner_amf_packets = inner_amf;
    } else {
        last_inner_amf = packet->inner_amf_packets;
        while (last_inner_amf->next) {
            last_inner_amf = last_inner_amf->next;
        }
        last_inner_amf->next = inner_amf;
    }
    return RTMP_SUCCESS;
}


rtmp_result_t rtmp_packet_allocate_body_data(
    rtmp_packet_t *packet, size_t length)
{
    if (packet->body_data) {
        free(packet->body_data);
    }
    packet->body_data = (unsigned char*)malloc(length);
    if (!packet->body_data) {
        packet->body_data_length = 0;
	return RTMP_ERROR_MEMORY_ALLOCATION;
    }
    packet->body_data_length = length;
    return RTMP_SUCCESS;
}


void rtmp_packet_retrieve_status_info(
    rtmp_packet_t *packet, char **code, char **level)
{
    rtmp_packet_inner_amf_t *inner_amf;
    amf_packet_t *amf;
    amf_packet_object_property_t *properties;

    *code = NULL;
    *level = NULL;

    inner_amf = packet->inner_amf_packets;
    while (inner_amf) {
        amf = inner_amf->amf;
        if (amf->datatype == AMF_DATATYPE_OBJECT) {
            properties = amf->object.properties;
            while (properties) {
                if (strcmp(properties->key, "code") == 0) {
                    *code = properties->value->string.value;
                } if (strcmp(properties->key, "level") == 0) {
                    *level = properties->value->string.value;
                }
                properties = properties->next;
            }
        }
        inner_amf = inner_amf->next;
    }
}
