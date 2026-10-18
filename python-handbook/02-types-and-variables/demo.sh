#!/usr/bin/env bash
# ============================================================================
# 第 02 章《变量是标签不是盒子》—— 整章证据一键复现
#
#   bash demo.sh                 跑完自动删沙箱（默认）
#   bash demo.sh --keep          跑完保留沙箱并打印路径，进去自己接着敲
#   bash demo.sh > out.txt 2>&1  重定向时自动关颜色，得到和本章 README 一样的纯文本
#
# 屏幕上出现的每一行，都是这条命令在这台机器（CPython 3.14.4 / Ubuntu）上的真实结果，
# 没有一行是手写的、没有一行是「我记得应该这样」。
#
# 安全说明：
#   * 示例源码整份复制进 mktemp 出来的沙箱再运行，所以本手册目录里**不会**留下
#     __pycache__/、*.pyc、gcc 出来的可执行文件（第 23 节当场用 find 自证为 0）。
#   * 只用标准库 + 系统自带的 gcc（第 1 节的 C 对照组）；不 pip install、不联网、
#     不改 ~/.bashrc / ~/.profile / ~/.config / ~/.local。
#   * 故意不加 `set -e`：本章有一半是「失败现场」（UnboundLocalError、解包数量不对、
#     给 str 按下标赋值、-W error 下的 SyntaxError），必须让报错原样打出来并继续往下跑。
#   * 也故意不加 `set -o pipefail`：加了它之后 `cmd | tail -6` 会因为 tail 提前关管道
#     而报 141，屏幕上会冒出一堆和教学无关的退出码。
#   * 屏幕上不主动打印绝对路径：需要「在某个目录里跑」时用 runc_at，只留目录 basename。
#     （Python 自己的 traceback 里会带沙箱全路径 —— 那是 3.9 起 `__file__` 变绝对的
#      解释器行为，不在本脚本控制范围内；形如 /tmp/py02-XXXXXX，随机后缀每次不同，
#      README 里把它归一化成 <沙箱>，除此之外逐行可复现。）
# ============================================================================

KEEP=0
for arg in "$@"; do
    case "$arg" in
        --keep) KEEP=1 ;;
        -h|--help) echo "用法: bash demo.sh [--keep]"; exit 0 ;;
        *) echo "未知参数：$arg（本脚本只认 --keep）" >&2; exit 2 ;;
    esac
done

set -u

# 固定语言：Python 自己的输出恒为英文，但 `command not found` 这类消息是 bash 翻译的；
# 设成 C.UTF-8 后整份 transcript 才能逐字比对（你手敲时看到中文完全正常）。
export LC_ALL=C.UTF-8
export LANG=C.UTF-8
# 清掉可能干扰结论的环境变量（只影响本脚本这个进程和它的子进程，不碰你的配置文件）。
unset PYTHONPATH PYTHONSTARTUP PYTHONOPTIMIZE PYTHONINSPECT PYTHONWARNINGS
# 哈希随机化会影响 set 的遍历顺序、进而影响 id 的复用运气；本章只比成员关系不比顺序，
# 这里显式固定成 0，是为了让同一份脚本在你机器上打出同样的输出。
export PYTHONHASHSEED=0

# 只有输出到终端才上色；重定向到文件时是干净纯文本
if [ -t 1 ]; then B=$'\033[1m'; C=$'\033[36m'; R=$'\033[0m'; else B=''; C=''; R=''; fi

HERE=$(cd "$(dirname "$0")" && pwd)
SB=$(mktemp -d "${TMPDIR:-/tmp}/py02-XXXXXX")
export SB

banner() { printf '\n%s══════ %s %s\n' "$B" "$1" "══════════════════════════════════$R"; }
note()   { printf '%s\n' "$1"; }

# 打印命令 → 执行（stderr 合并进来）→ 打印退出码
runc() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    bash -c "$1" 2>&1
    printf '[退出码 %d]\n' "$?"
}

