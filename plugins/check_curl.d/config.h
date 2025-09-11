#pragma once

#include "../../config.h"
#include "../common.h"
#include "../../lib/states.h"
#include "../../lib/thresholds.h"
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include "curl/curl.h"
#include "regex.h"

enum {
	MAX_RE_SIZE = 1024,
	HTTP_PORT = 80,
	HTTPS_PORT = 443,
	MAX_PORT = 65535,
	DEFAULT_MAX_REDIRS = 15
};

enum {
	FOLLOW_HTTP_CURL = 0,
	FOLLOW_LIBCURL = 1
};

enum {
	STICKY_NONE = 0,
	STICKY_HOST = 1,
	STICKY_PORT = 2
};

#define HTTP_EXPECT         "HTTP/"
#define DEFAULT_BUFFER_SIZE 2048
#define DEFAULT_SERVER_URL  "/"

typedef struct {
	char *server_address;
	char *server_url;
	char *host_name;

	char *http_method;

	char *http_post_data;

	unsigned short virtualPort;
	unsigned short serverPort;

	bool use_ssl;
	bool no_body;
} check_curl_working_state;

check_curl_working_state check_curl_working_state_init();

typedef struct {
	bool automatic_decompression;
	bool haproxy_protocol;
	long socket_timeout;
	sa_family_t sin_family;
	int curl_http_version;
	char **http_opt_headers;
	size_t http_opt_headers_count;
	int ssl_version;
	char *client_cert;
	char *client_privkey;
	char *ca_cert;
	bool verify_peer_and_host;
	char user_agent[DEFAULT_BUFFER_SIZE];
	char proxy_auth[MAX_INPUT_BUFFER];
	char user_auth[MAX_INPUT_BUFFER];
	char *http_content_type;
	char *cookie_jar_file;
} check_curl_static_curl_config;

typedef struct {
	check_curl_working_state initial_config;

	check_curl_static_curl_config curl_config;
	int max_depth;
	int followmethod;
	int followsticky;

	int maximum_age;

	// the original regex string from the command line
	char regexp[MAX_RE_SIZE];

	// the compiled regex for usage later
	regex_t compiled_regex;

	mp_state_enum state_regex;
	bool invert_regex;
	bool check_cert;
	bool continue_after_check_cert;
	int days_till_exp_warn;
	int days_till_exp_crit;
	thresholds *thlds;
	size_t min_page_len;
	size_t max_page_len;
	struct {
		char string[MAX_INPUT_BUFFER];
		bool is_present;
	} server_expect;
	char string_expect[MAX_INPUT_BUFFER];
	char header_expect[MAX_INPUT_BUFFER];
	mp_state_enum on_redirect_result_state;
	bool on_redirect_dependent;

	bool show_extended_perfdata;
	bool show_body;
	bool display_html;
} check_curl_config;

check_curl_config check_curl_config_init();
