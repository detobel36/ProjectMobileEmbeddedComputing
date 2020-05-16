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
static struct runicast_conn runicast_broadcast;
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
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// PROCESS
// PROCESS(broadcast_process, "Broadcast process");
PROCESS(send_data_process, "Send data process");
PROCESS(serialProcess, "Serial communications with server");
AUTOSTART_PROCESSES(&send_data_process, &serialProcess);
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// UTILS
static uint8_t char_to_int(char* char_text)
{
  return (uint8_t) *char_text - '0';
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
  // Request to find parent

  // Check that transmission already exist
  if(runicast_is_transmitting(&runicast_broadcast)) {
    printf("[WARN - Border] Could not reply to %d.%d, runicast_broadcast is already used\n", 
      from->u8[0], 
        from->u8[1]);
    return;
  }
  packetbuf_clear();

  struct rank_packet packet_response;
  packet_response.rank = rank;

  packetbuf_copyfrom(&packet_response, sizeof(struct rank_packet));
  printf("[INFO - Border] Send response to broadcast\n");
  runicast_send(&runicast_broadcast, from, MAX_RETRANSMISSIONS);
  packetbuf_clear();
}

/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
// Receive broadcast response
static void
recv_broadcast_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno)
{
  printf("[SEVERE - Border] Not normal to be here\n");
}

static void
sent_broadcast_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("[INFO - Border] runicast broadcast message sent to %d.%d, retransmissions %d\n",
   to->u8[0], to->u8[1], retransmissions);
}

static void
timedout_broadcast_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("[INFO - Border] runicast broadcast message timed out when sending to %d.%d, retransmissions %d\n",
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

  printf("[INFO - Border] Receive data packet from: %d.%d with value: %d (seqno %d)\n", source_addr.u8[0], 
    source_addr.u8[1], data_packet->data, seqno);
  printf("[DATA] %d.%d - %d\n", source_addr.u8[0], source_addr.u8[1], data_packet->data);
}

static void
sent_data_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("[INFO - Border] runicast data message sent to %d.%d, retransmissions %d\n",
   to->u8[0], to->u8[1], retransmissions);
}

static void
timedout_data_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("[INFO - Border] runicast data message timed out when sending to %d.%d, retransmissions %d\n",
   to->u8[0], to->u8[1], retransmissions);
}

static void
recv_valve_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno)
{
  printf("[SEVERE - Border] Not normal to be here\n");
}

static void
sent_valve_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("[INFO - Border] runicast valve message sent to %d.%d, retransmissions %d\n",
   to->u8[0], to->u8[1], retransmissions);
}

static void
timedout_valve_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("[INFO - Border] runicast valve message timed out when sending to %d.%d, retransmissions %d\n",
   to->u8[0], to->u8[1], retransmissions);
}

static const struct runicast_callbacks runicast_data_callbacks = {
                   recv_data_runicast,
                   sent_data_runicast,
                   timedout_data_runicast};

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static const struct runicast_callbacks runicast_broadcast_callbacks = {
                   recv_broadcast_runicast,
                   sent_broadcast_runicast,
                   timedout_broadcast_runicast};

static const struct runicast_callbacks runicast_valve_callbacks = {
                   recv_valve_runicast,
                   sent_valve_runicast,
                   timedout_valve_runicast};


/*================================ THREADS ==================================*/


/*---------------------------------------------------------------------------*/
// SEND DATA PROCESS
PROCESS_THREAD(send_data_process, ev, data)
{
  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
  PROCESS_EXITHANDLER(runicast_close(&runicast_broadcast);)

  PROCESS_EXITHANDLER(runicast_close(&runicast_valve);)
  PROCESS_EXITHANDLER(runicast_close(&runicast_data);)


  PROCESS_BEGIN();


  broadcast_open(&broadcast, BROADCAST_CHANNEL, &broadcast_call);
  runicast_open(&runicast_broadcast, RUNICAST_CHANNEL_BROADCAST, &runicast_broadcast_callbacks);

  runicast_open(&runicast_data, RUNICAST_CHANNEL_DATA, &runicast_data_callbacks);

  runicast_open(&runicast_valve, RUNICAST_CHANNEL_VALVE, &runicast_valve_callbacks);


  list_init(valve_list);
  memb_init(&valve_mem);

  static struct etimer et;

  while(1) {
    etimer_set(&et, CLOCK_SECOND * DATA_MIN_DELAY + random_rand() % (CLOCK_SECOND * DATA_MAX_DELAY));
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    if(list_length(valve_list) > 0) {
      if(runicast_is_transmitting(&runicast_valve)) {
        printf("[INFO - Border] Runicast valve is already used\n");
      } else {

        struct valve_packet_entry *entry = list_pop(valve_list);
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
          if(list_length(valve_list) > 0) {
            printf("[INFO - Border] Send valve information with destination %d.%d (%d data in queue)\n", 
              address_destination.u8[0], address_destination.u8[1], list_length(valve_list));
          } else {
            printf("[INFO - Border] Send valve information with destination %d.%d\n", 
              address_destination.u8[0], address_destination.u8[1]);
          }
          packetbuf_clear();
        }

      }

    } else {
      printf("[INFO - Border] No data to send\n");
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
        PROCESS_YIELD();
        if(ev == serial_line_event_message) {
          char* receivedData = (char *) data;

          char* u8_split;

          u8_split = strtok(receivedData, ".");
          uint8_t u8_0 = char_to_int(u8_split);
          u8_split = strtok(NULL, ".");
          uint8_t u8_1 = char_to_int(u8_split);

          printf("[INFO - Border] Open valve of: %d.%d\n", u8_0, u8_1);

          struct valve_packet_entry *valve_entry = memb_alloc(&valve_mem);
          valve_entry->address_u8_0 = u8_0;
          valve_entry->address_u8_1 = u8_1;
          list_add(valve_list, valve_entry);
        }
    }

    PROCESS_END();
}
/*---------------------------------------------------------------------------*/