# runc_at <目录> <命令>：先 cd 进那个目录再跑，屏幕上只留目录 basename + 相对命令。
runc_at() {
    printf '\n%s$ 〔在 %s 里〕 %s%s\n' "$C" "$(basename "$1")" "$2" "$R"
    ( cd "$1" && eval "$2" ) 2>&1
    printf '[退出码 %d]\n' "$?"
}

cleanup() {
    if [ "$KEEP" -eq 1 ]; then
        printf '\n[沙箱已保留] %s\n  进去接着敲： cd "%s/work"\n  看完自己删： rm -rf "%s"\n' \
               "$SB" "$SB" "$SB"
    else
        rm -rf "$SB"
        printf '\n[沙箱已删除] %s\n' "$SB"
    fi
}
trap cleanup EXIT

# ============================================================================
banner "0. 环境体检：本章结论只在这么一个解释器上验过"
# ============================================================================
mkdir -p "$SB/work"
cp -R "$HERE"/. "$SB/work"/
cd "$SB/work" || exit 1
note '本章的示例源码一共 17 个文件（16 个 .py + 1 个 .c），沙箱里列一下：'
runc 'ls'
runc 'python3 --version'
runc 'python3 -c "import sys; print(sys.version)"'
runc 'python3 -c "import sys; print(sys.prefix, sys.base_prefix)"'
runc 'echo "LC_ALL=$LC_ALL  LANG=$LANG  PYTHONHASHSEED=$PYTHONHASHSEED"'
runc 'python3 -c "import sys; print(sys.getfilesystemencoding(), sys.stdout.encoding)"'
note '↑ 本章不碰文件名编码，固定 LC_ALL 只是想让 bash 自己的报错语言可预测。'
runc 'python3 -c "import sys; print(sys.int_info)"'
runc 'python3 -c "import sys; print(sys.getsizeof(0), sys.getsizeof(1), sys.getsizeof(2**30))"'

# ============================================================================
banner "1. 对照组：C 语言里变量真是盒子（gcc 现编现跑）"
# ============================================================================
runc 'gcc -O0 -Wall -o a_c_box a_c_box.c'
runc './a_c_box'
note '↑ 记住这四件事：赋值是倒一份值、sizeof 编译期钉死、溢出不会自动变长、'
note '  想让两个名字指同一块内存得自己上指针。第 2 节开始全是它的反面。'
runc 'python3 -c "import sys; print(\"C 的 int 32 位装不下 2**63；Python 的 int 想多大多大：\", 2**63, 2**100)"'

# ============================================================================
banner "2. 场景 1：a = 1; b = a; a = 2 —— b 是什么？"
# ============================================================================
runc 'python3 -c "a = 1; b = a; a = 2; print(b)"'
runc 'python3 -c "a = 1; b = a; a = 2; print(id(a) == id(b))"'
runc 'python3 -c "a = [1]; b = a; a.append(2); print(b, id(a) == id(b))"'
runc 'python3 -c "a = [1]; b = a; a = [1, 2]; print(b, id(a) == id(b))"'
note '↑ 第 3 条和第 4 条就是本节全部重点：a.append 改对象（b 跟着变），'
note '  a = [...] 换标签（b 留在原地）。C 的直觉只能解释第 4 条，解释不了第 3 条。'
runc 'python3 b_name_object.py'

# ============================================================================
banner "3. 场景 1 续：赋值编译成什么，那张名字表在哪"
# ============================================================================
runc 'python3 -c "import dis; dis.dis(compile(\"a = 1\", \"<s>\", \"exec\"))"'
runc 'python3 -c "x = 5; print(type(globals()).__name__, \"x\" in globals(), globals()[\"x\"])"'
runc 'python3 -c "x = 5; print(globals() is vars())"'
runc 'python3 -c "x = 5; del x; print(\"x\" in globals())"'
runc 'python3 -c "import dis; dis.dis(compile(\"def f():\n    a = 1\n    return a\", \"<s>\", \"exec\"))"'
note '↑ STORE_NAME（查 dict）vs STORE_FAST（帧里的槽）：01 章场景 2 埋的那句话在这里兑现。'
runc 'python3 c_assign_dis.py'

