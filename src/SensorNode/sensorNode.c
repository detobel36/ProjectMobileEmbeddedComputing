#include <stdio.h>

#include "contiki.h"
#include "net/rime/rime.h"

#include "lib/list.h"
#include "lib/memb.h"

#include "dev/button-sensor.h"
#include "dev/leds.h"
#include "random.h"

#include "../Common/PacketType.c"


#define MAX_RETRANSMISSIONS 4
#define NUM_HISTORY_ENTRIES 4


/*---------------------------------------------------------------------------*/
// VARIABLES
static struct broadcast_conn broadcast;
static struct runicast_conn runicast;
static uint16_t rank = 0;
static linkaddr_t parentAddr;
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
    struct general_packet packet_response;
    packet_response.type = DISCOVERY_RESP;
    packet_response.rank = rank;

    // TODO send respond with runicast
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

    /* Delay 2-4 seconds */
    // TODO may be add more time
    etimer_set(&et, CLOCK_SECOND * 4 + random_rand() % (CLOCK_SECOND * 4));

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    struct general_packet packet;
    packet.type = DISCOVERY_REQ;
    packet.rank = rank;
    packetbuf_copyfrom(&packet, sizeof(struct general_packet));
    broadcast_send(&broadcast);
    printf("broadcast message sent\n");
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

  if (abstr_packet->type == DISCOVERY_RESP) {
    printf("Discovery response from %d.%d, seqno %d\n", from->u8[0], from->u8[1], seqno);

  } else if(abstr_packet->type == PACKET_DATA) {
    printf("Data packet from %d.%d, seqno %d\n", from->u8[0], from->u8[1], seqno);
    struct data_packet *data_packet;
    data_packet = packetbuf_dataptr();
    printf("Data: %d\n", data_packet->data);
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

  printf("runicast message received from %d.%d, seqno %d\n",
   from->u8[0], from->u8[1], seqno);
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

    etimer_set(&et, 2 * CLOCK_SECOND);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    // Generates a random int between 0 and 100
    uint8_t random_int = random_rand() % (100 + 1 - 0) + 0; // (0 for completeness)

    // TODO may be add other
    struct data_packet packet;
    packet.type = PACKET_DATA;
    packet.data = random_int;
    packetbuf_copyfrom(&packet, sizeof(struct data_packet));
    
    // TODO update address
    linkaddr_t recv;
    recv.u8[0] = 1;
    recv.u8[1] = 0;

    printf("Send random data %d\n", random_int);
    runicast_send(&runicast, &recv, MAX_RETRANSMISSIONS);
    SEND DATA
  }

  PROCESS_END();
}
