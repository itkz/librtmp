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

#include "rtmp.h"
#include "amf_packet.h"
#include "data_rw.h"


static amf_packet_t *amf_packet_analyze_number(
    unsigned char *raw_data, size_t raw_data_size, size_t *packet_size);
static amf_packet_t *amf_packet_analyze_boolean(
    unsigned char *raw_data, size_t raw_data_size, size_t *packet_size);
static amf_packet_t *amf_packet_analyze_string(
    unsigned char *raw_data, size_t raw_data_size, size_t *packet_size);
static amf_packet_t *amf_packet_analyze_object(
    unsigned char *raw_data, size_t raw_data_size, size_t *packet_size);
static amf_packet_t *amf_packet_analyze_null(void);
static amf_packet_t *amf_packet_analyze_undefined(void);
static amf_packet_t *amf_packet_analyze_ecma_array(
    unsigned char *raw_data, size_t raw_data_size, size_t *packet_size);

static size_t amf_packet_serialize_number(
    amf_packet_t *amf,
    unsigned char *output_buffer, size_t output_buffer_size);
static size_t amf_packet_serialize_boolean(
    amf_packet_t *amf,
    unsigned char *output_buffer, size_t output_buffer_size);
static size_t amf_packet_serialize_string(
    amf_packet_t *amf,
    unsigned char *output_buffer, size_t output_buffer_size);
static size_t amf_packet_serialize_object(
    amf_packet_t *amf,
    unsigned char *output_buffer, size_t output_buffer_size);
static size_t amf_packet_serialize_null(
    amf_packet_t *amf,
    unsigned char *output_buffer, size_t output_buffer_size);
static size_t amf_packet_serialize_undefined(
    amf_packet_t *amf,
    unsigned char *output_buffer, size_t output_buffer_size);
static size_t amf_packet_serialize_ecma_array(
    amf_packet_t *amf,
    unsigned char *output_buffer, size_t output_buffer_size);


amf_packet_t *amf_packet_analyze_data(
    unsigned char *raw_data, size_t raw_data_size, size_t *packet_size)
{
    amf_packet_t *amf;

    if (raw_data_size < 1) {
        return NULL;
    }

    if (raw_data[0] == 0x00 && raw_data[1] == 0x00 && raw_data[2] == 0x09) {
        /* FIXME: end of object without object start, why? */
        amf = amf_packet_analyze_undefined();
        *packet_size = 3;
        return amf;
    }

    switch (raw_data[0]) {
    case AMF_DATATYPE_NUMBER:
        amf = amf_packet_analyze_number(
            raw_data, raw_data_size, packet_size);
        return amf;
    case AMF_DATATYPE_BOOLEAN:
        amf = amf_packet_analyze_boolean(
            raw_data, raw_data_size, packet_size);
        return amf;
    case AMF_DATATYPE_STRING:
        amf = amf_packet_analyze_string(
            raw_data, raw_data_size, packet_size);
        return amf;
    case AMF_DATATYPE_OBJECT:
        amf = amf_packet_analyze_object(
            raw_data, raw_data_size, packet_size);
        return amf;
    case AMF_DATATYPE_NULL:
        amf = amf_packet_analyze_null();
        *packet_size = 1;
        return amf;
    case AMF_DATATYPE_UNDEFINED:
        amf = amf_packet_analyze_undefined();
        *packet_size = 1;
        return amf;
    case AMF_DATATYPE_ECMA_ARRAY:
        amf = amf_packet_analyze_ecma_array(
            raw_data, raw_data_size, packet_size);
        return amf;
    case AMF_DATATYPE_OBJECT_END:
        
        break;
    default:
        break;
    }

    *packet_size = 0;
    return NULL;
}

amf_packet_t *amf_packet_analyze_number(
    unsigned char *raw_data, size_t raw_data_size, size_t *packet_size)
{
    amf_packet_t *amf;

    if (raw_data_size < 9) {
        if (packet_size) {
            *packet_size = 1;
        }
        return NULL;
    }

    amf = (amf_packet_t*)malloc(sizeof(amf_packet_number_t));
    if (amf == NULL) {
        return NULL;
    }
    amf->datatype = AMF_DATATYPE_NUMBER;
    amf->number.value = read_be64double(raw_data + 1);
#ifdef DEBUG
    printf("AMF number: %f\n", amf->number.value);
#endif
    if (packet_size) {
        *packet_size = 9;
    }
    return amf;
}


