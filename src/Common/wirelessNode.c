// Used by node that need to discover parent node
// Thus: Computation and Sensor node


/*---------------------------------------------------------------------------*/
// UTILS
static void 
add_reset_packet_to_queue(const linkaddr_t destination) 
{
  struct rank_packet_entry *rank_packet = memb_alloc(&rank_mem);
  rank_packet->destination = destination;

  printf("[DEBUG - %s] Inform children %d.%d that rank have been reset\n", 
    NODE_TYPE, destination.u8[0], destination.u8[1]);

  // list_add add element at the end of the list
  // list_push add element at the begining (first inform children before to try to send to parent)
  list_push(rank_list, rank_packet);
}

static void 
reset_rank() 
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
register_parent(const linkaddr_t from, const uint8_t getting_rank, const uint16_t current_rssi) 
{
  rank = getting_rank+1;
  parent_addr = from;
  parent_rank = getting_rank;
  parent_last_rssi = current_rssi;
}
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// Send reset packet if receive packet when rank is not define
static bool
send_reset_packet_if_no_rank(const linkaddr_t *from)
{
  if(rank == MAX_RANK) {
    // Send a reset packet
    add_reset_packet_to_queue(*from);
    // Wake up process to inform children
    if(!runicast_is_transmitting(&runicast_rank)) {
      process_poll(&rank_process);
    }

    return true;
  }
  return false;
}
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// Forward received data
static void
forward_data(const linkaddr_t *from, const struct data_packet *data_packet) 
{
  uint8_t custom_seqno = data_packet->custom_seqno;
  linkaddr_t source_addr = data_packet->address;

  printf("[INFO - %s] Get data to forward %d (source: %d.%d) from %d.%d to %d.%d, custom_seqno %d\n", 
    NODE_TYPE, data_packet->data, source_addr.u8[0], source_addr.u8[1], from->u8[0], from->u8[1], 
    parent_addr.u8[0], parent_addr.u8[1], custom_seqno);

  struct data_packet_entry *data_entry = memb_alloc(&data_mem);
  data_entry->custom_seqno = ((custom_seqno+1) % NUM_MAX_SEQNO);
  data_entry->data = data_packet->data;
  data_entry->address = source_addr;
  list_add(data_list, data_entry);

  if(!runicast_is_transmitting(&runicast_data)) {
    process_poll(&send_data_process);
  }
}
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// Receive broadcast
static void
broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  struct rank_packet *rank_packet = packetbuf_dataptr();
  uint16_t received_rank = rank_packet->rank;
  printf("[DEBUG - %s] Receive broadcast from %d.%d with rank: %d\n", NODE_TYPE, 
    from->u8[0], from->u8[1], received_rank);

  // Respond to broadcast
  if(rank != MAX_RANK) {
    if(linkaddr_cmp(from, &parent_addr) && received_rank == MAX_RANK) {
      printf("[DEBUG - %s] Parent send reset packet\n", NODE_TYPE);
      reset_rank();
    } else if(received_rank > rank) {
      create_rank_response_packet(from);
      
    } else {
      printf("[DEBUG - %s] Do not send broadcast response if rank is lower than current\n",
        NODE_TYPE);
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
  printf("[DEBUG - %s] Discovery response from %d.%d, rank: %d\n", NODE_TYPE,
    from->u8[0], from->u8[1], getting_rank);

  uint16_t current_rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);

  // If no rank
  if(rank == MAX_RANK && getting_rank != MAX_RANK) {
    register_parent(*from, getting_rank, current_rssi);
    printf("[INFO - %s] Rank update: init rank: %d (quality: %d, parent: %d.%d)\n", NODE_TYPE,
      rank, current_rssi,parent_addr.u8[0], parent_addr.u8[1]);

    // Wake up send of data if parent was lost
    process_poll(&send_data_process);

  // If broadcast from parent
  } else if(linkaddr_cmp(&parent_addr, from)) {

    if(getting_rank == MAX_RANK) {
      printf("[INFO - %s] Receive reset rank packet !\n", NODE_TYPE);
      reset_rank();

    // If parent have a rank upper than knowed rank
    } else if(getting_rank > parent_rank) {
      // If parent change to have upper rank it means it's been reset (may be not receive 
      // the reset packet)

      printf("[WARN - %s] Receive parent rank upper that knowed (old: %d, new: %d). Reset rank !\n", 
        NODE_TYPE, parent_rank, getting_rank);
      reset_rank();

    } else {
      parent_last_rssi = current_rssi;
      printf("[INFO - %s] Update parent quality signal (new: %d)\n", NODE_TYPE, current_rssi);

    }

  // If potential new parent
  } else if(rank > getting_rank) {  // current_rank > new_getting_rank
    if(current_rssi > parent_last_rssi) {
      register_parent(*from, getting_rank, current_rssi);
      printf("[INFO - %s] Rank update: change parent (new: %d.%d) and get rank: %d (quality signal: %d)\n", 
        NODE_TYPE, from->u8[0], from->u8[1], rank, parent_last_rssi);
    } else {
      printf("[INFO - %s] Quality signal not enought to change parent (new: %d, parent: %d)\n", 
        NODE_TYPE, current_rssi, parent_last_rssi);
    }
  }

  packetbuf_clear();
}
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// Receive data packet

