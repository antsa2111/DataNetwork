/*
 * transport_layer.h
 *
 *  Created on: Jul 29, 2011
 *      Author: isti
 */

#ifndef TRANSPORT_LAYER_H_
#define TRANSPORT_LAYER_H_

#include <string.h>
#include "packet.h"
#include "network_layer.h"
//timer setting for pending discovery process
#define DISCOVERY_PENDING_TIME 400000
#define EV_DISCOVERY_PENDING_TIMER  EV_TIMER8
#define EV_FLUSH_TRANSPORT_QUEUE EV_TIMER4
//A packet container to store packets while routing
typedef struct {
	uint16_t dest;
	uint32_t len;
	char packet[MAX_MESSAGE_SIZE+PACKET_HEADER_SIZE];
} __attribute__((packed)) PKT_CONTAINER;
#define PKT_CONTAINER_HEADER_SIZE  (sizeof(uint16_t)+sizeof(uint32_t))
#define PKT_CONTAINER_SIZE(f) (PKT_CONTAINER_HEADER_SIZE +PACKET_HEADER_SIZE+ f.len)


//-----------------------------------------------------------------------------
// initialize transport layer
extern void init_transport();
//-----------------------------------------------------------------------------
// read outgoing message from application to transport layer
extern void write_transport(CnetEvent ev, CnetTimerID timer, CnetData data);
//-----------------------------------------------------------------------------
// write incoming message from network to transport
extern void read_transport(PACKETKIND,uint16_t,CnetAddr,char*);
//-----------------------------------------------------------------------------

#endif /* TRANSPORT_LAYER_H_ */
