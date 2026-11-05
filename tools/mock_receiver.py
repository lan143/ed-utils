#!/usr/bin/env python3
"""Stdlib-only mock of the ESP32 network-logger receiver.

Accepts POST on any path and parses X-BOOT-ID, X-SEQ and X-DROPPED-TOTAL plus
an LF-separated text/plain body. It tracks per-boot sequence gaps/duplicates
so hardware stress scenarios can detect lost or duplicated batches.

Marker lines "[netlog] dropped N records" count as content lines AND add N to
the marker sum. Only accepted (2xx) batches count toward sequence/line stats;
failed POSTs are retried with the same X-SEQ and are not counted as duplicates.

On SIGINT prints a final summary and exits 0 without gaps/duplicates, else 1.
"""

import argparse
import re
import signal
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

STOP = threading.Event()
LOCK = threading.Lock()
BOOTS = {}
ARGS = None
POST_COUNT = 0

MARKER_RE = re.compile(r"^\[netlog\] dropped (\d+) records$")


def boot_state(boot_id):
    if boot_id not in BOOTS:
        BOOTS[boot_id] = {
            "seen": set(), "highest": None, "expected_next": None,
            "gaps": 0, "duplicates": 0, "lines": 0, "marker_lines": 0,
            "marker_sum": 0, "last_dropped_total": None,
        }
    return BOOTS[boot_id]


def as_int(value, default):
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def record_batch(boot_id, seq, raw, dropped_total):
    """Update counters; return (lines, markers, marker_sum, duplicate).

    A retry of an already accepted X-SEQ counts as a duplicate only and its
    lines are not added twice. A missing/malformed X-SEQ still contributes
    lines but cannot take part in gap/duplicate tracking.
    """
    lines = raw.decode("utf-8", "replace").split("\n")
    if lines and lines[-1] == "":
        lines.pop()  # ignore the single trailing empty piece

    marker_lines = 0
    marker_sum = 0
    for line in lines:
        match = MARKER_RE.match(line.rstrip("\r"))
        if match:
            marker_lines += 1
            marker_sum += int(match.group(1))

    with LOCK:
        state = boot_state(boot_id)
        if seq is not None and seq in state["seen"]:
            state["duplicates"] += 1
            return len(lines), marker_lines, marker_sum, True

        state["lines"] += len(lines)
        state["marker_lines"] += marker_lines
        state["marker_sum"] += marker_sum
        state["last_dropped_total"] = dropped_total
        if seq is not None:
            state["seen"].add(seq)
            if state["highest"] is None:
                state["highest"] = seq
            elif seq > state["highest"]:
                state["gaps"] += seq - state["highest"] - 1
                state["highest"] = seq
            state["expected_next"] = state["highest"] + 1

    return len(lines), marker_lines, marker_sum, False


def response_code():
    global POST_COUNT
    with LOCK:
        index = POST_COUNT
        POST_COUNT += 1
    if ARGS.fail_all or index < ARGS.fail_first:
        return 500
    return ARGS.status


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):
        pass  # our own summaries are enough

    def do_GET(self):
        if self.path.split("?", 1)[0] == "/healthz":
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Content-Length", "2")
            self.end_headers()
            self.wfile.write(b"ok")
            return
        self.send_response(404)
        self.send_header("Content-Length", "0")
        self.end_headers()

    def do_POST(self):
        try:
            length = as_int(self.headers.get("Content-Length"), 0)
            raw = self.rfile.read(length) if length > 0 else b""
        except Exception:
            raw = b""  # never crash on a malformed request

        boot_id = self.headers.get("X-BOOT-ID") or "unknown"
        seq = as_int(self.headers.get("X-SEQ"), None)
        dropped_total = as_int(self.headers.get("X-DROPPED-TOTAL"), None)

        if ARGS.hang > 0:
            STOP.wait(ARGS.hang)

        code = response_code()
        accepted = 200 <= code <= 299
        if accepted:
            lines, markers, marker_sum, duplicate = record_batch(
                boot_id, seq, raw, dropped_total
            )
            if not ARGS.quiet:
                print(
                    "boot={} seq={} lines={} marker_sum={} markers={} "
                    "dropped_total={} duplicate={}".format(
                        boot_id, seq if seq is not None else "?",
                        lines, marker_sum, markers,
                        dropped_total if dropped_total is not None else "?",
                        1 if duplicate else 0,
                    ),
                    flush=True,
                )

        try:
            self.send_response(code)
            if code not in (204, 304):
                self.send_header("Content-Length", "0")
            self.end_headers()
        except (BrokenPipeError, ConnectionResetError):
            pass  # the firmware gave up while --hang was sleeping


def print_summary():
    with LOCK:
        total_gaps = sum(s["gaps"] for s in BOOTS.values())
        total_dups = sum(s["duplicates"] for s in BOOTS.values())
        print("=== final summary ===", flush=True)
        if not BOOTS:
            print("no accepted batches", flush=True)
        for boot_id, state in sorted(BOOTS.items()):
            fmt = (
                "boot {}: batches={} highest_seq={} expected_next={} gaps={} "
                "duplicates={} lines={} marker_lines={} marker_sum={} "
                "last_dropped_total={}"
            )
            print(fmt.format(
                boot_id, len(state["seen"]),
                state["highest"] if state["highest"] is not None else "-",
                state["expected_next"] if state["expected_next"] is not None else "-",
                state["gaps"], state["duplicates"], state["lines"],
                state["marker_lines"], state["marker_sum"],
                state["last_dropped_total"]
                if state["last_dropped_total"] is not None else "-",
            ), flush=True)
        print("total gaps={} duplicates={}".format(total_gaps, total_dups), flush=True)
    return 0 if total_gaps == 0 and total_dups == 0 else 1


def handle_signal(signum, frame):
    STOP.set()


def main():
    global ARGS
    parser = argparse.ArgumentParser(
        description="Mock receiver for the EDUtils ESP32 network logger.",
        epilog=(
            "Marker lines '[netlog] dropped N records' count as content lines "
            "and also add N to the marker sum. Only accepted (2xx) batches are "
            "counted toward sequence gaps, duplicates and lines; failed POSTs "
            "are retried with the same X-SEQ and are not counted."
        ),
    )
    parser.add_argument("--port", type=int, default=8080,
                        help="listen port (default 8080)")
    parser.add_argument("--status", type=int, default=204,
                        help="reply code for accepted batches (default 204)")
    parser.add_argument("--fail-first", type=int, default=0, metavar="N",
                        help="reply 500 to the first N POSTs (default 0)")
    parser.add_argument("--fail-all", action="store_true", help="always reply 500")
    parser.add_argument("--hang", type=float, default=0, metavar="SECONDS",
                        help="sleep before replying (unresponsive receiver)")
    parser.add_argument("--quiet", action="store_true",
                        help="suppress per-batch lines")
    ARGS = parser.parse_args()

    signal.signal(signal.SIGINT, handle_signal)
    signal.signal(signal.SIGTERM, handle_signal)

    server = ThreadingHTTPServer(("0.0.0.0", ARGS.port), Handler)
    server.daemon_threads = True
    server.timeout = 0.2
    if not ARGS.quiet:
        print("listening on http://0.0.0.0:{} status={} fail_first={} "
              "fail_all={} hang={}s".format(ARGS.port, ARGS.status,
                                             ARGS.fail_first, ARGS.fail_all,
                                             ARGS.hang), flush=True)

    while not STOP.is_set():
        server.handle_request()
    server.server_close()
    return print_summary()


if __name__ == "__main__":
    sys.exit(main())
