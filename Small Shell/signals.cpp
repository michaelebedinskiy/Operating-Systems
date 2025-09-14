#include <iostream>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlCHandler(int sig_num) {
    cout << "smash: got ctrl-C" << endl;
    SmallShell& smash = SmallShell::getInstance();
    pid_t fg_pid = smash.getCurrentProcess();

    if (fg_pid != -1 && fg_pid != getpid()) {
        if (kill(fg_pid, SIGKILL) == 0) {
            cout << "smash: process " << fg_pid << " was killed" << endl;
        } else {
            if (errno != ESRCH) {
                perror("smash error: kill failed in ctrlCHandler");
            }
        }
    }
}


void sigchldHandler(int sig_num) {
    int old_errno = errno;
    int status;
    pid_t pid;
    SmallShell &smash = SmallShell::getInstance();
    pid_t current_fg_pid = smash.getCurrentProcess();

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {

        if (pid == current_fg_pid) {
            continue;
        }
        JobsList::JobEntry* job = smash.getJobs().getJobByPid(pid);

        if (job) {
            if (WIFSTOPPED(status)) {
                job->isStopped = true;
            } else if (WIFCONTINUED(status)) {
                job->isStopped = false;
            }
        }
    }
    errno = old_errno;
}
