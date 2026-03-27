#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <pty.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

using clock_type = std::chrono::steady_clock;

struct Config {
  enum class ParentMode { orphan, shell };
  enum class StopExpectation { exact, any };

  std::string jai_bin;
  std::string helper_bin;
  std::string config_dir;
  std::string workdir;
  std::string user;
  std::string signal_name = "stop";
  int signal_number = SIGSTOP;
  bool new_pgrp = false;
  bool foreground = false;
  ParentMode parent_mode = ParentMode::orphan;
  StopExpectation stop_expectation = StopExpectation::exact;
};

[[noreturn]] void
die(std::string_view msg)
{
  std::cerr << "FAIL: " << msg << '\n';
  std::exit(1);
}

void
check(bool ok, std::string_view msg)
{
  if (!ok)
    die(msg);
}

int
parse_signal(std::string_view name)
{
  if (name == "stop")
    return SIGSTOP;
  if (name == "tstp")
    return SIGTSTP;
  if (name == "ttin")
    return SIGTTIN;
  if (name == "ttou")
    return SIGTTOU;
  die("unknown signal");
}

Config
parse_args(int argc, char **argv)
{
  Config cfg;
  for (int i = 1; i < argc; ++i) {
    std::string_view arg = argv[i];
    auto need_value = [&](std::string_view name) -> std::string {
      if (i + 1 >= argc)
        die(std::string(name) + " requires a value");
      return argv[++i];
    };

    if (arg == "--jai-bin")
      cfg.jai_bin = need_value("--jai-bin");
    else if (arg == "--helper-bin")
      cfg.helper_bin = need_value("--helper-bin");
    else if (arg == "--config-dir")
      cfg.config_dir = need_value("--config-dir");
    else if (arg == "--workdir")
      cfg.workdir = need_value("--workdir");
    else if (arg == "--user")
      cfg.user = need_value("--user");
    else if (arg == "--new-pgrp")
      cfg.new_pgrp = true;
    else if (arg == "--foreground")
      cfg.foreground = true;
    else if (arg == "--shell-parent")
      cfg.parent_mode = Config::ParentMode::shell;
    else if (arg == "--accept-any-stop-signal")
      cfg.stop_expectation = Config::StopExpectation::any;
    else if (arg.starts_with("--signal="))
      cfg.signal_name = std::string(arg.substr(9));
    else
      die(std::string("unknown argument: ") + std::string(arg));
  }

  cfg.signal_number = parse_signal(cfg.signal_name);
  check(!cfg.jai_bin.empty(), "--jai-bin is required");
  check(!cfg.helper_bin.empty(), "--helper-bin is required");
  check(!cfg.config_dir.empty(), "--config-dir is required");
  check(!cfg.workdir.empty(), "--workdir is required");
  check(!cfg.user.empty(), "--user is required");
  return cfg;
}

std::string
read_until(int fd, std::string_view needle, std::chrono::milliseconds timeout)
{
  auto deadline = clock_type::now() + timeout;
  std::string out;
  char buf[4096];

  for (;;) {
    if (out.find(needle) != std::string::npos)
      return out;

    auto now = clock_type::now();
    if (now >= deadline)
      break;

    auto wait_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now)
            .count();
    pollfd pfd{.fd = fd, .events = POLLIN | POLLHUP, .revents = 0};
    int pr = poll(&pfd, 1, static_cast<int>(wait_ms));
    if (pr < 0) {
      if (errno == EINTR)
        continue;
      die(std::string("poll failed: ") + std::strerror(errno));
    }
    if (pr == 0)
      continue;

    ssize_t n = read(fd, buf, sizeof(buf));
    if (n > 0) {
      out.append(buf, static_cast<std::size_t>(n));
      continue;
    }
    if (n == 0 || errno == EIO)
      break;
    if (errno == EINTR)
      continue;
    die(std::string("read failed: ") + std::strerror(errno));
  }

  die(std::string("timeout waiting for \"") + std::string(needle) +
      "\"; output was:\n" + out);
}

int
wait_for_stop(pid_t pid, std::chrono::milliseconds timeout)
{
  auto deadline = clock_type::now() + timeout;
  for (;;) {
    int status = 0;
    pid_t r = waitpid(pid, &status, WUNTRACED | WCONTINUED | WNOHANG);
    if (r == -1) {
      if (errno == EINTR)
        continue;
      die(std::string("waitpid failed: ") + std::strerror(errno));
    }
    if (r == pid) {
      if (WIFSTOPPED(status))
        return WSTOPSIG(status);
      if (WIFEXITED(status))
        die(std::string("jai exited early with status ") +
            std::to_string(WEXITSTATUS(status)));
      if (WIFSIGNALED(status))
        die(std::string("jai died from signal ") +
            std::to_string(WTERMSIG(status)));
    }

    if (clock_type::now() >= deadline)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  die("jai never stopped after the child self-suspended");
}

void
wait_for_exit(pid_t pid, std::chrono::milliseconds timeout)
{
  auto deadline = clock_type::now() + timeout;
  for (;;) {
    int status = 0;
    pid_t r = waitpid(pid, &status, WUNTRACED | WCONTINUED | WNOHANG);
    if (r == -1) {
      if (errno == EINTR)
        continue;
      die(std::string("waitpid failed: ") + std::strerror(errno));
    }
    if (r == pid) {
      if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) != 0)
          die(std::string("jai exited with status ") +
              std::to_string(WEXITSTATUS(status)));
        return;
      }
      if (WIFSIGNALED(status))
        die(std::string("jai died from signal ") +
            std::to_string(WTERMSIG(status)));
    }

    if (clock_type::now() >= deadline)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  die("timed out waiting for jai to exit");
}

