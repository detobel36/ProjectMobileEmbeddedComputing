#include <stdio.h>

#include "contiki.h"
#include "net/rime/rime.h"

#include "lib/list.h"
#include "lib/memb.h"

#include "dev/button-sensor.h"
#include "dev/leds.h"
#include "random.h"
#include <stdbool.h>


#include "../Common/PacketStruct.c"
#include "../Common/Constants.c"



/*---------------------------------------------------------------------------*/
// VARIABLES
static struct broadcast_conn broadcast;
static struct runicast_conn runicast_data;
static struct runicast_conn runicast_broadcast;
static uint8_t rank = MAX_RANK;
static linkaddr_t parent_addr;
static uint16_t parent_last_rssi;
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// PROCESS
PROCESS(broadcast_process, "Broadcast process");
PROCESS(data_process, "Data process");
AUTOSTART_PROCESSES(&broadcast_process, &data_process);
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// Receive broadcast
static void
broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  // Respond to broadcast
  if(rank != MAX_RANK) {
    if(runicast_is_transmitting(&runicast_broadcast)) {
      // printf("wait runicast_broadcast %s\n", PROCESS_CURRENT()->name);
      printf("Could not reply to %d.%d, runicast_broadcast is already used\n", from->u8[0], 
        from->u8[1]);
      return;
    }
    packetbuf_clear();

    struct rank_packet packet_response;
    packet_response.rank = rank;

    packetbuf_copyfrom(&packet_response, sizeof(struct rank_packet));
    printf("Send response to broadcast of %d.%d\n", from->u8[0], from->u8[1]);
    printf("OPEN runicast broadcast\n");
    runicast_send(&runicast_broadcast, from, MAX_RETRANSMISSIONS);
    packetbuf_clear();
  }
}
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
// Receive broadcast response
static void
recv_broadcast_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno)
{
  struct rank_packet *rank_packet = packetbuf_dataptr();
  uint16_t getting_rank = rank_packet->rank;
  printf("Discovery response from %d.%d, seqno %d, rank: %d\n", from->u8[0], 
    from->u8[1], seqno, getting_rank);

  uint16_t current_rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);

  // If no rank
  if(rank == MAX_RANK) {
    rank = getting_rank+1;
    parent_addr = *from;
    parent_last_rssi = current_rssi;
    printf("Init rank: %d (quality: %d)\n", rank, current_rssi);

  // If broadcast from parent
  } else if(parent_addr.u8[0] == from->u8[0] && parent_addr.u8[1] == from->u8[1]) {
    parent_last_rssi = current_rssi;
    printf("Update parent quality signal (new: %d)\n", current_rssi);

  // If potential new parent
  } else if(rank > getting_rank) {  // current_rank > new_getting_rank
    if(current_rssi > parent_last_rssi) {
      rank = getting_rank+1;
      parent_addr = *from;
      parent_last_rssi = current_rssi;
      printf("Change parent and get rank: %d (quality signal: %d)\n", rank, parent_last_rssi);
    } else {
      printf("Quality signal not enought to change parent (new: %d, parent: %d)\n", current_rssi, parent_last_rssi);
    }
  }

  packetbuf_clear();
}

static void
sent_broadcast_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("runicast broadcast message sent to %d.%d, retransmissions %d\n",
   to->u8[0], to->u8[1], retransmissions);
  printf("END runicast broadcast\n");
}

static void
timedout_broadcast_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("runicast broadcast message timed out when sending to %d.%d, retransmissions %d\n",
   to->u8[0], to->u8[1], retransmissions);
  printf("END runicast broadcast\n");
}

/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// Receive data packet
static void
recv_data_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno)
{
  struct data_packet *forward_data_packet = packetbuf_dataptr();

  if(runicast_is_transmitting(&runicast_data)) {
    // TODO keep history
    printf("Could not reply to %d.%d, runicast_data is already used\n", from->u8[0], 
        from->u8[1]);
    return;
  }
  packetbuf_clear();
  
  linkaddr_t source_addr = forward_data_packet->address;
  printf("Forward data %d (source: %d.%d) from %d.%d to %d.%d, seqno %d\n", forward_data_packet->data, 
    source_addr.u8[0], source_addr.u8[1], from->u8[0], from->u8[1], parent_addr.u8[0], 
    parent_addr.u8[1], seqno);
  packetbuf_copyfrom(forward_data_packet, sizeof(struct data_packet));

  runicast_send(&runicast_data, &parent_addr, MAX_RETRANSMISSIONS);
  printf("OPEN runicast data\n");
  packetbuf_clear();

  /* OPTIONAL: Sender history */
  // struct history_entry *e = NULL;
  // for(e = list_head(history_table); e != NULL; e = e->next) {
  //   if(linkaddr_cmp(&e->addr, from)) {
  //     break;
  //   }
  // }
  // if(e == NULL) {
  //   /* Create new history entry */
  //   e = memb_alloc(&history_mem);
  //   if(e == NULL) {
  //     e = list_chop(history_table); /* Remove oldest at full history */
  //   }
  //   linkaddr_copy(&e->addr, from);
  //   e->seq = seqno;
  //   list_push(history_table, e);
  // } else {
  //   /* Detect duplicate callback */
  //   if(e->seq == seqno) {
  //     printf("runicast message received from %d.%d, seqno %d (DUPLICATE)\n",
  //      from->u8[0], from->u8[1], seqno);
  //     return;
  //   }
  //   /* Update existing history entry */
  //   e->seq = seqno;
  // }
}

