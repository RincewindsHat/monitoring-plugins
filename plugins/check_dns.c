/*****************************************************************************
 *
 * Monitoring check_dns plugin
 *
 * License: GPL
 * Copyright (c) 2000-2024 Monitoring Plugins Development Team
 *
 * Description:
 *
 * This file contains the check_dns plugin
 *
 * LIMITATION: nslookup on Solaris 7 can return output over 2 lines, which
 * will not be picked up by this plugin
 *
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *****************************************************************************/

const char *progname = "check_dns";
const char *copyright = "2000-2024";
const char *email = "devel@monitoring-plugins.org";

#include "common.h"
#include "utils.h"
#include "utils_base.h"
#include "netutils.h"
#include "runcmd.h"

#include "states.h"
#include "check_dns.d/config.h"

typedef struct {
	int errorcode;
	check_dns_config config;
} check_dns_config_wrapper;
static check_dns_config_wrapper process_arguments(int /*argc*/, char ** /*argv*/);
static check_dns_config_wrapper validate_arguments(check_dns_config_wrapper /*config_wrapper*/);
static mp_state_enum error_scan(char * /*input_buffer*/, bool * /*is_nxdomain*/, const char /*dns_server*/[ADDRESS_LENGTH]);
static bool ip_match_cidr(const char * /*addr*/, const char * /*cidr_ro*/);
static unsigned long ip2long(const char * /*src*/);
static void print_help(void);
void print_usage(void);

static bool verbose = false;

static int qstrcmp(const void *p1, const void *p2) {
	/* The actual arguments to this function are "pointers to
	   pointers to char", but strcmp() arguments are "pointers
	   to char", hence the following cast plus dereference */
	return strcmp(*(char *const *)p1, *(char *const *)p2);
}

