#ifndef d2fd05_NET
#define d2fd05_NET

#include <stddef.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>


struct connection {
	int socket;
	SSL* SSL;
	struct sockaddr addr;
	uint8_t use_tls : 1;
};

ssize_t cwrite(struct connection* connection, void* buffer, size_t n);
ssize_t cread(struct connection* connection, void* buffer, size_t n);

#endif
