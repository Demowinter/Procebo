#pragma once
#include <unordered_map>
#include <stdexcept>
#include <mutex>
#include <optional>
#include <limits>
#include <random>
#include <tuple>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>
#include "pipe.hpp"

namespace procebo::proc {
    namespace runtime {
        struct proc_entry {
            int32_t procid;
            uint64_t fingerprint;

            bool child;
            bool alive;

            int32_t ecode;

            pipe::descriptor pipe;
        };

        struct descriptor {
            uint32_t id;
            uint64_t fingerprint;
        };

        inline std::unordered_map<int32_t, proc_entry> proc_table;
        inline std::mutex proc_table_mutex;

        inline std::unique_lock<std::mutex> lock_table() {
            return std::unique_lock(proc_table_mutex);
        }

        inline proc_entry& get_entry(descriptor desc) {
            auto it = proc_table.find(desc.id);

            if (it == proc_table.end()) throw std::runtime_error("Invalid process descriptor");
            if (it->second.fingerprint != desc.fingerprint) throw std::runtime_error("Invalid process descriptor");

            return it->second;
        }

        inline void update_state(descriptor desc) {
            auto lockt = lock_table();
            auto& entry = get_entry(desc);

            siginfo_t info{};
            int32_t wret = ::waitid(P_PID, entry.procid, &info, WEXITED | WNOWAIT | WNOHANG);

            entry.alive = (!wret) ? !info.si_pid : false;

            if (!entry.alive) entry.ecode = info.si_status;
        }

        namespace utils {
            inline std::optional<uint32_t> find_unused_descriptor() {
                auto lockt = lock_table();

                for (uint32_t i = 0; i < std::numeric_limits<uint32_t>::max(); i++) 
                    if (proc_table.find(i) == proc_table.end()) return i;

                return std::nullopt;
            }

            inline uint64_t gen_fingerprint() {
                static std::random_device rd;
                static std::mt19937_64 mt(rd());

                return mt();
            }
        }
    }

    using descriptor = runtime::descriptor;
    using safe_entry = std::tuple<std::unique_lock<std::mutex>, std::unique_lock<std::mutex>, runtime::proc_entry*>;

    
    inline descriptor fork() {
        auto desc_opt = runtime::utils::find_unused_descriptor();
        
        if (!desc_opt.has_value()) throw std::runtime_error("Fork error!");

        uint32_t descid = desc_opt.value();
        uint64_t fingerprint = runtime::utils::gen_fingerprint();

        auto lockt = runtime::lock_table();

        runtime::proc_entry& proc = runtime::proc_table.try_emplace(descid).first->second;
        proc.fingerprint = fingerprint;
        
        pipe::descriptor pipe_pwcr = pipe::create();
        pipe::descriptor pipe_prcw = pipe::create();

        proc.procid = ::fork();

        if (proc.procid < 0) throw std::runtime_error("Fork error!");

        else if (!proc.procid) {
            proc.procid = ::getpid();
            proc.child = true;

            close(pipe_pwcr.wfd);
            close(pipe_prcw.rfd);

            proc.pipe.rfd = pipe_pwcr.rfd;
            proc.pipe.wfd = pipe_prcw.wfd;
        }

        else {
            proc.child = false;

            close(pipe_pwcr.rfd);
            close(pipe_prcw.wfd);

            proc.pipe.rfd = pipe_prcw.rfd;
            proc.pipe.wfd = pipe_pwcr.wfd;
        }

        proc.alive = true;

        return {descid, fingerprint};
    }

    inline int32_t getprocid(descriptor desc) {
        auto lockt = runtime::lock_table();
        auto& entry = runtime::get_entry(desc);

        return entry.procid;
    }

    inline bool is_child(descriptor desc) {
        auto lockt = runtime::lock_table();
        auto& entry = runtime::get_entry(desc);

        return entry.child;
    }

    inline bool is_alive(descriptor desc) {
        runtime::update_state(desc);

        auto lockt = runtime::lock_table();
        auto& entry = runtime::get_entry(desc);

        return entry.alive;
    }

    inline std::optional<int32_t> exit_code(descriptor desc) {
        runtime::update_state(desc);

        auto lockt = runtime::lock_table();
        auto& entry = runtime::get_entry(desc);

        if (entry.alive) return std::nullopt;

        return entry.ecode;
    }

    inline pipe::descriptor getpipe(descriptor desc) {
        auto lockt = runtime::lock_table();
        auto& entry = runtime::get_entry(desc);

        return entry.pipe;
    }
    
    inline void wait(descriptor desc, int options = WEXITED | WNOWAIT) {
        if (::waitid(P_PID, getprocid(desc), nullptr, options) < 0) throw std::runtime_error("Wait error!");
    }

    inline void terminate(descriptor desc) {
        if (::kill(getprocid(desc), SIGTERM) < 0) throw std::runtime_error("Terminate error!");
    }

    inline void kill(descriptor desc) {
        auto lockt = runtime::lock_table();
        auto& entry = runtime::get_entry(desc);

        if (::kill(entry.procid, SIGKILL) < 0) throw std::runtime_error("Kill error!");
    }

    inline void cleanup(descriptor desc) {
        if (is_alive(desc)) wait(desc, WEXITED);

        auto lockt = runtime::lock_table();
        
        runtime::proc_table.erase(desc.id);
    }
}