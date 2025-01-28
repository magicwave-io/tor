//
// Created by root on 6/29/20.
//

#ifndef TOR_PLUS_RATE_LIMITER_H
#define TOR_PLUS_RATE_LIMITER_H

#endif //TOR_PLUS_RATE_LIMITER_H
#include <time.h>

typedef struct rate_limiter_t {
    int actions_in_period;
    int max_actions_in_period;
    time_t last_action;
    int min_time_between_actions_milisec;
} rate_limiter_t;

void init_limiter();
void consume();