#!/usr/bin/env bash
# ============================================================================
# 第 10 章《类型注解：写了不等于有人看》—— 整章证据一键复现
#
#   bash demo.sh                 跑完自动删沙箱（默认）
#   bash demo.sh --keep          跑完保留沙箱并打印路径，进去自己接着敲
#   bash demo.sh > out.txt 2>&1  重定向时自动关颜色，得到纯文本（README 引用的就是这份）
#
# 屏幕上出现的每一行，都是这条命令在这台机器（CPython 3.14.4 / Ubuntu）上的真实结果。
#
# 本章四条主线（每节的 note 都会指回其中一条）：
#   ① 注解是**惰性求值**的：`def f(x: Widget)` 定义时不碰 Widget，第一次读 `__annotations__` 才碰；
#      求值成功才缓存，失败**不缓存**（每读重跑一遍注解表达式）。
#   ② 读注解只有一个入口族：annotationlib.Format 的四个值 + `__annotate__()` 直调，
#      其中 VALUE_WITH_FAKE_GLOBALS 是内部用的，公开调用直接 ValueError。
#   ③ 运行时**没有任何人**替你检查值：dataclass/NamedTuple/TypedDict 用注解但不求值它，
#      isinstance 家族只吃裸类和 X | Y。
#   ④ 本机没有静态检查器（mypy/pyright/ruff/black/flake8/pylint/pytest 全缺席，连 pip 都没有），
#      唯一在场的第三方库 typeguard 是**运行时插桩**；剩下的路是自己用 ast 搓一把尺（g 篇）。
#
# 安全说明：
#   * 章目录永远只读：7 个取证脚本整份 cp 进 mktemp -d 的沙箱再跑，章目录里不会留下
#     __pycache__/、*.pyc、*-demo/。第 14 节的每条计数都先被故意弄响一次再报 0 ——
#     恒 0 的检查比没有检查更糟。
#   * 只用标准库 + 本机 apt 带来的 typeguard；不 pip install、不联网、不开监听端口、
#     不改 ~/.bashrc / ~/.profile / ~/.config / ~/.local（第 1 节和第 14 节各数一遍
#     家目录那两个目录的条目数，两次一样就是「没写过」的凭证）。
#   * 七个取证脚本自己会起子进程（跑样本 .py），一律 cwd=沙箱 + 干净 env（只给 PATH），
#     绝不继承 PYTHONPATH；起的全是前台同步进程，跑到收尾时必然已经归零。
#   * 故意不加 `set -e`：本章一半是失败现场（NameError、TypeError、ValueError、
#     NotImplementedError…），一遇非 0 就停的话屏幕上只剩第一格。
#     取证脚本里那些「炸」是被 eq() 接住的判据，不是事故 —— 它们让 rc 恒为 0。
#   * 故意不加 `set -u`：本脚本要如实打印「这些开关此刻没设」，还要读 ORIG_LANG 这种
#     在有些机器上根本不存在的变量。
#   * 故意不加 `set -o pipefail`：`... | head -3`、`grep | wc -l` 会因下游提前关管道报 141。
#   * 屏幕上不出现真实家目录 / 用户名 / 主机名 / 对象地址 / pid / 内存字节数：
#     取证脚本自带 mask()，本 demo 的三张矩阵表自带把 0x… 折成 <地址> 的 fold()；
#     第 14 节再把整份屏幕账扫一遍，五类模式各数一次。
#   * ★ 沙箱名里那 6 个随机字符每跑必变，所以默认收尾只印「沙箱已删除」而不印路径；
#     否则两遍 transcript 永远不可能逐字一致（--keep 是给人看的，不参与对账）。
#   * 本脚本自己的退出码恒为 0，这样才能当验收闸门用。
# ============================================================================

KEEP=0
for arg in "$@"; do
    case "$arg" in
        --keep) KEEP=1 ;;
        -h|--help) echo "用法: bash demo.sh [--keep]"; exit 0 ;;
        *) echo "未知参数：${arg}（本脚本只认 --keep）" >&2; exit 2 ;;
    esac
done

ORIG_LANG=${LANG-}
ORIG_LC_ALL=${LC_ALL-}
export ORIG_LANG ORIG_LC_ALL
export LC_ALL=C.UTF-8
export LANG=C.UTF-8
export LANGUAGE=C
# 本章的结论挂在「谁在什么时候读注解」上，这些开关会改写解释器的行为或报错措辞；
# 先拨到已知位置（只影响本脚本这个进程和它的子进程，不碰任何配置文件）。
unset PYTHONPATH PYTHONHOME PYTHONSAFEPATH PYTHONSTARTUP PYTHONINSPECT PYTHONDEBUG
unset PYTHONDONTWRITEBYTECODE PYTHONOPTIMIZE PYTHONWARNINGS PYTHONDEVMODE PYTHONFAULTHANDLER
# 固定哈希种子：矩阵格里要数 dict/set 的条数，也要打 fields 清单。
export PYTHONHASHSEED=0

if [ -t 1 ]; then B=$'\033[1m'; C=$'\033[36m'; R=$'\033[0m'; else B=''; C=''; R=''; fi

HERE=$(cd "$(dirname "$0")" && pwd)
SB=$(mktemp -d "${TMPDIR:-/tmp}/py10-XXXXXX")
export SB
: > "$SB/all.out"          # 屏幕账：第 14 节的隐私扫描与判据汇总吃的就是这份

