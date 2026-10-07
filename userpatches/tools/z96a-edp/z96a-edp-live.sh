#!/bin/bash
# Z96A eDP live bring-up (no kernel rebuild).
# Note: this kernel has CONFIG_DRM_DISPLAY_DP_AUX_CHARDEV=n → no /dev/drm_dp_aux*.
set -uo pipefail

DP=0xfe0c0000
VOP_IF=0xfe040028
VP1_DSP=0xfe040d00
DEVMEM=${DEVMEM:-busybox devmem}

die() { echo "ERR: $*" >&2; exit 1; }
need_root() { [[ $(id -u) -eq 0 ]] || die "run as root"; }

rd() { $DEVMEM "$1" 32; }
wr() { $DEVMEM "$1" 32 "$2"; }

rdu() {
	local v
	v=$(rd "$1")
	v=${v,,}
	v=${v#0x}
	printf '%d' "$((16#$v))"
}

hex() { printf '0x%08x' "$1"; }

aux_dev() {
	local d
	for d in /dev/drm_dp_aux0 /dev/drm_dp_aux1 /dev/drm_dp_aux2; do
		[[ -e "$d" ]] && { echo "$d"; return 0; }
	done
	# some kernels use sysfs-only names
	for d in /sys/class/drm_dp_aux_dev/*/dev; do
		[[ -e "$d" ]] || continue
	done
	return 1
}

have_aux() { aux_dev >/dev/null 2>&1; }

dpcd_read() {
	local addr=$1 len=${2:-1} dev
	dev=$(aux_dev) || { echo "(no aux)"; return 1; }
	dd if="$dev" bs=1 skip=$((addr)) count=$((len)) status=none 2>/dev/null | od -An -tx1
}

dpcd_write_byte() {
	local addr=$1 val=$2 dev
	dev=$(aux_dev) || return 1
	python3 - "$dev" "$addr" "$val" <<'PY'
import sys
dev, addr, val = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
with open(dev, "r+b", buffering=0) as f:
    f.seek(addr)
    f.write(bytes([val & 0xff]))
PY
}

dump_analogix() {
	echo "=== Analogix @ $(hex $DP) ==="
	local a
	for a in \
		0x18 0x20 0x24 0x2c 0x44 \
		0x48 0x4c 0x50 0x54 0x64 0x68 0x6c 0x70 \
		0x600 0x604 0x608 0x60c \
		0x700 0x704 0x708 0x70c 0x710 0x714 \
		0x800 0x9d8
	do
		printf '  [%04x] %s\n' "$a" "$(rd $((DP + a)))"
	done
	echo "=== VOP ==="
	echo "  IF_EN  $(rd $VOP_IF)   (want ~0x4008 = EDP_EN+vp1)"
	echo "  VP1    $(rd $VP1_DSP)  (low4=OUT_MODE; 0=P888)"
}

dump_msa() {
	local tl th al ah tp thp ap ahp
	tl=$(rdu $((DP+0x48))); th=$(rdu $((DP+0x4c)))
	al=$(rdu $((DP+0x50))); ah=$(rdu $((DP+0x54)))
	tp=$(rdu $((DP+0x64))); thp=$(rdu $((DP+0x68)))
	ap=$(rdu $((DP+0x6c))); ahp=$(rdu $((DP+0x70)))
	local vtot=$(( (th & 0xf) << 8 | (tl & 0xff) ))
	local vact=$(( (ah & 0xf) << 8 | (al & 0xff) ))
	local htot=$(( (thp & 0x3f) << 8 | (tp & 0xff) ))
	local hact=$(( (ahp & 0x3f) << 8 | (ap & 0xff) ))
	echo "=== MSA decode ==="
	echo "  vtot=$vtot vact=$vact htot=$htot hact=$hact  (want 1100/1080 2142/1920)"
}

dump_mn() {
	local m0 m1 m2 n0 n1 n2 m n sys4
	m0=$(rdu $((DP+0x700))); m1=$(rdu $((DP+0x704))); m2=$(rdu $((DP+0x708)))
	n0=$(rdu $((DP+0x70c))); n1=$(rdu $((DP+0x710))); n2=$(rdu $((DP+0x714)))
	m=$(( (m2 & 0xff) << 16 | (m1 & 0xff) << 8 | (m0 & 0xff) ))
	n=$(( (n2 & 0xff) << 16 | (n1 & 0xff) << 8 | (n0 & 0xff) ))
	sys4=$(rdu $((DP+0x60c)))
	echo "=== M/N vid ==="
	echo "  M=$m N=$n  SYS_CTL_4=$(hex $sys4)  FIX_M_VID=$(( (sys4>>2)&1 ))"
}

dump_dpcd() {
	echo "=== DPCD ==="
	if ! have_aux; then
		echo "  (no /dev/drm_dp_aux* — kernel CONFIG_DRM_DISPLAY_DP_AUX_CHARDEV=n)"
		return 0
	fi
	echo -n "  0x000-0x00f:"; dpcd_read 0x000 16
	echo -n "  0x100-0x101:"; dpcd_read 0x100 2
	echo -n "  0x10a edp_cfg:"; dpcd_read 0x10a 1
	echo -n "  0x202-0x207 lane/sink:"; dpcd_read 0x202 6
	echo -n "  0x170 psr_cfg:"; dpcd_read 0x170 1
	echo "  (SINK_STATUS @0x205 bit0=1 => sink receiving stream)"
}

cmd_dump() {
	need_root
	dump_analogix
	dump_msa
	dump_mn
	dump_dpcd
	echo "=== backlight ==="
	local f
	for f in /sys/class/backlight/*/brightness; do
		[[ -e "$f" ]] || continue
		echo "  $f = $(cat "$f") / $(cat "${f%/brightness}/max_brightness")"
	done
}

cmd_msa_1080p() {
	need_root
	local hact=1920 vact=1080 hfp=48 hsw=32 hbp=142 vfp=3 vsw=6 vbp=11
	local htot=$((hact+hfp+hsw+hbp)) vtot=$((vact+vfp+vsw+vbp))
	echo "write MSA ${hact}x${vact} htot=$htot vtot=$vtot"
	wr $((DP+0x48)) $((vtot & 0xff))
	wr $((DP+0x4c)) $(((vtot >> 8) & 0xf))
	wr $((DP+0x50)) $((vact & 0xff))
	wr $((DP+0x54)) $(((vact >> 8) & 0xf))
	wr $((DP+0x58)) $((vfp & 0xff))
	wr $((DP+0x5c)) $((vsw & 0xff))
	wr $((DP+0x60)) $((vbp & 0xff))
	wr $((DP+0x64)) $((htot & 0xff))
	wr $((DP+0x68)) $(((htot >> 8) & 0x3f))
	wr $((DP+0x6c)) $((hact & 0xff))
	wr $((DP+0x70)) $(((hact >> 8) & 0x3f))
	wr $((DP+0x74)) $((hfp & 0xff))
	wr $((DP+0x78)) $(((hfp >> 8) & 0xf))
	wr $((DP+0x7c)) $((hsw & 0xff))
	wr $((DP+0x80)) $(((hsw >> 8) & 0xf))
	wr $((DP+0x84)) $((hbp & 0xff))
	wr $((DP+0x88)) $(((hbp >> 8) & 0xf))
	wr $((DP+0x44)) $(( $(rdu $((DP+0x44))) | 0x10 ))
	dump_msa
}

# Fixed M/N for 141.4MHz pixel / 2.7G link, N=0x8000
# M = round(141400000 * 32768 / 270000000) = 17159
cmd_mn_fix() {
	need_root
	local m=${1:-17159} n=${2:-32768}
	local sys4
	echo "FIX M=$m N=$n"
	sys4=$(rdu $((DP+0x60c)))
	sys4=$(( sys4 | 0x4 ))	# FIX_M_VID
	wr $((DP+0x60c)) "$sys4"
	wr $((DP+0x700)) $((m & 0xff))
	wr $((DP+0x704)) $(((m >> 8) & 0xff))
	wr $((DP+0x708)) $(((m >> 16) & 0xff))
	wr $((DP+0x70c)) $((n & 0xff))
	wr $((DP+0x710)) $(((n >> 8) & 0xff))
	wr $((DP+0x714)) $(((n >> 16) & 0xff))
	dump_mn
}

cmd_mn_auto() {
	need_root
	local sys4
	echo "CALCULATED_M (clear FIX_M_VID, N=0x8000)"
	sys4=$(rdu $((DP+0x60c)))
	sys4=$(( sys4 & ~0x4 ))
	wr $((DP+0x60c)) "$sys4"
	wr $((DP+0x70c)) 0x00
	wr $((DP+0x710)) 0x80
	wr $((DP+0x714)) 0x00
	dump_mn
}

cmd_slave() {
	need_root
	echo "force SOC slave 0x101 (readback may stay 0 on rk3568)"
	wr $((DP+0x800)) 0x101
	echo "  SOC=$(rd $((DP+0x800)))"
	wr $((DP+0x20)) 0x80
	wr $((DP+0x608)) 0x77
	echo "  CTL1=$(rd $((DP+0x20))) SYS3=$(rd $((DP+0x608)))"
}

cmd_bist() {
	need_root
	local on=${1:-1}
	if [[ "$on" == "1" || "$on" == "on" ]]; then
		echo "BIST on — watch panel"
		wr $((DP+0x2c)) 0x8
		wr $((DP+0x20)) 0x80
	else
		echo "BIST off"
		wr $((DP+0x2c)) 0x0
	fi
	echo "  CTL4=$(rd $((DP+0x2c)))"
}

cmd_psr_off() {
	need_root
	if ! have_aux; then
		echo "skip psr_off (no aux chardev)"
		return 0
	fi
	echo "PSR cfg before:$(dpcd_read 0x170 1)"
	dpcd_write_byte 0x170 0x00
	echo "PSR cfg after: $(dpcd_read 0x170 1)"
}

cmd_assr_off() {
	need_root
	local pol
	pol=$(( $(rdu $((DP+0x9d8))) & ~0x80 ))
	wr $((DP+0x9d8)) "$pol"
	echo "TX LINK_POLICY=$(rd $((DP+0x9d8))) (bit7 ASSR cleared)"
	if have_aux; then
		local cfg
		cfg=$(dpcd_read 0x10a 1 | awk '{print $1}')
		cfg=${cfg#0x}
		cfg=$((16#${cfg:-0} & ~0x1))
		dpcd_write_byte 0x10a "$cfg"
		echo "RX edp_cfg:$(dpcd_read 0x10a 1)"
	else
		echo "RX ASSR not cleared (no aux chardev)"
	fi
}

cmd_bl() {
	need_root
	local v=${1:-0}
	local f
	for f in /sys/class/backlight/*/brightness; do
		[[ -e "$f" ]] || continue
		echo "$v" >"$f"
		echo "set $f = $v (now $(cat "$f"))"
	done
}

cmd_fb_red() {
	need_root
	systemctl stop gdm gdm3 2>/dev/null || true
	python3 - <<'PY'
fb=open("/dev/fb0","r+b",buffering=0)
fb.seek(0)
fb.write(bytes([0,0,255,0])*1920*1080)
fb.seek(0)
print("fb head", fb.read(16).hex())
PY
}

cmd_sink() {
	need_root
	if ! have_aux; then
		echo "no aux chardev"
		return 0
	fi
	echo -n "lane/sink 0x202: "; dpcd_read 0x202 6
}

# Restore panel after accidental bl=0
cmd_unblank() {
	need_root
	cmd_bl 13
	cmd_slave
	cmd_bist on
	echo "backlight+ BIST — screen should not stay black"
}

cmd_fixup() {
	need_root
	systemctl stop gdm gdm3 2>/dev/null || true
	# do NOT leave bl=0 if later steps fail
	trap 'cmd_bl 13 || true' EXIT
	cmd_psr_off
	cmd_msa_1080p
	cmd_mn_fix
	cmd_slave
	cmd_bist on
	cmd_bl 13
	trap - EXIT
	echo "--- BIST+fixed M/N: watch panel 5s ---"
	sleep 5
	cmd_bist off
	cmd_fb_red
	echo "--- RED: watch panel 5s ---"
	sleep 5
	cmd_dump
}

usage() {
	cat <<EOF
Usage: sudo $0 <cmd>
  dump       registers + MSA + M/N
  unblank    bl=13 + slave + BIST (recover black screen)
  msa        rewrite 1920x1080 REGISTER MSA
  mn_fix    fixed M/N for 141.4M/2.7G
  mn_auto    calculated M mode
  slave      force SOC slave + VIDEO_EN + STRM_VALID
  bist [on|off]
  psr_off    clear DPCD PSR (needs aux chardev)
  assr_off   clear TX ASSR (+ RX if aux)
  bl <n>     backlight brightness
  fb_red     stop gdm, paint fb0 red
  fixup      msa + mn_fix + slave + bist + fb_red
EOF
}

cmd=${1:-}
shift || true
case "$cmd" in
	dump) cmd_dump;;
	unblank) cmd_unblank;;
	sink) cmd_sink;;
	msa) cmd_msa_1080p;;
	mn_fix) cmd_mn_fix "$@";;
	mn_auto) cmd_mn_auto;;
	slave) cmd_slave;;
	bist) cmd_bist "${1:-on}";;
	psr_off) cmd_psr_off;;
	assr_off) cmd_assr_off;;
	bl) cmd_bl "${1:-0}";;
	fb_red) cmd_fb_red;;
	fixup) cmd_fixup;;
	""|-h|--help) usage;;
	*) die "unknown cmd: $cmd";;
esac
