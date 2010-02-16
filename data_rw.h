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

#ifndef _data_rw_H_
#define _data_rw_H_


/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif


extern int is_little_endian(void);
extern int is_big_endian(void);

extern int read_be16int(unsigned char *data);
extern int read_le32int(unsigned char *data);
extern int read_be32int(unsigned char *data);
extern int read_be24int(unsigned char *data);
extern double read_be64double(unsigned char *data);

extern void write_be16int(unsigned char *data, int value);
extern void write_be24int(unsigned char *data, int value);
extern void write_le32int(unsigned char *data, int value);
extern void write_be32int(unsigned char *data, int value);
extern void write_be64double(unsigned char *data, double value);


/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif


#endif
