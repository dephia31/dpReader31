"""Serve simulator assets with the same gzip headers as the public site."""
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from functools import partial
from pathlib import Path
import sys
class Handler(SimpleHTTPRequestHandler):
    def end_headers(self):
        if self.path.split('?')[0].endswith('.data.gz'):
            self.send_header('Content-Encoding','gzip')
            self.send_header('Cache-Control','public, max-age=31536000, immutable')
        super().end_headers()
if __name__=='__main__':
    directory=Path(sys.argv[2]) if len(sys.argv)>2 else Path(__file__).resolve().parent/'dist'
    ThreadingHTTPServer(('127.0.0.1',int(sys.argv[1]) if len(sys.argv)>1 else 8882),partial(Handler,directory=str(directory))).serve_forever()
