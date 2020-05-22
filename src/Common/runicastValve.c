#ifndef runicast_valve_c
#define runicast_valve_c

#include "../Common/Constants.c"
#include "../Common/PacketStruct.c"

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
  printf("[INFO - %s] runicast valve message sent to %d.%d, retransmissions %d\n",
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

static void
open_runicast_valve() {
  runicast_open(&runicast_valve, RUNICAST_CHANNEL_VALVE, &runicast_valve_callbacks);

  list_init(valve_list);
  memb_init(&valve_mem);

  current_valve_seqno = 0;
}

#endif