# CPU OC bench

Locks CPU OPPs (default 1992 + 2208 MHz), verifies via `clk_scmi_cpu`, burns with parallel `openssl sha256`.

Aborts with exit 2 if any thermal zone reaches **TEMP_LIMIT** (default **90°C**), kills burn workers, restores governors.

```bash
SECS=2 /root/cpu/z96a-cpu-oc-bench.sh
SECS=10 FREQS="1992000 2208000" /root/cpu/z96a-cpu-oc-bench.sh
TEMP_LIMIT=85 SECS=5 /root/cpu/z96a-cpu-oc-bench.sh   # stricter
```

Needs root. Cool between long runs.
