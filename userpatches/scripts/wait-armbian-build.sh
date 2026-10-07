#!/usr/bin/env bash
# Stream Armbian compile console output and wait for completion.
#
# Watch an existing log (prints every new line to the terminal):
#   wait-armbian-build.sh /tmp/build.log
#   wait-armbian-build.sh /tmp/build.logpath
#
# Start compile, tee console to you + log, then wait:
#   wait-armbian-build.sh --run /tmp/z96a-kernel.log -- \
#     ./compile.sh kernel BOARD=z96a-rk3568-laptop BRANCH=current \
#     KERNEL_CONFIGURE=no CPUTHREADS=20 PREFER_DOCKER=yes
#
# Env: TIMEOUT_SEC (default 3600)
#
# Exit: 0 success, 1 failure, 2 timeout/usage
set -euo pipefail

TIMEOUT_SEC="${TIMEOUT_SEC:-3600}"

# Full image builds also print "Kernel build finished" mid-run — do not treat that alone
# as done when watching an existing log (false ALREADY_DONE). Prefer image/docker markers;
# "Kernel build finished" only counts when the log has no later "build" artifact work, or
# when used via --run and the compile process has exited (handled by pipe exit).
SUCCESS_RE='Docker run finished after .*successful|Done building image'
SUCCESS_RE_KERNEL='Kernel build finished'
FAIL_RE='Error 1 occurred|Docker run failed|PROBLEM: don.t run|failed with exit code|Build interrupted'

strip_ansi() { sed 's/\x1b\[[0-9;]*m//g' | tr -d '\r'; }

is_success() {
	# Image/docker markers always count.
	grep -aEq "$SUCCESS_RE" "$1" && return 0
	# Kernel-only marker: only if no in-flight compile/docker for this host.
	if grep -aEq "$SUCCESS_RE_KERNEL" "$1"; then
		if pgrep -f '[c]ompile\.sh' >/dev/null 2>&1; then
			return 1
		fi
		return 0
	fi
	return 1
}
is_fail() { grep -aEq "$FAIL_RE" "$1"; }

usage() {
	sed -n '2,20p' "$0" | sed 's/^# \?//'
	exit 2
}

resolve_log() {
	local arg="$1"
	[[ -n "$arg" ]] || return 1
	if [[ -f "$arg" ]]; then
		local sz
		sz=$(stat -c%s "$arg" 2>/dev/null || echo 0)
		if (( sz > 0 && sz < 512 )); then
			local maybe
			maybe=$(tr -d ' \n' <"$arg")
			if [[ -n "$maybe" && ( -f "$maybe" || ! -e "$maybe" ) ]]; then
				# path file may point at not-yet-created log when --run
				printf '%s\n' "$maybe"
				return 0
			fi
		fi
	fi
	printf '%s\n' "$arg"
}

