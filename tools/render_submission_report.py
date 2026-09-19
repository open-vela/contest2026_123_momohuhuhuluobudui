#!/usr/bin/env python3
"""Render the AgentGuard Markdown report as print-ready HTML."""

from __future__ import annotations

import argparse
import html
import re
from pathlib import Path


def inline(text: str) -> str:
    escaped = html.escape(text.strip())
    escaped = re.sub(r"`([^`]+)`", r"<code>\1</code>", escaped)
    escaped = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", escaped)
    escaped = re.sub(
        r"\[([^]]+)]\(([^)]+)\)", r'<a href="\2">\1</a>', escaped)
    return escaped


def table(lines: list[str]) -> str:
    rows = [[cell.strip() for cell in line.strip().strip("|").split("|")]
            for line in lines]
    if len(rows) > 1 and all(re.fullmatch(r":?-{3,}:?", cell)
                             for cell in rows[1]):
        rows.pop(1)
    output = ["<table>"]
    for row_index, row in enumerate(rows):
        tag = "th" if row_index == 0 else "td"
        output.append("<tr>" + "".join(
            f"<{tag}>{inline(cell)}</{tag}>" for cell in row) + "</tr>")
    output.append("</table>")
    return "\n".join(output)


def markdown_body(source: str) -> str:
    lines = source.splitlines()
    output: list[str] = []
    paragraph: list[str] = []
    list_type: str | None = None
    code_lines: list[str] = []
    in_code = False
    index = 0

    def flush_paragraph() -> None:
        if paragraph:
            output.append(f"<p>{inline(' '.join(paragraph))}</p>")
            paragraph.clear()

    def close_list() -> None:
        nonlocal list_type
        if list_type:
            output.append(f"</{list_type}>")
            list_type = None

    while index < len(lines):
        line = lines[index]
        if line.startswith("```"):
            flush_paragraph()
            close_list()
            if in_code:
                output.append("<pre><code>" +
                              html.escape("\n".join(code_lines)) +
                              "</code></pre>")
                code_lines.clear()
            in_code = not in_code
            index += 1
            continue
        if in_code:
            code_lines.append(line)
            index += 1
            continue
        if line.startswith("|"):
            flush_paragraph()
            close_list()
            table_lines = []
            while index < len(lines) and lines[index].startswith("|"):
                table_lines.append(lines[index])
                index += 1
            output.append(table(table_lines))
            continue
        heading = re.match(r"^(#{1,6})\s+(.+)$", line)
        if heading:
            flush_paragraph()
            close_list()
            level = len(heading.group(1))
            output.append(f"<h{level}>{inline(heading.group(2))}</h{level}>")
            index += 1
            continue
        item = re.match(r"^\s*(-|\d+\.)\s+(.+)$", line)
        if item:
            flush_paragraph()
            desired = "ul" if item.group(1) == "-" else "ol"
            if list_type != desired:
                close_list()
                list_type = desired
                output.append(f"<{list_type}>")
            output.append(f"<li>{inline(item.group(2))}</li>")
            index += 1
            continue
        if line.startswith(">"):
            flush_paragraph()
            close_list()
            output.append(f"<blockquote>{inline(line[1:])}</blockquote>")
            index += 1
            continue
        if not line.strip():
            flush_paragraph()
            close_list()
        else:
            paragraph.append(line.strip())
        index += 1

    flush_paragraph()
    close_list()
    return "\n".join(output)


def render(source: Path, destination: Path) -> None:
    body = markdown_body(source.read_text(encoding="utf-8"))
    document = f"""<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8">
<title>AgentGuard 技术报告</title>
<style>
@page {{ size: A4; margin: 20mm 18mm 18mm; }}
body {{ font-family: 'Noto Sans CJK SC', 'Microsoft YaHei', sans-serif;
       color: #172033; font-size: 10.5pt; line-height: 1.55; }}
h1 {{ color: #123b66; font-size: 20pt; text-align: center;
      margin: 50mm 0 18mm; page-break-after: avoid; }}
h2 {{ color: #0c5684; font-size: 16pt; border-bottom: 2px solid #4da3d9;
      padding-bottom: 4px; margin-top: 18px; page-break-after: avoid; }}
h3 {{ color: #174f72; font-size: 13pt; margin-top: 15px;
      page-break-after: avoid; }}
p {{ text-align: justify; margin: 6px 0; }}
table {{ border-collapse: collapse; width: 100%; margin: 10px 0 14px;
         page-break-inside: avoid; }}
th {{ background: #dceefa; color: #123b66; }}
th, td {{ border: 1px solid #7b98ad; padding: 6px 8px; vertical-align: top; }}
th {{ font-size: 9.5pt; }}
td {{ font-size: 9pt; line-height: 1.4; }}
th:first-child, td:first-child {{ width: 22%; }}
code {{ font-family: 'Noto Sans Mono CJK SC', monospace; color: #8a2d14;
        background: #f4f6f8; padding: 1px 3px; }}
pre {{ background: #f4f6f8; border-left: 4px solid #4da3d9;
       padding: 10px; white-space: pre-wrap; }}
blockquote {{ background: #fff6dd; border-left: 4px solid #e3a52b;
              margin: 10px 0; padding: 7px 12px; }}
li {{ margin: 3px 0; }}
</style></head><body>{body}</body></html>"""
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(document, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    render(args.source, args.destination)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
