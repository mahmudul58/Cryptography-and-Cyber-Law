#!/usr/bin/env python3
"""OSI zig-zag visualizer.

Builds the C programs, starts the server, runs the client, then prints the data
flow two OSI layers at a time: the SENDER (client) goes down L7 -> L1, the data
crosses a real TCP socket, then the RECEIVER (server) goes up L1 -> L7.

Usage:  python3 visualizer.py [--message "text"] [--port 8080] [--delay 0.8]
                              [--name "Your Name"] [--id "Your ID"]
"""
import argparse
import datetime
import os
import re
import shutil
import subprocess
import sys
import time

# Shown in the info box. Change here, or pass --name / --id.
STUDENT_NAME = "Md. Mahmudul Islam Amit"
STUDENT_ID = "IT-23008"

HERE = os.path.dirname(os.path.abspath(__file__))
EVENT = re.compile(r"^\[(TX|RX)\] L(\d) (\S+)\s*\| (.*?) \| (.*)$")
HEADER = re.compile(r"^\w+\{([^|]*)\|")
ANSI = re.compile(r"\033\[[0-9;]*m")
NAMES = {7: "Application", 6: "Presentation", 5: "Session", 4: "Transport",
         3: "Network", 2: "DataLink", 1: "Physical"}
TAGS = {7: "APP", 6: "PRS", 5: "SES", 4: "TCP", 3: "IP", 2: "ETH"}

RESET, BOLD = "\033[0m", "\033[1m"
COLOR = {7: "\033[32m", 6: "\033[36m", 5: "\033[35m", 4: "\033[33m",
         3: "\033[34m", 2: "\033[31m", 1: "\033[37m"}
GREEN, RED = "\033[32m", "\033[31m"
SENDER_BANNER = "\033[1;30;46m"     # black on cyan
RECEIVER_BANNER = "\033[1;30;43m"   # black on yellow
OK_BANNER = "\033[1;97;42m"         # white on green
BAD_BANNER = "\033[1;97;41m"        # white on red


def vis(text):
    """Visible length (ignores colour codes)."""
    return len(ANSI.sub("", text))


def parse(text):
    events = []
    for line in text.splitlines():
        m = EVENT.match(line)
        if m:
            events.append({"dir": m.group(1), "no": int(m.group(2)),
                           "note": m.group(4), "data": m.group(5)})
    return events


def describe(e):
    """Short, readable text for one layer step."""
    no, tag = e["no"], TAGS.get(e["no"])
    hdr = HEADER.match(e["data"])
    info = hdr.group(1) if hdr else ""
    if e["dir"] == "TX":
        if no == 7:
            msg = e["data"][e["data"].index("|") + 1:-1]
            return f'message "{msg}" + {tag} header ({info})'
        if no == 1:
            return f"sending {e['note']}: {e['data']}"
        return f"+ {tag} header ({info})"
    if no == 1:
        return f"{e['note']} from the Sender's Physical layer"
    if no == 7:
        return f'message delivered: "{e["data"]}"'
    if no == 6:
        return f"- {tag} header removed, data decoded"
    if no == 2:
        return f"- {tag} header removed (checksum OK)"
    return f"- {tag} header removed"


def info_box(name, sid, when):
    rows = [f"Name : {name}", f"ID   : {sid}", f"Time : {when}"]
    inner = max(max(len(r) for r in rows) + 2, 30)
    title = "STUDENT INFO".center(inner)
    box = ["┌" + "─" * inner + "┐", "│" + BOLD + title + RESET + "│",
           "├" + "─" * inner + "┤"]
    box += ["│ " + r.ljust(inner - 1) + "│" for r in rows]
    box.append("└" + "─" * inner + "┘")
    return box


