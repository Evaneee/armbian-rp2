# Z96A on-image stress / HW tests

Categorized under `/root`:

| Dir | What |
|-----|------|
| `cpu/` | CPU OC peak burn (`openssl` @ locked OPP); aborts ≥90°C |
| `gpu/` | Mali GPU OC / glmark2 via devfreq |
| `rga/` | Mainline RGA (`/dev/video0`) + 4K MPP visual |
| `jpegd/` | JPEG HW decode (`fded0000.jpegd` via MPP) |

```bash
# CPU peak (short; heats hard at 2208 MHz; auto-abort ≥90C)
SECS=2 /root/cpu/z96a-cpu-oc-bench.sh

# GPU peak
SECS=5 /root/gpu/z96a-gpu-oc-bench.sh

# RGA: generate sample media once, then bench
/root/rga/prepare-media.sh
/root/rga/run-rga-bench.sh

# JPEG HW decode (jpegd)
/root/jpegd/run-jpegd-bench.sh
```