amf_packet_t *amf_packet_analyze_boolean(
    unsigned char *raw_data, size_t raw_data_size, size_t *packet_size)
{
    amf_packet_t *amf;

    if (raw_data_size < 2) {
        if (packet_size) {
            *packet_size = 0;
        }
        return NULL;
    }

    amf = (amf_packet_t*)malloc(sizeof(amf_packet_boolean_t));
    if (amf == NULL) {
        return NULL;
    }
    amf->datatype = AMF_DATATYPE_BOOLEAN;

    amf->boolean.value = raw_data[1];
#ifdef DEBUG
    printf("AMF boolean: %d\n", amf->boolean.value);
#endif

    if (packet_size) {
        *packet_size = 2;
    }

    return amf;
}

amf_packet_t *amf_packet_analyze_string(
    unsigned char *raw_data, size_t raw_data_size, size_t *packet_size)
{
    amf_packet_t *amf;
    size_t string_data_length;
    char *string_data;

    if (raw_data_size < 3) {
        if (packet_size) {
            *packet_size = 0;
        }
        return NULL;
    }

    string_data_length = read_be16int(raw_data + 1);
    if (raw_data_size < 3 + string_data_length) {
        if (packet_size) {
            *packet_size = 0;
        }
        return NULL;
    }

    amf = (amf_packet_t*)malloc(sizeof(amf_packet_string_t));
    if (amf == NULL) {
        return NULL;
    }
    amf->datatype = AMF_DATATYPE_STRING;

    string_data = (char*)malloc(string_data_length + 1);
    if (string_data == NULL) {
        free(amf);
        return NULL;
    }
    memset(string_data, 0x00, string_data_length + 1);

    memmove(string_data, raw_data + 3, string_data_length);
    string_data[string_data_length] = '\0';

    amf->string.value = string_data;
#ifdef DEBUG
    printf("AMF string: %d \"%s\"\n", string_data_length, string_data);
#endif

    if (packet_size) {
        /* datatype(1) + length(2) + string */
        *packet_size = 1 + 2 + string_data_length;
    }

    return amf;
}

amf_packet_t *amf_packet_analyze_object(
    unsigned char *raw_data, size_t raw_data_size, size_t *packet_size)
{
    amf_packet_t *amf;
    int raw_data_position;
    size_t string_length;
    size_t property_packet_size;
    amf_packet_object_property_t *property;
    amf_packet_object_property_t *previous_property;

    if (raw_data_size < 1) {
        return NULL;
    }

    amf = (amf_packet_t*)malloc(sizeof(amf_packet_object_t));
    if (amf == NULL) {
        return NULL;
    }
    amf->datatype = AMF_DATATYPE_OBJECT;

    raw_data_position = 1;
    amf->object.properties = NULL;
#ifdef DEBUG
    printf("AMF object start\n");
#endif
    while (1) {
        string_length = read_be16int(raw_data + raw_data_position);
        if (string_length == 0 && raw_data[raw_data_position + 2] == 0x09) {
            raw_data_position += 3;
            break;
        }
        raw_data_position += 2;

        property = (amf_packet_object_property_t*)
            malloc(sizeof(amf_packet_object_property_t));
        if (property == NULL) {
            *packet_size = raw_data_position;
            return NULL;
        }
        property->next = NULL;

        property->key = malloc(string_length + 1);
        if (property->key == NULL) {
            free(property);
            *packet_size = raw_data_position;
            return NULL;
        }
        memmove(property->key, raw_data + raw_data_position, string_length);
printf("out\n");
        property->key[string_length] = '\0';
        raw_data_position += string_length;

#ifdef DEBUG
    printf("AMF object key: %d \"%s\"\n", string_length, property->key);
#endif

        property->value = amf_packet_analyze_data(
            raw_data + raw_data_position,
            raw_data_size - raw_data_position,
            &property_packet_size);
        raw_data_position += property_packet_size;

        if (amf->object.properties == NULL) {
            amf->object.properties = property;
        } else {
            previous_property->next = property;
        }
        previous_property = property;
    }
#ifdef DEBUG
    printf("AMF object end\n");
#endif

    *packet_size = raw_data_position;

    return amf;
}

