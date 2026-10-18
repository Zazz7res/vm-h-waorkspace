#!/usr/bin/env bash
# ============================================================================
# 第 06 章《类与 dataclass：对象里到底存了什么》—— 整章证据一键复现
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
#     __pycache__/、*.pyc（最后一节当场用 find 自证为 0）。
#   * 只用标准库 + 本机自带的 python3；不 pip install、不联网、
#     不改 ~/.bashrc / ~/.profile / ~/.config / ~/.local。
#   * 故意不加 `set -e`：本章有一半是「失败现场」（__init__ 返回字符串、unhashable、
#     只读属性赋值、__slots__ 加新属性、frozen 赋值、抽象类实例化、
#     __getattribute__ 死循环），必须让报错原样打出来并继续往下跑。
#   * 也故意不加 `set -o pipefail`：加了它之后 `cmd | tail -6` 会因为 tail 提前关管道
#     而报 141，屏幕上会冒出一堆和教学无关的退出码。
#   * 屏幕上不主动打印绝对路径：需要「在某个目录里跑」时用 runc_at，只留目录 basename。
#     （Python 自己的 traceback 里会带沙箱全路径 —— 那是解释器行为，不在本脚本控制
#      范围内；形如 /tmp/py06-XXXXXX，随机后缀每次不同，README 里归一化成 <沙箱>，
#      除此之外逐行可复现。）
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
export LANGUAGE=C
# 清掉可能干扰结论的环境变量（只影响本脚本这个进程和它的子进程，不碰你的配置文件）。
unset PYTHONPATH PYTHONSTARTUP PYTHONOPTIMIZE PYTHONINSPECT PYTHONWARNINGS
# 本章比的多是「名字在不在字典里」「相等不相等」，不吃哈希顺序；这里固定成 0
# 只是为了让同一份脚本在你机器上打出尽量稳定的输出。
export PYTHONHASHSEED=0

# 只有输出到终端才上色；重定向到文件时是干净纯文本
if [ -t 1 ]; then B=$'\033[1m'; C=$'\033[36m'; R=$'\033[0m'; else B=''; C=''; R=''; fi

