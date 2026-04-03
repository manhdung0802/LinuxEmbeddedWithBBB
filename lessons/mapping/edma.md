## 10. EDMA

### 10.1 EDMA3 Controller

| Module | Base Address | IRQ (CC0) | Channels | Dùng cho |
|--------|-------------|-----------|---------|---------|
| EDMA3CC0 | `0x49000000` | 12 (compl), 14 (err) | 64 channels | DMA engine học |
| EDMA3TC0 | `0x49800000` | - | Transfer controller 0 | |
| EDMA3TC1 | `0x49900000` | - | Transfer controller 1 | |
| EDMA3TC2 | `0x49A00000` | - | Transfer controller 2 | |

### 10.2 PaRAM Set Structure (32 bytes mỗi entry)

```c
struct edma_param {
    uint32_t opt;       /* Options: chain, IRQ, sync type */
    uint32_t src;       /* Source address */
    uint32_t a_b_cnt;   /* [31:16]=BCNT, [15:0]=ACNT (bytes) */
    uint32_t dst;       /* Destination address */
    uint32_t src_dst_bidx; /* [31:16]=DSTBIDX, [15:0]=SRCBIDX */
    uint32_t link_bcntrld; /* [31:16]=BCNTRLD, [15:0]=LINK */
    uint32_t src_dst_cidx; /* [31:16]=DSTCIDX, [15:0]=SRCCIDX */
    uint32_t ccnt;      /* [15:0]=CCNT */
};
```

**OPT bits quan trọng:**
```
Bit 20    (TCINTEN) : Transfer completion interrupt enable
Bit 17    (ITCINTEN): Intermediate completion IRQ
Bit 23-12 (TCC)     : Transfer completion code
Bit 2-0   (SAM/DAM) : 0=const, 1=incr
Bit 3     (SYNCDIM) : 0=A-sync, 1=AB-sync
```

> **Nguồn:** AM335x TRM `spruh73q.pdf`, Section 11 (EDMA3), trang ~1650

---
---

> **Xem thêm:** [device_tree_snippets.md](device_tree_snippets.md) | [kernel_api.md](kernel_api.md) | [pin_conflicts.md](pin_conflicts.md)


---

> **Xem them:** [device_tree_snippets.md](device_tree_snippets.md) | [kernel_api.md](kernel_api.md) | [pin_conflicts.md](pin_conflicts.md)