# ============================================================================
banner "4. 场景 2：可变 vs 不可变（list 原地改 / int 和 str 只能换对象）"
# ============================================================================
runc 'python3 -c "xs = [1, 2]; ys = xs; xs.append(3); print(ys)"'
runc 'python3 -c "xs = [1, 2]; ys = xs; ys[0] = 9; print(xs)"'
runc 'python3 -c "s = \"abc\"; print(s.upper() == s, id(s.upper()) == id(s))"'
runc 'python3 -c "s = \"abc\"; s[0] = \"X\""'
note '↑ 最后一条就是「不可变」的官方说法，报错原文值得逐字读（退出码 1）。'
runc 'python3 -c "t = (1, [2]); t[1].append(3); print(t)"'
runc 'python3 -c "print(hash((1, 2)) == hash((1, 2)))"'
runc 'python3 -c "hash((1, [2]))"'
runc 'python3 d_mutable.py'

# ============================================================================
banner "5. 场景 3：+= 对 list 和 str 是两种命运（__iadd__）"
# ============================================================================
runc 'python3 -c "xs = [1, 2]; before = id(xs); xs += [3]; print(id(xs) == before)"'
runc 'python3 -c "s = \"ab\"; before = id(s); s += \"c\"; print(id(s) == before, s)"'
runc 'python3 -c "t = (1, 2); before = id(t); t += (3,); print(id(t) == before, t)"'
runc 'python3 -c "import dis; dis.dis(compile(\"xs += [3]\", \"<s>\", \"exec\"))"'
note '↑ 同一个 += 号，三种命运：list 原地、str 换对象、tuple 换对象。'
runc 'python3 e_iadd.py'

# ============================================================================
banner "6. 场景 4：tuple 里装 list —— 不可变管到哪一层"
# ============================================================================
runc 'python3 -c "t = (1, [10]); inner = id(t[1]); t[1].append(20); print(t, id(t[1]) == inner)"'
runc 'python3 -c "t = (1, [10]); t[1] += [20]; print(t)"'
note '↑ 上面这条退出码 1，而且 print 根本没跑；但用 try 抓住异常之后再打，会发现 list 真的被改了（下面脚本）。'
runc 'python3 f_tuple_inner.py'

# ============================================================================
banner "7. 场景 5：小整数缓存，边界自己扫出来"
# ============================================================================
runc 'python3 -c "n = 100; print(n + 0 is n + 0)"'
runc 'python3 -c "n = 300; print(n + 0 is n + 0)"'
runc 'python3 -c "n = 300; print(n + 0 == n + 0)"'
runc 'python3 -c "n = -6; print(n + 0 is n + 0, n + 0 == n + 0)"'
note '↑ 必须用运行时算出来的整数：把 300 直接写成字面量，编译器就折成同一个常量了（第 8 节）。'
runc 'python3 g_small_ints.py'

# ============================================================================
banner "8. 场景 6：is vs == —— 3.14 直接警告你，-W error 还升级成错误"
# ============================================================================
runc 'python3 -c "print(256 is 256, 257 is 257)"'
runc 'python3 -c "print(\"abc\" is \"abc\", \"a b\" is \"a b\")"'
runc 'python3 -W error -c "print(256 is 256)"'
runc 'python3 -c "import sys; xs = [1]; print(id(xs) == id(xs), xs == xs)"'
runc 'python3 -c "print(1 == True, 1 is True, None is None)"'
note '↑ is 的正确用法只有三类：比 None、比单例、比「到底是不是同一个东西」。'