static void
sent_data_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("runicast data message sent to %d.%d, retransmissions %d\n",
   to->u8[0], to->u8[1], retransmissions);
  printf("END runicast data\n");
}

static void
timedout_data_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("runicast data message timed out when sending to %d.%d, retransmissions %d\n",
   to->u8[0], to->u8[1], retransmissions);
  printf("END runicast data\n");
}

/*---------------------------------------------------------------------------*/



static const struct runicast_callbacks runicast_data_callbacks = {
                   recv_data_runicast,
                   sent_data_runicast,
                   timedout_data_runicast};

static const struct runicast_callbacks runicast_broadcast_callbacks = {
                   recv_broadcast_runicast,
                   sent_broadcast_runicast,
                   timedout_broadcast_runicast};




/*================================ THREADS ==================================*/

/*---------------------------------------------------------------------------*/
// Send broadcast threat
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
PROCESS_THREAD(broadcast_process, ev, data)
{
  static struct etimer et;

  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
  PROCESS_EXITHANDLER(runicast_close(&runicast_broadcast);)

  PROCESS_BEGIN();

  broadcast_open(&broadcast, BROADCAST_CHANNEL, &broadcast_call);
  runicast_open(&runicast_broadcast, RUNICAST_CHANNEL_BROADCAST, &runicast_broadcast_callbacks);

  // Delay for broadcast
  // printf("Broadcast message between: %i sec and %i sec\n", BROADCAST_DELAY + 0 % (BROADCAST_DELAY), 
  //     BROADCAST_DELAY + (BROADCAST_DELAY-1) % (BROADCAST_DELAY));

  // First broadcast send between 0 sec and max broadcast delay
  etimer_set(&et, random_rand() % (CLOCK_SECOND * BROADCAST_DELAY));
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

  while(1) {

    // struct general_packet packet;
    // packet.type = DISCOVERY_REQ;
    // packet.rank = rank;
    // packetbuf_copyfrom(&packet, sizeof(struct general_packet));
    broadcast_send(&broadcast);
    printf("broadcast message sent\n");

    /* Delay between BROADCAST_DELAY and 2*BROADCAST_DELAY seconds */
    // TODO may be add more time
    etimer_set(&et, CLOCK_SECOND * BROADCAST_DELAY + random_rand() % (CLOCK_SECOND * BROADCAST_DELAY));

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
// DATA PROCESS
PROCESS_THREAD(data_process, ev, data)
{
  PROCESS_EXITHANDLER(runicast_close(&runicast_data);)
  PROCESS_BEGIN();

  runicast_open(&runicast_data, RUNICAST_CHANNEL_DATA, &runicast_data_callbacks);

  while(1) {
    static struct etimer et;

    // TODO add random to not send all at the same time
    etimer_set(&et, 60 * CLOCK_SECOND);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    if(rank != MAX_RANK) {
      // Generates a random int between 0 and 100
      uint8_t random_int = random_rand() % (100 + 1 - 0) + 0; // (0 for completeness)

      if(runicast_is_transmitting(&runicast_data)) {
        printf("runicast data is already used\n");
        // TODO create history ?
      } else {
        packetbuf_clear();

        struct data_packet packet;
        packet.data = random_int;
        packet.address = linkaddr_node_addr;
        packetbuf_copyfrom(&packet, sizeof(struct data_packet));
        
        runicast_send(&runicast_data, &parent_addr, MAX_RETRANSMISSIONS);
        printf("Send random data %d\n", random_int);
        printf("OPEN runicast data\n");
        packetbuf_clear();
      }
    }
    // SEND DATA
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

/*===========================================================================*/
