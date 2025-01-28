/*
 * Copyright (c) 2017-2019, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file circuitpayment.h
 * \brief Header file for circuit_payment.c.
 **/
 #include "trunnel/circpad_negotiation.h"
#include "lib/evloop/timers.h"


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
typedef int error_t;

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
struct List_of_str_st {
    char msg[MAX_MESSAGE_LEN];
};


typedef struct payment_session_context_st {
    char session_id[PAYMENT_HASH_KEY_LEN];
    char nickname[USER_NAME_LEN];
    uint32_t circuit_id;
    uint64_t channel_global_id;
} payment_session_context_t;

typedef struct payment_info_context_st {
    int delay_payments_counter;
    uint32_t circuit_id;
} payment_info_context_t;

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

typedef struct List_of_str_st List_of_str_t;


typedef struct OR_OP_request_st OR_OP_request_t;


error_t circuit_payment_send_command_to_hop(
        origin_circuit_t *circ,
        uint8_t hopnum,
        uint8_t relay_command,
        const uint8_t *payload,
        ssize_t payload_len);

const node_t* circuit_payment_get_nth_node(origin_circuit_t *circ, int hop);

OR_OP_request_t * payment_payload_new(void);
ssize_t payment_into(OR_OP_request_t *obj, const uint8_t *input, const size_t len_in);
ssize_t circuit_payment_negotiate_parse(OR_OP_request_t **output, const uint8_t *input, const size_t len_in);

error_t circuit_payment_send_command_to_origin(circuit_t *circ, uint8_t relay_command, const uint8_t *payload, ssize_t payload_len);
void circuit_payment__free(OR_OP_request_t *obj);
static void circuit_payment_negotiate_clear(OR_OP_request_t *obj);
ssize_t circuit_payment_negotiate_encode(uint8_t *output, const size_t avail, const OR_OP_request_t *obj);
void divideString(List_of_str_t* output, char *str, int len, int n);
// public API

OR_OP_request_t* circuit_payment_handle_payment_negotiate(const cell_t *cell);
int circuit_get_num_by_nickname(origin_circuit_t * circ, char* nickname);
extend_info_t* circuit_get_extended_data_by_nickname(origin_circuit_t * circ, char* nickname);

int circuit_get_length(origin_circuit_t * circ);
error_t circuit_payment_send_OP(circuit_t *circ, uint8_t target_hopnum, OR_OP_request_t* input);
error_t circuit_payment_send_OR(circuit_t *circ, OR_OP_request_t* input);

void set_to_session_context(const char* session, const char* nickname, uint64_t channel_global_id, uint32_t circuit_id);
payment_session_context_t* get_from_session_context_by_session_id(const char* session);
void remove_from_session_context(payment_session_context_t* element);

void set_circuit_payment_info(uint32_t circuit_id);
payment_info_context_t* get_circuit_payment_info(int circuit_id);
void remove_circuit_payment_info(payment_info_context_t* element);