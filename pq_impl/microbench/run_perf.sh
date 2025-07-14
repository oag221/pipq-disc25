#!/bin/bash

ctl_dir=/tmp/

ctl_fifo=${ctl_dir}perf_ctl.fifo
test -p ${ctl_fifo} && unlink ${ctl_fifo}
mkfifo ${ctl_fifo}
exec {ctl_fd}<>${ctl_fifo}

ctl_ack_fifo=${ctl_dir}perf_ctl_ack.fifo
test -p ${ctl_ack_fifo} && unlink ${ctl_ack_fifo}
mkfifo ${ctl_ack_fifo}
exec {ctl_fd_ack}<>${ctl_ack_fifo}

perf stat -D -1 -e cpu-cycles -a -I 1000       \
            --control fd:${ctl_fd},${ctl_fd_ack} \
            \-- sleep 30 &
perf_pid=$!

sleep 5  && echo 'enable' >&${ctl_fd} && read -u ${ctl_fd_ack} e1 && echo "enabled(${e1})"
sleep 10 && echo 'disable' >&${ctl_fd} && read -u ${ctl_fd_ack} d1 && echo "disabled(${d1})"

exec {ctl_fd_ack}>&-
unlink ${ctl_ack_fifo}

exec {ctl_fd}>&-
unlink ${ctl_fifo}

wait -n ${perf_pid}
exit $?