#pragma once
#include <string_view>
#include <cstdint>

// FAVICON_SVG — Python main.py'deki ile birebir.
inline constexpr std::string_view FAVICON_SVG = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
<rect width="100" height="100" rx="20" fill="#6366f1"/>
<text x="50" y="72" font-size="60" text-anchor="middle" fill="white">&#9889;</text>
</svg>)SVG";

// PAGE_TEMPLATE — Python main.py'deki ile birebir.
// Python str.format() konvansiyonu: {name} placeholder, {{ ve }} literal { ve } kacisi.
// render_template() (main.cpp icinde) tam ayni semantik ile islem yapar.
inline constexpr std::string_view PAGE_TEMPLATE = R"HTML(<!DOCTYPE html>
<html lang="tr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{title}</title>
<link rel="icon" href="/favicon.ico" type="image/svg+xml">
<style>
  @import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap');
  * {{ box-sizing: border-box; margin: 0; padding: 0; }}
  :root {{
    --bg: #0a0e1a;
    --card: rgba(15,23,42,0.6);
    --hover: rgba(30,41,59,0.8);
    --border: rgba(99,102,241,0.15);
    --text1: #f1f5f9;
    --text2: #94a3b8;
    --text3: #475569;
    --accent: #6366f1;
    --accent2: #818cf8;
    --glow: rgba(99,102,241,0.25);
    --ok: #22c55e;
    --err: #ef4444;
    --warn: #f59e0b;
  }}
  body {{
    font-family: 'Inter',-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;
    background: var(--bg); color: var(--text1);
    min-height: 100vh; padding: 24px 16px;
    background-image:
      radial-gradient(ellipse 80% 50% at 50% -20%, rgba(99,102,241,0.12), transparent),
      radial-gradient(ellipse 60% 40% at 80% 100%, rgba(124,58,237,0.08), transparent);
  }}
  .c {{ max-width: 860px; margin: 0 auto; animation: fadeUp .4s ease-out; }}
  @keyframes fadeUp {{ from {{ opacity:0; transform:translateY(16px); }} to {{ opacity:1; transform:translateY(0); }} }}

  .hdr {{ display:flex; align-items:center; gap:14px; margin-bottom:6px; }}
  .hdr-ico {{ width:44px; height:44px; background:linear-gradient(135deg,#6366f1,#8b5cf6);
    border-radius:12px; display:flex; align-items:center; justify-content:center;
    font-size:1.4rem; box-shadow:0 4px 20px var(--glow); flex-shrink:0; }}
  h1 {{ font-size:1.35rem; font-weight:700; letter-spacing:-0.02em;
    background:linear-gradient(135deg,#f1f5f9,#94a3b8);
    -webkit-background-clip:text; -webkit-text-fill-color:transparent; background-clip:text; }}
  .path {{ font-size:.78rem; color:var(--text3); margin-bottom:6px;
    font-family:'Cascadia Code','Fira Code',monospace; word-break:break-all; padding-left:58px; }}
  .apphint {{ font-size:.75rem; color:var(--text3); padding-left:58px; margin-bottom:24px; }}
  .apphint a {{ color:#7dd3fc; text-decoration:none; }}
  .apphint a:hover {{ color:#38bdf8; }}
  .ft a {{ color:#7dd3fc; text-decoration:none; }}
  .ft a:hover {{ color:#38bdf8; }}

  .ubox {{ border:2px dashed var(--border); border-radius:16px; padding:28px 20px;
    text-align:center; transition:all .3s cubic-bezier(.4,0,.2,1);
    cursor:pointer; backdrop-filter:blur(12px); background:var(--card); position:relative; overflow:hidden; }}
  .ubox::before {{ content:''; position:absolute; inset:0;
    background:radial-gradient(circle at 50% 50%,var(--glow),transparent 70%);
    opacity:0; transition:opacity .3s; pointer-events:none; }}
  .ubox:hover,.ubox.drag {{ border-color:var(--accent); box-shadow:0 0 30px var(--glow); transform:translateY(-2px); }}
  .ubox:hover::before,.ubox.drag::before {{ opacity:1; }}
  .ubox .uico {{ font-size:2rem; margin-bottom:8px; display:block; }}
  .ubox p {{ color:var(--text2); font-size:.9rem; margin-bottom:14px; position:relative; }}
  .ubox input {{ display:none; }}

  .btn-row {{ display:flex; gap:10px; justify-content:center; flex-wrap:wrap; }}
  .btn {{ display:inline-flex; align-items:center; gap:6px; padding:10px 24px;
    border-radius:10px; background:linear-gradient(135deg,#6366f1,#7c3aed);
    color:#fff; font-size:.85rem; font-weight:600; border:none; cursor:pointer;
    transition:all .2s; text-decoration:none; position:relative;
    box-shadow:0 2px 12px var(--glow); }}
  .btn:hover {{ background:linear-gradient(135deg,#4f46e5,#6d28d9);
    box-shadow:0 4px 20px var(--glow); transform:translateY(-1px); }}
  .btn-folder {{ background:linear-gradient(135deg,#0ea5e9,#7c3aed); }}
  .btn-folder:hover {{ background:linear-gradient(135deg,#0284c7,#6d28d9); }}

  /* Top Dashboard / Progress */
  #dashboard {{ margin-bottom:24px; display:none; background:var(--card); border:1px solid var(--border); border-radius:16px; padding:20px; box-shadow:0 10px 30px rgba(0,0,0,0.2); }}
  #dashboard.active {{ display:block; animation:fadeUp .3s ease-out; }}
  #overall {{ margin-bottom:16px; }}
  .ov-bar {{ height:8px; background:rgba(99,102,241,0.15); border-radius:4px; overflow:hidden; }}
  .ov-fill {{ height:100%; background:linear-gradient(90deg,#6366f1,#22c55e); border-radius:4px;
    width:0%; transition:width .15s linear; box-shadow:0 0 10px rgba(99,102,241,0.5); }}
  .ov-text {{ display:flex; justify-content:space-between; margin-top:8px;
    font-size:.85rem; color:var(--text1); font-weight:500; }}
  .ov-subtext {{ font-size:.75rem; color:var(--text2); display:flex; justify-content:space-between; margin-top:4px; }}
  #status {{ font-size:.82rem; min-height:20px; color:var(--text2); text-align:center; margin-bottom:12px; }}
  #status.ok {{ color:var(--ok); }} #status.err {{ color:var(--err); }}

  /* Queue */
  #queue {{ display:flex; flex-direction:column; gap:8px; }}
  .q-item {{ background:rgba(15,23,42,0.8); border:1px solid var(--border);
    border-radius:10px; padding:10px 14px;
    animation:fadeUp .25s ease-out; }}
  .q-top {{ display:flex; justify-content:space-between; align-items:center; margin-bottom:6px; }}
  .q-name {{ font-size:.82rem; font-weight:500; color:var(--text1);
    white-space:nowrap; overflow:hidden; text-overflow:ellipsis; max-width:60%; }}
  .q-path {{ font-size:.7rem; color:var(--text3); font-family:'Cascadia Code',monospace;
    white-space:nowrap; overflow:hidden; text-overflow:ellipsis; max-width:100%;
    margin-bottom:3px; }}
  .q-info {{ font-size:.72rem; color:var(--text2); font-family:'Cascadia Code',monospace; }}
  .q-dest {{ font-size:.7rem; color:var(--text2); font-family:'Cascadia Code',monospace;
    white-space:nowrap; overflow:hidden; text-overflow:ellipsis; max-width:100%;
    margin:2px 0 6px; }}
  .q-dest b {{ color:var(--accent2); font-weight:600; }}
  .q-bar {{ height:4px; background:rgba(99,102,241,0.15); border-radius:2px; overflow:hidden; }}
  .q-fill {{ height:100%; border-radius:2px; width:0%; transition:width .1s linear; }}
  .q-fill.uploading {{ background:linear-gradient(90deg,#6366f1,#a78bfa); }}
  .q-fill.done {{ background:var(--ok); width:100%!important; }}
  .q-fill.error {{ background:var(--err); }}
  .q-fill.waiting {{ background:var(--text3); width:0%; }}
  .q-fill.skipped {{ background:var(--warn); width:100%!important; }}

  /* File list */
  .fl {{ list-style:none; margin-top:24px; }}
  .fl li {{ display:flex; align-items:center; gap:12px; padding:12px 16px;
    border-radius:12px; transition:all .15s ease; position:relative;
    animation:slideIn .3s ease-out backwards; }}
  .fl li:hover {{ background:var(--hover); transform:translateX(4px); }}
  .fl li+li {{ border-top:1px solid rgba(30,41,59,0.6); }}
  @keyframes slideIn {{ from {{ opacity:0; transform:translateX(-8px); }} to {{ opacity:1; transform:translateX(0); }} }}
  .ico {{ font-size:1.25rem; flex-shrink:0; width:36px; height:36px; display:flex;
    align-items:center; justify-content:center; background:var(--card);
    border-radius:8px; border:1px solid var(--border); }}
  .nm {{ flex:1; min-width:0; }}
  .nm a {{ color:#a5b4fc; text-decoration:none; word-break:break-all; font-weight:500;
    font-size:.92rem; transition:color .15s; }}
  .nm a:hover {{ color:var(--accent2); }}
  .nm .dl {{ color:#7dd3fc; }} .nm .dl:hover {{ color:#38bdf8; }}
  .mt {{ font-size:.73rem; color:var(--text3); flex-shrink:0; text-align:right;
    font-family:'Cascadia Code','Fira Code',monospace; line-height:1.5; }}
  .wsdl {{ flex-shrink:0; background:var(--card); border:1px solid var(--border);
    color:#7dd3fc; border-radius:8px; padding:6px 11px; cursor:pointer;
    font-size:.95rem; line-height:1; transition:all .15s; }}
  .wsdl:hover {{ border-color:var(--accent); color:#38bdf8;
    background:var(--hover); box-shadow:0 0 12px var(--glow); }}
  .q-fill.downloading {{ background:linear-gradient(90deg,#0ea5e9,#22c55e); }}
  .empty {{ color:var(--text3); text-align:center; padding:48px 20px; font-size:.9rem; }}
  .empty-ico {{ font-size:2.5rem; margin-bottom:12px; display:block; opacity:.5; }}

  .ft {{ margin-top:40px; text-align:center; color:var(--text3); font-size:.72rem; padding-bottom:20px; }}

  ::-webkit-scrollbar {{ width:6px; }}
  ::-webkit-scrollbar-track {{ background:transparent; }}
  ::-webkit-scrollbar-thumb {{ background:var(--border); border-radius:3px; }}
</style>
</head>
<body>
<div class="c">
  <div class="hdr">
    <div class="hdr-ico">&#9889;</div>
    <h1>{title}</h1>
  </div>
  <div class="path">{rel_path}</div>
  <div class="apphint">&#128161; Klas&ouml;rleri tam h&iacute;zda, yap&iacute;s&iacute;yla indirmek i&ccedil;in:
    <a href="/lan_share.exe">uygulamay&iacute; indir</a>
    &middot; a&ccedil; &rarr; "Al (Indir)" sekmesi &rarr; Tara &rarr; bu cihaza ba&#287;lan</div>

  <div id="dashboard">
    <div id="overall">
      <div class="ov-text">
        <span id="ov-label">Hazırlanıyor...</span>
        <span id="ov-speed">0 MB/s</span>
      </div>
      <div class="ov-bar" style="margin: 8px 0;"><div class="ov-fill" id="ov-fill"></div></div>
      <div class="ov-subtext">
        <span id="ov-counts">0 / 0 dosya</span>
        <span id="ov-time">Kalan: hesaplanıyor</span>
      </div>
    </div>
    <div id="status"></div>
    <div id="queue"></div>
  </div>

  <div class="ubox" id="dz">
    <span class="uico">&#9729;&#65039;</span>
    <p>Dosya veya klas&ouml;r&uuml; s&uuml;r&uuml;kle-b&iacute;rak &nbsp;<strong>veya</strong></p>
    <div class="btn-row">
      <label class="btn">
        &#128206; Dosya Se&ccedil;
        <input type="file" id="fi" multiple>
      </label>
      <label class="btn btn-folder">
        &#128193; Klas&ouml;r Se&ccedil;
        <input type="file" id="fi-dir" multiple webkitdirectory>
      </label>
    </div>
  </div>

  <ul class="fl">
{entries}
  </ul>
  <div class="ft">LAN Dosya Paylasimi &mdash; {server_info} &mdash;
    <a href="/lan_share.exe">&#128190; Uygulamay&iacute; &Iacute;ndir</a></div>
</div>

<script>
const PARALLEL  = {max_parallel};
const UP_URL    = "{upload_url}";
const dz        = document.getElementById("dz");
const fi        = document.getElementById("fi");
const fiDir     = document.getElementById("fi-dir");
const queue     = document.getElementById("queue");
const status    = document.getElementById("status");
const dashboard = document.getElementById("dashboard");
const ovFill    = document.getElementById("ov-fill");
const ovLabel   = document.getElementById("ov-label");
const ovSpeed   = document.getElementById("ov-speed");
const ovCounts  = document.getElementById("ov-counts");
const ovTime    = document.getElementById("ov-time");
const WS_CHUNK  = 4 * 1024 * 1024;
const HYBRID_THRESHOLD = 2 * 1024 * 1024;

document.querySelectorAll('.fl li').forEach((li,i) => {{
  li.style.animationDelay = `${{Math.min(i*30,600)}}ms`;
}});

dz.addEventListener("click", e => {{
  if (!e.target.closest("label") && !e.target.closest("input")) fi.click();
}});
["dragenter","dragover"].forEach(ev => dz.addEventListener(ev, e => {{
  e.preventDefault(); dz.classList.add("drag");
}}));
["dragleave","drop"].forEach(ev => dz.addEventListener(ev, e => {{
  e.preventDefault(); dz.classList.remove("drag");
}}));

function fmtB(b) {{
  if (b === 0) return '0 B';
  const u = ['B','KB','MB','GB','TB'];
  const i = Math.floor(Math.log(b) / Math.log(1024));
  return parseFloat((b / Math.pow(1024, i)).toFixed(1)) + ' ' + u[i];
}}
function fmtTime(s) {{
  if (s < 60) return s.toFixed(0) + 's';
  if (s < 3600) return Math.floor(s/60) + 'm ' + Math.floor(s%60) + 's';
  return Math.floor(s/3600) + 'h ' + Math.floor((s%3600)/60) + 'm';
}}

function getOrCreateEngine() {{
  if (window._UPL_ENGINE) return window._UPL_ENGINE;
  const eng = {{
    pending: [], active: new Map(), errorFiles: 0, skippedFiles: 0,
    uploadedFiles: 0, totalFiles: 0,
    uploadedSize: 0, totalSize: 0,
    workersRunning: 0, hasError: false,
    startT: Date.now(), discoveryDone: false, discoveryRafPending: false
  }};
  window._UPL_ENGINE = eng;
  dashboard.classList.add("active");
  dz.style.display = "none";
  return eng;
}}

function finishUpload(eng) {{
  if (eng.hasError) return;
  status.textContent = 'Tum dosyalar tamamlandi.';
  status.className = 'ok';
  ovFill.style.width = '100%';
  ovLabel.textContent = `${{fmtB(eng.totalSize)}} / ${{fmtB(eng.totalSize)}}`;
  ovSpeed.textContent = 'Tamamlandi';
  ovTime.textContent = '0s';
  ovCounts.textContent = `${{eng.uploadedFiles}} yuklendi` + (eng.skippedFiles ? ` (${{eng.skippedFiles}} atlandi)` : '');
  setTimeout(() => {{ dz.style.display = "block"; }}, 2000);
}}

function scheduleDiscoveryUpdate(eng) {{
  if (eng.discoveryRafPending) return;
  eng.discoveryRafPending = true;
  requestAnimationFrame(() => {{
    eng.discoveryRafPending = false;
    let cntStr = `${{eng.uploadedFiles.toLocaleString()}} / ${{eng.totalFiles.toLocaleString()}} dosya`;
    if (eng.skippedFiles > 0) cntStr += ` (${{eng.skippedFiles.toLocaleString()}} atlandı)`;
    if (eng.errorFiles > 0) cntStr += ` | ${{eng.errorFiles.toLocaleString()}} hata`;
    if (!eng.discoveryDone) cntStr += ' — tarama devam ediyor…';
    ovCounts.textContent = cntStr;
  }});
}}

function scheduleProgressUpdate(eng) {{
  let inFlight = 0;
  for (const a of eng.active.values()) inFlight += a.loaded;
  const current = eng.uploadedSize + inFlight;
  const total   = Math.max(eng.totalSize, 1);
  const pct     = Math.min(100, Math.round(current / total * 100));
  ovFill.style.width = pct + '%';
  const elapsed   = (Date.now() - eng.startT) / 1000;
  const speed     = elapsed > 0 ? current / elapsed : 0;
  const remaining = speed > 0 ? (total - current) / speed : 0;

  ovLabel.textContent = `${{fmtB(current)}} / ${{fmtB(total)}}`;
  ovSpeed.textContent = `${{fmtB(speed)}}/s`;
  ovTime.textContent = `Kalan: ${{fmtTime(remaining)}}`;

  let cntStr = `${{eng.uploadedFiles.toLocaleString()}} / ${{eng.totalFiles.toLocaleString()}} dosya`;
  if (eng.skippedFiles > 0) cntStr += ` (${{eng.skippedFiles.toLocaleString()}} atlandı)`;
  if (eng.errorFiles > 0) cntStr += ` | ${{eng.errorFiles.toLocaleString()}} hata`;
  if (!eng.discoveryDone) cntStr += ' — tarama devam ediyor…';
  ovCounts.textContent = cntStr;
}}

// Yuklenen dosyanin sunucuda kaydedilecegi tam yolu (gorunum icin) uretir.
// UP_URL: bu tarayici klasoru ("/" veya "/altklasor"); relPath: dosyanin
// klasor icindeki goreli yolu.
function serverDestPath(relPath) {{
  let base = (UP_URL && UP_URL !== '/') ? UP_URL : '';
  let full = (base + '/' + relPath).replace(/\/{{2,}}/g, '/');
  if (!full.startsWith('/')) full = '/' + full;
  return full;
}}

// Dosya adlari textContent ile yazilir (innerHTML degil) — XSS yok.
// Her satir: dosya adi + sunucudaki hedef yol + ilerleme cubugu.
function createActiveEl(eng, id, item) {{
  const el = document.createElement('div');
  el.className = 'q-item';

  const top = document.createElement('div'); top.className = 'q-top';
  const nm = document.createElement('span'); nm.className = 'q-name';
  nm.textContent = item.file.name;
  const info = document.createElement('span'); info.className = 'q-info';
  info.textContent = 'Basliyor...';
  top.appendChild(nm); top.appendChild(info);

  const dest = document.createElement('div'); dest.className = 'q-dest';
  const dlab = document.createElement('b'); dlab.textContent = 'Sunucuya: ';
  const dpath = document.createElement('span'); dpath.textContent = serverDestPath(item.relPath);
  dest.appendChild(dlab); dest.appendChild(dpath);

  const wrap = document.createElement('div'); wrap.className = 'q-bar';
  const bar = document.createElement('div'); bar.className = 'q-fill uploading';
  wrap.appendChild(bar);

  el.appendChild(top); el.appendChild(dest); el.appendChild(wrap);
  queue.appendChild(el);
  el._dpath = dpath;  // sunucudan gelen gercek kayit yolunu yazmak icin
  return el;
}}

function processFilesChunked(files, getRelPath) {{
  const eng = getOrCreateEngine();
  const CHUNK = 2000;
  let i = 0;
  function processChunk() {{
    const end = Math.min(i + CHUNK, files.length);
    for (; i < end; i++) {{
      const f = files[i];
      const relPath = getRelPath(f);
      eng.pending.push({{ file: f, relPath }});
      eng.totalFiles++;
      eng.totalSize += f.size;
    }}
    scheduleDiscoveryUpdate(eng);
    if (i < files.length) {{
      setTimeout(processChunk, 0);
    }} else {{
      eng.discoveryDone = true;
      scheduleDiscoveryUpdate(eng);
      while (eng.workersRunning < PARALLEL && eng.workersRunning < eng.pending.length) {{
        eng.workersRunning++;
        workerDispatch(eng);
      }}
    }}
  }}
  processChunk();
}}

async function wsUploadOne(eng, ws, item) {{
  const id = 'W' + Math.random().toString(36).substring(2);
  const el = createActiveEl(eng, id, item);
  const bar = el.querySelector('.q-fill');
  const info = el.querySelector('.q-info');
  const state = {{ loaded: 0 }};
  eng.active.set(id, state);

  return new Promise((resolve, reject) => {{
    ws.send(JSON.stringify({{ path: item.relPath, size: item.file.size }}));
    if (item.file.size > 0) info.textContent = 'Gonderiliyor…';
    let offset = 0;
    function sendNext() {{
      if (offset >= item.file.size) return;
      const ch = item.file.slice(offset, offset + WS_CHUNK);
      offset += WS_CHUNK;
      ch.arrayBuffer().then(buf => {{
        ws.send(buf);
        sendNext();
      }}).catch(reject);
    }}
    if (item.file.size > 0) sendNext();
    ws.onmessage = (e) => {{
      const res = JSON.parse(e.data);
      if (res.ok) {{
        state.loaded = res.written;
        bar.style.width = '100%';
        bar.className = 'q-fill done';
        info.textContent = 'Yuklendi';
        if (res.saved && el._dpath) el._dpath.textContent = '/' + res.saved;
        setTimeout(() => el.remove(), 1200);
        resolve();
      }} else reject(new Error(res.error));
    }};
  }});
}}

async function httpUploadOne(eng, item) {{
  const id = 'U' + Math.random().toString(36).substring(2);
  const el = createActiveEl(eng, id, item);
  const bar = el.querySelector('.q-fill');
  const info = el.querySelector('.q-info');
  const state = {{ loaded: 0 }};
  eng.active.set(id, state);

  try {{
    const xhr = new XMLHttpRequest();
    let savedPath = null;
    await new Promise((resolve, reject) => {{
      xhr.upload.onprogress = (e) => {{
        if (e.lengthComputable) {{
          state.loaded = e.loaded;
          const p = (e.loaded / e.total * 100).toFixed(1);
          bar.style.width = p + '%';
          info.textContent = `${{p}}% (${{fmtB(e.loaded)}})`;
          scheduleProgressUpdate(eng);
        }}
      }};
      xhr.onload = () => {{
        if (xhr.status === 200) {{
          try {{ savedPath = JSON.parse(xhr.responseText).saved; }} catch (_) {{}}
          resolve();
        }} else reject(new Error(`HTTP ${{xhr.status}}`));
      }};
      xhr.onerror = () => reject(new Error('Ag hatasi'));
      const q = `?dir=${{encodeURIComponent(UP_URL)}}&path=${{encodeURIComponent(item.relPath)}}`;
      xhr.open('POST', '/api/upload' + q, true);
      xhr.send(item.file);
    }});
    eng.uploadedSize += item.file.size;
    eng.uploadedFiles++;
    bar.style.width = '100%';
    bar.className = 'q-fill done';
    info.textContent = 'Yuklendi';
    if (savedPath && el._dpath) el._dpath.textContent = '/' + savedPath;
    setTimeout(() => el.remove(), 1200);
  }} catch (e) {{
    eng.errorFiles++;
    bar.style.width = '100%';
    bar.className = 'q-fill error';
    info.textContent = e.message;
    throw e;
  }} finally {{
    eng.active.delete(id);
    scheduleProgressUpdate(eng);
  }}
}}

async function workerDispatch(eng) {{
  const wsUrl = `ws://${{location.host}}/ws/upload?dir=${{encodeURIComponent(UP_URL)}}`;
  let ws = null;
  let wsConnecting = false;
  try {{
    while (!eng.hasError) {{
      const item = eng.pending.shift();
      if (!item) {{
        if (!eng.discoveryDone) {{ await new Promise(r => setTimeout(r, 200)); continue; }}
        if (eng.pending.length === 0) break;
        continue;
      }}
      try {{
        if (item.file.size > HYBRID_THRESHOLD) {{
          await httpUploadOne(eng, item);
        }} else {{
          if (!ws) {{
            wsConnecting = true;
            ws = new WebSocket(wsUrl);
            ws.binaryType = 'arraybuffer';
            await new Promise((resolve, reject) => {{ ws.onopen = resolve; ws.onerror = () => reject(new Error('WS Error')); setTimeout(() => reject(new Error('WS Timeout')), 5000); }});
            wsConnecting = false;
          }}
          await wsUploadOne(eng, ws, item);
        }}
      }} catch (e) {{
        if (ws && !wsConnecting) {{ try {{ ws.close(); }} catch(err) {{}} ws = null; }}
      }}
    }}
  }} finally {{
    if (ws) try {{ ws.close(); }} catch(e) {{}}
    eng.workersRunning--;
    if (eng.workersRunning === 0 && eng.pending.length === 0) finishUpload(eng);
  }}
}}

dz.addEventListener("drop", async e => {{
  e.preventDefault();
  const dtItems = e.dataTransfer.items;
  if (!dtItems) return;
  const entries = [];
  for (const item of dtItems) {{
    if (item.kind !== 'file') continue;
    const entry = item.webkitGetAsEntry ? item.webkitGetAsEntry() : null;
    if (entry) entries.push(entry);
    else {{ const f = item.getAsFile(); if (f) entries.push({{ isFile: true, file: () => Promise.resolve(f), name: f.name }}); }}
  }}

  const allFiles = [];
  let disc = 0;
  async function traverse(entry, base) {{
    if (entry.isFile) {{
      await new Promise(res => {{
        entry.file(f => {{
          f.webkitRelativePath_mock = base ? base + '/' + entry.name : entry.name;
          allFiles.push(f);
          disc++;
          res();
        }}, res);
      }});
      if (disc % 200 === 0) await new Promise(r => setTimeout(r, 0));
    }} else if (entry.isDirectory) {{
      const ch = await new Promise(res => {{
        const rd = entry.createReader();
        const all = (acc) => {{ rd.readEntries(e => {{ if(!e.length) res(acc); else all(acc.concat(Array.from(e))); }}, ()=>res(acc)); }};
        all([]);
      }});
      const nb = base ? base+'/'+entry.name : entry.name;
      for (const c of ch) await traverse(c, nb);
    }}
  }}
  await Promise.all(entries.map(e => traverse(e, '')));
  if (allFiles.length > 0) {{
    processFilesChunked(allFiles, f => f.webkitRelativePath_mock || f.name);
  }}
}});

fi.addEventListener("change", () => {{
  if (!fi.files.length) return;
  processFilesChunked(Array.from(fi.files), f => f.name);
  fi.value = '';
}});

fiDir.addEventListener("change", () => {{
  if (!fiDir.files.length) return;
  processFilesChunked(Array.from(fiDir.files), f => f.webkitRelativePath || f.name);
}});

// ── WS indirme ────────────────────────────────────────────────────────
// /ws/download uzerinden pencereli (windowed) chunk akisi. Guvenli baglamda
// (localhost/https) File System Access API ile diske stream edilir; aksi
// halde (LAN IP + http) Blob'a toplanir. Klasorler istemci tarafinda ZIP
// (store, sifir sikistirma) olarak paketlenir — harici kutuphane yok.

// Tek dosyayi WS ile ceker. onChunk(u8, received, size) her parca icin
// cagrilir ve await edilir (backpressure). done olunca resolve.
function wsStream(serverPath, onChunk) {{
  return new Promise((resolve, reject) => {{
    const ws = new WebSocket(`ws://${{location.host}}/ws/download`);
    ws.binaryType = 'arraybuffer';
    let size = 0, received = 0, chain = Promise.resolve(), done = false;
    ws.onopen  = () => ws.send(JSON.stringify({{ path: serverPath }}));
    ws.onerror = () => {{ if (!done) {{ done = true; reject(new Error('Baglanti hatasi')); }} }};
    ws.onclose = () => {{ if (!done) {{ done = true; reject(new Error('Baglanti koptu')); }} }};
    ws.onmessage = (e) => {{
      if (typeof e.data === 'string') {{
        const m = JSON.parse(e.data);
        if (m.ok === false) {{ done = true; try {{ ws.close(); }} catch (_) {{}} reject(new Error(m.error || 'Hata')); return; }}
        if (m.ok === true)  {{ size = m.size; return; }}
        if (m.done) {{
          chain.then(() => {{ done = true; try {{ ws.close(); }} catch (_) {{}} resolve({{ size, received }}); }})
               .catch((err) => {{ done = true; reject(err); }});
        }}
        return;
      }}
      const u8 = new Uint8Array(e.data);
      chain = chain.then(async () => {{
        await onChunk(u8, received, size);
        received += u8.byteLength;
        ws.send(JSON.stringify({{ ack: true }}));
      }});
    }};
  }});
}}

// ── Ilerleme satiri (dosya adlari textContent ile — XSS yok) ──
function dlRow(labelText, destText) {{
  dashboard.classList.add('active');
  const el = document.createElement('div'); el.className = 'q-item';
  const top = document.createElement('div'); top.className = 'q-top';
  const nm = document.createElement('span'); nm.className = 'q-name'; nm.textContent = labelText;
  const info = document.createElement('span'); info.className = 'q-info'; info.textContent = 'Baglaniyor...';
  top.appendChild(nm); top.appendChild(info);
  el.appendChild(top);
  let dpath = null;
  if (destText) {{
    const dest = document.createElement('div'); dest.className = 'q-dest';
    const dlab = document.createElement('b'); dlab.textContent = 'Kaydediliyor: ';
    dpath = document.createElement('span'); dpath.textContent = destText;
    dest.appendChild(dlab); dest.appendChild(dpath);
    el.appendChild(dest);
  }}
  const wrap = document.createElement('div'); wrap.className = 'q-bar';
  const bar = document.createElement('div'); bar.className = 'q-fill downloading'; wrap.appendChild(bar);
  el.appendChild(wrap);
  queue.appendChild(el);
  return {{ el, nm, bar, info, dpath, t0: Date.now() }};
}}
function dlRowDone(row, text) {{
  row.bar.style.width = '100%'; row.bar.className = 'q-fill done';
  row.info.textContent = text || 'Bitti';
  setTimeout(() => row.el.remove(), 2000);
}}
function dlRowFail(row, msg) {{
  row.bar.style.width = '100%'; row.bar.className = 'q-fill error';
  row.info.textContent = msg || 'Hata';
}}
function downloadBlob(blob, name) {{
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob); a.download = name;
  document.body.appendChild(a); a.click(); a.remove();
  setTimeout(() => URL.revokeObjectURL(a.href), 15000);
}}
function joinPath(a, b) {{ return a.replace(/\/+$/, '') + '/' + b; }}

// ── Tek dosya indirme ──
async function wsDownloadFile(btn) {{
  const relPath = btn.dataset.path, name = btn.dataset.name;
  let writable = null, parts = null, savedName = name;
  if (window.showSaveFilePicker) {{
    try {{
      const h = await window.showSaveFilePicker({{ suggestedName: name }});
      writable = await h.createWritable();
      savedName = h.name || name;
    }} catch (e) {{
      if (e && e.name === 'AbortError') return;
      writable = null;  // guvensiz baglam / reddedildi → Blob'a dus
    }}
  }}
  if (!writable) parts = [];

  const destText = writable ? (savedName + '  (sectiginiz konum)')
                            : ('Indirilenler klasoru / ' + name);
  const row = dlRow('⬇️ ' + name, destText);
  try {{
    await wsStream(relPath, async (u8, received, size) => {{
      if (writable) await writable.write(u8); else parts.push(u8);
      const cur = received + u8.byteLength;
      const pct = size > 0 ? Math.min(100, cur / size * 100) : 0;
      row.bar.style.width = pct.toFixed(1) + '%';
      const sp = cur / Math.max((Date.now() - row.t0) / 1000, 0.001);
      row.info.textContent = `${{pct.toFixed(0)}}% (${{fmtB(sp)}}/s)`;
    }});
    if (writable) await writable.close();
    else downloadBlob(new Blob(parts), name);
    dlRowDone(row);
  }} catch (e) {{
    dlRowFail(row, e.message);
    try {{ if (writable) await writable.abort(); }} catch (_) {{}}
  }}
}}

// ── Klasor indirme ──
async function wsDownloadFolder(btn) {{
  const relDir = btn.dataset.path, name = btn.dataset.name;

  // Guvenli baglamda diske klasor yapisiyla yaz; degilse ZIP olustur.
  let dirHandle = null;
  if (window.showDirectoryPicker) {{
    try {{ dirHandle = await window.showDirectoryPicker({{ mode: 'readwrite' }}); }}
    catch (e) {{ if (e && e.name === 'AbortError') return; dirHandle = null; }}
  }}

  let data;
  try {{
    const res = await fetch('/api/list?dir=' + encodeURIComponent(relDir));
    data = await res.json();
  }} catch (e) {{ alert('Klasor listesi alinamadi: ' + e.message); return; }}
  if (!data || !data.ok) {{ alert('Klasor listesi alinamadi'); return; }}
  if (data.count === 0) {{ alert('Klasor bos'); return; }}
  if (!dirHandle && (data.total || 0) > 0xFFFFFFFF) {{
    alert('Klasor 4GB\\'tan buyuk — ZIP olusturulamiyor. Dosyalari tek tek indirin.');
    return;
  }}

  const destText = dirHandle ? (dirHandle.name + ' / ' + name + ' /…')
                             : ('Indirilenler klasoru / ' + name + '.zip');
  const row = dlRow('📁 ' + name + '  (0/' + data.count + ')', destText);
  const totalSize = data.total || 0;
  let filesDone = 0, grandRecv = 0;
  const progress = (extra) => {{
    const cur = grandRecv + extra;
    const pct = totalSize > 0 ? Math.min(100, cur / totalSize * 100) : 0;
    row.bar.style.width = pct.toFixed(1) + '%';
    const sp = cur / Math.max((Date.now() - row.t0) / 1000, 0.001);
    row.info.textContent = `${{filesDone}}/${{data.count}} dosya | ${{fmtB(cur)}} (${{fmtB(sp)}}/s)`;
  }};

  try {{
    if (dirHandle) {{
      const destRoot = await dirHandle.getDirectoryHandle(name, {{ create: true }});
      for (const f of data.files) {{
        const segs = f.path.split('/'); const fn = segs.pop();
        let dir = destRoot;
        for (const p of segs) dir = await dir.getDirectoryHandle(p, {{ create: true }});
        const fh = await dir.getFileHandle(fn, {{ create: true }});
        const w = await fh.createWritable();
        try {{
          await wsStream(joinPath(relDir, f.path), async (u8, rec) => {{ await w.write(u8); progress(rec + u8.byteLength); }});
          await w.close();
        }} catch (e) {{ try {{ await w.abort(); }} catch (_) {{}} throw e; }}
        grandRecv += f.size; filesDone++;
        row.nm.textContent = '📁 ' + name + '  (' + filesDone + '/' + data.count + ')';
      }}
    }} else {{
      const zip = new ZipStore();
      for (const f of data.files) {{
        const chunks = [];
        await wsStream(joinPath(relDir, f.path), async (u8, rec) => {{ chunks.push(u8); progress(rec + u8.byteLength); }});
        zip.add(f.path, chunks);
        grandRecv += f.size; filesDone++;
        row.nm.textContent = '📁 ' + name + '  (' + filesDone + '/' + data.count + ')';
      }}
      downloadBlob(zip.finish(), name + '.zip');
    }}
    dlRowDone(row, `Bitti — ${{filesDone}}/${{data.count}} dosya (${{fmtB(grandRecv)}})`);
  }} catch (e) {{
    dlRowFail(row, e.message);
  }}
}}

// ── ZIP (store / sifir sikistirma) — harici kutuphane yok ──
const CRC_TABLE = (() => {{
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {{
    let c = n;
    for (let k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320 ^ (c >>> 1)) : (c >>> 1);
    t[n] = c >>> 0;
  }}
  return t;
}})();
function crc32(chunks) {{
  let c = 0xFFFFFFFF;
  for (const u8 of chunks) for (let i = 0; i < u8.length; i++) c = CRC_TABLE[(c ^ u8[i]) & 0xFF] ^ (c >>> 8);
  return (c ^ 0xFFFFFFFF) >>> 0;
}}
class ZipStore {{
  constructor() {{ this.parts = []; this.central = []; this.offset = 0; this.count = 0; this.enc = new TextEncoder(); }}
  add(name, chunks) {{
    const nameBytes = this.enc.encode(name);
    let size = 0; for (const c of chunks) size += c.length;
    const crc = crc32(chunks);
    const lh = new DataView(new ArrayBuffer(30));
    lh.setUint32(0, 0x04034b50, true);
    lh.setUint16(4, 20, true); lh.setUint16(6, 0x0800, true); lh.setUint16(8, 0, true);
    lh.setUint16(10, 0, true); lh.setUint16(12, 0x21, true);
    lh.setUint32(14, crc, true); lh.setUint32(18, size, true); lh.setUint32(22, size, true);
    lh.setUint16(26, nameBytes.length, true); lh.setUint16(28, 0, true);
    const off = this.offset;
    this._push(new Uint8Array(lh.buffer)); this._push(nameBytes);
    for (const c of chunks) this._push(c);
    const ch = new DataView(new ArrayBuffer(46));
    ch.setUint32(0, 0x02014b50, true);
    ch.setUint16(4, 20, true); ch.setUint16(6, 20, true); ch.setUint16(8, 0x0800, true); ch.setUint16(10, 0, true);
    ch.setUint16(12, 0, true); ch.setUint16(14, 0x21, true);
    ch.setUint32(16, crc, true); ch.setUint32(20, size, true); ch.setUint32(24, size, true);
    ch.setUint16(28, nameBytes.length, true); ch.setUint16(30, 0, true); ch.setUint16(32, 0, true);
    ch.setUint16(34, 0, true); ch.setUint16(36, 0, true); ch.setUint32(38, 0, true);
    ch.setUint32(42, off, true);
    this.central.push(new Uint8Array(ch.buffer)); this.central.push(nameBytes);
    this.count++;
  }}
  _push(u8) {{ this.parts.push(u8); this.offset += u8.length; }}
  finish() {{
    const centralStart = this.offset;
    let centralSize = 0;
    for (const p of this.central) {{ this.parts.push(p); centralSize += p.length; this.offset += p.length; }}
    const eocd = new DataView(new ArrayBuffer(22));
    eocd.setUint32(0, 0x06054b50, true);
    eocd.setUint16(4, 0, true); eocd.setUint16(6, 0, true);
    eocd.setUint16(8, this.count, true); eocd.setUint16(10, this.count, true);
    eocd.setUint32(12, centralSize, true); eocd.setUint32(16, centralStart, true);
    eocd.setUint16(20, 0, true);
    this.parts.push(new Uint8Array(eocd.buffer));
    return new Blob(this.parts, {{ type: 'application/zip' }});
  }}
}}

document.querySelector('.fl').addEventListener('click', (e) => {{
  const b = e.target.closest('.wsdl');
  if (!b) return;
  e.preventDefault(); e.stopPropagation();
  if (b.dataset.dir === '1') wsDownloadFolder(b); else wsDownloadFile(b);
}});
</script>
</body>
</html>)HTML";
