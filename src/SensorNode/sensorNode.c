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
static struct runicast_conn runicast_rank;
static struct runicast_conn runicast_valve;
static uint8_t rank = MAX_RANK;
static linkaddr_t parent_addr;
static uint16_t parent_last_rssi;
static process_event_t new_data_event;
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// Allocate memory

// Doc about list: http://contiki.sourceforge.net/docs/2.6/a01682.html
MEMB(data_mem, struct data_packet_entry, NUM_DATA_IN_QUEUE);
LIST(data_list);

MEMB(valve_mem, struct valve_packet_address_entry, NUM_DATA_IN_QUEUE);
LIST(valve_list);

MEMB(rank_mem, struct rank_packet_entry, NUM_MAX_CHILDREN);
LIST(rank_list);

MEMB(children_mem, struct children_entry, NUM_MAX_CHILDREN);
LIST(children_list);
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// PROCESS
PROCESS(broadcast_process, "Broadcast process");
PROCESS(rank_process, "Rank process");
PROCESS(collect_data_process, "Collect Data process");
PROCESS(valve_data_process, "Valve Data process");
PROCESS(send_data_process, "Send Data process");
AUTOSTART_PROCESSES(&broadcast_process, &rank_process, &collect_data_process, &send_data_process, 
  &valve_data_process);
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// UTILS
static struct children_entry* get_child_entry(const linkaddr_t *destination_addr) {
  struct children_entry *child;
  for(child = list_head(children_list); child != NULL; child = list_item_next(child)) {
    if(linkaddr_cmp(&child->address_destination, destination_addr)) {
      break;
    }
  }
  return child;
}

static void resetRank() {
  rank = MAX_RANK;
  parent_addr = linkaddr_null;

  // Wake up broadcast system
  process_poll(&broadcast_process);
}

/*---------------------------------------------------------------------------*/



/*---------------------------------------------------------------------------*/
// Receive broadcast
static void
broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  // Respond to broadcast
  if(rank != MAX_RANK) {
    struct rank_packet_entry *packet_response = memb_alloc(&rank_mem);
    packet_response->destination = *from;
    printf("[INFO - Sensor] Create response rank for %d.%d\n", from->u8[0], from->u8[1]);
    list_add(data_list, entry);
    packetbuf_clear();

    if(!runicast_is_transmitting(&runicast_rank)) {
      process_poll(&rank_process);
    }

  }
}
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
// Receive rank packet
static void
recv_rank_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno)
{
  struct rank_packet *rank_packet = packetbuf_dataptr();
  uint16_t getting_rank = rank_packet->rank;
  printf("[DEBUG - Sensor] Discovery response from %d.%d, seqno %d, rank: %d\n", from->u8[0], 
    from->u8[1], seqno, getting_rank);

  uint16_t current_rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);

  // If no rank
  if(rank == MAX_RANK) {
    rank = getting_rank+1;
    parent_addr = *from;
    parent_last_rssi = current_rssi;
    printf("[INFO - Sensor] Init rank: %d (quality: %d)\n", rank, current_rssi);

    // Wake up send of data if parent was lost
    process_poll(&send_data_process);

  // If broadcast from parent
  } else if(linkaddr_cmp(&parent_addr, from)) {

    if(getting_rank == MAX_RANK) {
      printf("[INFO - Sensor] Receive reset rank packet !\n");
      resetRank();

    } else {
      parent_last_rssi = current_rssi;
      printf("[INFO - Sensor] Update parent quality signal (new: %d)\n", current_rssi);

    }

  // If potential new parent
  } else if(rank > getting_rank) {  // current_rank > new_getting_rank
    if(current_rssi > parent_last_rssi) {
      rank = getting_rank+1;
      parent_addr = *from;
      parent_last_rssi = current_rssi;
      printf("[INFO - Sensor] Change parent and get rank: %d (quality signal: %d)\n", 
        rank, parent_last_rssi);
    } else {
      printf("[INFO - Sensor] Quality signal not enought to change parent (new: %d, parent: %d)\n", 
        current_rssi, parent_last_rssi);
    }
  }

  packetbuf_clear();
}

