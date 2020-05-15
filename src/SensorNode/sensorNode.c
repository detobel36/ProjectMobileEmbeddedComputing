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
// Allocate memory

// Doc about list: http://contiki.sourceforge.net/docs/2.6/a01682.html
MEMB(data_mem, struct data_packet_list, NUM_DATA_IN_QUEUE);
LIST(data_list);

/*---------------------------------------------------------------------------*/
// PROCESS
PROCESS(broadcast_process, "Broadcast process");
PROCESS(collect_data_process, "Collect Data process");
PROCESS(send_data_process, "Send Data process");
AUTOSTART_PROCESSES(&broadcast_process, &collect_data_process, &send_data_process);
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
}

static void
timedout_broadcast_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("runicast broadcast message timed out when sending to %d.%d, retransmissions %d\n",
   to->u8[0], to->u8[1], retransmissions);
}

/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// Receive data packet
static void
recv_data_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno)
{
  struct data_packet *forward_data_packet = packetbuf_dataptr();

  // TODO save children

  linkaddr_t source_addr = forward_data_packet->address;

  if(runicast_is_transmitting(&runicast_data)) {
    printf("Could not forward data of %d.%d, runicast_data is already used\n", from->u8[0], 
        from->u8[1]);

    struct data_packet_list *entry = memb_alloc(&data_mem);
    entry->data = forward_data_packet->data;
    entry->address = source_addr;
    list_add(data_list, entry);
    return;
  }
  packetbuf_clear();
  
  printf("Forward data %d (source: %d.%d) from %d.%d to %d.%d, seqno %d\n", forward_data_packet->data, 
    source_addr.u8[0], source_addr.u8[1], from->u8[0], from->u8[1], parent_addr.u8[0], 
    parent_addr.u8[1], seqno);
  packetbuf_copyfrom(forward_data_packet, sizeof(struct data_packet));

  runicast_send(&runicast_data, &parent_addr, MAX_RETRANSMISSIONS);
  packetbuf_clear();
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
// COLLECT DATA PROCESS
PROCESS_THREAD(collect_data_process, ev, data)
{
  PROCESS_BEGIN();

  static struct etimer et;

  while(1) {
    etimer_set(&et, CLOCK_SECOND * DATA_COLLECTING_DELAY);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    // Generates a random int between 0 and 100
    uint8_t random_int = random_rand() % (100 + 1 - 0) + 0; // (0 for completeness)
    struct data_packet_list *entry = memb_alloc(&data_mem);
    entry->data = random_int;
    entry->address = linkaddr_node_addr;
    printf("Collect new data %d\n", random_int);
    list_add(data_list, entry);
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
// SEND DATA PROCESS
PROCESS_THREAD(send_data_process, ev, data)
{
  PROCESS_EXITHANDLER(runicast_close(&runicast_data);)
  PROCESS_BEGIN();

  runicast_open(&runicast_data, RUNICAST_CHANNEL_DATA, &runicast_data_callbacks);
  list_init(data_list);
  memb_init(&data_mem);

  static struct etimer et;

  while(1) {
    etimer_set(&et, CLOCK_SECOND * DATA_MIN_DELAY + random_rand() % (CLOCK_SECOND * DATA_MAX_DELAY));
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    if(rank != MAX_RANK) {
      if(list_length(data_list) > 0) {
        if(runicast_is_transmitting(&runicast_data)) {
          printf("Runicast data is already used\n");
        } else {

          struct data_packet_list *entry = list_pop(data_list);
          packetbuf_clear();

          struct data_packet packet;
          packet.data = entry->data;
          packet.address = entry->address;
          packetbuf_copyfrom(&packet, sizeof(struct data_packet));
          
          runicast_send(&runicast_data, &parent_addr, MAX_RETRANSMISSIONS);
          if(list_length(data_list) > 0) {
            printf("Send data %d (%d data in queue)\n", packet.data, list_length(data_list));
          } else {
            printf("Send data %d\n", packet.data);
          }
          packetbuf_clear();
        }

      } else {
        printf("No data to send\n");
      }

    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

/*===========================================================================*/
