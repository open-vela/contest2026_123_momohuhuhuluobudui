#!/usr/bin/env python3

import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "build_submission_package.py"


class BuildSubmissionPackageTest(unittest.TestCase):
    def make_required_files(self, root: Path, *, include_video: bool = True) -> None:
        (root / "AgentGuard-技术报告.pdf").write_bytes(b"pdf")
        (root / "AgentGuard-技术报告.docx").write_bytes(b"docx")
        if include_video:
            (root / "AgentGuard-演示视频.mp4").write_bytes(b"video")

        photos = root / "photos"
        photos.mkdir()
        for number in range(1, 7):
            (photos / f"{number:02d}-evidence.jpg").write_bytes(b"photo")

    def run_builder(self, root: Path, output: Path) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [
                sys.executable,
                str(SCRIPT),
                "--source",
                str(root),
                "--output",
                str(output),
                "--team-name",
                "测试队伍",
            ],
            check=False,
            capture_output=True,
            text=True,
        )

    def test_missing_video_refuses_to_create_archive(self) -> None:
        """Catches accidentally packaging reports and photos without the required demo."""
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir) / "submission"
            root.mkdir()
            self.make_required_files(root, include_video=False)
            output = Path(temp_dir) / "out.zip"

            result = self.run_builder(root, output)

            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Demo 视频", result.stderr)
            self.assertFalse(output.exists())

    def test_complete_materials_create_archive_with_only_deliverables(self) -> None:
        """Catches leaking helper notes or source/log files into the upload archive."""
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir) / "submission"
            root.mkdir()
            self.make_required_files(root)
            (root / "AgentGuard-作品海报.pdf").write_bytes(b"poster")
            (root / "AgentGuard-答辩PPT.pptx").write_bytes(b"slides")
            (root / "README.md").write_text("helper", encoding="utf-8")
            (root / "secret.jsonl").write_text("log", encoding="utf-8")
            output = Path(temp_dir) / "out.zip"

            result = self.run_builder(root, output)

            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue(output.exists())
            with zipfile.ZipFile(output) as archive:
                self.assertEqual(
                    sorted(archive.namelist()),
                    [
                        "AgentGuard-作品海报.pdf",
                        "AgentGuard-技术报告.docx",
                        "AgentGuard-技术报告.pdf",
                        "AgentGuard-演示视频.mp4",
                        "AgentGuard-答辩PPT.pptx",
                        "photos/01-evidence.jpg",
                        "photos/02-evidence.jpg",
                        "photos/03-evidence.jpg",
                        "photos/04-evidence.jpg",
                        "photos/05-evidence.jpg",
                        "photos/06-evidence.jpg",
                    ],
                )


if __name__ == "__main__":
    unittest.main()
