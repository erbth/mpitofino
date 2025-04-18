/** Common utility functions for client-side (i.e. compute-node-side)
related functionality.
*/
#ifndef __COMMON_CLIENT_UTILS_H
#define __COMMON_CLIENT_UTILS_H

#include <filesystem>

/* Determine the Unix domain socket for communicating with the node
daemon (nd) */
std::filesystem::path determine_nd_socket();

#ifndef /* __COMMON_CLIENT_UTILS_H */
