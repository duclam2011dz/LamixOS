# Bảo mật

**Chưa audit; không có bảo đảm bảo mật tổng thể.** M20 cung cấp address-space isolation/fault isolation. M21 đã nghiệm thu syscall entry BSP, kiểm tra user RIP/RSP/RFLAGS, copy user buffer qua mapping có `VMM_USER`/read permission và cô lập process khi return state sai. Các kiểm tra này chưa được audit bảo mật độc lập; PC thật và cấu hình ngoài QEMU chưa chạy. Baseline: [roadmap, mục 1](../ROADMAP.md).

M21 dùng `SYSCALL/SYSRETQ`, `FMASK` để giữ mask interrupt trong entry, thread kernel stack và IST riêng cho NMI/#DB/#MC; chỉ bật nếu CPU báo feature syscall. M22 đang thêm generation handles, rights, channel queues và SHM mappings trong profile riêng. Guest đã chứng minh MOVE SHM cross-process và cô lập một user write fault trên mapping read-only; guest negative-rights/invalid-pointer matrix, đầy đủ close/send/receive/reuse races và audit bảo mật độc lập chưa hoàn thành. `lamix_syscall_handle_valid()` của M21 chỉ là range check, không thay thế xác minh owner/generation/rights. Không coi M22 đang triển khai là bảo đảm bảo mật.

Disk/image phải bảo vệ GPT metadata và User Data. LamFS cần bounds checks, metadata consistency và kiểm thử dữ liệu hỏng; A/B updates cần recovery có bằng chứng. Chính sách cụ thể chưa đặc tả, không tuyên bố đã an toàn.

Lỗi hoặc nguy cơ mới được ghi ở [handoff](SESSION_HANDOFF.md), cùng tác động và bước xác minh; thay đổi baseline phải qua approval gate.
