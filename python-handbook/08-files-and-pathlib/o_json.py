#!/usr/bin/env python3
# ============================================================================
# 取证脚本 o_json.py —— 第 08 章第 16 节：json 的类型表、报错原文，以及它不保存类型
#
# 这个脚本只证一件事：JSON 是**值**的格式，不是**类型**的格式。
#   ① 它的世界只有六种值（对象/数组/字符串/数/true-false/null）。Python 的 tuple、set、
#      bytes、datetime 出了这扇门就回不来：tuple 被降级成 list（悄悄回来、类型已变），
#      其余当场 TypeError —— 两种「回不来」的形状完全不同，必须都亲眼看过。
#   ② 报错不是玄学：JSONDecodeError 带 lineno / colno / pos 三个结构化字段，
#      而且它是 ValueError 的儿子（第 07 章那棵家谱树）。
#   ③ load/dump 吃的是**流**：文件指针会前移，第二次 load 不是「再读一遍」。
# 现场全在 json-demo/ 里现造现删；故意失败一律接住、原样打印 f"{type(e).__name__}: {e}"。
# 屏幕上不打绝对路径、不打时间、不打内存；dict 一律 sort_keys=True，保证两遍逐字一致。
# ============================================================================
import json
import math
import shutil
from datetime import date
from pathlib import Path

WORK = Path("json-demo")


def one(value, **kw):
    """dumps 的薄封装：默认 sort_keys（不统一就没有可比性），失败把原文交回。"""
    kw.setdefault("sort_keys", True)           # 允许调用点显式关掉，否则这里会撞重复关键字
    try:
        return json.dumps(value, **kw)
    except (TypeError, ValueError) as exc:
        return f"★{type(exc).__name__}: {exc}"


def back(text):
    """loads 的薄封装：报错原文当字符串交回，屏幕上永远只有一行。"""
    try:
        return json.loads(text)
    except json.JSONDecodeError as exc:
        return f"★{type(exc).__name__}: {exc}"


class Point:
    """自定义类：json 天生不认识它。用来演 default= / object_hook= 这对**自己约定**的钩子。"""

    def __init__(self, x, y):
        self.x, self.y = x, y

    def __repr__(self):
        return f"Point({self.x}, {self.y})"

    def __eq__(self, other):
        return isinstance(other, Point) and (self.x, self.y) == (other.x, other.y)


def point_default(obj):
    """写侧钩子：__kind__ 这个键名是我们自己起的，JSON 规范里没有这一说。"""
    if isinstance(obj, Point):
        return {"__kind__": "Point", "x": obj.x, "y": obj.y}
    raise TypeError(f"没有为 {type(obj).__name__} 准备规则")


def point_hook(dct):
    """读侧钩子：每个 JSON 对象都会经过这里；不认识的原样返回，别乱吞。"""
    if dct.get("__kind__") == "Point":
        return Point(dct["x"], dct["y"])
    return dct


print("=== 1. 类型往返表：进得去也回得来的，只有这七种 ===")
if WORK.exists():                        # 上次崩溃留下的残骸：先清干净，保证每次同一现场
    shutil.rmtree(WORK)
WORK.mkdir()
for v in (None, True, 1, 1.5, "s", [1, 2], {"a": 1}):
    text = one(v)
    got = back(text)
    print(f"  进 {type(v).__name__:<9} {v!r:<10} → {text:<12} → 出 {type(got).__name__:<9}"
          f" {got!r:<12} 相等={got == v}")
text_t = one((1, 2))
got_t = back(text_t)
print(f"  ★ tuple：进 tuple (1, 2) → {text_t} → 出 {type(got_t).__name__} {got_t!r}"
      f" 相等={got_t == (1, 2)}")
print("  → 判据：tuple 这一行**不报错**，但类型换了 —— 比报错危险，你到算错都不知道出错。")