static void
sent_rank_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("[INFO - Sensor] runicast rank message sent to %d.%d, retransmissions %d\n",
   to->u8[0], to->u8[1], retransmissions);
}

static void
timedout_rank_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("[WARN - Sensor] runicast rank message timed out when sending to %d.%d, retransmissions %d\n",
   to->u8[0], to->u8[1], retransmissions);
}

/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// Receive data packet
static void
recv_data_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno)
{
  struct data_packet *forward_data_packet = packetbuf_dataptr();

  linkaddr_t source_addr = forward_data_packet->address;

  struct children_entry *child = get_child_entry(&source_addr);
  if(child == NULL) {
    struct children_entry *child_entry = memb_alloc(&children_mem);
    child_entry->address_to_contact = *from;
    child_entry->address_destination = source_addr;
    list_add(children_list, child_entry);
    printf("[DEBUG - Sensor] Save new child %d.%d (using node %d.%d)\n", source_addr.u8[0], 
      source_addr.u8[1], from->u8[0], from->u8[1]);

  } else if(linkaddr_cmp(&child->address_to_contact, from)) {
    // TODO update to say that connexion is valid

  } else {
    child->address_to_contact = *from;
  }

  printf("[INFO - Sensor] Get data to forward %d (source: %d.%d) from %d.%d to %d.%d, seqno %d\n", 
    forward_data_packet->data, source_addr.u8[0], source_addr.u8[1], from->u8[0], from->u8[1], 
    parent_addr.u8[0], parent_addr.u8[1], seqno);
  struct data_packet_entry *data_entry = memb_alloc(&data_mem);
  data_entry->data = forward_data_packet->data;
  data_entry->address = source_addr;
  list_add(data_list, data_entry);

  // If nothing is transmit
  if(!runicast_is_transmitting(&runicast_data)) {
    process_poll(&send_data_process);
  }

  packetbuf_clear();
}

static void
sent_data_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("[INFO - Sensor] runicast data message sent to %d.%d, retransmissions %d\n",
   to->u8[0], to->u8[1], retransmissions);

  process_poll(&send_data_process);
}

static void
timedout_data_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("[WARN - Sensor] Problem to send data to parent (%d.%d, retransmissions %d). Reset rank !\n", 
    to->u8[0], to->u8[1], retransmissions);
  resetRank();

  // TODO inform children
}

/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// Receive valse packet
static void
recv_valve_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno)
{
  struct valve_packet *forward_valve_packet = packetbuf_dataptr();

  linkaddr_t destination_addr = forward_valve_packet->address;

  if(linkaddr_cmp(&destination_addr, &linkaddr_node_addr)) {
    // TODO change led, launch timer (IN ANOTHER THRED !!!)
    printf("[INFO - Sensor] Get valve information\n");

  } else {
    printf("[INFO - Sensor] Get valve packet to forward to: %d.%d from: %d.%d, seqno %d\n", 
      destination_addr.u8[0], destination_addr.u8[1], from->u8[0], from->u8[1], seqno);

    struct valve_packet_address_entry *entry = memb_alloc(&valve_mem);
    entry->address = forward_valve_packet->address;
    list_add(valve_list, entry);

    if(!runicast_is_transmitting(&runicast_valve)) {
      process_poll(&valve_data_process);
    }

  }
  packetbuf_clear();
  
}

static void
sent_valve_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("[INFO - Sensor] runicast valve message sent to %d.%d, retransmissions %d\n",
   to->u8[0], to->u8[1], retransmissions);

  process_poll(&valve_data_process);
}

static void
timedout_valve_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("[WARN - Sensor] runicast valve message timed out when sending to child %d.%d, retransmissions %d\n",
   to->u8[0], to->u8[1], retransmissions);

  // TODO remove children from the list (if contact/destination address are still the same)
}

/*---------------------------------------------------------------------------*/


