#include "eventlist.h"
#include <stdlib.h>
#include <stdio.h>

struct EventList* create_list() {
  struct EventList* list = (struct EventList*)malloc(sizeof(struct EventList));
  if (!list) return NULL;
  list->head = NULL;
  list->tail = NULL;
  return list;
}

int append_to_list(struct EventList* list, struct Event* event) {
  if (!list) return 1;

  struct ListNode* new_node = (struct ListNode*)malloc(sizeof(struct ListNode));
  if (!new_node) return 1;

  new_node->event = event;
  new_node->next = NULL;

  if (list->head == NULL) {
    list->head = new_node;
    list->tail = new_node;
  } else {
    list->tail->next = new_node;
    list->tail = new_node;
  }

  return 0;
}

static void free_event(struct Event* event) {
  if (!event) return;

  free(event->data);
  free(event);
}

void free_list(struct EventList* list) {
  printf("1/n");
  if (!list) return;

  struct ListNode* current = list->head;
  printf("2");
  while (current) {
    printf("3");
    struct ListNode* temp = current;
    current = current->next;
    printf("4");
    free_event(temp->event);
    printf("5");
    free(temp);
    printf("6");
  }

  free(list);
  printf("7");
}

struct Event* get_event(struct EventList* list, unsigned int event_id) {
  if (!list) return NULL;

  struct ListNode* current = list->head;
  while (current) {
    struct Event* event = current->event;
    if (event->id == event_id) {
      return event;
    }
    current = current->next;
  }

  return NULL;
}
