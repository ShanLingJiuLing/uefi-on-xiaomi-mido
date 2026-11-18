# Redmi Note 4X（mido / MSM8953）UEFI 移植 —— 交接归档

> 归档日期：2026-11-17
> 目标设备：Xiaomi Redmi Note 4X 骁龙版（`mido`，MSM8953 / 骁龙 625，3GB + 32GB）
> 一句话状态：**UEFI 可以在真机上启动、显示正常、能进 Boot Menu 和 UEFI Shell、音量上键可用；
> 存储（eMMC/microSD）尚未打通，卡在数据传送。**

---

## 1. 当前状态

### 1.1 已经能用的

| 能力 | 状态 | 说明 |
|---|---|---|
| 从 lk2nd `fastboot boot` 加载 UEFI | ✅ | 不写任何分区，随时可回滚 |
| 显示 | ✅ | 1080×1920，24bpp RGB888，无错位 |
| Boot Menu / UEFI Shell | ✅ | `EDK II / UEFI v2.70 (Renegade Project)` |
| 音量上键 | ✅ | 短按下移、长按 ≥800ms 当回车 |
| PrePi → DXE → BDS | ✅ | 含 GIC、架构定时器、内存图 |
| SDCC1（eMMC）控制器 | ✅ | HC 模式、时钟、复位、`SdMmcPassThru` 全部工作 |
| SDCC2（microSD）控制器 | ✅ | 时钟树由本项目自己开启，控制器可正常访问 |
| eMMC 卡识别 | ✅ | CMD0/1/2/3/7/9/13/16 全部成功，卡进入 Tran 状态 |

### 1.2 还没打通的

| 项 | 状态 | 卡点（详见 `docs/02-移植技术文档.md` §13） |
|---|---|---|
| eMMC 数据读写 | ❌ | CMD17 超时；命令全通、数据全不通 |
| microSD 识别 | ❌ | SD 槽 VCC/VCCQ 由 RPM 供电，UEFI 无 RPM 驱动 |
| 文件系统 | ❌ | 依赖上面两项；另外 EDK2 原生只有 FAT，而本机分区是 ext2/ext4 |
| USB host | ❌ | 需 dwc3 驱动 |
| ACPI 表 | ❌ | 当前是无 DSDT 运行 |
| 音量下键 / 电源键 | ❌ | 在 PMIC PON 上，需要 SPMI |

---

## 2. 快速开始

### 2.1 硬件前提

- 手机已解锁 bootloader，`boot` 分区里已经刷了 **lk2nd**（本机版本 `21.0-next-c82940f3-20250825-dirty`）。
- 如果没有 lk2nd，需要先把它刷进 `boot` 分区（这会覆盖原厂内核，请先备份）。

### 2.2 构建环境

原项目是在**手机自己**（Debian 13 aarch64）上编译的，但任何 Linux 主机都可以：

```bash
sudo apt install -y git build-essential uuid-dev acpica-tools nasm \
     python3-dev pkg-config unzip bc device-tree-compiler curl gettext
```

拉取 EDK2 树（**不要** `--recurse-submodules`，会递归拉 openssl 极慢）：

```bash
git clone --depth 1 https://github.com/edk2-porting/edk2-msm
cd edk2-msm
git submodule update --init --depth 1 \
    Common/edk2 Common/edk2-platforms \
    GPLDrivers/Library/SimpleInit Platform/EFI_Binaries
git -C Common/edk2 submodule update --init --depth 1 \
    BaseTools/Source/C/BrotliCompress/brotli \
    MdeModulePkg/Library/BrotliCustomDecompressLib/brotli \
    CryptoPkg/Library/OpensslLib/openssl
git -C GPLDrivers/Library/SimpleInit submodule update --init --depth 1
```

### 2.3 把本归档的源码放进树里

`sources/` 目录**已经按 edk2-msm 的路径组织好**，直接把内容覆盖进去即可：

```bash
cp -r sources/* /path/to/edk2-msm/
```

对应关系：

