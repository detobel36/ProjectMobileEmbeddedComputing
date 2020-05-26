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

// Event
static process_event_t active_led;
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// PROCESS
PROCESS(broadcast_process, "Broadcast process");
PROCESS(rank_process, "Rank process");   // Thread define in runicastRank.c
PROCESS(send_valve_process, "Send Valve Data process");  // Thread define in runicastValve
PROCESS(send_data_process, "Send Data process");
PROCESS(collect_data_process, "Collect Data process");
PROCESS(led_process, "Manage led process");
AUTOSTART_PROCESSES(&broadcast_process, &rank_process, &collect_data_process, &send_data_process, 
  &send_valve_process, &led_process);
/*---------------------------------------------------------------------------*/

// Manage easily children
#include "../Common/utilsChildren.c"
static void extra_remove_children(const linkaddr_t address_destination) {
  // Nothing to do
}

// Set up variable with rank, parent, ...
#include "../Common/wirelessNode.h"

// Manage runicast_data connection (open, callback and directly get packet on recv)
// Definition of class are in wirelessNode
#include "../Common/runicastData.c"

// Define thread to reply to braodcast with current rank
#include "../Common/runicastRank.c"

// Define thread to send valve packet to children
#include "../Common/runicastValve.c"

// Define thread to broadcast and to send data (collected or data to forward)
// Define sent and timeout of data packet and recv rank packet
#include "../Common/wirelessNode.c"


/*---------------------------------------------------------------------------*/
// Receive data packet
static void
recv_data_runicast(const linkaddr_t *from, const struct data_packet *data_packet)
{

  // If we don't send reset packet
  if(!send_reset_packet_if_no_rank(from)) {
    uint8_t custom_seqno = data_packet->custom_seqno;
    linkaddr_t source_addr = data_packet->address;

    // Save children
    if(create_child_or_udpate_and_detect_duplicate(from, custom_seqno, source_addr, data_packet->data)) {
      return;
    }

    forward_data(from, data_packet);
  }

  packetbuf_clear();
}
/*---------------------------------------------------------------------------*/

static void get_valve_packet()
{
  printf("[DEBUG - Sensor] Get valve information, try to open it\n");
  process_post(&led_process, active_led, NULL);
}


/*================================ THREADS ==================================*/


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
    entry->custom_seqno = ((++current_data_seqno) % NUM_MAX_SEQNO);
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
