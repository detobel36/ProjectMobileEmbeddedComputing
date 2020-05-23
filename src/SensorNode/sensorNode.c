#include <stdio.h>

#include "contiki.h"
#include "net/rime/rime.h"

#include "lib/list.h"
#include "lib/memb.h"

#include "dev/button-sensor.h"
#include "dev/leds.h"
#include "random.h"
#include <stdbool.h>


#define NODE_TYPE "Sensor"

#include "../Common/constants.c"
#include "../Common/packetStruct.c"


/*---------------------------------------------------------------------------*/
// VARIABLES

// Communication system
static struct broadcast_conn broadcast;
static uint8_t rank = MAX_RANK;

// Parent information
static linkaddr_t parent_addr;
static uint8_t parent_rank = MAX_RANK;
static uint16_t parent_last_rssi;

// Avoid duplicate
static uint8_t parent_last_valve_seqno = 255;
static uint8_t current_data_seqno;

// Event
static process_event_t new_data_event;
static process_event_t active_led;
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// Allocate memory

// Doc about list: http://contiki.sourceforge.net/docs/2.6/a01682.html
MEMB(data_mem, struct data_packet_entry, NUM_DATA_IN_QUEUE);
LIST(data_list);

MEMB(valve_mem, struct valve_packet_entry, NUM_DATA_IN_QUEUE);
LIST(valve_list);
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// PROCESS
PROCESS(broadcast_process, "Broadcast process");
PROCESS(rank_process, "Rank process");   // Thread define in runicastRank.c
PROCESS(collect_data_process, "Collect Data process");
PROCESS(send_valve_process, "Send Valve Data process");  // Thread define in runicastValve
PROCESS(send_data_process, "Send Data process");
PROCESS(led_process, "Manage led process");
AUTOSTART_PROCESSES(&broadcast_process, &rank_process, &collect_data_process, &send_data_process, 
  &send_valve_process, &led_process);
/*---------------------------------------------------------------------------*/

#include "../Common/utilsChildren.c"

#include "../Common/runicastData.c"
#include "../Common/runicastRank.c"
#include "../Common/runicastValve.c"


/*---------------------------------------------------------------------------*/
// UTILS
static void 
add_reset_packet_to_queue(const linkaddr_t destination) 
{
  struct rank_packet_entry *rank_packet = memb_alloc(&rank_mem);
  rank_packet->destination = destination;

  printf("[DEBUG - Sensor] Inform children %d.%d that rank have been reset\n", 
    destination.u8[0], destination.u8[1]);

  // list_add add element at the end of the list
  // list_push add element at the begining (first inform children before to try to send to parent)
  list_push(rank_list, rank_packet);
}

static void 
resetRank() 
{
  rank = MAX_RANK;
  parent_rank = MAX_RANK;
  parent_addr = linkaddr_null;

  struct children_entry *child;
  while (list_length(children_list) > 0) {
    child = list_pop(children_list);
    memb_free(&children_mem, child);
    
    // If child is direct linked
    if(linkaddr_cmp(&child->address_destination, &child->address_to_contact)) {
      add_reset_packet_to_queue(child->address_to_contact);
    }

  }

  // Wake up process without delay to inform children
  if(!runicast_is_transmitting(&runicast_rank)) {
    process_poll(&rank_process);
    // process_post(&rank_process, broadcast_add_delay_event, NULL);
  }

  // Wake up broadcast system
  process_poll(&broadcast_process);
}

static void 
registerParent(const linkaddr_t from, const uint8_t getting_rank, const uint16_t current_rssi) 
{
  rank = getting_rank+1;
  parent_addr = from;
  parent_rank = getting_rank;
  parent_last_rssi = current_rssi;
}