| 归档里的路径 | 树里的路径 | 作用 |
|---|---|---|
| `configs/msm8953.conf` | `configs/msm8953.conf` | FD 基址/大小 |
| `configs/devices/mido.conf` | `configs/devices/mido.conf` | 设备名、SoC |
| `Platform/Qualcomm/msm8953/msm8953.dsc` | 同路径 | SoC 级 PCD、组件 |
| `Platform/Xiaomi/msm8953/mido.dsc` | 同路径 | 设备级：屏幕、组件 |
| `Platform/Xiaomi/msm8953/mido.fdf.inc` | 同路径 | 设备 DXE |
| `Platform/Xiaomi/msm8953/mido-startup.nsh` | 同路径 | Shell 自动执行脚本（探针） |
| `Platform/Xiaomi/msm8953/ButtonsDxe/` | 同路径 | 音量上键驱动 |
| `Platform/Xiaomi/msm8953/SdhcPlatformDxe/` | 同路径 | SDHCI 控制器注册 + 时钟 |
| `Platform/Xiaomi/msm8953/MmcProbe/` | 同路径 | 存储诊断应用 |
| `Platform/Xiaomi/msm8953/Library/mido/PlatformMemoryMapLib/` | 同路径 | **手写内存图（最关键）** |
| `Platform/Xiaomi/msm8953/FdtBlob_compat/mido.dtb` | 同路径 | 喂给 lk2nd 的 DTB |
| `Silicon/Qualcomm/msm8953/platform.sh.inc` | 同路径 | 打包钩子（boot.img 地址） |
| `Silicon/Qualcomm/msm8953/Library/PlatformPrePiLib/PlatformUtils.h` | 同路径 | 看门狗基址修正 |
| `BootShim.S` | `tools/BootShim/BootShim.S` | 跳板（含心跳） |
| `PrePi-Pi.c` | `Silicon/Qualcomm/QcomPkg/PrePi/Pi.c` | 带阶段标记的 PrePi |
| `SimpleFbDxe.c` | `Silicon/Qualcomm/QcomPkg/Drivers/SimpleFbDxe/SimpleFbDxe.c` | 24bpp 适配 |
| `Platform/Xiaomi/msm8953/FdtBlob_compat/mido.dtb` | 同路径 | 从 `dumps/mido-live.dtb` 复制而来 |

> 注意区分两个 `tools`：
> `sources/tools/` 是 **edk2 树里的** `tools/BootShim/`（随 `sources/*` 一起覆盖）；
> 归档顶层的 `tools/` 是本项目自用的 Python 辅助脚本，**不需要**放进 edk2 树。

此外还需要在 edk2 上游打几个补丁（**不要手打，用脚本**）：
`scripts/build-mido-all.sh` 里的 2 / 2b / 2c / 2d / 2e 段会自动给
`MdeModulePkg/Bus/Pci/SdMmcPciHcDxe` 打上「eMMC 常驻」「探针标记」等补丁。

### 2.4 构建

```bash
export USE_IASL=true
./build.sh -d mido -t GCC5 -r RELEASE -b
# 产物：boot-mido.img
```

**务必做全清构建**，否则会遇到「改了代码但镜像没变」：

```bash
rm -rf workspace/Build/mido
```

判据：每次构建后比较 `boot-mido.img` 解压后载荷的 md5，`tools/verify_img.py` 可用。

本归档的 `scripts/build-mido-all.sh` 就是把这些步骤（安装源码 → 打补丁 → 全清构建 → 打印指纹）固化好的脚本。

### 2.5 部署与运行

```powershell
.\fastboot.exe boot .\boot-mido.img
```

- **只进 RAM，不写任何分区**，断电即恢复原状，不会变砖。
- 进入 lk2nd fastboot 的方法：正常开机，**感觉到震动后立刻按住音量下**。

### 2.6 期望看到什么

上电后依次出现：lk2nd 显示 → 横幅 → Renegade logo → Boot Menu →（超时）UEFI Shell。
Shell 会自动执行 `startup.nsh`，含 `map -r`、`MmcProbe.efi` 和若干 `dmem` 诊断输出。

---

## 3. 硬件基线（实测，可直接引用）

