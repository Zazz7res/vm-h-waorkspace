# -*- coding: utf-8 -*-
"""第 03 章示例 D：f-string —— 调试语法/转换/转义、格式规范、3.14 嵌套引号实测、
datetime 与 locale、以及 format/%/拼接 的选型对照。

用法：python3 d_fstring.py 1|2|3|4|5
每行输出都是「f-string 模板原文 → 实际结果（repr）」，模板列是普通字符串，不会被求值。
"""
import sys
from datetime import datetime

DT = datetime(2026, 10, 3, 14, 5, 9)
X = 42
NAME = "世界"
DICT = {"k": "v", "名字": "张三"}


def show(template_text, value, note=""):
    tail = f"  ← {note}" if note else ""
    print(f"{template_text:<38} → {value!r}{tail}")


def part1():
    print("=" * 68)
    print("一、调试语法 {x=}、转换 !r/!s/!a、花括号转义 {{ }}")
    print("=" * 68)
    x = X
    show('f"{NAME}, {x=}"', f"{NAME}, {x=}", "等号=名字加值的 repr，调试神器")
    show('f"{x = }"（空格也保留）', f"{x = }")
    show('f"{x=:.2f}"（等号后还能接格式）', f"{x=:.2f}")
    show('f"{DT.year=}, {DT.month=}"', f"{DT.year=}, {DT.month=}", "左边整个是表达式")
    show('f"{1.5!s}"', f"{1.5!s}", "!s 强制 str()")
    show("f\"{' 文本 '!r}\"", f"{' 文本 '!r}", "!r 强制 repr()，空格现形")
    show("f\"{'中'!a}\"", f"{'中'!a}", "!a 用 ascii()，非 ASCII 变 \\uXXXX")
    show('f"{{{x}}}"（花括号双写）', f"{{{x}}}", "想输出 {42} 就这么写")
    print("→ 坑：f-string 里的字面花括号必须成对双写，单个 { 直接 SyntaxError。")


def part2():
    print("=" * 68)
    print("二、格式规范速写：填充/对齐/宽度/千分位/精度/符号/进制")
    print("=" * 68)
    show("f\"{'ab':*^10}\"", f"{'ab':*^10}", "填充字符 * + 居中 ^")
    show('f"{3.14159:>10.2f}"', f"{3.14159:>10.2f}", "右对齐宽 10，两位小数")
    show('f"{-3.14159:<+8.1f}"', f"{-3.14159:<+8.1f}", "左对齐 + 强制符号")
    show('f"{1234567:,}"', f"{1234567:,}", "千分位")
    show('f"{1234567.5:,.2f}"', f"{1234567.5:,.2f}", "千分位 + 精度")
    show('f"{42:b}" / f"{42:#010b}"', f"{42:b} / {42:#010b}", "二进制；# 带 0b 前缀")
    show('f"{255:#x}"', f"{255:#x}", "十六进制带前缀")
    show('f"{0.256:.1%}"', f"{0.256:.1%}", "百分数")
    show('f"{65:c}"', f"{65:c}", "整数按码点出字符")
    show('f"{-42:+d}  f"{0:+d}"', f"{-42:+d}  {0:+d}", "符号位")
    show('f"{-42:=05d}"', f"{-42:=05d}", "= 把符号留在最左、零填充跟在后面")
    w = 8
    show('f"{\'x\':>{w}}"（宽度现算）', f"{'x':>{w}}")
    show('f"{\'中\':>6}A"', f"{'中':>6}A", "宽度按【码点数】补，不是显示格数！")
    try:
        eval('f"{9876:.4}"')
    except ValueError as e:
        print(f"整数给小数精度 → ValueError: {e}")
    print("→ 记忆：[[fill]align][sign][#][0][width][,][.precision][type]")


