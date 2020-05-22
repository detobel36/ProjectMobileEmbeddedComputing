#ifndef runicast_rank_c
#define runicast_rank_c

#include "../Common/Constants.c"
#include "../Common/PacketStruct.c"

static struct runicast_conn runicast_rank;

// Save rank packet
MEMB(rank_mem, struct rank_packet_entry, NUM_MAX_CHILDREN);
LIST(rank_list);


static void create_rank_response_packet(const linkaddr_t *from) {
  struct rank_packet_entry *packet_response = memb_alloc(&rank_mem);
    packet_response->destination = *from;
    printf("[INFO - %s] Create response rank for %d.%d\n", NODE_TYPE, from->u8[0], from->u8[1]);
    list_add(rank_list, packet_response);
    packetbuf_clear();

    if(!runicast_is_transmitting(&runicast_rank)) {
      process_poll(&rank_process);
    }
}


static void recv_rank_runicast(const linkaddr_t *from, const struct rank_packet *rank_packet);

// Receive broadcast response
static void
recv_rank_general_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno)
{
  struct rank_packet *rank_packet = packetbuf_dataptr();
  recv_rank_runicast(from, rank_packet);
}

static void
sent_rank_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("[INFO - %s] runicast rank message sent to %d.%d, retransmissions %d\n",
   NODE_TYPE, to->u8[0], to->u8[1], retransmissions);
  process_poll(&rank_process);
}

static void
timedout_rank_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("[WARN - %s] runicast rank message timed out when sending to %d.%d, retransmissions %d\n",
   NODE_TYPE, to->u8[0], to->u8[1], retransmissions);
  process_poll(&rank_process);
}


static const struct runicast_callbacks runicast_rank_callbacks = {
                   recv_rank_general_runicast,
                   sent_rank_runicast,
                   timedout_rank_runicast};

static void open_runicast_rank() {
  runicast_open(&runicast_rank, RUNICAST_CHANNEL_BROADCAST, &runicast_rank_callbacks);

  list_init(rank_list);
  memb_init(&rank_mem);
}

#endif