interface CpuCore  { id: number; usage: number; user: number; system: number; iowait: number; }
interface MemInfo  { total_kb: number; used_kb: number; free_kb: number; cached_kb: number;
                     buffers_kb: number; swap_total_kb: number; swap_used_kb: number; }
interface ProcRow  { pid: number; ppid: number; name: string; user: string; state: string;
                     cpu_pct: number; mem_pct: number; mem_kb: number; threads: number;
                     priority: number; }
interface Snapshot {
    hostname: string;
    uptime: number;
    load: [number, number, number];
    cpus: CpuCore[];
    mem: MemInfo;
    processes: ProcRow[];
}

type SortKey = "pid" | "user" | "cpu_pct" | "mem_pct" | "mem_kb" | "state" | "name";

declare const webui: any;

const TOP_N = 20;
const DESC_DEFAULT: ReadonlyArray<SortKey> = ["cpu_pct", "mem_pct", "mem_kb", "pid"];

const state = {
    sortKey:  "cpu_pct" as SortKey,
    sortDesc: true,
    last:     null as Snapshot | null,
};

const $ = (id: string): HTMLElement => {
    const el = document.getElementById(id);
    if (!el) throw new Error("missing #" + id);
    return el;
};

const formatUptime = (sec: number): string => {
    const s = Math.floor(sec);
    const d = Math.floor(s / 86400);
    const h = Math.floor((s % 86400) / 3600);
    const m = Math.floor((s % 3600) / 60);
    return d > 0 ? `${d}d ${h}h ${m}m` : `${h}h ${m}m`;
};

const formatKb = (kb: number): string => {
    if (kb >= 1024 * 1024) return (kb / 1024 / 1024).toFixed(1) + " GiB";
    if (kb >= 1024)        return (kb / 1024).toFixed(1) + " MiB";
    return kb + " KiB";
};

const fillClass = (pct: number): string =>
    pct >= 85 ? "fill crit" :
    pct >= 60 ? "fill warn" : "fill";

const cpuClass = (pct: number): string =>
    pct >= 50 ? "num cpu-hot" :
    pct >= 20 ? "num cpu-warm" : "num";

const escapeHtml = (s: string): string =>
    s.replace(/[&<>"']/g, ch => ({
        "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;",
    } as Record<string, string>)[ch]);

function setBar(prefix: string, pct: number, text: string): void {
    const fill = $(prefix + "-fill");
    fill.style.width = pct.toFixed(1) + "%";
    fill.className   = fillClass(pct);
    $(prefix + "-text").textContent = text;
}

function renderHeader(snap: Snapshot): void {
    $("host").textContent   = snap.hostname || "unknown";
    $("uptime").textContent = formatUptime(snap.uptime);
    $("load").textContent   = snap.load.map(v => v.toFixed(2)).join(" / ");
    $("status").textContent = "live · " + new Date().toLocaleTimeString();
}

function renderBars(snap: Snapshot): void {
    const agg   = snap.cpus.find(c => c.id === -1) ?? snap.cpus[0];
    const cores = snap.cpus.filter(c => c.id !== -1).length;
    if (agg) {
        const pct = Math.max(0, Math.min(100, agg.usage));
        setBar("cpu", pct, `${pct.toFixed(1)}%  (${cores} cores)`);
    }

    const m       = snap.mem;
    const memPct  = m.total_kb > 0 ? 100 * m.used_kb / m.total_kb : 0;
    setBar("mem", memPct,
        `${formatKb(m.used_kb)} / ${formatKb(m.total_kb)} (${memPct.toFixed(1)}%)`);

    if (m.swap_total_kb > 0) {
        $("swap-row").hidden = false;
        const swapPct = 100 * m.swap_used_kb / m.swap_total_kb;
        setBar("swap", swapPct,
            `${formatKb(m.swap_used_kb)} / ${formatKb(m.swap_total_kb)} (${swapPct.toFixed(1)}%)`);
    } else {
        $("swap-row").hidden = true;
    }
}

function sortProcs(procs: ProcRow[]): ProcRow[] {
    const { sortKey, sortDesc } = state;
    const dir = sortDesc ? -1 : 1;
    return [...procs].sort((a, b) => {
        const av = a[sortKey], bv = b[sortKey];
        return typeof av === "number" && typeof bv === "number"
            ? (av - bv) * dir
            : String(av).localeCompare(String(bv)) * dir;
    }).slice(0, TOP_N);
}

function renderProcs(procs: ProcRow[]): void {
    const body = $("proc-body") as HTMLTableSectionElement;
    body.innerHTML = sortProcs(procs).map(p => `
        <tr data-pid="${p.pid}" title="dblclick to SIGTERM ${escapeHtml(p.name)} (pid ${p.pid})">
            <td>${p.pid}</td>
            <td>${escapeHtml(p.user)}</td>
            <td class="${cpuClass(p.cpu_pct)}">${p.cpu_pct.toFixed(1)}</td>
            <td class="num">${p.mem_pct.toFixed(1)}</td>
            <td class="num">${formatKb(p.mem_kb)}</td>
            <td>${p.state}</td>
            <td class="name">${escapeHtml(p.name)}</td>
        </tr>`).join("");

    body.querySelectorAll<HTMLTableRowElement>("tr").forEach(tr => {
        const pid  = Number(tr.dataset.pid);
        const name = tr.querySelector(".name")?.textContent ?? "";
        tr.addEventListener("dblclick", () => requestKill(pid, name, tr));
    });
}

function render(snap: Snapshot): void {
    state.last = snap;
    renderHeader(snap);
    renderBars(snap);
    renderProcs(snap.processes);
}

async function requestKill(pid: number, name: string, tr: HTMLElement): Promise<void> {
    if (!confirm(`Send SIGTERM to "${name}" (pid ${pid})?`)) return;
    try {
        const rc = Number(await webui.kill_pid(pid));
        if (rc === 0) tr.classList.add("killed");
        else alert(`kill failed (rc=${rc}). Need root or process is gone?`);
    } catch (err) {
        alert("kill RPC failed: " + err);
    }
}

function bindHeaderSort(): void {
    document.querySelectorAll<HTMLTableCellElement>("th[data-sort]").forEach(th => {
        th.addEventListener("click", () => {
            const key = th.dataset.sort as SortKey;
            if (state.sortKey === key) {
                state.sortDesc = !state.sortDesc;
            } else {
                state.sortKey  = key;
                state.sortDesc = DESC_DEFAULT.includes(key);
            }
            document.querySelectorAll("th").forEach(h => h.classList.remove("active"));
            th.classList.add("active");
            if (state.last) renderProcs(state.last.processes);
        });
    });
}

(window as any).updateSnapshot = (payload: string): void => {
    try {
        render(JSON.parse(payload) as Snapshot);
    } catch (e) {
        console.error("bad snapshot payload", e);
    }
};

async function bootstrap(): Promise<void> {
    bindHeaderSort();
    try {
        render(JSON.parse(await webui.get_snapshot()) as Snapshot);
    } catch (e) {
        $("status").textContent = "waiting for backend…";
        console.warn("initial get_snapshot failed", e);
    }
}

window.addEventListener("load", bootstrap);
