# Bài 14 - DMA (EDMA): Truyền Dữ Liệu Không Qua CPU

## 1. Mục tiêu bài học

- Hiểu tại sao cần DMA và lợi ích so với CPU-driven transfer
- Nắm kiến trúc EDMA3 trên AM335x: Channel, PaRAM, Event, Completion
- Hiểu cấu trúc PaRAM Set: src, dst, count, linking
- Dùng DMA từ Linux kernel qua DMA Engine API
- Ví dụ thực tế: DMA copy memory-to-memory

---

## 2. Tại Sao Cần DMA?

### Vấn đề với CPU-driven transfer

```
CPU copy 1MB từ SRC → DST:
  for (i = 0; i < 256*1024; i++)
      dst[i] = src[i];    ← CPU bận 100% trong suốt quá trình
```

**Hậu quả**:
- CPU không làm được việc gì khác
- Latency cao cho các task khác
- Đặc biệt tệ khi transfer giữa memory và peripheral (UART, SPI, ADC)

### Giải pháp: DMA

```
CPU setup DMA → DMA tự chạy → CPU làm việc khác → DMA interrupt khi xong
     ↓
  [DMA Controller]
     │   copy data
  SRC → DST   (không cần CPU)
```

---

## 3. EDMA3 trên AM335x

AM335x dùng **EDMA3** (Enhanced Direct Memory Access v3):

### 3.1 Thành phần chính

```
┌─────────────────────────────────────┐
│              EDMA3                  │
│  ┌──────────┐  ┌──────────────────┐ │
│  │  Channel │→ │   PaRAM Set      │ │
│  │ Controller│  │ (Parameter RAM)  │ │
│  └──────────┘  └──────────────────┘ │
│  ┌──────────┐  ┌──────────────────┐ │
│  │  Event   │  │  Transfer        │ │  
│  │ Queue    │  │  Completion      │ │
│  └──────────┘  └──────────────────┘ │
└─────────────────────────────────────┘
```

- **Channel**: 64 DMA channels + 8 QDMA channels
- **PaRAM**: 256 Parameter RAM sets (mỗi set = 1 transfer descriptor)
- **Event**: hardware event từ peripheral kích hoạt DMA
- **Completion**: interrupt hoặc polling khi transfer xong

### 3.2 Base Addresses

| Module | Base Address |
|--------|-------------|
| EDMA CC (Channel Controller) | `0x49000000` |
| EDMA TC0 (Transfer Controller 0) | `0x49800000` |
| EDMA TC1 | `0x49900000` |
| EDMA TC2 | `0x49A00000` |

> **Nguồn**: spruh73q.pdf, Table 2-2 (EDMA addresses)

### 3.3 PaRAM Set Structure

Mỗi PaRAM Set gồm 8 word (32 bytes):

```c
struct edma_param_set {
    uint32_t opt;     /* Transfer options (TCC, SYNCDIM, STATIC, interrupt) */
    uint32_t src;     /* Source address */
    uint32_t abcnt;   /* A-count [15:0] và B-count [31:16] */
    uint32_t dst;     /* Destination address */
    uint32_t srcbidx; /* Source B-index (stride giữa B-transfers) */
    uint32_t link;    /* Link PaRAM [15:0] và BCNT reload [31:16] */
    uint32_t dstbidx; /* Destination B-index */
    uint32_t ccnt;    /* C-count (số lần lặp toàn bộ) */
};
```

> **Nguồn**: spruh73q.pdf, Table 11-8 (PaRAM Set Entry Field Descriptions)

### 3.4 Three Transfer Dimensions

EDMA3 hỗ trợ 3 chiều transfer: A, B, C:

```
Tổng bytes = A_count × B_count × C_count
```

- **A-type sync**: mỗi event trigger 1 array (A_count bytes)
- **AB-type sync**: mỗi event trigger 1 frame (A×B bytes)

Đối với memory-to-memory: dùng A-count = total_bytes, B=C=1.

---

## 4. EDMA3 Register Map (một số quan trọng)

| Thanh ghi | Offset | Chức năng |
|-----------|--------|-----------|
| `EDMA_PID`         | `0x000` | Peripheral ID |
| `EDMA_CCCFG`       | `0x004` | CC Configuration |
| `EDMA_DCHMAP0..63` | `0x100..` | Channel mapping → PaRAM set |
| `EDMA_ESR`         | `0x1010` | Event Set Register (SW trigger) |
| `EDMA_ESRH`        | `0x1014` | Event Set Register High |
| `EDMA_EER`         | `0x1020` | Event Enable Register |
| `EDMA_EERH`        | `0x1024` | Event Enable High |
| `EDMA_IPR`         | `0x1068` | Interrupt Pending Register |
| `EDMA_IPRH`        | `0x106C` | Interrupt Pending High |
| `EDMA_ICR`         | `0x1070` | Interrupt Clear Register |
| `EDMA_OPT` (PaRAM0)| `0x4000` | PaRAM 0 OPT |

> **Nguồn**: spruh73q.pdf, Table 11-49 (EDMA3 CC Register Map)

---

## 5. DMA Engine API trong Linux Kernel

Thay vì thao tác EDMA registers trực tiếp, Linux cung cấp **DMA Engine API** trừu tượng hóa tất cả DMA controller:

```c
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>

/* 1. Lấy DMA channel */
struct dma_chan *chan = dma_request_chan(dev, "tx");

/* 2. Chuẩn bị descriptor cho memcpy */
struct dma_async_tx_descriptor *desc =
    dmaengine_prep_dma_memcpy(chan, dst_dma, src_dma, len, DMA_PREP_INTERRUPT);

/* 3. Set callback khi xong */
desc->callback = dma_complete_callback;
desc->callback_param = NULL;

/* 4. Submit và issue */
dma_cookie_t cookie = dmaengine_submit(desc);
dma_async_issue_pending(chan);

/* 5. Đợi hoàn tất (hoặc dùng callback) */
dma_sync_wait(chan, cookie);

/* 6. Trả channel */
dma_release_channel(chan);
```

