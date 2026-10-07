# jpegd — RK3568 JPEG HW decode test

Hardware: `fded0000.jpegd` (MPP `RKJPEGD`) via `/dev/mpp_service`.

FFmpeg on this image has **no** `mjpeg_rkmpp` decoder (only encoder), so the test
drives Rockchip `mpi_dec_test` (`rockchip-mpp-demos`).

```bash
# dependency (also pulled into next image builds)
apt-get install -y rockchip-mpp-demos

cd /root/jpegd
./prepare-media.sh           # sample-1080.jpg + sample-4k.jpg
./run-jpegd-bench.sh         # N=50 default; jpegd IRQ should rise

./jpegd_test -n 10 sample-1080.jpg
./jpegd_test -o /tmp/out.yuv sample-4k.jpg
```

Success: `jpegd IRQ` delta ≈ number of decoded frames.