# ============================================================================
banner "9. 场景 7：字符串驻留 —— 哪些字符串会被共用"
# ============================================================================
runc 'python3 -c "a = \"foo_bar\"; b = \"foo\" + \"_bar\"; print(a == b, id(a) == id(b))"'
runc 'python3 -c "import sys; parts = [\"foo\", \"_bar\"]; a = \"\".join(parts); b = \"foo_bar\"; print(sys.intern(a) is b)"'
runc 'python3 -c "import sys; print(sys.intern(\"a b\") is sys.intern(\"a b\"))"'
runc 'python3 h_intern.py'

# ============================================================================
banner "10. 场景 8：bool 是 int 的儿子"
# ============================================================================
runc 'python3 -c "print(issubclass(bool, int), isinstance(True, int), type(True) is int)"'
runc 'python3 -c "print(True + 1, sum([True, False, True]), True == 1)"'
runc 'python3 -c "one_is = 1; print(one_is is True, one_is == True)"'
runc 'python3 -c "d = {1: \"甲\", True: \"乙\"}; print(d, len(d))"'
runc 'python3 -c "d = {1: \"甲\", 1.0: \"乙\"}; print(d, len(d))"'
runc 'python3 i_bool.py'

# ============================================================================
banner "11. 场景 9：None 是单例，而且「空」不等于 None"
# ============================================================================
runc 'python3 -c "print(None is None, type(None).__name__, bool(None))"'
runc 'python3 -c "print(None == 0, None == False, None == \"\")"'
runc 'python3 -c "import copy; print(copy.copy(None) is None)"'
runc "python3 -c \"
def forget():
    pass
print(repr(forget()), forget() is None)
\""
runc 'python3 j_none.py'

# ============================================================================
banner "12. 场景 10：NaN 跟谁都不相等（包括它自己）"
# ============================================================================
runc 'python3 -c "x = float(\"nan\"); print(x == x, x != x, id(x) == id(x))"'
runc 'python3 -c "import math; print(math.isnan(float(\"nan\")), math.isnan(0.0))"'
runc 'python3 -c "x = float(\"nan\"); print([x].count(x), x in [x])"'
runc 'python3 -c "print(float(\"nan\") == float(\"nan\"))"'
runc 'python3 k_nan.py'

# ============================================================================
banner "13. 场景 11：0.1 + 0.2 != 0.3，以及 1 == 1.0 为什么成立"
# ============================================================================
runc 'python3 -c "print(0.1 + 0.2 == 0.3)"'
runc 'python3 -c "print(format(0.1 + 0.2, \".20f\"))"'
runc 'python3 -c "import math; print(math.isclose(0.1 + 0.2, 0.3, rel_tol=1e-09))"'
runc 'python3 -c "import decimal; print(decimal.Decimal(\"0.1\") + decimal.Decimal(\"0.2\") == decimal.Decimal(\"0.3\"))"'
runc 'python3 -c "print(1 == 1.0, id(1) == id(1.0))"'
runc 'python3 -c "print(float(2**53 + 1) == float(2**53), 2**53 + 1 == float(2**53))"'
note '↑ 最后两条的差别是本章和 C 最实在的分工：先转 float 会撞车，int 直接比不撞。'
runc 'python3 l_float_tower.py'

# ============================================================================
banner "14. 场景 12：类型挂在对象上；混合 list；int 无限精度"
# ============================================================================
runc 'python3 -c "x = 1; print(type(x).__name__); x = \"换\"; print(type(x).__name__)"'
runc 'python3 -c "print(type(1) is int, isinstance(True, int), type(True) is int)"'
runc 'python3 -c "import sys; print([sys.getsizeof(v) for v in (0, 1, 2**30, 2**60)])"'
runc 'python3 -c "print(2**200)"'
runc 'python3 -c "xs = [1, \"甲\", 3.0, None]; print([type(v).__name__ for v in xs])"'
runc 'python3 -c "print(2**53 + 1 > 2**53, 2**200 + 1 != 2**200)"'
note '↑ 最后一条对照 C：int64 加一在 C 里要么溢出要么未定义，这里永远是真。'
runc 'python3 m_types_int.py'

