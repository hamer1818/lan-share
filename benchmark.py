"""Python vs C++ lan_share benchmark.

Three scenarios:
1. Single mid file via WebSocket  (4 MB)
2. Single large file via WebSocket (256 MB)
3. Bulk: 5000 x 1 KB via single WS connection
4. Large file download via HTTP GET
"""
from __future__ import annotations

import asyncio
import json
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from urllib.parse import quote

import httpx
import websockets

ROOT = Path(__file__).parent.resolve()
CPP_EXE  = ROOT / "build" / "bin" / "lan_share.exe"
PY_MAIN  = ROOT.parent / "main.py"
PY_PORT  = 8001
CPP_PORT = 8002
SMALL_SZ = 4 * 1024 * 1024            # 4 MB
LARGE_SZ = 256 * 1024 * 1024          # 256 MB
BULK_N   = 5000
BULK_SZ  = 1024                       # 1 KB


# ── helpers ────────────────────────────────────────────────────────────

def start_server(cmd: list[str]) -> subprocess.Popen:
    p = subprocess.Popen(
        cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        cwd=ROOT)
    # Sunucu portu acana kadar bekle
    for _ in range(40):
        time.sleep(0.1)
        try:
            with httpx.Client(timeout=0.3) as c:
                if c.get(f"http://127.0.0.1:{cmd[cmd.index('-p')+1]}/ping").status_code == 200:
                    return p
        except Exception:
            pass
    return p


def stop_server(p: subprocess.Popen) -> None:
    try:
        p.terminate()
        p.wait(timeout=3)
    except Exception:
        p.kill()


async def ws_upload(port: int, rel: str, path: Path) -> tuple[int, float]:
    size = path.stat().st_size
    url = f"ws://127.0.0.1:{port}/ws/upload?dir=" + quote("/")
    t0 = time.perf_counter()
    async with websockets.connect(url, max_size=None,
                                    max_queue=64) as ws:
        await ws.send(json.dumps({"path": rel, "size": size}))
        with open(path, "rb") as f:
            while True:
                c = f.read(4 * 1024 * 1024)
                if not c:
                    break
                await ws.send(c)
        resp = json.loads(await ws.recv())
        assert resp.get("ok"), resp
    return size, time.perf_counter() - t0


async def ws_bulk(port: int, count: int, sz: int) -> tuple[int, float]:
    url = f"ws://127.0.0.1:{port}/ws/upload?dir=" + quote("/")
    payload = b"x" * sz
    t0 = time.perf_counter()
    async with websockets.connect(url, max_size=None,
                                    max_queue=2048) as ws:
        for i in range(count):
            await ws.send(json.dumps({"path": f"bulk/f_{i:05d}.bin",
                                        "size": sz}))
            await ws.send(payload)
            resp = json.loads(await ws.recv())
            if not resp.get("ok"):
                raise RuntimeError(resp)
    return count, time.perf_counter() - t0


def http_download(port: int, rel: str) -> tuple[int, float]:
    url = f"http://127.0.0.1:{port}/{quote(rel)}"
    t0 = time.perf_counter()
    total = 0
    with httpx.stream("GET", url, timeout=60) as r:
        r.raise_for_status()
        for chunk in r.iter_bytes(chunk_size=4 * 1024 * 1024):
            total += len(chunk)
    return total, time.perf_counter() - t0


def mb(b: int) -> str:
    return f"{b / (1024 * 1024):8.1f} MB"


def speed(b: int, dt: float) -> str:
    return f"{(b / dt) / (1024 * 1024):7.1f} MB/s"


# ── one server benchmark ──────────────────────────────────────────────

