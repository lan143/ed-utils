#pragma once

/*
 * Compile-time options for the network logger.
 *
 * Every value below is overridable with a build flag, e.g.
 *   -DLOG_NET_RING_SIZE=32768
 * Keep this header macro-only: FreeRTOS identifiers referenced by defaults are
 * expanded lazily at the use site, never here.
 */

#ifndef LOG_NET_RING_SIZE
#define LOG_NET_RING_SIZE 24576
#endif

#ifndef LOG_NET_CHUNK_SIZE
#define LOG_NET_CHUNK_SIZE 4096
#endif

/* Max ring element size in bytes, LF included, NUL excluded. */
#ifndef LOG_NET_LINE_MAX
#define LOG_NET_LINE_MAX 256
#endif

#ifndef LOG_NET_FLUSH_INTERVAL_MS
#define LOG_NET_FLUSH_INTERVAL_MS 2000
#endif

#ifndef LOG_NET_FLUSH_THRESHOLD
#define LOG_NET_FLUSH_THRESHOLD (LOG_NET_RING_SIZE * 3 / 4)
#endif

#ifndef LOG_NET_URGENT_MIN_GAP_MS
#define LOG_NET_URGENT_MIN_GAP_MS 500
#endif

#ifndef LOG_NET_TASK_STACK
#define LOG_NET_TASK_STACK 6144
#endif

#ifndef LOG_NET_TASK_PRIO
#define LOG_NET_TASK_PRIO (tskIDLE_PRIORITY + 1)
#endif

#ifndef LOG_NET_TASK_CORE
#define LOG_NET_TASK_CORE tskNO_AFFINITY
#endif

#ifndef LOG_NET_CONNECT_TIMEOUT_MS
#define LOG_NET_CONNECT_TIMEOUT_MS 1500
#endif

#ifndef LOG_NET_TIMEOUT_MS
#define LOG_NET_TIMEOUT_MS 3000
#endif

#ifndef LOG_NET_BACKOFF_MIN_MS
#define LOG_NET_BACKOFF_MIN_MS 1000
#endif

#ifndef LOG_NET_BACKOFF_MAX_MS
#define LOG_NET_BACKOFF_MAX_MS 60000
#endif

#ifndef LOG_NET_STRIP_ANSI
#define LOG_NET_STRIP_ANSI 1
#endif

#ifndef LOG_NET_MIN_LEVEL
#define LOG_NET_MIN_LEVEL NET_LOG_VERBOSE
#endif