# ============================================================================
banner "15. 场景 13：赋值语义全家桶（多重赋值 / 交换 / 解包）"
# ============================================================================
runc 'python3 -c "a = b = []; a.append(1); print(b, a is b)"'
runc 'python3 -c "a, b = 1, 2; a, b = b, a; print(a, b)"'
runc 'python3 -c "import dis; dis.dis(compile(\"a, b = b, a\", \"<s>\", \"exec\"))"'
runc 'python3 -c "import dis; dis.dis(compile(\"a = b\nb = a\", \"<s>\", \"exec\"))"'
runc 'python3 -c "first, *rest = [1, 2, 3, 4]; print(first, rest)"'
runc 'python3 -c "*head, last = (1, 2, 3); print(head, last)"'
runc 'python3 -c "x, y = 1, 2, 3"'
runc 'python3 -c "x, y = 5"'
runc 'python3 -c "*a, *b = [1, 2]"'
runc 'python3 n_assign_pack.py'

# ============================================================================
banner "16. 场景 14：del 删的是名字，不是对象（引用计数一格一格数）"
# ============================================================================
runc 'python3 -c "import sys; xs = [1]; print(sys.getrefcount(xs))"'
runc 'python3 -c "import sys; xs = [1]; ys = xs; print(sys.getrefcount(xs))"'
runc 'python3 -c "xs = [1]; ys = xs; del xs; print(ys)"'
runc 'python3 -c "xs = [1]; del xs; print(xs)"'
note '↑ 倒数第二条证明 del 删的是名字（ys 还好好指着对象）；最后一条 NameError，退出码 1。'
runc 'python3 o_refcount_del.py'

# ============================================================================
banner "17. 场景 14 续：引用计数拆不开的环，靠分代 GC 兜底"
# ============================================================================
runc 'python3 -c "import gc; print(gc.get_threshold(), gc.isenabled())"'
runc "python3 -c \"
import gc, weakref
class Node: pass
gc.disable()                      # 先关掉分代 GC，只剩引用计数
a, b = Node(), Node()
a.peer, b.peer = b, a             # 互相指着
ra, rb = weakref.ref(a), weakref.ref(b)
del a, b
print('关了 gc、del 两个名字之后，环还活着吗：', ra() is not None or rb() is not None)
gc.enable()
gc.collect()
print('gc.collect() 之后没了？', ra() is None and rb() is None)
\""
runc 'python3 -c "import gc; print(gc.collect() >= 0)"'
note '↑ gc.collect() 返回的「回收了几个」包含标准库自己的环，每次都不同，所以这里只判非负。'

# ============================================================================
banner "18. 场景 15：作用域最小版 —— UnboundLocalError 报错原文逐行读"
# ============================================================================
runc "python3 -c \"
count = 0
def bump():
    print(count)
    count = count + 1
bump()
\""
note '↑ 上面这条的退出码是 1，traceback 的每一行都有含义，README 第 18 节逐行拆。'
runc 'python3 p_scope_min.py'

# ============================================================================
banner "19. 场景 15 续：global / nonlocal 只演示存在（深入留给第 05 章）"
# ============================================================================
runc "python3 -c \"
count = 0
def good():
    global count
    count = count + 1
    return count
print(good(), count)
\""
runc "python3 -c \"
def outer():
    seen = 0
    def inner():
        nonlocal seen
        seen += 1
        return seen
    return inner()
print(outer())
\""
runc "python3 -c \"
total = 0
def f():
    print(total)
    total = 1
f()
\" | tail -4"
runc 'python3 -c "count = 0; exec(\"count = count + 1\", globals()); print(count)"'
note '↑ 上面那条走管道的：屏幕上 [退出码 0] 是 tail 的退出码，不是 python3 的（脚本故意不加 pipefail）。'
note '↑ 最后一条是「绕过作用域」的第三种写法：直接往 globals dict 里改，能跑但没人这么写。'

