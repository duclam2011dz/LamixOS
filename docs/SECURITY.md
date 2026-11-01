# Bảo mật

**Chưa audit; không có bảo đảm bảo mật tổng thể.** M20 cung cấp address-space isolation/fault isolation. M21 đang triển khai entry syscall BSP, kiểm tra user RIP/RSP/RFLAGS, copy user buffer qua mapping có `VMM_USER`/read permission và cô lập process khi return state sai. Các kiểm tra này chưa được audit độc lập; PC thật và cấu hình ngoài QEMU chưa chạy. Baseline: [roadmap, mục 1](../ROADMAP.md).

M21 dùng `SYSCALL/SYSRETQ`, `FMASK` để giữ mask interrupt trong entry, thread kernel stack và IST riêng cho NMI/#DB/#MC; chỉ bật nếu CPU báo feature syscall. User pointers được kiểm tra theo từng trang trước khi copy; syscall thường không nhận handle. `lamix_syscall_handle_valid()` hiện chỉ là range check, không thay thế xác minh owner/generation/rights. Capability, object lifetime, IPC và quyền tối thiểu cho handle thuộc M22. Driver kernel-mode và user-mode đều tồn tại; boundary và quyền cần được kiểm chứng khi có interfaces.

Disk/image phải bảo vệ GPT metadata và User Data. LamFS cần bounds checks, metadata consistency và kiểm thử dữ liệu hỏng; A/B updates cần recovery có bằng chứng. Chính sách cụ thể chưa đặc tả, không tuyên bố đã an toàn.

Lỗi hoặc nguy cơ mới được ghi ở [handoff](SESSION_HANDOFF.md), cùng tác động và bước xác minh; thay đổi baseline phải qua approval gate.
