#include <server.h>

#include "../questions.txt"

static inline /* __attribute__((always_inline)) */ int int_str_len(unsigned int n) {
	int s = 1;
	for (int i = 10; n/i > 0; i*=10) s++;
	return s;
}
static char* Anonymous = "Anonymous";
hash_table* users;

void user_renew(char* uid) {
	ht_set(users, uid, NULL);
	ht_set(users, uid, malloc(sizeof(struct user)));
}

short returnUser(http_responce* responce, char* uid) {
//	char cookie[41] = "uid=";
//	strncpy(cookie+4, uid, 36);
//	cookie[40] = 0;
//	return ht_set_str(responce->headers, "Set-Cookie", cookie);
	return ht_set_str(responce->headers, "X-UID", uid);
}

char* getUser(http_request* rq, char* uid) {
	uuid_t uuid;
	uint8_t progress = 0;
	char* auth = ht_get(rq->headers, "X-UID");
	uid[36] = 0;
	if (auth)
		if (strlen(auth) == 36) {
			if (ht_get(users, auth)) {
				strcpy(uid, auth);
				return uid;
			}
		}

	do {
		uuid_generate_random(uuid);
		uuid_unparse(uuid, uid);
	} while (ht_get(users, uid));
	struct user* user = malloc(sizeof(struct user));
	if (!user) return uid;
	user->name = Anonymous;
	user_renew(uid);
	ht_set_heap(users, uid, user);
	return uid;
}

char* getUserFromCookies(char* cookies, char* uid) {
	uuid_t uuid;
	uint8_t progress = 0;
	char* cookie = NULL;
	char* semicolon;
	uid[36] = 0;
	if (cookies)
		cookie = (cookie = strstr(cookies, "uid=")) ? cookie+4 : NULL;
	if (cookie)
		if ((semicolon = strchr(cookie, ';')))
			if (semicolon - cookie == 36)
				strncpy(uid, cookie, 36);
			else goto bad_uid;
		else
			if (strlen(cookie) == 36)
				strncpy(uid, cookie, 36);
			else goto bad_uid;

	if (ht_get(users, uid))
		return uid;

	do {
bad_uid:
		uuid_generate_random(uuid);
		uuid_unparse(uuid, uid);
	} while (ht_get(users, uid));
	struct user* user = malloc(sizeof(struct user));
	if (!user) return uid;
	user->name = Anonymous;
	user_renew(uid);
	ht_set_heap(users, uid, user);
	return uid;
}

short send_status(short status, HTTP_REQUEST_HANDLER_ARGUMENTS) {
	status_code_page(status, HTTP_PASS_REQUEST_HANDLER_ARGUMENTS);
	send_responce(HTTP_PASS_REQUEST_HANDLER_ARGUMENTS);
	return 0;
}

short YOU_CANT_VOTE(HTTP_REQUEST_HANDLER_ARGUMENTS) {
	responce->status = 403;
	responce->data = "You can't answer!";
	send_responce(HTTP_PASS_REQUEST_HANDLER_ARGUMENTS);
	return 0;
}

short YOU_HAVE_VOTED(HTTP_REQUEST_HANDLER_ARGUMENTS) {
	responce->status = 403;
	responce->data = "You have already chosen your answer!";
	send_responce(HTTP_PASS_REQUEST_HANDLER_ARGUMENTS);
	return 0;
}

short send_forbidden(char* msg, size_t msg_len, HTTP_REQUEST_HANDLER_ARGUMENTS) {
	responce->status = 403;
	responce->data = msg;
	responce->ContentLength = msg_len;
	send_responce(HTTP_PASS_REQUEST_HANDLER_ARGUMENTS);
	return 0;
}

short send_bad(char* msg, size_t msg_len, HTTP_REQUEST_HANDLER_ARGUMENTS) {
	responce->status = 400;
	responce->data = msg;
	responce->ContentLength = msg_len;
	send_responce(HTTP_PASS_REQUEST_HANDLER_ARGUMENTS);
	return 0;
}

short this_answer(char* uid, struct answer* answer) {
	for (uint16_t user = 0; user < answer->votes; user++)
		if (!strcmp(answer->voters[user], uid))
			return 1;
	return 0;
}

