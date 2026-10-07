#!/bin/bash
# z96a-cpu-oc-bench.sh — lock CPU freq, verify via clk_scmi_cpu, run openssl sha256 burn.
#
# On-board (as root):
#   SECS=2 /root/cpu/z96a-cpu-oc-bench.sh          # safe short peak
#   SECS=10 FREQS="1992000 2208000" /root/cpu/z96a-cpu-oc-bench.sh
#
# WARNING: 4-core @ 2208 MHz heats the laptop hard. Aborts if SoC temp >= TEMP_LIMIT
# (default 90C) to avoid thermal shutdown (~95C critical).
#
# Env:
#   SECS=2              burn seconds per lock (default 2)
#   FREQS="1992000 2208000"   kHz targets (default both)
#   CORES=4             parallel openssl workers (default 4)
#   COOL_S=15           powersave cool-down before each lock (default 15)
#   SKIP_COOL=1         skip cool-down
#   TEMP_LIMIT=90       abort when max thermal zone >= this (°C)
set -euo pipefail

CPU=/sys/devices/system/cpu
SCMI=/sys/kernel/debug/clk/clk_scmi_cpu/clk_rate
SECS=${SECS:-2}
FREQS=${FREQS:-"1992000 2208000"}
CORES=${CORES:-4}
COOL_S=${COOL_S:-15}
SKIP_COOL=${SKIP_COOL:-0}
TEMP_LIMIT=${TEMP_LIMIT:-90}

if [[ $(id -u) -ne 0 ]]; then
  echo "run as root" >&2
  exit 1
fi

mountpoint -q /sys/kernel/debug || mount -t debugfs none /sys/kernel/debug
if [[ ! -r $SCMI ]]; then
  echo "missing $SCMI (need debugfs + SCMI clk)" >&2
  exit 1
fi

command -v openssl >/dev/null || { echo "need openssl" >&2; exit 1; }
command -v taskset >/dev/null || { echo "need taskset" >&2; exit 1; }
command -v python3 >/dev/null || { echo "need python3" >&2; exit 1; }

declare -a oldgov
declare -a BURN_PIDS=()
ncpu=$(nproc)
(( CORES > ncpu )) && CORES=$ncpu
for ((c=0; c<ncpu; c++)); do
  oldgov[$c]=$(cat "$CPU/cpu$c/cpufreq/scaling_governor")
done

kill_burn() {
  local p
  for p in "${BURN_PIDS[@]:-}"; do
    kill "$p" 2>/dev/null || true
  done
  wait 2>/dev/null || true
  BURN_PIDS=()
}

restore() {
  local max=2208000 c
  kill_burn
  if [[ -f /etc/default/cpufrequtils ]]; then
    max=$(awk -F= '/MAX_SPEED/{gsub(/"/,"");print $2}' /etc/default/cpufrequtils)
  fi
  [[ -n ${max:-} ]] && echo "$max" > "$CPU/cpu0/cpufreq/scaling_max_freq" || true
  for ((c=0; c<ncpu; c++)); do
    echo "${oldgov[$c]}" > "$CPU/cpu$c/cpufreq/scaling_governor" 2>/dev/null || \
      echo ondemand > "$CPU/cpu$c/cpufreq/scaling_governor" 2>/dev/null || true
  done
}
trap restore EXIT

# Max across all thermal zones (millidegree → °C).
temp_c() {
  local t max=0 f
  for f in /sys/class/thermal/thermal_zone*/temp; do
    [[ -r $f ]] || continue
    t=$(cat "$f" 2>/dev/null || echo 0)
    (( t > max )) && max=$t
  done
  echo $((max / 1000))
}

abort_hot() {
  local t=$1 where=${2:-bench}
  echo
  echo "!!! THERMAL ABORT: SoC ${t}C >= TEMP_LIMIT=${TEMP_LIMIT}C (${where})" >&2
  echo "!!! stopping burn, restoring governors — cool down before retry" >&2
  kill_burn
  exit 2
}

# Must NOT run via $(...) — abort uses exit on this shell.
guard_temp() {
  local where=${1:-bench} t
  t=$(temp_c)
  if (( t >= TEMP_LIMIT )); then
    abort_hot "$t" "$where"
  fi
  REPLY=$t
}

