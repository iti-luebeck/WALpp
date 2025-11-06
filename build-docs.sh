#!/usr/bin/env bash

docker run  -v $(pwd):/root/BuddelChip -v $(pwd)/.bash_history:/root/.bash_history  --security-opt seccomp=unconfined --cap-add=SYS_PTRACE registry.iti.uni-luebeck.de/buddelchip/buddelchip/systemc:ubuntu-24.04 bash -c "doxide build && mkdocs build"