# Bài 17 - Input Subsystem

## 1. Mục tiêu bài học
- Hiểu input subsystem trong Linux kernel
- Đăng ký input_dev cho button/keyboard/touchscreen
- Dùng `input_report_key()`, `input_report_abs()`, `input_sync()`
- Viết button driver với GPIO interrupt
- Implement debounce bằng kernel timer

---

## 2. Input Subsystem Architecture

```
┌──────────────────────────────────────────┐
│  Userspace                                │
│  /dev/input/event0   evtest   libevdev    │
└───────────────────┬──────────────────────┘
                    │
┌───────────────────┴──────────────────────┐
│  Input Core (drivers/input/input.c)       │
│  - Dispatch events                        │
│  - /dev/input/eventN management           │
└───────────────────┬──────────────────────┘
                    │
┌───────────────────┴──────────────────────┐
│  Input Driver (your driver)               │
│  - input_allocate_device()                │
│  - input_register_device()                │
│  - input_report_key()                     │
└──────────────────────────────────────────┘
```

Input subsystem cung cấp:
- Giao diện thống nhất cho tất cả input device
- Event-based: `EV_KEY`, `EV_ABS`, `EV_REL`, ...
- Tự tạo `/dev/input/eventN`

---

## 3. Đăng ký Input Device

```c
#include <linux/input.h>

struct input_dev *input;

/* Allocate */
input = devm_input_allocate_device(&pdev->dev);
if (!input)
    return -ENOMEM;

/* Cấu hình */
input->name = "BBB Button";
input->phys = "bbb-button/input0";
input->id.bustype = BUS_HOST;

/* Khai báo loại event */
set_bit(EV_KEY, input->evbit);        /* Key events */
set_bit(KEY_ENTER, input->keybit);    /* Phím Enter */

/* Đăng ký */
ret = input_register_device(input);
if (ret)
    return ret;
```

---

## 4. Report Events

```c
/* Button pressed */
input_report_key(input, KEY_ENTER, 1);  /* 1 = pressed */
input_sync(input);                       /* Kết thúc 1 event */

/* Button released */
input_report_key(input, KEY_ENTER, 0);  /* 0 = released */
input_sync(input);

/* QUAN TRỌNG: input_sync() phải gọi sau mỗi nhóm event */
```

### 4.1 Event types:

| Type | Macro | Ví dụ |
|------|-------|-------|
| Key/button | `EV_KEY` | KEY_ENTER, BTN_LEFT |
| Relative | `EV_REL` | REL_X, REL_Y (mouse) |
| Absolute | `EV_ABS` | ABS_X, ABS_Y (touchscreen) |
| Switch | `EV_SW` | SW_LID |

---

## 5. Button Driver hoàn chỉnh

