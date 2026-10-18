#!/usr/bin/env python3
# ============================================================================
# 取证脚本 r_capstone.py —— 第 19 节：把本章零件装成一个真工具（安全读配置）
#
# 需求：读一个 JSON 配置；分清「文件没有」「没权限」「内容坏」「字段缺」；
#       只重试真正可能是瞬时的错；报错要能一层层追溯到根因；不留半截临时文件。
# 用到的节：第 8 节（errno 分派）第 9 节（异常家族）第 5 节（from 链）
#           第 14 节（ExitStack callback）第 18 节（with + 真实文件）第 16 节（不许静默）
# ============================================================================
import errno
import json
import os
import shutil
from contextlib import ExitStack
from pathlib import Path

WORK = Path("capstone-demo")
if WORK.exists():
    shutil.rmtree(WORK)
WORK.mkdir()

STATS = {"attempts": 0, "ok": 0, "retried": 0}
AUDIT = []                      # 由 ExitStack.callback 填：每一次尝试都留下一条


# ---------------------------------------------------------------- 家族（第 9 节）
class AppError(Exception):
    """自家一切的根：带一个给机器看的 code，消息只给人看。"""

    def __init__(self, message, code="E-GENERIC"):
        super().__init__(message)
        self.code = code


class ConfigError(AppError):
    """配置读不出来 —— 调用方接这一个就能一网打尽。"""


class ConfigUnreadable(ConfigError):
    """到不了内容那一步（IO 层）。"""


class ConfigInvalid(ConfigError):
    """内容读出来了，但不合约定（schema 层）。"""


# ---------------------------------------------------------------- 工具本体
def read_config(path, attempts=3, open_fn=None):
    """返回 dict。任何失败都抛 ConfigError 的儿子，并带着 __cause__ 指回根因。

    open_fn 是为了能把「瞬时故障」那一支演出来（第 6 段）而留的口子；默认就是 path.open。
    """
    if open_fn is None:
        open_fn = lambda: path.open(encoding="utf-8")
    last_oserror = None
    for i in range(1, attempts + 1):
        STATS["attempts"] += 1
        try:
            with ExitStack() as stack:
                # callback 登记的是「不管成功失败都要留下这一条」——审计日志的标准用法（第 14 节）
                stack.callback(AUDIT.append, f"第 {i} 次尝试读 {path}")
                f = stack.enter_context(open_fn())
                data = json.load(f)
        except FileNotFoundError as exc:
            raise ConfigUnreadable(
                f"配置文件 {path} 不存在（{exc.strerror}）", code="E-CFG-ENOENT") from exc
        except PermissionError as exc:
            raise ConfigUnreadable(
                f"配置文件 {path} 读不了（{exc.strerror}）——这是环境配置错误，不是首次运行",
                code="E-CFG-ACCES") from exc
        except IsADirectoryError as exc:
            # 路径写成了目录：重试没有意义，但要留下「试了几次」的证据
            raise ConfigUnreadable(
                f"{path} 是个目录，不是一个文件", code="E-CFG-ISDIR") from exc
        except json.JSONDecodeError as exc:
            raise ConfigInvalid(
                f"{path} 第 {exc.lineno} 行第 {exc.colno} 列不是合法 JSON：{exc.msg}",
                code="E-CFG-JSON") from exc
        except OSError as exc:
            # 只有走到这一支的错才当瞬时故障重试（EBUSY / EMFILE 之类）
            last_oserror = exc
            STATS["retried"] += 1
            if i == attempts:
                raise ConfigUnreadable(
                    f"{path} 连试 {attempts} 次都失败：{exc.strerror}",
                    code="E-CFG-IO") from exc
            continue
        STATS["ok"] += 1
        return data
    raise ConfigUnreadable("不该走到这里（循环出口写漏了）", code="E-CFG-BUG") from last_oserror


def require(data, keys, path):
    """schema 校验：缺字段就说清缺哪个、手上有哪些。"""
    missing = [k for k in keys if k not in data]
    if missing:
        raise ConfigInvalid(
            f"{path} 里缺字段 {missing}；实际读到的字段是 {sorted(data)}", code="E-CFG-SCHEMA")
    return {k: data[k] for k in keys}


