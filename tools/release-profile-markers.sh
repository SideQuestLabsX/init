#!/bin/sh
# shellcheck shell=sh disable=SC2034

profile="${PROFILE:?PROFILE is required}"
release="${RELEASE:?RELEASE is required}"
arch="${MARKER_ARCH:-$ARCH}"
shutdown_signal="${MARKER_SHUTDOWN_SIGNAL:-10}"

expect "init $arch starting, pid 1, release $release, profile $profile"
expect "FIXTURE selftest done"
expect "shutdown requested by signal $shutdown_signal"
expect "syncing"
reject "PANIC"
reject "Kernel panic"
reject "FIXTURE console completion incomplete"
reject "FIXTURE interleaved log invalid"
reject "FIXTURE logfile incomplete"
reject "FIXTURE log writer fixture incomplete"
reject "FIXTURE profile completion incomplete"
reject "FIXTURE watchdog completion incomplete"
