// SPDX-License-Identifier: GPL-2.0-only
/*
 * labmsg - IPC client for labwc (swaymsg-compatible interface)
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <json-c/json.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define IPC_MAGIC "labwc-ipc"
#define IPC_MAGIC_LEN 9
#define IPC_HEADER_SIZE (IPC_MAGIC_LEN + 4 + 4)

/* Message types */
enum ipc_msg_type {
	IPC_RUN_COMMAND = 0,
	IPC_GET_WORKSPACES = 1,
	IPC_SUBSCRIBE = 2,
	IPC_GET_OUTPUTS = 3,
	IPC_GET_TREE = 4,
	IPC_GET_BAR_CONFIG = 6,
	IPC_GET_VERSION = 7,
	IPC_GET_CONFIG = 9,
	IPC_SEND_TICK = 10,
	IPC_SYNC = 11,
	IPC_GET_INPUTS = 100,
	IPC_GET_SEATS = 101,
};

static const char *version_str = "labmsg 0.1";

static const struct option long_options[] = {{"help", no_argument, NULL, 'h'},
	{"monitor", no_argument, NULL, 'm'}, {"pretty", no_argument, NULL, 'p'},
	{"quiet", no_argument, NULL, 'q'}, {"raw", no_argument, NULL, 'r'},
	{"socket", required_argument, NULL, 's'},
	{"type", required_argument, NULL, 't'},
	{"version", no_argument, NULL, 'v'}, {0, 0, 0, 0}};

static const char usage_str[] =
	"Usage: labmsg [options] [message]\n"
	"\n"
	"Options:\n"
	"  -h, --help           Show help and exit\n"
	"  -m, --monitor        Monitor for events (subscribe mode)\n"
	"  -p, --pretty         Force pretty-printed JSON output\n"
	"  -q, --quiet          Suppress response output\n"
	"  -r, --raw            Force raw JSON output\n"
	"  -s, --socket <path>  Override socket path\n"
	"  -t, --type <type>    Message type (default: run_command)\n"
	"  -v, --version        Show version and exit\n"
	"\n"
	"Message types:\n"
	"  run_command, get_workspaces, subscribe, get_outputs,\n"
	"  get_tree, get_bar_config, get_version, get_config,\n"
	"  send_tick, get_inputs, get_seats\n";

static int
parse_msg_type(const char *name)
{
	if (!name || !strcmp(name, "run_command")) {
		return IPC_RUN_COMMAND;
	} else if (!strcmp(name, "get_workspaces")) {
		return IPC_GET_WORKSPACES;
	} else if (!strcmp(name, "subscribe")) {
		return IPC_SUBSCRIBE;
	} else if (!strcmp(name, "get_outputs")) {
		return IPC_GET_OUTPUTS;
	} else if (!strcmp(name, "get_tree")) {
		return IPC_GET_TREE;
	} else if (!strcmp(name, "get_bar_config")) {
		return IPC_GET_BAR_CONFIG;
	} else if (!strcmp(name, "get_version")) {
		return IPC_GET_VERSION;
	} else if (!strcmp(name, "get_config")) {
		return IPC_GET_CONFIG;
	} else if (!strcmp(name, "send_tick")) {
		return IPC_SEND_TICK;
	} else if (!strcmp(name, "get_inputs")) {
		return IPC_GET_INPUTS;
	} else if (!strcmp(name, "get_seats")) {
		return IPC_GET_SEATS;
	}
	fprintf(stderr, "Unknown message type: %s\n", name);
	return -1;
}

static int
ipc_connect(const char *socket_path)
{
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		return -1;
	}

	struct sockaddr_un addr = {0};
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("connect");
		close(fd);
		return -1;
	}
	return fd;
}

static bool
ipc_send(int fd, uint32_t type, const char *payload, uint32_t len)
{
	char header[IPC_HEADER_SIZE];
	memcpy(header, IPC_MAGIC, IPC_MAGIC_LEN);
	memcpy(header + IPC_MAGIC_LEN, &len, sizeof(uint32_t));
	memcpy(header + IPC_MAGIC_LEN + 4, &type, sizeof(uint32_t));

	if (write(fd, header, IPC_HEADER_SIZE) != IPC_HEADER_SIZE) {
		return false;
	}
	if (len > 0 && write(fd, payload, len) != (ssize_t)len) {
		return false;
	}
	return true;
}

static bool
read_exact(int fd, void *buf, size_t count)
{
	size_t total = 0;
	while (total < count) {
		ssize_t n = read(fd, (char *)buf + total, count - total);
		if (n <= 0) {
			return false;
		}
		total += n;
	}
	return true;
}

static bool
ipc_recv(int fd, uint32_t *type, char **payload, uint32_t *len)
{
	char header[IPC_HEADER_SIZE];
	if (!read_exact(fd, header, IPC_HEADER_SIZE)) {
		return false;
	}

	if (memcmp(header, IPC_MAGIC, IPC_MAGIC_LEN) != 0) {
		fprintf(stderr, "Invalid IPC response magic\n");
		return false;
	}

	memcpy(len, header + IPC_MAGIC_LEN, sizeof(uint32_t));
	memcpy(type, header + IPC_MAGIC_LEN + 4, sizeof(uint32_t));

	if (*len > 0) {
		*payload = malloc(*len + 1);
		if (!*payload) {
			return false;
		}
		if (!read_exact(fd, *payload, *len)) {
			free(*payload);
			*payload = NULL;
			return false;
		}
		(*payload)[*len] = '\0';
	} else {
		*payload = NULL;
	}
	return true;
}

