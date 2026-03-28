# Bài 14 - DMA (EDMA3)

## Tóm tắt nhanh

- **EDMA CC base**: `0x49000000`
- **64 channels + 256 PaRAM sets**
- **PaRAM = 32 bytes**: opt, src, abcnt, dst, srcbidx, link, dstbidx, ccnt

## Công thức transfer size

```
Total bytes = A_count × B_count × C_count
```

## Linux DMA Engine workflow

```
dma_request_channel()
    → dmaengine_prep_dma_memcpy()
    → dmaengine_submit()
    → dma_async_issue_pending()
    → wait_for_completion()
    → dma_release_channel()
```

## File trong bài

- `bai_14_dma_edma.md` — Bài học chi tiết
- `examples/` — Kernel module DMA memcpy
- `exercises/` — Bài tập
- `solutions/` — Lời giải
