#pragma once
#include <stdbool.h>

struct event;

struct event_actions {
    bool (*add)(struct event *evt);
    bool (*del)(struct event *evt);
    void (*enable)(struct event *evt);
    void (*disable)(struct event *evt);
};

extern struct event_actions actions;

#define add_event actions.add
#define del_event actions.del
#define enable_event actions.enable
#define disable_event actions.disable

struct event *get_event();
void init_actions();
