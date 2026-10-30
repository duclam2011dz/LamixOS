# Toolchain và build M01

## Phạm vi

Chỉ build sample NASM/C11/C++17 để kiểm chứng toolchain, không phải kernel, bootloader hoặc LBP. Host mục tiêu WSL2 Linux x86_64; CI là Linux x86_64 thường. Minimum Python 3.10, CMake 3.22, Ninja 1.10; reference cross GCC/G++ 12.2.0 và Binutils 2.39. Không cam kết ELF giống nhau giữa mọi host/compiler version.

## Chuẩn bị host

Dùng cross-toolchain đang có nếu doctor và sample đạt. Nếu đặt LAMIX_CROSS_ROOT, prefix phải chứa bin/x86_64-elf-gcc cùng G++ và Binutils; nếu không đặt, tìm x86_64-elf-gcc trong PATH. Không fallback GCC host.

APT chỉ cài gói thiếu khi có quyền: build-essential, bison, flex, libgmp-dev, libmpfr-dev, libmpc-dev, texinfo, xz-utils, nasm, cmake, ninja-build, python3-pip, python3-venv, qemu-system-x86, gdb, ovmf, seabios, dosfstools, mtools, clang, lld, clang-format, clang-tidy và cppcheck. Không cần cài lại công cụ đã có.

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

## M03 host và target contract

Host GCC/G++ chỉ dùng để chạy tests và shared library validation; không được dùng thay cross compiler cho target. M03 CMake độc lập tại tests/boot, không thay entry/linker script sample M01. `python scripts/check_boot_cross.py` dùng toolchain M01, compile static archive và kiểm tra undefined symbols; không yêu cầu i686 hoặc bootstrap lại toolchain.

Flags target M03 gồm freestanding/no builtin/no stack protector/no red-zone/general-registers-only/no PIE, C++ không exceptions/RTTI. Headers dùng stdint/stddef do compiler cung cấp; không link libc/libstdc++. [TESTING](TESTING.md) có lệnh Debug/Release/sanitizer/cross cụ thể.

## M05 C32 và kernel probe64

Suite độc lập tests/bios_stage2 dùng cross GCC hiện có -m32 -malign-double và GNU LD elf_i386, không i686 bootstrap/libgcc64. Helpers unsigned division có oracle host và vectors thực thi core32. Kernel probe dùng -m64 -mcmodel=kernel -mno-red-zone -mgeneral-regs-only; linker checks và no undefined/runtime vẫn bắt buộc. Gate artifacts gate-core.elf/gate-probe.elf chỉ compile/link, không đưa vào image. Kernel.elf thực tế là probe khác, có entry-state/page-walk checks; [TESTING](TESTING.md#bios-stage2-m05) có lệnh target/host/sanitizer. Không thay sample/repro inputs M01.

## M06 EFI application

`tests/uefi/build_uefi.py` dùng Clang `--target=x86_64-pc-win32-coff`, NASM `win64` và `lld-link` subsystem `efi_application`, `/nodefaultlib`, `/dynamicbase`, `/nxcompat`. Runtime C freestanding do dự án sở hữu; PE report ghi compiler/linker/NASM version, commands, source SHA-256, inspection và hash EFI output. `clang`/`lld` là gói host build-time; không thay GCC cross cho BIOS/kernel ELF. EFI firmware function pointers và entry dùng Microsoft x64 ABI; kernel entry chuyển theo System V LBP qua assembly trampoline. CMake/test runner nằm trong `tests/uefi`; test guest yêu cầu OVMF và QEMU.


## M07 image và boot parity

Generator [m07_image.py](../scripts/m07_image.py) nhận build reports M05/M06 đã PASS; không tự build hay cài dependencies. Cần `mkfs.fat` từ `dosfstools`, cùng `mmd`, `mcopy`, `mdir` từ `mtools`. Doctor chỉ kiểm tra hiện diện/version lệnh và không tự cài. Toolchain CI cài hai gói trên. Python dùng GUID ngẫu nhiên mặc định hoặc UUIDv5 ổn định theo `--seed`.

Image raw sparse theo profile 8 GiB. Chỉ ESP 256 MiB được format FAT32 và populate; System A/B và User Data để nguyên. Checker dùng GPT validator M03, `mtools` đọc file trong ESP và đối chiếu nội dung với artifact M05/M06. QEMU đọc image với snapshot; CODE firmware chỉ đọc, VARS riêng mỗi run. Image và firmware không được đưa lên CI artifact.

## M08 kernel ELF

`tests/m08/build_kernel.py` dùng GCC cross, GNU LD/Binutils và NASM đã có; không cài hoặc bootstrap dependency mới. Cờ freestanding loại runtime host, PIE, stack protector, unwind tables, red zone và SIMD; ELF report xác minh ET_EXEC x86_64, higher-half entry, `p_paddr=0`, segments W^X, không dynamic/interpreter/undefined symbols, compiler/linker/NASM versions, commands, source hashes, ELF inspection và SHA-256. Thư viện LBP M03 được build lại với cùng target flags.

`scripts/m07_image.py create/check --kernel-build build/m08` xác minh report/hash M08 trước khi dùng `kernel.elf`; nếu không truyền tùy chọn, vẫn dùng kernel M05 như M07 ban đầu. M08 CTest khởi chạy SeaBIOS và OVMF guest thật; report JSON lưu command/version/hash/status, serial/QMP paths và protected hash trước/sau. CI lưu reports, không lưu image hoặc firmware.

## C++ freestanding kernel M11

M11 dùng GCC/G++ cross 12.2.0, GNU LD/Binutils 2.39 và NASM như các profile kernel trước. Consumer C++17 được compile riêng với `-ffreestanding -fno-exceptions -fno-rtti -fno-use-cxa-atexit`, sau đó liên kết vào ELF kernel bằng cross LD; không link libstdc++ hoặc runtime C++. Build verifier từ chối undefined symbols và `.init_array`/`.ctors`. Đây chỉ là smoke consumer gọi C API heap; không định nghĩa global `new/delete`.

`tests/m11/build_kernel.py --build build/m11` ghi compiler/linker versions, commands, source hashes, ELF inspection và kernel SHA-256. Toolchain CI chạy `tests/m11` CTest, gồm native ASan/UBSan cùng guest BIOS/OVMF; kết quả phải được xác minh trên SHA nghiệm thu cụ thể. QEMU launch-only và build compile-only không thay thế guest boot.