char* answered(char* uid, struct question* question) {
	for (uint16_t i = 0; i < question->na; i++)
		if (this_answer(uid, &question->answers[i]))
			return question->answers[i].str;
	return NULL;
}

short answer(HTTP_REQUEST_HANDLER_ARGUMENTS) {
	if (strcmp("POST", request->method)) {
		status_code_page(405, HTTP_PASS_REQUEST_HANDLER_ARGUMENTS);
		send_responce(HTTP_PASS_REQUEST_HANDLER_ARGUMENTS);
		return 0;
	}

	if (!request->data) {
		status_code_page(411, HTTP_PASS_REQUEST_HANDLER_ARGUMENTS);
		send_responce(HTTP_PASS_REQUEST_HANDLER_ARGUMENTS);
		return 0;
	}


	//
	//	Variables
	//
	char uid[37];
	char* answerIDstr;
	char* questionIDstr;
	char* sectionIDstr;
	uint16_t sectionIndex = 0;
	uint16_t questionIndex = 0;
	uint16_t answerIndex = 0;
	struct section* section = NULL;
	struct question* question = NULL;
	struct answer* answer = NULL;


	// getting input
#ifdef GET_INPUT_FROM_POST_DATA
	sectionIDstr = strtok_r(request->data, "\n", &questionIDstr);
	strtok_r(questionIDstr, "\n", &answerIDstr);
#else
	sectionIDstr = strtok_r(request->suburl, "/", &questionIDstr);
	strtok_r(questionIDstr, "/", &answerIDstr);
#endif

	//
	//	Getting data
	//
	getUser(request, uid);
	returnUser(responce, uid);
	if (!sectionIDstr || !answerIDstr || !questionIDstr) {
		status_code_page(400, HTTP_PASS_REQUEST_HANDLER_ARGUMENTS);
		send_responce(HTTP_PASS_REQUEST_HANDLER_ARGUMENTS);
		return 0;
	}

	// index must be = index + 1
	if (!(sectionIndex = atoi(sectionIDstr)) || sectionIndex > section_n)
		return send_bad("Invalid section id", 19, HTTP_PASS_REQUEST_HANDLER_ARGUMENTS);
	section = &sections[--sectionIndex];
	if (!(questionIndex = atoi(questionIDstr)) || questionIndex > section->nq)
		return send_bad("Invalid question id", 20, HTTP_PASS_REQUEST_HANDLER_ARGUMENTS);
	question = &section->questions[--questionIndex];

	// get answer index
	// and check if user has already answered
	for (uint16_t i = 0; i < question->na; i++) {
		for (uint16_t user = 0; user < question->answers[i].votes; user++)
			if (!strcmp(question->answers[i].voters[user], uid))
				return send_forbidden("You have already chosen your answer for this entry", 51, HTTP_PASS_REQUEST_HANDLER_ARGUMENTS);
		if (!strcmp(question->answers[i].str, answerIDstr))
			answer = &question->answers[i];
	}
	if (!answer)
		return send_bad("Invalid answer", 15, HTTP_PASS_REQUEST_HANDLER_ARGUMENTS);

	// add user to people who have chosen this answer
	char** temp = realloc(answer->voters, ++answer->votes * sizeof(char*));
	if (!temp) {
		answer->votes--;
		return 1;
	}
	answer->voters = temp;
	answer->voters[answer->votes-1] = strdup(uid);
	question->total_votes++;

	responce->status = 200;
	send_responce(HTTP_PASS_REQUEST_HANDLER_ARGUMENTS);

	return 0;
}

ssize_t send_chunk(struct connection* c, size_t n, void* buffer) {
	uint16_t nl = n+4;
	size_t num = n;
	if (!buffer)
		if (n)
			return -2;
	if (num)
		for (; num; num /= 16)
			nl++;
	// handle 0 separately
	else nl++;
	char data[nl];
	memcpy(data + 
		sprintf(data, "%zx\r\n", n),
	// good when n == 0 && buffer == NULL
	// otherwise - not my mistake
	buffer, n);
	data[nl-2] = '\r'; data[nl-1] = '\n';
	return cwrite(c, data, nl);
}

