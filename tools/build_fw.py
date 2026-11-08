# -*- coding: utf-8 -*-
"""用 Keil 自带的 armclang(AC6) 手工构建这个 AC5 工程，产出 hex。
   源目录带中文，先镜像到 ASCII 目录再编译，避免命令行编码问题。"""
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "src")
BUILD = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "build")
# 第 3 个参数：额外的宏定义，逗号分隔，例如 "SHOW_ON_OLED=1"
EXTRA_DEFS = [d.strip() for d in (sys.argv[3].split(",") if len(sys.argv) > 3 and sys.argv[3] else [])]
# 第 4 个参数：产物名（不含扩展名），默认 fw
OUT_NAME = sys.argv[4] if len(sys.argv) > 4 else "fw"
SRCD = os.path.join(BUILD, "src")
OBJ = os.path.join(BUILD, "obj")
def _find_bin():
    """自动找 armclang 所在目录：环境变量 > 常见安装路径 > 全盘浅搜。"""
    cands = []
    env = os.environ.get("KEIL_ARMCLANG_BIN")
    if env:
        cands.append(env)
    for root in ("C:/", "D:/", "E:/", "F:/"):
        cands.append(os.path.join(root, "Keil_v5", "ARM", "ARMCLANG", "bin"))
    for root in ("C:/", "D:/", "E:/", "F:/"):
        cands.append(os.path.join(root, "keil5", "core", "ARM", "ARMCLANG", "bin"))
    for c in cands:
        if os.path.isfile(os.path.join(c, "armclang.exe")):
            return c
    raise SystemExit("找不到 armclang.exe，请设置环境变量 KEIL_ARMCLANG_BIN 指向 ...\\ARM\\ARMCLANG\\bin")


BIN = _find_bin()

CC = os.path.join(BIN, "armclang.exe")
AS = os.path.join(BIN, "armasm.exe")
LD = os.path.join(BIN, "armlink.exe")
ELF = os.path.join(BIN, "fromelf.exe")

CSRC = [
    ("USER", "main.c"), ("USER", "stm32f10x_it.c"), ("USER", "system_stm32f10x.c"),
    ("CORE/OTHER", "adc.c"), ("CORE/OTHER", "display.c"), ("CORE/OTHER", "init.c"),
    ("CORE/OTHER", "led.c"), ("CORE/OTHER", "motor.c"), ("CORE/OTHER", "oled.c"),
    ("CORE/OTHER", "timer.c"),
    ("SYSTEM/sys", "sys.c"), ("SYSTEM/delay", "delay.c"),
]
FWLIB = [
    "misc.c", "stm32f10x_adc.c", "stm32f10x_bkp.c", "stm32f10x_can.c", "stm32f10x_cec.c",
    "stm32f10x_crc.c", "stm32f10x_dac.c", "stm32f10x_dbgmcu.c", "stm32f10x_dma.c",
    "stm32f10x_exti.c", "stm32f10x_flash.c", "stm32f10x_fsmc.c", "stm32f10x_gpio.c",
    "stm32f10x_i2c.c", "stm32f10x_iwdg.c", "stm32f10x_pwr.c", "stm32f10x_rcc.c",
    "stm32f10x_rtc.c", "stm32f10x_sdio.c", "stm32f10x_spi.c", "stm32f10x_tim.c",
    "stm32f10x_usart.c", "stm32f10x_wwdg.c",
]

INCLUDES = ["USER", "CORE", "STM32F10x_FWLib/inc", "SYSTEM/delay", "SYSTEM/sys", "CORE/OTHER"]

SCATTER = """LR_IROM1 0x08000000 0x10000  {
  ER_IROM1 0x08000000 0x10000  {
   *.o (RESET, +First)
   *(InRoot$$Sections)
   .ANY (+RO)
  }
  RW_IRAM1 0x20000000 0x5000  {
   .ANY (+RW +ZI)
  }
}
"""


def run(cmd, what):
    p = subprocess.run(cmd, capture_output=True, text=False)
    out = (p.stdout or b"").decode("gbk", "replace") + (p.stderr or b"").decode("gbk", "replace")
    if p.returncode != 0:
        print("!! %s 失败 (rc=%d)" % (what, p.returncode))
        print(out.strip()[:3000])
        return False
    if out.strip():
        lines = [l for l in out.splitlines() if "warning" in l.lower() or "error" in l.lower()]
        if lines:
            print("   %s: %d 条告警/错误，前3条:" % (what, len(lines)))
            for l in lines[:3]:
                print("     " + l.strip()[:160])
    return True