```c
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

#define DEBOUNCE_MS 50

struct bbb_button {
	struct input_dev *input;
	struct gpio_desc *gpio;
	int irq;
	struct timer_list debounce_timer;
	int keycode;
};

/* Timer callback cho debounce */
static void debounce_timer_fn(struct timer_list *t)
{
	struct bbb_button *btn = from_timer(btn, t, debounce_timer);
	int val;

	val = gpiod_get_value(btn->gpio);
	input_report_key(btn->input, btn->keycode, val);
	input_sync(btn->input);
}

/* IRQ handler — chỉ restart timer */
static irqreturn_t button_irq_handler(int irq, void *dev_id)
{
	struct bbb_button *btn = dev_id;

	mod_timer(&btn->debounce_timer,
	          jiffies + msecs_to_jiffies(DEBOUNCE_MS));

	return IRQ_HANDLED;
}

static int bbb_button_probe(struct platform_device *pdev)
{
	struct bbb_button *btn;
	int ret;

	btn = devm_kzalloc(&pdev->dev, sizeof(*btn), GFP_KERNEL);
	if (!btn)
		return -ENOMEM;

	/* GPIO */
	btn->gpio = devm_gpiod_get(&pdev->dev, "button", GPIOD_IN);
	if (IS_ERR(btn->gpio))
		return PTR_ERR(btn->gpio);

	/* IRQ từ GPIO */
	btn->irq = gpiod_to_irq(btn->gpio);
	if (btn->irq < 0)
		return btn->irq;

	/* Keycode từ DT */
	of_property_read_u32(pdev->dev.of_node, "linux,code", &btn->keycode);
	if (!btn->keycode)
		btn->keycode = KEY_ENTER;

	/* Input device */
	btn->input = devm_input_allocate_device(&pdev->dev);
	if (!btn->input)
		return -ENOMEM;

	btn->input->name = "BBB GPIO Button";
	btn->input->phys = "bbb-button/input0";
	btn->input->id.bustype = BUS_HOST;
	btn->input->dev.parent = &pdev->dev;

	set_bit(EV_KEY, btn->input->evbit);
	set_bit(btn->keycode, btn->input->keybit);

	/* Debounce timer */
	timer_setup(&btn->debounce_timer, debounce_timer_fn, 0);

	/* IRQ */
	ret = devm_request_irq(&pdev->dev, btn->irq,
	                        button_irq_handler,
	                        IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
	                        "bbb-button", btn);
	if (ret)
		return ret;

	/* Register input */
	ret = input_register_device(btn->input);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, btn);
	dev_info(&pdev->dev, "Button registered, keycode=%d\n", btn->keycode);
	return 0;
}

static int bbb_button_remove(struct platform_device *pdev)
{
	struct bbb_button *btn = platform_get_drvdata(pdev);
	del_timer_sync(&btn->debounce_timer);
	return 0;
}

static const struct of_device_id bbb_button_match[] = {
	{ .compatible = "bbb,gpio-button" },
	{ },
};
MODULE_DEVICE_TABLE(of, bbb_button_match);

static struct platform_driver bbb_button_driver = {
	.probe = bbb_button_probe,
	.remove = bbb_button_remove,
	.driver = {
		.name = "bbb-button",
		.of_match_table = bbb_button_match,
	},
};

module_platform_driver(bbb_button_driver);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BBB GPIO Button with debounce (Input subsystem)");
```

### Device Tree:

```dts
bbb_button {
    compatible = "bbb,gpio-button";
    button-gpios = <&gpio2 8 GPIO_ACTIVE_LOW>;
    linux,code = <28>;  /* KEY_ENTER */
    pinctrl-names = "default";
    pinctrl-0 = <&button_pin>;
};
```

---

## 6. Debounce

### 6.1 Vấn đề:

Button cơ học tạo "bounce" — nhiều signal thay đổi trong vài ms:

```
Thực tế khi nhấn nút:
    ─────┐   ┌┐ ┌┐ ┌──────
         └───┘└─┘└─┘
         ←bounce→
         (~1-10ms)
```

### 6.2 Giải pháp: Timer debounce

```
IRQ → restart timer (50ms)
      ↓
Timer expires → đọc GPIO → report event
```

Nếu bounce gây nhiều IRQ liên tiếp, timer chỉ expire **một lần** sau khi signal ổn định.

---

## 7. Test input device

```bash
# Cài evtest
apt install evtest

# Liệt kê input devices
cat /proc/bus/input/devices

# Test event
evtest /dev/input/event0
# Nhấn nút → thấy EV_KEY KEY_ENTER 1/0
```

---

## 8. Kiến thức cốt lõi sau bài này

1. **Input subsystem**: `input_allocate_device` → `set_bit` → `input_register_device`
2. **Report**: `input_report_key(input, code, value)` + `input_sync(input)`
3. **Debounce**: Dùng kernel timer, IRQ chỉ restart timer
4. **DT binding**: `linux,code` cho keycode, `button-gpios` cho GPIO
5. Userspace đọc event qua `/dev/input/eventN`

---

## 9. Câu hỏi kiểm tra

1. `input_sync()` dùng để làm gì? Khi nào gọi?
2. Debounce bằng timer hoạt động ra sao?
3. Sự khác nhau giữa `EV_KEY` và `EV_ABS`?
4. Viết DT node cho 3 button, mỗi button keycode khác nhau.
5. Làm sao biết input device đã được đăng ký thành công?

---

## 10. Chuẩn bị cho bài sau

Bài tiếp theo: **Bài 18 - LED Subsystem**

Ta sẽ học LED class driver: `led_classdev`, LED triggers, viết driver cho onboard LEDs BBB.
