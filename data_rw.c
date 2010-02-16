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


#include <string.h>
#include <stdio.h>

#include "data_rw.h"


typedef union endian_checker endian_checker_t;

union endian_checker
{
    long endian_value;
    char byte_array[sizeof(long)];
};

static endian_checker_t EndianChecker = {1};


int is_little_endian(void)
{
    return EndianChecker.byte_array[0];
}


int is_big_endian(void)
{
    return !EndianChecker.byte_array[0];
}


int read_be16int(unsigned char *data)
{
    int value;

    if (is_little_endian()) {
        value  = data[0] << 8;
        value += data[1];
    } else {
        value  = data[0];
        value += data[1] <<  8;
    }
    return value;
}


int read_be24int(unsigned char *data)
{
    int value;

    if (is_little_endian()) {
        value  = data[0] << 16;
        value += data[1] <<  8;
        value += data[2];
    } else {
        value  = data[0];
        value += data[1] <<  8;
        value += data[2] << 16;
    }
    return value;
}


int read_le32int(unsigned char *data)
{
    int value;

    if (is_little_endian()) {
        value  = data[0];
        value += data[1] <<  8;
        value += data[2] << 16;
        value += data[3] << 24;
    } else {
        value  = data[0] << 24;
        value += data[1] << 16;
        value += data[2] <<  8;
        value += data[3];
    }
    return value;
}


int read_be32int(unsigned char *data)
{
    int value;

    if (is_little_endian()) {
        value  = data[0] << 24;
        value += data[1] << 16;
        value += data[2] <<  8;
        value += data[3];
    } else {
        value  = data[0];
        value += data[1] <<  8;
        value += data[2] << 16;
        value += data[3] << 24;
    }
    return value;
}


double read_be64double(unsigned char *data)
{
    double value;
    unsigned char number_data[8];

    if (is_little_endian()) {
        number_data[0] = data[7];
        number_data[1] = data[6];
        number_data[2] = data[5];
        number_data[3] = data[4];
        number_data[4] = data[3];
        number_data[5] = data[2];
        number_data[6] = data[1];
        number_data[7] = data[0];
        memmove(&value, number_data, 8);
    } else {
        memmove(&value, data, 8);
    }

    return value;
}


void write_be64double(unsigned char *data, double value)
{
    unsigned char number_data[8];

    if (is_little_endian()) {
        memmove(number_data, &value, 8);
        data[0] = number_data[7];
        data[1] = number_data[6];
        data[2] = number_data[5];
        data[3] = number_data[4];
        data[4] = number_data[3];
        data[5] = number_data[2];
        data[6] = number_data[1];
        data[7] = number_data[0];
    } else {
        memmove(data, &value, 8);
    }
}


void write_le32int(unsigned char *data, int value)
{
    if (is_little_endian()) {
        data[0] = value & 0xFF;
        data[1] = (value >> 8) & 0xFF;
        data[2] = (value >> 16) & 0xFF;
        data[3] = value >> 24;
    } else {
        data[0] = value >> 24;
        data[1] = (value >> 16) & 0xFF;
        data[2] = (value >> 8) & 0xFF;
        data[3] = value & 0xFF;
    }
}


void write_be32int(unsigned char *data, int value)
{
    if (is_little_endian()) {
        data[0] = value >> 24;
        data[1] = (value >> 16) & 0xFF;
        data[2] = (value >> 8) & 0xFF;
        data[3] = value & 0xFF;
    } else {
        data[0] = value & 0xFF;
        data[1] = (value >> 8) & 0xFF;
        data[2] = (value >> 16) & 0xFF;
        data[3] = value >> 24;
    }
}


void write_be24int(unsigned char *data, int value)
{
    if (is_little_endian()) {
        data[0] = value >> 16;
        data[1] = (value >> 8) & 0xFF;
        data[2] = value & 0xFF;
    } else {
        memmove(data, &value, 3);
    }
}


void write_be16int(unsigned char *data, int value)
{
    unsigned char int_data[2];

    if (is_little_endian()) {
        memmove(int_data, &value, 2);
        data[0] = int_data[1];
        data[1] = int_data[0];
    } else {
        memmove(data, &value, 2);
    }
}
