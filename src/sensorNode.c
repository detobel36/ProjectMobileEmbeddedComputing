#include <stdio.h>

#include "contiki.h"
#include "net/rime/rime.h"

#include "lib/list.h"
#include "lib/memb.h"

#include "dev/button-sensor.h"
#include "dev/leds.h"
#include "random.h"

#include "Common/PacketType.c"


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
// TODO add a process to get Data
AUTOSTART_PROCESSES(&broadcast_process, &runicast_process);
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
    packet_response.type = DISCOVERY_RESPONSE;
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
PROCESS_THREAD(runicast_process, ev, data)
{
  PROCESS_EXITHANDLER(runicast_close(&runicast);)

  PROCESS_BEGIN();

  // TODO
  // runicast_open(&runicast, 144, &runicast_callbacks);

  // /* OPTIONAL: Sender history */
  // list_init(history_table);
  // memb_init(&history_mem);

  // /* Receiver node: do nothing */
  // if(linkaddr_node_addr.u8[0] == 1 &&
  //    linkaddr_node_addr.u8[1] == 0) {
  //   PROCESS_WAIT_EVENT_UNTIL(0);
  // }

  // while(1) {
  //   static struct etimer et;

  //   etimer_set(&et, 10*CLOCK_SECOND);
  //   PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

  //   if(!runicast_is_transmitting(&runicast)) {
  //     linkaddr_t recv;

  //     packetbuf_copyfrom("Hello", 5);
  //     recv.u8[0] = 1;
  //     recv.u8[1] = 0;

  //     printf("%u.%u: sending runicast to address %u.%u\n",
  //        linkaddr_node_addr.u8[0],
  //        linkaddr_node_addr.u8[1],
  //        recv.u8[0],
  //        recv.u8[1]);

  //     runicast_send(&runicast, &recv, MAX_RETRANSMISSIONS);
  //   }
  // }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