/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// Receive broadcast
static void
broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  struct rank_packet *rank_packet = packetbuf_dataptr();
  uint16_t received_rank = rank_packet->rank;
  printf("[DEBUG - Sensor] Receive broadcast from %d.%d with rank: %d\n", 
    from->u8[0], from->u8[1], received_rank);

  // Respond to broadcast
  if(rank != MAX_RANK) {
    if(linkaddr_cmp(from, &parent_addr) && received_rank == MAX_RANK) {
      printf("[DEBUG - Sensor] Parent send reset packet\n");
      resetRank();
    } else if(received_rank > rank) {
      create_rank_response_packet(from);
      
    } else {
      printf("[DEBUG - Sensor] Do not send broadcast response if rank is lower than current\n");
    }

  }

  packetbuf_clear();
}
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// Receive rank packet
static void
recv_rank_runicast(const linkaddr_t *from, const struct rank_packet *rank_packet)
{
  uint16_t getting_rank = rank_packet->rank;
  printf("[DEBUG - Sensor] Discovery response from %d.%d, rank: %d\n", from->u8[0], 
    from->u8[1], getting_rank);

  uint16_t current_rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);

  // If no rank
  if(rank == MAX_RANK && getting_rank != MAX_RANK) {
    registerParent(*from, getting_rank, current_rssi);
    printf("[INFO - Sensor] Rank update: init rank: %d (quality: %d, parent: %d.%d)\n", rank, current_rssi,
      parent_addr.u8[0], parent_addr.u8[1]);

    // Wake up send of data if parent was lost
    process_poll(&send_data_process);

  // If broadcast from parent
  } else if(linkaddr_cmp(&parent_addr, from)) {

    if(getting_rank == MAX_RANK) {
      printf("[INFO - Sensor] Receive reset rank packet !\n");
      resetRank();

    // If parent have a rank upper than knowed rank
    } else if(getting_rank > parent_rank) {
      // If parent change to have upper rank it means it's been reset (may be not receive 
      // the reset packet)

      printf("[WARN - Sensor] Receive parent rank upper that knowed (old: %d, new: %d). Reset rank !\n", 
        parent_rank, getting_rank);
      resetRank();

    } else {
      parent_last_rssi = current_rssi;
      printf("[INFO - Sensor] Update parent quality signal (new: %d)\n", current_rssi);

    }

  // If potential new parent
  } else if(rank > getting_rank) {  // current_rank > new_getting_rank
    if(current_rssi > parent_last_rssi) {
      registerParent(*from, getting_rank, current_rssi);
      printf("[INFO - Sensor] Rank update: change parent (new: %d.%d) and get rank: %d (quality signal: %d)\n", 
        from->u8[0], from->u8[1], rank, parent_last_rssi);
    } else {
      printf("[INFO - Sensor] Quality signal not enought to change parent (new: %d, parent: %d)\n", 
        current_rssi, parent_last_rssi);
    }
  }

  packetbuf_clear();
}
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// Receive data packet
static void
recv_data_runicast(const linkaddr_t *from, const struct data_packet *data_packet)
{

  // If we receive data and we don't have any rank
  if(rank == MAX_RANK) {
    // Send a reset packet
    add_reset_packet_to_queue(*from);
    // Wake up process to inform children
    if(!runicast_is_transmitting(&runicast_rank)) {
      process_poll(&rank_process);
    }
  } else {
    uint8_t custom_seqno = data_packet->custom_seqno;
    linkaddr_t source_addr = data_packet->address;

    // Save children
    struct children_entry *child = get_child_entry(&source_addr);
    if(create_child_or_udpate_and_detect_duplicate(child, from, custom_seqno, source_addr, data_packet->data)) {
      return;
    }

    printf("[INFO - Sensor] Get data to forward %d (source: %d.%d) from %d.%d to %d.%d, custom_seqno %d\n", 
      data_packet->data, source_addr.u8[0], source_addr.u8[1], from->u8[0], from->u8[1], 
      parent_addr.u8[0], parent_addr.u8[1], custom_seqno);
    struct data_packet_entry *data_entry = memb_alloc(&data_mem);
    data_entry->data = data_packet->data;
    data_entry->address = source_addr;
    list_add(data_list, data_entry);

    if(!runicast_is_transmitting(&runicast_data)) {
      process_poll(&send_data_process);
    }

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
  printf("[WARN - Sensor] Problem to send data to parent %d.%d (retransmissions %d). Reset rank !\n", 
    to->u8[0], to->u8[1], retransmissions);
  resetRank();
}

/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// Receive valve packet
static void
recv_valve_runicast(const linkaddr_t *from, const struct valve_packet *forward_valve_packet)
{

  if(!linkaddr_cmp(from, &parent_addr)) {
    printf("[WARN - Sensor] Receive valve information from %d.%d but parent address is %d.%d\n",
      from->u8[0], from->u8[1], parent_addr.u8[0], parent_addr.u8[1]);
  }

  if(parent_last_valve_seqno == forward_valve_packet->custom_seqno) {
    printf("[INFO - Sensor] Detect duplicate valve from parent %d.%d, custom_seqno: %d\n", 
      parent_addr.u8[0], parent_addr.u8[1], forward_valve_packet->custom_seqno);
  } else {
    parent_last_valve_seqno = forward_valve_packet->custom_seqno;

    linkaddr_t destination_addr = forward_valve_packet->address;

    if(linkaddr_cmp(&destination_addr, &linkaddr_node_addr)) {
      printf("[DEBUG - Sensor] Get valve information, try to open it\n");
      process_post(&led_process, active_led, NULL);

    } else {
      printf("[INFO - Sensor] Get valve packet to forward to: %d.%d from: %d.%d, custom_seqno %d\n", 
        destination_addr.u8[0], destination_addr.u8[1], from->u8[0], from->u8[1], parent_last_valve_seqno);

      struct valve_packet_entry *entry = memb_alloc(&valve_mem);
      entry->address = forward_valve_packet->address;
      list_add(valve_list, entry);

      if(!runicast_is_transmitting(&runicast_valve)) {
        process_poll(&send_valve_process);
      }

    }

  }

  packetbuf_clear();
}
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};

