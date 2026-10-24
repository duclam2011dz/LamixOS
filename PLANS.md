# Kế hoạch hiện hành — M01

Trạng thái milestone: M00=DONE; M01=DONE

Người dùng đã duyệt triển khai M01 trong phiên hiện tại ngày 2026-10-24. M00 giữ nguyên tag milestone/M00 trên aa6aeb9ff8ea4620227020ea43e03f430328d6b6.

## Phạm vi được duyệt

Doctor, dependencies cô lập, CMake/Ninja Debug/Release, sample NASM/C11/C++17 freestanding, kiểm chứng ELF/link không dùng host runtime và SHA-256 tái lập ở hai đường dẫn nguồn khác nhau. Bootstrap CI dùng GCC 12.2.0/Binutils 2.39 từ GNU source ghim hash; không thay cross-toolchain local.

## Thứ tự

- [x] Chuẩn bị công cụ thiếu, doctor và dependency lock.
- [x] Build sample, CTest/host tests và ELF verification.
- [x] Chứng minh tái lập Debug/Release và regression tài liệu.
- [x] Push commits đã review; Documentation CI và Toolchain CI trên fee069b8d22f836113c0f0049157de819a2f7f09 đã SUCCESS, gồm GNU bootstrap thật. Commit tài liệu nghiệm thu tiếp sau phải có cả hai workflows xanh trước báo cáo cuối.
- [x] Người dùng phê duyệt kết thúc M01; annotated tag milestone/M01 đã tạo/push và xác minh local/remote trên 99f7591046c843d77362eed5f16cf6a34a085bbb.

## Giới hạn và rollback

Không triển khai bootloader/kernel/LBP hoặc M02; không chạy ELF như chương trình host. Local và CI thật đã đạt; M01 PASS cuối phiên chỉ khi CI trên HEAD tài liệu nghiệm thu cũng đạt; không tự hạ tiêu chí. Không đổi tag M00, force-push, overwrite prefix chưa xác minh hoặc gỡ gói hệ thống. Rollback bằng diff/commit mới, artifacts ở build/out và .venv. Tag M01 đã tạo sau phê duyệt kết thúc riêng; không chuyển tag sang commit tài liệu đóng thủ tục.

Thủ tục kết thúc M01 được hoàn tất trong phiên hiện tại. Theo chỉ dẫn mới nhất, lượt này không chuyển phiên hoặc bắt đầu M02. Trạng thái thật tại [handoff](docs/SESSION_HANDOFF.md), nguồn milestone là [roadmap](ROADMAP.md).

## Bằng chứng nghiệm thu

Doctor local PASS (KVM WARN tùy chọn); 39 host tests PASS, CTest 2/2 mỗi profile, ELF verification và SHA-256 hai source roots PASS. CI đã build GNU source thật, doctor/build/reproducibility PASS, cache prefix đã lưu. URLs và giới hạn tại [handoff](docs/SESSION_HANDOFF.md).

Annotated milestone/M01 đã được xác minh đúng SHA nghiệm thu và giữ nguyên sau commit tài liệu đóng thủ tục. Commit tài liệu cần cả hai workflows SUCCESS trước báo cáo cuối. Chờ chỉ dẫn tiếp theo của người dùng; không tự chuyển phiên hoặc triển khai M02.
