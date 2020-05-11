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

#define BROADCAST_CHANNEL 129
#define RUNICAST_CHANNEL_DATA 144
#define RUNICAST_CHANNEL_BROADCAST 146


/*---------------------------------------------------------------------------*/
// VARIABLES
static struct broadcast_conn broadcast;
static struct runicast_conn runicast_broadcast;
static struct runicast_conn runicast_data;
static uint8_t rank = 0;
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// PROCESS
PROCESS(broadcast_process, "Broadcast process");
PROCESS(runicast_process, "Runicast process");
AUTOSTART_PROCESSES(&broadcast_process, &runicast_process);
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// Receive broadcast
static void
broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  printf("broadcast message received from %d.%d.\n",
         from->u8[0], from->u8[1]);

  // Request to find parent
  struct rank_packet packet_response;
  packet_response.rank = rank;

  // Check that transmission already exist
  while(runicast_is_transmitting(&runicast_broadcast)) { }
  packetbuf_clear();

  packetbuf_copyfrom(&packet_response, sizeof(struct rank_packet));
  printf("Send response to broadcast\n");
  runicast_send(&runicast_broadcast, from, MAX_RETRANSMISSIONS);
  packetbuf_clear();
}

/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
// Receive broadcast response
static void
recv_broadcast_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno)
{
  printf("Not normal to be here");
}

static void
sent_broadcast_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("runicast broadcast message sent to %d.%d, retransmissions %d\n",
   to->u8[0], to->u8[1], retransmissions);
}

static void
timedout_broadcast_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("runicast broadcast message timed out when sending to %d.%d, retransmissions %d\n",
   to->u8[0], to->u8[1], retransmissions);
}

/*---------------------------------------------------------------------------*/





/*---------------------------------------------------------------------------*/
// RUNICAST

static void
recv_data_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno)
{
  // printf("Data packet from %d.%d, seqno %d\n", from->u8[0], from->u8[1], seqno);
  struct data_packet *data_packet = packetbuf_dataptr();
  linkaddr_t address = data_packet->address;
  printf("Receive data packet from: %d.%d with value: %d (seqno %d)\n", address.u8[0], 
    address.u8[1], data_packet->data, seqno);
}

static void
sent_data_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("runicast data message sent to %d.%d, retransmissions %d\n",
   to->u8[0], to->u8[1], retransmissions);
}

static void
timedout_data_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("runicast data message timed out when sending to %d.%d, retransmissions %d\n",
   to->u8[0], to->u8[1], retransmissions);
}

static const struct runicast_callbacks runicast_data_callbacks = {
                   recv_data_runicast,
                   sent_data_runicast,
                   timedout_data_runicast};

static const struct runicast_callbacks runicast_broadcast_callbacks = {
                   recv_broadcast_runicast,
                   sent_broadcast_runicast,
                   timedout_broadcast_runicast};


/*================================ THREADS ==================================*/


static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
PROCESS_THREAD(broadcast_process, ev, data)
{
  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
  PROCESS_EXITHANDLER(runicast_close(&runicast_broadcast);)

  PROCESS_BEGIN();

  broadcast_open(&broadcast, BROADCAST_CHANNEL, &broadcast_call);
  runicast_open(&runicast_broadcast, RUNICAST_CHANNEL_BROADCAST, &runicast_broadcast_callbacks);

  PROCESS_WAIT_EVENT_UNTIL(0);

  PROCESS_END();
}


PROCESS_THREAD(runicast_process, ev, data)
{
  PROCESS_EXITHANDLER(runicast_close(&runicast_data);)

  PROCESS_BEGIN();

  runicast_open(&runicast_data, RUNICAST_CHANNEL_DATA, &runicast_data_callbacks);

  PROCESS_WAIT_EVENT_UNTIL(0);

  // TODO QUESTION: better to use ?
  // while(1) {
  //     PROCESS_YIELD();
  // }

  PROCESS_END();
}
