#ifndef packetStruct_c
#define packetStruct_c

//// PACKET ////

struct rank_packet 
{
    uint8_t rank;
};

struct data_packet
{
    uint8_t data;
    linkaddr_t address;
};

// Valve packet always tell that the valve need to be open
struct valve_packet
{
    linkaddr_t address;
};


//// LIST ////

struct data_packet_entry
{
    struct data_packet_entry *next;
    uint8_t data;
    linkaddr_t address;
};

struct children_entry
{
    struct children_entry *next;
    linkaddr_t address_destination;
    linkaddr_t address_to_contact;
};


#endif