// Ver: 10-4-2025
#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <string>
#include <list>
#include <map>
#include <regex>
#include <sys/unistd.h>
using std::vector;
using std::string;
using std::map;
using std::regex;
using std::list;

#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

class SmallShell;

class Command {
protected:
    string cmd_line;
    pid_t pid;
    char** args;
    int numArgs;
public:
    Command(const char *cmd_line_str);

    virtual ~Command() {
        if (args) {
            for (int i = 0; i < numArgs; ++i) {
                if (args[i] != nullptr) {
                    free(args[i]);
                }
            }
            delete[] args;
        }
    }

    virtual void execute() = 0;

    //virtual void prepare();
    //virtual void cleanup();
    // TODO: Add your extra methods if needed

    const string& get_cmd_line() const { return cmd_line; }
};

class BuiltInCommand : public Command {
public:
    BuiltInCommand(const char *cmd_line) : Command(cmd_line) {};

    virtual ~BuiltInCommand() {
    }
};

class ExternalCommand : public Command {
public:
    ExternalCommand(const char *cmd_line) : Command(cmd_line) {};

    virtual ~ExternalCommand() = default;

    void execute() override;
};


class RedirectionCommand : public Command {
    string command;
    string path;
    bool isAppend;
    int stdoutCopy;
    int fd;
    bool isOpened;
public:
    explicit RedirectionCommand(const char *cmd_line);

    virtual ~RedirectionCommand() {
    }

    void execute() override;
};

class PipeCommand : public Command {
    // TODO: Add your data members
public:
    PipeCommand(const char *cmd_line) : Command(cmd_line) {};

    virtual ~PipeCommand() {
    }

    void execute() override;
};

class DiskUsageCommand : public Command {
    long compute(const string& dir);
public:
    DiskUsageCommand(const char *cmd_line) : Command(cmd_line) {};

    virtual ~DiskUsageCommand() {
    }

    void execute() override;
};

class WhoAmICommand : public Command {
public:
    WhoAmICommand(const char *cmd_line) : Command(cmd_line) {};

    virtual ~WhoAmICommand() {
    }

    void execute() override;
};

class NetInfo : public Command {
    // TODO: Add your data members **BONUS: 10 Points**
public:
    NetInfo(const char *cmd_line) : Command(cmd_line) {};

    virtual ~NetInfo() {
    }

    void execute() override;
};

class ChangeSmashPrompt : public BuiltInCommand {
public:
    string prompt;

    ChangeSmashPrompt(const char *cmd_line) : BuiltInCommand(cmd_line) {};

    virtual ~ChangeSmashPrompt() {}

    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
    string *lastDirectory;
public:
    ChangeDirCommand(const char *cmd_line, string* lastDirectory) : BuiltInCommand(cmd_line), lastDirectory(lastDirectory) {};

    virtual ~ChangeDirCommand() = default;

    void execute() override;

    string *lastPwd;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {};

    virtual ~GetCurrDirCommand() {
    }

    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {};

    virtual ~ShowPidCommand() {
    }

    void execute() override;

};

class JobsList;

class QuitCommand : public BuiltInCommand {
public:
    // TODO: Add your data members public:
    JobsList* jobs;
    QuitCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs) {};

    virtual ~QuitCommand() {
    }

    void execute() override;
};


class JobsList {
public:
    class JobEntry {
    public:
        string command;
        int id;
        pid_t pid;
        bool isStopped;

        JobEntry(const string& command, int id, pid_t pid, bool isStopped) : command(command), id(id), pid(pid), isStopped(isStopped) {};
    };

    JobsList() = default;

    ~JobsList() = default;

    void addJob(const string& command_str, pid_t pid, bool isStopped = false);
    void addJob(Command *cmd, pid_t pid, bool isStopped = false);

    void printJobsList();

    void printJobsList_Quit();

    void killAllJobs();

    static bool isFinished(JobEntry* job);

    void removeFinishedJobs();

    JobEntry *getJobById(int jobId) const;

    void removeJobById(int jobId);

    JobEntry *getLastJob();

    JobEntry *getLastStoppedJob(int *jobId);

    JobEntry *getJobByPid(pid_t pid) const;

    list<JobEntry*> innerList;

    int maxJobId;
};

class JobsCommand : public BuiltInCommand {
    JobsList* jobs;
public:
    JobsCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs) {};

    virtual ~JobsCommand() = default;

    void execute() override;
};

class KillCommand : public BuiltInCommand {
    // TODO: Add your data members
    JobsList* jobs;
public:
    KillCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs) {};

    virtual ~KillCommand() {
    }

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    // TODO: Add your data members
    JobsList* jobs;
public:
    ForegroundCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs) {};

    virtual ~ForegroundCommand() {
    }

    void execute() override;
};

class AliasCommand : public BuiltInCommand {
public:
    AliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {};

    virtual ~AliasCommand() {

    }

    void execute() override;
};

class UnAliasCommand : public BuiltInCommand {
public:
    UnAliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {};

    virtual ~UnAliasCommand() {
    }

    void execute() override;
};

class UnSetEnvCommand : public BuiltInCommand {
public:
    UnSetEnvCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {};

    virtual ~UnSetEnvCommand() {
    }

    void execute() override;
};

class WatchProcCommand : public BuiltInCommand {
public:
    WatchProcCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {};

    virtual ~WatchProcCommand() {
    }

    void execute() override;
};

class SmallShell {
    SmallShell();
    pid_t pid;
    string prompt;
    string currentDirectory;
    string lastDirectory;
    JobsList jobs;
    pid_t currentProcess;
    string currentCommand;
    map<string, string> alias;
public:

    const pid_t& getCurrentProcess() {
        return currentProcess;
    }

    void setCurrentProcess(pid_t pid) {
        currentProcess = pid;
    }

    const string& getCurrentCommand() {
        return currentCommand;
    }

    void setCurrentCommand(const string& command){
        currentCommand = command;
    }

    JobsList& getJobs() {
        return jobs;
    }

    const string& getPrompt() {
        return prompt;
    }

    const map<string, string>& getAlias() {
        return alias;
    }

    void setAlias(const map<string, string>& alias) {
        this->alias = alias;
    }

    void setPrompt(const string& newPrompt) {
        prompt = newPrompt;
    }

    Command *CreateCommand(const char *cmd_line);

    SmallShell(SmallShell const &) = delete; // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }

    ~SmallShell();

    void executeCommand(const char *cmd_line);

    // TODO: add extra methods as needed
};

#endif //SMASH_COMMAND_H_
