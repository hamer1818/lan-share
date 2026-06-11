"""WebSocket upload smoke test for lan_share C++ port."""
import asyncio
import json
import sys
import time
import websockets
from urllib.parse import quote

BASE = "ws://127.0.0.1:18901/ws/upload?dir=" + quote("/")
CHUNK = 4 * 1024 * 1024  # 4 MB


async def upload(rel_path: str, data: bytes) -> dict:
    async with websockets.connect(BASE, max_size=None) as ws:
        t0 = time.time()
        await ws.send(json.dumps({"path": rel_path, "size": len(data)}))
        sent = 0
        while sent < len(data):
            ws_chunk = data[sent : sent + CHUNK]
            await ws.send(ws_chunk)
            sent += len(ws_chunk)
        resp = await ws.recv()
        elapsed = time.time() - t0
        result = json.loads(resp)
        mb = len(data) / (1024 * 1024)
        speed = mb / max(elapsed, 0.001)
        print(f"  [{elapsed:5.2f}s, {speed:6.1f} MB/s]  {rel_path}  -> {result}")
        return result


async def upload_multi_files_one_conn():
    """Tek bir WS bağlantısı üzerinden ardışık 3 küçük dosya."""
    print("[T4] Tek conn'da 3 dosya")
    async with websockets.connect(BASE, max_size=None) as ws:
        for i in range(3):
            d = (f"file_{i}_content_" + str(i)).encode() * 1000
            await ws.send(json.dumps({"path": f"multi/f_{i}.bin", "size": len(d)}))
            sent = 0
            while sent < len(d):
                await ws.send(d[sent : sent + CHUNK])
                sent += len(d) - sent if (len(d) - sent < CHUNK) else CHUNK
            resp = json.loads(await ws.recv())
            print(f"     #{i}: {resp}")


async def main():
    print("[T1] 500 byte küçük dosya")
    await upload("ws_small.bin", b"x" * 500)

    print("[T2] 10 MB orta dosya")
    await upload("ws_medium.bin", bytes(10 * 1024 * 1024))

    print("[T3] 50 MB nested path (ws_nested/sub/big.bin)")
    await upload("ws_nested/sub/big.bin", bytes(50 * 1024 * 1024))

    await upload_multi_files_one_conn()

    print("[T5] Path traversal denemesi -> base dir ICINDE kalmali")
    await upload("../../evil_ws.bin", b"kotu")


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except Exception as e:
        print(f"HATA: {type(e).__name__}: {e}", file=sys.stderr)
        sys.exit(1)