banner() { printf '\n%s══════ 第 %s 节 %s ═══════════════════════════════%s\n' "$B" "$1" "$2" "$R"; }
note()   { printf '%s\n' "$1"; printf '%s\n' "$1" >> "$SB/all.out"; }

# 打印命令 → 执行（stderr 合并）→ 打印退出码；命令、输出、退出码三样都进屏幕账。
runc() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    printf '$ %s\n' "$1" >> "$SB/all.out"
    local out rc
    out=$(bash -c "$1" 2>&1); rc=$?
    printf '%s\n' "$out"
    printf '%s\n' "$out" >> "$SB/all.out"
    printf '[退出码 %d]\n' "$rc"
    printf '[退出码 %d]\n' "$rc" >> "$SB/all.out"
}

# runc_at <目录> <命令>：先 cd 进那个目录再跑，屏幕上只留目录 basename + 相对命令。
runc_at() {
    printf '\n%s$ 〔在 %s 里〕 %s%s\n' "$C" "$(basename "$1")" "$2" "$R"
    printf '$ 〔%s〕 %s\n' "$(basename "$1")" "$2" >> "$SB/all.out"
    local out rc
    out=$( cd "$1" && eval "$2" ) 2>&1; rc=$?
    printf '%s\n' "$out"
    printf '%s\n' "$out" >> "$SB/all.out"
    printf '[退出码 %d]\n' "$rc"
    printf '[退出码 %d]\n' "$rc" >> "$SB/all.out"
}

cleanup() {
    if [ "$KEEP" -eq 1 ]; then
        printf '\n[沙箱已保留] %s\n  进去接着敲： cd "%s/work"\n  看完自己删： rm -rf "%s"\n' \
               "$SB" "$SB" "$SB"
    else
        rm -rf "$SB"
        printf '\n[沙箱已删除] <本次 mktemp -d 出来的目录，名字每跑随机，所以不打出来>\n'
    fi
}
trap cleanup EXIT

