#!/usr/bin/env python3
"""Validate AgentGuard contest materials and build the official upload ZIP."""

import argparse
import sys
import zipfile
from pathlib import Path


REPORTS = (
    "AgentGuard-技术报告.pdf",
    "AgentGuard-技术报告.docx",
)
VIDEO_NAMES = (
    "AgentGuard-演示视频.mp4",
    "AgentGuard-演示视频.mov",
)
OPTIONAL_FILES = (
    "AgentGuard-作品海报.pdf",
    "AgentGuard-作品海报.jpg",
    "AgentGuard-作品海报.pptx",
    "AgentGuard-答辩PPT.pptx",
)
PHOTO_SUFFIXES = {".jpg", ".jpeg", ".png"}
PLACEHOLDER_MARKERS = ("待填写", "<队伍名称>", "【队伍名称】")


def collect_deliverables(source: Path) -> tuple[list[tuple[Path, str]], list[str]]:
    files: list[tuple[Path, str]] = []
    errors: list[str] = []

    for name in REPORTS:
        path = source / name
        if path.is_file() and path.stat().st_size:
            files.append((path, name))
        else:
            errors.append(f"缺少技术报告：{name}")

    videos = [source / name for name in VIDEO_NAMES if (source / name).is_file()]
    if len(videos) != 1:
        errors.append("必须且只能提供一个 Demo 视频：AgentGuard-演示视频.mp4 或 .mov")
    elif videos[0].stat().st_size == 0:
        errors.append("Demo 视频为空")
    else:
        files.append((videos[0], videos[0].name))

    photos_dir = source / "photos"
    photos = []
    if photos_dir.is_dir():
        photos = sorted(
            path
            for path in photos_dir.iterdir()
            if path.is_file()
            and path.suffix.lower() in PHOTO_SUFFIXES
            and path.stat().st_size
        )
    if len(photos) < 6:
        errors.append(f"硬件作品至少需要 6 张有效照片；当前找到 {len(photos)} 张")
    else:
        files.extend((path, f"photos/{path.name}") for path in photos)

    for name in OPTIONAL_FILES:
        path = source / name
        if path.is_file() and path.stat().st_size:
            files.append((path, name))

    return files, errors


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="校验 AgentGuard 比赛材料并生成只含交付物的 ZIP"
    )
    parser.add_argument("--source", type=Path, default=Path("submission"))
    parser.add_argument("--team-name", required=True, help="报名表中的正式队伍名称")
    parser.add_argument("--output", type=Path, help="输出 ZIP 路径")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    team_name = args.team_name.strip()
    if not team_name or any(marker in team_name for marker in PLACEHOLDER_MARKERS):
        print("错误：--team-name 必须填写报名表中的正式队伍名称", file=sys.stderr)
        return 2

    source = args.source.resolve()
    if not source.is_dir():
        print(f"错误：材料目录不存在：{source}", file=sys.stderr)
        return 2

    output = args.output or source.parent / (
        f"{team_name}-AgentGuard-contest2026_123_momohuhuhuluobudui.zip"
    )
    output = output.resolve()
    files, errors = collect_deliverables(source)
    if errors:
        for error in errors:
            print(f"错误：{error}", file=sys.stderr)
        return 2

    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path, archive_name in files:
            archive.write(path, archive_name)

    print(f"已生成：{output}")
    print(f"交付文件数：{len(files)}")
    print("注意：请人工确认 Demo 视频时长不超过 5 分钟并完整播放检查。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
