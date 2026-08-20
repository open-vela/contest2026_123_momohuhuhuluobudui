#!/usr/bin/env python3

import os
import shutil
import subprocess
import tempfile
from pathlib import Path


TEST_DIR = Path(__file__).resolve().parent
REPO_ROOT = TEST_DIR.parents[2]
CONFIG_SCRIPT = REPO_ROOT / "tools" / "apply_agentguard_config.sh"

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
        nuttx_dir = openvela_root / "nuttx"
        tweak = (openvela_root / "prebuilts" / "build-tools" /
                 "linux-x86_64" / "bin" / "kconfig-tweak")
        copied_script = tools_dir / CONFIG_SCRIPT.name
        config_path = nuttx_dir / ".config"

        tools_dir.mkdir(parents=True)
        nuttx_dir.mkdir(parents=True)
        tweak.parent.mkdir(parents=True)
        shutil.copy2(CONFIG_SCRIPT, copied_script)
        copied_script.chmod(0o755)
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
        subprocess.run([str(copied_script)], check=True, env=environment,
                       capture_output=True, text=True)

        values = read_values(config_path)
        assert values["CONFIG_ESP32S3_SPI_DMA"] == "y"
        assert values["CONFIG_ESP32S3_SPI_DMA_BUFSIZE"] == "15360"
        assert values["CONFIG_ESP32S3_SPI_DMATHRESHOLD"] == "64"
        assert values["CONFIG_LCD_ST7789_FREQUENCY"] == "80000000"

    print("AgentGuard LCD DMA config test: PASS")


if __name__ == "__main__":
    main()
