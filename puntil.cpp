#define _POSIX_C_SOURCE

#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
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

bool isNumber(const char*);
bool checkPid(pid_t);
const std::map<const std::string, std::vector<pid_t>> buildProcessMap(void);

bool checkPid(pid_t pid) {
    int saved_errno = errno;
    int rc = kill(pid, 0);

    bool rt = rc == -1 && errno == ESRCH;
    errno = saved_errno;
    return rt;
}

bool isNumber(const char *arg) {
    while(*arg) {
        if(!isdigit(*arg++)) return false;
    }

    return true;
}

const std::map<const std::string, std::vector<pid_t>> buildProcessMap(void) {
    static const std::string proc_path = "/lmao";

    std::map<const std::string, std::vector<pid_t>> processes;

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
                processes[name].push_back(std::atoi(proc_entry->d_name));
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

    const std::map<const std::string, std::vector<pid_t>> processes = buildProcessMap();

    //for(std::map<const std::string, std::vector<pid_t>>::const_iterator it = processes.begin(); it != processes.end(); it++) {
        //std::cout << it->second.size() << '\n';
    //}

    return 0;
}
