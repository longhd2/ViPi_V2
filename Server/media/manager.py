import asyncio
import logging
import secrets
import socket
from pathlib import Path
from urllib.parse import urlencode

log = logging.getLogger("ViPiV2.Media")


class MediaProxy:
    def __init__(self, port=8088):
        self.port = port
        self.items = {}

    def register(self, path):
        token = secrets.token_urlsafe(12)
        self.items[token] = str(path)
        return token

    def local_ip(self, peer):
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            s.connect((peer, 9))
            return s.getsockname()[0]
        finally:
            s.close()

    async def run(self):
        server = await asyncio.start_server(
            self.handle, "0.0.0.0", self.port
        )

        log.info("MEDIA PROXY listening :%d", self.port)

        async with server:
            await server.serve_forever()

    async def handle(self, reader, writer):
        try:
            line = await asyncio.wait_for(reader.readline(), 5)
            parts = line.decode("latin1", "ignore").split()

            if len(parts) < 2:
                return

            method = parts[0]
            request_path = parts[1].split("?", 1)[0]

            headers = {}

            while True:
                line = await reader.readline()

                if line in (b"", b"\r\n", b"\n"):
                    break

                k, _, v = line.decode(
                    "latin1", "ignore"
                ).partition(":")

                if k:
                    headers[k.strip().lower()] = v.strip()

            token = request_path.removeprefix("/media/")
            filename = self.items.get(token)

            if not filename:
                writer.write(
                    b"HTTP/1.1 404 Not Found\r\n"
                    b"Connection: close\r\n\r\n"
                )
                await writer.drain()
                return

            path = Path(filename)

            if not path.is_file():
                writer.write(
                    b"HTTP/1.1 404 Not Found\r\n"
                    b"Connection: close\r\n\r\n"
                )
                await writer.drain()
                return

            size = path.stat().st_size
            start = 0
            end = size - 1
            partial = False

            r = headers.get("range", "")

            if r.startswith("bytes="):
                spec = r[6:].split(",", 1)[0]
                a, _, b = spec.partition("-")

                if a:
                    start = int(a)

                if b:
                    end = min(int(b), size - 1)

                partial = True

            length = end - start + 1

            if partial:
                writer.write(
                    b"HTTP/1.1 206 Partial Content\r\n"
                )
            else:
                writer.write(b"HTTP/1.1 200 OK\r\n")

            writer.write(b"Content-Type: audio/mp4\r\n")
            writer.write(
                f"Content-Length: {length}\r\n".encode()
            )
            writer.write(b"Accept-Ranges: bytes\r\n")

            if partial:
                writer.write(
                    f"Content-Range: bytes "
                    f"{start}-{end}/{size}\r\n".encode()
                )

            writer.write(b"Connection: close\r\n\r\n")
            await writer.drain()

            if method != "HEAD":
                with path.open("rb") as f:
                    f.seek(start)
                    remain = length

                    while remain:
                        chunk = f.read(min(16384, remain))

                        if not chunk:
                            break

                        writer.write(chunk)
                        await writer.drain()
                        remain -= len(chunk)

        except (ConnectionResetError, BrokenPipeError):
            # ESP32/media player closed the HTTP stream intentionally,
            # e.g. playback stopped or audio pipeline switched to voice.
            log.info("MEDIA PROXY client disconnected")

        except Exception:
            log.exception("MEDIA PROXY ERROR")

        finally:
            try:
                writer.close()
                await writer.wait_closed()
            except Exception:
                pass


class MediaManager:
    def __init__(self):
        self.proxy = MediaProxy()

        self.root = Path(__file__).resolve().parents[1]
        self.cache = self.root / "cache" / "media"
        self.cache.mkdir(parents=True, exist_ok=True)

        self.ytdlp = "/home/server/onju-v2/env/bin/yt-dlp"

    async def post(self, ip, path, data=None):
        body = urlencode(data or {}).encode()

        reader, writer = await asyncio.open_connection(ip, 80)

        writer.write(
            (
                f"POST {path} HTTP/1.1\r\n"
                f"Host: {ip}\r\n"
                f"Content-Type: application/x-www-form-urlencoded\r\n"
                f"Content-Length: {len(body)}\r\n"
                f"Connection: close\r\n\r\n"
            ).encode() + body
        )

        await writer.drain()
        status = await reader.readline()

        writer.close()
        await writer.wait_closed()

        return status

    async def search(self, query):
        log.info('MEDIA SEARCH "%s"', query)

        p = await asyncio.create_subprocess_exec(
            self.ytdlp,
            "--quiet",
            "--no-warnings",
            "--no-playlist",
            "--print", "%(webpage_url)s",
            f"ytsearch1:{query}",
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE
        )

        out, err = await asyncio.wait_for(
            p.communicate(), 60
        )

        if p.returncode:
            raise RuntimeError(
                err.decode(errors="ignore")
            )

        urls = [
            x.strip()
            for x in out.decode(
                errors="ignore"
            ).splitlines()
            if x.strip()
        ]

        if not urls:
            raise RuntimeError(
                "Không tìm thấy bài hát"
            )

        log.info(
            "MEDIA SEARCH result %s",
            urls[0]
        )

        return urls[0]

    async def download(self, url):
        template = str(
            self.cache / "%(id)s.%(ext)s"
        )

        fmt = (
            "bestaudio[ext=m4a]"
            "[acodec^=mp4a]/"
            "bestaudio[ext=m4a]"
        )

        log.info("MEDIA DOWNLOAD M4A")

        p = await asyncio.create_subprocess_exec(
            self.ytdlp,
            "--quiet",
            "--no-warnings",
            "--no-playlist",
            "--fixup", "force",
            "-f", fmt,
            "-o", template,
            "--print", "after_move:filepath",
            url,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE
        )

        out, err = await asyncio.wait_for(
            p.communicate(), 120
        )

        if p.returncode:
            raise RuntimeError(
                err.decode(errors="ignore")
            )

        paths = [
            x.strip()
            for x in out.decode(
                errors="ignore"
            ).splitlines()
            if x.strip()
        ]

        if not paths:
            raise RuntimeError(
                "Không có file M4A"
            )

        path = Path(paths[-1]).resolve()

        if not path.is_file():
            raise RuntimeError(
                f"File không tồn tại: {path}"
            )

        log.info(
            "MEDIA DOWNLOAD OK %s %d bytes",
            path.name,
            path.stat().st_size
        )

        return path

    async def play(self, query, ctx):
        ip = ctx.metadata.get("ip")

        if not ip:
            raise RuntimeError(
                "ESP32 chưa có IP"
            )

        url = await self.search(query)
        path = await self.download(url)

        token = self.proxy.register(path)

        play_url = (
            f"http://"
            f"{self.proxy.local_ip(ip)}:"
            f"{self.proxy.port}/media/{token}"
        )

        log.info(
            "MEDIA PROXY URL %s",
            play_url
        )

        status = await self.post(
            ip,
            "/music/play",
            {
                "url": play_url,
                "format": "m4a"
            }
        )

        if b" 200 " not in status:
            raise RuntimeError(
                f"ESP32 HTTP {status!r}"
            )

        log.info(
            'MEDIA PLAY [%s] "%s"',
            ctx.device_id,
            query
        )

    async def stop(self, ctx):
        ip = ctx.metadata.get("ip")

        if ip:
            await self.post(
                ip,
                "/music/stop"
            )

        log.info(
            "MEDIA STOP [%s]",
            ctx.device_id
        )


media_manager = MediaManager()