async def run_against(label: str, port: int, small: Path, large: Path
                       ) -> dict[str, tuple[float, float]]:
    print(f"\n=== {label} (port {port}) ===")
    results: dict[str, tuple[float, float]] = {}

    s, dt = await ws_upload(port, "ws_small.bin", small)
    print(f"  WS upload    {mb(s)}  in {dt:6.3f}s -> {speed(s, dt)}")
    results["ws_small"] = (s, dt)

    s, dt = await ws_upload(port, "ws_large.bin", large)
    print(f"  WS upload    {mb(s)}  in {dt:6.3f}s -> {speed(s, dt)}")
    results["ws_large"] = (s, dt)

    n, dt = await ws_bulk(port, BULK_N, BULK_SZ)
    print(f"  Bulk WS      {n:>5} dosya x {BULK_SZ}B in {dt:6.3f}s -> "
          f"{n/dt:6.0f} dosya/s")
    results["bulk"] = (n, dt)

    # Download (sunucu kendi yazdigi ws_large.bin'i sun)
    t, dt = http_download(port, "ws_large.bin")
    print(f"  HTTP downld  {mb(t)}  in {dt:6.3f}s -> {speed(t, dt)}")
    results["download"] = (t, dt)

    return results


# ── main ──────────────────────────────────────────────────────────────

async def main() -> None:
    print(f"C++  exe : {CPP_EXE}  (var: {CPP_EXE.exists()})")
    print(f"Python   : {PY_MAIN}  (var: {PY_MAIN.exists()})")

    # Test dosyalari
    tmp = Path(tempfile.mkdtemp(prefix="lan_bench_"))
    small = tmp / "small.bin"
    large = tmp / "large.bin"

    print(f"\nDosya hazirligi (tmp: {tmp})")
    with open(small, "wb") as f:
        f.write(b"x" * SMALL_SZ)
    with open(large, "wb") as f:
        chunk = b"x" * (8 * 1024 * 1024)
        for _ in range(LARGE_SZ // len(chunk)):
            f.write(chunk)
    print(f"  small.bin  = {small.stat().st_size:,} byte")
    print(f"  large.bin  = {large.stat().st_size:,} byte")

    # ─── Python ───
    share_py = Path(tempfile.mkdtemp(prefix="bench_share_py_"))
    py = start_server([sys.executable, str(PY_MAIN),
                        "-p", str(PY_PORT), "-d", str(share_py)])
    py_res = {}
    try:
        py_res = await run_against("Python (http.server)", PY_PORT, small, large)
    finally:
        stop_server(py)

    # ─── C++ ───
    share_cpp = Path(tempfile.mkdtemp(prefix="bench_share_cpp_"))
    cpp = start_server([str(CPP_EXE),
                          "-p", str(CPP_PORT), "-d", str(share_cpp)])
    cpp_res = {}
    try:
        cpp_res = await run_against("C++ (Crow)", CPP_PORT, small, large)
    finally:
        stop_server(cpp)

    # ─── Karsilastirma tablosu ───
    print("\n" + "=" * 66)
    print(f"{'Senaryo':<22}  {'Python':>14}  {'C++':>14}  {'Hizlanma':>10}")
    print("-" * 66)

    def row(label: str, key: str, unit: str) -> None:
        if key not in py_res or key not in cpp_res:
            return
        py_v = py_res[key][0] / py_res[key][1]
        cpp_v = cpp_res[key][0] / cpp_res[key][1]
        if unit == "MB/s":
            py_s = f"{py_v / (1024*1024):8.1f} {unit}"
            cpp_s = f"{cpp_v / (1024*1024):8.1f} {unit}"
        else:
            py_s = f"{py_v:8.0f} {unit}"
            cpp_s = f"{cpp_v:8.0f} {unit}"
        speedup = cpp_v / py_v if py_v > 0 else 0
        print(f"{label:<22}  {py_s:>14}  {cpp_s:>14}  {speedup:>8.2f}x")

    row("WS 4 MB upload",   "ws_small", "MB/s")
    row("WS 256 MB upload", "ws_large", "MB/s")
    row("WS bulk 5K x 1KB", "bulk",     "f/s ")
    row("HTTP 256 MB downl","download", "MB/s")
    print("=" * 66)


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except Exception as e:
        print(f"\nHATA: {type(e).__name__}: {e}", file=sys.stderr)
        raise
