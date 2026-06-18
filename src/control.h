#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * control_thread_start - Spawn the control plane listener thread.
 *
 * Creates a Unix Domain Socket at CTRL_SOCKET_PATH and starts a POSIX
 * thread that waits for connections from spi_cli.  When a reload request
 * arrives the thread calls matcher_reload() and sends back a status line.
 *
 * This thread runs entirely outside the DPDK lcore pool so it has zero
 * impact on the data-path performance.
 *
 * Returns 0 on success, -1 on failure (socket or thread creation error).
 */
int control_thread_start(void);

/**
 * control_thread_stop - Signal the control thread to exit and clean up.
 *
 * Sets the stop flag, wakes the thread by connecting to its own socket,
 * then joins and removes the socket file.
 */
void control_thread_stop(void);

#ifdef __cplusplus
}
#endif
