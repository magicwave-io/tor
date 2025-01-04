//
// Created by root on 6/29/20.
//
#include "trunnel-impl.h"
#include "rate_limiter.h"
#include <time.h>
#include <pthread.h>
#include <stdio.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#include <src/lib/malloc/malloc.h>

#endif

pthread_mutex_t lock;

struct rate_limiter_t *rateLimiter;

void init_limiter(){
    rateLimiter = tor_malloc_(sizeof(rate_limiter_t));

    rateLimiter->last_action = time(0);
    rateLimiter->min_time_between_actions_milisec = 10;

}

void consume() {
        long curTime = time(0);
        long timeLeft;

        //calculate when can we do the action
     pthread_mutex_lock(&lock);

     timeLeft = rateLimiter->last_action + rateLimiter->min_time_between_actions_milisec - curTime;
            if(timeLeft > 0) {
                rateLimiter->last_action  += rateLimiter->min_time_between_actions_milisec;
            }
            else {
                rateLimiter->last_action = curTime;
            }
     pthread_mutex_unlock(&lock);

        //If needed, wait for our time
        if(timeLeft <= 0) {
            return;
        }
        else {
            usleep(timeLeft*1000);
        }
}