def build_rows(events, port, message):
    """Left-hand text. Returns (rows, sleep_after) where sleep_after holds row numbers."""
    rows, sleep_after = [], set()
    sender, receiver = events[:7], events[7:]
    total = 8
    rows.append(f"{BOLD}OSI 7-Layer Simulation{RESET}")
    rows.append(f"Message: {message!r}")

    def section(banner, label, note, groups, first_step):
        rows.append(f"{banner} {label} {RESET}  {note}")
        for i, pair in enumerate(groups):
            title = " → ".join(f"{e['dir']} L{e['no']} {NAMES[e['no']]}" for e in pair)
            rows.append(f"{BOLD}── Step {first_step + i}/{total}: {title}{RESET}")
            for e in pair:
                rows.append(f"  {COLOR[e['no']]}{e['dir']} L{e['no']} {NAMES[e['no']]:<12}{RESET} {describe(e)}")
            sleep_after.add(len(rows) - 1)

    section(SENDER_BANNER, "SENDER (client)  ",
            "encapsulation: data goes DOWN L7 → L1, every layer adds a header",
            [sender[0:2], sender[2:4], sender[4:6], sender[6:7]], 1)
    rows.append(f"{BOLD}  Sender's Physical layer ══════ TCP socket 127.0.0.1:{port} ══════▶ "
                f"Receiver's Physical layer{RESET}")
    sleep_after.add(len(rows) - 1)
    section(RECEIVER_BANNER, "RECEIVER (server)",
            "decapsulation: data goes UP L1 → L7, every layer removes its header",
            [receiver[0:1], receiver[1:3], receiver[3:5], receiver[5:7]], 5)
    return rows, sleep_after


def merge_box(rows, box, cols, top=3, gap=8):
    """Put the box on the right, `top` rows down; if it does not fit, put it above everything."""
    n = len(box)
    rows = rows + [""] * max(0, top + n - len(rows))
    left = max(vis(r) for r in rows[top:top + n])
    for g in (gap, 4):
        if left + g + vis(box[0]) <= cols:
            merged = [r + " " * (left - vis(r) + g) + box[i - top] if top <= i < top + n else r
                      for i, r in enumerate(rows)]
            return merged, 0
    return box + [""] + rows, n + 1


def result_box(sent, delivered, ok):
    color, banner = (GREEN, OK_BANNER) if ok else (RED, BAD_BANNER)
    title = "✔  SUCCESS: message delivered correctly" if ok else "✘  FAILED: delivered message is different"
    lines = [f"Sent      : {sent!r}", f"Delivered : {delivered!r}"]
    inner = max(len(title), *(len(x) for x in lines)) + 4
    out = [f"{color}╔{'═' * inner}╗{RESET}",
           f"{color}║{RESET}{banner}  {title.ljust(inner - 2)}{RESET}{color}║{RESET}"]
    out += [f"{color}║{RESET}  {x.ljust(inner - 2)}{color}║{RESET}" for x in lines]
    out.append(f"{color}╚{'═' * inner}╝{RESET}")
    return out


def main():
    ap = argparse.ArgumentParser(description="OSI 7-layer zig-zag visualizer")
    ap.add_argument("--message", default="Hello OSI Model")
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--delay", type=float, default=0.8, help="seconds between steps")
    ap.add_argument("--name", default=STUDENT_NAME)
    ap.add_argument("--id", default=STUDENT_ID)
    args = ap.parse_args()

    if subprocess.run(["make", "-s"], cwd=HERE).returncode != 0:
        sys.exit("make failed - check that Xcode command line tools (cc, make) are installed.")

    server = subprocess.Popen([os.path.join(HERE, "server"), "--once", str(args.port)],
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    ready = server.stdout.readline()
    if "listening" not in ready:
        rest = server.communicate()[0]
        sys.exit(f"Server did not start: {ready}{rest}\n"
                 f"Try another port, e.g. --port 9090 (check with: lsof -i :{args.port})")

    client = subprocess.run([os.path.join(HERE, "client"), args.message, str(args.port)],
                            capture_output=True, text=True)
    try:
        server_out = server.communicate(timeout=10)[0]
    except subprocess.TimeoutExpired:
        server.kill()
        sys.exit("Server timed out.")

    events = parse(client.stdout) + parse(server_out)
    if client.returncode != 0 or len(events) != 14:
        print("Something went wrong.\n--- client ---\n" + client.stdout + client.stderr +
              "\n--- server ---\n" + server_out)
        sys.exit(1)

    when = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    rows, sleep_after = build_rows(events, args.port, args.message)
    cols = shutil.get_terminal_size((120, 30)).columns
    out, offset = merge_box(rows, info_box(args.name, args.id, when), cols)
    for i, line in enumerate(out):
        print(line)
        if (i - offset) in sleep_after:
            sys.stdout.flush()
            time.sleep(args.delay)

    delivered = events[-1]["data"]
    ok = delivered == args.message
    print()
    print("\n".join(result_box(args.message, delivered, ok)))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
