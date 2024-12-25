/*
 * Copyright (c) 2017-2021, The ?????? Project */
/* See LICENSE for licensing information */

/**
 * \file circuit_payment.h
 * \brief Header file for circuit_payment.c.
 **/
#ifndef __TOR_PLUS_CIRCUIT_PAYMENT_H_INCLUDED__
#define __TOR_PLUS_CIRCUIT_PAYMENT_H_INCLUDED__

#include "trunnel/circpad_negotiation.h"
#include "lib/evloop/timers.h"
#include <src/core/proto/payment_http_client.h>

#define PAYMENT_MSG_VERSION 0

struct circuit_t;
struct origin_circuit_t;
struct cell_t;
#define MAX_EXIT_MESSAGES 107000
#define MAX_RELAY_MESSAGES 107000
#define USER_NAME_LEN 50
#define COMMAND_ID_LEN 40
#define SESSION_ID_LEN 40
#define MAX_MESSAGE_LEN 301
#define MAX_REAL_MESSAGE_LEN 20000

#define PAYMENT_NODE_ID 300
#define PAYMENT_HASH_KEY_LEN 150
#define PAYMENT_URL_LEN 1000
typedef int32_t error_t;

#define tp_zero_mem(buf, len) memset((buf), 0, (len))

typedef struct payment_chunks_st {
    char key[PAYMENT_HASH_KEY_LEN];
    char value[MAX_REAL_MESSAGE_LEN];
} payment_chunks_t;

struct OR_OP_request_st {
    uint8_t version;
    uint8_t message_type;
    uint8_t command;
    uint8_t is_last;
    int16_t command_type;
    uint16_t session_id_length;
    char session_id[SESSION_ID_LEN];
    uint16_t command_id_length;
    char command_id[COMMAND_ID_LEN];
    uint16_t nicknameLength;
    char nickname[USER_NAME_LEN];
    uint16_t messageLength;
    char message[MAX_MESSAGE_LEN];
    uint16_t messageTotalLength;
};

typedef struct OR_OP_request_st OR_OP_request_t;

typedef struct payment_message_for_sending_st {
    char sessionId[SESSION_ID_LEN + 1];
    char nodeId[USER_NAME_LEN + 1];
    OR_OP_request_t * message;
} payment_message_for_sending_t;

typedef struct payment_session_context_st {
    char session_id[SESSION_ID_LEN];
    char nickname[USER_NAME_LEN];
    uint32_t circuit_id;
    uint64_t channel_global_id;
    time_t timestamp_created;
} payment_session_context_t;

typedef struct payment_chunk_st {
    char nickname[USER_NAME_LEN];
    char merged_string[MAX_REAL_MESSAGE_LEN];
    int merged_string_len;
}payment_chunk_t;

typedef struct list_of_chunks_st {
    int circuit_id;
    payment_chunk_t chunk[3];
}list_of_chunks_t;

typedef struct chunk_payment_st {
    int len;
    list_of_chunks_t list[50];
}chunk_payment_t;

// public API
void tp_init_lists(void);
void tp_scan_sessions(void);
void tp_init(void);
void tp_deinit(void);
void tp_fill_stellar_address(char *dst);
int is_invalid_stellar_address(const char *addr);

ssize_t circuit_payment_negotiate_parse(OR_OP_request_t **output, const uint8_t *input, const size_t len_in);
ssize_t circuit_payment_negotiate_encode(uint8_t *output, const size_t avail, const OR_OP_request_t *obj);

void tp_store_session_context(const char* session, const char* nickname, uint64_t channel_global_id, uint32_t circuit_id);
payment_session_context_t* get_from_session_context_by_session_id(const char* session);
void remove_from_session_context(payment_session_context_t* element);
void tp_free_session_context(uint64_t channel_global_id, circid_t circ_id);

int tp_process_payment_cell_async(const cell_t *cell, circuit_t *circ);
void tp_send_payment_request_to_client_async(circuit_t *circ, int message_number);
int tp_process_payment_command_cell_to_node_async(const cell_t *cell, circuit_t *circ);

#endif //__TOR_PLUS_CIRCUIT_PAYMENT_H_INCLUDED__
