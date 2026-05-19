#include "SystemMonitor.hpp"

#include <webui.hpp>
#include <nlohmann/json.hpp>

#include <sys/types.h>
#include <signal.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

namespace {
    std::atomic_bool g_running{true};
    std::mutex g_monitor_mtx;
    SystemMonitor g_monitor;

    std::string escape_for_js(const std::string &src) {
        std::string out;
        out.reserve(src.size() + 16);
        for (char c: src) {
            switch (c) {
                case '\\': out += "\\\\";
                    break;
                case '\'': out += "\\'";
                    break;
                case '\n': out += "\\n";
                    break;
                case '\r': out += "\\r";
                    break;
                case '\t': out += "\\t";
                    break;
                default: out += c;
            }
        }
        return out;
    }

    std::filesystem::path locate_frontend(const char *argv0) {
        namespace fs = std::filesystem;
        fs::path exe = fs::weakly_canonical(fs::path(argv0));
        fs::path bin_dir = exe.parent_path();
        for (const fs::path &candidate: {
                 bin_dir / "frontend",
                 bin_dir.parent_path() / "src" / "frontend",
             }) {
            if (fs::exists(candidate / "index.html")) return candidate;
        }
        return bin_dir / "frontend";
    }

    void on_kill(webui::window::event *e) {
        int pid = static_cast<int>(e->get_int());
        if (pid <= 1) {
            e->return_int(-1);
            return;
        }
        e->return_int(::kill(pid, SIGTERM));
    }

    void on_force_kill(webui::window::event *e) {
        int pid = static_cast<int>(e->get_int());
        if (pid <= 1) {
            e->return_int(-1);
            return;
        }
        e->return_int(::kill(pid, SIGKILL));
    }

    void on_snapshot_request(webui::window::event *e) {
        nlohmann::json j;
        {
            std::lock_guard<std::mutex> lk(g_monitor_mtx);
            j = to_json(g_monitor.snapshot());
        }
        e->return_string(j.dump());
    }
}

int main(int argc, char **argv) {
    std::signal(SIGINT, [](int) {
        g_running = false;
        webui::exit();
    });
    std::signal(SIGTERM, [](int) {
        g_running = false;
        webui::exit();
    });
    std::signal(SIGPIPE, SIG_IGN);

    webui::window win;
    win.bind("get_snapshot", on_snapshot_request);
    win.bind("kill_pid", on_kill);
    win.bind("force_kill", on_force_kill);

    auto frontend = locate_frontend(argv[0]);

    if (!std::filesystem::exists(frontend / "index.html")) {
        std::cerr << "frontend not found at " << frontend << "\n";
        return 1;
    }
    win.set_root_folder(frontend.string());

    {
        std::lock_guard<std::mutex> lk(g_monitor_mtx);
        g_monitor.snapshot();
    }

    if (!win.show("index.html")) {
        auto url = win.start_server("index.html");
        if (url.empty()) {
            std::cerr << "failed to start webui server\n";
            return 1;
        }
        std::cerr << "open in your browser: " << url << "\n";
    }

    std::thread pusher([&] {
        using namespace std::chrono_literals;
        while (g_running.load()) {
            std::this_thread::sleep_for(1s);
            if (!win.is_shown()) break;
            nlohmann::json j;
            {
                std::lock_guard<std::mutex> lk(g_monitor_mtx);
                j = to_json(g_monitor.snapshot());
            }
            std::string script = "window.updateSnapshot && updateSnapshot('"
                                 + escape_for_js(j.dump()) + "')";
            win.run(script);
        }
    });

    webui::wait();
    g_running = false;
    if (pusher.joinable()) pusher.join();
    return 0;
}
