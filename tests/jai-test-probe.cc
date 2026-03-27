#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

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
signal_from_name(std::string_view name)
{
  if (name == "stop")
    return SIGSTOP;
  if (name == "tstp")
    return SIGTSTP;
  if (name == "ttin")
    return SIGTTIN;
  if (name == "ttou")
    return SIGTTOU;
  die("unknown signal name");
}

std::string
join_pids(const std::set<int> &pids)
{
  std::string out;
  bool first = true;
  for (int pid : pids) {
    if (!first)
      out += ',';
    first = false;
    out += std::to_string(pid);
  }
  return out;
}

std::set<int>
numeric_proc_entries()
{
  std::set<int> out;
  DIR *dir = opendir("/proc");
  check(dir != nullptr, "opendir(/proc) failed");
  while (dirent *de = readdir(dir)) {
    char *end{};
    long v = std::strtol(de->d_name, &end, 10);
    if (*de->d_name != '\0' && end && *end == '\0')
      out.insert(static_cast<int>(v));
  }
  closedir(dir);
  return out;
}

int
open_tty()
{
  return open("/dev/tty", O_RDWR | O_CLOEXEC);
}

long
foreground_pgrp(int ttyfd)
{
  if (ttyfd < 0)
    return -1;
  auto pg = tcgetpgrp(ttyfd);
  if (pg == -1)
    return -1;
  return pg;
}

void
print_probe()
{
  struct stat tmp{}, vartmp{};
  check(stat("/tmp", &tmp) == 0, "stat(/tmp) failed");
  check(stat("/var/tmp", &vartmp) == 0, "stat(/var/tmp) failed");

  int ttyfd = open_tty();
  const auto pids = numeric_proc_entries();

  std::cout << "pid=" << getpid() << '\n';
  std::cout << "ppid=" << getppid() << '\n';
  std::cout << "pgid=" << getpgid(0) << '\n';
  std::cout << "sid=" << getsid(0) << '\n';
  std::cout << "tty=" << (ttyfd >= 0 ? "yes" : "no") << '\n';
  std::cout << "fg_pgrp=" << foreground_pgrp(ttyfd) << '\n';
  std::cout << "tmp_same_inode="
            << ((tmp.st_dev == vartmp.st_dev && tmp.st_ino == vartmp.st_ino)
                    ? "yes"
                    : "no")
            << '\n';
  std::cout << "proc_pids=" << join_pids(pids) << '\n';

  if (ttyfd >= 0)
    close(ttyfd);
}

void
run_suspend(int argc, char **argv)
{
  bool new_pgrp = false;
  bool foreground = false;
  int stop_signal = SIGSTOP;

  for (int i = 2; i < argc; ++i) {
    std::string_view arg = argv[i];
    if (arg == "--new-pgrp")
      new_pgrp = true;
    else if (arg == "--foreground")
      foreground = true;
    else if (arg.starts_with("--signal="))
      stop_signal = signal_from_name(arg.substr(9));
    else
      die("unknown suspend argument");
  }

  if (new_pgrp)
    check(setpgid(0, 0) == 0, "setpgid failed");

  int ttyfd = open_tty();
  if (foreground && ttyfd >= 0) {
    struct sigaction old_act{}, ign{};
    ign.sa_handler = SIG_IGN;
    check(sigaction(SIGTTOU, &ign, &old_act) == 0, "sigaction(SIGTTOU) failed");
    check(tcsetpgrp(ttyfd, getpgrp()) == 0, "tcsetpgrp failed");
    check(sigaction(SIGTTOU, &old_act, nullptr) == 0,
          "restoring SIGTTOU failed");
  }

  std::cout << "READY"
            << " pid=" << getpid() << " ppid=" << getppid()
            << " pgid=" << getpgid(0) << " sid=" << getsid(0)
            << " fg_pgrp=" << foreground_pgrp(ttyfd)
            << " signal=" << stop_signal << '\n'
            << std::flush;

  check(raise(stop_signal) == 0, "raise failed");

  std::cout << "RESUMED"
            << " pid=" << getpid() << " pgid=" << getpgid(0)
            << " fg_pgrp=" << foreground_pgrp(ttyfd) << '\n'
            << std::flush;

  if (ttyfd >= 0)
    close(ttyfd);
}

} // namespace

int
main(int argc, char **argv)
{
  std::ios::sync_with_stdio(false);

  if (argc < 2)
    die("missing subcommand");

  std::string_view cmd = argv[1];
  if (cmd == "probe")
    print_probe();
  else if (cmd == "suspend")
    run_suspend(argc, argv);
  else
    die("unknown subcommand");

  return 0;
}
