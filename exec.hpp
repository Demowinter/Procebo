#pragma once
#include <string>
#include <vector>
#include <unistd.h>
#include "proc.hpp"

extern char** environ;

namespace procebo::exec {
    proc::descriptor spawn(const std::string& path, const std::vector<std::string>& argv = {}, const std::vector<std::string>& env = {}) {
        proc::descriptor task_proc = proc::fork();

        if (proc::is_child(task_proc)) {
            std::vector<char*> raw_argv;
            std::vector<char*> raw_env;

            raw_argv.push_back(const_cast<char*>(path.c_str()));

            for (const std::string& str : argv) raw_argv.push_back(const_cast<char*>(str.c_str()));
            for (const std::string& str : env) raw_env.push_back(const_cast<char*>(str.c_str()));

            raw_argv.push_back(nullptr);
            raw_env.push_back(nullptr);

            pipe::descriptor task_pipe = proc::getpipe(task_proc);

            dup2(task_pipe.rfd, STDIN_FILENO);
            dup2(task_pipe.wfd, STDOUT_FILENO);
            dup2(task_pipe.wfd, STDERR_FILENO);

            procebo::pipe::close(task_pipe);

            if (env.size()) execve(path.c_str(), raw_argv.data(), raw_env.data());
            else execve(path.c_str(), raw_argv.data(), environ);
            _exit(errno);
        }

        return task_proc;
    }
}