amf_packet_t *amf_packet_analyze_null(void)
{
    amf_packet_t *amf;
    
    amf = (amf_packet_t*)malloc(sizeof(amf_packet_null_t));
    if (amf == NULL) {
        return NULL;
    }
    amf->datatype = AMF_DATATYPE_NULL;
#ifdef DEBUG
    printf("AMF null\n");
#endif
    
    return amf;
}

amf_packet_t *amf_packet_analyze_undefined(void)
{
    amf_packet_t *amf;
    
    amf = (amf_packet_t*)malloc(sizeof(amf_packet_undefined_t));
    if (amf == NULL) {
        return NULL;
    }
    amf->datatype = AMF_DATATYPE_UNDEFINED;
#ifdef DEBUG
    printf("AMF undefined\n");
#endif
    
    return amf;
}


amf_packet_t *amf_packet_analyze_ecma_array(
    unsigned char *raw_data, size_t raw_data_size, size_t *packet_size)
{
    amf_packet_t *amf;
    int raw_data_position;
    size_t string_length;
    int array_num;
    int i;
    size_t property_packet_size;
    amf_packet_object_property_t *property;
    amf_packet_object_property_t *previous_property;

    if (raw_data_size < 1) {
        return NULL;
    }

    amf = (amf_packet_t*)malloc(sizeof(amf_packet_ecma_array_t));
    if (amf == NULL) {
        return NULL;
    }
    amf->datatype = AMF_DATATYPE_ECMA_ARRAY;

    array_num = read_le32int(raw_data + 1);

    raw_data_position = 5;
    amf->object.properties = NULL;
#ifdef DEBUG
    printf("AMF ecma array start: %d\n", array_num);
#endif
    for (i = 0; i < array_num; ++i) {
        string_length = read_be16int(raw_data + raw_data_position);
        if (string_length == 0 && raw_data[raw_data_position + 2] == 0x09) {
            raw_data_position += 3;
            break;
        }
        raw_data_position += 2;

        property = (amf_packet_object_property_t*)
            malloc(sizeof(amf_packet_object_property_t));
        if (property == NULL) {
            *packet_size = raw_data_position;
            return NULL;
        }
        property->next = NULL;

        property->key = malloc(string_length + 1);
        if (property->key == NULL) {
            free(property);
            *packet_size = raw_data_position;
            return NULL;
        }
        memmove(property->key, raw_data + raw_data_position, string_length);
        property->key[string_length] = '\0';
        raw_data_position += string_length;

#ifdef DEBUG
    printf("AMF ecma array key: %d \"%s\"\n", string_length, property->key);
#endif

        property->value = amf_packet_analyze_data(
            raw_data + raw_data_position,
            raw_data_size - raw_data_position,
            &property_packet_size);
        raw_data_position += property_packet_size;

        if (amf->ecma_array.properties == NULL) {
            amf->ecma_array.properties = property;
        } else {
            previous_property->next = property;
        }
        previous_property = property;
    }
#ifdef DEBUG
    printf("AMF ecma array end\n");
#endif

    *packet_size = raw_data_position;

    return amf;
}


void amf_packet_free(amf_packet_t *amf)
{
    amf_packet_object_property_t *property;
    amf_packet_object_property_t *next;

    switch (amf->datatype) {
    case AMF_DATATYPE_NUMBER:
        break;
    case AMF_DATATYPE_BOOLEAN:
        break;
    case AMF_DATATYPE_STRING:
        free(amf->string.value);
        break;
    case AMF_DATATYPE_OBJECT:
        property = amf->object.properties;
        while (property) {
            next = property->next;
            free(property->key);
            amf_packet_free(property->value);
            free(property);
            property = next;
        }
        break;
    case AMF_DATATYPE_NULL:
        break;
    case AMF_DATATYPE_UNDEFINED:
        break;
    case AMF_DATATYPE_OBJECT_END:
        break;
    default:
        break;
    }

    free(amf);
}


