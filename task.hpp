#pragma once
#include "proc.hpp"

namespace procebo::task {
    template<typename Func, typename... Args>
    inline proc::descriptor spawn(Func&& func, Args&&... args) {
        proc::descriptor task_proc = proc::fork();

        if (proc::is_child(task_proc)) {
            func(task_proc, args...);

            std::exit(0);
        }

        return task_proc;
    }
}