print("\n=== 2. 撞 TypeError 的名单，以及「键必须是字符串」这条硬规矩 ===")
for v in ({1, 2}, b"ab", bytearray(b"x"), complex(3, 4), date(2026, 10, 3), Point(1, 2)):
    print(f"  进 {type(v).__name__:<10} {v!r:<26} → {one(v)}")
int_key = {1: "甲", 2: "乙"}
got_int_key = back(one(int_key))
print(f"  进 dict 键是 int：{one(int_key)} → 回来 {got_int_key}"
      f" 键的类型 {[type(k).__name__ for k in got_int_key]}")
print("  → ★能序列化，但键回来看是 str：int 键不报错，这是最容易被当成「没事」的一条。")
tuple_key = {(1, 2): "键"}
mixed = {(1, 2): "键", 1: "int键"}
print(f"  进 dict 键是 tuple {tuple_key!r} → {one(tuple_key)}")
print(f"  同一个加 skipkeys=True（不排序）→ {one(mixed, skipkeys=True, sort_keys=False)}"
      f"  ← 不认识的键**直接消失**")
print(f"  但 skipkeys=True + sort_keys=True 在同一份数据上 → {one(mixed, skipkeys=True)}")
print("    ← 键排序发生在写之前，混合类型的键先撞死在 sorted() 上（这条报错和 json 无关）")
print("  → 判据：dumps 只接受 str/int/float/bool/None 五种键（后三种写出来都是字符串）；")
print("    超出的一律 TypeError。skipkeys=True 是「把不能表达的悄悄扔掉」，别拿它当兜底。")

print("\n=== 3. ensure_ascii：默认全 \\uXXXX，ASCII 安全，代价是长度 ===")
cn = {"名字": "张三", "备注": "预算"}
ascii_on = one(cn, ensure_ascii=True)
ascii_off = one(cn, ensure_ascii=False)
print(f"  默认（True）      → {ascii_on}")
print(f"  ensure_ascii=False → {ascii_off}")
print(f"  字符数 len()：{len(ascii_on)} vs {len(ascii_off)}"
      f" ｜ 落盘字节数 encode('utf-8')：{len(ascii_on.encode('utf-8'))} vs {len(ascii_off.encode('utf-8'))}")
print(f"  两份 loads 回来相等吗 → {back(ascii_on) == back(ascii_off)}（\\u5f20 和「张」是同一个字符）")
print("  → 判据：ensure_ascii=True 让输出**只用 ASCII 字符**，塞进任何不明管道都不会变乱码；")
print("    代价就在上面那行数字里：一个中文写成 \\uXXXX 是 6 个 ASCII 字符。文件自己已经写了 encoding=，落盘用 False；")
print("    只有「不知道下游什么编码」的场合（邮件头、老 HTTP 头、日志采集）才值回 True。")

print("\n=== 4. 排版三开关：indent / sort_keys / separators（同一份 dict）===")
doc = {"name": "取证", "tags": ["b", "a"], "meta": {"z": 1, "a": 2}}
print(f"  ① 默认 → {one(doc)!r}")
print(f"  ② separators=(',',':') → {one(doc, separators=(',', ':'))!r}   ← 最紧凑，机器间传输用")
print("  ③ indent=2 + sort_keys=True（人读的那种写法，键按 sorted 落定，diff 才不会假变更）：")
for line in one(doc, indent=2, sort_keys=True).splitlines():
    print(f"      {line}")
print(f"  sort_keys=False 的键序 = 插入序（b 先进去）→ {one({'b': 1, 'a': 2}, sort_keys=False)!r}"
      f" ｜ True → {one({'b': 1, 'a': 2}, sort_keys=True)!r}")
print("  ★ indent 会顺手改写 separators 的默认值：不带 indent 时项之间是 ', '，带了就只剩 ','：")
print(f"    不带 indent → {one({'a': 1, 'b': 2})!r}")
print(f"    带 indent=2 → {one({'a': 1, 'b': 2}, indent=2)!r}   ← 冒号后那个空格没变，变的是逗号后面")
print("    所以风格要固定就显式写 separators=，别指望两处默认值一致（第 07 章那条：默认值是别人的决定）。")

