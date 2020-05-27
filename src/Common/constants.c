#ifndef constants_c
#define constants_c

// Number of retransmission before to achieved that a communication is down
#define MAX_RETRANSMISSIONS 4

// Number of data that are keep in queue (before to be send)
#define NUM_DATA_IN_QUEUE 10

// Maximum number that could ahve SEQNO (seqno is used by valve packet to detect duplicate)
#define NUM_MAX_SEQNO 10

// Delay to broadcast 
// This value is not used identically if node have parent or not:
// IF NO PARENT:
//   min: BROADCAST_DELAY
//   max: 2*BROADCAST_DELAY
// IF PARENT:
//   min: 4*BROADCAST_DELAY
//   max: 8*BROADCAST_DELAY
// This is to avoid to much broadcast but also to discover potential new parent and new road
#define BROADCAST_DELAY 15
// Add delay to reply to broadcast (to avoid collision), between 0 and BROADCAST_REPLY_DELAY
#define BROADCAST_REPLY_DELAY 10

// Delay to try to send data (no message if no data)
#define DATA_MAX_DELAY 30

// Default rank on start (equals to max rank)
#define MAX_RANK 255

// Max size of the network (border node need to have all children)
#define NUM_MAX_CHILDREN 100

// Time before a child need to be removed from a node (down link)
#define CHILDREN_TIMEOUT 150   // A litle bit more than 2 minutes (because data are not always send directly)


///////////// CHANNEL /////////////
// Communication channel
#define BROADCAST_CHANNEL 129          // Broadcast to know parent
#define RUNICAST_CHANNEL_BROADCAST 144 // Channel to reply to broadcast
#define RUNICAST_CHANNEL_DATA 154      // Channel to send data
#define RUNICAST_CHANNEL_VALVE 164     // Channel to receive valve data
///////////////////////////////////////


///////////// SENSOR /////////////
// Time during which the led must be ON
#define ACTIVE_LED_TIMER 600   // 60sec * 10 = 10 minutes

// Delay to measure air quality
#define DATA_COLLECTING_DELAY 60 // 60 sec = 1 min
///////////////////////////////////////


///////////// COMPUTATION /////////////
// Timer to check if data must be compute
#define TIME_TO_FORCE_COMPUTATION_OF_DATA 30  // 30 secondes

// Number of sensor that could be compute by 1 comptutation node
#define NUMBER_SENSOR_IN_COMPUTATION 5  // If modified, do not forget to init new list

// Number of value to collect before to compute least square root
#define NUMBER_OF_DATA_TO_COMPUTE 30

// Threshold value
#define THRESHOLD 0

// Time before to delete a sensor if no data received
#define MAX_DATA_DIFFERENCE 190  // If we don't have any news since more than 3 minute (thus we lost 3 values)
///////////////////////////////////////


#endif