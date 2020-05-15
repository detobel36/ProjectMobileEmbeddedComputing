#ifndef packetStruct_c
#define packetStruct_c

struct rank_packet 
{
    uint8_t rank;
};

// TODO enhance
struct data_packet
{
    uint8_t data;
    linkaddr_t address;
};

struct data_packet_list
{
    struct data_packet_list *next;
    uint8_t data;
    linkaddr_t address;
};

#endif