#include <http.h>
#define sfree(p) {free(p);p=NULL;}

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void http_request_free(http_request* request) {
	hash_table_free(request->headers);
	request->headers = NULL;
	sfree(request->http_version);
	sfree(request->data);
	sfree(request->method);
	sfree(request->suburl);
	sfree(request->url);
}
void http_responce_free(http_responce* responce) {
	hash_table_free(responce->headers);
	responce->headers = NULL;
	sfree(responce->status_str);
	sfree(responce->http_version);
	sfree(responce->data);
}


//
//	Front-end functions
//

char* load_file(char* path, size_t* returnSize) {
	FILE* f = fopen(path, "r");
	if (f == NULL) goto OPEN_err;
	if (fseek(f, 0, SEEK_END) < 0) goto SIZE_err;
	long size = ftell(f);
	if (size < 0) goto SIZE_err;
	if (fseek(f, 0, SEEK_SET) < 0) goto SIZE_err;
	char* mem = malloc(size);
	if (mem == NULL) goto BUFFER_ALLOC_err;
	if (fread(mem, 1, size, f) < size) goto READ_err;
	fclose(f);
	*returnSize = size;
	return mem;

	// Simple error handling
	READ_err: ;
	free(mem);
	BUFFER_ALLOC_err: ;
	SIZE_err: ;
	fclose(f);
	OPEN_err: ;
	fprintf(stderr, COLOR_PREFIX "(load_file) " COLOR_ERR "Failed to load file \"%s\" into memory: %s" COLOR_RESET "\n", path, strerror(errno));
	*returnSize = 0;
	return NULL;
}

static hash_table* ContentType_ht = NULL;

char* Content_Type(char* path) {
	char* ContentType = NULL;
	char *dot = strrchr(path, '.');
	if ( (ContentType = ht_get(ContentType_ht, dot)) == NULL) return strdup("application/octet-stream");
	return strdup(ContentType);
}

/*
char* Content_Type(char* path) {
	char* type;
	magic_t magic;
	if ( !(magic = magic_open(MAGIC_MIME_TYPE)) ) goto open_err;
//	if (magic_compile(magic, path)) goto err;
	if (magic_load(magic, NULL)) goto err;
	if ( !(type = magic_file(magic, path))) goto err;
	type = strdup(type);
	magic_close(magic);
	return type;
	err:
	magic_close(magic);
	open_err:
	return strdup("application/octet-stream");
}
*/

short webpage(http_rh_info* info, http_request* request, http_responce* responce, char* root_dir) {
	unsigned int err = 0;
	if (request == NULL) return 1;
	if (root_dir == NULL) return 2;
	if (responce == NULL) return 3;
	if (info == NULL) return 4;
	if (info->request_handlers == NULL) return 5;
	if (strlen(request->suburl) > 4096) return 414;
	char* ext;
	char* ct;
	size_t file_size;
	char* file_name = request->suburl ? request->suburl : "";
	size_t filen_l = strlen(file_name);
	size_t root_l = strlen(root_dir);
	size_t plen = root_l + filen_l; 
	int8_t slash = 0;
	if (file_name[0] == '/')
		if (root_dir[root_l - 1] == '/') {
			root_dir[root_l - 1] = 0;
			plen--;
		} else ;
	else if (root_dir[root_l - 1] != '/')
		slash = 1;
	plen += slash;
	char path[plen + 1];
	char* mfile = NULL;
	char* file = path;
	if (sprintf(path, "%s%s%s", root_dir, slash == 0 ? "" : "/", file_name) < 0) goto p1_err;
	struct stat finfo;
	if (stat(path, &finfo) == -1)
		switch (errno) {
			case ENOENT: return 404;
			case EFAULT: return 400;
			case EACCES: return 423;
		}
	if (S_ISDIR(finfo.st_mode)) {
		if ( (mfile = ( file = malloc(plen + 11) )) == NULL) goto malloc_err;
		if (sprintf(mfile, "%s%s%s", path, (path[plen - 1] == '/') ? "" : "/", "index.html") < 0) goto p2_err;
	} else if (!S_ISREG(finfo.st_mode)) return 404;

	if ( (responce->data = load_file(file , &file_size)) == NULL)
		return 404;

	if ( (ext = Content_Type(file)) == NULL) goto ext_err;
	if (!(ct = realloc(ext, (size_t) strlen(ext) + strlen("; charset=utf-8") + 1))) goto ext_realloc_err;
	strcpy(ct + strlen(ct), "; charset=utf-8");
	ht_set_heap(responce->headers, "Content-Type", ct);
	responce->ContentLength = file_size;
	responce->status = 200;

	sfree(mfile);
	return 0;

	ext_realloc_err: err++;
	ext_err: err++;
	sfree(responce->data);
	p2_err: err++;
	sfree(mfile);
	malloc_err: err++;
	p1_err: err+=6;
	char* msg[] = {
		// TODO
	};
	return err;
}