int main(int argc, char **argv) {
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	/* Set signal handling and alarm */
	if (signal(SIGALRM, runcmd_timeout_alarm_handler) == SIG_ERR) {
		usage_va(_("Cannot catch SIGALRM"));
	}

	/* Parse extra opts if any */
	argv = np_extra_opts(&argc, argv, progname);

	check_dns_config_wrapper tmp = process_arguments(argc, argv);

	if (tmp.errorcode == ERROR) {
		usage_va(_("Could not parse arguments"));
	}

	const check_dns_config config = tmp.config;

	char *command_line = NULL;
	/* get the command to run */
	xasprintf(&command_line, "%s %s %s", NSLOOKUP_COMMAND, config.query_address, config.dns_server);

	struct timeval tv;
	alarm(timeout_interval);
	gettimeofday(&tv, NULL);

	if (verbose) {
		printf("%s\n", command_line);
	}

	output chld_out;
	output chld_err;
	char *msg = NULL;
	mp_state_enum result = STATE_UNKNOWN;
	/* run the command */
	if ((np_runcmd(command_line, &chld_out, &chld_err, 0)) != 0) {
		msg = (char *)_("nslookup returned an error status");
		result = STATE_WARNING;
	}

	/* =====
	 * scan stdout, main results get retrieved here
	 * =====
	 */
	char *address = NULL;    /* comma separated str with addrs/ptrs (sorted) */
	char **addresses = NULL; // All addresses parsed from stdout
	size_t n_addresses = 0;  // counter for retrieved addresses
	bool non_authoritative = false;
	bool is_nxdomain = false;
	bool parse_address = false; /* This flag scans for Address: but only after Name: */
	for (size_t i = 0; i < chld_out.lines; i++) {
		if (addresses == NULL) {
			addresses = malloc(sizeof(*addresses) * 10);
		} else if (!(n_addresses % 10)) {
			addresses = realloc(addresses, sizeof(*addresses) * (n_addresses + 10));
		}

		if (verbose) {
			puts(chld_out.line[i]);
		}

		if (strcasestr(chld_out.line[i], ".in-addr.arpa") || strcasestr(chld_out.line[i], ".ip6.arpa")) {
			if ((strstr(chld_out.line[i], "canonical name = ") != NULL)) {
				continue;
			}
			char *temp_buffer = NULL;
			if ((temp_buffer = strstr(chld_out.line[i], "name = "))) {
				addresses[n_addresses++] = strdup(temp_buffer + 7);
			} else {
				msg = (char *)_("Warning plugin error");
				result = STATE_WARNING;
			}
		}

		/* bug ID: 2946553 - Older versions of bind will use all available dns
							 servers, we have to match the one specified */
		if (strstr(chld_out.line[i], "Server:") && strlen(config.dns_server) > 0) {
			char *temp_buffer = strchr(chld_out.line[i], ':');
			if (temp_buffer == NULL) {
				die(STATE_UNKNOWN, _("'%s' returned a weirdly formatted Server line\n"), NSLOOKUP_COMMAND);
			}

			temp_buffer++;

			/* Strip leading tabs */
			for (; *temp_buffer != '\0' && *temp_buffer == '\t'; temp_buffer++) {
				/* NOOP */;
			}

			strip(temp_buffer);
			if (strlen(temp_buffer) == 0) {
				die(STATE_CRITICAL, _("DNS CRITICAL - '%s' returned empty server string\n"), NSLOOKUP_COMMAND);
			}

			if (strcmp(temp_buffer, config.dns_server) != 0) {
				die(STATE_CRITICAL, _("DNS CRITICAL - No response from DNS %s\n"), config.dns_server);
			}
		}

		/* the server is responding, we just got the host name... */
		if (strstr(chld_out.line[i], "Name:")) {
			parse_address = true;
		} else if (parse_address && (strstr(chld_out.line[i], "Address:") || strstr(chld_out.line[i], "Addresses:"))) {
			char *temp_buffer = strchr(chld_out.line[i], ':');
			if (temp_buffer == NULL) {
				die(STATE_UNKNOWN, _("'%s' returned a weirdly formatted Address line\n"), NSLOOKUP_COMMAND);
			}

			temp_buffer++;

			/* Strip leading spaces */
			while (*temp_buffer == ' ') {
				temp_buffer++;
			}

			strip(temp_buffer);
			if (strlen(temp_buffer) == 0) {
				die(STATE_CRITICAL, _("DNS CRITICAL - '%s' returned empty host name string\n"), NSLOOKUP_COMMAND);
			}

			addresses[n_addresses++] = strdup(temp_buffer);
		} else if (strstr(chld_out.line[i], _("Non-authoritative answer:"))) {
			non_authoritative = true;
		}

		result = error_scan(chld_out.line[i], &is_nxdomain, config.dns_server);
		if (result != STATE_OK) {
			msg = strchr(chld_out.line[i], ':');
			if (msg) {
				msg++;
			}
			break;
		}
	}

	char input_buffer[MAX_INPUT_BUFFER];
	/* scan stderr */
	for (size_t i = 0; i < chld_err.lines; i++) {
		if (verbose) {
			puts(chld_err.line[i]);
		}

		if (error_scan(chld_err.line[i], &is_nxdomain, config.dns_server) != STATE_OK) {
			result = max_state(result, error_scan(chld_err.line[i], &is_nxdomain, config.dns_server));
			msg = strchr(input_buffer, ':');
			if (msg) {
				msg++;
			} else {
				msg = input_buffer;
			}
		}
	}

	if (is_nxdomain && !config.expect_nxdomain) {
		die(STATE_CRITICAL, _("Domain '%s' was not found by the server\n"), config.query_address);
	}

	if (addresses) {
		size_t slen = 1;
		char *adrp = NULL;
		qsort(addresses, n_addresses, sizeof(*addresses), qstrcmp);
		for (size_t i = 0; i < n_addresses; i++) {
			slen += strlen(addresses[i]) + 1;
		}

		// Temporary pointer adrp gets moved, address stays on the beginning
		adrp = address = malloc(slen);
		for (size_t i = 0; i < n_addresses; i++) {
			if (i) {
				*adrp++ = ',';
			}
			strcpy(adrp, addresses[i]);
			adrp += strlen(addresses[i]);
		}
		*adrp = 0;
	} else {
		die(STATE_CRITICAL, _("DNS CRITICAL - '%s' msg parsing exited with no address\n"), NSLOOKUP_COMMAND);
	}

	/* compare to expected address */
	if (result == STATE_OK && config.expected_address_cnt > 0) {
		result = STATE_CRITICAL;
		char *temp_buffer = "";
		unsigned long expect_match = (1 << config.expected_address_cnt) - 1;
		unsigned long addr_match = (1 << n_addresses) - 1;

		for (size_t i = 0; i < config.expected_address_cnt; i++) {
			/* check if we get a match on 'raw' ip or cidr */
			for (size_t j = 0; j < n_addresses; j++) {
				if (strcmp(addresses[j], config.expected_address[i]) == 0 || ip_match_cidr(addresses[j], config.expected_address[i])) {
					result = STATE_OK;
					addr_match &= ~(1 << j);
					expect_match &= ~(1 << i);
				}
			}

			/* prepare an error string */
			xasprintf(&temp_buffer, "%s%s; ", temp_buffer, config.expected_address[i]);
		}
		/* check if expected_address must cover all in addresses and none may be missing */
		if (config.all_match && (expect_match != 0 || addr_match != 0)) {
			result = STATE_CRITICAL;
		}
		if (result == STATE_CRITICAL) {
			/* Strip off last semicolon... */
			temp_buffer[strlen(temp_buffer) - 2] = '\0';
			xasprintf(&msg, _("expected '%s' but got '%s'"), temp_buffer, address);
		}
	}

	if (config.expect_nxdomain) {
		if (!is_nxdomain) {
			result = STATE_CRITICAL;
			xasprintf(&msg, _("Domain '%s' was found by the server: '%s'\n"), config.query_address, address);
		} else {
			if (address != NULL) {
				free(address);
			}
			address = "NXDOMAIN";
		}
	}

	/* check if authoritative */
	if (result == STATE_OK && config.expect_authority && non_authoritative) {
		result = STATE_CRITICAL;
		xasprintf(&msg, _("server %s is not authoritative for %s"), config.dns_server, config.query_address);
	}

	long microsec = deltime(tv);
	double elapsed_time = (double)microsec / 1.0e6;

	if (result == STATE_OK) {
		result = get_status(elapsed_time, config.time_thresholds);
		if (result == STATE_OK) {
			printf("DNS %s: ", _("OK"));
		} else if (result == STATE_WARNING) {
			printf("DNS %s: ", _("WARNING"));
		} else if (result == STATE_CRITICAL) {
			printf("DNS %s: ", _("CRITICAL"));
		}
		printf(ngettext("%.3f second response time", "%.3f seconds response time", elapsed_time), elapsed_time);
		printf(_(". %s returns %s"), config.query_address, address);
		if ((config.time_thresholds->warning != NULL) && (config.time_thresholds->critical != NULL)) {
			printf("|%s\n", fperfdata("time", elapsed_time, "s", true, config.time_thresholds->warning->end, true,
									  config.time_thresholds->critical->end, true, 0, false, 0));
		} else if ((config.time_thresholds->warning == NULL) && (config.time_thresholds->critical != NULL)) {
			printf("|%s\n", fperfdata("time", elapsed_time, "s", false, 0, true, config.time_thresholds->critical->end, true, 0, false, 0));
		} else if ((config.time_thresholds->warning != NULL) && (config.time_thresholds->critical == NULL)) {
			printf("|%s\n", fperfdata("time", elapsed_time, "s", true, config.time_thresholds->warning->end, false, 0, true, 0, false, 0));
		} else {
			printf("|%s\n", fperfdata("time", elapsed_time, "s", false, 0, false, 0, true, 0, false, 0));
		}
	} else if (result == STATE_WARNING) {
		printf(_("DNS WARNING - %s\n"), !strcmp(msg, "") ? _(" Probably a non-existent host/domain") : msg);
	} else if (result == STATE_CRITICAL) {
		printf(_("DNS CRITICAL - %s\n"), !strcmp(msg, "") ? _(" Probably a non-existent host/domain") : msg);
	} else {
		printf(_("DNS UNKNOWN - %s\n"), !strcmp(msg, "") ? _(" Probably a non-existent host/domain") : msg);
	}

	exit(result);
}

