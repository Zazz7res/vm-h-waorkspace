"""第 02 章第 9 节：字符串驻留（intern）—— 哪些字符串会被偷偷共用。

跑法：python3 h_intern.py

「驻留」= 同一个字符串值只存一份，多个名字共用一个对象，于是 `is` 也能碰巧为真。
CPython 会在**编译期**把「看起来像标识符」的字符串字面量自动驻留。
本节每个字面量都单独 compile 一次再执行，避开「同一份代码里的常量表去重」这个假象
（那只是一份代码内共用了常量槽位，不是跨代码的驻留）。
"""
import sys


def via_compile(text):
    """把字符串当成一个字面量，现场编译成一份独立代码再执行，返回那个对象。"""
    ns = {}
    exec(compile("x = %s" % repr(text), "<独立代码>", "exec"), ns)
    return ns["x"]


print("=" * 68)
print("一、哪些字面量被自动驻留（每行都是两段独立编译的代码产物）")
print("=" * 68)
candidates = [
    "foo_bar",      # 字母 + 下划线
    "abc_def",      # 同上，换个值
    "1abc",         # 数字开头也算「标识符样子」
    "_x",           # 下划线开头
    "foo bar",      # 带空格
    "Foo.Bar",      # 带点
    "café_ok",      # 含非 ASCII 字母
    "a b",          # 带空格，短
]
for cand in candidates:
    a = via_compile(cand)
    b = via_compile(cand)
    print("%-12r 两段独立代码给出同一个对象？ %s" % (cand, a is b))

print()
print("本机测出来的规则：全是 ASCII 字母 / 数字 / 下划线 → 驻留；")
print("带空格、带点、含非 ASCII 字母 → 不驻留。")
print("（空串 \"\" 也恒 True，但那是因为它自己是个单例，不能当驻留规则的证据。）")

print()
print("=" * 68)
print("二、运行时拼出来的字符串不驻留，sys.intern 手动驻留")
print("=" * 68)
literal = via_compile("foo_bar")
built = "".join(["foo", "_", "bar"])
print("运行时拼出来的和字面量同一个对象吗？", built is literal)
print("两个值相等吗？", built == literal)
interned = sys.intern(built)
print("sys.intern 之后同一个对象了吗？", interned is literal)
print("sys.intern 是幂等的：再驻留一次还是它自己吗？", sys.intern(interned) is interned)

print()
print("=" * 68)
print("三、同一份代码里两个相同字面量：那是常量表去重，不是驻留")
print("=" * 68)
code = "x = 'a b'\ny = 'a b'\nz = x is y"
ns = {}
exec(compile(code, "<同一代码>", "exec"), ns)
print("同一份代码里两个 'a b' 值相等吗：", ns["x"] == ns["y"])
print("同一份代码里两个 'a b' 是同一个对象吗：", ns["z"])
print("两段独立代码里的 'a b' 是同一个对象吗：", via_compile("a b") is via_compile("a b"))
print("→ 差别就在这：常量表去重只在一份代码内有效；跨代码还共用的，才是被驻留过的。")

print()
print("=" * 68)
print("四、属性名走的就是驻留（解释器为了让 dict 查找快才这么干的）")
print("=" * 68)


class Holder:
    pass


h = Holder()
h.foo_bar = 1
key = list(h.__dict__)[0]
print("h.__dict__ 里那个键：", repr(key))
print("它和字面量 'foo_bar' 是同一个对象吗？", key is via_compile("foo_bar"))
print("→ 别在业务代码里靠这个：它是 CPython 的实现细节，不是语言保证。")

print()
print("=" * 68)
print("五、驻留的用处是快，不是正确性")
print("=" * 68)
words = ["ok", "ok", "ok"]
same_all = words[0] is words[1] is words[2]
print("同一个 list 里三个 'ok' 是同一个对象吗？", same_all)
print("读文件 / input() / 算出来的字符串一律不驻留，所以：")
print("  判内容一律写 ==；is 只留给 None 和「就要问是不是同一个东西」的场合。")
