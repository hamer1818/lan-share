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
  .path {{ font-size:.78rem; color:var(--text3); margin-bottom:24px;
    font-family:'Cascadia Code','Fira Code',monospace; word-break:break-all; padding-left:58px; }}

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
  <div class="ft">LAN Dosya Paylasimi &mdash; {server_info}</div>
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

function createActiveEl(eng, id, item) {{
  const el = document.createElement('div');
  el.className = 'q-item';
  const showPath = item.relPath.includes('/');
  el.innerHTML = `
    ${{showPath ? `<div class="q-path">&#128193; ${{item.relPath}}</div>` : ''}}
    <div class="q-top">
      <span class="q-name">${{item.file.name}}</span>
      <span class="q-info">Basliyor...</span>
    </div>
    <div class="q-bar"><div class="q-fill uploading"></div></div>`;
  queue.appendChild(el);
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
        info.textContent = 'WS Bitti';
        setTimeout(() => el.remove(), 1000);
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
      xhr.onload = () => {{ if (xhr.status === 200) resolve(); else reject(new Error(`HTTP ${{xhr.status}}`)); }};
      xhr.onerror = () => reject(new Error('Ag hatasi'));
      const q = `?dir=${{encodeURIComponent(UP_URL)}}&path=${{encodeURIComponent(item.relPath)}}`;
      xhr.open('POST', '/api/upload' + q, true);
      xhr.send(item.file);
    }});
    eng.uploadedSize += item.file.size;
    eng.uploadedFiles++;
    bar.style.width = '100%';
    bar.className = 'q-fill done';
    info.textContent = 'HTTP Bitti';
    setTimeout(() => el.remove(), 1000);
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
</script>
</body>
</html>)HTML";