bool ip_match_cidr(const char *addr, const char *cidr_ro) {
	char *subnet;
	char *mask_c;
	char *cidr = strdup(cidr_ro);
	int mask;
	subnet = strtok(cidr, "/");
	mask_c = strtok(NULL, "\0");
	if (!subnet || !mask_c) {
		return false;
	}
	mask = atoi(mask_c);

	/* https://www.cryptobells.com/verifying-ips-in-a-subnet-in-php/ */
	return (ip2long(addr) & ~((1 << (32 - mask)) - 1)) == (ip2long(subnet) >> (32 - mask)) << (32 - mask);
}

unsigned long ip2long(const char *src) {
	unsigned long ip[4];
	/* http://computer-programming-forum.com/47-c-language/1376ffb92a12c471.htm */
	return (sscanf(src, "%3lu.%3lu.%3lu.%3lu", &ip[0], &ip[1], &ip[2], &ip[3]) == 4 && ip[0] < 256 && ip[1] < 256 && ip[2] < 256 &&
			ip[3] < 256)
			   ? ip[0] << 24 | ip[1] << 16 | ip[2] << 8 | ip[3]
			   : 0;
}

mp_state_enum error_scan(char *input_buffer, bool *is_nxdomain, const char dns_server[ADDRESS_LENGTH]) {

	const int nxdomain = strstr(input_buffer, "Non-existent") || strstr(input_buffer, "** server can't find") ||
						 strstr(input_buffer, "** Can't find") || strstr(input_buffer, "NXDOMAIN");
	if (nxdomain) {
		*is_nxdomain = true;
	}

	/* the DNS lookup timed out */
	if (strstr(input_buffer, _("Note: nslookup is deprecated and may be removed from future releases.")) ||
		strstr(input_buffer, _("Consider using the `dig' or `host' programs instead.  Run nslookup with")) ||
		strstr(input_buffer, _("the `-sil[ent]' option to prevent this message from appearing."))) {
		return STATE_OK;
	}

	/* DNS server is not running... */
	else if (strstr(input_buffer, "No response from server")) {
		die(STATE_CRITICAL, _("No response from DNS %s\n"), dns_server);
	} else if (strstr(input_buffer, "no servers could be reached")) {
		die(STATE_CRITICAL, _("No response from DNS %s\n"), dns_server);
	}

	/* Host name is valid, but server doesn't have records... */
	else if (strstr(input_buffer, "No records")) {
		die(STATE_CRITICAL, _("DNS %s has no records\n"), dns_server);
	}

	/* Connection was refused */
	else if (strstr(input_buffer, "Connection refused") || strstr(input_buffer, "Couldn't find server") ||
			 strstr(input_buffer, "Refused") || (strstr(input_buffer, "** server can't find") && strstr(input_buffer, ": REFUSED"))) {
		die(STATE_CRITICAL, _("Connection to DNS %s was refused\n"), dns_server);
	}

	/* Query refused (usually by an ACL in the namserver) */
	else if (strstr(input_buffer, "Query refused")) {
		die(STATE_CRITICAL, _("Query was refused by DNS server at %s\n"), dns_server);
	}

	/* No information (e.g. nameserver IP has two PTR records) */
	else if (strstr(input_buffer, "No information")) {
		die(STATE_CRITICAL, _("No information returned by DNS server at %s\n"), dns_server);
	}

	/* Network is unreachable */
	else if (strstr(input_buffer, "Network is unreachable")) {
		die(STATE_CRITICAL, _("Network is unreachable\n"));
	}

	/* Internal server failure */
	else if (strstr(input_buffer, "Server failure")) {
		die(STATE_CRITICAL, _("DNS failure for %s\n"), dns_server);
	}

	/* Request error or the DNS lookup timed out */
	else if (strstr(input_buffer, "Format error") || strstr(input_buffer, "Timed out")) {
		return STATE_WARNING;
	}

	return STATE_OK;
}

