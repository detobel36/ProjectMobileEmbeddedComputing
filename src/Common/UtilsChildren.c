#ifndef utils_children_c
#define utils_children_c

#include <stdbool.h>

#include "../Common/Constants.c"
#include "../Common/PacketStruct.c"


MEMB(children_mem, struct children_entry, NUM_MAX_CHILDREN);
LIST(children_list);

static struct children_entry* get_child_entry(const uint8_t u8_0, const uint8_t u8_1) {
  struct children_entry *child;
  for(child = list_head(children_list); child != NULL; child = list_item_next(child)) {
    if(child->address_destination.u8[0] == u8_0 && child->address_destination.u8[1] == u8_1) {
      break;
    }
  }
  return child;
}

static struct children_entry* get_child_entry_address(const linkaddr_t *destination_addr) {
  return get_child_entry(destination_addr->u8[0], destination_addr->u8[1]);
}

static void remove_children(void *child_entry_ptr) {
  struct children_entry *child_entry = child_entry_ptr;
  printf("[INFO - %s] Remove children %d.%d: no recent message\n", NODE_TYPE, 
    child_entry->address_destination.u8[0], 
    child_entry->address_destination.u8[1]);

  list_remove(children_list, child_entry);
  memb_free(&children_mem, child_entry);
}

static void remove_all_children_linked_to_address(const linkaddr_t *destination_addr) {
  struct children_entry *child;
  for(child = list_head(children_list); child != NULL; child = list_item_next(child)) {
    if(linkaddr_cmp(&child->address_destination, destination_addr)) {
      list_remove(children_list, child);
      memb_free(&children_mem, child);
    }
  }
}

static bool 
create_child_or_udpate_and_detect_duplicate(struct children_entry *child, 
  const linkaddr_t *from, const uint8_t custom_seqno, const linkaddr_t source_addr, 
  const uint8_t data) {
  if(child == NULL) {
    struct children_entry *child_entry = memb_alloc(&children_mem);
    child_entry->address_to_contact = *from;
    child_entry->address_destination = source_addr;
    child_entry->last_custom_seqno = custom_seqno;
    list_add(children_list, child_entry);
    ctimer_set(&child_entry->ctimer, CHILDREN_TIMEOUT * CLOCK_SECOND, remove_children, child_entry);
    printf("[DEBUG - %s] Save new child %d.%d (using node %d.%d)\n", 
      NODE_TYPE, source_addr.u8[0], source_addr.u8[1],
      from->u8[0], from->u8[1]);

  } else {

      if(child->last_custom_seqno == custom_seqno) {
        printf("[WARN - %s] Detect duplicate data from %d.%d source %d.%d (data: %d)\n", 
          NODE_TYPE, 
          child->address_to_contact.u8[0], child->address_to_contact.u8[1], 
          child->address_destination.u8[0], child->address_destination.u8[1],
          data);

        packetbuf_clear();
        return true;
      }

      // If saved contact address equals the from packet
      // It means it is a direct connection.
      // if(linkaddr_cmp(&child->address_to_contacct, from)) {
      // }

      child->address_to_contact = *from;
      child->last_custom_seqno = custom_seqno;
      ctimer_set(&child->ctimer, CHILDREN_TIMEOUT * CLOCK_SECOND, remove_children, child);
  }

  return false;
}


#endif