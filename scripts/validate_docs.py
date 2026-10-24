#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Kiểm tra tài liệu Lamix OS bằng Python standard library, không cần mạng."""

import argparse
import html
from pathlib import Path
import re
import unicodedata
from urllib.parse import unquote, urlsplit


ROOT_DOCS = (
    "AGENTS.md", "README.md", "GOALS.md", "PLANS.md", "CHANGELOG.md",
    "CONTRIBUTING.md", "ROADMAP.md", "MASTER_PROMPT_LAMIX_OS.md",
    "PROMPT_NEXT_MILESTONE.md", "PROMPT_SESSION_HANDOFF.md",
)
TECH_DOCS = (
    "ARCHITECTURE", "CONTEXT", "CONVENTIONS", "RULES", "TESTING", "SECURITY",
    "DECISIONS", "BOOT_PROTOCOL", "MEMORY_MANAGEMENT", "GRAPHICS", "API",
    "TROUBLESHOOTING", "SESSION_HANDOFF",
)
SKILL = ".agents/skills/docs-consistency/SKILL.md"
REQUIRED = ROOT_DOCS + tuple(f"docs/{name}.md" for name in TECH_DOCS) + (
    SKILL, "LICENSE", ".gitignore", ".github/workflows/docs-ci.yml",
)
EXCLUDED = {
    ".git", ".aws", ".codex", "build", "out", "__pycache__", ".pytest_cache",
    ".venv", "node_modules",
}
STATES = {"PLANNED", "IN_PLAN", "APPROVED", "IN_PROGRESS", "VERIFYING", "DONE", "BLOCKED"}
STATUS_FILES = ("ROADMAP.md", "PLANS.md", "docs/SESSION_HANDOFF.md")
STATUS_PATTERN = re.compile(r"^Trạng thái milestone: M00=(\w+); M01=(\w+)$", re.M)


def prose_lines(text):
    """Bỏ fenced code, giữ số dòng; Markdown subset được mô tả trong TESTING."""
    fence = None
    for number, line in enumerate(text.splitlines(), 1):
        marker = re.match(r"^ {0,3}(`{3,}|~{3,})(.*)$", line)
        if fence:
            if marker and marker[1][0] == fence[0] and len(marker[1]) >= len(fence) and not marker[2].strip():
                fence = None
            continue
        if marker:
            fence = marker[1]
            continue
        # Indented code không tham gia kiểm tra link hoặc headings.
        if line.startswith(("    ", "\t")):
            continue
        yield number, line


def without_code(line):
    return re.sub(r"(`+)(?!`)(.+?)(?<!`)\1(?!`)", "", line)


def anchors(text):
    result = set()
    lines = list(prose_lines(text))
    for index, (_, line) in enumerate(lines):
        heading = re.match(r"^ {0,3}#{1,6}\s+(.+?)(?:\s+#+\s*)?$", line)
        if heading:
            title = heading[1]
        elif index + 1 < len(lines) and lines[index + 1][0] == lines[index][0] + 1 and line.strip() and re.fullmatch(r" {0,3}(?:=+|-+)\s*", lines[index + 1][1]):
            title = line
        else:
            continue
        title = html.unescape(re.sub(r"<[^>]*>", "", title))
        title = re.sub(r"!?\[([^]]+)\]\([^)]*\)", r"\1", title).lower()
        slug = "".join(c for c in title if c in " -_" or unicodedata.category(c)[0] in "LNM").replace(" ", "-")
        candidate, count = slug, 0
        while candidate in result:
            count += 1
            candidate = f"{slug}-{count}"
        result.add(candidate)
    return result


def reference_key(label):
    return " ".join(label.lower().split())


def destination(text, start=0):
    """Đọc destination với ngoặc cân bằng hoặc <...>; bỏ title tùy chọn."""
    text = text[start:].lstrip()
    if text.startswith("<"):
        end = text.find(">")
        return text[1:end] if end >= 0 else None
    depth = 0
    escaped = False
    for index, char in enumerate(text):
        if escaped:
            escaped = False
            continue
        if char == "\\":
            escaped = True
        elif char == "(":
            depth += 1
        elif char == ")":
            if depth == 0:
                return text[:index]
            depth -= 1
        elif char.isspace() and depth == 0:
            return text[:index]
    return text if depth == 0 else None


