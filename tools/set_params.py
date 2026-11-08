# -*- coding: utf-8 -*-
"""改 src/USER/main.c 的六个参数。

GBK 安全：先把整段文本 encode 成 GBK 成功，再写盘，避免"写一半失败"把源文件清空。

用法：
    python tools/set_params.py 950 638 1.02 0.075 450 25
    顺序：speed Steer_Max p d speed_min Dead_Zone
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
P = os.path.join(HERE, "src", "USER", "main.c")

NAMES = ["speed", "Steer_Max", "p", "d", "speed_min", "Dead_Zone"]


def patch(path, vals):
    s = open(path, "rb").read().decode("gbk")
    lines = s.split("\n")
    done = set()
    for i, ln in enumerate(lines):
        body = ln.split("//")[0]
        m = re.match(r"^(\s*(?:int|float)\s+)(\w+)(\s*=\s*)([-\d\.]+)(f?)(\s*;)\s*$", body)
        if not m:
            continue
        name = m.group(2)
        if name not in NAMES or name in done:
            continue
        new = "%s%s%s%s%s%s" % (m.group(1), name, m.group(3), vals[NAMES.index(name)],
                                m.group(5), m.group(6))
        lines[i] = new
        done.add(name)
    if len(done) != len(NAMES):
        raise SystemExit("只改到 %d 个参数：%s" % (len(done), sorted(done)))
    open(path, "wb").write("\n".join(lines).encode("gbk"))


if __name__ == "__main__":
    if len(sys.argv) != 7:
        raise SystemExit(__doc__)
    patch(P, sys.argv[1:])
    print("ok:", " ".join("%s=%s" % (n, v) for n, v in zip(NAMES, sys.argv[1:])))