HERE=$(cd "$(dirname "$0")" && pwd)
SB=$(mktemp -d "${TMPDIR:-/tmp}/py06-XXXXXX")
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
banner "第 1 节 环境体检：本章结论只在这么一个解释器上验过"
# ============================================================================
mkdir -p "$SB/work"
cp -R "$HERE"/. "$SB/work"/
cd "$SB/work" || exit 1
note '本章的示例源码一共 20 个 .py 文件，沙箱里列一下（demo.sh 本身也在）：'
runc 'ls'
runc 'python3 --version && python3 -c "import sys; print(sys.version)"'
runc 'echo "LC_ALL=$LC_ALL  LANG=$LANG  LANGUAGE=$LANGUAGE  PYTHONHASHSEED=$PYTHONHASHSEED"; python3 -c "
import sys, dataclasses, inspect, abc
print(\"标准库三件套齐天：\", {\"dataclasses\", \"inspect\", \"abc\"} <= sys.stdlib_module_names)
print(\"前缀:\", sys.prefix, \"｜ 编码:\", sys.getfilesystemencoding(), sys.stdout.encoding)
"'
note '↑ 本章不碰文件名编码，固定 LC_ALL 只是想让 bash 自己的报错语言可预测。'

# ============================================================================
banner "第 2 节 类就是个字典：A.__dict__ 里到底有什么"
# ============================================================================
runc 'python3 -c "
class A:
    kind = \"A 家族\"
    def hello(self):
        return \"你好\"
print(\"type(A.__dict__).__name__ =\", type(A.__dict__).__name__)
print(\"普通 key（不含双下划线）：\", sorted(k for k in A.__dict__ if not k.startswith(\"__\")))
print(\"vars(A) is A.__dict__ 内容相同：\", dict(vars(A)) == dict(A.__dict__))
"'
runc 'python3 -c "
class A:
    kind = \"A 家族\"
try:
    A.__dict__[\"extra\"] = 1
except TypeError as e:
    print(\"type(e).__name__ =\", type(e).__name__)
    print(\"原文：\", e)
"'
note '↑ 上面第二条：类字典其实是 mappingproxy，只读门面，往里写会 TypeError——但改 A.__dict__ 不行，改 A.kind 却随便（第 3 节）。'
runc 'python3 a_class_dict.py'

# ============================================================================
banner "第 3 节 属性查找：先翻实例字典，再翻类字典 —— del 掉就露出来"
# ============================================================================
runc 'python3 -c "
class A:
    tag = \"类上的\"
a = A()
print(\"没遮挡时 a.tag：\", a.tag)
a.tag = \"实例上的\"
print(\"遮挡后 a.tag：\", a.tag, \"｜ A.tag：\", A.tag)
print(\"实例字典里现在有：\", sorted(vars(a)))
del a.tag
print(\"del 之后 a.tag：\", a.tag, \"｜ vars(a)：\", sorted(vars(a)))
"'
note '↑ 三条命令一个故事：查 a.tag 先翻 vars(a)，没有再翻 vars(A)；同名一存就遮住，del 掉又露出来。'
runc 'python3 -c "
class A:
    tag = \"类上的\"
one, two = A(), A()
one.tag = \"只有 one 改\"
print(\"two.tag 被带了吗：\", two.tag)
print(\"A.tag 被带了吗：\", A.tag)
print(\"one 的实例字典：\", sorted(vars(one)), \"｜ two 的实例字典：\", sorted(vars(two)))
"'

# ============================================================================
banner "第 4 节 self 就是普通第一个参数：obj.f() 与 A.f(obj) 等价"
# ============================================================================
runc 'python3 -c "
class Calc:
    def __init__(self, start=0):
        self.value = start
    def add(self, n):
        self.value += n
        return self.value
c1, c2 = Calc(10), Calc(10)
print(\"Calc.add(c1, 5) 返回：\", Calc.add(c1, 5))
print(\"c2.add(5)    返回：\", c2.add(5), \"← 一模一样的两条路，只是写法不同\")
print(\"c1 和 c2 状态同步吗：\", c1.value == c2.value)
print(\"c1.add.__func__ is Calc.add：\", c1.add.__func__ is Calc.add)
print(\"c1.add.__self__ is c1：\", c1.add.__self__ is c1)
print(\"vars(Calc)[\\\"add\\\"] 的类型：\", type(vars(Calc)[\"add\"]).__name__, \"← 类字典里躺的就是普通函数\")
"'
runc 'python3 b_self_param.py'

# ============================================================================
banner "第 5 节 __init__ 返回非 None：3.14 的报错原文"
# ============================================================================
runc 'python3 -u c_init_return.py'
note '↑ 退出码 1。最后一行 TypeError 的原文值得逐字读：__init__() should return None, not ...'
note '  （第 05 章讲过 return 是函数的事；构造器只负责“填房间”，不负责“交货”。）'

# ============================================================================
banner "第 6 节 __repr__ vs __str__：print、f-string、容器里各走哪个"
# ============================================================================
runc 'python3 -c "
class T:
    def __str__(self): return \"str 给的\"
    def __repr__(self): return \"repr 给的\"
t = T()
print(\"print(t) →\", t)
print(\"f\\\"{t}\\\" →\", f\"{t}\")
print(\"f\\\"{t!r}\\\" →\", f\"{t!r}\")
print(\"[t] →\", [t])
print(\"(t,) →\", (t,))
print(\"{1: t} →\", {1: t})
"'
note '↑ 六行三句话：print/str()/裸 f-string 走 __str__；!r、repr()、容器打印走 __repr__。'
runc 'python3 d_repr_str.py'

# ============================================================================
banner "第 7 节 __eq__ 的代价：__hash__ 被没收，进不了 set / 当不了 dict 键"
# ============================================================================
runc 'python3 -c "
class P:
    def __eq__(self, other): return True
p = P()
{p}
"'
note '↑ 退出码 1，这是「放进 set」的裸报错现场，原文下一节还会分三种场合再抄一遍。'
runc 'python3 e_eq_hash.py'

# ============================================================================
banner "第 8 节 @property：getter/setter/deleter 三件套与两种报错原文"
# ============================================================================
runc 'python3 -c "
class R:
    @property
    def v(self): return 1
r = R()
r.v = 2
"'
note '↑ 只有 getter 就赋值的裸现场（退出码 1）：property ... has no setter。'
runc 'python3 f_property_trio.py'

# ============================================================================
banner "第 9 节 @dataclass 现形（一）：它只是替你写进类字典的三个函数"
# ============================================================================
runc 'python3 -c "
from dataclasses import dataclass
@dataclass
class D:
    x: int
    y: str = \"hi\"
print(\"类字典里的普通名字（y 是字段声明留下的默认值）：\", sorted(k for k in vars(D) if not k.startswith(\"__\")))
print(\"生成三件套都在：\", all(n in vars(D) for n in (\"__init__\", \"__repr__\", \"__eq__\")))
import inspect
print(\"__init__ 的签名：\", inspect.signature(D.__init__))
print(\"D(1) 的 repr →\", repr(D(1)))
"'
runc 'python3 g_dataclass_intro.py'

# ============================================================================
banner "第 10 节 @dataclass 现形（二）：可变默认值它当场拒绝你"
# ============================================================================
runc 'python3 -c "
from dataclasses import dataclass
@dataclass
class Draft:
    items: list = []
"'
note '↑ 退出码 1：手写 def __init__(self, items=[]) 那种「默认值只造一次」的坑（第 05 章），'
note '  dataclass 在定义那一刻就 ValueError 拦下，原文就在上面 traceback 最后一行。'
runc 'python3 h_dc_mutable_default.py'

# ============================================================================
banner "第 11 节 @dataclass(frozen=True)：赋值现场与 FrozenInstanceError 原文"
# ============================================================================
runc 'python3 -c "
from dataclasses import dataclass
@dataclass(frozen=True)
class Cfg:
    host: str
c = Cfg(\"localhost\")
c.host = \"evil\"
"'
note '↑ 退出码 1。注意异常类是 dataclasses.FrozenInstanceError，它是 AttributeError 的儿子（脚本里验证）。'
runc 'python3 i_dc_frozen.py'

# ============================================================================
banner "第 12 节 @dataclass 现形（三）：eq 怎么比、order 才有的 <、repr 长什么样"
# ============================================================================
runc 'python3 -c "
from dataclasses import dataclass
@dataclass
class V:
    a: int
V(1) < V(2)
"'
note '↑ 不写 order=True 就没有 __lt__，TypeError 原文见上（退出码 1）。'
runc 'python3 j_dc_eq_order.py'

# ============================================================================
banner "第 13 节 @dataclass(slots=True)：3.10+ 的选项，真把 __dict__ 去掉了吗"
# ============================================================================
runc 'python3 -u -c "
from dataclasses import dataclass, fields
@dataclass(slots=True)
class P:
    x: int
p = P(1)
print(\"实例有 __dict__ 吗：\", hasattr(p, \"__dict__\"))
print(\"类字典里的 __slots__：\", P.__slots__)
print(\"fields 照常：\", [f.name for f in fields(P)])
p.z = 3
"'
note '↑ 最后一步崩：给 slots 实例加新属性的原文值得逐字读（退出码 1）。'
runc 'python3 k_dc_slots.py'

# ============================================================================
banner "第 14 节 __slots__ 的账：省多少内存（10 万实例实测）与三个代价"
# ============================================================================
runc 'python3 -c "
import sys
class Loose:
    def __init__(self, x, y):
        self.x = x; self.y = y
class Tight:
    __slots__ = (\"x\", \"y\")
    def __init__(self, x, y):
        self.x = x; self.y = y
a, b = Loose(1, 2), Tight(3, 4)
print(\"Loose 实例有 __dict__ 吗：\", hasattr(a, \"__dict__\"), \"｜ Tight 实例：\", hasattr(b, \"__dict__\"))
print(\"实例本体都是\", sys.getsizeof(a), \"字节；差额全在挂不挂 __dict__ 上（详见下一条脚本的 10 万实例总账）\")
"'
runc 'python3 l_slots_cost.py'
note '↑ 时间那行每次会抖，README 只该引用「字节总账」那组数；两组字节数是确定的。'

# ============================================================================
banner "第 15 节 钻石继承：__mro__ 是名单，调用链照着名单走"
# ============================================================================
runc 'python3 m_mro_super.py'

# ============================================================================
banner "第 16 节 super() 找谁，由「实例的 MRO」定，不由「写在哪」定"
# ============================================================================
runc 'python3 -c "
class A:
    def hi(self): return \"A\"
class B(A):
    def hi(self): return \"B\"
class C(A):
    def hi(self): return \"C\"
class D(B, C):
    pass
d = D()
print(\"D.__mro__ →\", [c.__name__ for c in D.__mro__])
print(\"super(B, d).hi.__func__ 落在哪：\", super(B, d).hi.__func__(d),
      \"← B 的下一家是 C，不是亲爹 A\")
print(\"super(C, d).hi.__func__ 落在哪：\", super(C, d).hi.__func__(d))
b = B()
print(\"换 B 自己的实例：super(B, b).hi 落在哪：\", super(B, b).hi.__func__(b))
"'
note '↑ 同一个 super(B, ...)：实例是 D 就去找 C，实例是 B 就去找 A —— CPython 源码里 super 就是这么查的。'

# ============================================================================
banner "第 17 节 类变量在继承里：+= 悄悄建遮挡物，可变类变量全体一起脏"
# ============================================================================
runc 'python3 n_shared_classvar.py'

# ============================================================================
banner "第 18 节 isinstance 认户口，type() 认现形"
# ============================================================================
runc 'python3 -c "
class Animal: pass
class Dog(Animal): pass
d = Dog()
print(\"isinstance(d, Animal)：\", isinstance(d, Animal), \"｜ type(d) is Animal：\", type(d) is Animal)
print(\"type(d).__name__：\", type(d).__name__)
print(\"isinstance(d, (Animal, int))：\", isinstance(d, (Animal, int)), \"← 元组一并问\")
"'
runc 'python3 o_isinstance_type.py'

# ============================================================================
banner "第 19 节 __init_subclass__ 与 ABC：定义子类那一刻的钩子"
# ============================================================================
runc 'python3 -c "
import abc
class Shape(abc.ABC):
    @abc.abstractmethod
    def area(self): ...
Shape()
"'
note '↑ 退出码 1，抽象类裸实例化的报错原文（捕获版在下一条命令里还有对照）。'
runc 'python3 p_subclass_hooks.py'

# ============================================================================
banner "第 20 节 名字改写：__private 在字典里的真名是 _类名__private"
# ============================================================================
runc 'python3 q_mangling.py'

# ============================================================================
banner "第 21 节 __call__：实例为什么能带括号 —— f(x) 原来是 type(f).__call__(f, x)"
# ============================================================================
runc 'python3 r_callable.py'

# ============================================================================
banner "第 22 节 __getattr__ 是兜底，__getattribute__ 是全量拦截（写错死循环）"
# ============================================================================
runc 'python3 -u -c "
class Good:
    def __init__(self):
        self.data = {\"x\": 1}
    def __getattribute__(self, name):
        print(\"    拦截到：\", name)
        return super().__getattribute__(name)   # 正确姿势：把活交回给 object
g = Good()
print(\"g.data →\", g.data)
"'
note '↑ 正确版：__getattribute__ 里要用 super()/object.__getattribute__ 把查找交回去。'
runc 'python3 -u s_getattr.py'
note '↑ 退出码 1：错误版 self.__dict__[name] 里「取 self.__dict__」这一步又走回 __getattribute__，'
note '  自己咬自己，traceback 里 [Previous line repeated ...] 就是证据，最后 RecursionError。'

# ============================================================================
banner "第 23 节 半个上下文管理器：__enter__/__exit__（第 07 章的钩子）"
# ============================================================================
runc 'python3 t_context_manager.py'

# ============================================================================
banner "第 24 节 取证手段汇总：这一章用到的标准库入口"
# ============================================================================
runc 'python3 -c "
import dataclasses, inspect, abc, collections.abc, sys
print(\"dataclasses 公开件：\", [n for n in (\"dataclass\", \"field\", \"fields\", \"replace\", \"FrozenInstanceError\") if hasattr(dataclasses, n)])
print(\"inspect.signature / isclass：\", hasattr(inspect, \"signature\"), hasattr(inspect, \"isclass\"))
print(\"abc.ABC / abstractmethod：\", hasattr(abc, \"ABC\"), hasattr(abc, \"abstractmethod\"))
print(\"collections.abc.Callable：\", hasattr(collections.abc, \"Callable\"))
"'
runc 'python3 -c "
from dataclasses import dataclass
@dataclass(frozen=True, slots=True)
class DC:
    x: int
print(\"slots=True 是 3.10 加的参数，本机 3.14 可用：\", DC.__slots__)
"'
note '↑ 全章没有任何第三方依赖；上面这些件全部来自本机 /usr/lib/python3.14。'

# ============================================================================
banner "第 25 节 收尾：证明这一章没在手册目录里留下任何产物"
# ============================================================================
runc_at "$HERE" "find . \\( -name '__pycache__' -o -name '*.pyc' \\) | wc -l"
note '↑ 这个数字必须是 0：__pycache__、pyc 全没生过 —— 本章只用 python3 xxx.py 跑脚本，'
note '  从没 import 过章目录里的示例模块（dataclasses 是标准库，缓存生在 /usr/lib 且只读）。'
runc_at "$HERE" "ls | wc -l"
runc 'ls'
runc 'ls __pycache__'
note '↑ 最后一条退出码 2：沙箱里同样没有 __pycache__ —— 这一章全程 python3 xxx.py 直接跑脚本，'
note '  从没 import 过章目录里的示例模块（dataclasses 是标准库，缓存生在 /usr/lib 且只读）。'

printf '\n%s[全部跑完] 上面每一段都是真实执行结果；README.md 引用的输出全部出自这一次运行。%s\n' "$B" "$R"
exit 0
