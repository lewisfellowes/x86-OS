#include "event.h"

#define EVENT_BUF_SIZE 128

static event_t ring[EVENT_BUF_SIZE];
static volatile uint8_t head, tail;

void event_init(void) {
    head = tail = 0;
}

void event_push(const event_t *ev) {
    uint8_t next = (head + 1) % EVENT_BUF_SIZE;
    if (next == tail) return; /* drop if full */
    ring[head] = *ev;
    head = next;
}

bool event_poll(event_t *ev) {
    if (head == tail) return false;
    *ev = ring[tail];
    tail = (tail + 1) % EVENT_BUF_SIZE;
    return true;
}