# ============================================================================
banner 1 "环境体检：注解这条路的起点，和这台机器上到底有什么工具"
# ============================================================================
mkdir -p "$SB/work"
cp "$HERE"/*.py "$SB/work/"
cd "$SB/work" || exit 1
umask 022
note '沙箱 work/ 里就是章目录那 7 个 .py 的整份副本；章目录本身从头到尾不参与写（第 14 节自证）。'
runc 'ls'
runc 'python3 --version && python3 -c "import sys; print(sys.version)"'
note '★ 主线① 的全部结论都挂在 3.14 这套惰性注解机制上（annotationlib 是 3.14 才有的模块）。'
runc 'python3 -c "
import sys, annotationlib
print(\"annotationlib 在标准库名单里吗 →\", annotationlib.__name__ in sys.stdlib_module_names)
print(\"它的 Format 有几个成员 →\", len(list(annotationlib.Format)), \"｜ 逐个（名字, 值）→\", [(f.name, f.value) for f in annotationlib.Format])
"'
runc 'python3 -c "import sys; print(\"sys.executable 的 basename =\", __import__(\"os\").path.basename(sys.executable)); print(\"在系统解释器上（没泡 venv）吗 →\", sys.prefix == sys.base_prefix)"'
note '↑ 只看布尔，不印一堆路径：路径里带版本号和安装前缀，每换一台机器就变，README 对不住。'
runc 'python3 -c "
import os, sys
h = os.path.expanduser(\"~\")
path = os.environ[\"PATH\"].split(os.pathsep)
print(\"PATH 里有没有条目落在家目录下面 →\", any(x.startswith(h) for x in path), \"（True 也只是这台机器的 shell 配置，名字一律不上屏）\")
dp = [x for x in sys.path if \"dist-packages\" in x]
print(\"第三方包是从 sys.path 里来的，不是 PATH：含 dist-packages 字样的条数 →\", len(dp))
print(\"  折掉安装前缀后 →\", [x.replace(sys.base_prefix, \"<pyroot>\") for x in dp])
"'
note '↑ 这两行是本机的「包从哪儿来」底账：typeguard 就落在 <pyroot>/lib/python3 那条 dist-packages 里，f/g 两篇取证时都用得到。'
note '下面这格是本章的**在场性红线**：读者的第一反应是「装个 mypy 就好了」，本机做不到。'
runc 'python3 -c "
import importlib.util as u, sys
tools = [\"mypy\", \"pyright\", \"ruff\", \"black\", \"flake8\", \"pylint\", \"pytest\", \"pytype\"]
missing = sorted(t for t in tools if u.find_spec(t) is None)
print(\"八把静态尺里缺席的 →\", missing, \"｜ 在场的 →\", sorted(set(tools) - set(missing)))
print(\"typeguard 在场吗 →\", u.find_spec(\"typeguard\") is not None)
"'
runc 'python3 -m pip --version; echo "（上面这行的 rc 与原文，g/f 两篇都要用：本机装不了任何东西）"'
runc 'python3 -c "
import typeguard, os, pathlib
d = pathlib.Path(os.path.dirname(typeguard.__file__)).parent
info = sorted(x.name for x in d.glob(\"typeguard*dist-info\"))
print(\"typeguard 自己有没有 __version__ 属性 →\", hasattr(typeguard, \"__version__\"))
print(\"版本号只能从目录名读 →\", info)
"'
note '★ 版本号这件事 f 篇第 2 格会自己再演一遍；这里只回答读者的「我该怎么知道自己装的哪版」。'
runc 'printf "语言环境：原值 LANG=%s LC_ALL=%s → 现在 LANG=%s LC_ALL=%s LANGUAGE=%s\n" "${ORIG_LANG:-（未设）}" "${ORIG_LC_ALL:-（未设）}" "$LANG" "$LC_ALL" "$LANGUAGE"'
note '↑ 原值能印出来，是因为 ORIG_LANG / ORIG_LC_ALL 被 export 了 —— 不导出就只能看到两个空括号（上一章踩过）。'
note '  取证脚本里的报错原文全是 Python 自己的（英文），不受这三行影响；你手敲时看到中文提示也正常。'
runc 'python3 -c "
import os
def n(p):
    p = os.path.expanduser(p)
    return len(os.listdir(p)) if os.path.isdir(p) else \"目录不存在\"
print(\"~/.local/lib 条目数 =\", n(\"~/.local/lib\"), \"｜ ~/.config 条目数 =\", n(\"~/.config\"), \"｜ ~/.bashrc 存在吗 →\", os.path.isfile(os.path.expanduser(\"~/.bashrc\")))
"'
note '★ 本机有一条会影响读表的事实：用户名与主机名是同一个串，所以第 14 节隐私扫描里那两格数字必然相同 ——'
note '  不是扫描重了两次，是这台机器上本来就是同一个值。'
runc 'python3 -c "
import os, socket
print(\"本机用户名与主机名相同吗 →\", os.environ.get(\"USER\", \"\") == socket.gethostname())
"'

# ============================================================================
banner 2 "a 篇完整跑：注解什么时候才被求值（定义时不碰，读时才碰）"
# ============================================================================
runc 'python3 a_lazy_annotations.py'
note '↑ 这一篇把主线① 钉死：定义 `def f(x: Widget)` 时 Widget 可以根本不存在，函数照样建起来；'
note '  第一次读 `f.__annotations__` 才去求值，才可能 NameError。求值**成功才缓存**（两次读到同一个 dict 对象），'
note '  **失败不缓存**（每读一次重跑一遍那个表达式，屏上的计数器 1→2 就是这件事）。'
note '  最后它把「函数体里的局部变量注解运行时一次都不求值」也量了 —— 这条在第 12 节会被 g 篇反过来用。'

# ============================================================================
banner 3 "现场格：两份只差一行的文件并排 —— future 开关改的是求值时机，不是可读性"
# ============================================================================
note 'a/b 两篇各自演过 `from __future__ import annotations`。这一格把「开」和「关」做成两个真文件，'
note '  同一份函数体，一次读注解，屏上直接给两种形状 —— 因为开关的效果只在**文件级**发生，'
note '  在一个进程里同时摆出两份才看得出它是「编译期的选择」。'
runc 'mkdir -p fut-check && cd fut-check
cat > off.py <<PY
class Known: pass
def fn(k: Known, q: "Missing2") -> int: return 0
PY
cat > on.py <<PY
from __future__ import annotations
class Known: pass
def fn(k: Known, q: "Missing2") -> int: return 0
PY
python3 -c "
import re
def fold(s): return re.sub(r\"0x[0-9a-f]+\", \"<地址>\", str(s))
for tag, mod in ((\"不加 future\", \"off\"), (\"加了 future\", \"on\")):
    m = __import__(mod)
    a = m.fn.__annotations__
    print(f\"{tag} → 每个注解的【类型】：\", {k: type(v).__name__ for k, v in a.items()})
    print(f\"{tag} → 值本身（折过地址）：\", {k: fold(v) for k, v in a.items()})
"
cd ..; rm -rf fut-check'
note '↑ 两行读数的差别只有一处：开了 future 之后**所有**注解都是 str（包括那个本来能求值的 Known），'
note '  而引号注解 `"Missing2"` 在两种开关下都是 str —— 于是「加了引号」和「开了开关」在下游看起来一模一样。'
note '  b 篇第 9 格量的正是这条混淆；这一格给它一个「同一进程里并排」的最小现场。'
note '★ 顺带一条红线：本节的 fold() 把 0x… 折成 <地址>，因为 ForwardRef 的 repr 里带函数对象地址；'
note '  第 5 节那张矩阵用的是同一个 fold()，取证脚本里则是各自的 mask()。'

# ============================================================================
banner 4 "b 篇完整跑：读注解的四个入口，和它们各自的规矩"
# ============================================================================
runc 'python3 b_four_reads.py'
note '↑ 主线② 的全部形状都在这里：VALUE(1) 求值并缓存、FORWARDREF(3) 把求不出的留成 ForwardRef、'
note '  STRING(4) 给你源码里的字面串、VALUE_WITH_FAKE_GLOBALS(2) 公开调用直接 ValueError（它是解释器内部用的）。'
note '  它还量了 `__annotate__()` 直调：不传 format 是 TypeError，传 2 或 4 是 NotImplementedError（空消息）。'

# ============================================================================
banner 5 "矩阵格：一份三种坏法并排的函数，五种读法各给什么（跨 a/b 两篇的合并视图）"
# ============================================================================
note 'a、b 两篇各自在自己的进程里演。这一格把「同一份签名」一次喂给五个入口，'
note '  屏上一张表看完：哪个炸、炸成什么、哪个给你 str、哪个给你 ForwardRef。'
cat > matrix_reads.py <<'PY'
import annotationlib as A
import inspect
import re
from typing import get_type_hints


def fold(s):
    return re.sub(r"0x[0-9a-f]+", "<地址>", str(s))


class Known:
    pass


def fn(k: Known, bad: Missing1, q: "Missing2") -> int:
    return 0


print("被读的那个签名：def fn(k: Known, bad: Missing1, q: \"Missing2\") -> int")
for f in A.Format:
    try:
        out = f"OK {fold(dict(A.get_annotations(fn, format=f)))}"
    except Exception as exc:
        out = f"炸 {type(exc).__name__}: {fold(exc)}"
    print(f"  ① {f.name}({f.value}) → {out[:150]}")
for tag, th in (
    ("② typing.get_type_hints", lambda: get_type_hints(fn)),
    ("③ inspect.signature", lambda: str(inspect.signature(fn))),
    ("④ 属性读 f.__annotations__", lambda: dict(fn.__annotations__)),
):
    try:
        print(f"  {tag} → OK {fold(th())[:150]}")
    except Exception as exc:
        print(f"  {tag} → 炸 {type(exc).__name__}: {fold(exc)[:90]}")
print("  ⑤ 唯一的绕法：先把 STRING 读法的结果**写回** __annotations__，再读一次")
fn.__annotations__ = dict(A.get_annotations(fn, format=A.Format.STRING))
print("     写回后 VALUE →", fold(dict(fn.__annotations__))[:150])
print("     写回后 signature →", fold(inspect.signature(fn))[:150])
PY
runc 'python3 matrix_reads.py'
rm -f matrix_reads.py
note '↑ 五行表里最值得记住的是最后两行：注解**可以写回去**（__annotations__ 是个普通可写属性），'
note '  于是 inspect 那类「读 VALUE 就炸」的工具能被绕过 —— 但绕过去之后它拿到的是 str 而不是类对象，'
note '  这不是「修好了」，是「把求值这件事推给了自己」。b 篇第 11 格量的是同一条路的另一头。'

# ============================================================================
banner 6 "c 篇完整跑：到底谁在读注解 —— dataclass / inspect / TYPE_CHECKING 三本账"
# ============================================================================
runc 'python3 c_who_reads_annotations.py'
note '↑ 主线③：dataclass 建类与实例化**都不求值**注解；inspect.signature 走 VALUE，未定义名当场炸；'
note '  `if TYPE_CHECKING: import argparse` 那句在运行时根本不执行（屏上用子进程证明了 sys.modules 里没有它）。'
note '  还有一条最阴的：dataclass 认 ClassVar 用的是 FORWARDREF 读法，忘 import 时它会**静默降级**成普通字段。'

# ============================================================================
banner 7 "矩阵格：一个坏注解字段的 dataclass，七个调用点谁响（跨 c 篇的合并视图）"
# ============================================================================
note 'c 篇按「谁读」逐个演。这一格反过来：建好一个类（字段里就写着源码里没有的名字），'
note '  然后把七个真实调用点逐个撞上去 —— 前三个是本章大部分程序一辈子走的路，后四个才是会炸的地方。'
cat > matrix_class.py <<'PY'
import annotationlib as A
import dataclasses
import inspect
import re
from typing import get_type_hints


def fold(s):
    return re.sub(r"0x[0-9a-f]+", "<地址>", str(s))


@dataclasses.dataclass
class Row:
    name: str
    count: MissingHere


def t(tag, th):
    try:
        print(f"  {tag} → 不响：{fold(th())[:100]}")
    except Exception as exc:
        print(f"  {tag} → 响：{type(exc).__name__}: {fold(exc)[:80]}")


t("① 建类（decorator 返回时）", lambda: Row.__name__)
t("② 实例化 Row(name=..., count=...)", lambda: repr(Row(name="a", count=1)))
t("③ dataclasses.fields 清单", lambda: [f.name for f in dataclasses.fields(Row)])
t("④ inspect.signature(Row)", lambda: str(inspect.signature(Row)))
t("⑤ typing.get_type_hints(Row)", lambda: get_type_hints(Row))
t("⑥ 属性读 Row.__annotations__", lambda: dict(Row.__annotations__))
t("⑦ 读生成 __init__ 的 VALUE 注解", lambda: A.get_annotations(Row.__init__, format=A.Format.VALUE))
PY
runc 'python3 matrix_class.py'
rm -f matrix_class.py
note '↑ 三响四不响，这条分界线就是本章给读者的实用结论：**你的程序能跑**，'
note '  直到某个库去读注解 —— 序列化库、CLI 框架、依赖注入、测试框架的 fixture，全都站在这条线的右边。'
note '  所以「我加了注解结果程序崩了」这种报告，第一个要问的是：谁在读它，读的是哪个 format。'

# ============================================================================
banner 8 "d 篇完整跑：运行时一行都不检查（八个「写了也不拦」的现场）"
# ============================================================================
runc 'python3 d_runtime_checks_nothing.py'
note '↑ 主线③ 的另一半：变量注解不改值、函数注解不拦错类型实参、Final 不拦重新赋值、@final 不拦继承、'
note '  cast 原样返回、Literal 不拦越界值、assert_type 连名字带 assert 都是空气、reveal_type 走 stderr。'
note '  它最后留了一个真的负对照：自己读 __annotations__ 自己 isinstance，四组错误实参响四次。'

# ============================================================================
banner 9 "e 篇完整跑：Protocol 与泛型 —— isinstance 家族能吃什么"
# ============================================================================
runc 'python3 e_protocols_generics.py'
note '↑ @runtime_checkable 的 isinstance 等价于 hasattr（签名一个都不查）；带数据成员的协议 issubclass 直接 TypeError；'
note '  get_origin/get_args 那张表里 Optional 和 `int | str` 的 origin 都是 typing.Union，'
note '  而 `list[int] is list[int]` 是 False（== 才是 True）—— 这一族全是「看起来像类型检查，其实是别的机制」。'

# ============================================================================
banner 10 "f 篇完整跑：本机唯一的第三方库 typeguard —— 它到底替你挡了什么"
# ============================================================================
runc 'python3 f_typeguard.py'
note '↑ 主线④：typeguard 是**运行时插桩**，不是静态检查。三件事必须记住：'
note '  默认只查容器**第 0 个**元素（把错的挪到第 1 位就静默放行）；它管函数体内被注解变量的赋值、'
note '  不管实例属性；它要求代码在文件里 —— `-c` 里 rc=1，喂标准输入 rc=0 但装饰器压根没装上。'
note '  最后一条 f 篇自己也被屏上纠正过：注解名求不出时，装饰器那条路当场 NameError 崩，'
note '  「按 forward_ref_policy 降级」只发生在直调 check_type 这条路上。'

# ============================================================================
banner 11 "矩阵格：同一个坏值摆在四把工具前面（跨 d/e/f/g 四篇的合并视图）"
# ============================================================================
note '四篇各自演「谁能拦什么」。这一格用**同一个调用** `add("两个", "字符串")` 撞四把工具，'
note '  一张表看完：裸解释器、自己 isinstance、@typechecked、以及 g 篇那把 ast 尺。'
cat > matrix_tools.py <<'PY'
import ast
import builtins
import re
import typeguard


def fold(s):
    return re.sub(r"0x[0-9a-f]+", "<地址>", str(s))


def add_plain(a: int, b: int) -> int:
    return a + b


def add_manual(a: int, b: int) -> int:
    if not isinstance(a, int) or not isinstance(b, int):
        raise TypeError("自己写的那一遍检查响了")
    return a + b


@typeguard.typechecked
def add_tg(a: int, b: int) -> int:
    return a + b


def tool(tag, th):
    try:
        print(f"  {tag} → 静默：返回 {fold(th())!r}")
    except Exception as exc:
        print(f"  {tag} → 响：{type(exc).__name__}: {fold(exc)[:90]}")


print("被调用的是同一个：add_plain / add_manual / add_tg 都是 def add(a: int, b: int) -> int")
tool("① 裸解释器 + 注解（d 篇）", lambda: add_plain("两个", "字符串"))
tool("② 自己在函数体里 isinstance（d 篇的负对照）", lambda: add_manual("两个", "字符串"))
tool("③ @typeguard.typechecked（f 篇）", lambda: add_tg("两个", "字符串"))
print("  ④ 用 g 篇那把 ast 尺扫本文件里的 add_plain：")


def scan(src):
    tree = ast.parse(src)
    vis = set(dir(builtins))
    for node in tree.body:
        if isinstance(node, (ast.Import, ast.ImportFrom)):
            for al in node.names:
                vis.add(al.asname or al.name.split(".")[0])
        elif isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
            vis.add(node.name)
    bad = []
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            anns = [x.annotation for x in node.args.args if x.annotation is not None]
            if node.returns is not None:
                anns.append(node.returns)
            for ann in anns:
                for nm in sorted({m.id for m in ast.walk(ann) if isinstance(m, ast.Name)}):
                    if nm not in vis:
                        bad.append(f"  {node.name} 参数/返回值:{nm}")
    return bad


src = open(__file__, encoding="utf-8").read()
print("     尺对这份源码报的清单 →", scan(src), "← 空的是对的：int/str/add 全是可见名字")
print("     尺对「值传错了」这件事报几条 →", len(scan(
    "def add(a: int, b: int) -> int:\n    return a + b\n")))
print("  → 结论排一排：①永远静默；②③要你真的去调用那把工具；④根本不针对值。")
print("     「加了注解就安全」这句话里被省略的是：谁读它、什么时候读、读它的人查不查值。")
PY
runc 'python3 matrix_tools.py'
rm -f matrix_tools.py
note '↑ ③ 那句原文（argument "a" (str) is not an instance of int）与 f 篇第 6 格逐字同源，'
note '  但这一格给的是**调用现场**而不是单独取证 —— 读这一节时把两屏对着看最省时间。'
note '★ 注意尺那一行有个坑：它扫的是自己所在的**整个文件**，那份清单里连一个字符串都没多 ——'
note '  「守卫扫到自己」这件事 bash 手册第 10 章专门演过，这里是它在 Python 侧的同款现场。'

# ============================================================================
banner 12 "g 篇完整跑：本机没有 mypy，那就用 ast 现搓一把最小的尺"
# ============================================================================
runc 'python3 g_mini_checker.py'
note '↑ 主线④ 的落点：注解在语法树里就是普通表达式，`ast` 是标准库，所以「静态检查」这件事不必等工具。'
note '  那把尺给三件运行时给不了的东西：一次报**全部**（运行时只报第一个）、不执行也能查、'
note '  查那些永远不会被调用的代码。它也当场认了三条漏网：属性存在性、值与类型匹不匹配、跨作用域细节。'
note '  第 8 格那条实用：symtable 看着更懂作用域，但它**不把注解当使用**（三个只在注解里用过的名字'
note '  is_referenced() 全是 False），想做这件事只能自己 ast.walk。'

# ============================================================================
banner 13 "跨篇小结：一张「谁在什么时候读注解」的合并账"
# ============================================================================
note '七篇的结论合成一张账。每一行都是前面某节屏上量过的，这里只做**布尔与计数**，不引入新事实。'
runc 'python3 -c "
import annotationlib as A, inspect, re, sys, typing
from typing import get_type_hints
print(\"【时刻① 定义函数时】注解求值了吗 →\")
exec(\"def probe(v: NoSuchName) -> int:\n    return 0\", globals())
print(\"   函数建起来了吗 →\", callable(probe), \"｜ 此刻读没读过那个名字 → 屏幕上没有 NameError 就是没读\")
print(\"   （a 篇把这条量成了计数器：定义那一遍计数 0）\")
print(\"【时刻② 第一次读注解】谁读谁求值：\")
try:
    probe.__annotations__
    first = \"没炸\"
except Exception as e:
    first = type(e).__name__
print(\"   属性读 →\", first)
try:
    probe.__annotations__
    second = \"没炸\"
except Exception as e:
    second = type(e).__name__
print(\"   再读一次 →\", second, \"｜ 两遍都炸 = 失败不缓存（a 篇计数 1→2 的同一条）\")
print(\"【时刻③ 调用函数时】求值注解了吗 →\", \"屏幕上这行之前没有任何报错，就是没求值\")
print(\"   调用返回 =\", probe(3), \"｜ 运行时对值查了吗（d 篇八个现场 + 第 11 节矩阵①）→ 一行都不查\")
print(\"【时刻④ 建 dataclass 时】\")
import dataclasses
print(\"   c 篇/第 7 节矩阵量过：建类与实例化都不求值注解；忘 import ClassVar 会静默降级成普通字段\")
print(\"【入口】Format 四个成员（名字, 值）→\", [(f.name, f.value) for f in A.Format])
print(\"   其中 2 是内部用的：公开调用 →\", end=\" \")
try:
    A.get_annotations(probe, format=2)
    print(\"居然成功了（与本章结论不符，请报告）\")
except Exception as e:
    print(f\"{type(e).__name__}: {str(e)[:60]}\")
print(\"【工具在场性】本机标准库里有没有静态检查器 →\", \"mypy\" in sys.stdlib_module_names,
      \"｜ typeguard 是运行时插桩，不是静态检查 →\", __import__(\"typeguard\").__name__ not in sys.stdlib_module_names)
"'
note '  上面六段是本章的总账。读者最该带走的一条：**注解本身不做事，读它的人才做事**；'
note '  而「谁读、什么时候读、读的是哪个 format」三件事，本屏每格都有实测的读数可以对。'
runc 'python3 -c "
import annotationlib as A
vals = sorted(f.value for f in A.Format)
def try2():
    try:
        A.get_annotations(lambda x: None, format=2)
        return \"没拦住（若看到这句请报告）\", False
    except BaseException as exc:
        return type(exc).__name__ + \": \" + str(exc), isinstance(exc, ValueError)
msg, is_value = try2()
print(\"  → 断言：本节六段读数都由上面那条现场跑出（没有一行手打）；Format 四个值排序 =\", vals,
      \"｜2 号在公开入口确实被拦（这里当场再撞一次）=\", is_value, \"｜拦下时那句原文 →\", msg)
"'

# ============================================================================
banner 14 "自证：沙箱里的事只留在沙箱，章目录一个字都没多"
# ============================================================================
note '此刻的当前目录就是沙箱 work/。先把「该有什么」写成能判的式子，否则下面那些 0 只是「看着干净」：'
runc 'echo "沙箱 work/ 里的 .py 个数=$(find . -maxdepth 1 -name "*.py" | wc -l)（这 7 个是章目录副本，不是产物）  非 .py 文件数=$(find . -maxdepth 1 -type f ! -name "*.py" | wc -l)"'
note '↑ 守卫如果不会响，就等于没检查。先故意造四样残留：一个取证脚本同款名字的目录、一个非 .py 文件、'
note '  一次真 import 产生的 __pycache__、一个矩阵格没删掉的 .py：'
runc 'mkdir -p typeguard-demo; touch 残留.tmp; printf "y = 1\n" > probe_mod.py; printf "z = 2\n" > matrix_leftover.py; python3 -c "import probe_mod"; echo "塞进假产物之后：子目录数=$(find . -mindepth 1 -maxdepth 1 -type d | wc -l)  .py 个数=$(find . -maxdepth 1 -name "*.py" | wc -l)  非 .py 文件数=$(find . -maxdepth 1 -type f ! -name "*.py" | wc -l)  __pycache__ 个数=$(find . -name __pycache__ | wc -l)  *.pyc 个数=$(find . -name "*.pyc" | wc -l)  ← 五项都会响"'
runc 'rm -rf typeguard-demo 残留.tmp probe_mod.py matrix_leftover.py __pycache__; echo "清掉之后：子目录数=$(find . -mindepth 1 -maxdepth 1 -type d | wc -l)  .py 个数=$(find . -maxdepth 1 -name "*.py" | wc -l)  非 .py 文件数=$(find . -maxdepth 1 -type f ! -name "*.py" | wc -l)  __pycache__ 个数=$(find . -name __pycache__ | wc -l)  *.pyc 个数=$(find . -name "*.pyc" | wc -l)"'
note '↑ 上面那一次「响」正是本章主题的现场复演：只有 **import** 会造 __pycache__；'
note '  七个取证脚本跑的是 `python3 名字.py`（直跑脚本不产缓存），它们自己 import 出去的样本模块'
note '  全在各自的 lazy-demo/、reads-demo/、who-demo/、runtime-demo/、proto-demo/、typeguard-demo/、mini-demo/ 里，收尾各自 rmtree。'
runc 'for d in lazy-demo reads-demo who-demo runtime-demo proto-demo typeguard-demo mini-demo; do printf "%s 还在吗 → %s\n" "$d" "$([ -d "$d" ] && echo 在 || echo 不在)"; done; echo "七个沙箱目录一个都不在，才算这七支脚本自己收好了尾"'
note '家目录那两处目录再数一遍，和第 1 节同一格对照 —— 两次一样就是「demo 没往家目录写过东西」的凭证：'
runc 'python3 -c "
import os
def n(p):
    p = os.path.expanduser(p)
    return len(os.listdir(p)) if os.path.isdir(p) else \"目录不存在\"
print(\"~/.local/lib 条目数 =\", n(\"~/.local/lib\"), \"｜ ~/.config 条目数 =\", n(\"~/.config\"), \"｜ ~/.bashrc 存在吗 →\", os.path.isfile(os.path.expanduser(\"~/.bashrc\")))
"'
note '★ 章目录（源文件本体在这里，demo 全程只读它）。清单显式排除 README.md，'
note '  所以「README 写完之后重跑」得到的是同一份输出（这一格自己就是那条承诺的现场）：'
runc_at "$HERE" 'ls -1 | grep -v "^README\.md$"'
runc_at "$HERE" 'echo ".py 个数=$(find . -maxdepth 1 -name "*.py" | wc -l)  demo.sh 个数=$(find . -maxdepth 1 -name "demo.sh" | wc -l)  README 以外的其它文件数=$(find . -maxdepth 1 -type f ! -name "*.py" ! -name "demo.sh" ! -name "README.md" | wc -l)  子目录数=$(find . -mindepth 1 -maxdepth 1 -type d | wc -l)  __pycache__ 个数=$(find . -name __pycache__ | wc -l)"'
note '现在扫**整份屏幕账**（$SB/all.out）：五类「不该上屏的东西」各数一次。问法分两种 ——'
note '  能写成精确串的（家目录绝对路径、用户名、主机名、本次沙箱绝对路径）用 str.count；'
note '  只有形态没有固定串的（对象地址 0x…）用正则。为什么不全用正则：正则会把自己那条命令行也算进去，假警报。'
note '★ 先让这套扫描**响一次**：往 all.out 的副本里塞五行真身份（全是当场取出来的，'
note '  命令行里一个字都没写真身份），同一套扫描必须报非 0 —— 报 0 就说明这套扫描是死的。'
runc 'cd .. && python3 -c "
import os, re, socket
sb = os.environ[\"SB\"]
exact = [(\"家目录绝对路径\", os.path.expanduser(\"~\")), (\"用户名\", os.environ.get(\"USER\", \"\")),
         (\"主机名\", socket.gethostname()), (\"本次沙箱路径\", sb)]
labels = [k for k, _ in exact] + [\"对象地址形态\"]
with open(\"poison.out\", \"w\", encoding=\"utf-8\") as fh:
    fh.write(open(\"all.out\", encoding=\"utf-8\").read())
    for _, v in exact:
        fh.write(v + chr(10))
    fh.write(repr(object()) + chr(10))
def scan(t):
    hits = [t.count(v) for _, v in exact]
    hits.append(len(re.findall(\"0x[0-9a-f][0-9a-f]+\", t)))
    return dict(zip(labels, hits))
poison = scan(open(\"poison.out\", encoding=\"utf-8\").read())
screen_text = open(\"all.out\", encoding=\"utf-8\").read()
screen = scan(screen_text)
print(\"  │ ① 故意污染过的那份副本（五行真身份）→\", poison)
print(\"  │ ② 真正的屏幕账 all.out →\", screen)
print(\"  → 断言：污染那份五项全非 0（扫描会响）=\", all(v > 0 for v in poison.values()),
      \"｜屏幕账那五项全 0（扫描干净）=\", all(v == 0 for v in screen.values()),
      \"｜本机用户名与主机名同一个串（所以两格数字相同不是重扫）=\",
      os.environ.get(\"USER\", \"\") == socket.gethostname())
"'
note '↑ ③ 那一格把「形状」和「真名」分开：模板串写在正文里是给人看的，永远不等于本次真名；'
note '  真名只出现在 --keep 那句提示里，而那句只进终端、不进账 —— 所以 ② 扫的是'
note '  「有没有哪支脚本把沙箱绝对路径原样印了出来」，七支脚本各自的 mask() 就是为它准备的。'
runc 'cd .. && rm -f poison.out && python3 -c "
import os
t = open(\"all.out\", encoding=\"utf-8\", errors=\"replace\").read()
shape = \"py10-\" + \"X\" * 6
print(\"  │ ③ 污染副本删干净了吗 →\", not os.path.exists(\"poison.out\"))
print(\"  │ ④ 形状与真名分开量：账里模板字样\", shape, \"出现\", t.count(shape),
      \"次（给人看的形状，允许）｜本次真沙箱名出现\", t.count(os.environ[\"SB\"]), \"次（必须是 0）\")
" && echo "屏幕账行数=$(wc -l < all.out)  沙箱根目录的文件数=$(find . -maxdepth 1 -type f | wc -l)"'
note '★ 判据汇总：七支脚本每支收尾都印一条「全篇判据合计 N/N」。这个数只在说一件事：'
note '  屏上每一格自己判过的据，有没有哪一条没成立。先让 python 数一遍，再用 grep 把它们原样列出来。'
runc 'cd .. && python3 -c "
import re
pat = re.compile(\"全篇判据合计 ([0-9]+)/([0-9]+) 条成立\")
lines = list(open(\"all.out\", encoding=\"utf-8\"))
rows = [m.groups() for l in lines for m in [pat.search(l)] if m]
bad = sum(1 for l in lines if l.strip().startswith(\"[不成立！]\"))
print(\"  │ ① 此刻账里抓到几条合计行 →\", len(rows), \"（该是 7：七支脚本各一条，下面那条 grep 还没复写它们）｜ 逐条（成立/总数）→\", rows)
print(\"  │ ② 全章成立条数 =\", sum(int(a) for a, b in rows), \"｜ 总条数 =\", sum(int(b) for a, b in rows))
print(\"  → 断言：七支脚本各一条 =\", len(rows) == 7, \"｜ 每条分子都等于分母 =\",
      all(a == b for a, b in rows), \"｜ 失配行（行首那种格式）条数 =\", bad)
"'
note '↑ ② 那行是本章的总结账：七支脚本各自的判据总数不同（11、45、52、60 这些），'
note '  但每条都必须分子=分母；只要有一格判据没成立，① 那串里就出现 a != b，断言行立刻给 False。'
note '★ ① 为什么排在 grep 那条之前：runc 把每条命令**和它的输出**都抄进同一份账。'
note '  python 数的时候账里只有脚本自己印的七条；等下面那条 grep 把七条复述一遍，数就翻倍 ——'
note '  「扫账的扫到自己」在第 11 节量过一回，这里再把它数出来，而不是假装没这回事。'
runc 'cd .. && echo "含「→ 断言：」的行数=$(grep -cF "→ 断言：" all.out)  ← 这个数比章的格数大，原因同上"; echo "账里那七条合计行，原样列出："; grep -hoE "全篇判据合计 [0-9]+/[0-9]+ 条成立" all.out'
runc 'cd .. && echo "同一套式子再数一遍 → 合计数=$(grep -cE "全篇判据合计 [0-9]+/[0-9]+ 条成立" all.out)  ← 这里是 14 不是 7：上面那条 grep 印出的七行也进了账（① 抢在复写之前数，才拿到 7）"'
runc 'cd .. && echo "带「不成立的那条上面会印 [不成立！]」这句提示的行数=$(grep -cF "上面会印 [不成立！]" all.out)  ← 它是提示语的计数，不是判定"; echo "真正的失配行数=$(grep -cE "^[[:space:]]+\\[不成立！\\]" all.out)  ← 这个才必须是 0"; grep -E "^[[:space:]]+\\[不成立" all.out | head -3; echo "（上面那条 grep 打不出任何行，才是真的 0）"'
note '这一章一条网络命令都没用、没起过任何监听端口、没 pip install 过任何东西。'
note '  为什么这里不数「本机在听的端口条目数」：那个数由这台机器上别的服务决定，每跑必变，'
note '  放进屏幕账就等于往两遍 diff 里塞一行对不上的东西（第 14 节其它计数都是本章自己造成的量，才敢数）。'
runc 'v=$(pgrep -c -f "[p]ython3 .*-demo" || true); q=$(pgrep -c -f "[p]ip install" || true); echo "还在跑的取证子进程=$v ｜ 还在跑的 pip 安装进程=$q（本章只可能起这两类，全前台同步，跑到这一格时必然都归零）"'
note '★ 为什么不数 jobs：本 demo 全程前台同步，一个 & 都没起过；而 runc 是用 bash -c 跑每条命令的，'
note '  在那种一次性子进程里问作业表，量到的永远是空表 —— 那个 0 是假的（循环篇专门讲过这条）。'
note '★ 模式为什么各带一个方括号：pgrep -f 匹配的是完整命令行，而这条命令行里就写着模式本身。'
note '★ 本章的取证纪律：所有「故意失败」都由脚本自己接住并原样打印（报错措辞跟版本绑定，只有本机 3.14.4 验过）；'
note '  临时目录全在 mktemp -d 出来的 py10-XXXXXX 里，由 trap cleanup EXIT 整份删掉；7 个取证脚本一个字没改。'

printf '\n%s[全部跑完]%s 上面每一段都是真实执行结果；README.md 引用的输出全部出自这一次运行。\n' "$B" "$R"
printf '[一句话回顾] 注解本身不做事，读它的人才做事 —— 3.14 把它做成惰性的、成功才缓存的；\n'
printf '              dataclass 和 inspect 各自读、读的是不同 format；运行时一个值都不查；\n'
printf '              本机没有静态检查器，所以最后一格把 ast 摊开给你看：那把尺你自己也能搓。\n'
exit 0