def part3():
    print("=" * 68)
    print("三、嵌套引号在 3.14 上的实测（这台机器跑出来的，不是文档抄的）")
    print("=" * 68)
    d = DICT
    x = X
    show('f\'{d["k"]}\'（老规矩：外单内双）', f'{d["k"]}')
    show('f"{d["k"]}"（同种引号复用）', f"{d["k"]}", "3.12+ PEP 701，本机通过")
    show('f"{d["名字"]}"', f"{d["名字"]}", "中文键也照样")
    show('f"{chr(10).join(["a", "b"])}"（表达式带反斜杠）',
         f"{chr(10).join(['a', 'b'])}", "3.12+ 允许表达式里出现 \\n")
    show('f"{f"内层{x}外"}"（f 套 f）', f"{f"内层{x}外"}")
    show('f"{"a" if x else "b"}"（引号直接复用）', f"{"a" if x else "b"}")
    print("三引号包裹、内部再嵌 f 的多行写法：")
    print(f"""{DT:%Y 年 %m 月 %d 日}  ← 这一行是三引号 f 串打出来的
第二行照打。""")
    print("→ 结论：本机 3.14.4 上面六条全部能跑；3.11 及以前只有第一条能活。")
    print("  旧教程说「f-string 里绝对不能同种引号/反斜杠」——那是 3.11 的老规矩。")


def part4():
    print("=" * 68)
    print("四、datetime 格式串；strftime 是唯一跟 locale 纠缠的地方")
    print("=" * 68)
    dt = DT
    show('f"{DT:%Y-%m-%d %H:%M:%S}"', f"{dt:%Y-%m-%d %H:%M:%S}")
    show('f"{DT:%F %T}"', f"{dt:%F %T}", "%F/%T 是 ISO 速记")
    show('f"{DT:%A %B %d}"', f"{dt:%A %B %d}", "LC_TIME=C → 英文星期月名")
    show('f"{DT:%j} 天 / 第 %U 周"', f"{dt:%j} 天 / 第 {dt:%U} 周")
    show('DT.isoformat()', dt.isoformat(), "想要机器可读就这个")
    import locale
    print("—— 现场换 locale 再打同一张串 ——")
    print(f"不调 setlocale：{dt.strftime('%A %B %d')}")
    try:
        locale.setlocale(locale.LC_TIME, "zh_CN.UTF-8")
        print(f'setlocale(LC_TIME, "zh_CN.UTF-8") 后：{dt.strftime("%A %B %d")}')
    except locale.Error as e:
        print(f"setlocale 失败：{e}")
    locale.setlocale(locale.LC_TIME, "C")
    print(f"设回 C：{dt.strftime('%A %B %d')}")
    print("→ 关键实测：外部环境变量 LC_ALL=zh_CN.UTF-8 起进程时 %A 仍是英文；")
    print("  Python 只把 LC_CTYPE 交给自己管，LC_TIME 默认停在 C，要代码里 setlocale。")
    print("  而 str.upper/casefold 完全不理会 locale（第 21 节现场对照）。")


def part5():
    print("=" * 68)
    print("五、str.format、%、加号、join 同台")
    print("=" * 68)
    a, b = "张三", 42
    print(f"f-string（日常首选）：{a} 得了 {b} 分")
    print("str.format：{} 得了 {} 分".format(a, b))
    print("str.format：{0}-{1}-{0}".format(a, b))
    print("str.format：{a}-{b}".format(a=a, b=b))
    print("format_map：{k}-{名字}".format_map({"k": a, "名字": b}))
    print("百分号老式写法：%s 得了 %d 分" % (a, b))
    print("百分号配字典：%(who)s 的分数 %(score)d" % {"who": a, "score": b})
    print("百分号里打一个字面 %% 要写两个（这行的结尾就是）：%% → %%" % ())
    print(f"加号拼接：{'a' + 'b' + str(3)}（数字必须手动 str()）")
    words = ["春眠", "不觉晓"]
    print(f"join 拼接：{'，'.join(words)} ← 分隔符在前，可迭代对象在后")
    print("→ 选型：日常 f-string；模板来自配置/用户的字符串才用 .format/format_map；")
    print("  % 只在老代码里认得它就行；成批拼接用 join，别用 +=（第 16 节量给你看）。")


if __name__ == "__main__":
    which = sys.argv[1] if len(sys.argv) > 1 else "1"
    {"1": part1, "2": part2, "3": part3, "4": part4, "5": part5}[which]()
