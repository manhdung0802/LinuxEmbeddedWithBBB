# Bài 9 - Managed Resources (devm)

## 1. M?c tiêu bài h?c
- Hi?u v?n d? resource leak trong driver
- N?m nguyên t?c ho?t d?ng c?a `devm_*` (device-managed) API
- S? d?ng các devm API ph? bi?n: devm_kzalloc, devm_ioremap, devm_request_irq
- So sánh code có/không dùng devm
- Hi?u resource lifecycle và th? t? cleanup

---

## 2. V?n d?: Resource Leak

### 2.1 Code không dùng devm (error-prone):

```c
static int my_probe(struct platform_device *pdev)
{
    struct my_dev *dev;
    int ret;

    dev = kzalloc(sizeof(*dev), GFP_KERNEL);        /* Bu?c 1 */
    if (!dev) return -ENOMEM;

    dev->base = ioremap(0x4804C000, 0x1000);         /* Bu?c 2 */
    if (!dev->base) { ret = -ENOMEM; goto err_alloc; }

    ret = request_irq(98, my_handler, 0, "my", dev);  /* Bu?c 3 */
    if (ret) goto err_ioremap;

    dev->clk = clk_get(&pdev->dev, NULL);             /* Bu?c 4 */
    if (IS_ERR(dev->clk)) { ret = PTR_ERR(dev->clk); goto err_irq; }

    return 0;

err_irq:     free_irq(98, dev);
err_ioremap: iounmap(dev->base);
err_alloc:   kfree(dev);
    return ret;
}

static int my_remove(struct platform_device *pdev)
{
    /* Ph?i cleanup ÐÚNG TH? T?, ÐÚNG S? LU?NG */
    clk_put(dev->clk);
    free_irq(98, dev);
    iounmap(dev->base);
    kfree(dev);
    return 0;
}
```

**V?n d?**: D? quên cleanup, sai th? t?, khó maintain.

### 2.2 Cùng driver dùng devm:

```c
static int my_probe(struct platform_device *pdev)
{
    struct my_dev *dev;

    dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
    if (!dev) return -ENOMEM;

    dev->base = devm_ioremap(&pdev->dev, 0x4804C000, 0x1000);
    if (!dev->base) return -ENOMEM;

    ret = devm_request_irq(&pdev->dev, 98, my_handler, 0, "my", dev);
    if (ret) return ret;

    dev->clk = devm_clk_get(&pdev->dev, NULL);
    if (IS_ERR(dev->clk)) return PTR_ERR(dev->clk);

    return 0;
    /* KHÔNG C?N goto cleanup! */
}

static int my_remove(struct platform_device *pdev)
{
    /* KHÔNG C?N cleanup th? công! Kernel t? làm */
    return 0;
}
```

---

## 3. devm_* ho?t d?ng nhu th? nào?

### 3.1 Nguyên t?c:

```
probe() thành công:
    devm resource g?n vào struct device
    ?
Khi module remove ho?c probe fail:
    Kernel t? d?ng gi?i phóng T?T C? devm resource
    theo TH? T? NGU?C (LIFO - Last In, First Out)
```

### 3.2 Co ch? bên trong:

```c
/* Khi g?i devm_kzalloc(&pdev->dev, size, GFP_KERNEL): */
1. Kernel g?i kzalloc(size)
2. Ghi nh?n: "resource này thu?c v? pdev->dev"
3. Thêm vào danh sách resource c?a device

/* Khi device b? remove: */
1. Kernel duy?t danh sách resource (LIFO)
2. G?i hàm cleanup tuong ?ng (kfree, iounmap, free_irq, ...)
3. Xong — driver không c?n t? làm gì
```

### 3.3 LIFO order:

```c
devm_kzalloc(...)       /* Alloc 1st ? free last */
devm_ioremap(...)       /* Alloc 2nd ? free 2nd-to-last */
devm_request_irq(...)   /* Alloc 3rd ? free first */

/* Remove: free ngu?c = IRQ ? iounmap ? kfree */
```

---

## 4. Các devm API ph? bi?n

### 4.1 Memory:

```c
/* Allocate zeroed memory */
void *devm_kzalloc(struct device *dev, size_t size, gfp_t gfp);

/* Allocate memory */
void *devm_kmalloc(struct device *dev, size_t size, gfp_t gfp);

/* Dùng thay cho kzalloc/kmalloc */
```

### 4.2 I/O Memory:

```c
/* ioremap */
void __iomem *devm_ioremap(struct device *dev,
                            resource_size_t offset, unsigned long size);

/* ioremap + request_mem_region (combo) */
void __iomem *devm_ioremap_resource(struct device *dev,
                                     struct resource *res);
/* Khuy?n ngh? dùng devm_ioremap_resource thay vì devm_ioremap */
```

### 4.3 IRQ:

```c
int devm_request_irq(struct device *dev, unsigned int irq,
                      irq_handler_t handler, unsigned long irqflags,
                      const char *devname, void *dev_id);

/* Threaded IRQ */
int devm_request_threaded_irq(struct device *dev, unsigned int irq,
                               irq_handler_t handler,
                               irq_handler_t thread_fn,
                               unsigned long irqflags,
                               const char *devname, void *dev_id);
```

### 4.4 Clock:

```c
struct clk *devm_clk_get(struct device *dev, const char *id);
```

### 4.5 GPIO:

```c
struct gpio_desc *devm_gpiod_get(struct device *dev,
                                  const char *con_id,
                                  enum gpiod_flags flags);
```

### 4.6 Regulator:

```c
struct regulator *devm_regulator_get(struct device *dev, const char *id);
```

---

## 5. Khi nào KHÔNG nên dùng devm?

### 5.1 Resource c?n gi?i phóng s?m:

```c
/* Ví d?: buffer t?m ch? c?n trong probe */
buf = kzalloc(4096, GFP_KERNEL);
/* ... dùng buf ... */
kfree(buf);   /* Free ngay, không c?n devm */
```

### 5.2 Resource có lifecycle ph?c t?p:

```c
/* Resource du?c alloc/free nhi?u l?n trong runtime */
/* ví d?: DMA buffer m?i l?n transfer */
```

### 5.3 Quy t?c:

| Tình hu?ng | Dùng devm? |
|-----------|-----------|
| Resource s?ng t? probe t?i remove | ? Có |
| Resource t?m th?i trong m?t function | ? Không |
| Resource alloc/free nhi?u l?n | ? Không |
| IRQ c?n disable tru?c khi cleanup | ?? C?n th?n |

---

## 6. devm_ioremap_resource - Best practice

```c
static int my_probe(struct platform_device *pdev)
{
    struct resource *res;
    void __iomem *base;

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

    /* devm_ioremap_resource = request_mem_region + ioremap + devm */
    base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(base)) {
        dev_err(&pdev->dev, "Failed to map registers\n");
        return PTR_ERR(base);
    }

    /* base s?n sàng dùng, t? cleanup khi remove */
}
```

---

## 7. Ví d? hoàn ch?nh: Platform driver dùng devm

```c
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/clk.h>

struct my_gpio_dev {
	void __iomem *base;
	struct clk *clk;
	int irq;
};

static irqreturn_t my_irq_handler(int irq, void *dev_id)
{
	struct my_gpio_dev *gdev = dev_id;
	/* X? lý interrupt */
	pr_info("IRQ %d fired\n", irq);
	return IRQ_HANDLED;
}

static int my_gpio_probe(struct platform_device *pdev)
{
	struct my_gpio_dev *gdev;
	struct resource *res;

	/* 1. Allocate device struct — t? free khi remove */
	gdev = devm_kzalloc(&pdev->dev, sizeof(*gdev), GFP_KERNEL);
	if (!gdev)
		return -ENOMEM;

	/* 2. Map registers — t? iounmap khi remove */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	gdev->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(gdev->base))
		return PTR_ERR(gdev->base);

	/* 3. Get clock — t? put khi remove */
	gdev->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(gdev->clk))
		return PTR_ERR(gdev->clk);

	clk_prepare_enable(gdev->clk);

	/* 4. Request IRQ — t? free khi remove */
	gdev->irq = platform_get_irq(pdev, 0);
	if (gdev->irq < 0)
		return gdev->irq;

	if (devm_request_irq(&pdev->dev, gdev->irq, my_irq_handler,
	                      0, "my-gpio", gdev))
		return -ENODEV;

	platform_set_drvdata(pdev, gdev);
	dev_info(&pdev->dev, "Probed, base=%p irq=%d\n",
	         gdev->base, gdev->irq);
	return 0;
}

static int my_gpio_remove(struct platform_device *pdev)
{
	struct my_gpio_dev *gdev = platform_get_drvdata(pdev);

	/* Ch? c?n disable clock, devm lo ph?n còn l?i */
	clk_disable_unprepare(gdev->clk);

	dev_info(&pdev->dev, "Removed\n");
	return 0;
}

static const struct of_device_id my_gpio_of_match[] = {
	{ .compatible = "my,gpio-ctrl" },
	{ },
};
MODULE_DEVICE_TABLE(of, my_gpio_of_match);

static struct platform_driver my_gpio_driver = {
	.probe  = my_gpio_probe,
	.remove = my_gpio_remove,
	.driver = {
		.name = "my-gpio-ctrl",
		.of_match_table = my_gpio_of_match,
	},
};

module_platform_driver(my_gpio_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GPIO Platform Driver using devm");
```

---

## 8. Ki?n th?c c?t lõi sau bài này

1. **`devm_*`** = device-managed resource, kernel t? cleanup khi remove/probe-fail
2. **LIFO order**: resource alloc sau ? free tru?c
3. **`devm_ioremap_resource()`** = best practice cho MMIO mapping
4. **`devm_kzalloc()`** thay cho `kzalloc()` + manual `kfree()`
5. **Không c?n goto cleanup** khi probe fail n?u toàn b? dùng devm
6. **Không dùng devm** cho resource t?m th?i ho?c lifecycle ph?c t?p

---

## 9. Câu h?i ki?m tra

1. devm cleanup theo th? t? nào? T?i sao?
2. N?u probe() fail gi?a ch?ng, devm resource dã alloc có b? leak không?
3. So sánh `devm_ioremap()` và `devm_ioremap_resource()`.
4. Khi nào KHÔNG nên dùng devm_kzalloc()?
5. Vi?t probe() dùng devm cho device có 2 vùng reg và 1 IRQ.

---

## 10. Chu?n b? cho bài sau

Bài ti?p theo: **Bài 11 - Concurrency trong Kernel**

Ta s? h?c cách b?o v? shared data trong kernel: mutex, spinlock, atomic, completion, wait queue.
