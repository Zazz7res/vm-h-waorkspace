#!/usr/bin/env python3
# ============================================================================
# 取证脚本 k_read_write_text.py —— 第 08 章第 12 讲：read_text / write_text 的最短路径和它的三个坑
#
# 只证一件事：这两个方法是「整份读、整份写、小文件」的糖，甜的地方只有两行，
# 坑一共有三个，而且**没有一个会报错**：
#   坑 1 write_text 是截断写（打开那一刻旧的没了，还没等你写）；
#   坑 2 不写 encoding 时用的是 locale 的首选编码（换台机器就换一份字节）；
#   坑 3 read_text 会替你 close，但你要是混用了 open(p).read()，没人替你 close。
# 三个坑全现场演一遍，另外把三种必然失败的报错原文一字不改地打出来。
#
# 现场全在当前目录的 text-demo/ 里现造现删；只读自己的文件，不联网（子进程只跑本地代码）。
# ============================================================================
import contextlib
import gc
import inspect
import io
import locale
import os
import re
import shutil
import subprocess
import sys
import warnings
from pathlib import Path

WORK = Path("text-demo")


def rel(p):
    """屏幕上只出现相对路径：报错原文里的路径也是相对的，两遍跑才有得比。"""
    return str(p)


def read_bytes_len(p):
    return len(Path(p).read_bytes())


print("=== 1. 最短路径就两行：write_text 写完，read_text 读回；先看它返回的是什么 ===")
shutil.rmtree(WORK, ignore_errors=True)
WORK.mkdir()
p = WORK / "hello.txt"
n1 = p.write_text("内容\n", encoding="utf-8")
back = p.read_text(encoding="utf-8")
print("  write_text('内容\\n') 的返回值 =", repr(n1), "｜ 类型 =", type(n1).__name__)
print("  read_text(encoding='utf-8') 读回 =", repr(back), "｜ 字符数 =", len(back))
n2 = (WORK / "mix.txt").write_text("中文abc", encoding="utf-8")
print("  ★ 中文那一次实测：write_text('中文abc') 返回 =", n2, "个字符 ｜ 但磁盘上真实落的是", read_bytes_len(WORK / "mix.txt"), "字节")
print("  → 返回值是**字符数**（len(data)），不是写进磁盘的字节数；两者只在纯 ASCII 时相等。")
print("    纯中文时字节数是字符数的三倍（UTF-8 一个汉字 3 字节）——这个数不能拿去核对字节。")

print("\n=== 2. ★ 坑 1：write_text 是截断写，不是追加写 ===")
q = WORK / "trunc.txt"
long_n = q.write_text("0" * 30, encoding="utf-8")
before = read_bytes_len(q)
short_n = q.write_text("短", encoding="utf-8")
after = read_bytes_len(q)
print(f"  第一次写 {long_n} 个字符 → 磁盘 {before} 字节 ｜ 第二次写 {short_n} 个字符 → 磁盘 {after} 字节")
print("  现在文件里剩下的内容 =", repr(q.read_text(encoding="utf-8")), "｜ 长度 =", len(q.read_text(encoding="utf-8")))
print("  → 前 30 个字符**一个字都没剩**。write_text 内部就是 open(p, 'w')：'w' 在打开那一刻 truncate。")
sig = inspect.signature(Path.write_text)
print("  本机 3.14 的签名（inspect 实测，一字不改）：write_text", sig)
params = list(sig.parameters)
print("  里面有 'mode' 这个参数吗 →", "mode" in params, "｜ 里面有 'append' 吗 →", "append" in params)
try:
    q.write_text("x", mode="a", encoding="utf-8")
except TypeError as exc:
    print("  硬塞一个 mode= 试试 →", f"{type(exc).__name__}: {exc}")
print("  ★ 本机事实：3.14 的 write_text **没有** mode= 也没有 append=。")
print("    谁在代码里写 `p.write_text(s, mode='a')`，它不会变成追加，只会当场 TypeError。")

print("\n=== 3. 想追加只有一条路：自己 open(..., 'a')（同一个文件，接着上一节的状态）===")
before = read_bytes_len(q)
with open(q, "a", encoding="utf-8") as fh:
    fh.write("续")
after = read_bytes_len(q)
print("  open(p,'a') 写 1 个字符前后：磁盘字节数", before, "→", after, "｜ 现在的内容 =", repr(q.read_text(encoding="utf-8")))
before2 = read_bytes_len(q)
q.write_text("重来的内容", encoding="utf-8")
print("  再 write_text 一次：磁盘字节数", before2, "→", read_bytes_len(q), "｜ 内容 =", repr(q.read_text(encoding="utf-8")), "← 「续」又没了")
print("  → 追加语义在 open 的模式字符里，不在 write_text 的参数里。要追加就自己管 with。")

