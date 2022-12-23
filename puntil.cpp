#define _POSIX_C_SOURCE 200112L

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

#include <cstdlib>
#include <cctype>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <time.h>

#define TIMEOUT_SIGNAL SIGUSR1
#define USEC 1e8

static const struct option long_options[] = {
    {"timeout", required_argument, NULL, 't'},
    {"interval", required_argument, NULL, 'n'},
    {"help", no_argument, NULL, 'h'},
};

struct process {
    std::string name;
    pid_t pid;
};

bool isNumber(const char*);
bool checkPid(pid_t);
std::vector<process> buildProcessVec(void);
void timerExpire(int);

static volatile sig_atomic_t timer_done = 0;

void timerExpire(int signo) {
    (void) signo;
    timer_done = 1;
}

bool checkPid(pid_t pid) {
    int saved_errno = errno;
    int rc = kill(pid, 0);

    bool rt = rc == -1 && errno == ESRCH;
    errno = saved_errno;
    return !rt;
}

bool isNumber(const char *arg) {
    while(*arg) {
        if(!isdigit(*arg++)) return false;
    }

    return true;
}

std::vector<process> buildProcessMap(void) {
    static const std::string proc_path = "/proc";

    std::vector<process> processes;

    DIR* procd = opendir(proc_path.c_str());
    if(!procd) return processes;

    int procd_fd = dirfd(procd);

    struct dirent* proc_entry;
    struct stat entry_info;

    while((proc_entry = readdir(procd))) {

        // Use stat to determine filetype since d_type is not portable, see readdir(3)
        if(fstatat(procd_fd, proc_entry->d_name, &entry_info, 0) == -1) {
            std::cerr << "Failed to open " << proc_entry->d_name << '\n';
            continue;
        }

        if(!S_ISDIR(entry_info.st_mode) ||
                (!strcmp(proc_entry->d_name, "..") ||
                 !strcmp(proc_entry->d_name, ".")) ||
                !isNumber(proc_entry->d_name)) {

            continue;
        }

        std::string status_path = proc_path + "/" + proc_entry->d_name + "/status";
        std::ifstream status_file(status_path);

        if(!status_file) {
            std::cerr << "warn: Failed to open " << status_path << '\n';
            continue;
        }

        std::string line;

        while(std::getline(status_file, line)) {
            if(line.find("Name") == 0) {
                std::stringstream ss(line);
                std::string _, name;
                // TODO: Better name handling
                ss >> _ >> name;
                processes.push_back({name, std::atoi(proc_entry->d_name)});
                break;
            }
        }
    }

    closedir(procd);
    return processes;
}

int main(int argc, char** argv) {

    bool gathered_proc_info = false;
    std::vector<process> processes;

    double timeout = -1;
    double sleep_interval = -1;

    int opt;
    while((opt = getopt_long(argc, argv, "t:i:", long_options, NULL)) != -1) {
        switch(opt) {
            char* end;
            case 't': {
                end = NULL;
                double temp = strtod(optarg, &end);

                if(*end != '\0' || temp < 0) {
                    std::cerr << "Invalid argument " << optarg << '\n';
                    return 1;
                }

                timeout = temp > 0.1 ? temp : 0.1;
                break;

            }

            case 'i': {
                end = NULL;
                double temp = strtod(optarg, &end);
                if(*end != '\0' || temp < 0) {
                    std::cerr << "Invalid argument " << optarg << '\n';
                    return 1;
                }

                sleep_interval = temp > 0.1 ? temp : 0.1;
                break;
            }

            default:
                std::cerr << "Invalid argument\n";
                return 1;
        }
    }

    if(argc == optind) {
        std::cerr << program_invocation_short_name << ": No pids given\n";
        return 1;
    }

    std::vector<pid_t> target_pids;

    for(int idx = optind; idx < argc; idx++) {
        if(isNumber(argv[idx]))
            target_pids.push_back(std::atoi(argv[idx]));
        else {
            if(!gathered_proc_info) {
                processes = buildProcessMap();
                gathered_proc_info = true;
            }

            for(size_t i = 0; i < processes.size(); i++) {
                if(processes[i].name.find(argv[idx]) != std::string::npos) {
                    target_pids.push_back(processes[i].pid);
                }
            }
        }
    }

    if(target_pids.size() == 0) return 1;

    if(timeout != -1) {
        struct sigaction action;
        sigemptyset(&action.sa_mask);
        action.sa_flags = 0;
        action.sa_handler = timerExpire;

        if(sigaction(TIMEOUT_SIGNAL, &action, NULL) == -1) {
            std::cerr << "Failed to establish signal handler\n";
            return 1;
        }

        struct sigevent event;

        event.sigev_notify = SIGEV_SIGNAL;
        event.sigev_signo = TIMEOUT_SIGNAL;

        timer_t t_id;
        if(timer_create(CLOCK_REALTIME, &event, &t_id) == -1) {
            std::cerr << "Failed to create timer\n";
            return 1;
        }

        //struct timespec req;
        struct itimerspec req;
        req.it_interval = {0, 0};
        req.it_value.tv_sec = (long) timeout;
        req.it_value.tv_nsec = (long) ((timeout - (long) timeout) / 0.1) * USEC;

        if(timer_settime(t_id, 0, &req, NULL) == -1) {
            std::cerr << "Failed to set timer\n";
            return 1;
        }
    }

    struct timespec req;
    if(sleep_interval != -1) {
        req.tv_sec = (long) sleep_interval;
        req.tv_nsec = (long) ((sleep_interval - (long) sleep_interval) / 0.1) * USEC;
    } else {
        req.tv_sec = 1;
        req.tv_nsec = 0;
    }

    size_t count_done = 0;
    const size_t num_pids = target_pids.size();

    while(!timer_done) {
        for(size_t idx = 0; idx < target_pids.size(); idx++) {
            if(target_pids[idx] == 0 || checkPid(target_pids[idx])) continue;

            target_pids[idx] = 0;

            if(++count_done == num_pids) {
                goto out;
            }
        }

        clock_nanosleep(CLOCK_REALTIME, 0, &req, NULL);
    }

    out:
    int rv = !(count_done == num_pids);
    return rv;
}
