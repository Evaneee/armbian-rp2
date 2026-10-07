# GPU OC bench

Locks Mali via devfreq, verifies `clk_scmi_gpu` + `vdd_gpu`, runs glmark2.

```bash
SECS=5 /root/gpu/z96a-gpu-oc-bench.sh
SECS=5 FREQS="800000000 1000000000" /root/gpu/z96a-gpu-oc-bench.sh
```

Needs root + desktop session for glmark2 display (or offscreen if available).
