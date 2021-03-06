/*
 * transport_layer.h
 *
 *  Created on: Aug 30, 2011
 *      Author: kirill
 */

#include <stdio.h>
#include <string.h>

#include <cnet.h>

#include "assert.h"
#include "dl_frame.h"
#include "datalink_container.h"
#include "datalink_layer.h"
#include "network_layer.h"

#ifndef max
        #define max( a, b ) ( ((a) > (b)) ? (a) : (b) )
#endif
//-----------------------------------------------------------------------------
// an array of link timers
CnetTimerID datalink_timers[MAX_LINKS_COUNT];
// an array of output queues for each link
QUEUE output_queues[MAX_LINKS_COUNT];
uint64_t output_max[MAX_LINKS_COUNT+1];
//-----------------------------------------------------------------------------
// read a frame from physical layer
void read_datalink(CnetEvent event, CnetTimerID timer, CnetData data) {
    int link;
    DL_FRAME frame;
    size_t len = PACKET_HEADER_SIZE+DATAGRAM_HEADER_SIZE +
            DL_FRAME_HEADER_SIZE + MAX_MESSAGE_SIZE;
    CHECK(CNET_read_physical(&link, (char *)&frame, &len));

    // compare the checksum
    size_t dtg_len = len - DL_FRAME_HEADER_SIZE;
    uint16_t checksum = frame.checksum;
    uint16_t checksum_to_compare = CNET_ccitt((unsigned char *)&frame.data, dtg_len);
    if (checksum_to_compare != checksum) {
        return;
    }

    //read a datagram to network layer
    read_network(link, dtg_len, (char*) frame.data);
}
//-----------------------------------------------------------------------------
// write a frame to the link
void write_datalink(int link, char *datagram, uint32_t length) {
    DTG_CONTAINER container;
    container.len = length;
    container.link = link;
    container.checksum = CNET_ccitt((unsigned char *) datagram, (size_t)length);

    size_t datagram_length = length;
    memcpy(&container.data, datagram, datagram_length);

    // check if timer is null to avoid polling
    if (datalink_timers[link] == NULLTIMER) {
        CnetTime timeout_flush = 1;
        datalink_timers[link] = CNET_start_timer(EV_DATALINK_FLUSH, timeout_flush, link);
    }

    output_max[link] = max(output_max[link], queue_nitems(output_queues[link]));

    // add to the link queue
    if(queue_nitems(output_queues[link]) < 5000) {
        size_t container_length = DTG_CONTAINER_SIZE(container);
        queue_add(output_queues[link], &container, container_length);
    }
}
//-----------------------------------------------------------------------------
// flush a queue
void flush_datalink_queue(CnetEvent ev, CnetTimerID t1, CnetData data) {
    int current_link = (int) data;
    if (queue_nitems(output_queues[current_link]) > 0) {
        // take a first datalink frame
        size_t containter_len;
        DTG_CONTAINER * dtg_container = queue_remove(output_queues[current_link], &containter_len);

        // write datalink frame to the link
        DL_FRAME frame;
        size_t datagram_length = dtg_container->len;
        frame.checksum = dtg_container->checksum;
        memcpy(&frame.data, dtg_container->data, datagram_length);
        size_t frame_length = datagram_length + DL_FRAME_HEADER_SIZE;

        int link = dtg_container->link;
        CHECK(CNET_write_physical(link, (char *)&frame, &frame_length));

        //compute timeout for the link
        double bandwidth = linkinfo[link].bandwidth;
        CnetTime timeout = 1+8000005.0*(frame_length / bandwidth);
        datalink_timers[link] = CNET_start_timer(EV_DATALINK_FLUSH, timeout, current_link);
        free(dtg_container);
    } else {
        datalink_timers[current_link] = NULLTIMER;
    }
}
//-----------------------------------------------------------------------------
// initialization: called by reboot_node
void init_datalink() {
    //set event handler for physical ready event
    CHECK(CNET_set_handler( EV_PHYSICALREADY, read_datalink, 0));
    CHECK(CNET_set_handler( EV_DATALINK_FLUSH, flush_datalink_queue, 0));

    for (int i = 1; i <= nodeinfo.nlinks; i++) {
        datalink_timers[i] = NULLTIMER;
        output_queues[i] = queue_new();
        output_max[i] = 0;
    }
}
//-----------------------------------------------------------------------------
void shutdown_datalink() {
    int packets[MAX_LINKS_COUNT];
    uint64_t memory_datalink = 0;
    for (int i = 1; i <= nodeinfo.nlinks; i++) {
        packets[i] = queue_nitems(output_queues[i]);
        while(queue_nitems(output_queues[i]) != 0) {
            size_t containter_len;
            DTG_CONTAINER * dtg_container = queue_remove(output_queues[i], &containter_len);
            memory_datalink += containter_len;
            free(dtg_container);
        }
    }

    fprintf(stderr, "\tdatalink memory: %f(MB)\n", (double)memory_datalink/(double)(1024*1024*8));

    for (int i = 1; i <= nodeinfo.nlinks; i++) {
        //fprintf(stderr, "\tlink %d - datalink queue: %d packets max: %lu\n", i, packets[i], output_max[i]);
        queue_free(output_queues[i]);
    }
}
//-----------------------------------------------------------------------------