/* process command-line arguments */
check_dns_config_wrapper process_arguments(int argc, char **argv) {
	static struct option long_opts[] = {{"help", no_argument, 0, 'h'},
										{"version", no_argument, 0, 'V'},
										{"verbose", no_argument, 0, 'v'},
										{"timeout", required_argument, 0, 't'},
										{"hostname", required_argument, 0, 'H'},
										{"server", required_argument, 0, 's'},
										{"reverse-server", required_argument, 0, 'r'},
										{"expected-address", required_argument, 0, 'a'},
										{"expect-nxdomain", no_argument, 0, 'n'},
										{"expect-authority", no_argument, 0, 'A'},
										{"all", no_argument, 0, 'L'},
										{"warning", required_argument, 0, 'w'},
										{"critical", required_argument, 0, 'c'},
										{0, 0, 0, 0}};

	check_dns_config_wrapper result = {
		.config = check_dns_config_init(),
		.errorcode = OK,
	};

	if (argc < 2) {
		result.errorcode = ERROR;
		return result;
	}

	for (int index = 1; index < argc; index++) {
		if (strcmp("-to", argv[index]) == 0) {
			strcpy(argv[index], "-t");
		}
	}

	char *warning = NULL;
	char *critical = NULL;
	int opt_index = 0;
	int index = 0;
	while (true) {
		index = getopt_long(argc, argv, "hVvALnt:H:s:r:a:w:c:", long_opts, &opt_index);

		if (index == -1 || index == EOF) {
			break;
		}

		switch (index) {
		case 'h': /* help */
			print_help();
			exit(STATE_UNKNOWN);
		case 'V': /* version */
			print_revision(progname, NP_VERSION);
			exit(STATE_UNKNOWN);
		case 'v': /* version */
			verbose = true;
			break;
		case 't': /* timeout period */
			timeout_interval = atoi(optarg);
			break;
		case 'H': /* hostname */
			if (strlen(optarg) >= ADDRESS_LENGTH) {
				die(STATE_UNKNOWN, _("Input buffer overflow\n"));
			}
			strcpy(result.config.query_address, optarg);
			break;
		case 's': /* server name */
			/* TODO: this host_or_die check is probably unnecessary.
			 * Better to confirm nslookup response matches */
			host_or_die(optarg);
			if (strlen(optarg) >= ADDRESS_LENGTH) {
				die(STATE_UNKNOWN, _("Input buffer overflow\n"));
			}
			strcpy(result.config.dns_server, optarg);
			break;
		case 'r': /* reverse server name */
			/* TODO: Is this host_or_die necessary? */
			// TODO This does not do anything!!! 2025-03-08 rincewind
			host_or_die(optarg);
			if (strlen(optarg) >= ADDRESS_LENGTH) {
				die(STATE_UNKNOWN, _("Input buffer overflow\n"));
			}
			static char ptr_server[ADDRESS_LENGTH] = "";
			strcpy(ptr_server, optarg);
			break;
		case 'a': /* expected address */
			if (strlen(optarg) >= ADDRESS_LENGTH) {
				die(STATE_UNKNOWN, _("Input buffer overflow\n"));
			}
			if (strchr(optarg, ',') != NULL) {
				char *comma = strchr(optarg, ',');
				while (comma != NULL) {
					result.config.expected_address =
						(char **)realloc(result.config.expected_address, (result.config.expected_address_cnt + 1) * sizeof(char **));
					result.config.expected_address[result.config.expected_address_cnt] = strndup(optarg, comma - optarg);
					result.config.expected_address_cnt++;
					optarg = comma + 1;
					comma = strchr(optarg, ',');
				}
				result.config.expected_address =
					(char **)realloc(result.config.expected_address, (result.config.expected_address_cnt + 1) * sizeof(char **));
				result.config.expected_address[result.config.expected_address_cnt] = strdup(optarg);
				result.config.expected_address_cnt++;
			} else {
				result.config.expected_address =
					(char **)realloc(result.config.expected_address, (result.config.expected_address_cnt + 1) * sizeof(char **));
				result.config.expected_address[result.config.expected_address_cnt] = strdup(optarg);
				result.config.expected_address_cnt++;
			}
			break;
		case 'n': /* expect NXDOMAIN */
			result.config.expect_nxdomain = true;
			break;
		case 'A': /* expect authority */
			result.config.expect_authority = true;
			break;
		case 'L': /* all must match */
			result.config.all_match = true;
			break;
		case 'w':
			warning = optarg;
			break;
		case 'c':
			critical = optarg;
			break;
		default: /* args not parsable */
			usage5();
		}
	}

	index = optind;
	if (strlen(result.config.query_address) == 0 && index < argc) {
		if (strlen(argv[index]) >= ADDRESS_LENGTH) {
			die(STATE_UNKNOWN, _("Input buffer overflow\n"));
		}
		strcpy(result.config.query_address, argv[index++]);
	}

	if (strlen(result.config.dns_server) == 0 && index < argc) {
		/* TODO: See -s option */
		host_or_die(argv[index]);
		if (strlen(argv[index]) >= ADDRESS_LENGTH) {
			die(STATE_UNKNOWN, _("Input buffer overflow\n"));
		}
		strcpy(result.config.dns_server, argv[index++]);
	}

	set_thresholds(&result.config.time_thresholds, warning, critical);

	return validate_arguments(result);
}

