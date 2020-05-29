#ifndef runicast_valve_c
#define runicast_valve_c

#include "../Common/constants.c"
#include "../Common/packetStruct.c"

static struct runicast_conn runicast_valve;
static uint8_t current_valve_seqno;

static void
recv_valve_runicast(const linkaddr_t *from, const struct valve_packet *forward_valve_packet);


// Receive valve packet
static void
recv_valve_general_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno)
{
  struct valve_packet *forward_valve_packet = packetbuf_dataptr();
  recv_valve_runicast(from, forward_valve_packet);
}

static void
sent_valve_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("[NOTICE - %s] runicast valve message sent to %d.%d, retransmissions %d\n",
   NODE_TYPE, to->u8[0], to->u8[1], retransmissions);

  process_poll(&send_valve_process);
}

static void
timedout_valve_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("[WARN - %s] runicast valve message timed out when sending to %d.%d, retransmissions %d\n",
   NODE_TYPE, to->u8[0], to->u8[1], retransmissions);

  process_poll(&send_valve_process);

  remove_all_children_linked_to_address(to);
}

static const struct runicast_callbacks runicast_valve_callbacks = {
                   recv_valve_general_runicast,
                   sent_valve_runicast,
                   timedout_valve_runicast};


PROCESS_THREAD(send_valve_process, ev, data)
{
  PROCESS_EXITHANDLER(runicast_close(&runicast_valve);)
  PROCESS_BEGIN();

  list_init(children_list);
  memb_init(&children_mem);

  runicast_open(&runicast_valve, RUNICAST_CHANNEL_VALVE, &runicast_valve_callbacks);

  list_init(valve_list);
  memb_init(&valve_mem);

  current_valve_seqno = 0;


  while(1) {

    PROCESS_WAIT_EVENT_UNTIL(ev != serial_line_event_message);
    printf("[DEBUG - %s] Wake up send_valve_process\n", NODE_TYPE);

    while(list_length(valve_list) > 0) {
      while (runicast_is_transmitting(&runicast_valve)) {
        printf("[DEBUG - %s] Wait runicast_valve: %s\n", NODE_TYPE, PROCESS_CURRENT()->name);
        PROCESS_WAIT_EVENT_UNTIL(ev != serial_line_event_message);
        printf("[DEBUG - %s] Wake up send_valve_process end of transmitting\n", NODE_TYPE);
      }

      struct valve_packet_entry *entry = list_pop(valve_list);
      memb_free(&valve_mem, entry);
      linkaddr_t destination_addr = entry->address;
      packetbuf_clear();

      struct children_entry *child = get_child_entry(&destination_addr);

      if(child == NULL) {
        printf("[WARN - %s] Could not send packet to %d.%d. Destination unknow\n", 
          NODE_TYPE, destination_addr.u8[0], destination_addr.u8[1]);
      } else {
        linkaddr_t address_to_contact = child->address_to_contact;

        printf("[NOTICE - %s] Send valve information to %d.%d (%d valve in queue)\n", 
          NODE_TYPE, address_to_contact.u8[0], address_to_contact.u8[1], list_length(valve_list));

        struct valve_packet packet;
        packet.address = destination_addr;
        packet.custom_seqno = ((++current_valve_seqno) % NUM_MAX_SEQNO);
        packetbuf_copyfrom(&packet, sizeof(struct valve_packet));

        runicast_send(&runicast_valve, &address_to_contact, MAX_RETRANSMISSIONS);
        packetbuf_clear();
      }

    }

  }

  PROCESS_END();
}



#endif