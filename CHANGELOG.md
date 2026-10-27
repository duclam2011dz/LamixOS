# Lịch sử thay đổi

Theo dạng Keep a Changelog đơn giản. Chỉ ghi thay đổi thực tế; chưa có release.

## Unreleased

### Thêm

- Hướng dẫn Codex, giới thiệu, mục tiêu, kế hoạch M00 và quy trình đóng góp bằng tiếng Việt.
- Hệ thống 13 tài liệu docs: phân nguồn sự thật, skeleton kỹ thuật, kiểm thử và bàn giao.
- .gitignore cho build/out và cache Python.
- Repository Git với branch main.
- Skill docs-consistency tại .agents/skills/docs-consistency/SKILL.md, đã qua validator và rà soát M00.

- Giấy phép GPL-3.0-only, văn bản GPL v3 tiếng Anh chính thức.
- Validator Python độc lập, 22 unittest và workflow Documentation CI; đã chạy thành công trên GitHub-hosted runner.
- Repository GitHub public duclam2011dz/LamixOS, remote origin/main và commit bằng email noreply.
- Quy trình Git/CI, milestone tags và release gate; chuẩn hóa đường dẫn host trong tài liệu.

### Ghi chú

M00 đạt nghiệm thu phần GitHub/GPL/CI ngày 2026-10-24; người dùng đã chính thức xác nhận hoàn thành. Annotated tag milestone/M00 đã được tạo và push trên commit aa6aeb9ff8ea4620227020ea43e03f430328d6b6; chưa tạo version tag hoặc Release. Chưa có mã OS hoặc bản phát hành; M01 đạt kiểm chứng local và Toolchain CI cho build sample; annotated tag milestone/M01 đã được người dùng duyệt, tạo và push trên commit nghiệm thu 99f7591046c843d77362eed5f16cf6a34a085bbb. Không tạo version tag hoặc Release; tại mốc đóng M01 chưa bắt đầu M02.

### Thêm trong M01

- Doctor host/cross-toolchain với báo cáo JSON, dependencies có phiên bản/hash trong môi trường riêng.
- CMake/Ninja presets Debug/Release, sample NASM/C11/C++17 freestanding và verifier ELF/link provenance.
- Host tests và kiểm chứng SHA-256 giữa hai source roots; bootstrap GCC 12.2.0/Binutils 2.39 từ GNU archives đã xác minh chữ ký/hash.
- Workflow Toolchain CI đã chạy GNU source bootstrap và doctor/build/reproducibility thành công; cache chính xác và logs lỗi được cấu hình. URLs/kết quả tại handoff.

### Thêm trong M02

- Harness host QMP/serial với monotonic deadlines, phân loại kết quả, process-group cleanup và logs/JSON riêng từng run.
- Fixture tests và QEMU launch-only thật BIOS/UEFI; boot skeleton chưa có image được SKIPPED/77, không phải OS boot PASS.
- CTest host-only, pytest marker qemu, validator trạng thái mở rộng tới M02 và Harness CI riêng với JUnit/summary/logs lỗi.

M02 đã kiểm chứng local và cả ba workflows trên GitHub; kết quả/URLs ở handoff. Người dùng đã phê duyệt kết thúc; annotated tag milestone/M02 đã được tạo/push và xác minh trên commit nghiệm thu 3a4f6206f04287d07b8bd9f29d5cc941af2f50dc. Không tạo version tag hoặc GitHub Release; chưa chuyển phiên hoặc bắt đầu M03. Không tạo boot code/image hoặc thay tags M00/M01.

- Sửa setup CI bị chậm tải package từ Azure Ubuntu mirror: ưu tiên HTTPS Ubuntu archive trong runner, giữ chữ ký apt/package list, thêm retry/timeout; không đổi apt sources host local hoặc hạ tiêu chí tests.

### Thêm trong M03

- LBP v1 headers C11/C++17 với assertions từng field, ELF note descriptor, record parsing/validation và memory map normalization freestanding dùng caller-owned buffers.
- Đặc tả entry/ownership/compatibility và GPT profile; mô hình host metadata512/4096, không tạo bootable image hoặc disk I/O.
- Host positive/negative/golden tests, native mutations với ASan/UBSan, cross compile-only không undefined runtime symbols; Boot Contract CI và cross step trong Toolchain CI.
- Bảo vệ cả ACPI reclaim có nonvolatile attributes khỏi cấp phát/thu hồi; bổ sung negative test.
- Validator và unittest trạng thái mở rộng đến M03, đồng bộ tài liệu API/memory/testing; giữ nguyên ba prompt gốc và tags M00–M02. Kết quả local/CI thật ở handoff; chưa xác nhận DONE hoặc tạo tag M03.