amf_packet_t *amf_packet_create_number(double number)
{
    amf_packet_t *amf;

    amf = (amf_packet_t*)malloc(sizeof(amf_packet_number_t));
    if (amf == NULL) {
        return NULL;
    }
    amf->datatype = AMF_DATATYPE_NUMBER;
    amf->number.value = number;
    return amf;
}


amf_packet_t *amf_packet_create_boolean(int boolean)
{
    amf_packet_t *amf;

    amf = (amf_packet_t*)malloc(sizeof(amf_packet_boolean_t));
    if (amf == NULL) {
        return NULL;
    }
    amf->datatype = AMF_DATATYPE_BOOLEAN;
    amf->boolean.value = boolean;
    return amf;
}


amf_packet_t *amf_packet_create_string(const char *string)
{
    amf_packet_t *amf;

    amf = (amf_packet_t*)malloc(sizeof(amf_packet_string_t));
    if (amf == NULL) {
        return NULL;
    }
    amf->datatype = AMF_DATATYPE_STRING;
    amf->string.value = (char*)malloc(strlen(string) + 1);
    if (amf->string.value == NULL) {
        free(amf);
    }
    strcpy(amf->string.value, string);
    return amf;
}


amf_packet_t *amf_packet_create_object(void)
{
    amf_packet_t *amf;

    amf = (amf_packet_t*)malloc(sizeof(amf_packet_object_t));
    if (amf == NULL) {
        return NULL;
    }
    amf->datatype = AMF_DATATYPE_OBJECT;
    amf->object.properties = NULL;
    return amf;
}


rtmp_result_t amf_packet_add_property_to_object(
    amf_packet_t *amf, const char *key, amf_packet_t *value)
{
    amf_packet_object_property_t *property;
    amf_packet_object_property_t *last_property;

    property = (amf_packet_object_property_t*)
        malloc(sizeof(amf_packet_object_property_t));
    if (property == NULL) {
        return RTMP_ERROR_MEMORY_ALLOCATION;
    }
    property->key = (char*)malloc(strlen(key) + 1);
    if (property->key == NULL) {
        free(property);
        return RTMP_ERROR_MEMORY_ALLOCATION;
    }
    strcpy(property->key, key);
    property->value = value;

    property->next = NULL;
    if (amf->object.properties == NULL) {
        amf->object.properties = property;
    } else {
        last_property = amf->object.properties;
        while (last_property->next) {
            last_property = last_property->next;
        }
        last_property->next = property;
    }

    return RTMP_SUCCESS;
}


amf_packet_t *amf_packet_create_null(void)
{
    amf_packet_t *amf;

    amf = (amf_packet_t*)malloc(sizeof(amf_packet_null_t));
    if (amf == NULL) {
        return NULL;
    }
    amf->datatype = AMF_DATATYPE_NULL;
    return amf;
}


amf_packet_t *amf_packet_create_undefined(void)
{
    amf_packet_t *amf;

    amf = (amf_packet_t*)malloc(sizeof(amf_packet_undefined_t));
    if (amf == NULL) {
        return NULL;
    }
    amf->datatype = AMF_DATATYPE_UNDEFINED;
    return amf;
}


size_t amf_packet_get_size(amf_packet_t *amf)
{
    size_t size;
    amf_packet_object_property_t *property;

    switch (amf->datatype) {
    case AMF_DATATYPE_NUMBER:
        /* datatype(1) + number(8) */
        return 9;
    case AMF_DATATYPE_BOOLEAN:
        /* datatype(1) + boolean(1) */
        return 2;
    case AMF_DATATYPE_STRING:
        /* datatype(1) + length(2) + string */
        return 1 + 2 + strlen(amf->string.value);
    case AMF_DATATYPE_OBJECT:
        property = amf->object.properties;
        size = 1;
        while (property) {
            size += 2 + strlen(property->key); /* length + key */
            size += amf_packet_get_size(property->value);
            property = property->next;
        }
        size += 3; /* length + object end (0x09) */
        return size;
    case AMF_DATATYPE_NULL:
        /* datatype(1) */
        return 1;
    case AMF_DATATYPE_UNDEFINED:
        /* datatype(1) */
        return 1;
    case AMF_DATATYPE_OBJECT_END:
        /* datatype(1) */
        return 1;
    default:
        break;
    }
    return 0;
}