static void
sent_data_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("[INFO - %s] runicast data message sent to %d.%d, retransmissions %d\n", NODE_TYPE,
   to->u8[0], to->u8[1], retransmissions);

  process_poll(&send_data_process);
}

static void
timedout_data_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("[WARN - %s] Problem to send data to parent %d.%d (retransmissions %d). Reset rank !\n", 
    NODE_TYPE, to->u8[0], to->u8[1], retransmissions);
  if(number_fail_connection < MAX_FAIL_CONNNECTION_BEFORE_RESET) {
    ++number_fail_connection;
  } else {
    number_fail_connection = 0;
    reset_rank();
  }
}
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
// Method call when valve packet receive
static void 
get_valve_packet();

// Receive valve packet
static void
recv_valve_runicast(const linkaddr_t *from, const struct valve_packet *forward_valve_packet)
{

  if(!linkaddr_cmp(from, &parent_addr)) {
    printf("[WARN - %s] Receive valve information from %d.%d but parent address is %d.%d\n",
      NODE_TYPE, from->u8[0], from->u8[1], parent_addr.u8[0], parent_addr.u8[1]);
  }

  if(parent_last_valve_seqno == forward_valve_packet->custom_seqno) {
    printf("[INFO - %s] Detect duplicate valve from parent %d.%d, custom_seqno: %d\n", 
      NODE_TYPE, parent_addr.u8[0], parent_addr.u8[1], forward_valve_packet->custom_seqno);
  } else {
    parent_last_valve_seqno = forward_valve_packet->custom_seqno;

    linkaddr_t destination_addr = forward_valve_packet->address;

    if(linkaddr_cmp(&destination_addr, &linkaddr_node_addr)) {
      get_valve_packet();

    } else {
      printf("[INFO - %s] Get valve packet to forward to: %d.%d from: %d.%d, custom_seqno %d\n", 
        NODE_TYPE, destination_addr.u8[0], destination_addr.u8[1], from->u8[0], from->u8[1], 
        parent_last_valve_seqno);

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
    printf("[DEBUG - %s] broadcast message sent\n", NODE_TYPE);
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
      printf("[DEBUG - %s] Wake up because of new value !\n", NODE_TYPE);
      etimer_set(&et, random_rand() % (CLOCK_SECOND * DATA_MAX_DELAY));
      PROCESS_WAIT_EVENT();
      printf("[DEBUG - %s] end timer or event\n", NODE_TYPE);
    }

    while(list_length(data_list) > 0 && rank != MAX_RANK) {
      while (runicast_is_transmitting(&runicast_data) && rank != MAX_RANK) {
        printf("[DEBUG - %s] Wait runicast_data: %s\n", NODE_TYPE, PROCESS_CURRENT()->name);
        PROCESS_WAIT_EVENT();
        printf("[DEBUG - %s] Wake up send_data_process end of transmitting\n", NODE_TYPE);
      }

      if(rank == MAX_RANK) {
        break;
      }

      struct data_packet_entry *entry = list_pop(data_list);
      memb_free(&data_mem, entry);
      packetbuf_clear();

      struct data_packet packet;
      packet.custom_seqno = entry->custom_seqno;
      packet.data = entry->data;
      packet.address = entry->address;
      packetbuf_copyfrom(&packet, sizeof(struct data_packet));
      
      runicast_send(&runicast_data, &parent_addr, MAX_RETRANSMISSIONS);
      printf("[INFO - %s] Send data %d with custom seqno %d (%d data in queue)\n", NODE_TYPE, 
        packet.data, packet.custom_seqno, list_length(data_list));
      packetbuf_clear();
    }

  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
