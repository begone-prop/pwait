#define _POSIX_C_SOURCE

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
    (void) argc;
    (void) argv;

    bool gathered_proc_info = false;
    std::vector<process> processes;

    int opt;
    while((opt = getopt_long(argc, argv, "t:", long_options, NULL)) != -1) {
        switch(opt) {
            case 't':
                std::cout << "Timeout option given, value is " << optarg << '\n';
                break;

            case 'i':
                std::cout << "Interval option given, value is " << optarg << '\n';
                break;

            default:
                std::cerr << "Invalid argument\n";
                break;
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

    size_t pid_done = 0;
    bool check_pids = true;


    for(size_t idx = 0; idx < target_pids.size(); idx++) {
        std::cout << target_pids[idx] << (idx + 1 == target_pids.size() ? "\n" : ", ");
    }

    if(target_pids.size() == 0) return 0;

    size_t count_done = 0;

    //for(size_t idx = 0; idx < target_pids.size(); idx++) {
        //std::cout << checkPid(target_pids[idx]) << '\n';
    //}

    struct timespec req, rem;

    while(check_pids) {
        for(size_t idx = 0; idx < target_pids.size(); idx++) {
            if(target_pids[idx] == 0 || checkPid(target_pids[idx])) continue;

            target_pids[idx] = 0;

            if(++count_done == target_pids.size()) {
                check_pids = false;
                break;
            }
        }

        sleep(1);
    }

    return 0;
}
