// Used by node that need to discover parent node
// Thus: Computation and Sensor node


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

// More robuste connection
static uint8_t number_fail_connection = 0;

// Event
static process_event_t new_data_event;
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
// UTILS
static void 
add_reset_packet_to_queue(const linkaddr_t destination);

static void 
reset_rank();

static void 
register_parent(const linkaddr_t from, const uint8_t getting_rank, const uint16_t current_rssi);
/*---------------------------------------------------------------------------*/


