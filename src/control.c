#include "control.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/un.h>

#include "common.h"
#include "matcher.h"

/* Maximum length of a config file path sent over the socket */
#define CTRL_PATH_MAX 512
/* Maximum response message length */
#define CTRL_RESP_MAX 128

static pthread_t  g_ctrl_thread;
static int        g_server_fd  = -1;
static volatile int g_stop_flag = 0;

/*
 * ctrl_thread_fn - Main body of the control plane listener thread.
 *
 * Accepts one client connection at a time (CLI is interactive, not
 * high-frequency), reads a config file path, calls matcher_reload(),
 * and replies with a human-readable status message.
 *
 * This function intentionally uses printf/fprintf for control-plane
 * logging — it never runs on a DPDK fast-path lcore.
 */
static void *ctrl_thread_fn(void *arg)
{
	(void)arg;

	while (!g_stop_flag) {
		/* Block until a CLI client connects */
		int client_fd = accept(g_server_fd, NULL, NULL);
		if (client_fd < 0) {
			if (g_stop_flag) break; /* Normal shutdown */
			fprintf(stderr,
				"[control] accept() failed: %s\n",
				strerror(errno));
			continue;
		}

		/* Read the config file path from the client */
		char path[CTRL_PATH_MAX];
		memset(path, 0, sizeof(path));
		ssize_t n = recv(client_fd, path, sizeof(path) - 1, 0);

		char resp[CTRL_RESP_MAX];

		if (n <= 0) {
			snprintf(resp, sizeof(resp),
				 "ERROR: empty or failed recv\n");
		} else {
			/* Strip trailing newline if present */
			path[strcspn(path, "\r\n")] = '\0';

			printf("[control] Reload request: %s\n", path);

			if (matcher_reload(path) == 0) {
				uint32_t cnt = atomic_load_explicit(
					&g_active_num_rules,
					memory_order_relaxed);
				snprintf(resp, sizeof(resp),
					 "OK: loaded %u rules from %s\n",
					 cnt, path);
			} else {
				snprintf(resp, sizeof(resp),
					 "ERROR: failed to load %s\n", path);
			}
		}

		/* Send response — ignore partial-write for simplicity */
		send(client_fd, resp, strlen(resp), 0);
		close(client_fd);
	}

	return NULL;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

int control_thread_start(void)
{
	/* Remove stale socket file from a previous run */
	unlink(CTRL_SOCKET_PATH);

	g_server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (g_server_fd < 0) {
		fprintf(stderr, "[control] socket() failed: %s\n",
			strerror(errno));
		return -1;
	}

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, CTRL_SOCKET_PATH,
		sizeof(addr.sun_path) - 1);

	if (bind(g_server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "[control] bind() failed: %s\n",
			strerror(errno));
		close(g_server_fd);
		g_server_fd = -1;
		return -1;
	}

	if (listen(g_server_fd, 1) < 0) {
		fprintf(stderr, "[control] listen() failed: %s\n",
			strerror(errno));
		close(g_server_fd);
		g_server_fd = -1;
		return -1;
	}

	g_stop_flag = 0;

	if (pthread_create(&g_ctrl_thread, NULL, ctrl_thread_fn, NULL) != 0) {
		fprintf(stderr, "[control] pthread_create() failed: %s\n",
			strerror(errno));
		close(g_server_fd);
		g_server_fd = -1;
		return -1;
	}

	printf("[control] Listening on %s\n", CTRL_SOCKET_PATH);
	return 0;
}

void control_thread_stop(void)
{
	g_stop_flag = 1;

	/*
	 * Wake the blocking accept() by briefly connecting to the socket.
	 * This is the cleanest portable method without using signalfd or
	 * non-blocking I/O on the accept loop.
	 */
	int wake_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (wake_fd >= 0) {
		struct sockaddr_un addr;
		memset(&addr, 0, sizeof(addr));
		addr.sun_family = AF_UNIX;
		strncpy(addr.sun_path, CTRL_SOCKET_PATH,
			sizeof(addr.sun_path) - 1);
		connect(wake_fd, (struct sockaddr *)&addr, sizeof(addr));
		close(wake_fd);
	}

	pthread_join(g_ctrl_thread, NULL);

	if (g_server_fd >= 0) {
		close(g_server_fd);
		g_server_fd = -1;
	}
	unlink(CTRL_SOCKET_PATH);

	printf("[control] Control thread stopped.\n");
}
