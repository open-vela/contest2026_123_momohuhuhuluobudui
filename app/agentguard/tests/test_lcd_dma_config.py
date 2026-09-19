#!/usr/bin/env python3

import os
import shutil
import subprocess
import tempfile
from pathlib import Path


TEST_DIR = Path(__file__).resolve().parent
REPO_ROOT = TEST_DIR.parents[2]
CONFIG_SCRIPT = REPO_ROOT / "tools" / "apply_agentguard_config.sh"
CAMERA_PATCH = (REPO_ROOT / "tools" / "patches" /
                "nuttx-esp32s3-cam-vsync.patch")

CAMERA_SOURCE = "\n" * 758 + r'''/* preserve-agentguard-sentinel */

  priv->capturing = true;

  esp32s3_gpio_matrix_in(CONFIG_ESP32S3_CAM_VSYNC_PIN,
                         CAM_V_SYNC_IDX, false);
  up_udelay(10);
  esp32s3_gpio_matrix_in(CONFIG_ESP32S3_CAM_VSYNC_PIN,
                         CAM_V_SYNC_IDX, true);

  return OK;
}

'''

FAKE_KCONFIG_TWEAK = r'''#!/usr/bin/env python3
import sys
from pathlib import Path


def main() -> None:
    arguments = sys.argv[1:]
    config_path = Path(arguments[arguments.index("--file") + 1])
    operation = next(
        name for name in ("--enable", "--disable", "--set-val", "--set-str")
        if name in arguments
    )
    operation_index = arguments.index(operation)
    symbol = arguments[operation_index + 1]
    value = arguments[operation_index + 2] if operation.startswith("--set-") else ""
    config_symbol = f"CONFIG_{symbol}"
    lines = config_path.read_text(encoding="utf-8").splitlines()
    lines = [
        line for line in lines
        if not line.startswith(f"{config_symbol}=")
        and line != f"# {config_symbol} is not set"
    ]

    if operation == "--enable":
        lines.append(f"{config_symbol}=y")
    elif operation == "--disable":
        lines.append(f"# {config_symbol} is not set")
    elif operation == "--set-str":
        lines.append(f'{config_symbol}="{value}"')
    else:
        lines.append(f"{config_symbol}={value}")

    config_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
'''


def read_values(config_path: Path) -> dict[str, str]:
    values = {}
    for line in config_path.read_text(encoding="utf-8").splitlines():
        if line.startswith("CONFIG_") and "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
    return values


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="agentguard-lcd-dma-") as temp:
        openvela_root = Path(temp) / "openvela"
        contest_root = openvela_root / "contest"
        tools_dir = contest_root / "tools"
        patches_dir = tools_dir / "patches"
        nuttx_dir = openvela_root / "nuttx"
        tweak = (openvela_root / "prebuilts" / "build-tools" /
                 "linux-x86_64" / "bin" / "kconfig-tweak")
        copied_script = tools_dir / CONFIG_SCRIPT.name
        copied_patch = patches_dir / CAMERA_PATCH.name
        camera_source = (nuttx_dir / "arch" / "xtensa" / "src" /
                         "esp32s3" / "esp32s3_cam.c")
        config_path = nuttx_dir / ".config"

        patches_dir.mkdir(parents=True)
        nuttx_dir.mkdir(parents=True)
        camera_source.parent.mkdir(parents=True)
        tweak.parent.mkdir(parents=True)
        shutil.copy2(CONFIG_SCRIPT, copied_script)
        shutil.copy2(CAMERA_PATCH, copied_patch)
        copied_script.chmod(0o755)
        camera_source.write_text(CAMERA_SOURCE, encoding="utf-8")
        tweak.write_text(FAKE_KCONFIG_TWEAK, encoding="utf-8")
        tweak.chmod(0o755)
        config_path.write_text("# isolated AgentGuard test config\n",
                               encoding="utf-8")
        (nuttx_dir / "Makefile").write_text(
            "include/nuttx/config.h:\n"
            "\tmkdir -p include/nuttx\n"
            "\ttouch include/nuttx/config.h\n",
            encoding="utf-8",
        )

        environment = os.environ.copy()
        environment.pop("AGENTGUARD_SERVER_IPV4", None)
        environment.pop("AGENTGUARD_TOKEN_FILE", None)
        first_run = subprocess.run(
            [str(copied_script)], check=False, env=environment,
            capture_output=True, text=True,
        )
        second_run = subprocess.run(
            [str(copied_script)], check=False, env=environment,
            capture_output=True, text=True,
        )

        assert first_run.returncode == 0, first_run.stdout + first_run.stderr
        assert second_run.returncode == 0, second_run.stdout + second_run.stderr
        assert "Applied AgentGuard ESP32-S3 CAM VSYNC fix" in first_run.stdout
        assert "ESP32-S3 CAM VSYNC fix already applied" in second_run.stdout
        assert "esp32s3_gpio_matrix_in" not in camera_source.read_text(
            encoding="utf-8"
        )
        assert "preserve-agentguard-sentinel" in camera_source.read_text(
            encoding="utf-8"
        )

        values = read_values(config_path)
        assert values["CONFIG_ESP32S3_SPI_DMA"] == "y"
        assert values["CONFIG_ESP32S3_SPI_DMA_BUFSIZE"] == "15360"
        assert values["CONFIG_ESP32S3_SPI_DMATHRESHOLD"] == "64"
        assert values["CONFIG_LCD_ST7789_FREQUENCY"] == "80000000"

        incompatible_source = "/* incompatible CAM implementation */\n"
        camera_source.write_text(incompatible_source, encoding="utf-8")
        config_before_failure = config_path.read_text(encoding="utf-8")
        failed_run = subprocess.run(
            [str(copied_script)], check=False, env=environment,
            capture_output=True, text=True,
        )
        assert failed_run.returncode != 0
        assert "Cannot apply ESP32-S3 CAM VSYNC fix cleanly" in failed_run.stderr
        assert camera_source.read_text(encoding="utf-8") == incompatible_source
        assert config_path.read_text(encoding="utf-8") == config_before_failure

    print("AgentGuard LCD DMA config test: PASS")


if __name__ == "__main__":
    main()
