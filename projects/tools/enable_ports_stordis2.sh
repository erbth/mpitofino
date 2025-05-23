#!/bin/bash -e

# Pipes 1,2: ports 17...32 and 49...64 (but intermixed)

$SDE/run_bfshell.sh -f <( cat << "EOF"
ucli
pm
port-add 12/0 100G RS
port-add 13/0 100G RS
port-add 14/0 100G RS
port-add 15/0 100G RS
port-add 65/0 10G none
port-enb 12/0
port-enb 13/0
port-enb 14/0
port-enb 15/0
port-enb 65/0
port-add 17/0 100g none
port-loopback 17/0 mac-near
port-enb 17/0
port-add 18/0 100g none
port-loopback 18/0 mac-near
port-enb 18/0
port-add 19/0 100g none
port-loopback 19/0 mac-near
port-enb 19/0
port-add 20/0 100g none
port-loopback 20/0 mac-near
port-enb 20/0
port-add 21/0 100g none
port-loopback 21/0 mac-near
port-enb 21/0
port-add 22/0 100g none
port-loopback 22/0 mac-near
port-enb 22/0
port-add 23/0 100g none
port-loopback 23/0 mac-near
port-enb 23/0
port-add 24/0 100g none
port-loopback 24/0 mac-near
port-enb 24/0
port-add 25/0 100g none
port-loopback 25/0 mac-near
port-enb 25/0
port-add 26/0 100g none
port-loopback 26/0 mac-near
port-enb 26/0
port-add 27/0 100g none
port-loopback 27/0 mac-near
port-enb 27/0
port-add 28/0 100g none
port-loopback 28/0 mac-near
port-enb 28/0
port-add 29/0 100g none
port-loopback 29/0 mac-near
port-enb 29/0
port-add 30/0 100g none
port-loopback 30/0 mac-near
port-enb 30/0
port-add 31/0 100g none
port-loopback 31/0 mac-near
port-enb 31/0
port-add 32/0 100g none
port-loopback 32/0 mac-near
port-enb 32/0
port-add 49/0 100g none
port-loopback 49/0 mac-near
port-enb 49/0
port-add 50/0 100g none
port-loopback 50/0 mac-near
port-enb 50/0
port-add 51/0 100g none
port-loopback 51/0 mac-near
port-enb 51/0
port-add 52/0 100g none
port-loopback 52/0 mac-near
port-enb 52/0
port-add 53/0 100g none
port-loopback 53/0 mac-near
port-enb 53/0
port-add 54/0 100g none
port-loopback 54/0 mac-near
port-enb 54/0
port-add 55/0 100g none
port-loopback 55/0 mac-near
port-enb 55/0
port-add 56/0 100g none
port-loopback 56/0 mac-near
port-enb 56/0
port-add 57/0 100g none
port-loopback 57/0 mac-near
port-enb 57/0
port-add 58/0 100g none
port-loopback 58/0 mac-near
port-enb 58/0
port-add 59/0 100g none
port-loopback 59/0 mac-near
port-enb 59/0
port-add 60/0 100g none
port-loopback 60/0 mac-near
port-enb 60/0
port-add 61/0 100g none
port-loopback 61/0 mac-near
port-enb 61/0
port-add 62/0 100g none
port-loopback 62/0 mac-near
port-enb 62/0
port-add 63/0 100g none
port-loopback 63/0 mac-near
port-enb 63/0
show
exit
exit
EOF
)
