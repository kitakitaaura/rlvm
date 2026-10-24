#!/usr/bin/env python3
# Static dev server for web/ that disables browser caching, so rebuilt
# rlvm.wasm/rlvm.js and edited app.js are always picked up on reload.
import functools
import http.server
import sys


class NoCacheHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    directory = sys.argv[2] if len(sys.argv) > 2 else "web"
    handler = functools.partial(NoCacheHandler, directory=directory)
    http.server.ThreadingHTTPServer(("", port), handler).serve_forever()


if __name__ == "__main__":
    main()
