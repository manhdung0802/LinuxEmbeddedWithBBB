# Bài 27 - DMA Engine Framework

## 1. Mục tiêu bài học
- Hiểu DMA Engine framework trong Linux kernel
- AM335x EDMA (Enhanced DMA) controller
- `dmaengine` API: `dma_request_chan()`, `dmaengine_prep_slave_sg()`
- Viết driver sử dụng DMA transfer

---

## 2. AM335x EDMA

### 2.1 EDMA Controller

| Thông số | Giá trị | Nguồn: spruh73q.pdf |
|----------|---------|---------------------|
| Base Address | 0x49000000 | Chapter 11 |
| DMA channels | 64 | Chapter 11 |
| PaRAM sets | 256 | Chapter 11 |
| Event queues | 2 (Q0, Q1) | Chapter 11 |
| Transfer types | Memory-to-memory, Peripheral-to-memory, Memory-to-peripheral | Chapter 11 |

### 2.2 Kiến trúc EDMA

```
┌─────────────────────────────────────────────────┐
│                  EDMA Controller                 │
│                                                  │
│  ┌──────────┐   ┌────────────┐   ┌───────────┐ │
│  │ Channel  │──►│ PaRAM Set  │──►│ Transfer  │ │
│  │ (Event)  │   │ (src,dst,  │   │ Controller│ │
│  │          │   │  cnt,link) │   │           │ │
│  └──────────┘   └────────────┘   └───────────┘ │
│                                       │         │
│  Event Queue 0 ◄──────────────────────┘         │
│  Event Queue 1                                  │
└─────────────────────────────────────────────────┘
        │                     │
        ▼                     ▼
   ┌─────────┐          ┌─────────┐
   │  Memory  │          │Peripheral│
   │  (DDR)   │          │(SPI,UART)│
   └─────────┘          └─────────┘
```

### 2.3 PaRAM Set

Mỗi DMA channel có một PaRAM set chứa thông tin transfer:

| Field | Mô tả |
|-------|-------|
| SRC | Source address |
| DST | Destination address |
| ACNT | Number of bytes per array element |
| BCNT | Number of arrays per frame |
| CCNT | Number of frames |
| SRCBIDX | Source B index (bytes between arrays) |
| DSTBIDX | Destination B index |
| LINK | Link to next PaRAM set |
| OPT | Options (interrupt, chaining, etc.) |

---

## 3. Linux DMA Engine Framework

### 3.1 Kiến trúc

```
┌─────────────────────────────────────────┐
│ DMA Client Driver (SPI, UART, ADC...)   │
│  dma_request_chan()                      │
│  dmaengine_prep_slave_sg()              │
│  dmaengine_submit()                     │
│  dma_async_issue_pending()              │
├─────────────────────────────────────────┤
│ DMA Engine Core (drivers/dma/dmaengine.c)│
├─────────────────────────────────────────┤
│ DMA Controller Driver (edma.c)          │
│  dma_device ops: device_prep_*          │
├─────────────────────────────────────────┤
│ Hardware (EDMA Controller)              │
└─────────────────────────────────────────┘
```

### 3.2 API chính

```c
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>

/* 1. Request DMA channel */
struct dma_chan *chan;
chan = dma_request_chan(&pdev->dev, "rx");  /* Tên từ DT */

/* 2. Cấu hình slave */
struct dma_slave_config cfg = {
    .direction = DMA_DEV_TO_MEM,
    .src_addr = phys_addr_of_peripheral_reg,
    .src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES,
    .src_maxburst = 16,
};
dmaengine_slave_config(chan, &cfg);

/* 3. Chuẩn bị transfer descriptor */
struct dma_async_tx_descriptor *desc;
desc = dmaengine_prep_slave_sg(chan, sg, nents, direction, flags);
desc->callback = my_dma_callback;
desc->callback_param = my_data;

/* 4. Submit */
dma_cookie_t cookie;
cookie = dmaengine_submit(desc);

/* 5. Issue pending - bắt đầu transfer */
dma_async_issue_pending(chan);

/* 6. Chờ hoàn thành (hoặc dùng callback) */
dma_sync_wait(chan, cookie);

/* 7. Release channel */
dma_release_channel(chan);
```

---

## 4. Memory-to-Memory DMA