| 项 | 值 |
|---|---|
| CPU | 8× Cortex-A53 r0p4，2.016 GHz，ARMv8.0，含 `crc32`，**无 LSE**（⇒ Win11 24H2+ 不可能） |
| DRAM | 3 GiB @ `0x40000000`–`0xFFFFFFFF` |
| 帧缓冲 | `0x90001000`，20 MiB，**24bpp RGB888，stride 3240（1080×3）** |
| 面板 | Novatek NT35532，1080×1920，DSI video mode |
| GIC | **GICv2**，Dist `0x0B000000`，CPU I/F `0x0B002000` |
| 架构定时器 | 19.2 MHz，INTID 20 |
| 看门狗 | `0x0B017000`（bark 11 s） |
| TLMM | `0x01000000`；`CFG(n)=base+n*0x1000`；`IN_OUT(n)=base+n*0x1000+4`，bit0=电平 |
| eMMC | SDCC1 ≈ `0x07824900`（SDHCI）/ `0x07824000`（vendor core） |
| microSD | SDCC2 `0x07864900`（SDHCI）/ `0x07864000`（vendor core） |
| GCC | `0x01800000`（SDCC1 组 `+0x42000`，SDCC2 组 `+0x43000`） |
| 音量上键 | TLMM **GPIO 85**，低有效，上拉 |
| 音量下键 / 电源 | PMIC PON2 / PON1（SPMI，UEFI 不可达） |
| UART | `0x078AF000`，IRQ 107，**未引出**（所以只能靠帧缓冲调试） |
| USB | DWC3 `0x07000000` |

**内存图中必须避开的保留区**：
`0x84A00000+0x1E00000`（TZ）、`0x86C00000+0x6A00000`（modem）、
`0x8D600000+0x1100000`（adsp）、`0x8E700000+0x700000`（wcnss）、
`0x90001000+0x13FF000`（帧缓冲，写通）、`0x9FF00000+0x100000`（pstore）。

---

## 4. 目录说明

```
README.md                  ← 本文件
docs/
  01-可行性取证.md         可行性分析 + 取证过程（为什么这条路可行）
  02-移植技术文档.md       启动链、封装、平台层、内存图、显示、按键、调试方法论、踩坑
  03-存储调查记录.md       存储子系统 30 轮实验的完整记录（含所有失败路径）
sources/                  本项目写的所有源码（按 edk2 树路径组织）
scripts/                  构建脚本 + 关键探针脚本
dumps/                    从真机采集的证据（DTB、DTB 反编译、iomem、dmesg、分区表…）
tools/                    辅助工具（boot.img 打包/解析、DTB 反汇编）
images/
  boot-mido-mk6.img       显示+按键可用的版本（最稳定的「能进 Shell」镜像）
  boot-mido-sd8.img       eMMC 识别全部成功、CMD17 超时的诊断版本
```

---

## 5. 已知问题与建议的下一步

按「投入产出比」排序。

### 5.1 【首选】照 lk2nd 自己写一个精简 eMMC BlockIo 驱动

EDK2 的 `SdMmcPciHcDxe` + `EmmcDxe` 组合在本平台的数据通路上卡住，
但 **lk2nd 的 `platform/msm_shared/mmc.c` + `sdhci.c` 在这台机器上是验证可用的**
（它就是从这颗 eMMC 读的 `boot.img`）。

建议：不要用 `SdMmcPassThru`，直接在 `SdhcPlatformDxe` 里操作 SDHCI 寄存器，
照抄 lk2nd 的序列（§13.2 已经把命令序列整理好了），自己实现
`EFI_BLOCK_IO_PROTOCOL` 的 `ReadBlocks`/`WriteBlocks`（CMD17/CMD18/CMD24/CMD25）。
缓冲区用 `AllocatePages` 分配，并**手工做 cache 维护**
（`WriteBackDataCacheRange` / `InvalidateDataCacheRange`），绕开 EDK2 的 DMA 抽象。

### 5.2 【独立课题】RPM 稳压器支持，解锁 microSD

SD 槽的 VCC/VCCQ 由 RPM（独立小核）通过 SMD/SPMI 控制。需要：

1. SMEM 通信（`0x08600000` 起，需解析 SMEM 分区表）
2. APCS IPC 中断（触发 RPM 处理请求）
3. RPM 资源请求协议（`rpm_smd_regulator` 的 voltage level 请求）

