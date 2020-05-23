#ifndef runicast_data_c
#define runicast_data_c

#include "../Common/constants.c"
#include "../Common/packetStruct.c"


static struct runicast_conn runicast_data;


static void 
recv_data_runicast(const linkaddr_t *from, const struct data_packet *data_packet);


// Receive data packet
static void
recv_data_general_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno)
{
  struct data_packet *data_packet = packetbuf_dataptr();
  recv_data_runicast(from, data_packet);
}

static void
sent_data_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions);

static void
timedout_data_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions);


static const struct runicast_callbacks runicast_data_callbacks = {
                   recv_data_general_runicast,
                   sent_data_runicast,
                   timedout_data_runicast};

static void 
open_runicast_data() 
{
  runicast_open(&runicast_data, RUNICAST_CHANNEL_DATA, &runicast_data_callbacks);
}

#endif