def report(exc):
    """把异常链逐层打出来 —— 只用类型/code/消息，**不打 traceback**（第 9 段解释为什么不打）。"""
    lines, cur, walked, depth = [], exc, [], 1
    while cur is not None and depth <= 6:
        if any(cur is seen for seen in walked):
            lines.append(f"    第 {depth} 层：链上又绕回了已经打过的异常，到此为止")
            break
        walked.append(cur)
        code = getattr(cur, "code", None)
        lines.append(f"    第 {depth} 层：{type(cur).__name__}"
                     + (f"({code}) " if code else " ") + str(cur))
        cur = cur.__cause__ if cur.__cause__ is not None else cur.__context__
        depth += 1
    return "\n".join(lines)


print("=== 1. 正常路径：读到了就返回，报错一个字都不打 ===")
good = WORK / "good.json"
good.write_text('{"name": "demo", "port": 8080}', encoding="utf-8")
data = read_config(good)
print("  read_config(good.json) =", data)
print("  require(...) 取走的字段 =", require(data, ["name", "port"], good))
print("  记账：", STATS)

print("\n=== 2. 文件不存在：不重试，直接给出「这是首次运行」那一类结论 ===")
try:
    read_config(WORK / "没有.json")
except ConfigError as exc:
    print(report(exc))
    print("  记账（attempts 只 +1，因为 ENOENT 不重试）：", STATS)

print("\n=== 3. 路径是个目录：三类 OSError 走的是三条不同的支路 ===")
adir = WORK / "是个目录"
adir.mkdir()
try:
    read_config(adir)
except ConfigError as exc:
    print(report(exc))

print("\n=== 4. 内容坏：JSONDecodeError 自带行列，直接进消息（第 7 节那份字段的复用）===")
bad = WORK / "bad.json"
bad.write_text('{"name": "demo",\n "port": 8080,,,\n}', encoding="utf-8")
try:
    read_config(bad)
except ConfigError as exc:
    print(report(exc))
    print("  → 有了行列，你去修的是第 2 行；只有「Expecting value」的话整个文件都得读一遍。")

print("\n=== 5. 字段缺：IO 没错、JSON 没错，但**不合约定** → 另一个儿子 ===")
thin = WORK / "thin.json"
thin.write_text('{"name": "demo"}', encoding="utf-8")
try:
    require(read_config(thin), ["name", "port"], thin)
except ConfigError as exc:
    print(report(exc))

print("\n=== 6. 唯一该重试的那一支：瞬时故障（用假 open 演，不靠运气）===")
FLAKY = {"calls": 0}


def flaky_open():
    """前两次假装设备忙（EBUSY），第三次真的打开 good.json。"""
    FLAKY["calls"] += 1
    if FLAKY["calls"] < 3:
        raise OSError(errno.EBUSY, "设备忙（这是假的，真机上没法保证碰得到）")
    return good.open(encoding="utf-8")


before = dict(STATS)
print("  读到 =", read_config(good, attempts=3, open_fn=flaky_open),
      "｜ open 被叫了", FLAKY["calls"], "次")
print("  记账变化：", before, "→", STATS, "（retried 终于不是 0 了）")
FLAKY["calls"] = 0


def always_busy():
    raise OSError(errno.EBUSY, "一直忙")


try:
    read_config(good, attempts=3, open_fn=always_busy)
except ConfigError as exc:
    print(report(exc))
    print("  → 耗尽次数才报错，报的是**最后一次**那个 OSError（from last_oserror），")
    print("    并带着「试了几次」这个数字。真实代码这里该 sleep 一下再试；")
    print("    本章不睡，因为 sleep 会让输出不可复现（第 15 节的计时也因此只读倍率）。")

print("\n=== 7. 调用方只要接 ConfigError 一网打尽；想区分就接窄的（第 4 节的顺序律）===")
for path in (WORK / "没有.json", bad, thin):
    try:
        if path == thin:
            require(read_config(path), ["name", "port"], path)
        else:
            read_config(path)
    except ConfigUnreadable as exc:
        print(f"  {str(path):<24} → 走「读不到」分支：{exc.code}")
    except ConfigInvalid as exc:
        print(f"  {str(path):<24} → 走「内容坏」分支：{exc.code}")
    else:
        print(f"  {str(path):<24} → 成功")
