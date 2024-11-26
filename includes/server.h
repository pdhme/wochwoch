#ifndef de8cd5_SERV
#define de8cd5_SERV

#include <hash-table.h>
#include <http.h>
#include "../config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uuid/uuid.h>

#define FETCH_BUFFER_SIZE 512
#define ENABLE
#define GET_INPUT_FROM_POST_DATA

struct answer {
	char* str;
	size_t str_len;
	uint16_t votes;
	char** voters;
};

struct question {
	uint8_t typeTF : 1;
	uint8_t open : 1;
	uint32_t total_votes;
	uint16_t na;
	struct answer* answers;
	uint8_t correct_answer;
	uint8_t show_correct_after_answer : 1;
	uint8_t show_correct_to_all : 1;
};

struct section {
	struct question* questions;
	uint16_t nq;
};

struct user {
	char* name;
};

typedef struct linked_data {
	char* string;
	size_t len;
	struct linked_data* next;
} linked_data;


#endif
