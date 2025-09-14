#include "Commands.h"
#include <dirent.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>
#include <string>
#include <unistd.h>
#include <vector>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/sysinfo.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/unistd.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <signal.h>
#include <array>
#include <cctype>
#include <algorithm>

const std::string WHITESPACE = " \n\r\t\f\v";

using std::cin;
using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::vector;
using std::regex;
using std::regex_match;
using std::map;
using std::pair;

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

#define BUF_SIZE 4096

const vector<string> RESERVED_KEYWORDS = {
    "chprompt",
    "showpid",
    "pwd",
    "cd",
    "jobs",
    "fg",
    "quit",
    "kill",
    "alias",
    "unalias",
    "unsetenv",
    "watchproc",
    "du",
    "whoami",
    "netinfo"
};

string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}


vector<string> _getParams(const string& str) {
    vector<string> params;
    string word;
    bool skipFirst = true;

    for (char c : str) {
        if (isspace(c)) {
            if (!word.empty()) {
                if (!skipFirst) {
                    params.push_back(word);
                }
                skipFirst = false;
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty() && !skipFirst) {
        params.push_back(word);
    }

    return params;
}


bool _isNum(const string& str) {
    if (str.empty()) return false;
    return all_of(str.begin(), str.end(), ::isdigit);
}


int _parseCommandLine(const char *cmd_line, char **args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

int _countParams(const char* cmd_line) {
    if (!cmd_line) return 0;

    char* args[COMMAND_MAX_ARGS];
    int argCount = _parseCommandLine(cmd_line, args);

    // Free the allocated memory
    for (int i = 0; i < argCount; i++) {
        free(args[i]);
    }

    // Subtract 1 to exclude the command name
    return argCount > 0 ? argCount - 1 : 0;
}


bool _isBackgroundComamnd(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

bool isReserved(string cmd_) {
    cmd_ = _trim(cmd_);
    for(const auto& s : RESERVED_KEYWORDS) {
        if(s == cmd_) {
            return true;
        }
    }
    return false;
}



std::pair<std::string, size_t> findToken(const char* cmdLine) {
    bool inSingleQuotes = false;
    bool inDoubleQuotes = false;

    size_t len = strlen(cmdLine);

    for (size_t i = 0; i < len; ++i) {
        char c = cmdLine[i];

        if (c == '\'' && !inDoubleQuotes) {
            inSingleQuotes = !inSingleQuotes;
        } else if (c == '"' && !inSingleQuotes) {
            inDoubleQuotes = !inDoubleQuotes;
        } else if (!inSingleQuotes && !inDoubleQuotes) {
            if (c == '>' && cmdLine[i + 1] == '>') {
                return { ">>", i };
            }
            if (c == '>') {
                return { ">", i };
            }
            if (c == '|' && cmdLine[i + 1] == '&') {
                return { "|&", i };
            }
            if (c == '|') {
                return { "|", i };
            }
        }
    }

    return { "", len };  // no token found
}


bool stringReplaceFirst(std::string& str, const std::string& from, const std::string& to) {
    size_t startPos = str.find(from);
    if (startPos == std::string::npos) {
        return false;  // no occurrence found
    }
    str.replace(startPos, from.length(), to);
    return true;  // replaced successfully
}





SmallShell::SmallShell() :  pid(getpid()), prompt("smash"), lastDirectory(""), currentProcess(-1), currentCommand("") {
}

SmallShell::~SmallShell() {
}

Command::Command(const char *cmd_line_str) : cmd_line(cmd_line_str), pid(getpid()) {
    args = new char*[COMMAND_MAX_ARGS];
    for (int i = 0; i < COMMAND_MAX_ARGS; i++) args[i] = nullptr;
    char* temp_parse_buffer = new char[cmd_line.length() + 1];
    strcpy(temp_parse_buffer, cmd_line.c_str());
    numArgs = _parseCommandLine(temp_parse_buffer, args);
    delete[] temp_parse_buffer;
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line) {
    string cmd_s = _trim(string(cmd_line));
    if (cmd_s.size() == 0) return NULL;
    string firstWord = _trim(cmd_s.substr(0, cmd_s.find_first_of(" ")));

    if(this->getAlias().find(firstWord) != this->getAlias().end()) {
        stringReplaceFirst(cmd_s, firstWord, getAlias().at(firstWord));
    }

    firstWord = _trim(cmd_s.substr(0, cmd_s.find_first_of(" ")));

    auto specialToken = findToken(cmd_s.c_str());
    cmd_line = cmd_s.c_str();
    if (specialToken.first == ">" || specialToken.first == ">>") {
        return new RedirectionCommand(cmd_line);
    }

    if (specialToken.first == "|" || specialToken.first == "|&") {
        return new PipeCommand(cmd_line);
    }

    if (firstWord.compare("chprompt") == 0) {
      return new ChangeSmashPrompt(cmd_line);
    }
    else if (firstWord.compare("showpid") == 0) {
      return new ShowPidCommand(cmd_line);
    }
    else if (firstWord.compare("pwd") == 0) {
      return new GetCurrDirCommand(cmd_line);
    }
    else if (firstWord.compare("cd") == 0) {
      return new ChangeDirCommand(cmd_line, &lastDirectory);
    }
    else if (firstWord.compare("jobs") == 0) {
      return new JobsCommand(cmd_line, &jobs);
    }
    else if (firstWord.compare("fg") == 0) {
      return new ForegroundCommand(cmd_line, &jobs);
    }
    else if (firstWord.compare("quit") == 0) {
      return new QuitCommand(cmd_line, &jobs);
    }
    else if (firstWord.compare("kill") == 0) {
      return new KillCommand(cmd_line, &jobs);
    }
    else if (firstWord.compare("alias") == 0) {
      return new AliasCommand(cmd_line);
    }
    else if (firstWord.compare("unalias") == 0) {
      return new UnAliasCommand(cmd_line);
    }
    else if (firstWord.compare("unsetenv") == 0) {
      return new UnSetEnvCommand(cmd_line);
    }
    else if (firstWord == "watchproc") {
      return new WatchProcCommand(cmd_line);
    }
    else if (firstWord == "du") {
        return new DiskUsageCommand(cmd_line);
    }
    else if (firstWord == "whoami") {
        return new WhoAmICommand(cmd_line);
    }
    else if (firstWord == "netinfo") {
        return new NetInfo(cmd_line);
    }
    else {
    return new ExternalCommand(cmd_line);
    }
    return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
    jobs.removeFinishedJobs();
    Command* cmd = CreateCommand(cmd_line);
    if (cmd == nullptr) {
        setCurrentProcess(-1);
        setCurrentCommand("");
        return;
    }
    cmd->execute();
    delete cmd;
    currentProcess = -1;
    currentCommand = "";
}

void ChangeSmashPrompt::execute() {
    if (numArgs > 1 && args[1][0] != '&') {
        prompt = args[1];
    } else {
        prompt = "smash";
    }
    SmallShell::getInstance().setPrompt(prompt);
}

void ShowPidCommand::execute() {
    cout << "smash pid is " << getpid() << std::endl;
}


void GetCurrDirCommand::execute() {
    long max_size = pathconf(".", _PC_PATH_MAX);
    char *buffer = new char[max_size + 1];
    if (getcwd(buffer, max_size + 1) == NULL) {
        delete[] buffer;
        return;
    }
    cout << buffer << endl;
    delete[] buffer;
}

void ChangeDirCommand::execute() {
    if (numArgs > 2) {
        cerr << "smash error: cd: too many arguments" << endl;
        return;
    }
    string targetPath;
    if (numArgs == 1) {
        return;
    }
    targetPath = args[1];
    if (targetPath == "-") {
        if (lastDirectory && !lastDirectory->empty()) {
            targetPath = *lastDirectory;
        } else {
            cerr << "smash error: cd: OLDPWD not set" << endl;
            return;
        }
    }

    char current_wd_buf[PATH_MAX];
    if (getcwd(current_wd_buf, sizeof(current_wd_buf)) != NULL) {
        if (chdir(targetPath.c_str()) != 0) {
            perror("smash error: chdir failed");
        } else {
            if (lastDirectory) {
                *lastDirectory = current_wd_buf;
            }
        }
    } else {
        perror("smash error: getcwd failed");
        if (chdir(targetPath.c_str()) != 0) {
            perror("smash error: chdir failed");
        }
    }
}

void ForegroundCommand::execute() {

    JobsList::JobEntry* job = nullptr;
    int job_id_to_fg = -1;
    if (numArgs > 2) {
        cerr << "smash error: fg: invalid arguments" << endl;
        return;
    }

    if (numArgs == 2) {
        string param = args[1];
        if (!_isNum(param)) {
            cerr << "smash error: fg: invalid arguments" << endl;
            return;
        }
        try {
            job_id_to_fg = std::stoi(param);
        } catch (const std::out_of_range& oor) {
            cerr << "smash error: fg: invalid arguments" << endl;
            return;
        } catch (const std::invalid_argument& ia) {
            cerr << "smash error: fg: invalid arguments" << endl;
            return;
        }
        job = jobs->getJobById(job_id_to_fg);
        if (!job) {
            cerr << "smash error: fg: job-id " << job_id_to_fg << " does not exist" << endl;
            return;
        }
    } else {
        job = jobs->getLastJob();
        if (!job) {
            cerr << "smash error: fg: jobs list is empty" << endl;
            return;
        }
    }

    string command_to_run = job->command;
    pid_t pid_to_wait_for = job->pid;
    bool was_stopped = job->isStopped;
    int job_id_to_remove = job->id;

    cout << command_to_run << " " << pid_to_wait_for << endl;
    jobs->removeJobById(job_id_to_remove);

    SmallShell &shell = SmallShell::getInstance();
    shell.setCurrentProcess(pid_to_wait_for);
    shell.setCurrentCommand(command_to_run);

    if (was_stopped) {
        if (kill(pid_to_wait_for, SIGCONT) == -1) {
            perror("smash error: kill failed for SIGCONT");
        }
    }

    int status;
    if (waitpid(pid_to_wait_for, &status, WUNTRACED) == -1 && errno != ECHILD) {
        perror("smash error: waitpid failed");
    }

    shell.setCurrentProcess(-1);
    shell.setCurrentCommand("");
}

void KillCommand::execute() {
    if (numArgs != 3) {
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }

    string signal_str_arg = args[1];
    string job_id_str_arg = args[2];

    if (signal_str_arg.length() < 2 || signal_str_arg[0] != '-'
        || !_isNum(signal_str_arg.substr(1)) || !_isNum(job_id_str_arg)) {
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }

    int signal_num;
    int job_id;
    try {
        signal_num = stoi(signal_str_arg.substr(1));
        job_id = stoi(job_id_str_arg);
    } catch (const std::exception& e) {
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }

    jobs->removeFinishedJobs();
    JobsList::JobEntry *job = jobs->getJobById(job_id);
    if (!job) {
        cerr << "smash error: kill: job-id " << job_id << " does not exist" << endl;
        return;
    }

    if (kill(job->pid, signal_num) == -1) {
        perror("smash error: kill failed");
    } else {
        cout << "signal number " << signal_num << " was sent to pid " << job->pid << endl;
    }
}


void JobsList::addJob(Command *cmd, pid_t pid, bool isStopped) {
    removeFinishedJobs();
    int id;
    if (!innerList.empty()) {
        id = innerList.back()->id + 1;
    } else {
        id = 1;
    }
    string command = cmd->get_cmd_line();
    innerList.push_back(new JobEntry(command, id, pid, isStopped));
}

void JobsList::addJob(const string& command, pid_t pid, bool isStopped) {
    removeFinishedJobs();
    int new_job_id = 1;
    if (!innerList.empty()) {
        new_job_id = innerList.back()->id + 1;
    }
    innerList.push_back(new JobEntry(command, new_job_id, pid, isStopped));
}

void JobsList::printJobsList() {
    removeFinishedJobs();
    for (const auto& job : innerList) {
        cout << '[' << job->id << "] " << job->command << endl;
    }
}

void JobsList::printJobsList_Quit() {
    removeFinishedJobs();
    cout << "smash: sending SIGKILL signal to " << innerList.size() << " jobs:" << endl;
    for (const auto& job : innerList) {
        if (job) {
            cout << job->pid << ": " << job->command << endl;
        }
    }
}


void JobsList::killAllJobs() {
    for (JobEntry* job : innerList) {
        if (job) {
            if(kill(job->pid, SIGKILL) == -1) {
                perror("smash error: kill failed");
            }
            delete job;
        }
    }
    innerList.clear();
}

bool JobsList::isFinished(JobEntry* job) {
    if (job == nullptr) return true;
    int status;
    pid_t result = waitpid(job->pid, &status, WNOHANG);
    if (result == -1) {
        return true;
    }
    if (result == 0) {
        return false;
    }
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
        return true;
    }
    return false;
}

void JobsList::removeFinishedJobs() {
    innerList.remove_if([](JobEntry* job) {
        if(JobsList::isFinished(job)) {
            delete job;
            return true;
        }
        return false;
    });
}

JobsList::JobEntry *JobsList::getJobById(int jobId) const {
    for (const auto& job : innerList) {
        if (job->id == jobId) {
            return job;
        }
    }
    return nullptr;
}

JobsList::JobEntry *JobsList::getJobByPid(pid_t pid) const {
    for (const auto& job : innerList) {
        if (job != nullptr && job->pid == pid) {
            return job;
        }
    }
    return nullptr;
}

void JobsList::removeJobById(int jobId) {
    innerList.remove_if([jobId](JobEntry* job) {
        if (job != nullptr && job->id == jobId) {
            delete job;
            return true;
        }
        return false;
    });
}

JobsList::JobEntry *JobsList::getLastJob() {
    removeFinishedJobs();
    if (innerList.empty()) {
        return nullptr;
    }

    return innerList.back();
}


void JobsCommand::execute() {
    SmallShell &shell = SmallShell::getInstance();
    shell.getJobs().printJobsList();
}

void QuitCommand::execute() {
    if (numArgs >= 2 && string(args[1]).compare("kill") == 0) {
        jobs->printJobsList_Quit();
        jobs->killAllJobs();
    }
    exit(0);
}

void AliasCommand::execute() {
    regex regex("^alias [a-zA-Z0-9_]+='[^']*'$");
    if (!regex_match(cmd_line, regex)) {
        cerr << "smash error: alias: invalid alias format" << endl;
        return;
    }
    map<string, string> alias = SmallShell::getInstance().getAlias();
    if (numArgs == 1) {
        for (const auto& pair : alias) {
            cout << pair.first << "='" << pair.second << "'" << endl;
        }
        return;
    }
    string fullArgs;
    for (int i = 1; i < numArgs; i++) {
        if (i > 1) fullArgs += " ";
        fullArgs += args[i];
    }
    size_t eqPos = fullArgs.find('=');
    if (eqPos == string::npos) {
        cerr << "smash error: alias: invalid alias format" << endl;
        return;
    }
    string name = _trim(fullArgs.substr(0, eqPos));

    string command = _trim(fullArgs.substr(eqPos + 2, fullArgs.length() - eqPos - 3));
    if (alias.find(name) != alias.end() || isReserved(name)) {
        cerr << "smash error: alias: " << name << " already exists or is a reserved command" << endl;
        return;
    }
    alias[name] = command;
    SmallShell::getInstance().setAlias(alias);
}

void UnAliasCommand::execute() {
    if (numArgs == 1) {
        cerr << "smash error: unalias: not enough arguments" << endl;
        return;
    }
    map<string, string> alias = SmallShell::getInstance().getAlias();
    for (int i = 1; i < numArgs; i++) {
        if (alias.find(args[i]) == alias.end()) {
            cerr << "smash error: unalias: " << args[i] << " alias does not exist" << endl;
            return;
        }
        alias.erase(args[i]);
    }
    SmallShell::getInstance().setAlias(alias);
}

extern char **environ;

void UnSetEnvCommand::execute() {
    if (numArgs < 2) {
        cerr << "smash error: unsetenv: not enough arguments" << endl;
            return;
        }

        string proc_path = "/proc/" + std::to_string(getpid()) + "/environ";
        int proc_fd = open(proc_path.c_str(), O_RDONLY);
        if (proc_fd == -1) {
            perror("smash error: unsetenv: failed to open proc environ");
            return;
        }
        const size_t PROC_BUF_SIZE = 65536;
        char proc_buffer[PROC_BUF_SIZE];
        ssize_t bytes_read = read(proc_fd, proc_buffer, PROC_BUF_SIZE - 1);
        close(proc_fd);

        if (bytes_read <= 0) {
            if (bytes_read < 0) {
                 perror("smash error: unsetenv: failed to read proc environ");
            }
             bytes_read = 0;
        }

        if (bytes_read > 0 && bytes_read < PROC_BUF_SIZE) {
             proc_buffer[bytes_read] = '\0';
        } else if (bytes_read == PROC_BUF_SIZE -1) {
             proc_buffer[PROC_BUF_SIZE - 1] = '\0';
        }

        for (int i = 1; i < numArgs; i++) {
            string var = _trim(args[i]);
            if (var.empty()) continue;

            bool exists_in_proc = false;
            string prefix = var + "=";
            char* current_entry_start = proc_buffer;
            char* buffer_end = proc_buffer + bytes_read;

            while (current_entry_start < buffer_end && *current_entry_start != '\0') {
                 if (strncmp(current_entry_start, prefix.c_str(), prefix.length()) == 0) {
                     exists_in_proc = true;
                     break;
                 }
                 current_entry_start += strlen(current_entry_start) + 1;
            }
            if (!exists_in_proc) {
                cerr << "smash error: unsetenv: " << var << " does not exist" << endl;
                return;
            }
        }

        for (int i = 1; i < numArgs; i++) {
            string var = _trim(args[i]);
            if (var.empty()) continue;

            char **env_ptr = environ;
            while (*env_ptr != NULL) {
                if (strncmp(var.c_str(), *env_ptr, var.length()) == 0 && (*env_ptr)[var.length()] == '=') {
                    char **shift_target = env_ptr;
                    char **shift_source = env_ptr + 1;
                    while (true) {
                        *shift_target = *shift_source;
                        if (*shift_target == NULL) {
                            break;
                        }
                        shift_target++;
                        shift_source++;
                    }
                    break;
                } else {
                    env_ptr++;
                }
            }
        }
    }



void WatchProcCommand::execute() {
    if (numArgs != 2 || !_isNum(args[1])) {
        cerr << "smash error: watchproc: invalid arguments" << endl;
        return;
    }

    string pid = _trim(string(args[1]));
    string pidCpuStatsPath = "/proc/" + pid + "/stat";
    string pidMemStatsPath = "/proc/" + pid + "/statm";
    string cpuStatPath = "/proc/stat";

    struct sysinfo memoryInfo;
    sysinfo(&memoryInfo);

    auto pidCpuStats = std::array<char, 1024>{};
    auto pidMemStats = std::array<char, 1024>{};
    auto cpuStat = std::array<char, 1024>{};

    int pidCpuFile = open(pidCpuStatsPath.c_str(), O_RDONLY);
    if (pidCpuFile == -1) {
        cerr << "smash error: watchproc: pid " << pid << " does not exist" << endl;
        return;
    }

    int pidMemFile = open(pidMemStatsPath.c_str(), O_RDONLY);
    if (pidMemFile == -1) {
        close(pidCpuFile);
        cerr << "smash error: watchproc: pid " << pid << " does not exist" << endl;
        return;
    }

    int cpuFile = open(cpuStatPath.c_str(), O_RDONLY);
    if (cpuFile == -1) {
        close(pidCpuFile);
        close(pidMemFile);
        cerr << "smash error: watchproc: pid " << pid << " does not exist" << endl;
        return;
    }

    read(pidCpuFile, pidCpuStats.data(), pidCpuStats.size());
    read(cpuFile, cpuStat.data(), cpuStat.size());

    std::stringstream pidCpuStream1(pidCpuStats.data());
    std::stringstream cpuStream1(cpuStat.data());

    string dummy;
    string pidStats1[52];
    for (int i = 0; i < 52; i++) {
        pidCpuStream1 >> pidStats1[i];
    }
    unsigned long utime1 = std::stoul(pidStats1[13]);
    unsigned long stime1 = std::stoul(pidStats1[14]);
    unsigned long proc_time1 = utime1 + stime1;

    unsigned long user1, nice1, system1, idle1;
    cpuStream1 >> dummy >> user1 >> nice1 >> system1 >> idle1;
    unsigned long total_cpu1 = user1 + nice1 + system1 + idle1;

    usleep(100000);

    lseek(pidCpuFile, 0, SEEK_SET);
    lseek(cpuFile, 0, SEEK_SET);
    read(pidCpuFile, pidCpuStats.data(), pidCpuStats.size());
    read(cpuFile, cpuStat.data(), cpuStat.size());

    std::stringstream pidCpuStream2(pidCpuStats.data());
    std::stringstream cpuStream2(cpuStat.data());

    string pidStats2[52];
    for (int i = 0; i < 52; i++) {
        pidCpuStream2 >> pidStats2[i];
    }
    unsigned long utime2 = std::stoul(pidStats2[13]);
    unsigned long stime2 = std::stoul(pidStats2[14]);
    unsigned long proc_time2 = utime2 + stime2;

    unsigned long user2, nice2, system2, idle2;
    cpuStream2 >> dummy >> user2 >> nice2 >> system2 >> idle2;
    unsigned long total_cpu2 = user2 + nice2 + system2 + idle2;

    double cpu_usage = 0.0;
    if (total_cpu2 > total_cpu1) {
        cpu_usage = 100.0 * (proc_time2 - proc_time1) / (total_cpu2 - total_cpu1);
    }

    read(pidMemFile, pidMemStats.data(), pidMemStats.size());
    std::stringstream pidMemStream(pidMemStats.data());
    unsigned long size, rss;
    pidMemStream >> size >> rss;
    double mem_usage = rss * sysconf(_SC_PAGESIZE) / (1024.0 * 1024.0);

    cout << "PID: " << pid
         << " | CPU Usage: " << std::fixed << std::setprecision(1) << cpu_usage << "%"
         << " | Memory Usage: " << std::fixed << std::setprecision(1) << mem_usage << " MB"
         << endl;

    close(pidCpuFile);
    close(pidMemFile);
    close(cpuFile);
}


static void executeInChild(char* command_to_execute_str) {
    if (setpgrp() == -1) {
        perror("smash error: setpgrp failed");
        exit(1);
    }

    bool hasWildcards = (strchr(command_to_execute_str, '*') != NULL) ||
                        (strchr(command_to_execute_str, '?') != NULL);

    if (hasWildcards) {
        char bash_executable_path[] = "/bin/bash";
        char* bash_fork_args[] = {bash_executable_path, (char*)"-c", command_to_execute_str, nullptr};
        execv(bash_executable_path, bash_fork_args);
        perror("smash error: execv failed for complex command");

    } else {
        char* simple_cmd_parsed_args[COMMAND_MAX_ARGS + 1];
        for(int i = 0; i <= COMMAND_MAX_ARGS; ++i) {
            simple_cmd_parsed_args[i] = nullptr;
        }


        int arg_count = _parseCommandLine(command_to_execute_str, simple_cmd_parsed_args);

        if (arg_count > 0 && simple_cmd_parsed_args[0] != nullptr) {
            execvp(simple_cmd_parsed_args[0], simple_cmd_parsed_args);
            perror("smash error: execvp failed");
        } else {
            cerr << "smash error: failed to parse simple command for execvp" << endl;
        }

        for (int i = 0; i < arg_count; ++i) {
            if (simple_cmd_parsed_args[i] != nullptr) {
                free(simple_cmd_parsed_args[i]);
            }
        }
    }
    exit(1);
}


void ExternalCommand::execute() {
    SmallShell &shell = SmallShell::getInstance();
    string command_for_job_entry = _trim(this->cmd_line);

    if (command_for_job_entry.length() >= COMMAND_MAX_LENGTH) {
        cerr << "smash error: command too long" << endl;
        return;
    }


    char commandLine_for_exec[COMMAND_MAX_LENGTH];
    strcpy(commandLine_for_exec, command_for_job_entry.c_str());

    bool isBackground = _isBackgroundComamnd(commandLine_for_exec);
    if (isBackground) {
        _removeBackgroundSign(commandLine_for_exec);
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("smash error: fork failed");
        return;
    }

    if (pid == 0) {
        executeInChild(commandLine_for_exec);
    } else {
        if (isBackground) {
            shell.getJobs().addJob(command_for_job_entry, pid, false);
        } else {
            shell.setCurrentProcess(pid);
            shell.setCurrentCommand(string(commandLine_for_exec));
            int status;
            if (waitpid(pid, &status, WUNTRACED) == -1) {
                perror("smash error: waitpid failed");
            }
        }
    }
}


RedirectionCommand::RedirectionCommand(const char *cmd_line) : Command(cmd_line), isAppend(false), stdoutCopy(-1), fd(-1), isOpened(false) {
    string line = _trim(string(this->cmd_line));
    size_t redir_op_pos = string::npos;
    int opLen = 0;

    auto token = findToken(line.c_str());

    if (token.first == ">>") {
        isAppend = true;
        opLen = 2;
    } else if (token.first == ">") {
        isAppend = false;
        opLen = 1;
    } else {
        return;
    }
    redir_op_pos = token.second;

    if (redir_op_pos == 0) {
        if (line.length() == opLen || _trim(line.substr(opLen)).empty()) {
             cerr << "smash error: RedirectionCommand: Missing file path after operator in: [" << line << "]" << endl;
             return;
        }
    } else {
      command = _trim(line.substr(0, redir_op_pos));
    }

    if (redir_op_pos + opLen >= line.length()) {
        cerr << "smash error: RedirectionCommand: Missing file path after operator in: [" << line << "]" << endl;
        return;
    }
    path = _trim(line.substr(redir_op_pos + opLen));
    if (path.empty()){
        cerr << "smash error: RedirectionCommand: Missing file path after operator in: [" << line << "]" << endl;
        return;
    }

    stdoutCopy = dup(STDOUT_FILENO);
    if (stdoutCopy == -1) {
        perror("smash error: dup failed for stdoutCopy");
        return;
    }

    int temp_fd = -1;
    int open_flags = O_WRONLY | O_CREAT;
    if (isAppend) {
        open_flags |= O_APPEND;
    } else {
        open_flags |= O_TRUNC;
    }
    temp_fd = open(this->path.c_str(), open_flags, 0666);

    if (temp_fd == -1) {
        perror("smash error: open failed for redirection path");
        close(stdoutCopy);
        stdoutCopy = -1;
        return;
    }

    if (dup2(temp_fd, STDOUT_FILENO) == -1) {
        perror("smash error: dup2 failed to redirect STDOUT_FILENO to file");
        close(temp_fd);
        close(stdoutCopy);
        stdoutCopy = -1;
        return;
    }

    this->fd = temp_fd;
    isOpened = true;
}

void RedirectionCommand::execute() {
    if (isOpened) {
        SmallShell::getInstance().executeCommand(command.c_str());

        if (this->fd != -1) {
            if (close(this->fd) == -1) {
                perror("smash error: close failed for redirection file fd");
            }
            this->fd = -1;
        }
    }

    if (this->stdoutCopy != -1) {
        if (dup2(this->stdoutCopy, STDOUT_FILENO) == -1) {
            perror("smash error: dup2 failed to restore STDOUT_FILENO");
        }
        if (close(this->stdoutCopy) == -1) {
            perror("smash error: close failed for stdoutCopy");
        }
        this->stdoutCopy = -1;
    }
}

void PipeCommand::execute() {
    string tmpCmd = _trim(this->cmd_line);
    bool redirectErr = false;
    string firstCommand;
    string secondCommand;
    int pipeFd[2];
    auto token = findToken(tmpCmd.c_str());

    if (token.first == "|&") {
        redirectErr = true;
        firstCommand = _trim(tmpCmd.substr(0, token.second));
        secondCommand = _trim(tmpCmd.substr(token.second + 2));
    } else if (token.first == "|") {
        redirectErr = false;
        firstCommand = _trim(tmpCmd.substr(0, token.second));
        secondCommand = _trim(tmpCmd.substr(token.second + 1));
    } else {
        cerr << "smash error: PipeCommand: Invalid pipe command format (no | or |& found)" << endl;
        return;
    }

    if (firstCommand.empty() || secondCommand.empty()) {
        cerr << "smash error: PipeCommand: Missing command on one side of the pipe" << endl;
        return;
    }

    if (pipe(pipeFd) == -1) {
        perror("smash error: pipe failed");
        return;
    }

    SmallShell &shell = SmallShell::getInstance();
    pid_t pid1, pid2;

    pid1 = fork();
    if (pid1 < 0) {
        perror("smash error: fork failed for first pipe command");
        close(pipeFd[0]);
        close(pipeFd[1]);
        return;
    }

    if (pid1 == 0) {
        close(pipeFd[0]);

        if (redirectErr) {
            if (dup2(pipeFd[1], STDERR_FILENO) == -1) {
                perror("smash error: dup2 failed for command1 stderr");
                exit(1);
            }
        } else {
            if (dup2(pipeFd[1], STDOUT_FILENO) == -1) {
                perror("smash error: dup2 failed for command1 stdout");
                exit(1);
            }
        }
        close(pipeFd[1]);


        shell.executeCommand(firstCommand.c_str());
        exit(0);
    }

    pid2 = fork();
    if (pid2 < 0) {
        perror("smash error: fork failed for second pipe command");
        close(pipeFd[0]);
        close(pipeFd[1]);
        int status_pid1;
        waitpid(pid1, &status_pid1, 0);
        return;
    }

    if (pid2 == 0) {
        close(pipeFd[1]);

        if (dup2(pipeFd[0], STDIN_FILENO) == -1) {
            perror("smash error: dup2 failed for command2 stdin");
            exit(1);
        }
        close(pipeFd[0]);

        shell.executeCommand(secondCommand.c_str());
        exit(0);
    }

    close(pipeFd[0]);
    close(pipeFd[1]);

    int status_child1, status_child2;
    if (waitpid(pid1, &status_child1, 0) == -1) {
        perror("smash error: waitpid failed for first pipe command");
    }
    if (waitpid(pid2, &status_child2, 0) == -1) {
        perror("smash error: waitpid failed for second pipe command");
    }
}

long DiskUsageCommand::compute(const string &dir) {
    long totalSize = 0;
    long tmp = 0;

    int fd = open(dir.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd == -1) {
        perror("smash error: du: open failed");
        return -2;
    }

    char buf[BUF_SIZE];
    struct linux_dirent64 {
        ino64_t        d_ino;
        off64_t        d_off;
        unsigned short d_reclen;
        unsigned char  d_type;
        char           d_name[];
    };

    while (true) {
        int numBytes = syscall(SYS_getdents64, fd, buf, BUF_SIZE);
        if (numBytes == -1) {
            perror("smash error: du: directory");
            close(fd);
            return -1;
        }
        if (numBytes == 0) break;

        for (int bpos = 0; bpos < numBytes;) {
            struct linux_dirent64 *d = (struct linux_dirent64 *)(buf + bpos);
            string name = d->d_name;

            bpos += d->d_reclen;

            if (name == "." || name == "..") {
                continue;
            }

            string path = dir + "/" + name;

            struct stat st;
            if (lstat(path.c_str(), &st) == -1) {
                perror("smash error: lstat failed");
                close(fd);
                return -2;
            }

            totalSize += st.st_blocks * 512;

            if (S_ISDIR(st.st_mode)) {
                tmp = compute(path);
                if(tmp < 0) {
                    close(fd);
                    return tmp;
                }
                totalSize += tmp;
            }
        }
    }

    close(fd);
    return totalSize;
}

void DiskUsageCommand::execute() {
    if (numArgs > 2) {
        cerr << "smash error: du: too many arguments" << endl;
        return;
    }

    string dir = (numArgs == 1) ? "." : args[1];

    struct stat st;
    if (lstat(dir.c_str(), &st) == -1 || !S_ISDIR(st.st_mode)) {
        cerr << "smash error: du: directory " << args[1] << " does not exist" << endl;
        return;
    }

    long size_of_top_level_dir_bytes = st.st_blocks * 512;
    long size_of_contents_bytes = compute(dir);

    if (size_of_contents_bytes < 0) {
        return;
    }
    long totalUsage_bytes = size_of_top_level_dir_bytes + size_of_contents_bytes;

    long totalUsage = (totalUsage_bytes + 1023) / 1024;

    cout << "Total disk usage: " << totalUsage << " KB" << endl;
}

void WhoAmICommand::execute() {
    //lets get the real username
    char* buf = new char[4096];
    auto uid = getuid();
    auto passwdFd = open("/etc/passwd", O_RDONLY);
    if(passwdFd == -1) {
        perror("smash error: open failed");
        return;
    }
    for(int i = 0; i < 4096; i++) {
        buf[i] = 0;
    }
    int passwdLen = read(passwdFd, buf, 4096);
    if(passwdLen == -1) {
        perror("smash error: read failed");
        return;
    }
    std::stringstream bufProc;
    std::regex passwdRegex(R"(^([^:]+):[^:]*:(\d+):[^:]*:[^:]*:([^:]+):[^:]*$)");
    std::smatch matches;
    string line;
    int curUid;
    string uname;
    string homeDir;
    bufProc << buf;
    while (std::getline(bufProc, line, '\n')) {
        std::regex_match(line, matches, passwdRegex);
        uname = matches[1];
        curUid = stoi(matches[2]);
        homeDir = matches[3];
        if(curUid == uid) break;
    }
    cout << uname << " " << homeDir << endl;
}

void NetInfo::execute() {
    if (numArgs < 2) {
        cerr << "smash error: netinfo: interface not specified" << endl;
        return;
    }
    int sockFd;
    if ((sockFd = socket(AF_INET, SOCK_DGRAM, 0)) <= -1) {
        perror("smash error: socket failed");
        return;
    }
    ifconf *netConfigs = new ifconf();
    char *buf = new char[8192]();
    ifreq *interfaceReq;
    netConfigs->ifc_len = 8192;
    netConfigs->ifc_ifcu.ifcu_buf = buf;
    if (ioctl(sockFd, SIOCGIFCONF, netConfigs) <= -1) {
        perror("smash error: ioctl failed");
        delete[] buf;
        delete netConfigs;
        return;
    }
    interfaceReq = netConfigs->ifc_ifcu.ifcu_req;
    vector<string> interfaceNames;
    while (interfaceReq != netConfigs->ifc_req + (netConfigs->ifc_len / sizeof(*interfaceReq))) {
        interfaceNames.push_back(interfaceReq->ifr_name);
        interfaceReq++;
    }
    if (std::find(interfaceNames.begin(), interfaceNames.end(), string(args[1])) == interfaceNames.end()) {
        cerr << "smash error: netinfo: interface " << args[1] << " does not exist" << endl;
        delete[] buf;
        delete netConfigs;
        return;
    }
    delete netConfigs;

    interfaceReq = new ifreq();
    strncpy(interfaceReq->ifr_name, args[1], std::strlen(args[1]) + 1);
    auto sockInfo = reinterpret_cast<sockaddr_in *>(&interfaceReq->ifr_addr);
    if (ioctl(sockFd, SIOCGIFADDR, interfaceReq) > -1) {
        cout << "IP Address: " << inet_ntoa(sockInfo->sin_addr) << endl;
    } else {
        perror("smash error: ioctl failed");
        delete[] buf;
        delete interfaceReq;
        return;
    }

    //get subnet mask with ioctl
    if (ioctl(sockFd, SIOCGIFNETMASK, interfaceReq) > -1) {
        cout << "Subnet Mask: " << inet_ntoa(sockInfo->sin_addr) << endl;
    } else {
        perror("smash error: ioctl failed");
        delete interfaceReq;
        return;
    }

    //get default gateway with proc/net/route
    const char *routeFilePath = "/proc/net/route";
    int routeFd = open(routeFilePath, O_RDONLY);
    if (routeFd < 0) {
        delete[] buf;
        perror("smash error: open failed");
        return;
    }
    constexpr size_t bufSize = 8192;
    ssize_t bytesRead = read(routeFd, buf, bufSize - 1);
    if (bytesRead < 0) {
        perror("smash error: read failed");
        delete[] buf;
        close(routeFd);
        return;
    }
    buf[bytesRead] = '\0';
    char *linePtr = buf;
    char *savePtr;
    bool headerSkipped = false;
    char iface[32];
    unsigned int destHex, gatewayHex;
    while (char *line = strtok_r(linePtr, "\n", &savePtr)) {
        linePtr = nullptr;
        if (!headerSkipped) {
            headerSkipped = true;
            continue;
        }
        int fieldsParsed = sscanf(line, "%31s %x %x", iface, &destHex, &gatewayHex);
        if (fieldsParsed != 3)
            continue;
        if (destHex == 0) {  // default route
            in_addr gwAddr;
            gwAddr.s_addr = gatewayHex;
            cout << "Default Gateway: " << inet_ntoa(gwAddr) << endl;
            break;
        }
    }
    close(routeFd);

    //get DNS servers with resolv.conf
    vector<string> dnsNames;
    int dnsFile = open("/etc/resolv.conf", O_RDONLY);
    if(dnsFile < 0) {
        perror("smash error: open failed");
        delete[] buf;
        return;
    }
    memset(buf, 0, 8192);
    int numRead = read(dnsFile, buf, 8192);
    if(numRead < 0) {
        perror("smash error: read failed");
        delete[] buf;
        return;
    }
    std::stringstream fileStream(buf);
    string tokenName;
    string dnsNumber;
    bool comma = false;
    cout << "DNS Servers: ";
    while(fileStream >> tokenName) {
        if(tokenName == "nameserver" && fileStream >> dnsNumber) {
            cout << dnsNumber << ((comma) ? ", " : "");
            comma = true;
        }
    }
    cout << endl;
    delete[] buf;
}