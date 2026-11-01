# Bảo mật

**Chưa audit; không có bảo đảm bảo mật tổng thể.** M20 cung cấp address-space isolation/fault isolation. M21 đã nghiệm thu syscall entry BSP, kiểm tra user RIP/RSP/RFLAGS, copy user buffer qua mapping có `VMM_USER`/read permission và cô lập process khi return state sai. Các kiểm tra này chưa được audit bảo mật độc lập; PC thật và cấu hình ngoài QEMU chưa chạy. Baseline: [roadmap, mục 1](../ROADMAP.md).

M21 dùng `SYSCALL/SYSRETQ`, `FMASK` để giữ mask interrupt trong entry, thread kernel stack và IST riêng cho NMI/#DB/#MC; chỉ bật nếu CPU báo feature syscall. M22 đã nghiệm thu generation handles/rights, channel queues và SHM mappings trong profile riêng. Atomic batch transfer gắn với enqueue; đồ thị queue từ chối channel reference cycle. Native negative-rights/cycle/race tests và guest read-only fault isolation đã chạy; guest không bao phủ mọi tổ hợp sai tham số và audit bảo mật độc lập **NOT RUN**. `lamix_syscall_handle_valid()` của M21 chỉ là range check, không thay thế xác minh owner/generation/rights ở syscall M22. Nghiệm thu milestone không phải bảo đảm bảo mật tổng thể.

M23 đã triển khai parser newc/ELF trong profile riêng: kiểm tra kích thước/offset, segment overlap, entry, W^X và chỉ xử lý `R_X86_64_RELATIVE`; không có ASLR hoặc dynamic linking. Native sanitizer, guest BIOS/UEFI và CI **PASS**; chưa có audit bảo mật độc lập, và allocation-failure matrix chưa được kiểm thử exhaustive. ELF/initramfs malformed được test riêng; không xem milestone là bảo đảm an toàn tổng thể.

M24 SMP đang IN_PROGRESS: AP chỉ được đánh dấu online sau khi cài per-CPU GDT/TSS/IST/syscall stack; AP startup có timeout; TLB shootdown dùng IPI và acknowledgement giới hạn thời gian. BIOS/UEFI topology matrix và fault injection AP/topology/TLB timeout **PASS** local sau rerun; lượt CTest đầu có một OVMF startup timeout trước serial. Native teardown guard cũng **PASS**. Đã sửa race AP timer-start/configuration và kiểm guest AP timer IRQ. Cần tiếp tục review AP entry/return state, interrupt/preemption khi chờ IPI, CPU active-space mask trong CR3 switch, concurrent process teardown và mọi lock bảo vệ heap/PMM/VMM/IPC. GitHub acceptance CI/review và audit độc lập **PENDING/NOT RUN**.

Disk/image phải bảo vệ GPT metadata và User Data. LamFS cần bounds checks, metadata consistency và kiểm thử dữ liệu hỏng; A/B updates cần recovery có bằng chứng. Chính sách cụ thể chưa đặc tả, không tuyên bố đã an toàn.

Lỗi hoặc nguy cơ mới được ghi ở [handoff](SESSION_HANDOFF.md), cùng tác động và bước xác minh; thay đổi baseline phải qua approval gate.
