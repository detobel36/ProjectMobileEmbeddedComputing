#include <stdio.h>

#include "contiki.h"
#include "net/rime/rime.h"

#include "lib/list.h"
#include "lib/memb.h"

#include "dev/button-sensor.h"
#include "dev/leds.h"
#include "dev/serial-line.h"
#include "random.h"
#include <stdbool.h>


#include "../Common/PacketStruct.c"
#include "../Common/Constants.c"



/*---------------------------------------------------------------------------*/
// VARIABLES
static struct broadcast_conn broadcast;
static struct runicast_conn runicast_rank;
static struct runicast_conn runicast_data;
static struct runicast_conn runicast_valve;
static uint8_t rank = 0;
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// Allocate memory

// Doc about list: http://contiki.sourceforge.net/docs/2.6/a01682.html
MEMB(valve_mem, struct valve_packet_entry, NUM_DATA_IN_QUEUE);
LIST(valve_list);

MEMB(children_mem, struct children_entry, NUM_MAX_CHILDREN);
LIST(children_list);

MEMB(rank_mem, struct rank_packet_entry, NUM_MAX_CHILDREN);
LIST(rank_list);
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// PROCESS
PROCESS(send_valve_process, "Send valve process");
PROCESS(serialProcess, "Serial communications with server");
PROCESS(rank_process, "Rank process");
AUTOSTART_PROCESSES(&send_valve_process, &serialProcess, &rank_process);
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// UTILS
static uint8_t char_to_int(char* char_text)
{
  uint8_t result = 0;
  int i;
  for (i = 0; i < strlen(char_text); ++i) {
    result = result*10;
    result += (char_text[i] - '0');
  }

  return result;
}

// TODO redondant with sensorNode
static struct children_entry* get_child_entry(const uint8_t u8_0, const uint8_t u8_1) {
  struct children_entry *child;
  for(child = list_head(children_list); child != NULL; child = list_item_next(child)) {
    if(child->address_destination.u8[0] == u8_0 && child->address_destination.u8[1] == u8_1) {
      break;
    }
  }
  return child;
}
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// Receive broadcast
static void
broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  struct rank_packet_entry *packet_response = memb_alloc(&rank_mem);
  packet_response->destination = *from;
  printf("[INFO - Border] Create response rank for %d.%d\n", from->u8[0], from->u8[1]);
  list_add(rank_list, packet_response);
  packetbuf_clear();

  if(!runicast_is_transmitting(&runicast_rank)) {
    printf("[DEBUG - Border] New rank, try wake up rank_process\n");
    process_poll(&rank_process);
  }
  packetbuf_clear();
}

/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// Receive broadcast response
static void
recv_rank_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno)
{
  printf("[SEVERE - Border] Not normal to be here (recv_rank_runicast)\n");
}

static void
sent_rank_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("[INFO - Border] runicast rank message sent to %d.%d, retransmissions %d\n",
   to->u8[0], to->u8[1], retransmissions);
  process_poll(&rank_process);
}

static void
timedout_rank_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("[WARN - Border] runicast rank message timed out when sending to %d.%d, retransmissions %d\n",
   to->u8[0], to->u8[1], retransmissions);
  process_poll(&rank_process);
}

/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// Receive data
static void
recv_data_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno)
{
  // printf("Data packet from %d.%d, seqno %d\n", from->u8[0], from->u8[1], seqno);
  struct data_packet *data_packet = packetbuf_dataptr();
  linkaddr_t source_addr = data_packet->address;

  struct children_entry *child = get_child_entry(source_addr.u8[0], source_addr.u8[1]);
  if(child == NULL) {
    struct children_entry *child_entry = memb_alloc(&children_mem);
    child_entry->address_to_contact = *from;
    child_entry->address_destination = source_addr;
    list_add(children_list, child_entry);
    printf("[DEBUG - Border] Save new child %d.%d (using node %d.%d)\n", 
      source_addr.u8[0], source_addr.u8[1],
      from->u8[0], from->u8[1]);

  } else if(linkaddr_cmp(&child->address_to_contact, from)) {
    // TODO update to say that connexion is valid

  } else {
    child->address_to_contact = *from;
  }

  printf("[INFO - Border] Receive data %d (source %d.%d) from: %d.%d (seqno %d)\n", 
    data_packet->data, source_addr.u8[0], source_addr.u8[1], from->u8[0], from->u8[1], seqno);
  // Send information to server (only message with prefix "[DATA]")
  printf("[DATA] %d.%d - %d\n", source_addr.u8[0], source_addr.u8[1], data_packet->data);

  packetbuf_clear();
}

static void
sent_data_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("[SEVERE - Border] Not normal to send data packet (to %d.%d), retransmissions %d\n", 
    to->u8[0], to->u8[1], retransmissions);
}

static void
timedout_data_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("[SEVERE - Border] Not normal to try to send data packet (to %d.%d), retransmissions %d\n", 
    to->u8[0], to->u8[1], retransmissions);
}
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// Send Valve
static void
recv_valve_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno)
{
  printf("[SEVERE - Border] Not normal to receive valve packet (from %d.%d)\n", 
    from->u8[0], from->u8[1]);
}

static void
sent_valve_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("[INFO - Border] runicast valve message sent to %d.%d, retransmissions %d\n",
   to->u8[0], to->u8[1], retransmissions);

  process_poll(&send_valve_process);
}

