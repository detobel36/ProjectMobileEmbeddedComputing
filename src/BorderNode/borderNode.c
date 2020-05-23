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

#define NODE_TYPE "Border"

#include "../Common/constants.c"
#include "../Common/packetStruct.c"


/*---------------------------------------------------------------------------*/
// VARIABLES
static struct broadcast_conn broadcast;
static uint8_t rank = 0;
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// Allocate memory

// Doc about list: http://contiki.sourceforge.net/docs/2.6/a01682.html

// Save valve packet
MEMB(valve_mem, struct valve_packet_entry, NUM_DATA_IN_QUEUE);
LIST(valve_list);
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// PROCESS
PROCESS(send_valve_process, "Send valve process");
PROCESS(serialProcess, "Serial communications with server");
PROCESS(rank_process, "Rank process");
AUTOSTART_PROCESSES(&send_valve_process, &serialProcess, &rank_process);
/*---------------------------------------------------------------------------*/

#include "../Common/utilsChildren.c"

#include "../Common/runicastData.c"
#include "../Common/runicastRank.c"
#include "../Common/runicastValve.c"


/*---------------------------------------------------------------------------*/
// UTILS

static uint8_t 
char_to_int(char* char_text)
{
  uint8_t result = 0;
  int i;
  for (i = 0; i < strlen(char_text); ++i) {
    result = result*10;
    result += (char_text[i] - '0');
  }

  return result;
}

/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// Receive broadcast
static void
broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  create_rank_response_packet(from);
  packetbuf_clear();
}

/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// Receive broadcast response
static void
recv_rank_runicast(const linkaddr_t *from, const struct rank_packet *rank_packet)
{
  printf("[SEVERE - Border] Not normal to be here (recv_rank_runicast)\n");
}

/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// Receive data packet
static void
recv_data_runicast(const linkaddr_t *from, const struct data_packet *data_packet)
{
  uint8_t custom_seqno = data_packet->custom_seqno;
  linkaddr_t source_addr = data_packet->address;

  // Save children
  struct children_entry *child = get_child_entry(&source_addr);
  if(create_child_or_udpate_and_detect_duplicate(child, from, custom_seqno, source_addr, data_packet->data)) {
    return;
  }

  printf("[INFO - Border] Receive data %d (source %d.%d) from: %d.%d\n", 
    data_packet->data, source_addr.u8[0], source_addr.u8[1], from->u8[0], from->u8[1]);
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
recv_valve_runicast(const linkaddr_t *from, const struct valve_packet *forward_valve_packet)
{
  printf("[SEVERE - Border] Not normal to receive valve packet (from %d.%d)\n", 
    from->u8[0], from->u8[1]);
}
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};

/*---------------------------------------------------------------------------*/



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

  open_runicast_data();
  open_runicast_valve();


  while(1) {
    // TODO add delay to avoid send -> reply directly
    PROCESS_WAIT_EVENT_UNTIL(ev != serial_line_event_message);

    while(list_length(valve_list) > 0) {

      while (runicast_is_transmitting(&runicast_valve)) {
        PROCESS_WAIT_EVENT_UNTIL(ev != serial_line_event_message);
      }

      struct valve_packet_entry *entry = list_pop(valve_list);
      memb_free(&valve_mem, entry);
      packetbuf_clear();

      struct children_entry *child = get_child_entry(&entry->address);

      if(child == NULL) {
        printf("[WARN - Border] Could not send packet to %d.%d. Destination unknow\n", 
          entry->address.u8[0], entry->address.u8[1]);
      } else {
        linkaddr_t address_destination = child->address_destination;
        linkaddr_t address_to_contact = child->address_to_contact;

        struct valve_packet packet;
        packet.address = address_destination;
        packet.custom_seqno = ((++current_valve_seqno) % NUM_MAX_SEQNO);
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
  // runicast_open(&runicast_rank, RUNICAST_CHANNEL_BROADCAST, &runicast_rank_callbacks);

  // list_init(rank_list);
  // memb_init(&rank_mem);
  open_runicast_rank();

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(ev != serial_line_event_message);

    // Add delay to reply
    etimer_set(&et, random_rand() % (CLOCK_SECOND * BROADCAST_REPLY_DELAY));
    // If event or timer
    PROCESS_WAIT_EVENT_UNTIL(ev != serial_line_event_message);

    while(list_length(rank_list) > 0) {
      while (runicast_is_transmitting(&runicast_rank)) {
        PROCESS_WAIT_EVENT_UNTIL(ev != serial_line_event_message);
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
        PROCESS_WAIT_EVENT_UNTIL(ev == serial_line_event_message);

        char* receivedData = (char *) data;

        char* u8_split;

        u8_split = strtok(receivedData, ".");
        uint8_t u8_0 = char_to_int(u8_split);
        u8_split = strtok(NULL, ".");
        uint8_t u8_1 = char_to_int(u8_split);

        linkaddr_t receivedAddr;
        receivedAddr.u8[0] = u8_0;
        receivedAddr.u8[1] = u8_1;

        printf("[INFO - Border] Open valve of node: %d.%d\n", receivedAddr.u8[0], receivedAddr.u8[1]);

        struct valve_packet_entry *valve_entry = memb_alloc(&valve_mem);
        valve_entry->address = receivedAddr;
        list_add(valve_list, valve_entry);

        if(!runicast_is_transmitting(&runicast_valve)) {
          process_poll(&send_valve_process);
        }
    }

    PROCESS_END();
}
/*---------------------------------------------------------------------------*/