print("\n=== 4. ★ 坑 2：不写 encoding 时，默认值从哪来（本机能证明，换 locale 立刻变）===")
print("  本进程的默认值：sys.getfilesystemencoding() =", sys.getfilesystemencoding(),
      "｜ locale.getpreferredencoding(False) =", locale.getpreferredencoding(False))
c = WORK / "noenc.txt"
c.write_text("中文")                      # 故意不写 encoding
print("  write_text('中文') 不写 encoding → 磁盘字节 =", c.read_bytes().hex())
print("  read_text() 不写 encoding 读回来相等吗 →", c.read_text() == "中文")
print("  → 本机这条侥幸是对的，因为默认编码恰好是 UTF-8。**侥幸不是判据**，所以下面把 locale 换掉：")
snip = WORK / "child.py"
# 子进程的标签一律用 ASCII：它自己 print 的编码也受 locale 管，标签里塞中文会在 LC_ALL=C 下
# 先炸在 print 上，就撞不到我们想看的那条 write_text 的错了。
snip.write_text( "import locale\n" "from pathlib import Path\n" "print('pref =', locale.getpreferredencoding(False))\n" "try:\n"
    "    Path('child.txt').write_text('\\u4e2d\\u6587')\n"
    "    print('wrote bytes =', Path('child.txt').read_bytes().hex())\n"
    "except Exception as exc:\n"
    "    print(type(exc).__name__ + ': ' + str(exc)[:120])\n",
    encoding="utf-8")
base_env = {k: v for k, v in os.environ.items()
            if k not in ("PYTHONUTF8", "PYTHONCOERCECLOCALE", "LC_ALL", "LANG", "LC_CTYPE")}


def child_lines(text):
    """把子进程的输出削成只有结论行：Traceback / File \"...\" 这两类行整条丢掉
    —— 它们带绝对路径，是这台机器的沙箱名，不是本页要印的证据。"""
    keep = []
    for line in text.splitlines():
        if line.startswith(("Traceback", "  File \"", "    ~", "    File \"")):
            continue
        if line.strip():
            keep.append("  [子进程] " + line.strip())
    return keep


for label, extra in (("① 什么都不改", {}), ("② LC_ALL=C + 关掉 UTF-8 模式与 locale 强制", {"PYTHONUTF8": "0", "PYTHONCOERCECLOCALE": "0", "LC_ALL": "C"})):
    env = dict(base_env)
    env.update(extra)
    res = subprocess.run([sys.executable, "child.py"], cwd=str(WORK), env=env, capture_output=True, text=True, errors="replace")
    print(f"  {label}：")
    for line in child_lines(res.stdout + res.stderr):
        print(line)
print("  ★ 同一段代码、同一个 write_text，换个 locale 就换一种结局（UTF-8 字节 vs UnicodeEncodeError）。")
print("    判据：文本进出的**边界上必须写死 encoding='utf-8'**。省略它 = 把行为外包给环境变量。")

print("\n=== 5. ★ 坑 3：read_text 会替你 close，但 open(p).read() 没人替你 close ===")
leak = WORK / "no_close.txt"
leak.write_text("就这样\n", encoding="utf-8")
print("  默认过滤器里 ResourceWarning 的位置：", [f[:1] + (f[2].__name__,) for f in warnings.filters if f[2] is ResourceWarning],
      "← 第 0 位是动作，'ignore' 就是它平时看不见的原因")
with warnings.catch_warnings(record=True) as caught:      # 不动过滤器，只用默认值
    open(leak, encoding="utf-8").read()                   # 故意不 with、不 close
    gc.collect()
print("  默认过滤器下：这次捕获到的警告条数 =", len(caught), "（屏幕上一片安静 —— 它被吃掉了）")
PREFIX = re.compile(r"^[^:\n]*:\d+: ")                    # 行首的「文件名:行号: 」——代码一改就抖


def clean(text):
    """把不稳定 parts 抹掉，只留下能印进书里的那几行：
    ① 行首的「文件名:行号: 」整段丢掉（代码一改行号就漂）；
    ② `_io.TextIOWrapper` 只留类名（模块前缀随版本变，而且消息里本机没有内存地址）；
    ③ 解释器顺手回显的那行**源码**整条丢掉（它就是我这一行代码本身，改脚本就变）。"""
    out = []
    for line in text.splitlines():
        line = PREFIX.sub("", line, count=1).rstrip()
        line = re.sub(r"<(\w+\.)?(\w+)", r"<\2", line)
        if not line.startswith("ResourceWarning:"):
            continue
        out.append(line)
    return out


