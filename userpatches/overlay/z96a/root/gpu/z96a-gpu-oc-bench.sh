#!/bin/bash
# z96a-gpu-oc-bench.sh — lock Mali GPU via devfreq, verify clk_scmi_gpu + vdd_gpu, glmark2.
#
# On-board (as root, GNOME/Wayland session of USER must be logged in):
#   SECS=5 sudo -E /root/gpu/z96a-gpu-oc-bench.sh
#   SECS=5 FREQS="800000000 1000000000" sudo -E /root/gpu/z96a-gpu-oc-bench.sh
#
# Method: start glmark load first, then lock freq (idle drops to ~200 MHz / 0.85 V).
# Keep SECS short — GPU OC also heats the chassis.
#
# Env:
#   SECS=5
#   FREQS="800000000 1000000000"   # Hz
#   USERNAME=evanee
#   SIZE=1280x720
#   BENCH=terrain                 # glmark2 scene
#   COOL_S=10
set -euo pipefail

GPU=/sys/class/devfreq/fde60000.gpu
SCMI=/sys/kernel/debug/clk/clk_scmi_gpu/clk_rate
VDD=
SECS=${SECS:-5}
FREQS=${FREQS:-"800000000 1000000000"}
USERNAME=${USERNAME:-evanee}
SIZE=${SIZE:-1280x720}
BENCH=${BENCH:-terrain}
COOL_S=${COOL_S:-10}
UID_NUM=$(id -u "$USERNAME")
XDG_RUN=/run/user/$UID_NUM

if [[ $(id -u) -ne 0 ]]; then
  echo "run as root" >&2
  exit 1
fi

mountpoint -q /sys/kernel/debug || mount -t debugfs none /sys/kernel/debug
[[ -r $SCMI ]] || { echo "missing $SCMI" >&2; exit 1; }
[[ -d $GPU ]] || { echo "missing $GPU" >&2; exit 1; }

for d in /sys/class/regulator/regulator.*; do
  [[ -f $d/name ]] || continue
  if [[ $(cat "$d/name") == vdd_gpu ]]; then VDD=$d; break; fi
done

GLMARK=
for c in glmark2-es2-wayland glmark2-wayland glmark2-es2 glmark2; do
  if command -v "$c" >/dev/null; then GLMARK=$c; break; fi
done
[[ -n $GLMARK ]] || { echo "need glmark2*" >&2; exit 1; }

# pick a live wayland display for the user
WL=
for w in "$XDG_RUN"/wayland-*; do
  [[ -S $w ]] || continue
  WL=$(basename "$w")
  break
done
[[ -n $WL ]] || { echo "no wayland socket in $XDG_RUN (log into GNOME first)" >&2; exit 1; }

old_gov=$(cat "$GPU/governor")
old_max=$(cat "$GPU/max_freq")
old_min=$(cat "$GPU/min_freq")
restore() {
  echo "$old_gov" > "$GPU/governor" 2>/dev/null || true
  echo "$old_min" > "$GPU/min_freq" 2>/dev/null || true
  echo "$old_max" > "$GPU/max_freq" 2>/dev/null || true
  pkill -u "$USERNAME" -f "glmark2" 2>/dev/null || true
}
trap restore EXIT

temp_c() {
  local t
  t=$(cat /sys/class/thermal/thermal_zone1/temp 2>/dev/null || cat /sys/class/thermal/thermal_zone0/temp)
  echo $((t / 1000))
}
scmi() { cat "$SCMI"; }
vgpu() { [[ -n $VDD ]] && cat "$VDD/microvolts" || echo na; }

echo "===== board ====="
uname -r
echo "available: $(cat $GPU/available_frequencies)"
echo "gov=$old_gov min=$old_min max=$old_max"
echo "GLMARK=$GLMARK WAYLAND=$WL SECS=$SECS FREQS=$FREQS"
echo "idle: cur=$(cat $GPU/cur_freq) scmi=$(scmi) vdd_gpu_uV=$(vgpu) temp=$(temp_c)C"

run_one() {
  local target=$1 out fps score samples i hw vv cur
  echo
  echo "===== lock ${target} Hz, ${BENCH} ${SECS}s @ ${SIZE} ====="
  if [[ ${COOL_S} -gt 0 ]]; then
    echo "cool ${COOL_S}s..."
    echo simple_ondemand > "$GPU/governor" 2>/dev/null || true
    sleep "$COOL_S"
    echo "temp=$(temp_c)C"
  fi

  out=$(mktemp)
  # start load first (unlocked), then clamp
  sudo -u "$USERNAME" env \
    XDG_RUNTIME_DIR="$XDG_RUN" WAYLAND_DISPLAY="$WL" \
    "$GLMARK" -s "$SIZE" -b "${BENCH}:duration=${SECS}.0" >"$out" 2>&1 &
  local pid=$!
  sleep 1.2
  echo userspace > "$GPU/governor"
  # clamp both ends so simple_ondemand can't fight us
  echo "$target" > "$GPU/max_freq"
  echo "$target" > "$GPU/min_freq"
  if [[ -f $GPU/userspace ]]; then
    echo "$target" > "$GPU/userspace"
  fi

  samples=()
  for ((i=0; i<SECS+2; i++)); do
    if ! kill -0 "$pid" 2>/dev/null; then break; fi
    cur=$(cat "$GPU/cur_freq")
    hw=$(scmi)
    vv=$(vgpu)
    samples+=("$hw")
    echo "t=$((i+1)) cur=$cur scmi=$hw vdd_uV=$vv temp=$(temp_c)C"
    sleep 1
  done
  wait "$pid" || true

  fps=$(grep -Eo 'FPS:[[:space:]]*[0-9.]+' "$out" | tail -1 | awk '{print $2}' || true)
  score=$(grep -E 'glmark2 Score:' "$out" | tail -1 | awk '{print $(NF)}' || true)
  echo "---- glmark tail ----"
  tail -15 "$out"
  echo "---- summary ----"
  hw=$(scmi)
  python3 - "$target" "$hw" "$fps" "$score" "${samples[*]-}" <<'PY'
import sys
target=int(sys.argv[1]); hw=int(sys.argv[2])
fps=sys.argv[3]; score=sys.argv[4]
samples=[int(x) for x in sys.argv[5].split()] if len(sys.argv)>5 and sys.argv[5] else []
hh=hw/1e6 if hw>1e6 else hw/1e3
# samples are Hz
sm=[s/1e6 for s in samples] if samples else []
ok_n=sum(1 for s in samples if abs(s-target)<target*0.05)
print(f"target_mhz={target/1e6:.0f} scmi_end_mhz={hh:.1f} scmi_ok_samples={ok_n}/{len(samples)}")
if sm:
    print(f"scmi_mhz_samples={[round(x,1) for x in sm]}")
print(f"FPS={fps} score={score}")
print(f"scmi_held={ok_n>=max(1,len(samples)//2)}")
PY
  rm -f "$out"
  # release clamp briefly
  echo "$old_min" > "$GPU/min_freq" 2>/dev/null || true
  echo "$old_max" > "$GPU/max_freq" 2>/dev/null || true
  echo simple_ondemand > "$GPU/governor" 2>/dev/null || true
}

for f in $FREQS; do
  run_one "$f"
done
echo
echo "===== done ====="