def main():
    # 1) 镜像源码
    if os.path.isdir(SRCD):
        shutil.rmtree(SRCD)
    for sub in ["USER", "CORE", "SYSTEM", "STM32F10x_FWLib"]:
        shutil.copytree(os.path.join(SRC, sub), os.path.join(SRCD, sub))
    os.makedirs(OBJ, exist_ok=True)

    # 2) sys.c 的 AC5 内嵌汇编改成 CMSIS 写法（MSR_MSP 全工程未被调用）
    #    按字节替换，不假设源文件编码（GBK / UTF-8 都能处理）
    sp = os.path.join(SRCD, "SYSTEM/sys/sys.c")
    b = open(sp, "rb").read()
    old = ("__asm void MSR_MSP(u32 addr) \r\n{\r\n    MSR MSP, r0 \t\t\t//set Main Stack value\r\n    BX r14\r\n}").encode()
    new = (
        "#if defined(__CC_ARM)\r\n"
        "__asm void MSR_MSP(u32 addr)\r\n{\r\n    MSR MSP, r0\r\n    BX r14\r\n}\r\n"
        "#else\r\n"
        "void MSR_MSP(u32 addr)\r\n{\r\n    __asm volatile(\"msr msp, %0\" : : \"r\" (addr));\r\n}\r\n"
        "#endif"
    ).encode()
    if old in b:
        open(sp, "wb").write(b.replace(old, new))
        print("   sys.c: MSR_MSP 已改为 CMSIS 写法")

    # 3) 汇编启动文件
    start_s = os.path.join(SRCD, "CORE/startup_stm32f10x_hd.s")
    start_o = os.path.join(OBJ, "startup.o")
    if not run([AS, "--cpu=Cortex-M3", "-o", start_o, start_s], "armasm startup"):
        return 1

    # 4) 编译
    inc = []
    for i in INCLUDES:
        inc += ["-I", os.path.join(SRCD, i.replace("/", os.sep))]
    cflags = ["--target=arm-arm-none-eabi", "-mcpu=cortex-m3", "-c", "-Os",
              "-D", "STM32F10X_MD", "-D", "USE_STDPERIPH_DRIVER"]
    for d in EXTRA_DEFS:
        cflags += ["-D", d]
    cflags += inc

    objs = [start_o]
    all_src = [(os.path.join(SRCD, d.replace("/", os.sep)), f) for d, f in CSRC]
    all_src += [(os.path.join(SRCD, "STM32F10x_FWLib", "src"), f) for f in FWLIB]

    fails = 0
    for i, (d, f) in enumerate(all_src, 1):
        o = os.path.join(OBJ, os.path.splitext(f)[0] + ".o")
        if os.path.exists(o):
            os.remove(o)
        ok = run([CC] + cflags + ["-o", o, os.path.join(d, f)], "编译 %s" % f)
        if not ok:
            fails += 1
        else:
            objs.append(o)
    print("   编译完成：%d/%d 成功" % (len(all_src) - fails, len(all_src)))
    if fails:
        return 1

    # 5) 链接
    sct = os.path.join(BUILD, "fw.sct")
    open(sct, "w").write(SCATTER)
    axf = os.path.join(BUILD, OUT_NAME + ".axf")
    mapf = os.path.join(BUILD, OUT_NAME + ".map")
    ok = run([LD, "--cpu=Cortex-M3", "--scatter=" + sct, "--map", "--list=" + mapf,
              "--info=sizes,totals", "--output=" + axf] + objs, "armlink")
    if not ok:
        return 1

    # 6) 出 hex
    hexf = os.path.join(BUILD, OUT_NAME + ".hex")
    if not run([ELF, "--i32", "--output=" + hexf, axf], "fromelf"):
        return 1

    print("\n宏定义:", EXTRA_DEFS or "（默认）")
    print("生成：", hexf, os.path.getsize(hexf), "字节")
    return 0


if __name__ == "__main__":
    sys.exit(main())
