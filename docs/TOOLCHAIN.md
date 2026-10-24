# Toolchain và build M01

## Phạm vi

Chỉ build sample NASM/C11/C++17 để kiểm chứng toolchain, không phải kernel, bootloader hoặc LBP. Host mục tiêu WSL2 Linux x86_64; CI là Linux x86_64 thường. Minimum Python 3.10, CMake 3.22, Ninja 1.10; reference cross GCC/G++ 12.2.0 và Binutils 2.39. Không cam kết ELF giống nhau giữa mọi host/compiler version.

## Chuẩn bị host

Dùng cross-toolchain đang có nếu doctor và sample đạt. Nếu đặt LAMIX_CROSS_ROOT, prefix phải chứa bin/x86_64-elf-gcc cùng G++ và Binutils; nếu không đặt, tìm x86_64-elf-gcc trong PATH. Không fallback GCC host.

APT chỉ cài gói thiếu khi có quyền: build-essential, bison, flex, libgmp-dev, libmpfr-dev, libmpc-dev, texinfo, xz-utils, nasm, cmake, ninja-build, python3-pip, python3-venv, qemu-system-x86, gdb, ovmf, seabios, clang-format, clang-tidy và cppcheck. Không cần cài lại công cụ đã có.

Môi trường local hiện tại không có sudo không mật khẩu nên dùng .venv thay vì thay hệ thống. Dependencies Python/CMake/Ninja Linux x86_64 ghim cùng hash trong [requirements-dev.lock](../requirements-dev.lock). Khi host có venv/pip:

```bash
python3 -m venv .venv
.venv/bin/python -m pip install --require-hashes --only-binary=:all: -r requirements-dev.lock
. .venv/bin/activate
```

Nếu Ubuntu thiếu ensurepip, tạo venv bằng `python3 -m venv --without-pip .venv`; tải wheel pip đúng phiên bản và SHA-256 ghi trong lockfile từ PyPI vào out, đối chiếu hash rồi dùng PYTHONPATH trỏ wheel để chạy `.venv/bin/python -m pip install --require-hashes --only-binary=:all: -r requirements-dev.lock`. Không dùng curl-pipe-shell hoặc pip global. Dữ liệu dependencies/artifacts không được commit. CMake/Ninja từ wheel có thể được dùng song song với gói apt; activate .venv để chọn đúng bản.

## Doctor

```bash
python scripts/doctor.py --profile wsl2 --json out/m01/doctor.json
```

Doctor kiểm tra host, tool versions/targets, cross tools cùng prefix, GCC sử dụng linker trong prefix, pytest và firmware đọc được. Exit khác 0 khi FAIL; i686 và KVM tùy chọn chỉ WARN. Doctor không cài phần mềm, không boot firmware và không dump secrets/environment. CI dùng profile ci thay vì buộc runner là WSL2.

## Build, test và tái lập

```bash
cmake --preset m01-debug
cmake --build --preset m01-debug
ctest --preset m01-debug --output-on-failure
cmake --preset m01-release
cmake --build --preset m01-release
ctest --preset m01-release --output-on-failure
python scripts/smoke.py --reproducible
```

Kích hoạt .venv trước các lệnh. Smoke runner chỉ định Python đang chạy vào CMake, build/CTest cả hai profiles và kiểm tra ELF. Output build/m01-debug và build/m01-release chứa m01-smoke.elf/map; logs và smoke.json ở out/m01. Không chạy ELF trên Linux hoặc trong QEMU.

Sample entry ở 1 MiB, chỉ cho kiểm chứng; không phải entry ABI kernel. C/C++ freestanding, không PIE, stack protector, red zone, exceptions, RTTI hoặc dynamic initialization. Debug -O0/-g; Release -O2/-g0, không LTO. NASM dùng source filename tương đối và --reproducible, không thêm DWARF riêng; Debug C/C++ vẫn có DWARF. Link static bằng cross GCC/GNU LD, không startup/default libraries, tắt build-id, yêu cầu noexecstack và lỗi linker warnings.

Verifier kiểm tra ELF64/x86_64/entry, không PT_INTERP/PT_DYNAMIC hoặc dynamic sections, không undefined symbols; compiler trong compile_commands phải đúng cross đã chọn và link map chỉ chứa ba object sample. Đây là kiểm tra sample, không phải ELF loader an toàn cho OS.

Repro runner sao chép bộ input sample vào hai source roots tạm khác nhau, build sạch từng profile bằng cùng toolchain, chuẩn hóa paths và SOURCE_DATE_EPOCH từ commit. So SHA-256 Debug với Debug và Release với Release; khác hash là FAIL. Chỉ tự dọn TemporaryDirectory do script tạo, giữ logs/hashes. Không yêu cầu Debug và Release có cùng hash.

## Bootstrap CI

[Manifest](../scripts/toolchain-lock.json) ghi URL GNU archives, SHA-256, options và fingerprints chữ ký đã xác minh với gpgv/GNU keyring khi chốt manifest ngày 2026-10-24. Source upstream: [GCC 12.2.0](https://ftp.gnu.org/gnu/gcc/gcc-12.2.0/) và [Binutils](https://ftp.gnu.org/gnu/binutils/). Bootstrap mỗi lần kiểm tra hash, không tải latest hoặc dùng compiler binary không rõ nguồn.

```bash
python scripts/bootstrap_cross.py --prefix out/m01/cross --jobs 4
export LAMIX_CROSS_ROOT="$PWD/out/m01/cross"
```

Build Binutils rồi GCC C/C++ và target libgcc; chưa build libstdc++. Sample không link libgcc. Prefix mới phải rỗng; chỉ tiếp tục pending receipt đúng manifest hoặc dùng cache có receipt đúng, rồi doctor xác minh công cụ. Không cài đè toolchain local hiện có hoặc cài vào /usr. CI cache key gồm runner/manifest/bootstrap script; không dùng restore key rộng.

Toolchain CI chạy trên ubuntu-22.04, Python 3.12; Documentation CI tiếp tục độc lập. Xem [TESTING](TESTING.md) và [handoff](SESSION_HANDOFF.md) cho bằng chứng thực tế. Boot/QEMU smoke chưa thuộc M01.
