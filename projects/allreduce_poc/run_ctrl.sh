#!/bin/bash -e

# To be copied to ./build and run from there

sudo -E LD_LIBRARY_PATH="$SDE_INSTALL/lib" ctrl/allreduce_poc-ctrl