def leak_once():
    buf = io.StringIO()
    with warnings.catch_warnings():
        warnings.simplefilter("always")                    # ★ 就这一行，安静变成屏幕上的字
        with contextlib.redirect_stderr(buf):
            open(leak, encoding="utf-8").read()
            gc.collect()                                   # CPython 靠引用计数，断开引用即回收并告警
    return buf.getvalue()


first, second = clean(leak_once()), clean(leak_once())
print("  simplefilter('always') 之后，stderr 上真的打出这几行（原文，已抹掉行号前缀）：")
for line in first:
    print("   ", line)
print("  同一个动作连做两遍，清洗后的输出相等吗 →", first == second, "（消息里本机**没有**内存地址，只有 name/mode/encoding，所以能稳定复现）")
with warnings.catch_warnings(record=True) as caught2:
    warnings.simplefilter("always")
    open(leak, encoding="utf-8").read()
    gc.collect()
    for w in caught2:
        # record=True 拿到的是 WarningMessage 壳子，警告本身的类型在 w.category 上
        stable = re.sub(r"<(\w+\.)?(\w+)", r"<\2", str(w.message))
        print("  结构化一点看：w.category.__name__ =", w.category.__name__, "｜ 稳定部分 =", stable)
print("  → 平时它不打字不是「没有泄漏」，是默认过滤器把 ResourceWarning 设成了 ignore。")
print("    想让它响：`python -X dev`、`PYTHONWARNINGS=always`，或者上面那行 simplefilter('always')。")

print("\n=== 6. 三种必然失败，原文一字不改（errno 一起打）===")
cases = [
    ("read_text 读一个不存在的路径", lambda: (WORK / "没有.txt").read_text(encoding="utf-8")),
    ("write_text 写进一个不存在的父目录", lambda: (WORK / "没有的目录" / "x.txt").write_text("x", encoding="utf-8")),
    ("read_text 读一个目录", lambda: WORK.read_text(encoding="utf-8")),
]
for label, fn in cases:
    try:
        fn()
        print(f"  {label:26} → 竟然成功了（本机的行为和我预期不符，这条要重读）")
    except OSError as exc:
        print(f"  {label:26} → {type(exc).__name__}: {exc} ｜ errno={exc.errno}")
print("  → 前两条都是 FileNotFoundError(errno 2)，但**原因不同**：一个缺文件、一个缺目录，" "原文里的路径字符串能告诉你差在哪。第三条是 IsADirectoryError(errno 21) —— "
      "它不是 PermissionError，权限没挡你，是类型不对。")

print("\n=== 7. read_text 只有一个动作：把整份文件变成一个 str 放进内存 ===")
big = WORK / "big.txt"
unit = "中文字符"
big.write_text(unit * 200, encoding="utf-8")
txt = big.read_text(encoding="utf-8")
print("  写了 200 组「中文字符」 → read_text 返回类型 =", type(txt).__name__, "｜ len(str) =", len(txt), "个字符", "｜ 磁盘上 =", read_bytes_len(big), "字节")
print("  → 两个数字并排看：UTF-8 下一个汉字占 3 字节，所以 800 个字符 = 2400 字节。" "「文件多大」和「读进来多长」永远分两栏算。")
print("  → 更关键的：这一句没有中间状态，整个 str 一次性在内存里；")
print("    200 MB 的日志也能「读成功」，直到内存吃紧。怎么分块读、什么时候必须分块，见第 18 节。")

print("\n=== 8. 收口：什么时候能用这两个方法，什么时候必须自己 open ===")
print("  ① read_text/write_text 的适用面就一句话：**整份读、整份写、文件小**（配置、模板、几十个字的中间结果）。")
print("  ② 一旦脑子里冒出下面任何一个问题，就得退回 open() 自己管：")
print("     要追加？（write_text 没有 append，第 2、3 段实测）；")
print("     编码是谁定的？（默认跟 locale 走，第 4 段实测；边界上必须写死 utf-8）；")
print("     文件会有多大？（整份进内存，第 7 段）；")
print("     要不要边写边flush、要不要读一半就停？（这两个方法没有游标）。")
print("  ③ open(p).read() 这种「不 with」的写法不会报错，只会漏文件描述符，")
print("     而且默认过滤器连一声 ResourceWarning 都不给你（第 5 段实测：默认 0 条、always 才响）。")
print("  ★ 配置文件**不能**用 write_text：它是截断写，进程在第 2 段那种「已经清空、还没写新内容」的")
print("     窗口里被断电/OOM/Ctrl-C，配置文件就永久没了。写配置要走临时文件 + 原子 rename，见第 17 节。")
shutil.rmtree(WORK)
print(f"  收尾自证：{WORK.name}/ 还在吗 → {WORK.exists()} ｜ 本次在它里面建过的文件已全部随目录删除")