没有这一步，microSD 在 UEFI 下永远没有电。**注意**：这也会顺带解锁音量下键、
电源键、电池信息等一大批 PMIC 功能。

### 5.3 文件系统

EDK2 原生只有 FAT。本机的情况：

- eMMC：49 个分区，全是 ext4 / raw
- microSD：`p1 = ext2`（挂载在 `/boot`）、`p2 = ext4`（根）

三种选择：
1. 在 SD 卡上新建一个小的 **FAT 分区**放内核 + initrd（最省事，推荐）
2. 给 EDK2 加一个 **ext2 只读驱动**（`/boot` 正好是 ext2，足以加载内核）
3. 直接从已知 LBA 读内核（脆弱，不推荐）

### 5.4 其它可并行的方向

- **USB host（dwc3）**：`0x07000000` + PHY `0x07E000`/QSCRATCH；有了它可接键盘和 U 盘，
  调试便利性会大幅提升（现在只有音量上键可用）
- **ACPI 表**：DSDT/SSDT（GIC、GTDT、SBSA UART、SDHCI、XHCI、I2C/GPIO、电池、热区）
- 清理调试代码：`PrePi.c` 的 `M0..M18` 标记、`BootShim.S` 的心跳、`MmcProbe` 应用

---

## 6. 调试方法论（没有 UART 的机器上怎么做）

这三条是本项目最可复用的经验，详情见 `docs/02-移植技术文档.md` §8。

1. **三层可观测性**：BootShim 刷灰度心跳 → PrePi 打阶段标记 → DXE 异常处理器给
   带符号的调用栈。任何一层「完全没反应」都能立刻定位到区间。
2. **反汇编定位**：异常输出给 `基址+偏移`，用
   `aarch64-linux-gnu-objdump -d --start-address=... --stop-address=...` 反查指令。
   本项目靠它把 `X2=0xB` 反推成 `MmioWrite32(0x3+8, 7)`，锁定漏配的 PCD。
3. **不信任增量构建**：改头文件后 EDK2 未必重编，镜像会和上一版字节相同。
   **每次都要比载荷 md5**，并且优先全清构建。

补充判据：**`Synchronous external abort` + 合理地址 ⇒ 该外设时钟没开**；
`Translation fault` ⇒ 地址没映射。这是排查任何新外设的第一步。

---

## 7. 安全须知

- 本项目全程使用 `fastboot boot`（加载到 RAM），**没有写过任何分区**。
- `boot` 分区的布局：`0x00000` = lk2nd（512 KiB），`0x80000` = 原 Android boot.img。
  即使哪天真的 `fastboot flash boot`，lk2nd 那 512 KiB 也不受影响，随时可回滚。
- 已知会让 lk2nd fastboot 卡死的命令：`oem ramoops`，**不要用**（只能拔电池恢复）。
- 手机侧的 Debian（在 microSD 上）与 Android（在 eMMC 上）互不影响：
  lk2nd 优先从 SD 启动，拔掉 SD 卡就进 Android。

---

## 8. 参考仓库

| 仓库 | 用途 |
|---|---|
| `edk2-porting/edk2-msm` | 固件主树（含 edk2 / edk2-platforms / SimpleInit / EFI_Binaries 子模块） |
| `msm8916-mainline/lk2nd` | 设备实际引导器；源码当事实字典用 |
| `msm8953-mainline/lk2nd` | 设备上运行的 lk2nd 分支来源 |
| `msm8953-mainline/linux` | mido 的 mainline DTS（面板/触屏/按键/SD 供电依据） |
| `tianocore/edk2`、`tianocore/edk2-platforms` | EDK2 上游 |
| `BigfootACA/simple-init` | 图形启动菜单 |

---

## 9. 致谢与声明

本项目是在 lk2nd、Linux mainline、edk2-msm 三者既有成果之上的移植实验，
所有硬件知识来自公开源码与真机采集（见 `dumps/`）。
`docs/` 下的三份文档尽量做到「结论 + 证据 + 失败路径」三者齐全，
目的是让接手的人**不必重走已经走过的弯路**。
