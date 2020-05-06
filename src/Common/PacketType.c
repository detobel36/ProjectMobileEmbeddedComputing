#ifndef packetTypes_h
#define packetTypes_h

//types of packets
enum packet_type 
{
    DISCOVERY_REQ,
    DISCOVERY_RESP
};

struct general_packet 
{
    uint8_t type;
    uint16_t rank;
};

// TODO
// struct data_packet
// {
//     /* data */
// };

#endif