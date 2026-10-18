#!/usr/bin/env python3
# ============================================================================
# 取证脚本 h_custom.py —— 第 9 节：自定义异常怎么写才算「像个异常」
#
# 目标：继承自谁、带不带数据、能不能 pickle、消息怎么来的，全部现场验。
# ============================================================================
import pickle


class AppError(Exception):
    """本项目所有自定义异常的根：只多带一个 code 字段。"""

    def __init__(self, message, code="E-GENERIC"):
        super().__init__(message)
        self.code = code


class ConfigMissing(AppError, LookupError):
    """两个父亲：AppError 管自家口径，LookupError 让调用方能接宽的。"""


class Lazy(AppError):
    """什么都不写：继承下来的 __init__ 直接把参数塞进 args。"""


print("=== 1. 只继承、不加东西，就已经完全能用了 ===")
try:
    raise Lazy("这行就是消息本体")
except AppError as exc:
    print("  type(exc) =", type(exc).__name__)
    print("  str(exc) =", str(exc), "｜ repr(exc) =", repr(exc))
    print("  exc.args =", exc.args, "｜ args 来自 BaseException.__init__ 的原样保存")
    print("  用 Exception 能接住它吗 →", isinstance(exc, Exception),
          "｜ 用 BaseException →", isinstance(exc, BaseException))
print("  → 只要 raise 的是**类或实例**，`except` 按继承链匹配；这一条和第 3 节的家谱是同一套机制。")

print("\n=== 2. 带 payload（结构化数据）：字段要显式挂，args 只管消息 ===")
try:
    raise AppError("数据库连不上", code="E-DB-503")
except AppError as exc:
    print("  人类看的：str(exc) =", str(exc))
    print("  机器看的：exc.code =", exc.code, "｜ exc.args =", exc.args)
    print("  关键：args 里**没有** code —— super().__init__(message) 只收了第一个参数。")
    print("  → 所以给日志用 str(exc)，给代码用 exc.code；两者别混在一句话里再反解析。")

print("\n=== 3. 多继承：让调用方既能接「自家的」也能接「标准的」 ===")
print("  ConfigMissing.__mro__ =", [c.__name__ for c in ConfigMissing.__mro__])
for label, catcher in [("AppError", AppError), ("LookupError", LookupError),
                       ("ValueError", ValueError), ("Exception", Exception)]:
    try:
        raise ConfigMissing("少一个 section")
    except catcher as exc:
        print(f"  except {label:<12} → 接住了（{type(exc).__name__}: {exc}）")
    except BaseException as exc:
        print(f"  except {label:<12} → 接不住，漏出去的是 {type(exc).__name__}")
print("  → 自定义异常**同时**挂一个标准库的父亲，外部代码就能按标准口径接你。")
print("    这就是第 3 节里 OSError/FileNotFoundError 那种关系的用法：一层宽一层窄。")

print("\n=== 4. 该继承谁？三条判据（这节唯一需要背的东西）===")
print("  ① 想让调用方用 `except OSError` 之类现成口径接住 → 挂对应的标准异常做第二个父亲（第 3 小段）。")
print("  ② 纯自家业务错 → 直接继承 Exception，**不要**继承 BaseException（那会躲过 except Exception，")
print("     和第 3 节 KeyboardInterrupt 一样变成「必须显式接」的东西，除非你就是在做退出信号）。")
print("  ③ 不要继承 str / dict 之类具体容器来「顺便带数据」：异常的 __str__、args、pickle 全靠")
print("     BaseException 那套，多一套父亲会把报错文案搅浑。带数据就显式挂字段（第 2 小段）。")

print("\n=== 5. 能不能 pickle：跨进程/写日志时会撞上的问题 ===")


def roundtrip(label, obj):
    try:
        back = pickle.loads(pickle.dumps(obj))
        print(f"  {label} → 往返成功：{type(back).__name__} args={back.args}"
              f" ｜ code 还在吗 → {getattr(back, 'code', '（这个类没这字段）')}")
    except Exception as exc:
        print(f"  {label} → {type(exc).__name__}: {exc}")


roundtrip("Lazy（无自定义 __init__）", Lazy("消息"))
roundtrip("AppError（自定义 __init__ 收两个参数）", AppError("消息", "E-X"))


def make_hidden():
    """在函数里造一个异常类，且不把名字绑到模块上。"""
    return type("Hidden", (Exception,), {})("x")


roundtrip("函数里造的类、名字没绑在模块上", make_hidden())
Transient = type("Transient", (Exception,), {})
roundtrip("同一份 type()，但这次把名字绑到了模块全局", Transient("x"))
print("  → pickle 认的是 `__module__ + __qualname__` 这对**名字**，不是类对象本身：")
print("    上面两条的类是同一套代码造出来的，唯一区别是有没有这个名字能被找回去。")
print("    自定义 __init__ 若改了 super().__init__() 收的参数个数，往返也可能崩")
print("    （重建时只会把 args 当位置参数原样再喂一次）。要跨进程就**保持一致**。")

print("\n=== 6. 三个反面：能被接住但没人会去看 ===")
print("  ① raise Exception('出错') —— 类型信息为 0，调用方只能接 Exception 这个大筐，")
print("     于是把 MemoryError 之外的所有错都当同一件事处理（第 3 节那张家谱白画了）。")
print("  ② 自定义异常里再 raise 一个别的类型 —— 链会变长（第 5 节），")
print("     调用方接的是**最后一个**，前面那串只在 traceback 里当背景。")
print("  ③ 消息只写一个 'failed' —— 出错时最需要的是**上下文**：哪个文件、哪个键、什么值。")
print("     对比第 8 节那些内置报错原文：它们都带名字、带类型、带位置。你的报错也该带。")
print("  正解示例（现场跑一遍，看消息长什么样）：")


def load(section, available):
    if section not in available:
        raise ConfigMissing(
            f"配置段 {section!r} 不存在；可用的有 {sorted(available)}", code="E-CFG-001")
    return available[section]


try:
    load("db", {"server": 1, "log": 2})
except ConfigMissing as exc:
    print("   ", type(exc).__name__, "(E-CFG-001):", exc)

print("\n=== 7. 收口 ===")
print("  自定义异常的成本只有三行：类 + 继承 + 需要 payload 时 super().__init__(message) 加一个字段。")
print("  收益有三条：调用方能按类型分派（第 4 节）、能一网打尽（宽父亲）、")
print("     报错消息里能带上下文（上面 load() 那条就是内置报错的水平）。")
