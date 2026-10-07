#!/usr/bin/env python3
"""Analogix DP AUX via /dev/mem (no drm_dp_aux chardev needed)."""
import argparse, mmap, os, struct, sys, time

DP = 0xFE0C0000
SZ = 0x10000

BUFFER_DATA_CTL = 0x790
AUX_CH_CTL_1 = 0x794
AUX_ADDR_7_0 = 0x798
AUX_ADDR_15_8 = 0x79C
AUX_ADDR_19_16 = 0x7A0
AUX_CH_CTL_2 = 0x7A4
BUF_DATA_0 = 0x7C0
AUX_CH_STA = 0x780
AUX_RX_COMM = 0x78C
INT_STA = 0x3DC

BUF_CLR = 1 << 7
AUX_EN = 1 << 0
ADDR_ONLY = 1 << 1
RPLY_RECEIV = 1 << 1
AUX_ERR = 1 << 0

# CTL1
def AUX_LENGTH(n):
    return ((n - 1) & 0xF) << 4

AUX_TX_COMM_DP = 1 << 3
AUX_TX_COMM_READ = 1 << 0
AUX_TX_COMM_WRITE = 0


class DpMmio:
    def __init__(self):
        self.fd = os.open("/dev/mem", os.O_RDWR | os.O_SYNC)
        self.mm = mmap.mmap(self.fd, SZ, mmap.MAP_SHARED,
                            mmap.PROT_READ | mmap.PROT_WRITE, offset=DP)

    def close(self):
        self.mm.close()
        os.close(self.fd)

    def r(self, off):
        return struct.unpack_from("<I", self.mm, off)[0]

    def w(self, off, val):
        struct.pack_into("<I", self.mm, off, val & 0xFFFFFFFF)

    def aux(self, addr, size, write=False, data=b""):
        if size > 16:
            raise ValueError("size>16")
        self.w(BUFFER_DATA_CTL, BUF_CLR)
        ctl1 = AUX_LENGTH(size) | AUX_TX_COMM_DP
        ctl1 |= AUX_TX_COMM_WRITE if write else AUX_TX_COMM_READ
        self.w(AUX_CH_CTL_1, ctl1)
        self.w(AUX_ADDR_7_0, addr & 0xFF)
        self.w(AUX_ADDR_15_8, (addr >> 8) & 0xFF)
        self.w(AUX_ADDR_19_16, (addr >> 16) & 0xF)
        if write:
            for i, b in enumerate(data[:size]):
                self.w(BUF_DATA_0 + 4 * i, b)
        ctl2 = AUX_EN
        if size < 1:
            ctl2 |= ADDR_ONLY
        self.w(AUX_CH_CTL_2, ctl2)
        t0 = time.time()
        while self.r(AUX_CH_CTL_2) & AUX_EN:
            if time.time() - t0 > 0.5:
                raise TimeoutError("AUX_EN timeout")
            time.sleep(0.00005)
        t0 = time.time()
        while not (self.r(INT_STA) & RPLY_RECEIV):
            if time.time() - t0 > 0.05:
                raise TimeoutError("RPLY timeout")
            time.sleep(0.00005)
        self.w(INT_STA, RPLY_RECEIV)
        sta = self.r(INT_STA)
        if sta & AUX_ERR:
            aux_st = self.r(AUX_CH_STA) & 0xF
            self.w(INT_STA, AUX_ERR)
            raise IOError(f"AUX_ERR status={aux_st:#x}")
        out = bytearray()
        if not write:
            for i in range(size):
                out.append(self.r(BUF_DATA_0 + 4 * i) & 0xFF)
        return bytes(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["read", "write", "dump", "psr_off", "assr_off"])
    ap.add_argument("addr", nargs="?", default="0")
    ap.add_argument("val", nargs="?", default="0")
    ap.add_argument("-n", type=int, default=1)
    args = ap.parse_args()
    if os.geteuid() != 0:
        sys.exit("need root")
    dp = DpMmio()
    try:
        if args.cmd == "read":
            addr = int(args.addr, 0)
            data = dp.aux(addr, args.n, write=False)
            print(f"{addr:#x}:", data.hex(" "))
        elif args.cmd == "write":
            addr = int(args.addr, 0)
            val = int(args.val, 0) & 0xFF
            dp.aux(addr, 1, write=True, data=bytes([val]))
            print(f"wrote {addr:#x}={val:#x}")
        elif args.cmd == "dump":
            for a, n, name in [
                (0x000, 16, "DPCD"),
                (0x100, 2, "BW/LC"),
                (0x10A, 1, "EDP_CFG"),
                (0x200, 1, "SINK_COUNT"),
                (0x202, 6, "LANE/SINK"),
                (0x170, 1, "PSR_CFG"),
                (0x600, 1, "SET_POWER"),
            ]:
                try:
                    d = dp.aux(a, n, write=False)
                    print(f"{name:10} {a:#05x}: {d.hex(' ')}")
                except Exception as e:
                    print(f"{name:10} {a:#05x}: ERR {e}")
        elif args.cmd == "psr_off":
            try:
                cur = dp.aux(0x170, 1)[0]
            except Exception as e:
                print("read PSR failed", e)
                cur = None
            print(f"PSR_CFG before={cur}")
            dp.aux(0x170, 1, write=True, data=b"\x00")
            time.sleep(0.05)
            after = dp.aux(0x170, 1)[0]
            print(f"PSR_CFG after={after:#x}")
            st = dp.aux(0x202, 6)
            print(f"LANE/SINK after: {st.hex(' ')}  (byte3=SINK_STATUS, bit0=stream)")
        elif args.cmd == "assr_off":
            # TX
            os.system("busybox devmem 0xfe0c09d8 32 $(( $(busybox devmem 0xfe0c09d8 32) & ~0x80 ))")
            cfg = dp.aux(0x10A, 1)[0]
            print(f"EDP_CFG before={cfg:#x}")
            dp.aux(0x10A, 1, write=True, data=bytes([cfg & ~0x1]))
            print(f"EDP_CFG after={dp.aux(0x10A,1)[0]:#x}")
    finally:
        dp.close()


if __name__ == "__main__":
    main()
