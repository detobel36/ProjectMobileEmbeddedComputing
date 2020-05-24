#ifndef packetStruct_c
#define packetStruct_c

//// PACKET ////

struct rank_packet 
{
  uint8_t rank;
};

struct data_packet
{
  uint8_t custom_seqno;
  uint8_t data;
  linkaddr_t address;
};

// Valve packet always tell that the valve need to be open
struct valve_packet
{
  uint8_t custom_seqno;
  linkaddr_t address;
};


//// LIST ////

struct rank_packet_entry
{
  struct rank_packet_entry *next;
  linkaddr_t destination;
};

struct data_packet_entry
{
  struct data_packet_entry *next;
  uint8_t custom_seqno;
  uint8_t data;
  linkaddr_t address;
};

struct valve_packet_entry
{
  struct valve_packet_entry *next;
  linkaddr_t address;
};

struct children_entry
{
  struct children_entry *next;
  linkaddr_t address_destination;
  linkaddr_t address_to_contact;
  uint8_t last_custom_seqno;
  struct ctimer ctimer;
};

struct saved_node_to_compute_entry
{
  struct saved_node_to_compute_entry *next;
  linkaddr_t from;
  list_t *saved_data_list;
  bool already_computed;
};

struct saved_data_to_compute_entry
{
  struct saved_data_to_compute_entry *next;
  uint8_t data;
};

struct list_of_list_saved_data_entry
{
  struct list_of_list_saved_data_entry *next;
  list_t *saved_data_list;
};

#endif