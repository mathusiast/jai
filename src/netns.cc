#include "netns.h"
#include "err.h"

#include <cstring>

#include <linux/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>
#include <unistd.h>

// Bring up loopback using netlink RTM_NEWLINK.
// This is the minimal code to set the "lo" interface to IFF_UP.
void
setup_loopback()
{
  int fd = socket(AF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC, NETLINK_ROUTE);
  if (fd < 0)
    syserr("socket(AF_NETLINK)");

  struct {
    struct nlmsghdr nh;
    struct ifinfomsg ifi;
  } req{};

  req.nh.nlmsg_len = sizeof(req);
  req.nh.nlmsg_type = RTM_NEWLINK;
  req.nh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
  req.nh.nlmsg_seq = 1;
  req.ifi.ifi_family = AF_UNSPEC;
  req.ifi.ifi_index = 1;          // lo is always index 1
  req.ifi.ifi_flags = IFF_UP;
  req.ifi.ifi_change = IFF_UP;

  if (send(fd, &req, sizeof(req), 0) < 0) {
    close(fd);
    syserr("netlink send (loopback up)");
  }

  // Read ACK
  char buf[256];
  auto n = recv(fd, buf, sizeof(buf), 0);
  close(fd);

  if (n < 0)
    syserr("netlink recv (loopback up)");
  if (static_cast<size_t>(n) < sizeof(struct nlmsghdr))
    err("netlink: short response bringing up loopback");

  auto *resp = reinterpret_cast<struct nlmsghdr *>(buf);
  if (resp->nlmsg_type == NLMSG_ERROR) {
    auto *errmsg = reinterpret_cast<struct nlmsgerr *>(NLMSG_DATA(resp));
    if (errmsg->error != 0) {
      errno = -errmsg->error;
      syserr("netlink: bringing up loopback");
    }
  }
}
