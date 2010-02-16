
CC = gcc

CFLAGS = -g -Wall -Wextra -Wstrict-aliasing=2 -Wcast-qual -Wcast-align -Wwrite-strings -Wfloat-equal -Wpointer-arith -DDEBUG
# -Wconversion
LDFLAGS = -lws2_32 -lwinmm

TARGET = test.exe
OBJS = main.o rtmp.o rtmp_packet.o amf_packet.o data_rw.o

$(TARGET) : $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LDFLAGS)

clean:
	rm -f $(TARGET) *.o *~

main.o: main.c rtmp.h

rtmp.o: rtmp.c rtmp.h rtmp_packet.h amf_packet.h

rtmp_packet.o: rtmp_packet.c rtmp_packet.h amf_packet.h data_rw.h

amf_packet.o: amf_packet.c amf_packet.h data_rw.h

data_rw.o: data_rw.c data_rw.h