pthread_mutex_t echm = PTHREAD_MUTEX_INITIALIZER;
static http_request_handler error_code_handlers[63] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
// get global http error status code handler
static http_request_handler get_ghesch(short code) {
	if (code < 100 || code > 511) {
		puts("get_ghesch: Invalid status code");
		return NULL;
	}
	pthread_mutex_lock(&echm);
	http_request_handler rh = error_code_handlers[code % 63];
	pthread_mutex_unlock(&echm);
	return rh;
}
short set_ghesch(short code, http_request_handler rh) {
	if (code < 100 || code > 511) {
		puts("set_ghesch: Invalid status code");
		return 1;
	}
	pthread_mutex_lock(&echm);
	error_code_handlers[code % 63] = rh;
	pthread_mutex_unlock(&echm);
	return 0;
}

short http_respond(HTTP_REQUEST_HANDLER_ARGUMENTS, char* root_dir) {
	short code = webpage(HTTP_PASS_REQUEST_HANDLER_ARGUMENTS, root_dir);
	http_request_handler handler = NULL;
	switch (code) {
		case 0:
			send_responce(HTTP_PASS_REQUEST_HANDLER_ARGUMENTS);
			return 0;
		case 100 ... 511:
			break;
		default:
			code = 500;
	}
	if ( !(handler = get_ghesch(code)))
		status_code_page(code, HTTP_PASS_REQUEST_HANDLER_ARGUMENTS);
	else
		handler(HTTP_PASS_REQUEST_HANDLER_ARGUMENTS);
	send_responce(HTTP_PASS_REQUEST_HANDLER_ARGUMENTS);
	http_responce_free(responce);
	return 0;
}






static char exists(char* file) {
	struct stat buffer;
	return (stat (file, &buffer) == 0) ? 1 : 0;
}

static short fail(int err, int id) {

	char* template[] = {
		"Mutable lock failed",										// 0
		"Failed to allocate memory for server structure pointer",	// 1
		"Too many threads",											// 2
		"Failed to create new socket file descriptor",				// 3
		"Failed to find host",										// 4
		"Failed to bind socket",									// 5
		"Failed to create new SSL_CTX structure",					// 6
		"Failed to load SSL private key",							// 7
		"Failed to load SSL certificate",							// 8
		"Failed to listen on socket",								// 9
		"Failed to set up sigaction",								// 10
	};

	fprintf(stderr, "(http_server:%d) " COLOR_ERR "%s: %s" COLOR_RESET "\n", id, template[err-1], strerror(errno));
	return err;
}

static void request_err(int err, int id) {
	char* template[] = {
		"Failed to allocate memory for argument structure",		// 0
		"Failed to create new SSL structure",					// 1
		"Failed to accept new connection",						// 2
		"Failed to set SSL file descriptor",					// 3
		"TLS/SSL handshake failed",								// 4
		"Fatal TLS/SSL handshake error",						// 5
		"Failed to initialise thread attributes object",		// 6
		"Failed to detach thread",								// 7
		"Insufficient resources to create another thread",		// 8
		"Failed to allocate memory for request structure",		// 9
		"Read error",											// 10
		"Failed to initialise the request header table",				// 11
		"Failed to initialise the responce header table",				// 11
		"Failed to allocate memory for request buffer",			// 12
	};

	fprintf(stderr, "\e[37m" "(http_server:%d) " COLOR_ERR "%s: %s" COLOR_RESET "\n", id, template[err], strerror(errno));
}

static void printmsg(char* msg, char type, int id) {
	char* color;
	switch(type) {
		case 's': color = COLOR_SUCCESS; break;
		case 'e': color = COLOR_ERR; break;
		case 'a': color = COLOR_ACTION; break;
		case 'n': color = COLOR_RESET; break;
		default: color = COLOR_RESET; break;
	}
	printf("%s(http_server:%d) %s%s" COLOR_RESET "\n", type == 'n' ? "" : COLOR_PREFIX, id, color, msg);
}

static void log_request(int id, struct sockaddr* addr, http_request* rq, short status) {
	struct sockaddr_in* addr_p = (struct sockaddr_in*) addr;
	char* status_color = NULL;
	switch (status) {
		case 100 ... 199:
			status_color = "\e[36;1m";	break;
		case 200 ... 299:
			status_color = "\e[32;1m";	break;
		case 300 ... 399:
			status_color = "\e[1m";		break;
		case 400 ... 499:
			status_color = "\e[33;1m";	break;
		case 500 ... 599:
		default:
			status_color = "\e[31;1m";
	}
	printf(COLOR_PREFIX "(http_server:%d)" COLOR_RESET " %s --> [%s %s %s] --> %s%d" COLOR_RESET "\n", id, inet_ntoa(addr_p->sin_addr), rq->method, rq->url, rq->http_version, status_color, status);
}

