#include <stdio.h>

#include "contiki.h"
#include "net/rime/rime.h"

#include "lib/list.h"
#include "lib/memb.h"

#include "dev/button-sensor.h"
#include "dev/leds.h"
#include "random.h"
#include <stdbool.h>


#include "../Common/PacketType.c"


#define MAX_RETRANSMISSIONS 4
#define NUM_HISTORY_ENTRIES 4
#define BROADCAST_DELAY 15
#define MAX_RANK 255


/*---------------------------------------------------------------------------*/
// VARIABLES
static struct broadcast_conn broadcast;
static struct runicast_conn runicast;
static uint8_t rank = MAX_RANK;
static linkaddr_t parent_addr;
static uint16_t parent_last_rssi;
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// PROCESS
PROCESS(broadcast_process, "Broadcast process");
PROCESS(runicast_process, "Runicast process");
PROCESS(data_process, "Data process");
AUTOSTART_PROCESSES(&broadcast_process, &runicast_process, &data_process);
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// BROADCAST
static void
broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  struct general_packet *packet;
  packet = packetbuf_dataptr();

  printf("broadcast message received from %d.%d. Type: %d and rank: %d\n",
         from->u8[0], from->u8[1], packet->type, packet->rank);

  // Request to find parent
  if(packet->type == DISCOVERY_REQ) {

    // Respond to broadcast
    if(rank != MAX_RANK) {
      struct general_packet packet_response;
      packet_response.type = DISCOVERY_RESP;
      packet_response.rank = rank;

      int countTransmission=0;
      while (runicast_is_transmitting(&runicast) && ++countTransmission < MAX_RETRANSMISSIONS) {}

      packetbuf_copyfrom(&packet_response, sizeof(struct general_packet));
      printf("Send response to broadcast\n");
      runicast_send(&runicast, from, MAX_RETRANSMISSIONS);
    }

  } else {
    printf("Unknow packet broadcsat: %d\n", packet->type);
  }
}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
PROCESS_THREAD(broadcast_process, ev, data)
{
  static struct etimer et;

  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)

  PROCESS_BEGIN();

  broadcast_open(&broadcast, 129, &broadcast_call);

  while(1) {

    struct general_packet packet;
    packet.type = DISCOVERY_REQ;
    packet.rank = rank;
    packetbuf_copyfrom(&packet, sizeof(struct general_packet));
    broadcast_send(&broadcast);
    printf("broadcast message sent\n");

    /* Delay between BROADCAST_DELAY and 2*BROADCAST_DELAY seconds */
    // TODO may be add more time
    etimer_set(&et, CLOCK_SECOND * BROADCAST_DELAY + (random_rand() % (CLOCK_SECOND * BROADCAST_DELAY)));

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// RUNICAST

static void
recv_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno)
{
  struct abstract_packet *abstr_packet;
  abstr_packet = packetbuf_dataptr();
  uint16_t current_rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);

  // TODO
  // Update rssi when receive packet from parent
  // parent_last_rssi = current_rssi;

  // On other node reply
  if (abstr_packet->type == DISCOVERY_RESP) {

    struct general_packet *general_packet = (struct general_packet *) abstr_packet;
    uint16_t getting_rank = general_packet->rank;
    printf("Discovery response from %d.%d, seqno %d, rank: %d\n", from->u8[0], from->u8[1], seqno,
      getting_rank);

    if(rank == MAX_RANK) {
      rank = getting_rank+1;
      parent_addr = *from;
      parent_last_rssi = current_rssi;
      printf("Init rank: %d (quality: %d)\n", rank, current_rssi);

    } else if(rank > getting_rank+1) {
      if(current_rssi > parent_last_rssi) {
        rank = getting_rank+1;
        parent_addr = *from;
        parent_last_rssi = current_rssi;
        printf("Change parent and get rank: %d (quality signal: %d)\n", rank, parent_last_rssi);
      } else {
        printf("Quality signal not enought to change parent (new: %d, parent: %d)\n", current_rssi, parent_last_rssi);
      }

    } else if(parent_addr.u8[0] == from->u8[0] && parent_addr.u8[1] == from->u8[1]) {
      parent_last_rssi = current_rssi;
      printf("Update parent quality signal (new: %d)\n", current_rssi);
    }
    // TODO check "if(rank == getting_rank+1)" and better signal quality !


  // When we receive packet
  } else if(abstr_packet->type == PACKET_DATA) {

    struct data_packet *forward_data_packet = (struct data_packet *) abstr_packet;

    linkaddr_t source_addr = forward_data_packet->address;
    printf("Forward data %d (source: %d.%d) from %d.%d to %d.%d, seqno %d\n", forward_data_packet->data, 
      source_addr.u8[0], source_addr.u8[1], from->u8[0], from->u8[1], parent_addr.u8[0], 
      parent_addr.u8[1], seqno);
    packetbuf_copyfrom(forward_data_packet, sizeof(struct data_packet));
  
    int countTransmission = 0;
    while (runicast_is_transmitting(&runicast) && ++countTransmission < MAX_RETRANSMISSIONS) {}
    runicast_send(&runicast, &parent_addr, MAX_RETRANSMISSIONS);


  // Unknow packet
  } else {
    printf("[UNKNOW] runicast message received from %d.%d, seqno %d\n",
        from->u8[0], from->u8[1], seqno);
  }

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
sent_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("runicast message sent to %d.%d, retransmissions %d\n",
   to->u8[0], to->u8[1], retransmissions);
}

