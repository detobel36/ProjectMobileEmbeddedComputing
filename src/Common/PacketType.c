#ifndef packetTypes_h
#define packetTypes_h

//types of packets
enum packet_type 
{
    DISCOVERY_REQ,
    DISCOVERY_RESP,
    PACKET_DATA
};

struct abstract_packet
{
    uint8_t type;
};

struct general_packet 
{
    uint8_t type;
    uint8_t rank;
};

// TODO enhance
struct data_packet
{
    uint8_t type;
    uint8_t data;
    linkaddr_t address;
    bool link; // True = Up link; False = Downlink
};

#endif