stream_and_wait() {
	local logfile="$1"
	local show_existing="${2:-0}"

	if [[ ! -f "$logfile" ]]; then
		# wait briefly for tee to create it
		local i
		for i in $(seq 1 50); do
			[[ -f "$logfile" ]] && break
			sleep 0.1
		done
	fi
	if [[ ! -f "$logfile" ]]; then
		echo "log not found: $logfile" >&2
		return 2
	fi

	echo "======== Armbian build console: $logfile ========"

	if is_fail "$logfile"; then
		echo "ALREADY_FAILED"
		grep -aE "$FAIL_RE" "$logfile" | strip_ansi | tail -8
		return 1
	fi
	if is_success "$logfile"; then
		echo "ALREADY_DONE"
		# still show a useful tail so the user sees context
		tail -n 40 "$logfile" | strip_ansi
		grep -aE "$SUCCESS_RE" "$logfile" | strip_ansi | tail -5
		return 0
	fi

	local deadline=$((SECONDS + TIMEOUT_SEC))
	local last_size=0
	if [[ "$show_existing" == "1" ]]; then
		# dump what we have so far, then follow
		cat "$logfile" | strip_ansi
		last_size=$(stat -c%s "$logfile" 2>/dev/null || echo 0)
	else
		# show recent context, then only new bytes
		tail -n 30 "$logfile" | strip_ansi || true
		last_size=$(stat -c%s "$logfile" 2>/dev/null || echo 0)
	fi

	while (( SECONDS < deadline )); do
		if is_fail "$logfile"; then
			# flush remaining bytes first
			local cur
			cur=$(stat -c%s "$logfile" 2>/dev/null || echo 0)
			if (( cur > last_size )); then
				tail -c "$((cur - last_size))" "$logfile" | strip_ansi
			fi
			echo "======== FAILED ========"
			grep -aE "$FAIL_RE" "$logfile" | strip_ansi | tail -8
			return 1
		fi
		if is_success "$logfile"; then
			cur=$(stat -c%s "$logfile" 2>/dev/null || echo 0)
			if (( cur > last_size )); then
				tail -c "$((cur - last_size))" "$logfile" | strip_ansi
			fi
			echo "======== DONE ========"
			grep -aE "$SUCCESS_RE" "$logfile" | strip_ansi | tail -5
			return 0
		fi

		cur=$(stat -c%s "$logfile" 2>/dev/null || echo 0)
		if (( cur > last_size )); then
			# print ALL new bytes (full console), not a truncated tail
			tail -c "$((cur - last_size))" "$logfile" | strip_ansi
			last_size=$cur
		elif (( cur < last_size )); then
			# log rotated/truncated
			last_size=0
		fi
		sleep 0.5
	done

	echo "======== TIMEOUT after ${TIMEOUT_SEC}s ========" >&2
	tail -n 30 "$logfile" | strip_ansi >&2
	return 2
}

run_compile() {
	local logfile="$1"
	shift
	if [[ $# -lt 1 ]]; then
		echo "--run needs a command after --" >&2
		usage
	fi

	mkdir -p "$(dirname "$logfile")"
	: >"$logfile"
	# also write a .logpath next to common /tmp names
	local logpath="${logfile}.logpath"
	# if caller used /tmp/foo.log, also drop /tmp/foo.logpath style when basename matches
	printf '%s\n' "$logfile" >"${logfile%.log}.logpath" 2>/dev/null || true
	printf '%s\n' "$logfile" >"$logpath" 2>/dev/null || true

	echo "======== starting (tee -> $logfile) ========"
	echo "+ $*"

	# Stream to terminal AND log. stdbuf keeps lines timely under pipes.
	set +e
	stdbuf -oL -eL "$@" 2>&1 | stdbuf -oL tee -a "$logfile"
	local pipe_rc=${PIPESTATUS[0]}
	set -e

	echo "======== command exit=$pipe_rc ========"
	if is_fail "$logfile"; then
		return 1
	fi
	if is_success "$logfile"; then
		return 0
	fi
	# compile.sh sometimes returns 0 after success without our markers matching
	# (unlikely); honor exit code as fallback
	if (( pipe_rc == 0 )); then
		echo "(no success marker; command exit 0 — treat as OK)"
		return 0
	fi
	return 1
}

# ---- main ----
if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
	usage
fi

if [[ "${1:-}" == "--run" ]]; then
	shift
	logfile=""
	if [[ "${1:-}" == "--" ]]; then
		echo "need log path before --" >&2
		usage
	fi
	logfile="$(resolve_log "${1:-}")"
	shift
	[[ "${1:-}" == "--" ]] || { echo "expected -- before command" >&2; usage; }
	shift
	run_compile "$logfile" "$@"
	exit $?
fi

LOGFILE="$(resolve_log "${1:-${LOG:-}}")" || usage
# When only watching, show backlog then stream new lines
stream_and_wait "$LOGFILE" 0
exit $?
