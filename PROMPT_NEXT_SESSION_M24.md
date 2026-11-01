# Prompt chuyển phiên Codex — Lập kế hoạch M24

> Dùng trong một phiên Codex mới. **Trước khi khảo sát M24, hãy chủ động bật Codex Plan Mode thật trên giao diện.** Prompt và `AGENTS.md` không tự đổi chế độ UI.

Bạn là Codex tiếp tục phát triển **Lamix OS**. Làm việc và viết Markdown bằng tiếng Việt rõ ràng; giữ nguyên tên kỹ thuật, API, symbol, đường dẫn và lệnh khi cần.

## Nhiệm vụ

Chỉ khảo sát và lập kế hoạch cho **M24 — SMP: AP startup và per-CPU scheduler** theo `ROADMAP.md`. M23 đã nghiệm thu; M24 vẫn `PLANNED`, chưa được duyệt triển khai. Không bắt đầu code, tests, fixtures hoặc M25+ trong lượt lập kế hoạch.

## Plan Mode và giới hạn

- Nếu giao diện chưa ở Codex Plan Mode thật, chỉ nhắc tôi chủ động chuyển sang Plan Mode rồi dừng; chưa khảo sát milestone.
- Sau khi ở Plan Mode, chỉ đọc, xác minh và lập kế hoạch. Không sửa/tạo file dự án, triển khai code/tests/fixtures, cài dependencies, chạy build/CTest/QEMU/CI/benchmark, commit/push/tag, thay đổi GitHub hoặc milestone status.
- Kết thúc bằng kế hoạch M24, rủi ro và quyết định còn mở; dừng chờ tôi duyệt rõ ràng trước khi triển khai.

## Baseline đã nghiệm thu

- Workspace/repository: `/home/lam/projects/LamixOS`, `https://github.com/duclam2011dz/LamixOS`; xác minh lại `git status`, branch/HEAD, remote, PR/tag và CI khi dịch vụ sẵn sàng.
- M23 bổ sung Static PIE ET_DYN ELF64 loader, newc initramfs, `R_X86_64_RELATIVE`, user runtime freestanding trong profile riêng `m23-kernel`; giữ nguyên LBP v1, ABI 1.1/profile M22 và `LATEST_PROFILE=m20`.
- Acceptance commit, merge commit và annotated tag `milestone/M23`: **điền provenance thực tế từ `docs/SESSION_HANDOFF.md` khi dùng prompt này**; không suy ra từ snapshot cũ.
- Bằng chứng M23: CTest 3/3; guest SeaBIOS `pc` và OVMF `q35` 3/3 mỗi firmware; regressions M17/M19/M20/M21/M22; docs validator/unittest; bốn workflow CI và review được ghi cùng SHA trong `docs/SESSION_HANDOFF.md`.
- M18 vẫn `PLANNED`/để dành. `latest` giữ M20. M25+ không thuộc phạm vi. Snapshot chỉ gợi ý; nếu không kiểm tra được mục nào, ghi `NOT RUN`/`chưa xác minh`, không suy diễn PASS.

## Tài liệu và khảo sát

Tuân thủ `AGENTS.md` hiện hành; đọc `docs/SESSION_HANDOFF.md`, `PLANS.md`, phần M24 trong `ROADMAP.md`, `README.md`, `GOALS.md`, `CONTRIBUTING.md`, `CHANGELOG.md` và tài liệu architecture/scheduler/interrupt/memory/testing liên quan. Đọc `AGENTS.md` có phạm vi áp dụng và metadata skill phù hợp trước khi dùng.

Xác minh trạng thái thực tế của BSP scheduler, APIC/IOAPIC, MADT CPU discovery, trampoline/INIT-SIPI, per-CPU state/TSS/IST/interrupt stacks, CR3/TLB shootdown, locks/critical sections, affinity và các profile/CI gates. Phân biệt phần đã chạy trên một CPU với SMP thật; không giả định hệ thống hiện tại đã an toàn cho chạy đồng thời.

## Phạm vi M24 theo Roadmap

Roadmap mô tả AP discovery từ MADT, trampoline/INIT-SIPI, per-CPU stacks/TSS/interrupt state, TLB shootdown, per-CPU run queues và affinity/balancing. Acceptance dự kiến có QEMU 1/2/4/8 vCPU cùng stress concurrency không deadlock, đồng thời giữ profile BSP-only hiện hành. Không mở rộng sang filesystem, user-level threading, scheduler redesign ngoài mục tiêu, hay milestone M25+.

Đề xuất thứ tự triển khai, module/file/profile dự kiến, synchronization/memory ownership, interrupt/preemption constraints, fault-injection, test host/guest, serial evidence, CI/provenance, hồi quy M17/M19–M23, rollback và điều kiện dừng. Nêu rõ race/deadlock, startup timeout, AP partial failure, TLB coherence, per-CPU stack/context corruption và ảnh hưởng đến profile cũ.

Không đóng băng API, topology policy, startup protocol, queue/affinity semantics hoặc acceptance changes trong lượt lập kế hoạch nếu chưa được duyệt. Phân loại baseline đã duyệt, đề xuất, quyết định cần tôi chốt và phần chưa chứng minh.

**Đầu ra:** báo cáo trạng thái M23/M24 đã xác minh + kế hoạch M24 cụ thể + câu hỏi cần chốt; sau đó dừng chờ phê duyệt. Không bắt đầu M24 implementation.