size_t amf_packet_serialize(
    amf_packet_t *amf,
    unsigned char *output_buffer, size_t output_buffer_size)
{
    switch (amf->datatype) {
    case AMF_DATATYPE_NUMBER:
        return amf_packet_serialize_number(
            amf, output_buffer, output_buffer_size);
    case AMF_DATATYPE_BOOLEAN:
        return amf_packet_serialize_boolean(
            amf, output_buffer, output_buffer_size);
    case AMF_DATATYPE_STRING:
        return amf_packet_serialize_string(
            amf, output_buffer, output_buffer_size);
    case AMF_DATATYPE_OBJECT:
        return amf_packet_serialize_object(
            amf, output_buffer, output_buffer_size);
    case AMF_DATATYPE_NULL:
        return amf_packet_serialize_null(
            amf, output_buffer, output_buffer_size);
    case AMF_DATATYPE_UNDEFINED:
        return amf_packet_serialize_undefined(
            amf, output_buffer, output_buffer_size);
    case AMF_DATATYPE_ECMA_ARRAY:
        return amf_packet_serialize_ecma_array(
            amf, output_buffer, output_buffer_size);
    case AMF_DATATYPE_OBJECT_END:
        break;
    default:
        break;
    }
    return 0;
}


size_t amf_packet_serialize_number(
    amf_packet_t *amf,
    unsigned char *output_buffer, size_t output_buffer_size)
{
    if (output_buffer_size < 9) {
        return 0;
    }

    output_buffer[0] = AMF_DATATYPE_NUMBER;
    write_be64double(output_buffer + 1, amf->number.value);
#ifdef DEBUG
    printf("AMF serialize number: %f: ", amf->number.value);
    {
        int i;
        for (i = 0; i < 9; ++i) {
            printf("%02x, ", output_buffer[i]);
        }
    }
    printf("\n");
#endif

    return 9;
}

size_t amf_packet_serialize_boolean(
    amf_packet_t *amf,
    unsigned char *output_buffer, size_t output_buffer_size)
{
    if (output_buffer_size < 2) {
        return 0;
    }

    output_buffer[0] = AMF_DATATYPE_BOOLEAN;
    memmove(output_buffer + 1, &amf->boolean.value, 1);
#ifdef DEBUG
    printf("AMF serialize boolean: %d\n", amf->boolean.value);
#endif

    return 2;
}

size_t amf_packet_serialize_string(
    amf_packet_t *amf,
    unsigned char *output_buffer, size_t output_buffer_size)
{
    size_t length;

    if (output_buffer_size < (3 + strlen(amf->string.value)) ) {
        return 0;
    }

    output_buffer[0] = AMF_DATATYPE_STRING;
    length = strlen(amf->string.value);
    write_be16int(output_buffer + 1, (int)length);
    memmove(output_buffer + 3 , amf->string.value, length);
#ifdef DEBUG
    printf("AMF serialize string: %d: '%s': ", length, amf->string.value);
    {
        int i;
        for (i = 0; i < 3 + (int)length; ++i) {
            printf("%02x, ", output_buffer[i]);
        }
    }
    printf("\n");
#endif

    return 3 + length;
}

