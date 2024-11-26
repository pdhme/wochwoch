#ifndef cb16df_HTTP_SERVER
#define cb16df_HTTP_SERVER




#include <cnet.h>
#include <hash-table.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <magic.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>


#define HTTP_SEND_FILE 32768
#define HTTP_REQUEST_HANDLER_ARGUMENTS http_rh_info* info,http_request* request,http_responce* responce
#define HTTP_PASS_REQUEST_HANDLER_ARGUMENTS info,request,responce
#define BUFFER_SIZE 10240
#define HEADER_HT_SIZE 1000
#define HTTP_SERVER_USE_SSL_TLS 0b00000001
#define HTTP_SERVER_SET_SERVER_HEADER 0b00000010
#define HTTP_SERVER_SET_CONTENT_LENGTH_HEADER 0b00000100

#define COLOR_RESET "\e[0m"
#define COLOR_ERR "\e[31m"
#define COLOR_SUCCESS "\e[32m"
#define COLOR_ACTION "\e[33m"
#define COLOR_PREFIX "\e[37m"



#ifndef f854aa_HTTP
#define f854aa_HTTP
typedef struct http_request {
	char* method;
	char* url;
	char* suburl;
	char* http_version;
	hash_table* headers;
	size_t ContentLength;
	char* data;
} http_request;

typedef struct http_responce {
	char* http_version;
	int status;
	char* status_str;
	hash_table* headers;
	size_t ContentLength;
	char* data;
} http_responce;

typedef struct __processor_args {
	struct connection client;
	hash_table* request_handlers;
	int serverID;
	sem_t* threads;
//	pthread_mutex_t* mp;		// mutex pointer
} http_request_handler_info;
typedef http_request_handler_info http_rh_info;

enum http_request_action {
	CONTINUE,
	SKIP,
	REJECT,
};
typedef short (*http_request_handler)(http_rh_info*,http_request*,http_responce*);
typedef enum http_request_action (*http_request_pre_handler)(http_rh_info*,http_request*,http_responce*);
#endif

typedef struct http_server {
	int id;
	unsigned int threads;
	pthread_mutex_t mutex;

	char* host;
	char* port;
	int socket;
	int flags;
	hash_table* request_handlers;

	struct {
		uint8_t multi_thread : 1;
		uint8_t use_tls : 1;
		uint8_t no_server_header : 1;
		uint8_t no_color : 1;
	};
	char* SSL_Certificate;
	char* SSL_PrivateKey;
} http_server;

void http_request_free(http_request* request);
void http_responce_free(http_responce* responce);

char* load_file(char* path, size_t* returnSize);
char* Content_Type(char* path);
short webpage(http_rh_info* info, http_request* request, http_responce* responce, char* root_dir);
short http_respond(HTTP_REQUEST_HANDLER_ARGUMENTS, char* root_dir);
static void log_request(int id, struct sockaddr* addr, http_request* rq, short status);
static char* status_str(short code);
void send_responce(http_rh_info* info, http_request* request, http_responce* responce);
int http_server_run(http_server* srv);
short status_code_page(short code, HTTP_REQUEST_HANDLER_ARGUMENTS);
static short make_status_page(short code, http_responce* responce);



#endif