static http_server** http_servers = NULL;
static int http_server_amount = 0;
static int SR = 0;
static char RUN = 1;
static void quit() {
	RUN = 0;
	puts("\n" COLOR_PREFIX "(http_server) " COLOR_ACTION "Letting servers finish processing requests..." COLOR_RESET);
	for (int i = 0; i < http_server_amount; i++)
		if (http_servers[i] != NULL)
			if ( shutdown(http_servers[i]->socket, SHUT_RDWR) == -1) perror("(http_server) Failed to shutdown socket");
}

static void sigpipe() {
	puts("\n" COLOR_PREFIX "(http_server) " COLOR_ERR "[Broken pipe]" COLOR_RESET "\n");
}


static inline /* __attribute__((always_inline)) */ int int_str_len(unsigned int n) {
	int s = 1;
	for (int i = 10; n/i > 0; i*=10) s++;
	return s;
}

static char* strnrchr(char* s, char c, int l) {
	char str[l+1];
	strncpy(str, s, l);
	str[l] = 0;
	return s + (strrchr(str, c) - str);
}




//
//	HTTP SERVER
//



static char* status_str(short code) {
	char* msg[] = {
		[37] = "Continue",
		[38] = "Switching Protocols",
		[39] = "Processing",
		[40] = "Early Hints",
		[11] = "OK",
		[12] = "Created",
		[13] = "Accepted",
		[14] = "Non-Authoritative Information",
		[15] = "No Content",
		[16] = "Reset Content",
		[17] = "Partial Content",
		[18] = "Multi-Status",
		[19] = "Already Reported",
		[37] = "IM Used",
		[48] = "Multiple Choices",
		[49] = "Moved Permanently",
		[50] = "Found",
		[51] = "See Other",
		[52] = "Not Modified",
		[53] = "Use Proxy",
		[54] = "unused",
		[55] = "Temporary Redirect",
		[56] = "Permanent Redirect",
		[22] = "Bad Request",
		[23] = "Unauthorized",
		[24] = "Payment Required",
		[25] = "Forbidden",
		[26] = "Not Found",
		[27] = "Method Not Allowed",
		[28] = "Not Acceptable",
		[29] = "Proxy Authentication Required",
		[30] = "Request Timeout",
		[31] = "Conflict",
		[32] = "Gone",
		[33] = "Length Required",
		[34] = "Precondition Failed",
		[35] = "Payload Too Large",
		[36] = "URI Too Long",
		[37] = "Unsupported Media Type",
		[38] = "Range Not Satisfiable",
		[39] = "Expectation Failed",
		[40] = "I'm a teapot",
		[43] = "Misdirected Request",
		[44] = "Unprocessable Content",
		[45] = "Locked",
		[46] = "Failed Dependency",
		[47] = "Too Early",
		[48] = "Upgrade Required",
		[50] = "Precondition Required",
		[51] = "Too Many Requests",
		[53] = "Request Header Fields Too Large",
		[10] = "Unavailable For Legal Reasons",
		[59] = "Internal Server Error",
		[60] = "Not Implemented",
		[61] = "Bad Gateway",
		[62] = "Service Unavailable",
		[0] = "Gateway Timeout",
		[1] = "HTTP Version Not Supported",
		[2] = "Variant Also Negotiates",
		[3] = "Insufficient Storage",
		[4] = "Loop Detected",
		[6] = "Not Extended",
		[7] = "Network Authentication Required"
	};
	return strdup(msg[code % 63]);
}





///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