print("  → 三条支路的**处理方式本来就该不同**：读不到→提示路径/权限；内容坏→报行列让人修文件；")
print("    字段缺→给默认值或拒绝启动。要是只有一个 Exception，这三件事就会被写成同一个 pass。")

print("\n=== 8. 现场对账：临时文件有没有留下？（顺便证明这条对账**会响**）===")
leftovers = sorted(os.listdir(WORK))
print("  目录内容 =", leftovers)
print("  带 .tmp 后缀的残留 =", [n for n in leftovers if n.endswith(".tmp")], "→ 期望是 []")
leak = WORK / "故意漏下的.tmp"
leak.write_text("写到一半的那份", encoding="utf-8")
print("  先故意漏一个，同一条扫法立刻变红：",
      [n for n in sorted(os.listdir(WORK)) if n.endswith(".tmp")])
leak.unlink()
print("  删掉再扫：", [n for n in sorted(os.listdir(WORK)) if n.endswith(".tmp")],
      "→ 所以上面那句 [] 是查出来的，不是默认值。")
print("  审计流水（每次尝试都记了一条，失败的也记）：")
for entry in AUDIT:
    print("    ·", entry)
print("  记账总数 =", STATS, "｜ attempts 和 ok 的差 = 失败次数 =",
      STATS["attempts"] - STATS["ok"])

shutil.rmtree(WORK)
print("  清理后：capstone-demo/ 还在吗 →", WORK.exists(),
      "｜ 当前目录剩 =", sorted(os.listdir(".")))

print("\n=== 9. 为什么 report() 里不打 traceback ===")
print("  ① 这是**给用户看的 CLI 报错**：路径和帧对修配置的人没用，行列和 code 才有。")
print("     第 7 节教的是「怎么把帧拿出来」，第 15 节量的是「把帧排版出来要多少钱」——")
print("     两条都不等于「每一处报错都该带上整份栈」；带上不带上，取决于读者是谁。")
print("  ② 打整份 traceback 是第 15 节量过的那笔大开销（比 str 贵两个数量级），")
print("     但对**故障**来说该花 —— 所以工程做法是两条都要：")
print("     给人看的那一行走 stdout/CLI，给运维查的那份带帧的走日志（logging.exception，第 16 节第 3 段）。")
print("  ③ 上面这个函数走的是第一条；这个脚本没有日志系统，所以第二条留在 README 的示例里。")

print("\n=== 10. 收口：这个小工具用掉了本章哪几节 ===")
rows = [
    ("第 2 节", "except 接住之后 continue、正常路径才走到 return —— 谁跑谁不跑就是第 2 节那张走位图"),
    ("第 4 节", "except 的顺序：窄的先（FileNotFoundError 在 OSError 之前）"),
    ("第 5 节", "每一处包装都写 from exc，根因不丢"),
    ("第 8 节", "报错里读的是 strerror / lineno / colno 这些**结构化字段**，不是猜消息"),
    ("第 9 节", "AppError(ConfigError(...)) 三层家族 + code 字段"),
    ("第 11 节", "读文件全程写在 with 里：块体崩了也照样 close"),
    ("第 14 节", "ExitStack.callback 登记审计流水：成功失败都留下一条"),
    ("第 15 节", "只对可能是瞬时的错重试；ENOENT 重试三遍是纯粹的浪费"),
    ("第 16 节", "没有任何一处 except: pass；每条支路都有结论或者往上抛"),
    ("第 18 节", "close 不等于事务，所以第 8 段当场扫一遍 .tmp 残留"),
]
for section, what in rows:
    print(f"  {section:<8}→ {what}")
print("  这一版**没**演的一条：第 17 节「CLI 顶层接住 ConfigError 之后要 sys.exit(非 0)」——")
print("  这个脚本不是 CLI（它的作用是把过程打给你看），所以它的退出码恒 0；写法见 README 第 19 节。")