print("\n=== 5. default= 与 object_hook=：类型得自己带回来，而协议是你自己定的 ===")
text_p = one(Point(1, 2), default=point_default)
print(f"  【回不来】不用钩子：{one(Point(1, 2))}")
print(f"  【能写出去】写侧 default= 返回一个 json 认识的东西 → {text_p}")
loose = back(text_p)
print(f"  【回不来】loads 不传钩子 → {type(loose).__name__} {loose!r}"
      f" ｜ == Point(1, 2) 吗 → {loose == Point(1, 2)}")
strict = json.loads(text_p, object_hook=point_hook)
print(f"  【能回来】loads 传 object_hook= → {type(strict).__name__} {strict!r}"
      f" ｜ == Point(1, 2) 吗 → {strict == Point(1, 2)}")
seen = []
json.loads('{"a": {"b": 1}, "c": 2}', object_hook=lambda d: (seen.append(sorted(d)), d)[1])
print(f"  object_hook 是对**每一个**对象都调一次（含嵌套）：收到 {len(seen)} 个 dict → {seen}")
print("  ★ 这两栏的差别不在 json 的能力里，在你的代码里：__kind__ 这个键名、x/y 这两个字段")
print("    全是我们自己约定的，JSON 规范一个都没规定 —— 这是**你发明的协议**，")
print("    换一门语言读它的人必须读你的文档才知道。协议要当接口管，写进文档、加版本。")

print("\n=== 6. 三样报错原文 + JSONDecodeError 的家谱（lineno / colno / pos 是三个字段）===")
for bad, why in (('{"a": 1,}', "多余逗号"), ("{'a': 1}", "用了单引号"), ('{"a":', "文档被截断")):
    try:
        json.loads(bad)
        print(f"  {why:<6} 输入 {bad!r} → 竟然解析成功了（那这条不算坑）")
    except json.JSONDecodeError as exc:
        print(f"  {why:<6} 输入 {bad!r}")
        print(f"      {type(exc).__name__}: {exc}")
        print(f"      e.msg={exc.msg!r} ｜ e.lineno={exc.lineno} e.colno={exc.colno}"
              f" e.pos={exc.pos} ← pos 直接拿去切片就能指出错在哪一格")
print(f"  issubclass(json.JSONDecodeError, ValueError) → {issubclass(json.JSONDecodeError, ValueError)}")
print("  __mro__ →", " → ".join(c.__name__ for c in json.JSONDecodeError.__mro__))
print("  → 所以本脚本第 1、2 段那些 one()/back() 用 except (TypeError, ValueError) 就够接住它，")
print("    但也**只**接住它：except Exception 会顺手把 MemoryError 之外的一整棵树都咽掉（第 07 章）。")
print(f"  顺带一条：json 不认识 Python 的字面量。loads('null') → {back('null')!r}，"
      f"loads('None') → {back('None')!r} ← 配置文件里手打 None 是最常见的「读不进来」")

print("\n=== 7. dump/load 吃的是流：指针前移，第二次 load 不是重读一遍 ===")
p_cfg = WORK / "cfg.json"
p_cfg.write_text(json.dumps({"a": 1, "b": [1, 2]}, sort_keys=True), encoding="utf-8")
with open(p_cfg, encoding="utf-8") as fh:
    first = json.load(fh)
    print(f"  第一次 json.load → {first}  ｜ 读完 fh.tell()={fh.tell()}（指针已经到末尾）")
    try:
        json.load(fh)
    except json.JSONDecodeError as exc:
        print(f"  第二次 json.load（同一个句柄）→ {type(exc).__name__}: {exc}")
    fh.seek(0)
    print(f"  seek(0) 之后第三次 → {json.load(fh)}  ← 内容没坏，坏的是你以为指针会自己回去")
