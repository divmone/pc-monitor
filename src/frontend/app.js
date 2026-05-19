"use strict";
const TOP_N = 20;
const DESC_DEFAULT = ["cpu_pct", "mem_pct", "mem_kb", "pid"];
const state = {
    sortKey: "cpu_pct",
    sortDesc: true,
    last: null,
};
const $ = (id) => {
    const el = document.getElementById(id);
    if (!el)
        throw new Error("missing #" + id);
    return el;
};
const fmtUptime = (sec) => {
    const s = Math.floor(sec);
    const d = Math.floor(s / 86400);
    const h = Math.floor((s % 86400) / 3600);
    const m = Math.floor((s % 3600) / 60);
    return d > 0 ? `${d}d ${h}h ${m}m` : `${h}h ${m}m`;
};
const fmtKb = (kb) => {
    if (kb >= 1024 * 1024)
        return (kb / 1024 / 1024).toFixed(1) + " GiB";
    if (kb >= 1024)
        return (kb / 1024).toFixed(1) + " MiB";
    return kb + " KiB";
};
const fillClass = (pct) => pct >= 85 ? "fill crit" :
    pct >= 60 ? "fill warn" : "fill";
const cpuClass = (pct) => pct >= 50 ? "num cpu-hot" :
    pct >= 20 ? "num cpu-warm" : "num";
const escapeHtml = (s) => s.replace(/[&<>"']/g, ch => ({
    "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;",
}[ch]));
function setBar(prefix, pct, text) {
    const fill = $(prefix + "-fill");
    fill.style.width = pct.toFixed(1) + "%";
    fill.className = fillClass(pct);
    $(prefix + "-text").textContent = text;
}
function renderHeader(snap) {
    $("host").textContent = snap.hostname || "unknown";
    $("uptime").textContent = fmtUptime(snap.uptime);
    $("load").textContent = snap.load.map(v => v.toFixed(2)).join(" / ");
    $("status").textContent = "live · " + new Date().toLocaleTimeString();
}
function renderBars(snap) {
    const agg = snap.cpus.find(c => c.id === -1) ?? snap.cpus[0];
    const cores = snap.cpus.filter(c => c.id !== -1).length;
    if (agg) {
        const pct = Math.max(0, Math.min(100, agg.usage));
        setBar("cpu", pct, `${pct.toFixed(1)}%  (${cores} cores)`);
    }
    const m = snap.mem;
    const memPct = m.total_kb > 0 ? 100 * m.used_kb / m.total_kb : 0;
    setBar("mem", memPct, `${fmtKb(m.used_kb)} / ${fmtKb(m.total_kb)} (${memPct.toFixed(1)}%)`);
    if (m.swap_total_kb > 0) {
        $("swap-row").hidden = false;
        const swapPct = 100 * m.swap_used_kb / m.swap_total_kb;
        setBar("swap", swapPct, `${fmtKb(m.swap_used_kb)} / ${fmtKb(m.swap_total_kb)} (${swapPct.toFixed(1)}%)`);
    }
    else {
        $("swap-row").hidden = true;
    }
}
function sortProcs(procs) {
    const { sortKey, sortDesc } = state;
    const dir = sortDesc ? -1 : 1;
    return [...procs].sort((a, b) => {
        const av = a[sortKey], bv = b[sortKey];
        return typeof av === "number" && typeof bv === "number"
            ? (av - bv) * dir
            : String(av).localeCompare(String(bv)) * dir;
    }).slice(0, TOP_N);
}
function renderProcs(procs) {
    const body = $("proc-body");
    body.innerHTML = sortProcs(procs).map(p => `
        <tr data-pid="${p.pid}" title="dblclick to SIGTERM ${escapeHtml(p.name)} (pid ${p.pid})">
            <td>${p.pid}</td>
            <td>${escapeHtml(p.user)}</td>
            <td class="${cpuClass(p.cpu_pct)}">${p.cpu_pct.toFixed(1)}</td>
            <td class="num">${p.mem_pct.toFixed(1)}</td>
            <td class="num">${fmtKb(p.mem_kb)}</td>
            <td>${p.state}</td>
            <td class="name">${escapeHtml(p.name)}</td>
        </tr>`).join("");
    body.querySelectorAll("tr").forEach(tr => {
        const pid = Number(tr.dataset.pid);
        const name = tr.querySelector(".name")?.textContent ?? "";
        tr.addEventListener("dblclick", () => requestKill(pid, name, tr));
    });
}
function render(snap) {
    state.last = snap;
    renderHeader(snap);
    renderBars(snap);
    renderProcs(snap.processes);
}
async function requestKill(pid, name, tr) {
    if (!confirm(`Send SIGTERM to "${name}" (pid ${pid})?`))
        return;
    try {
        const rc = Number(await webui.kill_pid(pid));
        if (rc === 0)
            tr.classList.add("killed");
        else
            alert(`kill failed (rc=${rc}). Need root or process is gone?`);
    }
    catch (err) {
        alert("kill RPC failed: " + err);
    }
}
function bindHeaderSort() {
    document.querySelectorAll("th[data-sort]").forEach(th => {
        th.addEventListener("click", () => {
            const key = th.dataset.sort;
            if (state.sortKey === key) {
                state.sortDesc = !state.sortDesc;
            }
            else {
                state.sortKey = key;
                state.sortDesc = DESC_DEFAULT.includes(key);
            }
            document.querySelectorAll("th").forEach(h => h.classList.remove("active"));
            th.classList.add("active");
            if (state.last)
                renderProcs(state.last.processes);
        });
    });
}
window.updateSnapshot = (payload) => {
    try {
        render(JSON.parse(payload));
    }
    catch (e) {
        console.error("bad snapshot payload", e);
    }
};
async function bootstrap() {
    bindHeaderSort();
    try {
        render(JSON.parse(await webui.get_snapshot()));
    }
    catch (e) {
        $("status").textContent = "waiting for backend…";
        console.warn("initial get_snapshot failed", e);
    }
}
window.addEventListener("load", bootstrap);
