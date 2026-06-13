/*
 * spi_cli.c — CLI tool for the SPIFast control plane.
 *
 * Usage:
 *   ./build/spi_cli reload_rules <path-to-conf-file>
 *
 * Connects to the Unix Domain Socket created by the running spifast
 * application, sends the config file path, and prints the server's
 * response.  No DPDK dependency.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/un.h>

/* Keep in sync with common.h — spi_cli intentionally has no DPDK dep */
#define CTRL_SOCKET_PATH "/tmp/spifast_ctrl.sock"
#define RESP_BUF_SIZE    512

static void print_usage(const char *prog)
{
	fprintf(stderr, "Usage: %s reload_rules <config-file>\n", prog);
	fprintf(stderr, "  Connects to the running spifast instance and\n");
	fprintf(stderr, "  triggers a hot-reload of the given rule file.\n");
}

int main(int argc, char **argv)
{
	if (argc != 3 || strcmp(argv[1], "reload_rules") != 0) {
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	const char *conf_path = argv[2];

	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		fprintf(stderr, "socket() failed: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, CTRL_SOCKET_PATH,
		sizeof(addr.sun_path) - 1);

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr,
			"Cannot connect to spifast (%s): %s\n"
			"Is spifast running?\n",
			CTRL_SOCKET_PATH, strerror(errno));
		close(fd);
		return EXIT_FAILURE;
	}

	/* Send the config file path (no null terminator needed) */
	if (send(fd, conf_path, strlen(conf_path), 0) < 0) {
		fprintf(stderr, "send() failed: %s\n", strerror(errno));
		close(fd);
		return EXIT_FAILURE;
	}

	/* Read and print the server response */
	char resp[RESP_BUF_SIZE];
	memset(resp, 0, sizeof(resp));
	ssize_t n = recv(fd, resp, sizeof(resp) - 1, 0);
	if (n > 0)
		printf("%s", resp);
	else
		fprintf(stderr, "No response from spifast.\n");

	close(fd);

	/* Exit with failure if the response starts with "ERROR" */
	return (strncmp(resp, "ERROR", 5) == 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}