check_dns_config_wrapper validate_arguments(check_dns_config_wrapper config_wrapper) {
	if (config_wrapper.config.query_address[0] == 0) {
		printf("missing --host argument\n");
		config_wrapper.errorcode = ERROR;
		return config_wrapper;
	}

	if (config_wrapper.config.expected_address_cnt > 0 && config_wrapper.config.expect_nxdomain) {
		printf("--expected-address and --expect-nxdomain cannot be combined\n");
		config_wrapper.errorcode = ERROR;
		return config_wrapper;
	}

	return config_wrapper;
}

void print_help(void) {
	print_revision(progname, NP_VERSION);

	printf("Copyright (c) 1999 Ethan Galstad <nagios@nagios.org>\n");
	printf(COPYRIGHT, copyright, email);

	printf("%s\n", _("This plugin uses the nslookup program to obtain the IP address for the given host/domain query."));
	printf("%s\n", _("An optional DNS server to use may be specified."));
	printf("%s\n", _("If no DNS server is specified, the default server(s) specified in /etc/resolv.conf will be used."));

	printf("\n\n");

	print_usage();

	printf(UT_HELP_VRSN);
	printf(UT_EXTRA_OPTS);

	printf(" -H, --hostname=HOST\n");
	printf("    %s\n", _("The name or address you want to query"));
	printf(" -s, --server=HOST\n");
	printf("    %s\n", _("Optional DNS server you want to use for the lookup"));
	printf(" -a, --expected-address=IP-ADDRESS|CIDR|HOST\n");
	printf("    %s\n", _("Optional IP-ADDRESS/CIDR you expect the DNS server to return. HOST must end"));
	printf("    %s\n", _("with a dot (.). This option can be repeated multiple times (Returns OK if any"));
	printf("    %s\n", _("value matches)."));
	printf(" -n, --expect-nxdomain\n");
	printf("    %s\n", _("Expect the DNS server to return NXDOMAIN (i.e. the domain was not found)"));
	printf("    %s\n", _("Cannot be used together with -a"));
	printf(" -A, --expect-authority\n");
	printf("    %s\n", _("Optionally expect the DNS server to be authoritative for the lookup"));
	printf(" -w, --warning=seconds\n");
	printf("    %s\n", _("Return warning if elapsed time exceeds value. Default off"));
	printf(" -c, --critical=seconds\n");
	printf("    %s\n", _("Return critical if elapsed time exceeds value. Default off"));
	printf(" -L, --all\n");
	printf("    %s\n", _("Return critical if the list of expected addresses does not match all addresses"));
	printf("    %s\n", _("returned. Default off"));

	printf(UT_CONN_TIMEOUT, DEFAULT_SOCKET_TIMEOUT);

	printf(UT_SUPPORT);
}

void print_usage(void) {
	printf("%s\n", _("Usage:"));
	printf("%s -H host [-s server] [-a expected-address] [-n] [-A] [-t timeout] [-w warn] [-c crit] [-L]\n", progname);
}
