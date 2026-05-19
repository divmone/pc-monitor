# pc-monitor

## Как собрать

Требуется: `cmake >= 3.16`, C++ - компилятор (g++/clang), `git`, **Node.js** (для `npx` - нужен компилятору TypeScript).

```bash
cmake -B build
cmake --build build
```

## Запуск

```bash
./build/pc-monitor
```

Без аргументов. При старте:

- если в системе есть браузер - webui сам откроет окно, если нет выводится: `open in your browser: http://localhost:NNNNN/index.html`.

Остановка - `Ctrl+C` (SIGINT) или закрытие окна браузера / SIGTERM.

## Функционал

**Верхняя строка** (`#topbar`):
- hostname, uptime в формате `Nd Nh Nm`, load average 1 / 5 / 15;
- справа индикатор подключения: `connecting…` -> `live · HH:MM:SS` (обновляется на каждый пуш) либо `waiting for backend…`, если стартовый pull не прошёл.

**Таблица процессов** (top 50, ограничение в `to_json(..., max_processes = 50)`):
- Колонки: `PID · USER · CPU% · MEM% · RSS · S · NAME`.
- Клик по заголовку - сортировка по выбранному полю; повторный клик инвертирует направление. По умолчанию DESC для `cpu_pct`, `mem_pct`, `mem_kb`, `pid`, для остальных - ASC.
- В колонке CPU% значения подсвечиваются: `>= 20%` жёлтым (`cpu-warm`), `>= 50%` красным жирным (`cpu-hot`).
- **Двойной клик по строке** - подтверждение → `webui.kill_pid(pid)` → `SIGTERM`. Успешно убитая строка гаснет (`tr.killed { opacity: 0.4 }`). При `rc != 0` всплывает alert (нет прав / процесс уже исчез).
- На backend'е защита: `pid <= 1` (init / 0) сразу возвращает `-1`, `kill(2)` не вызывается.

**Авто-обновление**:
- На загрузке страницы фронт делает pull: `webui.get_snapshot()`.
- Дальше backend каждую секунду сам пушит снапшот - вызывает `updateSnapshot('<json>')` через `win.run(...)`. Pusher останавливается, когда окно закрыто (`win.is_shown() == false`) или пришёл SIGINT / SIGTERM.

## API

UI и backend связаны через WebUI.

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

`cpus[0]` с `id == -1` - агрегат по всем ядрам; остальные элементы - отдельные ядра. Список процессов отсортирован по `cpu_pct` desc, обрезан до 50 элементов (`max_processes` в `to_json`).

## Архитектура

`pc-monitor` - один процесс с двумя потоками:

- **главный поток**: `webui::window` поднимает локальный HTTP-сервер, отдаёт статику из `frontend/`, держит WebSocket к открытой странице. Биндит колбэки (`get_snapshot`, `kill_pid`) и блокируется на `webui::wait()`.
- **фоновой pusher**: раз в секунду берёт мьютекс, дёргает `SystemMonitor::snapshot()`, сериализует в JSON через `nlohmann::json`, шлёт фронту вызовом `win.run("updateSnapshot('...')")`.

`app.ts` компилируется в `app.js` автоматически на стадии сборки (`add_custom_target(frontend_ts)` в `CMakeLists.txt`). После линковки бинаря POST_BUILD копирует `src/frontend/` рядом с `pc-monitor`.

## Зависимости

- [webui-dev/webui](https://github.com/webui-dev/webui).
- [nlohmann/json](https://github.com/nlohmann/json).

Build-time:
- CMake >= 3.16, g++/clang, git.
- **Node.js** - для `npx` либо глобальный `tsc` (`npm i -g typescript`). Используется CMake'ом для компиляции `app.ts → app.js`.