static void
timedout_valve_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("[WARN - Border] runicast valve message timed out when sending to %d.%d, retransmissions %d\n",
   to->u8[0], to->u8[1], retransmissions);

  process_poll(&send_valve_process);
  // TODO remove children from the list (if contact/destination address are still the same)
}
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
static const struct runicast_callbacks runicast_data_callbacks = {
                   recv_data_runicast,
                   sent_data_runicast,
                   timedout_data_runicast};

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};

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
// SEND VALVE PROCESS
PROCESS_THREAD(send_valve_process, ev, data)
{
  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)

  PROCESS_EXITHANDLER(runicast_close(&runicast_valve);)
  PROCESS_EXITHANDLER(runicast_close(&runicast_data);)


  PROCESS_BEGIN();


  broadcast_open(&broadcast, BROADCAST_CHANNEL, &broadcast_call);

  runicast_open(&runicast_data, RUNICAST_CHANNEL_DATA, &runicast_data_callbacks);

  runicast_open(&runicast_valve, RUNICAST_CHANNEL_VALVE, &runicast_valve_callbacks);


  list_init(valve_list);
  memb_init(&valve_mem);

  while(1) {
    // TODO add delay to avoid send -> reply directly
    printf("[DEBUG - Border] <%s> Wait event different that server\n", PROCESS_CURRENT()->name);
    PROCESS_WAIT_EVENT_UNTIL(ev != serial_line_event_message);
    printf("[DEBUG - Border] Wake up send_valve_process (valve: %d)\n", list_length(valve_list));

    while(list_length(valve_list) > 0) {

      while (runicast_is_transmitting(&runicast_valve)) {
        printf("[DEBUG - Border] Wait runicast_valve: %s\n", PROCESS_CURRENT()->name);
        PROCESS_WAIT_EVENT_UNTIL(ev != serial_line_event_message);
        printf("[DEBUG - Border] Wake up send_valve_process end of transmitting\n");
      }

      struct valve_packet_entry *entry = list_pop(valve_list);
      memb_free(&valve_mem, entry);
      packetbuf_clear();

      struct children_entry *child = get_child_entry(entry->address_u8_0, entry->address_u8_1);

      if(child == NULL) {
        printf("[WARN - Border] Could not send packet to %d.%d. Destination unknow\n", 
          entry->address_u8_0, entry->address_u8_1);
      } else {
        linkaddr_t address_destination = child->address_destination;
        linkaddr_t address_to_contact = child->address_to_contact;

        struct valve_packet packet;
        packet.address = address_destination;
        packetbuf_copyfrom(&packet, sizeof(struct valve_packet));

        runicast_send(&runicast_valve, &address_to_contact, MAX_RETRANSMISSIONS);
        printf("[INFO - Border] Send valve information with destination %d.%d (%d data in queue)\n", 
            address_destination.u8[0], address_destination.u8[1], list_length(valve_list));
        packetbuf_clear();
      }

    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// SEND RANK RESPONSE
PROCESS_THREAD(rank_process, ev, data)
{
  PROCESS_EXITHANDLER(runicast_close(&runicast_rank);)

  PROCESS_BEGIN();

  static struct etimer et;
  runicast_open(&runicast_rank, RUNICAST_CHANNEL_BROADCAST, &runicast_rank_callbacks);

  list_init(rank_list);
  memb_init(&rank_mem);

  while(1) {
    printf("[DEBUG - Border] <%s> Wait event different of server server\n", PROCESS_CURRENT()->name);
    PROCESS_WAIT_EVENT_UNTIL(ev != serial_line_event_message);
    printf("[DEBUG - Border] <%s> Thread wake up, start timer\n", PROCESS_CURRENT()->name);

    // Add delay to reply
    etimer_set(&et, random_rand() % (CLOCK_SECOND * BROADCAST_REPLY_DELAY));
    // If event or timer
    PROCESS_WAIT_EVENT_UNTIL(ev != serial_line_event_message);
    printf("[DEBUG - Border] <%s> Thread wake up, end of timer or event\n", PROCESS_CURRENT()->name);

    while(list_length(rank_list) > 0) {
      while (runicast_is_transmitting(&runicast_rank)) {
        printf("[DEBUG - Border] Wait runicast_rank: %s\n", PROCESS_CURRENT()->name);
        PROCESS_WAIT_EVENT_UNTIL(ev != serial_line_event_message);
        printf("[DEBUG - Border] Wake up rank_process end of transmitting\n");
      }

      struct rank_packet_entry *entry = list_pop(rank_list);
      memb_free(&rank_mem, entry);
      linkaddr_t destination_addr = entry->destination;
      packetbuf_clear();

      printf("[INFO - Border] Send rank information to %d.%d (%d rank in queue)\n", 
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
// RECEIVE DATA FROM SERVER
PROCESS_THREAD(serialProcess, ev, data)
{
    PROCESS_BEGIN();

    while(1) {
        printf("[DEBUG - Border] <%s> Wait event from server\n", PROCESS_CURRENT()->name);
        PROCESS_WAIT_EVENT_UNTIL(ev == serial_line_event_message);
        printf("[DEBUG - Border] <%s> Thread wake up\n", PROCESS_CURRENT()->name);

        char* receivedData = (char *) data;

        char* u8_split;

        u8_split = strtok(receivedData, ".");
        uint8_t u8_0 = char_to_int(u8_split);
        u8_split = strtok(NULL, ".");
        uint8_t u8_1 = char_to_int(u8_split);

        printf("[INFO - Border] Open valve of node: %d.%d\n", u8_0, u8_1);

        struct valve_packet_entry *valve_entry = memb_alloc(&valve_mem);
        valve_entry->address_u8_0 = u8_0;
        valve_entry->address_u8_1 = u8_1;
        list_add(valve_list, valve_entry);

        if(!runicast_is_transmitting(&runicast_valve)) {
          printf("[DEBUG - Border] New valve, wake up send_valve_process\n");
          process_poll(&send_valve_process);
        }
    }

    PROCESS_END();
}
/*---------------------------------------------------------------------------*/
