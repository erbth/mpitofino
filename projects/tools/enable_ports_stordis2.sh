#!/bin/bash -e

$SDE/run_bfshell.sh -f <( cat << "EOF"
ucli
pm
port-add 12/0 100G RS
port-add 13/0 100G RS
port-add 14/0 100G RS
port-add 15/0 100G RS
port-add 65/0 10G none
port-add 4/0 100g none
port-add 3/0 100g none
port-loopback 4/0 serdes-near
port-loopback 3/0 serdes-near
port-enb 12/0
port-enb 13/0
port-enb 14/0
port-enb 15/0
port-enb 65/0
port-enb 4/0
port-enb 3/0
show
exit
exit
EOF
)