# ============================================================================
banner "20. 场景 16：想复制的时候该写什么（copy / deepcopy / 切片）"
# ============================================================================
runc 'python3 -c "import copy; xs = [[1], [2]]; ys = copy.copy(xs); print(ys is xs, ys[0] is xs[0])"'
runc 'python3 -c "import copy; xs = [[1], [2]]; ys = copy.deepcopy(xs); print(ys[0] is xs[0])"'
runc 'python3 -c "xs = [[1], [2]]; ys = xs[:]; ys[0].append(9); print(xs)"'
runc 'python3 -c "import copy; print(copy.copy(5) is 5, copy.deepcopy(\"ab\") is \"ab\")"'
runc 'python3 q_copy_table.py'

# ============================================================================
banner "21. 一页纸取证：本章最常写错的八行，当场判对错"
# ============================================================================
runc 'python3 -c "x = 300; print(x + 0 is x + 0, x + 0 == x + 0)"'
runc "python3 -c \"
def append_to(target, item):
    target = target + [item]      # 换标签：外面看不见
    return target
xs = [1]
append_to(xs, 2)
print(xs)
\""
runc "python3 -c \"
def append_inplace(target, item):
    target += [item]              # 原地吸收：外面看得见
xs = [1]
append_inplace(xs, 2)
print(xs)
\""
runc 'python3 -c "xs = [1, 2]; ys = xs; ys += [3]; print(xs)"'
runc 'python3 -c "s = \"a\"; t = s; t += \"b\"; print(s)"'
runc "python3 -c \"
def f(a=[]):
    a.append(1)
    return a
print(f(), f())
\""
note '↑ 最后一条是第 05 章的可变默认参数陷阱；本章先把「默认值只贴一次标签」这个根因露个脸。'
runc "python3 -c \"
key = [1, 2]
try:
    d = {key: 1}
except TypeError as e:
    print('TypeError:', e)
\""
runc 'python3 -c "import sys; xs = [1]; xs.append(2); print(sys.getrefcount(xs) >= 1)"'

# ============================================================================
banner "22. 取证手段汇总：这一章用到的标准库入口"
# ============================================================================
runc 'python3 -c "import sys, gc, dis, copy, math, weakref, numbers, decimal, array; print(\"标准库全部可用：\", all([sys, gc, dis, copy, math, weakref, numbers, decimal, array]))"'
runc 'python3 -c "import sys; print(sys.getsizeof([1,2,3]), sys.getsizeof(\"abc\"), sys.getsizeof((1,2)), sys.getsizeof({1:2}))"'
runc 'python3 -c "import sys; print(sys.version_info)"'
runc 'python3 -c "import dis; print([op.opname for op in dis.get_instructions(compile(\"a += b\", \"<s>\", \"exec\"))])"'

# ============================================================================
banner "23. 收尾：证明这一章没在手册目录里留下任何产物"
# ============================================================================
runc_at "$HERE" "find . \\( -name '__pycache__' -o -name '*.pyc' -o -name 'a_c_box' -o -name '*.out' \\) | wc -l"
note '↑ 这个数字必须是 0：gcc 的可执行文件、__pycache__、pyc 全生在沙箱里。'
runc_at "$HERE" "ls | wc -l"
runc 'ls'
runc 'ls __pycache__'
note '↑ 最后一条退出码 2：沙箱里也没有 __pycache__ —— 这一章全程 python3 xxx.py 直接跑脚本，没 import 过任何示例模块。'
runc 'python3 -c "import os; print(sorted(d for d in os.listdir(\".\") if \"pycache\" in d or d.endswith(\".pyc\")))"'
note "沙箱位置：$SB（默认跑完就删；想留着继续敲就用 bash demo.sh --keep）"

printf '\n%s[全部跑完] 上面每一段都是真实执行结果；README.md 引用的输出全部出自这一次运行。%s\n' "$B" "$R"
exit 0