void send_responce(http_rh_info* info, http_request* request, http_responce* responce) {

	/*if (handler_status != 0)*/
	/*	status_code_page((handler_status < 100) ? 500 : handler_status, info,request,responce);*/


	//
	// PHASE: Processing responce
	//

	if ( responce->http_version == NULL ) responce->http_version = strdup(request->http_version);
	if ( responce->status < 100 || 511 < responce->status ) goto BAD_RESPONCE;
	if ( responce->status_str == NULL )
		if ( (responce->status_str = status_str(responce->status)) == NULL)
			printmsg("Failed to get HTTP status string", 'e', info->serverID);

	if ( responce->data != NULL )
		if ( ht_get(responce->headers, "Content-Length")) 
			responce->ContentLength = atol(ht_get(responce->headers, "Content-Length"));
		else
			if ( responce->ContentLength > 0 ) {
				char ContentLength_str[int_str_len(responce->ContentLength) + 1];
				sprintf(ContentLength_str, "%zu", responce->ContentLength);
				ht_set_str(responce->headers, "Content-Length", ContentLength_str);
			}

	char status_code[4];
	if (sprintf(status_code, "%d", responce->status) < 0)
		status_code_page(500, info,request,responce);

	//
	// PHASE: Sending responce
	//

	// First line
	char* protocol[] = {
		responce->http_version,									" ",
		status_code,											" ",
		responce->status_str ? responce->status_str : "error",	"\r\n", 0
	};

	for (short i = 0; protocol[i] != NULL; i++)
		if ( cwrite(&info->client, protocol[i], strlen(protocol[i])) < strlen(protocol[i]) ) ; // TODO

	// Headers
	hash_table* ht = responce->headers;
	for (int i = 0; i < ht->size; i++) {
		htel* h = ht->e[i];
		if (h != NULL && h != &DELETED_ITEM) {
			cwrite(&info->client, h->key, strlen(h->key));				cwrite(&info->client, ": ", 2);
			cwrite(&info->client, h->value, strlen(h->value));			cwrite(&info->client, "\r\n", 2);
		}
	}
	cwrite(&info->client, "\r\n", 2);

	// Body
	if ( responce->data != NULL )
		if ( cwrite(&info->client, responce->data, responce->ContentLength) < responce->ContentLength)
			ERR_print_errors_fp(stderr);

	return;

	BAD_RESPONCE:
		status_code_page(500, info,request,responce);
		send_responce(info,request,responce);
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

static void* serve_client(void* arg) {

	char failed = 1;
	int err = 0;
	http_request_handler_info* info = (http_request_handler_info*) arg;

	char buffer[BUFFER_SIZE];
	char* rq = NULL;
	size_t br = 0;			// bytes read
	size_t tbr = 0;			// total bytes read

	http_request request;
	memset(&request, 0, sizeof(http_request));
	hash_table* headers = hash_table_init(HEADER_HT_SIZE);
	if ( !headers ) goto HEADER_HT_INIT_err;
	request.headers = headers;

	const char* brs = "\r\n";		// break string
	int hsp = 0;					// header start position
	int line = 0;
	char* temp = NULL;
	long long ContentLength = -1;
	hash_table* request_handlers = info->request_handlers;
	http_request_handler request_handler = NULL;
	http_request_handler pre_handler = NULL;

	short handler_status = 0;
	http_responce responce;
	memset(&responce, 0, sizeof responce);
	if ( !(responce.headers = hash_table_init(HEADER_HT_SIZE))) goto RESP_HEADER_HT_INIT_err;

	//
	// PHASE: loading into memory and parsing request headers
	//
	do {

		if ( (br = cread(&info->client, buffer, BUFFER_SIZE)) < 1 ) goto READ_err;

		if (!tbr)
			if (*buffer == 0x16)
				if (!info->client.use_tls)
					goto READ_err;


		char* temp_str = NULL;
		if ( (temp_str = (char*)realloc(rq, tbr + br + 1)) == NULL ) goto REQUEST_BUFFER_REALLOC_err;
		rq = temp_str;

		memcpy(rq + tbr, buffer, br);

		while ( (temp = strstr(rq + hsp, brs)) != NULL) {
			// b *(http_server_run+1194)
			if ( temp - rq == hsp ) goto header_end;
			if ( line == 0 ) {

				//
				// PHASE: Parcing first line
				//

				// Method
				char* le = temp;				// line end
				if ( (temp = strchr(rq, ' ')) == NULL) goto BAD_REQUEST;
				if ( temp >= le )  goto BAD_REQUEST;
				int cl = temp - rq;				// current length
				if ( (temp_str = malloc(cl + 1)) == NULL) goto BAD_REQUEST;

				memcpy(temp_str, rq, cl);
				temp_str[cl] = 0;
				request.method = temp_str;
				int ns = cl + 1;				// next start

				// URL
				if ( (temp = strchr(rq + ns, ' ')) == NULL) goto BAD_REQUEST;
				if ( temp >= le )  goto BAD_REQUEST;
				cl = temp - rq - ns;			// current length
				if ( (temp_str = malloc(cl + 1)) == NULL) goto BAD_REQUEST;

				memcpy(temp_str, rq + ns, cl);
				temp_str[cl] = 0;
				request.url = temp_str;
				ns = ns + cl + 1;				// next start

				// HTTP version
				int lep = le - rq;				// line end position
				cl = lep - ns;					// current length
				if ( (temp_str = malloc(cl + 1)) == NULL) goto BAD_REQUEST;

				memcpy(temp_str, rq + ns, cl);
				temp_str[cl] = 0;
				request.http_version = temp_str;

				line++;
				hsp = lep + 2;

				/*printf("\e[1m%s %s %s\e[0m\n", request->method, request.url, request->http_version);*/

			} else {
				
				//
				// PHASE: Parcing headers
				//

				int he = temp - rq;					// header end
				int c = 0;							// colon
				if ( (temp = strchr(rq + hsp, ':')) == NULL)  goto BAD_REQUEST;
				if ( (c = temp - rq) >= he )  goto BAD_REQUEST;
				
				int hnl = c - hsp;					// header name length
				int hvl = he - c - 2;				// header value length
				char hn[hnl + 1];					// header name
				char hv[hvl + 1];					// header value

				memcpy(hn, rq + hsp, hnl);
				memcpy(hv, rq + c + 2, hvl);
				hn[hnl] = 0; hv[hvl] = 0;

				if ( ht_set_str(headers, hn, hv) != 0 )
					printf(COLOR_PREFIX "(http_server:%d) " COLOR_ERR "Failed to set header. Header lost:" COLOR_RESET "\n\t\e[1m%s: " COLOR_RESET "%s\n", info->serverID, hn, hv);

				if ( strcmp(hn, "Content-Length") == 0 )
					ContentLength = atol(hv);

				hsp = he + 2;

			}
		}


		tbr += br;

		continue;
		header_end:
		tbr += br;
		break;
	} while ( br > 0 );


	//
	// PHASE: Loading data into memory
	//
	if ( ContentLength < 1 ) goto skip_body;

	if ( (request.data = malloc(ContentLength + 1)) == NULL ) ;
	size_t ad = tbr - hsp - 2;			// available data
	size_t md = ContentLength - ad;		// missing data
	strncpy(request.data, rq+hsp+2, ad);
	if ( cread(&info->client, request.data+ad, md) < md ) goto BAD_REQUEST;
	request.data[ContentLength] = 0;
	//if ( read(srv->socket, request->data, ContentLength) < ContentLength ) ; // TODO

	skip_body:

	enum http_request_action action = CONTINUE;
	if ( (pre_handler = ht_get(request_handlers, "pre")) != NULL)
		action = pre_handler(info,&request,&responce);

	switch (action) {
		case CONTINUE:
			break;
		case SKIP:
			goto exit;
	}

	if (request.url[0] != '/') request.url = "Bad Request";
	int url_len = strlen(request.url);
	if ( (request_handler = ht_get(request_handlers, request.url)) == NULL )
		if (strlen(request.url) > 1)
			if ( request.url[url_len - 1] == '/' ) 
				request_handler = ht_nget(request_handlers, request.url, url_len - 1);


	if (request_handler == NULL)
		while (1) {
			char* url = strnrchr(request.url, '/', url_len);
			url_len = url - request.url;

			char str[url_len + 3];
			memcpy(str, request.url, url_len + 1);
			str[url_len + 1] = ' ';
			str[url_len + 2] = 0;
			request.suburl = strdup(url+1);
			if ( (request_handler = ht_get(request_handlers, str)) != NULL ) break;

			if (url_len < 1) {
				if ( status_code_page(404, info,&request,&responce) == 0) send_responce(info,&request,&responce);
				goto exit;
			}
		}



	if (request_handler)
		handler_status = request_handler(info, &request, &responce);
	else
		handler_status = 404;
	switch (handler_status) {
		case 0: break;
		case 100 ... 511:
			status_code_page(handler_status, info,&request,&responce);
			send_responce(info,&request,&responce);
			break;
		default:
			printf(COLOR_PREFIX "(http_server:%d) " COLOR_ERR "request handler error (code: %hd)" COLOR_RESET "\n", info->serverID, handler_status);
			status_code_page(500, info,&request,&responce);
			send_responce(info,&request,&responce);
			break;
	}

	log_request(info->serverID, &info->client.addr, &request, responce.status);

	exit:
	
	failed = 0;

	REQUEST_BUFFER_REALLOC_err: err++;
	hash_table_free(responce.headers);
	RESP_HEADER_HT_INIT_err: err++;
	http_request_free(&request);
	HEADER_HT_INIT_err: err++;
	READ_err: err++;
	free(rq);
	REQUEST_BUFFER_ALLOC_err: err += 9;
	SSL_shutdown(info->client.SSL);
	SSL_free(info->client.SSL);
	close(info->client.socket);

	if (info->threads) sem_post(info->threads);

	if (failed) request_err(err, info->serverID);
	free(info);
	return NULL;
	SERVER_ERR:
		status_code_page(500, info,&request,&responce);
		send_responce(info,&request,&responce);
		goto exit;

	BAD_REQUEST:
		status_code_page(400, info,&request,&responce);
		send_responce(info,&request,&responce);
		goto exit;

}


int http_server_run(http_server* srv) {

	int A = 0;
	uint8_t opt = 1;

	if ( pthread_mutex_lock(&mutex) != 0 ) goto MUTEX_err;

	srv->id = http_server_amount;
	http_server_amount++;
	SR++;
	http_server** TEMP_http_servers = realloc(http_servers, http_server_amount * sizeof(http_server*));
	if (TEMP_http_servers == NULL) {
		if (pthread_mutex_unlock(&mutex) != 0) goto MUTEX_err;
		goto SAPALLOC_err;
	}
	http_servers = TEMP_http_servers;
	http_servers[http_server_amount-1] = srv;
	http_servers[http_server_amount] = NULL;


	if (ContentType_ht == NULL) {
		ContentType_ht = hash_table_init(100);

		ht_set_str(ContentType_ht, ".aac", "audio/aac");
		ht_set_str(ContentType_ht, ".abw", "application/x-abiword");
		ht_set_str(ContentType_ht, ".apng", "image/apng");
		ht_set_str(ContentType_ht, ".arc", "application/x-freearc");
		ht_set_str(ContentType_ht, ".avif", "image/avif");
		ht_set_str(ContentType_ht, ".avi", "video/x-msvideo");
		ht_set_str(ContentType_ht, ".azw", "application/vnd.amazon.ebook");

		ht_set_str(ContentType_ht, ".bin", "application/octet-stream");
		ht_set_str(ContentType_ht, ".", "application/octet-stream");

		ht_set_str(ContentType_ht, ".bmp", "image/bmp");
		ht_set_str(ContentType_ht, ".bz", "application/x-bzip");
		ht_set_str(ContentType_ht, ".bz2", "application/x-bzip2");
		ht_set_str(ContentType_ht, ".cda", "application/x-cdf");
		ht_set_str(ContentType_ht, ".csh", "application/x-csh");
		ht_set_str(ContentType_ht, ".css", "text/css");
		ht_set_str(ContentType_ht, ".csv", "text/csv");
		ht_set_str(ContentType_ht, ".doc", "application/msword");
		ht_set_str(ContentType_ht, ".docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document");
		ht_set_str(ContentType_ht, ".eot", "application/vnd.ms-fontobject");
		ht_set_str(ContentType_ht, ".epub", "application/epub+zip");
		ht_set_str(ContentType_ht, ".gz", "application/gzip");
		ht_set_str(ContentType_ht, ".gif", "image/gif");
		ht_set_str(ContentType_ht, ".htm", "text/html");
		ht_set_str(ContentType_ht, ".html", "text/html");
		ht_set_str(ContentType_ht, ".ico", "image/vnd.microsoft.icon");
		ht_set_str(ContentType_ht, ".ics", "text/calendar");
		ht_set_str(ContentType_ht, ".jar", "application/java-archive");
		ht_set_str(ContentType_ht, ".jpeg", "image/jpeg");
		ht_set_str(ContentType_ht, ".js", "text/javascript");
		ht_set_str(ContentType_ht, ".json", "application/json");
		ht_set_str(ContentType_ht, ".jsonld", "application/ld+json");
		ht_set_str(ContentType_ht, ".mid", "audio/midi");
		ht_set_str(ContentType_ht, ".mjs", "text/javascript");
		ht_set_str(ContentType_ht, ".mp3", "audio/mpeg");
		ht_set_str(ContentType_ht, ".mp4", "video/mp4");
		ht_set_str(ContentType_ht, ".mpeg", "video/mpeg");
		ht_set_str(ContentType_ht, ".mpkg", "application/vnd.apple.installer+xml");
		ht_set_str(ContentType_ht, ".odp", "application/vnd.oasis.opendocument.presentation");
		ht_set_str(ContentType_ht, ".ods", "application/vnd.oasis.opendocument.spreadsheet");
		ht_set_str(ContentType_ht, ".odt", "application/vnd.oasis.opendocument.text");
		ht_set_str(ContentType_ht, ".oga", "audio/ogg");
		ht_set_str(ContentType_ht, ".ogv", "video/ogg");
		ht_set_str(ContentType_ht, ".ogx", "application/ogg");
		ht_set_str(ContentType_ht, ".opus", "audio/ogg");
		ht_set_str(ContentType_ht, ".otf", "font/otf");
		ht_set_str(ContentType_ht, ".png", "image/png");
		ht_set_str(ContentType_ht, ".pdf", "application/pdf");
		ht_set_str(ContentType_ht, ".php", "application/x-httpd-php");
		ht_set_str(ContentType_ht, ".ppt", "application/vnd.ms-powerpoint");
		ht_set_str(ContentType_ht, ".pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation");
		ht_set_str(ContentType_ht, ".rar", "application/vnd.rar");
		ht_set_str(ContentType_ht, ".rtf", "application/rtf");
		ht_set_str(ContentType_ht, ".sh", "application/x-sh");
		ht_set_str(ContentType_ht, ".svg", "image/svg+xml");
		ht_set_str(ContentType_ht, ".tar", "application/x-tar");
		ht_set_str(ContentType_ht, ".tif", "image/tiff");
		ht_set_str(ContentType_ht, ".ts", "video/mp2t");
		ht_set_str(ContentType_ht, ".ttf", "font/ttf");
		ht_set_str(ContentType_ht, ".txt", "text/plain");
		ht_set_str(ContentType_ht, ".vsd", "application/vnd.visio");
		ht_set_str(ContentType_ht, ".wav", "audio/wav");
		ht_set_str(ContentType_ht, ".weba", "audio/webm");
		ht_set_str(ContentType_ht, ".webm", "video/webm");
		ht_set_str(ContentType_ht, ".webp", "image/webp");
		ht_set_str(ContentType_ht, ".woff", "font/woff");
		ht_set_str(ContentType_ht, ".woff2", "font/woff2");
		ht_set_str(ContentType_ht, ".xhtml", "application/xhtml+xml");
		ht_set_str(ContentType_ht, ".xls", "application/vnd.ms-excel");
		ht_set_str(ContentType_ht, ".xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet");
		ht_set_str(ContentType_ht, ".xml", "application/xml");
		ht_set_str(ContentType_ht, ".xul", "application/vnd.mozilla.xul+xml");
		ht_set_str(ContentType_ht, ".zip", "application/zip");
		ht_set_str(ContentType_ht, ".3gp", "video/3gpp");
		ht_set_str(ContentType_ht, ".3g2", "video/3gpp2");
		ht_set_str(ContentType_ht, ".7z", "application/x-7z-compressed");
	}

	/*print_hash_table(ContentType_ht, 1);*/

	pthread_mutex_unlock(&mutex);

	sem_t threads;
	if (srv->multi_thread)
		if (sem_init(&threads, 0, srv->threads > 0 ? srv->threads : 1) == EINVAL) goto THREAD_LIMIT_err;

	// PHASE: Start server
	int err = 0;
	short failed = 1;
	struct addrinfo h, *si, *temp_addr;

	if (( srv->socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) goto SOCKET_err;

	memset(&h, 0, sizeof h);
	h.ai_family = AF_INET;
	h.ai_socktype = SOCK_STREAM;

	if ( getaddrinfo(srv->host, srv->port, &h, &si) != 0) goto HOST_err;

	for (temp_addr = si; temp_addr != NULL; temp_addr = temp_addr->ai_next)
		if ( bind(srv->socket, temp_addr->ai_addr, temp_addr->ai_addrlen) > -1 ) break;
	if (temp_addr == NULL) goto BIND_err;

	SSL_CTX* ctx = NULL;
	SSL* ssl = NULL;
	if (srv->use_tls) {
		if ( (ctx = SSL_CTX_new(TLS_server_method())) == NULL) goto SSL_CTX_err;
		if (SSL_CTX_use_certificate_file(ctx, srv->SSL_Certificate, SSL_FILETYPE_PEM) != 1) goto SSL_CERTIFICATE_err;
		if (SSL_CTX_use_PrivateKey_file(ctx, srv->SSL_PrivateKey, SSL_FILETYPE_PEM) != 1 ) goto SSL_PRIVATE_KEY_err;
	}

	if ( listen(srv->socket, 1000) < 0 ) goto LISTEN_err;

	struct sigaction sa;
	sa.sa_handler = &quit;
	if ( sigaction(SIGINT, &sa, NULL) < 0 ) goto SA_FAIL_err;

	if (srv->multi_thread) pthread_mutex_init(&srv->mutex, NULL);

	while (RUN) {


		// PHASE: Accept client connection
		err = 0;
		int code = 0;
		http_request_handler_info* args = malloc(sizeof(http_request_handler_info));
		if (args == NULL) goto ARG_STRUCT_ALLOC_err;
		memset(args, 0, sizeof(http_request_handler_info));

		if (srv->use_tls)
			if ( ssl == NULL )
				if ( (ssl = SSL_new(ctx)) == NULL ) goto SSL_err;
				else args->client.SSL = ssl;

		socklen_t client_ss = sizeof(struct sockaddr);

		accept:
		if ( (args->client.socket = accept(srv->socket, (struct sockaddr*)&args->client.addr, &client_ss)) < 0 )
			if (!RUN) {
				SSL_shutdown(ssl);
				SSL_free(ssl);
				break;
			} else goto accept;

		if (srv->use_tls) {
			if ( SSL_set_fd(ssl, args->client.socket) == 0 ) goto SSL_FD_err;

			if ( (code = SSL_accept(ssl)) != 1 ) {
				ERR_print_errors_fp(stderr);
				switch (SSL_get_error(ssl, code)) {
					case SSL_ERROR_ZERO_RETURN: printf("ZERO_RETURN\n"); break;
					case SSL_ERROR_WANT_READ: printf("WANT_READ\n"); break;
					case SSL_ERROR_WANT_WRITE: printf("WANT_WRITE\n"); break;
					case SSL_ERROR_WANT_CONNECT: printf("WANT_CONNECT\n"); break;
					case SSL_ERROR_WANT_ACCEPT: printf("WANT_ACCEPT\n"); break;
					case SSL_ERROR_SYSCALL: printf("SYSCALL\n"); break;
					case SSL_ERROR_SSL: printf("SSL\n"); break;
				}
				if (code < 0) goto FATAL_TLS_HANDSHAKE_err;
				else goto TLS_HANDSHAKE_err;
			}
		}

		args->client.SSL = ssl;
		args->client.use_tls = srv->use_tls;
		args->request_handlers = srv->request_handlers;
		args->serverID = srv->id;
		args->threads = srv->multi_thread ? &threads : NULL;
//		args->mp = srv->multi_thread ? &srv->mutex : NULL;

		if (srv->multi_thread) {
			sem_wait(&threads);
			pthread_t thread;

			pthread_attr_t detachedThread;
			if (pthread_attr_init(&detachedThread) != 0) goto PTHREAD_ATTR_INIT_err;
			if (pthread_attr_setdetachstate(&detachedThread, PTHREAD_CREATE_DETACHED) != 0) goto THREAD_DETACH_err;

			if ( pthread_create(&thread, &detachedThread, serve_client, args) == EAGAIN) goto MAX_THREADS_err;
		} else
			serve_client(args);

		ssl = NULL;
		continue;


		MAX_THREADS_err: err++;
		THREAD_DETACH_err: err++;
		PTHREAD_ATTR_INIT_err: err++;
		sem_post(&threads);
		FATAL_TLS_HANDSHAKE_err: err++;
		if (srv->use_tls) SSL_shutdown(ssl);
		TLS_HANDSHAKE_err: err++;
		if (srv->use_tls) SSL_free(ssl);
		ssl = NULL;
		SSL_FD_err: err++;
		ACCEPT_err: err++;
		close(args->client.socket);
		SSL_err: err++;
		ARG_STRUCT_ALLOC_err: ;

		request_err(err, srv->id);

	}

	failed = 0;

	SA_FAIL_err: err++;
	LISTEN_err: err++;
	SSL_CERTIFICATE_err: err++;
	SSL_PRIVATE_KEY_err: err++;
	if (srv->use_tls) SSL_CTX_free(ctx);
	SSL_CTX_err: err++;
	BIND_err: err++;
	freeaddrinfo(si);
	HOST_err: err++;
	close(srv->socket);
	SOCKET_err: err++;
	THREAD_LIMIT_err: err++;
	SAPALLOC_err: err++;	// Server pointer allocation error
	MUTEX_err: err++;

	pthread_mutex_lock(&mutex);
	http_servers[srv->id] = NULL;
	if (http_server_amount == 0) {
		sfree(http_servers);
		hash_table_free(ContentType_ht);
		pthread_mutex_destroy(&mutex);
	}
	if (failed) printmsg("Server crashed", 'e', srv->id);
	else printmsg("Server stopped", 's', srv->id);
	pthread_mutex_unlock(&mutex);

	return failed ? fail(err, srv->id) : 0;
}


//
//	Built-in request handlers
//



short status_code_page(short code, HTTP_REQUEST_HANDLER_ARGUMENTS) {
	if (!code) return 0;
	http_request_handler request_handler = NULL;
	if ( (request_handler = get_ghesch(code)) != NULL) return request_handler(info,request,responce);
	char code_str[4];
	sprintf(code_str, "%d", code);
	if ( (request_handler = ht_get(info->request_handlers, code_str)) != NULL)
		return request_handler(info,request,responce);
	return make_status_page(code, responce);
}

static short make_status_page(short code, http_responce* responce) {
	responce->status = code;
	responce->status_str = status_str(code);
	size_t sl = strlen(responce->status_str);
	responce->ContentLength = sl*2 + 59;

	if ( (responce->data = malloc(responce->ContentLength)) == NULL) responce->data = "error";
	else
		sprintf(responce->data, "<head><title>%d %s</title></head><body><h1>%d %s</h1></body>", code, responce->status_str, code, responce->status_str);

	ht_set_str(responce->headers, "Content-Type", "text/html");
	return 0;
}

//	ht_set(server.request_handlers, "Not Found", &handler);
//	OR
//	ht_set(server.request_handlers, "404", &handler);
