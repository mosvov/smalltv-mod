#!/usr/bin/env python3
"""ai-usage-daemon — aggregate Claude, Cursor, and Codex usage for SmallTV."""

from __future__ import annotations

import argparse
import json
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

import httpx

from providers import claude, codex, cursor

CONFIG_PATH = Path(__file__).resolve().parent / "config.json"
DEFAULT_PORT = 8787


def load_config() -> dict:
    cfg = {
        "poll_sec": 60,
        "push_interval_sec": 20,
        "serve_host": "0.0.0.0",
        "serve_port": DEFAULT_PORT,
        "push_targets": [],
        "providers": {"claude": True, "cursor": True, "codex": True},
    }
    if CONFIG_PATH.is_file():
        try:
            cfg.update(json.loads(CONFIG_PATH.read_text()))
        except (OSError, json.JSONDecodeError):
            pass
    return cfg


class State:
    def __init__(self) -> None:
        self.lock = threading.Lock()
        self.payload: dict = {"v": 2, "ok": False}
        self.stop = threading.Event()

    def set(self, payload: dict) -> None:
        with self.lock:
            self.payload = payload

    def get(self) -> dict:
        with self.lock:
            return dict(self.payload)


state = State()


def merge_payload(cfg: dict) -> dict:
    providers_cfg = cfg.get("providers") or {}
    out: dict = {"v": 2, "ok": False}
    any_ok = False

    if providers_cfg.get("claude", True):
        c = claude.fetch()
        out["claude"] = c
        any_ok = any_ok or c.get("ok")

    if providers_cfg.get("cursor", True):
        cu = cursor.fetch()
        out["cursor"] = cu
        any_ok = any_ok or cu.get("ok")

    if providers_cfg.get("codex", True):
        co = codex.fetch()
        out["codex"] = co
        any_ok = any_ok or co.get("ok")

    out["ok"] = any_ok

    # v1 backward compatibility for Claude-only firmware
    cl = out.get("claude") or {}
    if cl.get("ok"):
        out["s"] = cl.get("s", 0)
        out["sr"] = cl.get("sr", 0)
        out["w"] = cl.get("w", 0)
        out["wr"] = cl.get("wr", 0)
        out["st"] = cl.get("st", "unknown")

    return out


def poll_loop(cfg: dict) -> None:
    interval = float(cfg.get("poll_sec") or 60)
    while not state.stop.is_set():
        payload = merge_payload(cfg)
        state.set(payload)
        state.stop.wait(interval)


def push_once(target: str, payload: dict) -> bool:
    url = target.rstrip("/")
    if not url.endswith("/api/usage"):
        url = url + "/api/usage"
    try:
        resp = httpx.post(url, json=payload, timeout=10.0)
        return resp.status_code == 200
    except httpx.HTTPError:
        return False


def discover_smalltvs() -> list[str]:
    try:
        from zeroconf import ServiceBrowser, Zeroconf

        found: list[str] = []

        class Listener:
            def add_service(self, zc, type_, name) -> None:
                info = zc.get_service_info(type_, name)
                if not info:
                    return
                props = {k.decode(): v.decode() for k, v in (info.properties or {}).items()}
                path = props.get("path", "/api/usage")
                host = info.server.rstrip(".")
                port = info.port or 80
                found.append(f"http://{host}:{port}{path}")

            def remove_service(self, *args) -> None:
                pass

            def update_service(self, *args) -> None:
                pass

        zc = Zeroconf()
        browser = ServiceBrowser(zc, "_clawdmeter._tcp.local.", Listener())
        time.sleep(2.5)
        browser.cancel()
        zc.close()
        return found
    except Exception:
        return []


def push_loop(cfg: dict, explicit: list[str] | None) -> None:
    interval = float(cfg.get("push_interval_sec") or 20)
    while not state.stop.is_set():
        payload = state.get()
        targets = list(explicit or [])
        if not targets:
            targets = list(cfg.get("push_targets") or [])
        if not targets:
            targets = discover_smalltvs()
        for t in targets:
            push_once(t, payload)
        state.stop.wait(interval)


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args) -> None:
        pass

    def do_GET(self) -> None:
        if self.path in ("/", "/health"):
            body = json.dumps(state.get()).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        else:
            self.send_error(404)

    def do_HEAD(self) -> None:
        if self.path in ("/", "/health"):
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
        else:
            self.send_error(404)


def run_serve(cfg: dict) -> None:
    host = cfg.get("serve_host") or "0.0.0.0"
    port = int(cfg.get("serve_port") or DEFAULT_PORT)
    poll = threading.Thread(target=poll_loop, args=(cfg,), daemon=True)
    poll.start()
    state.set(merge_payload(cfg))
    server = ThreadingHTTPServer((host, port), Handler)
    print(f"Serving on http://{host}:{port}/", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        state.stop.set()
        server.shutdown()


def run_push(cfg: dict, targets: list[str]) -> None:
    poll = threading.Thread(target=poll_loop, args=(cfg,), daemon=True)
    poll.start()
    state.set(merge_payload(cfg))
    print(f"Pushing every {cfg.get('push_interval_sec', 20)}s to {targets or 'mDNS-discovered devices'}", flush=True)
    try:
        push_loop(cfg, targets or None)
    except KeyboardInterrupt:
        state.stop.set()


def main() -> None:
    ap = argparse.ArgumentParser(description="Aggregate Claude + Cursor + Codex usage for SmallTV")
    ap.add_argument("--serve", action="store_true", help="HTTP server for device pull mode")
    ap.add_argument("--push", action="store_true", help="Push to SmallTVs via mDNS")
    ap.add_argument("--push-to", action="append", default=[], help="Push target host/IP (repeatable)")
    ap.add_argument("--once", action="store_true", help="Fetch once and print JSON")
    ap.add_argument("--port", type=int, help="Serve port override")
    args = ap.parse_args()
    cfg = load_config()
    if args.port:
        cfg["serve_port"] = args.port

    if args.once:
        print(json.dumps(merge_payload(cfg), indent=2))
        return

    if args.serve:
        run_serve(cfg)
        return

    if args.push or args.push_to:
        run_push(cfg, args.push_to)
        return

    # Default: serve
    run_serve(cfg)


if __name__ == "__main__":
    main()