static void
timedout_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("runicast message timed out when sending to %d.%d, retransmissions %d\n",
   to->u8[0], to->u8[1], retransmissions);
}

static const struct runicast_callbacks runicast_callbacks = {
                   recv_runicast,
                   sent_runicast,
                   timedout_runicast};
PROCESS_THREAD(runicast_process, ev, data)
{
  PROCESS_EXITHANDLER(runicast_close(&runicast);)

  PROCESS_BEGIN();

  runicast_open(&runicast, 144, &runicast_callbacks);

  /* OPTIONAL: Sender history */
  // list_init(history_table);
  // memb_init(&history_mem);

  /* Receiver node: do nothing */
  // if(linkaddr_node_addr.u8[0] == 1 &&
  //    linkaddr_node_addr.u8[1] == 0) {
    PROCESS_WAIT_EVENT_UNTIL(0);
  // }

  // while(1) {
    // static struct etimer et;

    // etimer_set(&et, 10*CLOCK_SECOND);
    // PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    // if(!runicast_is_transmitting(&runicast)) {
    //   linkaddr_t recv;

    //   packetbuf_copyfrom("Hello", 5);
    //   recv.u8[0] = 1;
    //   recv.u8[1] = 0;

    //   printf("%u.%u: sending runicast to address %u.%u\n",
    //      linkaddr_node_addr.u8[0],
    //      linkaddr_node_addr.u8[1],
    //      recv.u8[0],
    //      recv.u8[1]);

    //   runicast_send(&runicast, &recv, MAX_RETRANSMISSIONS);
    // }
  // }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
// DATA PROCESS
PROCESS_THREAD(data_process, ev, data)
{
  PROCESS_EXITHANDLER(runicast_close(&runicast);)
  PROCESS_BEGIN();

  while(1) {
    static struct etimer et;

    etimer_set(&et, 60 * CLOCK_SECOND);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    if(rank != MAX_RANK) {
      // Generates a random int between 0 and 100
      uint8_t random_int = random_rand() % (100 + 1 - 0) + 0; // (0 for completeness)

      struct data_packet packet;
      packet.type = PACKET_DATA;
      packet.data = random_int;
      packet.address = linkaddr_node_addr;
      packet.link = true;
      packetbuf_copyfrom(&packet, sizeof(struct data_packet));
      
      int countTransmission = 0;
      while (runicast_is_transmitting(&runicast) && ++countTransmission < MAX_RETRANSMISSIONS) {}
      runicast_send(&runicast, &parent_addr, MAX_RETRANSMISSIONS);
      printf("Send random data %d\n", random_int);
    }
    // SEND DATA
  }

  PROCESS_END();
}