/*---------------------------------------------------------------------------*/



/*================================ THREADS ==================================*/


/*---------------------------------------------------------------------------*/
// SEND BROADCAST
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
    packetbuf_clear();
    struct rank_packet packet;
    packet.rank = rank;
    packetbuf_copyfrom(&packet, sizeof(struct rank_packet));
    broadcast_send(&broadcast);
    printf("[DEBUG - Sensor] broadcast message sent\n");
    packetbuf_clear();

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

  open_runicast_data();
  list_init(data_list);
  memb_init(&data_mem);

  current_data_seqno = 0;

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
    }

    while(list_length(data_list) > 0 && rank != MAX_RANK) {
      while (runicast_is_transmitting(&runicast_data) && rank != MAX_RANK) {
        printf("[DEBUG - Sensor] Wait runicast_data: %s\n", PROCESS_CURRENT()->name);
        PROCESS_WAIT_EVENT();
        printf("[DEBUG - Sensor] Wake up send_data_process end of transmitting\n");
      }

      if(rank == MAX_RANK) {
        break;
      }

      struct data_packet_entry *entry = list_pop(data_list);
      memb_free(&data_mem, entry);
      packetbuf_clear();

      struct data_packet packet;
      packet.custom_seqno = ((++current_data_seqno) % NUM_MAX_SEQNO);
      packet.data = entry->data;
      packet.address = entry->address;
      packetbuf_copyfrom(&packet, sizeof(struct data_packet));
      
      runicast_send(&runicast_data, &parent_addr, MAX_RETRANSMISSIONS);
      printf("[INFO - Sensor] Send data %d with custom seqno %d (%d data in queue)\n", packet.data, 
          packet.custom_seqno, list_length(data_list));
      packetbuf_clear();
    }

  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
// MANAGE LED
PROCESS_THREAD(led_process, ev, data)
{
  PROCESS_BEGIN();

  active_led = process_alloc_event();
  static struct etimer et;

  while(1) {
    // Wait 
    PROCESS_WAIT_EVENT_UNTIL(ev == active_led);
    printf("[INFO - Sensor] Valve is now open\n");
    leds_on(LEDS_ALL);
    do {
      etimer_set(&et, CLOCK_SECOND * ACTIVE_LED_TIMER);
      PROCESS_WAIT_EVENT_UNTIL(ev == active_led || etimer_expired(&et));
      if(ev == active_led) {
        printf("[INFO - Sensor] Valve is already open, reset timer\n");
      }
    } while(ev == active_led);

    // When timer is done
    printf("[INFO - Sensor] Timer is out, valve is now closed\n");
    leds_off(LEDS_ALL);
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

/*===========================================================================*/