static const struct runicast_callbacks runicast_data_callbacks = {
                   recv_data_runicast,
                   sent_data_runicast,
                   timedout_data_runicast};

static const struct runicast_callbacks runicast_rank_callbacks = {
                   recv_rank_runicast,
                   sent_rank_runicast,
                   timedout_rank_runicast};

static const struct runicast_callbacks runicast_valve_callbacks = {
                   recv_valve_runicast,
                   sent_valve_runicast,
                   timedout_valve_runicast};



/*================================ THREADS ==================================*/

/*---------------------------------------------------------------------------*/
// SEND BROADCAST
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
PROCESS_THREAD(broadcast_process, ev, data)
{
  static struct etimer et;

  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)

  PROCESS_BEGIN();

  broadcast_open(&broadcast, BROADCAST_CHANNEL, &broadcast_call);

  // Delay for broadcast
  // printf("Broadcast message between: %i sec and %i sec\n", BROADCAST_DELAY + 0 % (BROADCAST_DELAY), 
  //     BROADCAST_DELAY + (BROADCAST_DELAY-1) % (BROADCAST_DELAY));

  // First broadcast send between 0 sec and max broadcast delay
  etimer_set(&et, random_rand() % (CLOCK_SECOND * BROADCAST_DELAY));
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

  while(1) {

    broadcast_send(&broadcast);
    printf("[DEBUG - Sensor] broadcast message sent\n");

    // If no rank
    if(rank == MAX_RANK) {
      /* Delay between BROADCAST_DELAY and 2*BROADCAST_DELAY seconds */
      etimer_set(&et, CLOCK_SECOND * BROADCAST_DELAY + random_rand() % (CLOCK_SECOND * BROADCAST_DELAY));
    } else {
      /* Delay between BROADCAST_DELAY*4 and 8*BROADCAST_DELAY seconds */
      etimer_set(&et, CLOCK_SECOND * (BROADCAST_DELAY*4) + random_rand() % (CLOCK_SECOND * (BROADCAST_DELAY*4)));
    }

    // Wait end of timer of manual call
    // PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    PROCESS_WAIT_EVENT();
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// SEND RANK RESPONSE
PROCESS_THREAD(rank_process, ev, data)
{
  static struct etimer et;

  PROCESS_EXITHANDLER(runicast_close(&runicast_rank);)

  PROCESS_BEGIN();

  runicast_open(&runicast_rank, RUNICAST_CHANNEL_BROADCAST, &runicast_rank_callbacks);

  list_init(rank_list);
  memb_init(&rank_mem);

  while(1) {

    PROCESS_WAIT_EVENT();
    printf("[DEBUG - Sensor] Wake up rank_process\n");

    while(list_length(rank_list) > 0) {
      while (runicast_is_transmitting(&runicast_rank)) {
        printf("[DEBUG - Sensor] Wait runicast_rank: %s\n", PROCESS_CURRENT()->name);
        PROCESS_WAIT_EVENT();
        printf("[DEBUG - Sensor] Wake up rank_process end of transmitting\n");
      }

      struct rank_packet_entry *entry = list_pop(rank_list);
      linkaddr_t destination_addr = entry->destination;
      packetbuf_clear();

      printf("[INFO - Sensor] Send rank information to %d.%d (%d rank in queue)\n", 
        destination_addr.u8[0], destination_addr.u8[1], list_length(rank_list));

      struct rank_packet packet;
      packet.rank = rank;
      packetbuf_copyfrom(&packet, sizeof(struct rank_packet));

      runicast_send(&runicast_rank, &destination_addr, MAX_RETRANSMISSIONS);
      packetbuf_clear();

    }

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
  new_data_event = process_alloc_event();

  while(1) {
    etimer_set(&et, CLOCK_SECOND * DATA_COLLECTING_DELAY);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    // Generates a random int between 0 and 100
    uint8_t random_int = random_rand() % (100 + 1 - 0) + 0; // (0 for completeness)
    struct data_packet_entry *entry = memb_alloc(&data_mem);
    entry->data = random_int;
    entry->address = linkaddr_node_addr;
    printf("[INFO - Sensor] Collect new data %d (%d data already in queue)\n", random_int, 
      list_length(data_list));
    list_add(data_list, entry);

    if(!runicast_is_transmitting(&runicast_data)) {
      process_post(&send_data_process, new_data_event, NULL);
    }
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
    // etimer_set(&et, CLOCK_SECOND * DATA_MIN_DELAY + random_rand() % (CLOCK_SECOND * DATA_MAX_DELAY));
    // PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    // PROCESS_YIELD();
    PROCESS_WAIT_EVENT();
    if (ev == new_data_event) {
      printf("[DEBUG - Sensor] Wake up because of new value !\n");
      etimer_set(&et, random_rand() % (CLOCK_SECOND * DATA_MAX_DELAY));
      PROCESS_WAIT_EVENT();
      printf("[DEBUG - Sensor] end timer or event\n");
    } else {
      printf("[DEBUG - Sensor] Wake up send_data_process\n");
    }

    if(rank != MAX_RANK) {
      while(list_length(data_list) > 0) {
        while (runicast_is_transmitting(&runicast_data)) {
          printf("[DEBUG - Sensor] Wait runicast_data: %s\n", PROCESS_CURRENT()->name);
          PROCESS_WAIT_EVENT();
          printf("[DEBUG - Sensor] Wake up send_data_process end of transmitting\n");
        }

        struct data_packet_entry *entry = list_pop(data_list);
        memb_free(&data_mem, entry);
        packetbuf_clear();

        struct data_packet packet;
        packet.data = entry->data;
        packet.address = entry->address;
        packetbuf_copyfrom(&packet, sizeof(struct data_packet));
        
        runicast_send(&runicast_data, &parent_addr, MAX_RETRANSMISSIONS);
        printf("[INFO - Sensor] Send data %d (%d data in queue)\n", packet.data, 
            list_length(data_list));
        packetbuf_clear();
      }

    }

  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
// SEND VALVE INFORMATION PROCESS
PROCESS_THREAD(valve_data_process, ev, data)
{
  PROCESS_EXITHANDLER(runicast_close(&runicast_valve);)
  PROCESS_BEGIN();

  runicast_open(&runicast_valve, RUNICAST_CHANNEL_VALVE, &runicast_valve_callbacks);
  list_init(children_list);
  memb_init(&children_mem);

  list_init(valve_list);
  memb_init(&valve_mem);

  while(1) {

    PROCESS_WAIT_EVENT();
    printf("[DEBUG - Sensor] Wake up valve_data_process\n");

    while(list_length(valve_list) > 0) {
      while (runicast_is_transmitting(&runicast_valve)) {
        printf("[DEBUG - Sensor] Wait runicast_valve: %s\n", PROCESS_CURRENT()->name);
        PROCESS_WAIT_EVENT();
        printf("[DEBUG - Sensor] Wake up valve_data_process end of transmitting\n");
      }

      struct valve_packet_address_entry *entry = list_pop(valve_list);
      linkaddr_t destination_addr = entry->address;
      packetbuf_clear();

      struct children_entry *child = get_child_entry(&destination_addr);

      if(child == NULL) {
        printf("[WARN - Sensor] Could not send packet to %d.%d. Destination unknow\n", 
          destination_addr.u8[0], destination_addr.u8[1]);
      } else {
        linkaddr_t address_to_contact = child->address_to_contact;

        printf("[INFO - Sensor] Send valve information to %d.%d (%d valve in queue)\n", 
          address_to_contact.u8[0], address_to_contact.u8[1], list_length(valve_list));

        struct valve_packet packet;
        packet.address = destination_addr;
        packetbuf_copyfrom(&packet, sizeof(struct valve_packet));

        runicast_send(&runicast_valve, &address_to_contact, MAX_RETRANSMISSIONS);
        packetbuf_clear();
      }

    }

  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

/*===========================================================================*/