print("  → 判据：'Expecting value: line 1 column 1 (char 0)' 出现在**第二次** load，")
print("    不是文件坏了，是读到了末尾。要么 seek(0)，要么 load 一次存进变量。")
print(f"  分工一句话：json.dump(obj, fh) 写流 ｜ json.dumps(obj) 返 str ｜ 本机这份文件 "
      f"{p_cfg.stat().st_size} 字节 = dumps 的 {len(one({'a': 1, 'b': [1, 2]}))} 字符"
      f"（纯 ASCII 时两者才相等，含中文就别当同一回事）")

print("\n=== 8. 数值精度与三个 inf 雷 ===")
big = 2 ** 62
got_big = back(one(big))
print(f"  大整数 {big} → {got_big!r} 类型 {type(got_big).__name__}"
      f" ｜ 相等={got_big == big}（没被变成 float）")
print(f"  带不带指数就不一样：loads '100' → {back('100')!r} {type(back('100')).__name__}"
      f" ｜ loads '1e2' → {back('1e2')!r} {type(back('1e2')).__name__}")
inf999 = back("1e999")
print(f"  1e999（超出 float 上限）：loads → {inf999!r} ｜ math.isinf → {math.isinf(inf999)}"
      f"  ← Python 不报错，直接给你一个 inf")
print(f"  Python 认常量的名字：loads 'NaN' → {back('NaN')!r} ｜ loads 'Infinity' → {back('Infinity')!r}")
print(f"  ★ 反向就出事：dumps(inf) → {one(float('inf'))} ｜ dumps(nan) → {one(float('nan'))}"
      f" ← 这两个**不是合法 JSON**，别的语言一读就崩")
print(f"  parse_constant 是唯一的拦截口：loads '[NaN, Infinity]' 带钩子 → "
      f"{json.loads('[NaN, Infinity]', parse_constant=lambda name: None)!r}"
      f"（钩子收到常量名，返回你允许的替身）")
print(f"  浮点原样进原样出：dumps(0.1+0.2) → {one(0.1 + 0.2)} ｜ loads 回来 == 0.1+0.2 → "
      f"{back(one(0.1 + 0.2)) == 0.1 + 0.2}")
print("  → 判据：NaN/Infinity 是 Python 的宽容，不是 JSON 的功能；跨语言传数就先把 inf 挡在门外。")

print("\n=== 9. 收口：一张类型表 + 三个字段 + 三个雷 ===")
print("  ① 类型表：None↔null、bool↔true/false、int/float↔数、str↔字符串、list↔数组、dict↔对象；")
print("     tuple **降级成 list**（不报错、回不来），set / bytes / datetime / 自定义类当场 TypeError；")
print("     键只认 str/int/float/bool/None，回来一律变 str，skipkeys=True 会把不认识的键丢掉。")
print("  ② 报错三字段：lineno / colno / pos（外加 msg 和 doc）。它是 ValueError 的儿子 ——")
print("     所以配置读失败时别只打一句「json 错了」，把这三个数打出来，一行就能定位到人。")
print("  ③ 三个雷：ensure_ascii 默认让中文体积翻倍以上；load 两次不 seek 必失败；")
print("     inf/nan 能被 Python 写出去，但那是坏 JSON（1e999 早在 loads 阶段就变成 inf 了）。")
print("  ★ 配置文件里别用 json 存你需要跨语言校验的东西 —— 除非你接受类型丢失，")
print("     并且把 default= / object_hook= 那套自己发明的协议写进文档、当成接口来管。")

if WORK.exists():
    shutil.rmtree(WORK)
print(f"\n  清理：{WORK.name}/ 已删除 → 还在吗 {WORK.exists()}"
      f" ｜ 当前目录里 *-demo 残留 = "
      f"{sorted(p.name for p in Path('.').iterdir() if p.name.endswith('-demo')) or '无'}")
