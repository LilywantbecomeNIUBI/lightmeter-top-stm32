# LightMeter Top STM32

[中文](#中文) | [English](#english)

---

## 中文

### 项目介绍

**LightMeter Top STM32** 是一个基于 **STM32F103C8T6** 的相机测光顶模块项目。它通过 **VEML7700** 数字光照传感器读取环境照度，结合 ISO、快门速度和光圈档位计算曝光参数，并通过 **SSD1306 128x64 OLED** 显示测光结果。

项目支持 **快门优先 Tv** 和 **光圈优先 Av** 两种模式，使用 **EC11 旋转编码器** 调节参数，按键用于切换模式和 ISO。该项目适合相机热靴测光顶、胶片相机外置测光表、嵌入式课程设计或 STM32 外设综合实践。

### 项目特点

- 基于 STM32F103C8T6 主控
- 使用 VEML7700 数字光照传感器读取 Lux
- 使用 SSD1306 128x64 OLED 显示测光界面
- 支持快门优先 `Tv` 模式
- 支持光圈优先 `Av` 模式
- EC11 编码器旋转调节当前模式下的主参数
- EC11 按下切换 `Tv / Av`
- 独立按键循环切换 ISO
- ADC 读取电池电压
- 软件按键消抖
- 基于 STM32CubeMX + CMake 工程结构
- 适合 VS Code、STM32CubeCLT、STM32CubeIDE 开发环境

### 硬件组成

| 模块 | 型号 / 说明 |
|---|---|
| 主控 | STM32F103C8T6 |
| 光照传感器 | VEML7700 |
| 显示屏 | SSD1306 128x64 OLED，I2C 接口 |
| 输入器件 | EC11 旋转编码器 |
| 按键 | 独立按键，用于 ISO 切换 |
| 电池检测 | ADC 分压检测 |

### 引脚分配

| 功能 | STM32 引脚 | 说明 |
|---|---|---|
| I2C1 SCL | PB6 | OLED 与 VEML7700 共用 |
| I2C1 SDA | PB7 | OLED 与 VEML7700 共用 |
| EC11 A 相 | PA0 | TIM2 Encoder Mode |
| EC11 B 相 | PA1 | TIM2 Encoder Mode |
| EC11 按下 | PB0 | GPIO 输入，内部上拉 |
| ISO 按键 | PB1 | GPIO 输入，内部上拉 |
| 电池检测 | PA2 | ADC1_IN2，外部分压输入 |

> VEML7700 默认 I2C 地址为 `0x10`。OLED 常见地址为 `0x3C` 或 `0x3D`，如果屏幕不显示，可以先检查地址是否匹配。

### 操作说明

#### 模式切换

按下 EC11 编码器：

```text
Tv -> Av -> Tv
```

#### Tv 快门优先模式

在 `Tv` 模式下，旋转 EC11 调节快门速度，系统根据当前 Lux 和 ISO 自动推荐光圈。

支持快门档位：

```text
1, 2, 4, 8, 15, 30, 60, 125, 250, 500
```

显示含义：

```text
1s, 1/2, 1/4, 1/8, 1/15, 1/30, 1/60, 1/125, 1/250, 1/500
```

#### Av 光圈优先模式

在 `Av` 模式下，旋转 EC11 调节光圈，系统根据当前 Lux 和 ISO 自动推荐快门。

支持光圈档位：

```text
F3.5, F4, F4.7, F5.6, F8, F11, F16, F22, F32, F45
```

#### ISO 切换

按下独立按键 PB1：

```text
ISO 100 -> 125 -> 160 -> 200 -> 400 -> ISO 100
```

### OLED UI

当前 UI 采用单页分区布局，突出主参数：

```text
Tv      ISO100      87%
--------------------------------
             1/125
--------------------------------
              F5.6
--------------------------------
Lux:80              EV:9.3
```

- 顶部：模式、ISO、电池电量
- 中部大字：当前模式下的主参数
- 中部小字：推荐参数
- 底部：Lux 与 EV 辅助信息

### 曝光计算

项目使用照度 Lux 估算 EV，并结合 ISO 进行修正。基础曝光关系为：

```text
EV = log2(N^2 / t)
```

其中：

- `N` 为光圈值
- `t` 为曝光时间，单位为秒

在快门优先模式下，已知快门和 EV，反推推荐光圈；在光圈优先模式下，已知光圈和 EV，反推推荐快门。最终结果会匹配到预设标准档位。

### 工程结构

```text
lightmeter-top-stm32/
├── Core/
│   ├── Inc/
│   │   ├── main.h
│   │   ├── gpio.h
│   │   ├── i2c.h
│   │   ├── adc.h
│   │   ├── tim.h
│   │   ├── veml7700.h
│   │   └── ssd1306_i2c.h
│   └── Src/
│       ├── main.c
│       ├── gpio.c
│       ├── i2c.c
│       ├── adc.c
│       ├── tim.c
│       ├── veml7700.c
│       └── ssd1306_i2c.c
├── Drivers/
├── cmake/
├── CMakeLists.txt
└── yyy_project.ioc
```

### 构建与烧录

#### 使用 CMake 构建

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

#### 烧录方式

可以使用以下任意方式烧录：

- STM32CubeProgrammer
- STM32CubeIDE
- VS Code STM32 插件
- OpenOCD + ST-Link

### 常见问题

#### OLED 不显示

- 检查 OLED 地址是 `0x3C` 还是 `0x3D`
- 检查 PB6 / PB7 是否接反
- 检查 I2C 是否有上拉电阻
- 检查 OLED 驱动芯片是否为 SSD1306

#### Lux 数值异常

- 检查 VEML7700 地址是否为 `0x10`
- 检查传感器供电是否为 3.3V
- 检查 I2C 总线连接
- 遮住传感器时 Lux 应下降，强光照射时 Lux 应升高

#### EC11 方向相反

可以在软件中反转编码器加减逻辑，或交换 PA0 / PA1 接线。

#### 电池电压异常

项目默认电池检测为 1:2 分压：

```text
电池真实电压 = PA2 ADC 换算电压 × 2
```

如果未连接电池分压，电压显示可能会不稳定。

### 项目状态

当前项目已经完成基础功能验证：

- OLED 显示正常
- VEML7700 可读取 Lux
- EC11 旋转可调节参数
- PB0 / PB1 按键响应正常
- ADC 可读取电池电压
- 测光 UI 已优化为分区显示

### 后续优化方向

- 增加测光校准参数
- 增加低电量提示
- 增加自动休眠以降低功耗
- 增加外壳与相机热靴安装结构
- 增加更多 ISO 档位
- 增加反射光 / 入射光测量模式切换

---

## English

### Introduction

**LightMeter Top STM32** is a compact camera light meter module based on the **STM32F103C8T6** microcontroller. It reads ambient illumination through a **VEML7700** digital light sensor, calculates exposure parameters based on ISO, shutter speed, and aperture, and displays the result on a **SSD1306 128x64 OLED**.

The project supports both **shutter priority Tv** and **aperture priority Av** modes. An **EC11 rotary encoder** is used for parameter adjustment, while buttons are used for mode switching and ISO selection. This project is suitable for a camera hot-shoe light meter, an external meter for film cameras, embedded coursework, or STM32 peripheral practice.

### Features

- STM32F103C8T6 microcontroller
- VEML7700 digital ambient light sensor
- SSD1306 128x64 I2C OLED display
- Shutter priority `Tv` mode
- Aperture priority `Av` mode
- EC11 rotary encoder for parameter adjustment
- EC11 push button for `Tv / Av` mode switching
- Independent button for ISO switching
- ADC-based battery voltage measurement
- Software button debounce
- STM32CubeMX + CMake project structure
- Suitable for VS Code, STM32CubeCLT, and STM32CubeIDE

### Hardware

| Module | Model / Description |
|---|---|
| MCU | STM32F103C8T6 |
| Light sensor | VEML7700 |
| Display | SSD1306 128x64 OLED, I2C |
| Input | EC11 rotary encoder |
| Button | Independent ISO switch button |
| Battery measurement | ADC voltage divider |

### Pinout

| Function | STM32 Pin | Description |
|---|---|---|
| I2C1 SCL | PB6 | Shared by OLED and VEML7700 |
| I2C1 SDA | PB7 | Shared by OLED and VEML7700 |
| EC11 A | PA0 | TIM2 Encoder Mode |
| EC11 B | PA1 | TIM2 Encoder Mode |
| EC11 Push | PB0 | GPIO input with internal pull-up |
| ISO Button | PB1 | GPIO input with internal pull-up |
| Battery Sense | PA2 | ADC1_IN2 with external voltage divider |

> The default I2C address of VEML7700 is `0x10`. The OLED address is usually `0x3C` or `0x3D`.

### Usage

#### Mode Switching

Press the EC11 encoder:

```text
Tv -> Av -> Tv
```

#### Tv Shutter Priority Mode

In `Tv` mode, rotate the EC11 encoder to select the shutter speed. The system recommends an aperture based on Lux and ISO.

Supported shutter steps:

```text
1, 2, 4, 8, 15, 30, 60, 125, 250, 500
```

Display meaning:

```text
1s, 1/2, 1/4, 1/8, 1/15, 1/30, 1/60, 1/125, 1/250, 1/500
```

#### Av Aperture Priority Mode

In `Av` mode, rotate the EC11 encoder to select the aperture. The system recommends a shutter speed based on Lux and ISO.

Supported aperture steps:

```text
F3.5, F4, F4.7, F5.6, F8, F11, F16, F22, F32, F45
```

#### ISO Switching

Press the independent PB1 button:

```text
ISO 100 -> 125 -> 160 -> 200 -> 400 -> ISO 100
```

### OLED UI

The current UI uses a single-page layout with a clear information hierarchy:

```text
Tv      ISO100      87%
--------------------------------
             1/125
--------------------------------
              F5.6
--------------------------------
Lux:80              EV:9.3
```

- Top bar: mode, ISO, battery level
- Large middle area: primary parameter
- Secondary middle area: recommended parameter
- Bottom line: Lux and EV information

### Exposure Calculation

The project estimates EV from Lux and adjusts it according to ISO. The basic exposure relationship is:

```text
EV = log2(N^2 / t)
```

Where:

- `N` is the aperture value
- `t` is the exposure time in seconds

In shutter priority mode, the system calculates the recommended aperture from shutter speed and EV. In aperture priority mode, it calculates the recommended shutter speed from aperture and EV. The result is then matched to the nearest predefined standard step.

### Project Structure

```text
lightmeter-top-stm32/
├── Core/
│   ├── Inc/
│   │   ├── main.h
│   │   ├── gpio.h
│   │   ├── i2c.h
│   │   ├── adc.h
│   │   ├── tim.h
│   │   ├── veml7700.h
│   │   └── ssd1306_i2c.h
│   └── Src/
│       ├── main.c
│       ├── gpio.c
│       ├── i2c.c
│       ├── adc.c
│       ├── tim.c
│       ├── veml7700.c
│       └── ssd1306_i2c.c
├── Drivers/
├── cmake/
├── CMakeLists.txt
└── yyy_project.ioc
```

### Build and Flash

#### Build with CMake

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

#### Flashing

You can flash the firmware using one of the following tools:

- STM32CubeProgrammer
- STM32CubeIDE
- VS Code STM32 extension
- OpenOCD + ST-Link

### Troubleshooting

#### OLED does not display

- Check whether the OLED address is `0x3C` or `0x3D`
- Check whether PB6 / PB7 are swapped
- Check I2C pull-up resistors
- Check whether the OLED driver chip is SSD1306

#### Lux value is abnormal

- Check whether the VEML7700 address is `0x10`
- Check whether the sensor is powered by 3.3V
- Check I2C wiring
- Lux should decrease when the sensor is covered and increase under strong light

#### EC11 direction is reversed

Reverse the encoder logic in software, or swap PA0 and PA1 wiring.

#### Battery voltage is abnormal

The default battery measurement circuit uses a 1:2 voltage divider:

```text
Real battery voltage = PA2 ADC voltage × 2
```

If the voltage divider is not connected, the displayed battery voltage may be unstable.

### Project Status

Basic functions have been verified:

- OLED display works
- VEML7700 reads Lux correctly
- EC11 rotary input works
- PB0 / PB1 buttons work
- ADC battery voltage reading works
- OLED UI has been optimized

### Future Improvements

- Add light meter calibration offset
- Add low battery warning
- Add sleep mode for lower power consumption
- Design a camera hot-shoe enclosure
- Add more ISO steps
- Add reflected / incident light metering modes

---

## License

This project is for learning and prototyping purposes. You may modify and reuse the code according to your own project requirements.
