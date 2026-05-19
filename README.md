# pc-monitor

Linux-аналог `top`/`htop` с web-UI: C++ backend читает `/proc`, фронтенд на TypeScript/HTML/CSS отображает CPU, память и список процессов в реальном времени.

## Как собрать

```bash
cmake -B build
cmake --build build
```

Первый `cmake -B build` подтянет через `FetchContent` две зависимости (см. ниже). Дальнейшие сборки уже без сети.

## Запуск

```bash
./build/pc-monitor
```

Без аргументов. При старте:

- если в системе есть браузер (Chrome / Firefox / Edge) — webui сам откроет окно, если нет (headless / SSH) — в stderr выводится URL вида `open in your browser: http://localhost:NNNNN/index.html`.

Остановка — `Ctrl+C` (SIGINT) или закрытие окна браузера / SIGTERM.

## API

UI и backend связаны через WebUI.

| JS-вызов              | C++-обработчик         | Что делает                                    |
|-----------------------|------------------------|-----------------------------------------------|
| `webui.get_snapshot()` | `on_snapshot_request` | Pull-запрос: вернёт текущий снапшот строкой JSON |
| `webui.kill_pid(pid)`  | `on_kill`             | Шлёт `SIGTERM` процессу, возвращает `rc` от `kill(2)` |
| `webui.force_kill(pid)`| `on_force_kill`       | Шлёт `SIGKILL` процессу                       |

Дополнительно backend каждую секунду пушит снапшот сам, вызывая на фронте `updateSnapshot(jsonPayload)`.

Формат снапшота (`application/json`):

```json
{
    "timestamp_ms": 1747627100000,
    "hostname": "linux-host",
    "uptime": 12345.6,
    "load": [0.42, 0.31, 0.20],
    "cpus": [
        { "id": -1, "usage": 23.4, "user": 18.0, "system": 5.4, "iowait": 0.0 },
        { "id":  0, "usage": 12.1, "user":  8.0, "system": 4.1, "iowait": 0.0 }
    ],
    "mem": {
        "total_kb": 16000000, "used_kb": 9000000, "free_kb": 7000000,
        "cached_kb": 3000000, "buffers_kb": 200000,
        "swap_total_kb": 4000000, "swap_used_kb": 50000
    },
    "processes": [
        { "pid": 1234, "ppid": 1, "name": "firefox", "user": "divmone",
          "state": "S", "cpu_pct": 12.3, "mem_pct": 5.4,
          "mem_kb": 86000, "threads": 80, "priority": 20 }
    ]
}
```

`cpus[0]` с `id == -1` — агрегат по всем ядрам; остальные элементы — отдельные ядра. Список процессов отсортирован по `cpu_pct` desc, обрезан до 50 элементов (`max_processes` в `to_json`).

## Архитектура

`pc-monitor` — один процесс с двумя потоками:

- **главный поток**: `webui::window` поднимает локальный HTTP-сервер, отдаёт статику из `frontend/`, держит WebSocket к открытой странице. Биндит колбэки (`get_snapshot`, `kill_pid`, `force_kill`) и блокируется на `webui::wait()`.
- **фоновой pusher**: раз в секунду берёт мьютекс, дёргает `SystemMonitor::snapshot()`, сериализует в JSON через `nlohmann::json`, шлёт фронту вызовом `win.run("updateSnapshot('...')")`.

- `CpuReader` — `/proc/stat`, считает загруженность ядер процессора;
- `MemReader` — `/proc/meminfo`, без состояния (used = total - available);
- `ProcessReader` — `/proc/[pid]/stat` + `/proc/[pid]/status`, хранит `pid → utime+stime` от прошлого вызова и общий tick-каунтер для дельт CPU%.

Каждая структура (`CpuCoreUsageInfo`, `MemInfo`, `ProcessInfo`, `Snapshot`) имеет inline `to_json(...)` в своём `.hpp`.

Фронтенд (`src/frontend/app.ts`) рисует:
- бары CPU / Mem / Swap (зелёный → жёлтый → красный по порогам 60/85 %),
- топ-20 процессов с сортировкой по клику на заголовке столбца,
- Двойной клик по строке → SIGTERM через `webui.kill_pid()`.

`app.ts` компилируется в  `npx -p typescript@5.5.4 tsc -p src/frontend`. После сборки CMake POST_BUILD копирует `src/frontend/` рядом с `pc-monitor`.

## Зависимости

- [webui-dev/webui](https://github.com/webui-dev/webui) — встроенный браузерный UI-мост (HTTP + WS на localhost) `FetchContent`
- [nlohmann/json](https://github.com/nlohmann/json) — header-only JSON, подтягивается через `FetchContent`.