echo "===== board ====="
uname -r
echo "available: $(cat $CPU/cpu0/cpufreq/scaling_available_frequencies)"
echo "max: $(cat $CPU/cpu0/cpufreq/scaling_max_freq)"
echo "SECS=$SECS CORES=$CORES FREQS=$FREQS COOL_S=$COOL_S TEMP_LIMIT=${TEMP_LIMIT}C temp=$(temp_c)C"
echo "thermal: abort >= ${TEMP_LIMIT}C (critical trip ~95C)"

bench_one() {
  local target=$1
  local tmp sc hw samples i c line rates_file t
  echo
  echo "===== lock ${target} Hz, burn ${SECS}s x${CORES} ====="

  if [[ $SKIP_COOL != 1 ]]; then
    echo "cooling ${COOL_S}s (powersave)..."
    for ((c=0; c<ncpu; c++)); do
      echo powersave > "$CPU/cpu$c/cpufreq/scaling_governor" 2>/dev/null || true
    done
    for ((i=0; i<COOL_S; i++)); do
      guard_temp "cool"
      t=$REPLY
      (( i % 5 == 4 || i == COOL_S - 1 )) && echo "cool t=$((i+1))/${COOL_S} temp=${t}C"
      sleep 1
    done
  else
    guard_temp "pre-burn"
  fi

  echo "$target" > "$CPU/cpu0/cpufreq/scaling_max_freq"
  for ((c=0; c<ncpu; c++)); do
    echo userspace > "$CPU/cpu$c/cpufreq/scaling_governor"
    echo "$target" > "$CPU/cpu$c/cpufreq/scaling_setspeed"
  done
  sleep 0.4
  guard_temp "lock"

  tmp=$(mktemp -d)
  BURN_PIDS=()
  for ((c=0; c<CORES; c++)); do
    (taskset -c "$c" openssl speed -seconds "$SECS" sha256 >"$tmp/c$c.out" 2>&1) &
    BURN_PIDS+=($!)
  done

  samples=()
  for ((i=0; i<SECS; i++)); do
    local alive=0 p
    for p in "${BURN_PIDS[@]}"; do
      kill -0 "$p" 2>/dev/null && alive=1 && break
    done
    (( alive == 0 )) && break

    sc=$(cat "$CPU/cpu0/cpufreq/scaling_cur_freq")
    hw=$(cat "$SCMI")
    samples+=("$hw")
    guard_temp "burn@${target}"
    t=$REPLY
    echo "t=$((i+1))/${SECS} scaling=$sc scmi=$hw temp=${t}C"
    sleep 1
  done
  wait || true
  BURN_PIDS=()

  sc=$(cat "$CPU/cpu0/cpufreq/scaling_cur_freq")
  hw=$(cat "$SCMI")
  echo "scaling_end=$sc scmi_end=$hw samples=${samples[*]} temp=$(temp_c)C"

  rates_file="$tmp/rates.txt"
  : >"$rates_file"
  for ((c=0; c<CORES; c++)); do
    line=$(grep -E '^sha256' "$tmp/c$c.out" | tail -1 || true)
    echo "cpu$c: $line"
    echo "$line" | awk '{print $NF}' | tr -d 'kK' >>"$rates_file"
  done
  python3 - "$target" "$hw" "$rates_file" <<'PY'
import sys
target = int(sys.argv[1])
hw = int(sys.argv[2])
rates = [float(x) for x in open(sys.argv[3]) if x.strip()]
hh = hw / 1000 if hw > 1e8 else float(hw)
print("per_core_kB_s", [round(x, 1) for x in rates])
print("sum_kB_s", round(sum(rates), 1))
print("avg_kB_s", round(sum(rates) / len(rates), 1) if rates else 0)
print(
    "target_mhz", target / 1000,
    "scmi_mhz", hh / 1000,
    "scmi_ok", abs(hh - target) < target * 0.02,
)
PY
  rm -rf "$tmp"
}

for f in $FREQS; do
  bench_one "$f"
done
echo
echo "===== done ====="
