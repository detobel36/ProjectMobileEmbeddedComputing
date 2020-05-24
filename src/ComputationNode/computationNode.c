#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#include "contiki.h"
#include "net/rime/rime.h"

#include "lib/list.h"
#include "lib/memb.h"

#include "dev/button-sensor.h"
#include "dev/leds.h"
#include "random.h"
#include <stdbool.h>


#define NODE_TYPE "Computation"

#include "../Common/constants.c"
#include "../Common/packetStruct.c"


/*---------------------------------------------------------------------------*/
// Allocate memory

// Doc about list: http://contiki.sourceforge.net/docs/2.6/a01682.html
MEMB(save_node_mem, struct saved_node_to_compute_entry, NUMBER_SENSOR_IN_COMPUTATION);
LIST(save_node_list);

MEMB(save_data_mem, struct saved_data_to_compute_entry, NUMBER_SENSOR_IN_COMPUTATION*NUMBER_OF_DATA_TO_COMPUTE);
LIST(save_data_list_0);
LIST(save_data_list_1);
LIST(save_data_list_2);
LIST(save_data_list_3);
LIST(save_data_list_4);

MEMB(list_of_list_data_mem, struct list_of_list_saved_data_entry, NUMBER_SENSOR_IN_COMPUTATION);
LIST(list_of_list_data_list);

/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// PROCESS
PROCESS(broadcast_process, "Broadcast process");
PROCESS(rank_process, "Rank process");   // Thread define in runicastRank.c
PROCESS(send_valve_process, "Send Valve Data process");  // Thread define in runicastValve
PROCESS(send_data_process, "Send Data process");
PROCESS(compute_data_process, "Compute Data process");
AUTOSTART_PROCESSES(&broadcast_process, &rank_process, &send_valve_process, &send_data_process, 
  &compute_data_process);
/*---------------------------------------------------------------------------*/

// Manage easily children
#include "../Common/utilsChildren.c"

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
// Manage data for the computation
static list_t* 
get_empty_saved_data_list()
{
  struct list_of_list_saved_data_entry *list_of_list_entry;

  for(list_of_list_entry = list_head(list_of_list_data_list); list_of_list_entry != NULL; 
    list_of_list_entry = list_item_next(list_of_list_entry)) {

    if(list_length(*list_of_list_entry->saved_data_list) == 0) {
      return list_of_list_entry->saved_data_list;
    }
  }
  return NULL;
}

static void 
extra_remove_children(const linkaddr_t address_destination) 
{
  struct saved_node_to_compute_entry *node_entry;
  for(node_entry = list_head(save_node_list); node_entry != NULL; node_entry = list_item_next(node_entry)) {
    if(linkaddr_cmp(&node_entry->from, &address_destination)) {
      break;
    }
  }

  if(node_entry != NULL) {
    list_t *saved_data_list = node_entry->saved_data_list;

    struct saved_data_to_compute_entry *saved_data;
    struct saved_data_to_compute_entry *new_saved_data;
    for(saved_data = list_head(*saved_data_list); saved_data != NULL; saved_data = new_saved_data) {
      new_saved_data = list_item_next(saved_data);

      list_remove(*saved_data_list, saved_data);
      memb_free(&save_data_mem, saved_data);
    }

    list_remove(save_node_list, node_entry);
    memb_free(&save_node_mem, node_entry);
    printf("[DEBUG - Computation] Remove all data of the node %d.%d\n", 
      address_destination.u8[0], address_destination.u8[1]);

  }

}

static double 
computeLeastSquare(list_t* saved_data_list) 
{
  double x = 0;
  double x2 = 0;
  double xy = 0;
  double y = 0;
  double y2 = 0;

  uint8_t i = 0;
  struct saved_data_to_compute_entry *saved_data;
  printf("[DEBUG - Computation] Data: ");
  for(saved_data = list_head(*saved_data_list); saved_data != NULL; saved_data = list_item_next(saved_data)) {
    printf("%d ", saved_data->data);

    x  += i;
    x2 += i * i;
    xy += i * saved_data->data;
    y  += saved_data->data; 
    y2 += saved_data->data * saved_data->data;

    ++i;
  }
  printf("\n");

  if(i != NUMBER_OF_DATA_TO_COMPUTE) {
    printf("[SEVERE - Computation] Number of data computed (%d) not equals to %d\n", i, 
      NUMBER_OF_DATA_TO_COMPUTE);
  }

  double denom = (NUMBER_OF_DATA_TO_COMPUTE * x2 - x*x);

  return (NUMBER_OF_DATA_TO_COMPUTE * xy  -  x * y) / denom;
}


