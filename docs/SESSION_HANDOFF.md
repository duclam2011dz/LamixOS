# Bàn giao trạng thái — M01, 2026-10-24

Trạng thái milestone: M00=DONE; M01=DONE

M01 đã đạt kiểm chứng local và GitHub CI. **Chưa chuyển phiên:** người dùng yêu cầu chỉ chuyển sau M01 PASS và hoàn tất thủ tục Git/tag/bàn giao. Annotated tag milestone/M01 còn chờ xác nhận hoàn thành riêng; không tạo version tag/Release hoặc triển khai M02.

## Repository và ranh giới milestone

Repository [duclam2011dz/LamixOS](https://github.com/duclam2011dz/LamixOS), Public/main, GPL-3.0-only; origin `https://github.com/duclam2011dz/LamixOS.git`. Tag M00 giữ nguyên trên aa6aeb9ff8ea4620227020ea43e03f430328d6b6.

Commits M01: eae3d68 (implementation) và fee069b8d22f836113c0f0049157de819a2f7f09 (sửa context workflow). Commit tài liệu chứa bản này ghi nhận nghiệm thu; lấy SHA bằng `git rev-parse HEAD`. Sau push phải xác minh main local/remote cùng SHA, working tree sạch và cả hai workflows thành công trên HEAD trước báo cáo cuối. Không tạo vòng lặp commit để tự ghi SHA/run của chính commit.

## Đã triển khai

- Doctor Python standard library, kiểm tra host/cross-tools/firmware; JSON không dump environment/secrets.
- Dependencies host trong .venv với lockfile có hashes; CMake/Ninja presets Debug/Release, sample NASM/C11/C++17 chỉ compile/link.
- ELF/link-map/compiler verification, CTest và host tests; so SHA-256 ở hai source roots tạm khác nhau.
- Bootstrap GCC 12.2.0/Binutils 2.39 từ GNU source đã xác minh signatures/hashes; không thay cross-toolchain local.
- Toolchain CI có cache chính xác, logs lỗi; cập nhật README/TOOLCHAIN/TESTING/CONVENTIONS/TROUBLESHOOTING/PLANS/CHANGELOG/ROADMAP, không đổi kiến trúc hoặc thứ tự milestone.

## Bằng chứng thật

| Kiểm tra | Kết quả |
|---|---|
| Doctor profile wsl2 | PASS yêu cầu bắt buộc; KVM WARN tùy chọn. Python 3.10.12, GCC/G++ 12.2.0, Binutils 2.39, CMake/CTest 3.22.6, Ninja 1.11.1, pytest 8.3.5 |
| `python -m pytest -q tests` trong .venv/PATH | PASS, 39 tests; gồm negative configure thật, ELF fixtures và bootstrap guards |
| `python scripts/smoke.py --reproducible` | PASS Debug/Release; CTest 2/2 mỗi profile; ELF static, đúng compiler và chỉ ba object sample |
| Hai source roots sạch | PASS; Debug SHA-256 27bff42a69a12de77ef5e28876258b13b8e6e5439a3fd23439c9c5bdba04967c; Release 7a940cf7c465d8fa300dee97a7d39916271f51b8f97238578df6f16a8ee7a099 |
| [Toolchain CI 40112238564](https://github.com/duclam2011dz/LamixOS/actions/runs/40112238564) | SUCCESS trên fee069b; logs xác nhận GNU bootstrap, doctor, CTest/ELF và tái lập thực sự chạy; cache đã lưu |
| [Documentation CI 40112238477](https://github.com/duclam2011dz/LamixOS/actions/runs/40112238477) | SUCCESS trên cùng fee069b |
| Validator tài liệu, 22 unittest tài liệu và checksum ba prompt | PASS; không đổi mục tiêu/thứ tự roadmap hoặc tag M00 |
| Boot BIOS/UEFI, kernel, QEMU smoke | NOT RUN, ngoài M01; không chạy target ELF trên host |

Logs/JSON local ở out/m01 và build/*/Testing/Temporary; ignored. CI run của commit tài liệu cuối được báo cuối phiên sau khi xác minh theo SHA; tra [Toolchain CI](https://github.com/duclam2011dz/LamixOS/actions/workflows/toolchain-ci.yml) và [Documentation CI](https://github.com/duclam2011dz/LamixOS/actions/workflows/docs-ci.yml). Không coi workflow tồn tại là PASS.

## Giới hạn và bước tiếp theo chính xác

Sudo local cần mật khẩu nên không cài apt; dependencies thiếu được cài cô lập trong .venv, không đổi Python hệ thống. CI dùng apt trên Ubuntu 22.04/Python 3.12. Hash tái lập chỉ cam kết cùng toolchain/input/profile giữa hai đường dẫn, không giữa mọi host/version. Lint tools và QEMU/GDB/firmware mới kiểm tra availability/version, không chứng minh OS chạy. Chưa bật branch protection; trigger PR/manual chưa chạy thật.

Run Toolchain đầu tiên 40112182236 FAIL trước tạo job do context runner.temp ở job env; đã sửa sang github.workspace và run kế tiếp SUCCESS. API steps có lúc chậm cập nhật so với kết luận run; đã đối chiếu logs hoàn chỉnh. GitHub cảnh báo các Actions ghim Node 20 được ép chạy Node 24; run thực tế vẫn thành công. Không âm thầm bỏ kiểm tra hoặc sửa tag/lịch sử.

**Bước tiếp theo trong phiên hiện tại:** báo SHA/CI cuối và chờ người dùng xác nhận M01 hoàn thành. Sau xác nhận mới tạo/push annotated milestone/M01 trên SHA đã CI xanh, xác minh tag/peeled SHA và cập nhật commit đóng thủ tục với CI thật. Chỉ sau đó mới chuyển phiên. M02 còn PLANNED; phiên mới phải bật Plan Mode, đọc AGENTS/handoff/PLANS/M02, khảo sát và chờ duyệt kế hoạch riêng.
