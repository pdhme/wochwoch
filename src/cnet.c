#include <cnet.h>

ssize_t cread(struct connection* connection, void* buffer, size_t n) {
	if (connection->use_tls)
		return (ssize_t) SSL_read(connection->SSL, buffer, (int)n);
	else
		return read(connection->socket, buffer, n);
}

ssize_t cwrite(struct connection* connection, void* buffer, size_t n) {
	if (connection->use_tls)
		return (ssize_t) SSL_write(connection->SSL, buffer, (int)n);
	else
		return write(connection->socket, buffer, n);
}
