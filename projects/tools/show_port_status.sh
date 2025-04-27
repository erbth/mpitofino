#!/bin/bash -e

$SDE/run_bfshell.sh -f <( cat << "EOF"
ucli
pm
show
exit
exit
EOF
)