def markdown_links(text):
    """Trả (dòng, destination, lỗi) cho inline/reference links và images."""
    lines = list(prose_lines(text))
    references = {}
    definition_lines = set()
    for number, line in lines:
        match = re.match(r"^ {0,3}\[([^]]+)\]:\s*(.+)$", line)
        if match:
            references[reference_key(match[1])] = destination(match[2])
            definition_lines.add(number)
    for number, line in lines:
        if number in definition_lines:
            continue
        line = without_code(line)
        consumed = 0
        for match in re.finditer(r"(?<!\\)!?\[([^]\n]+)\]", line):
            if match.start() < consumed:
                continue
            tail = line[match.end():]
            if tail.startswith("("):
                target = destination(tail, 1)
                if target is None:
                    yield number, "", "destination không hợp lệ"
                else:
                    yield number, target, ""
            elif tail.startswith("["):
                ref = re.match(r"\[([^]]*)\]", tail)
                if ref:
                    label = reference_key(ref[1] or match[1])
                    consumed = match.end() + ref.end()
                    if label not in references or references[label] is None:
                        yield number, "", f"reference chưa định nghĩa: {label}"
                    else:
                        yield number, references[label], ""
            elif reference_key(match[1]) in references:
                target = references[reference_key(match[1])]
                yield number, target or "", "" if target is not None else "reference không hợp lệ"


def validate_repository(root):
    root = Path(root).resolve()
    errors = []

    def error(path, message, line=None):
        errors.append(f"{path}{':' + str(line) if line else ''}: {message}")

    for name in REQUIRED:
        if not (root / name).is_file():
            error(name, "thiếu file bắt buộc")
    documents = {}
    for path in sorted(root.rglob("*.md")):
        relative = path.relative_to(root)
        if any(part in EXCLUDED for part in relative.parts):
            continue
        if not path.resolve().is_relative_to(root):
            error(relative, "symlink ra ngoài repository")
            continue
        try:
            text = path.read_bytes().decode("utf-8")
        except (UnicodeDecodeError, OSError):
            error(relative, "không đọc được UTF-8")
            continue
        documents[path.resolve()] = text
        if not text.endswith("\n"):
            error(relative, "thiếu newline cuối file")
        for number, line in enumerate(text.splitlines(), 1):
            suffix = line[len(line.rstrip()):]
            if suffix and (suffix != "  " or not line.strip()):
                error(relative, "whitespace cuối dòng; chỉ cho phép hai spaces cho hard break", number)

    all_anchors = {path: anchors(text) for path, text in documents.items()}
    for path, text in documents.items():
        relative = path.relative_to(root)
        for number, target, problem in markdown_links(text):
            if problem:
                error(relative, problem, number)
                continue
            target = html.unescape(re.sub(r"\\([() ])", r"\1", target))
            try:
                parsed = urlsplit(target)
            except ValueError:
                error(relative, "URL không hợp lệ", number)
                continue
            if parsed.scheme or parsed.netloc:
                continue
            resolved = (path.parent / unquote(parsed.path)).resolve() if parsed.path else path
            if not resolved.is_relative_to(root):
                error(relative, "link ra ngoài repository", number)
            elif not resolved.exists():
                error(relative, f"link không tồn tại: {target}", number)
            elif parsed.fragment and resolved.suffix == ".md" and unquote(parsed.fragment) not in all_anchors.get(resolved, set()):
                error(relative, f"anchor không tồn tại: {target}", number)

    skill = documents.get(root / SKILL, "")
    metadata = re.match(r"\A---\n(.*?)\n---\n", skill, re.S)
    if metadata:
        fields = dict(re.findall(r"^([a-z-]+): ([^\n]+)$", metadata[1], re.M))
        if fields.get("name") != "docs-consistency" or not fields.get("description", "").strip():
            error(SKILL, "metadata cần name docs-consistency và description không rỗng")
    else:
        error(SKILL, "thiếu front matter name/description dạng đơn giản")

    status = {}
    for name in STATUS_FILES:
        matches = STATUS_PATTERN.findall(documents.get(root / name, ""))
        if len(matches) != 1 or any(value not in STATES for pair in matches for value in pair):
            error(name, "cần một dòng trạng thái milestone hợp lệ theo TESTING")
        else:
            status[name] = matches[0]
    if len(set(status.values())) > 1:
        error("PLANS.md", "trạng thái milestone không khớp roadmap/handoff")

    roadmap = documents.get(root / "ROADMAP.md", "")
    milestones = re.findall(r"^\*\*(M\d{2}) — [^\n]+", roadmap, re.M)
    if milestones != [f"M{i:02d}" for i in range(45)]:
        error("ROADMAP.md", "milestone phải đủ M00–M44, đúng thứ tự, không trùng")
    actual = {}
    for match in re.finditer(r"^\*\*(M\d{2}) — (.+)$", roadmap, re.M):
        explicit = re.search(r"\*\*Trạng thái: (\w+)", match[2])
        state = explicit[1] if explicit else "PLANNED"
        if state not in STATES:
            error("ROADMAP.md", f"trạng thái không hợp lệ cho {match[1]}")
        actual[match[1]] = state
    if "ROADMAP.md" in status and tuple(actual.get(n) for n in ("M00", "M01")) != status["ROADMAP.md"]:
        error("ROADMAP.md", "trạng thái chuẩn không khớp trạng thái mục M00/M01")
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    errors = validate_repository(args.root)
    for message in errors:
        print(f"FAIL: {message}")
    if errors:
        return 1
    print("PASS: cấu trúc, UTF-8, whitespace, link/anchor, skill và trạng thái milestone")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
