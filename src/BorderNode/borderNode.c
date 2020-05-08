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
#define BROADCAST_DELAY 15


/*---------------------------------------------------------------------------*/
// VARIABLES
static struct broadcast_conn broadcast;
static struct runicast_conn runicast;
static uint16_t rank = 1;
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// PROCESS
PROCESS(broadcast_process, "Broadcast process");
PROCESS(runicast_process, "Runicast process");
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
    packet_response.type = DISCOVERY_RESP;
    packet_response.rank = rank;

    // Check that transmission already exist
    int countTransmission=0;
    while (runicast_is_transmitting(&runicast) && ++countTransmission < MAX_RETRANSMISSIONS) {}

    packetbuf_copyfrom(&packet_response, sizeof(struct general_packet));
    printf("Send response to broadcast\n");
    runicast_send(&runicast, from, MAX_RETRANSMISSIONS);
  } else {
    printf("Unknow packet broadcsat: %d\n", packet->type);
  }
}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
PROCESS_THREAD(broadcast_process, ev, data)
{
  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)

  PROCESS_BEGIN();

  broadcast_open(&broadcast, 129, &broadcast_call);

  PROCESS_WAIT_EVENT_UNTIL(0);

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
    // Not normal !
    printf("Discovery response from %d.%d, seqno %d\n", from->u8[0], from->u8[1], seqno);

  } else if(abstr_packet->type == PACKET_DATA) {
    printf("Data packet from %d.%d, seqno %d\n", from->u8[0], from->u8[1], seqno);
    struct data_packet *data_packet;
    data_packet = packetbuf_dataptr();
    printf("Data: %d\n", data_packet->data);
  } else {
    printf("runicast message received from %d.%d, seqno %d\n",
        from->u8[0], from->u8[1], seqno);
  }
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

  PROCESS_WAIT_EVENT_UNTIL(0);

  // TODO QUESTION: better to use ?
  // while(1) {
  //     PROCESS_YIELD();
  // }

  PROCESS_END();
}