void
kill_group(pid_t pgid)
{
  if (pgid > 0)
    kill(-pgid, SIGKILL);
}

void
tcsetpgrp_ignore_ttou(int tty_fd, pid_t pgid)
{
  struct sigaction old_act{}, ign{};
  ign.sa_handler = SIG_IGN;
  check(sigaction(SIGTTOU, &ign, &old_act) == 0, "sigaction(SIGTTOU) failed");
  check(tcsetpgrp(tty_fd, pgid) == 0, "tcsetpgrp failed");
  check(sigaction(SIGTTOU, &old_act, nullptr) == 0,
        "restoring SIGTTOU failed");
}

[[noreturn]] void
child_exec_orphan(const Config &cfg, int master_fd, int slave_fd)
{
  close(master_fd);
  check(setsid() != -1, "setsid failed");
  check(ioctl(slave_fd, TIOCSCTTY, 0) != -1, "TIOCSCTTY failed");
  check(tcsetpgrp(slave_fd, getpid()) != -1, "tcsetpgrp failed");

  check(dup2(slave_fd, STDIN_FILENO) != -1, "dup2(stdin) failed");
  check(dup2(slave_fd, STDOUT_FILENO) != -1, "dup2(stdout) failed");
  check(dup2(slave_fd, STDERR_FILENO) != -1, "dup2(stderr) failed");
  if (slave_fd > STDERR_FILENO)
    close(slave_fd);

  check(chdir(cfg.workdir.c_str()) == 0, "chdir failed");

  setenv("SUDO_USER", cfg.user.c_str(), 1);
  setenv("USER", cfg.user.c_str(), 1);
  setenv("LOGNAME", cfg.user.c_str(), 1);
  setenv("JAI_CONFIG_DIR", cfg.config_dir.c_str(), 1);
  setenv("TERM", "dumb", 1);

  std::vector<std::string> argv_store;
  auto push = [&](std::string s) { argv_store.push_back(std::move(s)); };

  push(cfg.jai_bin);
  push(cfg.helper_bin);
  push("suspend");
  if (cfg.new_pgrp)
    push("--new-pgrp");
  if (cfg.foreground)
    push("--foreground");
  push(std::string("--signal=") + cfg.signal_name);

  std::vector<char *> args;
  args.reserve(argv_store.size() + 1);
  for (auto &arg : argv_store)
    args.push_back(arg.data());
  args.push_back(nullptr);

  execv(cfg.jai_bin.c_str(), args.data());
  std::cerr << "FAIL: execv(" << cfg.jai_bin << ") failed: "
            << std::strerror(errno) << '\n';
  std::exit(1);
}

[[noreturn]] void
child_exec_shell(const Config &cfg, int master_fd, int slave_fd)
{
  close(master_fd);
  check(setpgid(0, 0) == 0, "setpgid failed");

  check(dup2(slave_fd, STDIN_FILENO) != -1, "dup2(stdin) failed");
  check(dup2(slave_fd, STDOUT_FILENO) != -1, "dup2(stdout) failed");
  check(dup2(slave_fd, STDERR_FILENO) != -1, "dup2(stderr) failed");
  if (slave_fd > STDERR_FILENO)
    close(slave_fd);

  check(chdir(cfg.workdir.c_str()) == 0, "chdir failed");

  setenv("SUDO_USER", cfg.user.c_str(), 1);
  setenv("USER", cfg.user.c_str(), 1);
  setenv("LOGNAME", cfg.user.c_str(), 1);
  setenv("JAI_CONFIG_DIR", cfg.config_dir.c_str(), 1);
  setenv("TERM", "dumb", 1);

  std::vector<std::string> argv_store;
  auto push = [&](std::string s) { argv_store.push_back(std::move(s)); };

  push(cfg.jai_bin);
  push(cfg.helper_bin);
  push("suspend");
  if (cfg.new_pgrp)
    push("--new-pgrp");
  if (cfg.foreground)
    push("--foreground");
  push(std::string("--signal=") + cfg.signal_name);

  std::vector<char *> args;
  args.reserve(argv_store.size() + 1);
  for (auto &arg : argv_store)
    args.push_back(arg.data());
  args.push_back(nullptr);

  execv(cfg.jai_bin.c_str(), args.data());
  std::cerr << "FAIL: execv(" << cfg.jai_bin << ") failed: "
            << std::strerror(errno) << '\n';
  std::exit(1);
}

