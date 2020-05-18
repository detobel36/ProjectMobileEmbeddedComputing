#ifndef constants_c
#define constants_c

// Number of retransmission before to achieved that a communication is down
#define MAX_RETRANSMISSIONS 4
// Number of data that are keep in queue
#define NUM_DATA_IN_QUEUE 10

// Delay to broadcast 
// (this value is used like: minimum = BROADCAST_DELAY and maximum = 2*BROADCAST_DELAY when 
// rank is not define and otherwise minimum = 4*BROADCAST_DELAY and maximum = 8*BROADCAST_DELAY)
#define BROADCAST_DELAY 15

// Delay to try to send data (no message if no data)
#define DATA_MIN_DELAY 10
#define DATA_MAX_DELAY 30
// Delay to try to send valve (no message if no valve data)
#define VALVE_MIN_DELAY 5
#define VALVE_MAX_DELAY 15
// Delay to measure air quality
#define DATA_COLLECTING_DELAY 60
// Default rank on start (equals to max rank)
#define MAX_RANK 255
// Max size of the network (border node need to have all children)
#define NUM_MAX_CHILDREN 100

// Communication channel
#define BROADCAST_CHANNEL 129          // Broadcast to know parent
#define RUNICAST_CHANNEL_BROADCAST 144 // Channel to reply to broadcast
#define RUNICAST_CHANNEL_DATA 154      // Channel to send data
#define RUNICAST_CHANNEL_VALVE 164     // Channel to receive valve data

#endif