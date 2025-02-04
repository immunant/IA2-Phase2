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

/*
 * These macros cannot be easily rewritten so they would require manual changes.
 * The tests are preprocessed with -DPRE_REWRITER` when running the rewriter and
 * without it when they're compiled afterwards so we use this to define the
 * following macros differently based on how they would be manually rewritten.
 */
#if PRE_REWRITER
#define check_actions(val) (actions.add == val)
#else
#define check_actions(val) (IA2_ADDR(actions.add) == val)
#endif

#if PRE_REWRITER
#define call_add_event(evt) (actions.add)(evt)
#else
#define call_add_event(evt) IA2_CALL(actions.add, _ZTSPFbP5eventE, evt)
#endif

struct event *get_event(void);
void init_actions(void);