[[noreturn]] void
shell_parent_exec(const Config &cfg, int master_fd, int slave_fd)
{
  check(setsid() != -1, "setsid failed");
  check(ioctl(slave_fd, TIOCSCTTY, 0) != -1, "TIOCSCTTY failed");
  struct sigaction ign{};
  ign.sa_handler = SIG_IGN;
  check(sigaction(SIGHUP, &ign, nullptr) == 0, "sigaction(SIGHUP) failed");
  pid_t shell_pgid = getpgrp();

  pid_t pid = fork();
  check(pid != -1, "fork failed");
  if (pid == 0)
    child_exec_shell(cfg, master_fd, slave_fd);

  if (setpgid(pid, pid) == -1 && errno != EACCES)
    die(std::string("setpgid(child) failed: ") + std::strerror(errno));
  tcsetpgrp_ignore_ttou(slave_fd, pid);

  struct Cleanup {
    pid_t pid;
    int master_fd;
    int slave_fd;
    pid_t shell_pgid;
    ~Cleanup()
    {
      if (shell_pgid > 0)
        tcsetpgrp_ignore_ttou(slave_fd, shell_pgid);
      kill_group(pid);
      int status = 0;
      while (waitpid(pid, &status, WNOHANG) > 0)
        ;
      if (master_fd >= 0)
        close(master_fd);
      if (slave_fd >= 0)
        close(slave_fd);
    }
  } cleanup{pid, master_fd, slave_fd, shell_pgid};

  std::string output =
      read_until(master_fd, "READY", std::chrono::milliseconds(4000));
  int stop_sig = wait_for_stop(pid, std::chrono::milliseconds(4000));
  if (cfg.stop_expectation == Config::StopExpectation::exact &&
      stop_sig != cfg.signal_number)
    die(std::string("jai stopped with signal ") + std::to_string(stop_sig) +
        ", expected " + std::to_string(cfg.signal_number));

  tcsetpgrp_ignore_ttou(slave_fd, shell_pgid);
  tcsetpgrp_ignore_ttou(slave_fd, pid);
  check(kill(-pid, SIGCONT) == 0, "kill(SIGCONT) failed");
  output += read_until(master_fd, "RESUMED", std::chrono::milliseconds(4000));
  wait_for_exit(pid, std::chrono::milliseconds(4000));

  if (output.find("READY") == std::string::npos ||
      output.find("RESUMED") == std::string::npos)
    die("missing helper output");

  tcsetpgrp_ignore_ttou(slave_fd, shell_pgid);
  cleanup.pid = -1;
  close(master_fd);
  cleanup.master_fd = -1;
  close(slave_fd);
  cleanup.slave_fd = -1;
  std::exit(0);
}

} // namespace

int
main(int argc, char **argv)
{
  Config cfg = parse_args(argc, argv);

  int master_fd = -1;
  int slave_fd = -1;
  check(openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0,
        "openpty failed");

  if (cfg.parent_mode == Config::ParentMode::shell) {
    pid_t shell_pid = fork();
    check(shell_pid != -1, "fork failed");
    if (shell_pid == 0)
      shell_parent_exec(cfg, master_fd, slave_fd);
    close(master_fd);
    close(slave_fd);
    int status = 0;
    check(waitpid(shell_pid, &status, 0) == shell_pid, "waitpid failed");
    if (WIFEXITED(status))
      return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
      die(std::string("shell parent died from signal ") +
          std::to_string(WTERMSIG(status)));
    die("shell parent ended unexpectedly");
  }

  pid_t pid = fork();
  check(pid != -1, "fork failed");
  if (pid == 0)
    child_exec_orphan(cfg, master_fd, slave_fd);
  close(slave_fd);

  struct Cleanup {
    pid_t pid;
    int master_fd;
    ~Cleanup()
    {
      kill_group(pid);
      int status = 0;
      while (waitpid(pid, &status, WNOHANG) > 0)
        ;
      if (master_fd >= 0)
        close(master_fd);
    }
  } cleanup{pid, master_fd};

  std::string output =
      read_until(master_fd, "READY", std::chrono::milliseconds(4000));
  int stop_sig = wait_for_stop(pid, std::chrono::milliseconds(4000));
  if (cfg.stop_expectation == Config::StopExpectation::exact &&
      stop_sig != cfg.signal_number)
    die(std::string("jai stopped with signal ") + std::to_string(stop_sig) +
        ", expected " + std::to_string(cfg.signal_number));

  check(kill(-pid, SIGCONT) == 0, "kill(SIGCONT) failed");
  output += read_until(master_fd, "RESUMED", std::chrono::milliseconds(4000));
  wait_for_exit(pid, std::chrono::milliseconds(4000));

  if (output.find("READY") == std::string::npos ||
      output.find("RESUMED") == std::string::npos)
    die("missing helper output");

  cleanup.pid = -1;
  close(master_fd);
  cleanup.master_fd = -1;
  return 0;
}
