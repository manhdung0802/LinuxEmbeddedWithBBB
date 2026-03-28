# Bài 19 - Platform Driver: Tham Khảo Nhanh

## Cấu trúc platform_driver
```c
static struct platform_driver my_drv = {
    .probe  = my_probe,
    .remove = my_remove,
    .driver = {
        .name  = "my_driver",
        .owner = THIS_MODULE,
        .of_match_table = my_ids,
    },
};
module_platform_driver(my_drv);  // thay cho module_init/exit
```

## of_match_table (phải khớp DTS compatible)
```c
static const struct of_device_id my_ids[] = {
    { .compatible = "vendor,device-v1" },
    { .compatible = "vendor,device-v2" },
    { }  // sentinel
};
MODULE_DEVICE_TABLE(of, my_ids);
```

## Trong probe() — lấy resources từ DT
```c
// Lấy và map memory (reg property)
struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
void __iomem *base = devm_ioremap_resource(&pdev->dev, res);

// Lấy interrupt
int irq = platform_get_irq(pdev, 0);

// Đọc custom property
u32 val;
of_property_read_u32(pdev->dev.of_node, "my-prop", &val);

// Lưu private data
platform_set_drvdata(pdev, my_data);
```

## Trong remove() — lấy lại data
```c
struct my_data *data = platform_get_drvdata(pdev);
// devm_* resources tự động freed - không cần cleanup thủ công
```

## devm_* (auto-cleanup khi remove)
```c
devm_kzalloc(dev, size, GFP_KERNEL)
devm_ioremap_resource(dev, res)
devm_request_irq(dev, irq, handler, flags, name, data)
devm_clk_get(dev, id)
```

## References
- [bai_19_platform_driver.md](bai_19_platform_driver.md)
- spruh73q.pdf, Section 25.4 (GPIO registers)
