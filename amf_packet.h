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

#ifndef _amf_packet_H_
#define _amf_packet_H_

#include "rtmp.h"


/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif


#define AMF_CHANK_SIZE 128

typedef enum amf_datatype amf_datatype_t;

enum amf_datatype
{
    AMF_DATATYPE_NUMBER       = 0x00,
    AMF_DATATYPE_BOOLEAN      = 0x01,
    AMF_DATATYPE_STRING       = 0x02,
    AMF_DATATYPE_OBJECT       = 0x03,
    AMF_DATATYPE_MOVIECLIP    = 0x04,
    AMF_DATATYPE_NULL         = 0x05,
    AMF_DATATYPE_UNDEFINED    = 0x06,
    AMF_DATATYPE_REFERENCE    = 0x07,
    AMF_DATATYPE_ECMA_ARRAY   = 0x08,
    AMF_DATATYPE_OBJECT_END   = 0x09,
    AMF_DATATYPE_STRICT_ARRAY = 0x0A,
    AMF_DATATYPE_DATE         = 0x0B,
    AMF_DATATYPE_LONG_STRING  = 0x0C,
    AMF_DATATYPE_UNSUPPORTED  = 0x0D,
    AMF_DATATYPE_RECORDSET    = 0x0E,
    AMF_DATATYPE_XML_DOCUMENT = 0x0F,
    AMF_DATATYPE_TYPED_OBJECT = 0x10,
};

typedef struct amf_packet_number_t amf_packet_number_t;
typedef struct amf_packet_boolean_t amf_packet_boolean_t;
typedef struct amf_packet_string_t amf_packet_string_t;
typedef struct amf_packet_object_property_t amf_packet_object_property_t;
typedef struct amf_packet_object_t amf_packet_object_t;
typedef struct amf_packet_null_t amf_packet_null_t;
typedef struct amf_packet_undefined_t amf_packet_undefined_t;
typedef struct amf_packet_ecma_array_property_t amf_packet_ecma_array_property_t;
typedef struct amf_packet_ecma_array_t amf_packet_ecma_array_t;
typedef struct amf_packet_object_end_t amf_packet_object_end_t;
typedef union amf_packet_t amf_packet_t;

struct amf_packet_number_t
{
    amf_datatype_t datatype;
    double value;
};

struct amf_packet_boolean_t
{
    amf_datatype_t datatype;
    int value;
};

struct amf_packet_string_t
{
    amf_datatype_t datatype;
    char *value;
};

struct amf_packet_object_property_t
{
    char *key;
    amf_packet_t *value;
    amf_packet_object_property_t *next;
};

struct amf_packet_object_t
{
    amf_datatype_t datatype;
    amf_packet_object_property_t *properties;
};

struct amf_packet_null_t
{
    amf_datatype_t datatype;
};

struct amf_packet_undefined_t
{
    amf_datatype_t datatype;
};

struct amf_packet_ecma_array_t
{
    amf_datatype_t datatype;
    int num;
    amf_packet_object_property_t *properties;
};

struct amf_packet_object_end_t
{
    amf_datatype_t datatype;
};

union amf_packet_t
{
    amf_datatype_t datatype;
    amf_packet_number_t number;
    amf_packet_boolean_t boolean;
    amf_packet_string_t string;
    amf_packet_object_t object;
    amf_packet_null_t null;
    amf_packet_undefined_t undefined;
    amf_packet_ecma_array_t ecma_array;
    amf_packet_object_end_t object_end;
};


extern amf_packet_t *amf_packet_analyze_data(
    unsigned char *data, size_t data_size, size_t *packet_size);

extern amf_packet_t *amf_packet_create_number(double number);
extern amf_packet_t *amf_packet_create_boolean(int boolean);
extern amf_packet_t *amf_packet_create_string(const char *string);
extern amf_packet_t *amf_packet_create_null(void);
extern amf_packet_t *amf_packet_create_undefined(void);
extern amf_packet_t *amf_packet_create_object(void);
extern rtmp_result_t amf_packet_add_property_to_object(
    amf_packet_t *amf, const char *key, amf_packet_t *value);

extern size_t amf_packet_get_size(amf_packet_t *amf);

extern size_t amf_packet_serialize(
    amf_packet_t *amf,
    unsigned char *output_buffer, size_t output_buffer_size);

extern void amf_packet_free(amf_packet_t *amf);


/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif


#endif
