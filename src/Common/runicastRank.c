#ifndef runicast_rank_c
#define runicast_rank_c

#include "dev/serial-line.h"

#include "../Common/constants.c"
#include "../Common/packetStruct.c"

static struct runicast_conn runicast_rank;

// Event
static process_event_t broadcast_add_delay_event;

// Save rank packet
MEMB(rank_mem, struct rank_packet_entry, NUM_MAX_CHILDREN);
LIST(rank_list);


static void 
create_rank_response_packet(const linkaddr_t *from) 
{
  struct rank_packet_entry *packet_response = memb_alloc(&rank_mem);
    packet_response->destination = *from;
    printf("[NOTICE - %s] Create response rank for %d.%d\n", NODE_TYPE, from->u8[0], from->u8[1]);
    list_add(rank_list, packet_response);
    packetbuf_clear();

    if(!runicast_is_transmitting(&runicast_rank)) {
      process_post(&rank_process, broadcast_add_delay_event, NULL);
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
  printf("[NOTICE - %s] runicast rank message sent to %d.%d, retransmissions %d\n",
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


PROCESS_THREAD(rank_process, ev, data)
{
  PROCESS_EXITHANDLER(runicast_close(&runicast_rank);)

  PROCESS_BEGIN();

  static struct etimer et;
  
  runicast_open(&runicast_rank, RUNICAST_CHANNEL_BROADCAST, &runicast_rank_callbacks);

  broadcast_add_delay_event = process_alloc_event();
  list_init(rank_list);
  memb_init(&rank_mem);

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(ev != serial_line_event_message);

    // If process is wake up to directly reply to broadcast event
    if(ev == broadcast_add_delay_event) {
      printf("[DEBUG - %s] Wake up rank_process due to broadcast event\n", NODE_TYPE);
      // Add delay to reply
      etimer_set(&et, random_rand() % (CLOCK_SECOND * BROADCAST_REPLY_DELAY));
      // If event or timer
      PROCESS_WAIT_EVENT_UNTIL(ev != serial_line_event_message);
    }

    while(list_length(rank_list) > 0) {
      while (runicast_is_transmitting(&runicast_rank)) {
        PROCESS_WAIT_EVENT_UNTIL(ev != serial_line_event_message);
      }

      struct rank_packet_entry *entry = list_pop(rank_list);
      memb_free(&rank_mem, entry);
      linkaddr_t destination_addr = entry->destination;
      packetbuf_clear();

      printf("[NOTICE - %s] Send rank information to %d.%d (%d rank in queue)\n", 
        NODE_TYPE, destination_addr.u8[0], destination_addr.u8[1], list_length(rank_list));

      struct rank_packet packet;
      packet.rank = rank;
      packetbuf_copyfrom(&packet, sizeof(struct rank_packet));

      runicast_send(&runicast_rank, &destination_addr, MAX_RETRANSMISSIONS);
      packetbuf_clear();

    }
  }

  PROCESS_END();
}

#endif