size_t amf_packet_serialize_object(
    amf_packet_t *amf,
    unsigned char *output_buffer, size_t output_buffer_size)
{
    amf_packet_object_property_t *property;
    size_t key_length;
    size_t outputed_size;
    size_t serialized_value_size;

    if (output_buffer_size < 1) {
        return 0;
    }

#ifdef DEBUG
    printf("AMF serialize object start\n");
#endif
    output_buffer[0] = AMF_DATATYPE_OBJECT;
    outputed_size = 1;
    property = amf->object.properties;
    while (property) {
        key_length = strlen(property->key);
        if ((outputed_size + 2 + key_length) > output_buffer_size) {
            return 0;
        }
        write_be16int(output_buffer + outputed_size, (int)key_length);
        memmove(
            output_buffer + outputed_size + 2,
            property->key, key_length);
#ifdef DEBUG
    printf("key serialize: %d: '%s': ", key_length, property->key);
    {
        int i;
        for (i = 0; i < 2 + (int)key_length; ++i) {
            printf("%02x, ", output_buffer[outputed_size + i]);
        }
    }
    printf("\n");
#endif
        outputed_size += 2 + key_length;
        serialized_value_size = amf_packet_serialize(
            property->value,
            output_buffer + outputed_size,
            output_buffer_size - outputed_size);
        if (serialized_value_size == 0) {
            return 0;
        }
        outputed_size += serialized_value_size;
        property = property->next;
    }
    if ((outputed_size + 3) > output_buffer_size) {
        return 0;
    }
    write_be16int(output_buffer + outputed_size, 0);
    outputed_size += 2;
    output_buffer[outputed_size] = 0x09;
    outputed_size += 1;
#ifdef DEBUG
    printf("AMF serialize object end\n");
#endif

    return outputed_size;
}

size_t amf_packet_serialize_null(
    amf_packet_t *amf,
    unsigned char *output_buffer, size_t output_buffer_size)
{
    if (output_buffer_size < 1 ) {
        return 0;
    }

    if (amf->datatype == AMF_DATATYPE_NULL) {
        output_buffer[0] = AMF_DATATYPE_NULL;
    }

    return 1;
}

size_t amf_packet_serialize_undefined(
    amf_packet_t *amf,
    unsigned char *output_buffer, size_t output_buffer_size)
{
    if (output_buffer_size < 1 ) {
        return 0;
    }


    if (amf->datatype == AMF_DATATYPE_UNDEFINED) {
        output_buffer[0] = AMF_DATATYPE_UNDEFINED;
    }
#ifdef DEBUG
    printf("AMF serialize undefined\n");
#endif

    return 1;
}


size_t amf_packet_serialize_ecma_array(
    amf_packet_t *amf,
    unsigned char *output_buffer, size_t output_buffer_size)
{
    amf_packet_object_property_t *property;
    size_t key_length;
    size_t outputed_size;
    size_t serialized_value_size;

    if (output_buffer_size < 1) {
        return 0;
    }

#ifdef DEBUG
    printf("AMF serialize ecma array start\n");
#endif
    output_buffer[0] = AMF_DATATYPE_OBJECT;
    outputed_size = 1;
    property = amf->ecma_array.properties;
    while (property) {
        key_length = strlen(property->key);
        if ((outputed_size + 2 + key_length) > output_buffer_size) {
            return 0;
        }
        write_be16int(output_buffer + outputed_size, (int)key_length);
        memmove(
            output_buffer + outputed_size + 2,
            property->key, key_length);
#ifdef DEBUG
    printf("key serialize: %d: '%s': ", key_length, property->key);
    {
        int i;
        for (i = 0; i < 2 + (int)key_length; ++i) {
            printf("%02x, ", output_buffer[outputed_size + i]);
        }
    }
    printf("\n");
#endif
        outputed_size += 2 + key_length;
        serialized_value_size = amf_packet_serialize(
            property->value,
            output_buffer + outputed_size,
            output_buffer_size - outputed_size);
        if (serialized_value_size == 0) {
            return 0;
        }
        outputed_size += serialized_value_size;
        property = property->next;
    }
    if ((outputed_size + 3) > output_buffer_size) {
        return 0;
    }
    write_be16int(output_buffer + outputed_size, 0);
    outputed_size += 2;
    output_buffer[outputed_size] = 0x09;
    outputed_size += 1;
#ifdef DEBUG
    printf("AMF serialize ecma array end\n");
#endif

    return outputed_size;
}
