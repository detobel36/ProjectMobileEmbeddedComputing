#ifndef packetTypes_h
#define packetTypes_h

struct abstract_packet
{
    uint8_t type;
};

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

#endif