# ESP32-S3-EYE openvela reference

## Official baseline

Use:

```text
vendor/espressif/boards/esp32s3/esp32s3-eye/configs/openvela
```

Do not base the application on the contest `board/contest_board` scaffold.
The official board overlay is required on the `dev-ai-contest-2026` branch.

## Device nodes

| Capability | Node |
| --- | --- |
| OV2640 QVGA RGB565 camera | `/dev/video0` |
| ST7789 LCD | `/dev/lcd0` |
| PDM microphone | `/dev/audio/pcm_in0` |
| BOOT button | `/dev/buttons` |
| Power LED | `/dev/userleds` |
| Accelerometer | `/dev/accel0` |
| microSD | `/dev/mmcsd1` |
| Wi-Fi | `wlan0` |

Use NuttX V4L2 `VIDIOC_S_FMT`, `VIDIOC_REQBUFS`, `VIDIOC_QBUF`,
`VIDIOC_STREAMON`, and `VIDIOC_DQBUF` with 32-byte-aligned USERPTR buffers.
QVGA RGB565 needs 153,600 bytes per frame.

## Build and flash

From the openvela root:

```bash
./build.sh vendor/espressif/boards/esp32s3/esp32s3-eye/configs/openvela -j8
```

Flash the simple-boot image at offset zero:

```bash
esptool --chip esp32s3 --port /dev/ttyACM0 --baud 460800 \
  write-flash 0x0 nuttx/nuttx.bin
```

If connection fails, hold BOOT, press and release RESET, then release BOOT.

## Important accuracy boundary

The official board overlay supplies camera/audio/display drivers; it does not
by itself supply ESP-WHO model APIs to a NuttX application. A skin-color or
motion heuristic is a bring-up fallback, not equivalent to face detection.
Report its limitations and replace it with a measured model adapter before
claiming production face count, pose angle, or gesture accuracy.
