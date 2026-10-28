# SPDX-License-Identifier: GPL-3.0-only
"""Kiểm thử bằng fixtures tạm; không sửa tài liệu thật."""

from pathlib import Path
import tempfile
import unittest

from scripts.validate_docs import REQUIRED, SKILL, STATUS_FILES, validate_repository


class ValidationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        for name in REQUIRED:
            self.write(name, "# Tài liệu\n")
        self.write(SKILL, "---\nname: docs-consistency\ndescription: Kiểm tra tài liệu.\n---\n# Skill\n")
        for name in STATUS_FILES:
            self.write(name, "Trạng thái milestone: M00=DONE; M01=PLANNED; M02=PLANNED; M03=PLANNED; M04=PLANNED; M05=PLANNED\n")
        self.append("ROADMAP.md", "\n".join(
            f"**M{i:02d} — Tên milestone.**" + (" **Trạng thái: DONE.**" if i == 0 else "")
            for i in range(45)
        ) + "\n")

    def write(self, name, text):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def append(self, name, text):
        path = self.root / name
        self.write(name, path.read_text(encoding="utf-8") + text)

    def errors(self):
        return validate_repository(self.root)

    def test_valid_inventory(self):
        self.assertEqual(self.errors(), [])

    def test_missing_required_file(self):
        (self.root / "GOALS.md").unlink()
        self.assertTrue(any("thiếu file" in e for e in self.errors()))

    def test_invalid_utf8(self):
        (self.root / "GOALS.md").write_bytes(b"\xff\n")
        self.assertTrue(any("UTF-8" in e for e in self.errors()))

    def test_missing_final_newline(self):
        self.write("GOALS.md", "# Mục tiêu")
        self.assertTrue(any("newline" in e for e in self.errors()))

    def test_two_spaces_hard_break_allowed(self):
        self.append("GOALS.md", "Một dòng  \nDòng tiếp\n")
        self.assertEqual(self.errors(), [])

    def test_invalid_whitespace(self):
        for suffix in (" ", "   ", "\t"):
            with self.subTest(suffix=repr(suffix)):
                self.write("GOALS.md", f"Nội dung{suffix}\n")
                self.assertTrue(any("whitespace" in e for e in self.errors()))

    def test_missing_link(self):
        self.append("GOALS.md", "[Thiếu](missing.md)\n")
        self.assertTrue(any("link không tồn tại" in e for e in self.errors()))

    def test_missing_anchor(self):
        self.append("GOALS.md", "[Sai](README.md#missing)\n")
        self.assertTrue(any("anchor không tồn tại" in e for e in self.errors()))

    def test_unicode_duplicate_and_encoded_links(self):
        self.write("docs/Thử (mới).md", "# Bộ nhớ\n# Bộ nhớ\n# Bộ nhớ-1\n")
        self.append("GOALS.md", "[Một](docs/Th%E1%BB%AD%20(m%E1%BB%9Bi).md#bộ-nhớ)\n"
                    "[Hai](<docs/Thử (mới).md#bộ-nhớ-1>)\n"
                    "[Ba](docs/Th%E1%BB%AD%20(m%E1%BB%9Bi).md#bộ-nhớ-1-1)\n")
        self.assertEqual(self.errors(), [])

    def test_reference_links_and_images(self):
        self.append("GOALS.md", "[Đọc][ nguồn ]\n[nguồn][]\n[nguồn]\n![Ảnh][nguồn]\n"
                    "[nguồn]: README.md#tài-liệu \"Nguồn\"\n")
        self.assertEqual(self.errors(), [])

    def test_missing_reference(self):
        self.append("GOALS.md", "[Nhãn][chưa có]\n")
        self.assertTrue(any("reference chưa định nghĩa" in e for e in self.errors()))

    def test_broken_reference_target(self):
        self.append("GOALS.md", "[Đọc][nguồn]\n[nguồn]: missing.md\n")
        self.assertTrue(any("link không tồn tại" in e for e in self.errors()))

    def test_code_and_external_urls_are_not_followed(self):
        self.append("GOALS.md", "```md\n[Ví dụ](missing.md)\n# Heading giả\n```\n"
                    "~~~\n[Ví dụ](missing.md)\n~~~\n"
                    "`[Ví dụ](missing.md)` và ``[Ví dụ](missing.md)``\n"
                    "    [Code thụt dòng](missing.md)\n"
                    "[Ngoài](https://example.invalid/no-such-page)\n")
        self.assertEqual(self.errors(), [])

    def test_heading_inside_code_is_not_an_anchor(self):
        self.append("GOALS.md", "```\n# Giả\n```\n[Đọc](#giả)\n")
        self.assertTrue(any("anchor không tồn tại" in e for e in self.errors()))

    def test_setext_heading(self):
        self.append("GOALS.md", "Tiêu đề\n=======\n[Đọc](#tiêu-đề)\n")
        self.assertEqual(self.errors(), [])

    def test_outside_repository_link(self):
        self.append("GOALS.md", "[Ngoài](../outside.md)\n")
        self.assertTrue(any("ngoài repository" in e for e in self.errors()))

    def test_excluded_output_directory(self):
        self.write("out/generated.md", "[Thiếu](missing.md)\n")
        self.assertEqual(self.errors(), [])

    def test_duplicate_and_reordered_milestones(self):
        path = self.root / "ROADMAP.md"
        original = path.read_text(encoding="utf-8")
        for text in (
            original.replace("**M02 —", "**M01 —"),
            original.replace("**M02 —", "**M99 —").replace("**M03 —", "**M02 —").replace("**M99 —", "**M03 —"),
        ):
            with self.subTest(text=text[-100:]):
                self.write("ROADMAP.md", text)
                self.assertTrue(any("đúng thứ tự" in e for e in self.errors()))

    def test_invalid_milestone_state(self):
        self.append("PLANS.md", "Trạng thái milestone: M00=BOGUS; M01=PLANNED; M02=PLANNED; M03=PLANNED; M04=PLANNED; M05=PLANNED\n")
        self.assertTrue(any("một dòng trạng thái" in e for e in self.errors()))

    def test_disagreeing_status_files(self):
        self.write("PLANS.md", "Trạng thái milestone: M00=VERIFYING; M01=PLANNED; M02=PLANNED; M03=PLANNED; M04=PLANNED; M05=PLANNED\n")
        self.assertTrue(any("không khớp roadmap/handoff" in e for e in self.errors()))

    def test_canonical_status_must_match_milestone(self):
        path = self.root / "ROADMAP.md"
        self.write("ROADMAP.md", path.read_text().replace("**Trạng thái: DONE.**", "**Trạng thái: VERIFYING.**"))
        self.assertTrue(any("không khớp trạng thái mục" in e for e in self.errors()))

    def test_invalid_skill_metadata(self):
        self.write(SKILL, "---\nname: wrong\ndescription: \n---\n# Skill\n")
        self.assertTrue(any("metadata" in e for e in self.errors()))

    def test_m02_state_must_match_roadmap(self):
        for name in STATUS_FILES:
            path = self.root / name
            self.write(name, path.read_text().replace("M02=PLANNED", "M02=VERIFYING"))
        self.assertTrue(any("không khớp trạng thái mục" in e for e in self.errors()))

    def test_m02_state_must_match_handoff(self):
        path = self.root / "docs/SESSION_HANDOFF.md"
        self.write(str(path.relative_to(self.root)), path.read_text().replace("M02=PLANNED", "M02=BLOCKED"))
        self.assertTrue(any("không khớp roadmap/handoff" in e for e in self.errors()))


    def test_m03_state_must_match_roadmap(self):
        for name in STATUS_FILES:
            path = self.root / name
            self.write(name, path.read_text().replace("M03=PLANNED", "M03=VERIFYING"))
        self.assertTrue(any("không khớp trạng thái mục" in e for e in self.errors()))

    def test_m03_state_must_match_handoff(self):
        self.write("docs/SESSION_HANDOFF.md", "Trạng thái milestone: M00=DONE; M01=PLANNED; M02=PLANNED; M03=BLOCKED; M04=PLANNED; M05=PLANNED\n")
        self.assertTrue(any("không khớp roadmap/handoff" in e for e in self.errors()))

    def test_m03_duplicate_status_rejected(self):
        self.append("PLANS.md", "Trạng thái milestone: M00=DONE; M01=PLANNED; M02=PLANNED; M03=PLANNED; M04=PLANNED; M05=PLANNED\n")
        self.assertTrue(any("một dòng trạng thái" in e for e in self.errors()))


    def test_m04_state_must_match_roadmap(self):
        for name in STATUS_FILES:
            path = self.root / name
            self.write(name, path.read_text().replace("M04=PLANNED", "M04=VERIFYING"))
        self.assertTrue(any("không khớp trạng thái mục" in e for e in self.errors()))

    def test_m04_state_must_match_handoff(self):
        path = self.root / "docs/SESSION_HANDOFF.md"
        self.write(str(path.relative_to(self.root)), path.read_text().replace("M04=PLANNED", "M04=BLOCKED"))
        self.assertTrue(any("không khớp roadmap/handoff" in e for e in self.errors()))

    def test_m04_duplicate_status_rejected(self):
        self.append("PLANS.md", "Trạng thái milestone: M00=DONE; M01=PLANNED; M02=PLANNED; M03=PLANNED; M04=PLANNED; M05=PLANNED\n")
        self.assertTrue(any("một dòng trạng thái" in e for e in self.errors()))

    def test_m05_state_must_match_roadmap(self):
        for name in STATUS_FILES:
            path = self.root / name
            self.write(name, path.read_text().replace("M05=PLANNED", "M05=VERIFYING"))
        self.assertTrue(any("không khớp trạng thái mục" in e for e in self.errors()))

    def test_m05_state_must_match_handoff(self):
        path = self.root / "docs/SESSION_HANDOFF.md"
        self.write(str(path.relative_to(self.root)), path.read_text().replace("M05=PLANNED", "M05=BLOCKED"))
        self.assertTrue(any("không khớp roadmap/handoff" in e for e in self.errors()))

    def test_m05_duplicate_status_rejected(self):
        self.append("PLANS.md", "Trạng thái milestone: M00=DONE; M01=PLANNED; M02=PLANNED; M03=PLANNED; M04=PLANNED; M05=PLANNED\n")
        self.assertTrue(any("một dòng trạng thái" in e for e in self.errors()))


if __name__ == "__main__":
    unittest.main()
