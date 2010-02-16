

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#if defined(__WIN32__) || defined(WIN32)
#include <windows.h>
#endif

#include "rtmp.h"
#include "rtmp_packet.h"
#include "amf_packet.h"


int main(void)
{

    rtmp_client_t *rc;
    int count;
    rtmp_event_t *event;

    rc = rtmp_client_create("rtmp://192.168.157.1:1935/fastplay/");
    if (rc == NULL) {
        printf("failed\n");
        return -1;
    }

    printf("created.\n");

    count = 20;
    while (count--) {
        rtmp_client_process_message(rc);
        event = rtmp_client_get_event(rc);
        if (event != NULL) {
            if (strcmp(event->code, "NetConnection.Connect.Success") == 0) {
                rtmp_client_create_stream(rc);
                rtmp_client_play(rc, "test.mp4");
            }
            rtmp_client_delete_event(rc);
        }
        sleep(1);
    }

    rtmp_client_free(rc);

    return 0;
}