static void
print_json(const char *data, bool pretty)
{
	struct json_object *obj = json_tokener_parse(data);
	if (!obj) {
		/* Not valid JSON, print as-is */
		printf("%s\n", data);
		return;
	}

	int flags = JSON_C_TO_STRING_NOSLASHESCAPE;
	if (pretty) {
		flags |= JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED;
	} else {
		flags |= JSON_C_TO_STRING_PLAIN;
	}

	printf("%s\n", json_object_to_json_string_ext(obj, flags));
	json_object_put(obj);
}

/* Check if any command in the result array failed */
static bool
check_command_success(const char *data)
{
	struct json_object *obj = json_tokener_parse(data);
	if (!obj) {
		return false;
	}

	bool success = true;
	if (json_object_get_type(obj) == json_type_array) {
		int len = json_object_array_length(obj);
		for (int i = 0; i < len; i++) {
			struct json_object *item =
				json_object_array_get_idx(obj, i);
			struct json_object *s = NULL;
			if (json_object_object_get_ex(item, "success", &s)) {
				if (!json_object_get_boolean(s)) {
					success = false;
					/* Print error to stderr */
					struct json_object *err = NULL;
					if (json_object_object_get_ex(item,
							"error", &err)) {
						fprintf(stderr, "Error: %s\n",
							json_object_get_string(
								err));
					}
				}
			}
		}
	} else if (json_object_get_type(obj) == json_type_object) {
		struct json_object *s = NULL;
		if (json_object_object_get_ex(obj, "success", &s)) {
			success = json_object_get_boolean(s);
		}
	}

	json_object_put(obj);
	return success;
}

int
main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IOLBF, 0);

	bool monitor = false;
	bool pretty = false;
	bool raw = false;
	bool quiet = false;
	const char *socket_path = NULL;
	const char *type_str = NULL;
	bool force_pretty = false;

	int c;
	while (1) {
		int index = 0;
		c = getopt_long(argc, argv, "hmpqrs:t:v", long_options, &index);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 'h':
			printf("%s", usage_str);
			return 0;
		case 'm':
			monitor = true;
			break;
		case 'p':
			pretty = true;
			force_pretty = true;
			break;
		case 'q':
			quiet = true;
			break;
		case 'r':
			raw = true;
			break;
		case 's':
			socket_path = optarg;
			break;
		case 't':
			type_str = optarg;
			break;
		case 'v':
			printf("%s\n", version_str);
			return 0;
		default:
			fprintf(stderr, "%s", usage_str);
			return 1;
		}
	}

	/* Auto-detect pretty mode */
	if (!force_pretty && !raw) {
		pretty = isatty(STDOUT_FILENO);
	}

	/* Determine message type */
	int msg_type = parse_msg_type(type_str);
	if (msg_type < 0) {
		return 1;
	}

	/* Build payload from remaining args */
	char *payload = NULL;
	if (optind < argc) {
		/* Concatenate remaining args with spaces */
		size_t total = 0;
		for (int i = optind; i < argc; i++) {
			total += strlen(argv[i]) + 1;
		}
		payload = malloc(total);
		payload[0] = '\0';
		for (int i = optind; i < argc; i++) {
			if (i > optind) {
				strcat(payload, " ");
			}
			strcat(payload, argv[i]);
		}
	}

	/* Get socket path */
	if (!socket_path) {
		socket_path = getenv("LABWC_IPC_SOCK");
	}
	if (!socket_path) {
		fprintf(stderr, "LABWC_IPC_SOCK not set and no -s option\n");
		free(payload);
		return 1;
	}

	int fd = ipc_connect(socket_path);
	if (fd < 0) {
		fprintf(stderr, "Failed to connect to %s\n", socket_path);
		free(payload);
		return 1;
	}

	/* Send message */
	uint32_t payload_len = payload ? strlen(payload) : 0;
	if (!ipc_send(fd, msg_type, payload, payload_len)) {
		fprintf(stderr, "Failed to send IPC message\n");
		close(fd);
		free(payload);
		return 1;
	}
	free(payload);

	/* Receive response */
	uint32_t resp_type = 0;
	char *resp_payload = NULL;
	uint32_t resp_len = 0;

	if (!ipc_recv(fd, &resp_type, &resp_payload, &resp_len)) {
		fprintf(stderr, "Failed to receive IPC response\n");
		close(fd);
		return 1;
	}

	int exit_code = 0;

	if (!quiet && resp_payload) {
		print_json(resp_payload, pretty);
	}

	/* Check for server-reported errors */
	if (resp_payload && msg_type == IPC_RUN_COMMAND) {
		if (!check_command_success(resp_payload)) {
			exit_code = 2;
		}
	}

	/* Monitor mode: keep reading events */
	if (monitor) {
		free(resp_payload);
		while (1) {
			resp_payload = NULL;
			if (!ipc_recv(fd, &resp_type, &resp_payload,
					&resp_len)) {
				break;
			}
			if (!quiet && resp_payload) {
				print_json(resp_payload, pretty);
			}
			free(resp_payload);
		}
	}

	free(resp_payload);
	close(fd);
	return exit_code;
}