---

## 6. Ví dụ: DMA Memcpy trong Kernel Module

```c
/*
 * edma_memcpy.c
 * Kernel module demo DMA memory-to-memory copy
 * Dùng DMA Engine API (không thao tác EDMA registers trực tiếp)
 *
 * Build: make -C /lib/modules/$(uname -r)/build M=$PWD modules
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include <linux/string.h>

#define BUF_SIZE    (64 * 1024)  /* 64KB */

static struct completion dma_done;

static void dma_callback(void *param)
{
    complete(&dma_done);
    printk(KERN_INFO "DMA memcpy hoàn tất!\n");
}

static int __init edma_demo_init(void)
{
    struct dma_chan *chan;
    dma_addr_t src_dma, dst_dma;
    void *src_buf, *dst_buf;
    struct dma_async_tx_descriptor *desc;
    dma_cookie_t cookie;
    int ret = 0;
    ktime_t t0, t1;

    /* Yêu cầu channel DMA memcpy */
    chan = dma_request_chan_by_mask(DMA_MEM_TO_MEM);
    if (IS_ERR(chan)) {
        /* Thử cách khác nếu không có */
        dma_cap_mask_t mask;
        dma_cap_zero(mask);
        dma_cap_set(DMA_MEMCPY, mask);
        chan = dma_request_channel(mask, NULL, NULL);
        if (!chan) {
            printk(KERN_ERR "Không lấy được DMA channel\n");
            return -ENODEV;
        }
    }
    printk(KERN_INFO "DMA channel: %s\n", dev_name(&chan->dev->device));

    /* Cấp phát source và destination buffers */
    src_buf = kmalloc(BUF_SIZE, GFP_KERNEL | GFP_DMA);
    dst_buf = kmalloc(BUF_SIZE, GFP_KERNEL | GFP_DMA);
    if (!src_buf || !dst_buf) {
        ret = -ENOMEM;
        goto free_buf;
    }

    /* Điền dữ liệu nguồn */
    memset(src_buf, 0xAB, BUF_SIZE);
    memset(dst_buf, 0x00, BUF_SIZE);

    /* Map sang DMA address */
    src_dma = dma_map_single(chan->device->dev, src_buf, BUF_SIZE, DMA_TO_DEVICE);
    dst_dma = dma_map_single(chan->device->dev, dst_buf, BUF_SIZE, DMA_FROM_DEVICE);

    /* Tạo descriptor */
    init_completion(&dma_done);
    desc = dmaengine_prep_dma_memcpy(chan, dst_dma, src_dma, BUF_SIZE, DMA_PREP_INTERRUPT);
    if (!desc) {
        printk(KERN_ERR "prep_dma_memcpy thất bại\n");
        ret = -ENOMEM;
        goto unmap;
    }
    desc->callback = dma_callback;
    desc->callback_param = NULL;

    /* Submit và issue */
    t0 = ktime_get();
    cookie = dmaengine_submit(desc);
    dma_async_issue_pending(chan);

    /* Đợi callback */
    wait_for_completion(&dma_done);
    t1 = ktime_get();

    printk(KERN_INFO "DMA copy %dKB xong trong %lldµs\n",
           BUF_SIZE / 1024, ktime_to_us(ktime_sub(t1, t0)));

    /* Verify */
    dma_unmap_single(chan->device->dev, src_dma, BUF_SIZE, DMA_TO_DEVICE);
    dma_unmap_single(chan->device->dev, dst_dma, BUF_SIZE, DMA_FROM_DEVICE);

    if (memcmp(src_buf, dst_buf, BUF_SIZE) == 0)
        printk(KERN_INFO "Verify: DMA copy ĐÚNG\n");
    else
        printk(KERN_ERR "Verify: DMA copy SAI!\n");

    goto free_buf;

unmap:
    dma_unmap_single(chan->device->dev, src_dma, BUF_SIZE, DMA_TO_DEVICE);
    dma_unmap_single(chan->device->dev, dst_dma, BUF_SIZE, DMA_FROM_DEVICE);
free_buf:
    kfree(src_buf);
    kfree(dst_buf);
    dma_release_channel(chan);
    return ret;
}

static void __exit edma_demo_exit(void)
{
    printk(KERN_INFO "EDMA demo module unloaded\n");
}

module_init(edma_demo_init);
module_exit(edma_demo_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("EDMA3 DMA Memcpy Demo");
```

---

## 7. Câu hỏi kiểm tra

1. Tại sao DMA tốt hơn CPU copy cho việc truyền dữ liệu lớn?
2. PaRAM Set gồm bao nhiêu field? Field nào chứa địa chỉ nguồn và đích?
3. EDMA3 "A-type sync" và "AB-type sync" khác nhau thế nào?
4. Trong Linux DMA Engine, `dmaengine_submit()` có bắt đầu transfer ngay không? Cần gọi thêm hàm gì?
5. Tại sao phải gọi `dma_map_single()` trước khi nạp descriptor?

---

## 8. Tài liệu tham khảo

| Nội dung | Tài liệu | Section |
|----------|----------|---------|
| EDMA3 overview | spruh73q.pdf | Chapter 11, Section 11.1 |
| PaRAM Set structure | spruh73q.pdf | Table 11-8 |
| EDMA3 CC registers | spruh73q.pdf | Table 11-49 |
| EDMA3 base addresses | spruh73q.pdf | Table 2-2 |
| Linux DMA Engine API | linux/Documentation/driver-api/dmaengine/ | — |