short section_data(HTTP_REQUEST_HANDLER_ARGUMENTS) {
	char uid[37];
	char str[FETCH_BUFFER_SIZE];
	int len;
	uint8_t answerd = 0;
	uint16_t sectionIndex;
	struct section* section = NULL;
	struct question* question = NULL;
	struct answer* answer = NULL;
	char* user_answer_str; // will be set to NULL later
	char* correct_str;
	getUser(request, uid);
	returnUser(responce, uid);

	responce->status = 200;
	ht_set(responce->headers, "Transfer-Encoding", "chunked");
	ht_set(responce->headers, "Content-Type", "application/json; charset=utf-8");
	send_responce(HTTP_PASS_REQUEST_HANDLER_ARGUMENTS);

	// index must be = index + 1
	if (!(sectionIndex = atoi(request->suburl)) || sectionIndex > section_n)
		return send_bad("Invalid section id", 19, HTTP_PASS_REQUEST_HANDLER_ARGUMENTS);
	section = &sections[--sectionIndex];

	// start question array
	send_chunk(&info->client, 1, "[");
	for (uint16_t q = 0; q < section->nq; q++) {
		question = &section->questions[q];
		user_answer_str = NULL;
		correct_str = NULL;

		if (!q)
			send_chunk(&info->client, 11, "{\"answers\":");
		else
			send_chunk(&info->client, 12, ",{\"answers\":");
		if (question->typeTF)
			send_chunk(&info->client, 5, "\"TF\",");
		else {
			for (uint16_t a = 0; a < question->na; a++) {
				answer = &question->answers[a];
				for (uint16_t user = 0; user < answer->votes; user++)
					if (!strcmp(answer->voters[user], uid))
						user_answer_str = answer->str;
				len = snprintf(str, FETCH_BUFFER_SIZE, "%c\"%s\"", (!a) ? '[' : ',', answer->str);
				if (len < 0) return 1;
				send_chunk(&info->client, len, str);
			}
			send_chunk(&info->client, 2, "],");
		}

		// load correct answer if allowed
		if (question->show_correct_to_all || (question->show_correct_after_answer && user_answer_str))
			correct_str = question->answers[question->correct_answer].str;

		// send chosen answer
		// comma at the end because of `open` after it
		if (user_answer_str) {
			len = snprintf(str, FETCH_BUFFER_SIZE, "\"answer\":\"%s\",", user_answer_str);
			if (len < 0) return 1;
			send_chunk(&info->client, len, str);
		}

		len = snprintf(str, FETCH_BUFFER_SIZE, "\"open\":%s%s",
				// is question available for answering
				(question->open) ? "true" : "false",
				// closes the question object if no correct answer is available
				(correct_str) ? "" : "}"
		);
		if (len < 0) return 1;
		send_chunk(&info->client, len, str);

		// send correct answer
		// comma at the start because of `open` before it
		if (correct_str) {
			len = snprintf(str, FETCH_BUFFER_SIZE, ",\"correct\":\"%s\"}", correct_str);
			if (len < 0) return 1;
			send_chunk(&info->client, len, str);
		}
	}
	send_chunk(&info->client, 1, "]");
	send_chunk(&info->client, 0, NULL);

	return 0;
}

short section_amount(HTTP_REQUEST_HANDLER_ARGUMENTS) {
	int s = 1;
	for (int i = 10; (int)(section_n/i) > 0; i*=10) s++;
	char str[s];
	responce->data = str;
	snprintf(responce->data, s, "%d", section_n);
	responce->status = 200;
	send_responce(HTTP_PASS_REQUEST_HANDLER_ARGUMENTS);
	return 0;
}

short file(HTTP_REQUEST_HANDLER_ARGUMENTS) {
	return http_respond(HTTP_PASS_REQUEST_HANDLER_ARGUMENTS, "./wochenbericht");
}

int main(int argc, char** argv) {
	if (argc == 1 && puts("give args idiot")) return 1;
	users = hash_table_init(10000);

	http_server server = {.request_handlers = hash_table_init(10000), .host = "0.0.0.0", .port = argv[1], .use_tls = 0, .multi_thread = 0};
	if (!server.request_handlers)
		return 2;

	ht_set(server.request_handlers, "/ ", file);
	ht_set(server.request_handlers, "/wochenbericht/ ", file);
	ht_set(server.request_handlers, "/api/get/section/ ", section_data);
	ht_set(server.request_handlers, "/api/answer", answer);

	http_server_run(&server);

	hash_table_free(server.request_handlers);
	hash_table_free(users);
	return 0;
}
