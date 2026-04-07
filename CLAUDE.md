# LinuxEmbeddedWithBBB — Hướng dẫn cho Claude

> Bắt đầu với **README.md** để nắm tổng quan dự án, sau đó xem
> `BBB_docs/hw-docs/index.md` để tra cứu tài liệu phần cứng BeagleBone Black.

## Cấu trúc thư mục

```
.
├── BBB_docs/
│   ├── hw-docs/                # Tài liệu phần cứng BBB (xem index.md bên trong)
│   ├── datasheets/             # Datasheets PDF (AM335x TRM, v.v.)
│   └── schematics/             # Schematic gốc
├── lessons/                    # Học liệu, ghi chú từng bài
├── notes/                      # Ghi chú bổ sung
├── scripts/                    # Scripts tiện ích
├── README.md                   # Tổng quan kiến thức AM335x & Embedded Linux
└── CLAUDE.md                   # File này
```

## Target board

- Board: **BeagleBone Black**
- IP: `192.168.137.2`
- SSH/SCP: `ssh debian@192.168.137.2` / `scp <file> debian@192.168.137.2:/home/debian/`
- Thư mục làm việc trên target: `/home/debian`

## Coding convention

Source code C tuân theo **Linux kernel coding style**:
- Indent bằng tab, tab width = 8
- Mở ngoặc nhọn `{` cùng dòng với statement (trừ function definition)
- Tên biến, hàm: `snake_case`, viết thường
- Tên macro, constant: `UPPER_CASE`
- Dòng không quá 80 ký tự (linh hoạt đến 100 nếu cần)
- Không dùng `typedef` che giấu struct/pointer trừ khi có lý do rõ ràng
- Comment dùng `/* */` cho block, `//` cho inline ngắn

Tham khảo: https://www.kernel.org/doc/html/latest/process/coding-style.html

## Build

- **"compile"** = dùng `gcc` của host (chạy trên máy host)
- **"cross compile"** = dùng toolchain BBB (chạy trên board)

BBB toolchain: `<đường dẫn đến toolchain>/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf`

```bash
# Compile cho host
gcc -g -O0 -Wall -Wextra -std=c99 -o <output> <source>.c

# Cross compile cho BBB
<toolchain_path>/bin/arm-none-linux-gnueabihf-gcc -g -O0 -Wall -Wextra -std=c99 -o <output> <source>.c

# Compile với Address Sanitizer (host)
gcc -g -O1 -fsanitize=address -fno-omit-frame-pointer -o <output> <source>.c

# Kiểm tra memory leak (host)
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./<program>
```

## Tài liệu tham khảo

> **Quy tắc ưu tiên**: Khi cần thông tin về BeagleBone Black, SoC AM335x, hoặc Linux programming,
> **luôn tìm trong repo trước** (hw-docs, BBB_docs). Tài liệu trong repo đã được
> kiểm chứng và chuẩn hơn nguồn trên mạng. Chỉ dùng kiến thức bên ngoài khi repo không cover.

### Hardware docs (BeagleBone Black)

Khi cần tra cứu về **phần cứng BBB** (register, pin mux, GPIO, timer, clock, boot process, schematic, device tree), đọc:

```
BBB_docs/hw-docs/index.md
```

File index mô tả mục đích từng tài liệu và workflow tra cứu. Đọc index trước, sau đó đọc file cụ thể.

## Ngôn ngữ

- Commit message: ưu tiên **tiếng Việt**
- Ghi chú markdown: tiếng Việt xen tiếng Anh (thuật ngữ kỹ thuật giữ nguyên tiếng Anh)
- Code comment: tiếng Anh