```c
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/completion.h>

#define BUF_SIZE  4096

struct learn_dma {
	struct dma_chan *chan;
	void *src_buf;
	void *dst_buf;
	dma_addr_t src_dma;
	dma_addr_t dst_dma;
	struct completion done;
};

static void learn_dma_callback(void *param)
{
	struct learn_dma *dma = param;
	complete(&dma->done);
}

static int learn_dma_transfer(struct learn_dma *dma, struct device *dev)
{
	struct dma_async_tx_descriptor *desc;
	dma_cookie_t cookie;
	int ret;

	/* Fill source with test pattern */
	memset(dma->src_buf, 0xAA, BUF_SIZE);
	memset(dma->dst_buf, 0x00, BUF_SIZE);

	/* Prepare memcpy descriptor */
	desc = dmaengine_prep_dma_memcpy(dma->chan, dma->dst_dma,
	                                  dma->src_dma, BUF_SIZE,
	                                  DMA_PREP_INTERRUPT);
	if (!desc) {
		dev_err(dev, "Failed to prepare DMA\n");
		return -EIO;
	}

	desc->callback = learn_dma_callback;
	desc->callback_param = dma;

	init_completion(&dma->done);
	cookie = dmaengine_submit(desc);
	dma_async_issue_pending(dma->chan);

	/* Chờ DMA hoàn thành, timeout 5 giây */
	ret = wait_for_completion_timeout(&dma->done, 5 * HZ);
	if (ret == 0) {
		dev_err(dev, "DMA timeout!\n");
		dmaengine_terminate_sync(dma->chan);
		return -ETIMEDOUT;
	}

	/* Verify */
	if (memcmp(dma->src_buf, dma->dst_buf, BUF_SIZE) == 0)
		dev_info(dev, "DMA memcpy OK! %d bytes\n", BUF_SIZE);
	else
		dev_err(dev, "DMA memcpy FAILED!\n");

	return 0;
}

static int learn_dma_probe(struct platform_device *pdev)
{
	struct learn_dma *dma;
	int ret;

	dma = devm_kzalloc(&pdev->dev, sizeof(*dma), GFP_KERNEL);
	if (!dma)
		return -ENOMEM;

	/* Request DMA channel */
	dma->chan = dma_request_chan(&pdev->dev, "memcpy");
	if (IS_ERR(dma->chan)) {
		ret = PTR_ERR(dma->chan);
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Failed to get DMA channel: %d\n", ret);
		return ret;
	}

	/* Allocate DMA-able buffers */
	dma->src_buf = dma_alloc_coherent(&pdev->dev, BUF_SIZE,
	                                   &dma->src_dma, GFP_KERNEL);
	dma->dst_buf = dma_alloc_coherent(&pdev->dev, BUF_SIZE,
	                                   &dma->dst_dma, GFP_KERNEL);
	if (!dma->src_buf || !dma->dst_buf) {
		ret = -ENOMEM;
		goto err_free;
	}

	platform_set_drvdata(pdev, dma);

	/* Thực hiện DMA transfer thử */
	ret = learn_dma_transfer(dma, &pdev->dev);
	if (ret)
		goto err_free;

	return 0;

err_free:
	if (dma->src_buf)
		dma_free_coherent(&pdev->dev, BUF_SIZE, dma->src_buf, dma->src_dma);
	if (dma->dst_buf)
		dma_free_coherent(&pdev->dev, BUF_SIZE, dma->dst_buf, dma->dst_dma);
	dma_release_channel(dma->chan);
	return ret;
}

static int learn_dma_remove(struct platform_device *pdev)
{
	struct learn_dma *dma = platform_get_drvdata(pdev);

	dma_free_coherent(&pdev->dev, BUF_SIZE, dma->src_buf, dma->src_dma);
	dma_free_coherent(&pdev->dev, BUF_SIZE, dma->dst_buf, dma->dst_dma);
	dma_release_channel(dma->chan);
	return 0;
}

static const struct of_device_id learn_dma_of_match[] = {
	{ .compatible = "learn,dma-test" },
	{ },
};
MODULE_DEVICE_TABLE(of, learn_dma_of_match);

static struct platform_driver learn_dma_driver = {
	.probe = learn_dma_probe,
	.remove = learn_dma_remove,
	.driver = {
		.name = "learn-dma",
		.of_match_table = learn_dma_of_match,
	},
};

module_platform_driver(learn_dma_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DMA Engine learning driver");
```

---

## 5. DMA cho Peripheral (Slave SG)

```c
/* Ví dụ: DMA từ SPI RX register vào memory buffer */
struct dma_slave_config cfg = {
    .direction = DMA_DEV_TO_MEM,
    .src_addr = spi_phys_base + SPI_RX0,
    .src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES,
    .src_maxburst = 1,
    .dst_maxburst = 1,
};
dmaengine_slave_config(rx_chan, &cfg);

/* Scatterlist cho buffer */
struct scatterlist sg;
sg_init_one(&sg, rx_buf, len);
dma_map_sg(dev, &sg, 1, DMA_FROM_DEVICE);

desc = dmaengine_prep_slave_sg(rx_chan, &sg, 1,
    DMA_DEV_TO_MEM, DMA_PREP_INTERRUPT);
```

---

## 6. Device Tree cho DMA

```dts
my_device {
    compatible = "learn,dma-test";
    dmas = <&edma 0 0     /* TX: channel 0 */
            &edma 1 0>;   /* RX: channel 1 */
    dma-names = "tx", "rx";
};
```

---

## 7. Kiến thức cốt lõi

1. **DMA Engine**: abstract layer cho DMA controller (EDMA, SDMA, etc.)
2. **Workflow**: request_chan → slave_config → prep → submit → issue_pending → callback/wait
3. **`dma_alloc_coherent()`**: cấp phát DMA-able memory (uncached)
4. **Memcpy vs Slave**: memcpy = mem-to-mem, slave = peripheral-to-mem hoặc ngược lại
5. **EDMA PaRAM**: src, dst, count, link → hardware tự transfer
6. **Callback**: chạy trong interrupt context → không sleep

---

## 8. Câu hỏi kiểm tra

1. `dma_alloc_coherent()` khác gì `kmalloc()`?
2. Phân biệt `dmaengine_prep_dma_memcpy()` và `dmaengine_prep_slave_sg()`.
3. Tại sao phải gọi `dma_async_issue_pending()` sau `dmaengine_submit()`?
4. DMA callback chạy trong context gì? Có được gọi `mutex_lock()` trong đó không?
5. EDMA PaRAM set chứa những gì?

---

## 9. Chuẩn bị cho bài sau

Bài tiếp theo: **Bài 28 - Power Management**

Ta sẽ học Runtime PM, PM ops, suspend/resume, clock gating trên AM335x.
