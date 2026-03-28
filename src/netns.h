// -*-C++-*-

#pragma once

// Bring up the loopback interface in a new (empty) network namespace.
// Must be called with CAP_NET_ADMIN (i.e., before dropping privileges).
void setup_loopback();