static bool
try_to_save_data_to_compute(const struct data_packet *data_packet)
{

  struct saved_node_to_compute_entry *node_entry;
  for(node_entry = list_head(save_node_list); node_entry != NULL; node_entry = list_item_next(node_entry)) {
    if(linkaddr_cmp(&node_entry->from, &data_packet->address)) {
      break;
    }
  }

  if(node_entry == NULL && list_length(save_node_list) < NUMBER_SENSOR_IN_COMPUTATION) {
    list_t* empty_saved_data = get_empty_saved_data_list();
    if(empty_saved_data == NULL) {
      printf("[SEVERE - Computation] Place in save_node_list (length %d) but no empty_saved_data !\n",
        list_length(save_node_list));
      return false;
    }

    node_entry = memb_alloc(&save_node_mem);
    node_entry->from = data_packet->address;
    node_entry->saved_data_list = empty_saved_data;
    list_push(save_node_list, node_entry);
  }

  if(node_entry != NULL) {
    // If list of data is full
    if(list_length(*node_entry->saved_data_list) == NUMBER_OF_DATA_TO_COMPUTE) {
      // Remove latest
      void * removed_elem = list_pop(*node_entry->saved_data_list);
      memb_free(&save_data_mem, removed_elem);
      printf("[DEBUG - Computation] Remove oldest element of %d.%d, length of list %d\n", 
        data_packet->address.u8[0], data_packet->address.u8[1],
        list_length(*node_entry->saved_data_list));
    }

    struct saved_data_to_compute_entry *saved_data = memb_alloc(&save_data_mem);
    saved_data->data = data_packet->data;
    list_add(*node_entry->saved_data_list, saved_data);
    node_entry->already_computed = false;
    printf("[INFO - Computation] Save data %d of node %d.%d (total: %d)\n", data_packet->data, 
      data_packet->address.u8[0], data_packet->address.u8[1], 
      list_length(*node_entry->saved_data_list));
    process_poll(&compute_data_process);
    return true;
  }

  return false;
}
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
// Receive data packet
static void
recv_data_runicast(const linkaddr_t *from, const struct data_packet *data_packet)
{

  // If we receive data and we don't have any rank
  send_reset_packet_if_no_rank(from);

  // But in all the way we try to compute data
  uint8_t custom_seqno = data_packet->custom_seqno;
  linkaddr_t source_addr = data_packet->address;

  // Save children
  if(create_child_or_udpate_and_detect_duplicate(from, custom_seqno, source_addr, data_packet->data)) {
    return;
  }

  if(try_to_save_data_to_compute(data_packet)) {
    printf("[DEBUG - Computation] Do not forward data of source %d.%d (used to local computation)\n",
      source_addr.u8[0], source_addr.u8[1]);
    return;
  }

  forward_data(from, data_packet);

  packetbuf_clear();
}
/*---------------------------------------------------------------------------*/



/*================================ THREADS ==================================*/


/*---------------------------------------------------------------------------*/
// COMPUTE DATA
PROCESS_THREAD(compute_data_process, ev, data)
{
  static struct etimer et;

  PROCESS_BEGIN();

  // Inform parameters
  printf("[INFO - Computation] Start Computation node with %d slot for nodes, collect %d data before computation and threshold is define on %d\n",
    NUMBER_SENSOR_IN_COMPUTATION, NUMBER_OF_DATA_TO_COMPUTE, THRESHOLD);

  list_init(save_node_list);
  memb_init(&save_node_mem);

  list_init(list_of_list_data_list);
  memb_init(&list_of_list_data_mem);

  struct list_of_list_saved_data_entry *list_of_list_entry;

  memb_init(&save_data_mem);

  list_init(save_data_list_0);
  list_of_list_entry = memb_alloc(&list_of_list_data_mem);
  list_of_list_entry->saved_data_list = &save_data_list_0;
  list_add(list_of_list_data_list, list_of_list_entry);

  list_init(save_data_list_1);
  list_of_list_entry = memb_alloc(&list_of_list_data_mem);
  list_of_list_entry->saved_data_list = &save_data_list_1;
  list_add(list_of_list_data_list, list_of_list_entry);

  list_init(save_data_list_2);
  list_of_list_entry = memb_alloc(&list_of_list_data_mem);
  list_of_list_entry->saved_data_list = &save_data_list_2;
  list_add(list_of_list_data_list, list_of_list_entry);

  list_init(save_data_list_3);
  list_of_list_entry = memb_alloc(&list_of_list_data_mem);
  list_of_list_entry->saved_data_list = &save_data_list_3;
  list_add(list_of_list_data_list, list_of_list_entry);

  list_init(save_data_list_4);
  list_of_list_entry = memb_alloc(&list_of_list_data_mem);
  list_of_list_entry->saved_data_list = &save_data_list_4;
  list_add(list_of_list_data_list, list_of_list_entry);

  while(1) {

    etimer_set(&et, CLOCK_SECOND * TIME_TO_FORCE_COMPUTATION_OF_DATA);
    PROCESS_WAIT_EVENT();
    printf("[DEBUG - Computation] Process compute_data_process have been poll\n");

    struct saved_node_to_compute_entry *node_entry;
    for(node_entry = list_head(save_node_list); node_entry != NULL; node_entry = list_item_next(node_entry)) {
      list_t* saved_data_list = node_entry->saved_data_list;

      if(list_length(*saved_data_list) == NUMBER_OF_DATA_TO_COMPUTE && !node_entry->already_computed) {
        printf("[DEBUG - Computation] Need to compute value of %d.%d\n", 
          node_entry->from.u8[0], node_entry->from.u8[1]);
        double result = computeLeastSquare(saved_data_list);
        printf("[DEBUG - Computation] Result: %d\n", (int) result);
        printf("[DEBUG - Computation] ----------\n");

        if(result > THRESHOLD) {
          struct valve_packet_entry *valve_entry = memb_alloc(&valve_mem);
          valve_entry->address = node_entry->from;
          list_add(valve_list, valve_entry);
          printf("[INFO - Computation] Open valve of node: %d.%d\n", node_entry->from.u8[0], 
            node_entry->from.u8[1]);

          if(!runicast_is_transmitting(&runicast_valve)) {
            process_poll(&send_valve_process);
          }
        } else {
          printf("[INFO - Computation] Value of node %d.%d %d is not upper than threshold %d\n",
            node_entry->from.u8[0], node_entry->from.u8[1], (int) result, THRESHOLD);
        }

        node_entry->already_computed = true;
      } else {
        printf("[DEBUG - Computation] Nothing to compute for node %d.%d (nbr data: %d, computed: %d)\n",
          node_entry->from.u8[0], node_entry->from.u8[1], list_length(*saved_data_list), 
          node_entry->already_computed);
      }
      
    }

  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/


/*===========================================================================*/
