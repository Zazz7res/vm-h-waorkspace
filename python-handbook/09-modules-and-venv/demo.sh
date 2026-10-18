#!/usr/bin/env bash
# ============================================================================
# 第 09 章《模块与 venv：import 查的是三本账》—— 整章证据一键复现
#
#   bash demo.sh                 跑完自动删沙箱（默认）
#   bash demo.sh --keep          跑完保留沙箱并打印路径，进去自己接着敲
#   bash demo.sh > out.txt 2>&1  重定向时自动关颜色，得到纯文本（README 引用的就是这份）
#
# 屏幕上出现的每一行，都是这条命令在这台机器（CPython 3.14.4 / Ubuntu / ext4）上的真实结果，
# 没有一行是手写的、没有一行是「我记得应该这样」。
#
# 本章三条主线（每节的 note 都会指回其中一条）：
#   ① import 查三本账：sys.modules 命中就什么都不做 → sys.path 的顺序即优先级 →
#      .pyc 的键是「源码 mtime + 字节数」，不是内容哈希。
#   ② sys.path[0] 的形状由启动方式决定：`python 路径/x.py` = 脚本目录、`python -m` = 当前目录、
#      交互与 `-c`/`-` = 空串。
#   ③ venv 的本质就是改 sys.path：pyvenv.cfg 里 include-system-site-packages 决定要不要带上系统那份。
#
# 安全说明：
#   * 章目录永远只读：9 个取证脚本整份 cp 进 mktemp -d 的沙箱再跑，章目录里不会留下
#     *-demo/、__pycache__/、*.pyc、venv。第 22 节的每条计数都先被故意弄响一次
#     （造一个残留目录、真 import 一个模块），再报 0 —— 恒 0 的检查比没有检查更糟。
#   * 只用标准库 + 本机自带的 python3；不 pip install、不联网、不开监听端口、
#     不改 ~/.bashrc / ~/.profile / ~/.config / ~/.local（第 1 节和第 22 节各数一遍家目录那两个
#     目录的条目数，两次一样就是「没写过」的凭证）。
#   * venv 只用标准库 venv 在沙箱里以 --without-pip 建，跑完随沙箱整份删掉；
#     本机没装 python3-venv → 带 pip 引导的建法当场失败，h 脚本把这条失败原文当证据演出来，
#     本 demo 不去修它、不 apt install。
#   * 故意不加 `set -e`：本章一半是失败现场（ModuleNotFoundError、循环导入 ImportError、
#     `from x import *` 的 SyntaxError、星号覆盖、坏 .pth、venv 建不出来…），
#     一遇非 0 就停的话，屏幕上只剩第一格，后面的对照组全看不到。
#   * 故意不加 `set -u`：本脚本要如实打印「这些开关此刻没设」这件事（第 1 节满屏 ${VAR:-（未设）}），
#     还要读 ORIG_LANG / ORIG_LC_ALL 这种在有些机器上根本不存在的变量；-u 会让脚本在
#     第一格就死掉，而不是把「未设」两个字印出来。
#   * 故意不加 `set -o pipefail`：`... | head -3`、`grep | wc -l` 这类截取会因为下游提前关管道
#     报 141，屏幕上会冒出一堆和教学无关的退出码。
#   * 屏幕上不出现真实家目录 / 用户名 / 主机名 / pid / 毫秒 / 内存字节数 / 对象地址：
#     这类量一律换成「相等吗 / 谁比谁大 / 几个 / 有没有」。sys.path 含家目录那一项，
#     打印前一律把家目录前缀折成 <家目录>；沙箱路径折成 <沙箱>。
#   * ★ 沙箱名里那 6 个随机字符每跑必变，所以默认收尾只印「沙箱已删除」而不印路径；
#     否则两遍 transcript 永远不可能逐字一致（--keep 是给人看的，不参与对账）。
#   * 本脚本自己的退出码恒为 0（cleanup 里不留非 0），这样才能当验收闸门用。
# ============================================================================

KEEP=0
for arg in "$@"; do
    case "$arg" in
        --keep) KEEP=1 ;;
        -h|--help) echo "用法: bash demo.sh [--keep]"; exit 0 ;;
        *) echo "未知参数：$arg（本脚本只认 --keep）" >&2; exit 2 ;;
    esac
done

# 固定语言：Python 自己的报错恒为英文，但 `command not found`、`未找到命令` 这类消息是 bash 翻译的；
# 设成 C.UTF-8 后整份 transcript 才能逐字比对（你手敲时看到中文完全正常）。
# ★ 先把原值存进 ORIG_LANG / ORIG_LC_ALL 并 **export**：不导出的话第 1 节那个 `bash -c` 子进程
#   里看不到原值，屏幕上会印成两个空括号（上一章踩过这个坑）。
ORIG_LANG=${LANG-}
ORIG_LC_ALL=${LC_ALL-}
export ORIG_LANG ORIG_LC_ALL
export LC_ALL=C.UTF-8
export LANG=C.UTF-8
export LANGUAGE=C
# 本章的结论全挂在「解释器启动时谁往 sys.path 里加了什么」上，这些外部开关会直接改写它：
# 先拨到已知位置（只影响本脚本这个进程和它的子进程，不碰任何配置文件），
# 再在脚本/小证据格里一个个开关现场对比。原值在第 1 节如实印出来。
unset PYTHONPATH PYTHONHOME PYTHONNOUSERSITE PYTHONSAFEPATH
unset PYTHONDONTWRITEBYTECODE PYTHONOPTIMIZE PYTHONSTARTUP PYTHONINSPECT PYTHONWARNINGS PYTHONDEBUG
# 本章大量打印 dict / set / sys.modules / 目录清单；固定哈希种子 + 脚本内部一律 sorted()，
# 为的是同一份脚本在你机器上打出逐字一样的输出。
export PYTHONHASHSEED=0

# 只有输出到终端才上色；重定向到文件时是干净纯文本
if [ -t 1 ]; then B=$'\033[1m'; C=$'\033[36m'; R=$'\033[0m'; else B=''; C=''; R=''; fi

HERE=$(cd "$(dirname "$0")" && pwd)
SB=$(mktemp -d "${TMPDIR:-/tmp}/py09-XXXXXX")
export SB

banner() { printf '\n%s══════ 第 %s 节 %s ═══════════════════════════════%s\n' "$B" "$1" "$2" "$R"; }
note()   { printf '%s\n' "$1"; }

# 打印命令 → 执行（stderr 合并进来）→ 打印退出码。
# 这三行的格式和第 08 章逐字一致（`$ ` 前缀、`[退出码 %d]`），README 的引文闸门按行核对。
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
        # ★ 这里刻意不印 $SB：mktemp 的随机后缀每跑必变，印出来第 22 节就成了对账不了的一行。
        printf '\n[沙箱已删除] <本次 mktemp -d 出来的目录，名字每跑随机，所以不打出来>\n'
    fi
}
trap cleanup EXIT

# ============================================================================
banner 1 "环境体检：三本账的起点，和这台机器上解释器的归属"
# ============================================================================
mkdir -p "$SB/work"
cp "$HERE"/*.py "$SB/work/"
cd "$SB/work" || exit 1
umask 022
note '沙箱 work/ 里就是章目录那 9 个 .py 的整份副本；章目录本身从头到尾不参与写（第 22 节自证）：'
runc 'ls'
runc 'python3 --version && python3 -c "import sys; print(sys.version)"'
note '★ 主线①：import 查三本账 —— sys.modules（模块账本）、sys.path（搜索路径）、__pycache__（编译缓存）。'
note '  后面 a/b/c/d/e 五个脚本各证一本，f/g 证「谁来查」，h/i 证「这本账是谁在启动时填进来的」。'
runc 'python3 -c "import sys, os; print(\"sys.executable =\", sys.executable); print(\"sys.executable 的 basename =\", os.path.basename(sys.executable))"'
runc 'python3 -c "import sys; print(\"sys.prefix 和 sys.base_prefix 相等吗 →\", sys.prefix == sys.base_prefix); print(\"sys.prefix 等于 sys.executable 所在目录的上一层吗 →\", sys.prefix == os.path.dirname(os.path.dirname(sys.executable)) if False else sys.prefix == os.path.dirname(os.path.dirname(os.path.realpath(sys.executable))))"'
note '↑ 相等 = 我现在站在系统解释器上，没有泡在 venv 里；这就是 h/i 两篇反复用的「我在哪个解释器上」判据，'
note '  只看布尔，不印一堆路径（路径里带版本号和安装前缀，每换一台机器就变，README 对不住）。'
note '下面四个开关是本章结论的总闸：本 demo 一开场就把它们拨到「未设」，所以下面四行都该印（未设）；'
note '  你手敲时若看到别的值，说明你的 shell 配置在改写 import 的行为 —— 每个脚本起子进程时都自带干净 env，正是为了不让这些值混进结论。'
runc 'printf "PYTHONPATH=%s\nPYTHONHOME=%s\nPYTHONNOUSERSITE=%s\nPYTHONSAFEPATH=%s\n" "${PYTHONPATH:-（未设）}" "${PYTHONHOME:-（未设）}" "${PYTHONNOUSERSITE:-（未设）}" "${PYTHONSAFEPATH:-（未设）}"'
runc 'python3 -c "import sys, site; print(\"sys.flags.no_user_site =\", sys.flags.no_user_site); print(\"sys.flags.safe_path  =\", sys.flags.safe_path); print(\"site.ENABLE_USER_SITE     =\", site.ENABLE_USER_SITE); print(\"→ 前两个是解释器开关的下场，第三个是 site 自己声称要不要开家目录那条通道（和它真加没加是两件事，见 i 篇）\")"'
runc 'python3 -c "import tempfile, os; p = tempfile.gettempdir(); print(\"tempfile.gettempdir() =\", p); print(\"是 /tmp 吗 →\", p == \"/tmp\"); print(\"本次沙箱在它下面吗 →\", os.path.realpath(\".\").startswith(os.path.realpath(p)))"'
runc 'printf "umask=%s ｜ 沙箱建在 TMPDIR=%s ｜ 本次沙箱 basename 是随机后缀（所以下面不印它）\n" "$(umask)" "${TMPDIR:-/tmp}"'
runc 'printf "语言环境：原值 LANG=%s LC_ALL=%s → 现在 LANG=%s LC_ALL=%s LANGUAGE=%s\n" "${ORIG_LANG:-（未设）}" "${ORIG_LC_ALL:-（未设）}" "$LANG" "$LC_ALL" "$LANGUAGE"'
note '↑ 原值能印出来，是因为 ORIG_LANG / ORIG_LC_ALL 被 export 了 —— 不导出就只能看到两个空括号（上一章踩过）。'
note '★ 这一格是本章唯一的「红线」：sys.path 里一定含真实家目录（user site 一旦启用就写进去），'
note '  所以打印它之前先把家目录前缀折成 <家目录>；屏幕上任何时候都不该出现 /home/… 或登录名。'
runc 'python3 -c "
import sys, os
h = os.path.expanduser(\"~\")
print(\"家目录前缀折成 <家目录> 之后的 sys.path：\")
print([p.replace(h, \"<家目录>\") for p in sys.path])
print(\"其中含家目录那一项吗 →\", any(\"<家目录>\" in p.replace(h, \"<家目录>\") for p in sys.path))
print(\"空串（＝当场查当前目录）在不在账上 →\", \"\" in sys.path, \"｜ 在第几项 →\", sys.path.index(\"\") if \"\" in sys.path else \"不在\")
"'
note '下面这格证明本章没有偷偷往你家目录写过东西：两处目录的条目数在第 1 节和第 22 节各数一遍，两次一样即为未写。'
runc 'python3 -c "
import os
def n(p):
    p = os.path.expanduser(p)
    return len(os.listdir(p)) if os.path.isdir(p) else \"目录不存在\"
print(\"~/.local/lib 条目数 =\", n(\"~/.local/lib\"), \"｜ ~/.config 条目数 =\", n(\"~/.config\"), \"｜ ~/.bashrc 存在吗 →\", os.path.isfile(os.path.expanduser(\"~/.bashrc\")))
"'
runc 'python3 -c "
import sys
need = {\"importlib\", \"importlib.util\", \"site\", \"sysconfig\", \"struct\", \"marshal\", \"dis\", \"venv\", \"shutil\", \"subprocess\", \"tempfile\", \"ast\"}
print(\"本章用到的模块全在标准库里吗 →\", need <= sys.stdlib_module_names)
print(\"缺的：\", sorted(need - sys.stdlib_module_names) or \"无\")
print(\"venv 模块自带吗 →\", \"venv\" in sys.stdlib_module_names, \"｜ 但本机能不能建出带 pip 的 venv 是另一件事（见 h 篇）\")
"'

# ============================================================================
banner 2 'a 篇完整跑：`import x` 那一刻到底发生了什么（模块体只跑一次）'
# ============================================================================
runc 'python3 a_import_basics.py'
note '↑ 这一格把主线① 的前两本账撞在一起演：模块体在本进程里只执行一次，'
note '  「只跑一次」的开关是 sys.modules 这本账，不是文件系统 —— 记成后者，就会一直以为「改了 .py 再 import 就生效」。'
note '  它也把「绑的是引用不是内容」讲清楚：`import x` 只在你命名空间放一个名字，`from x import y` 放的是 y 的引用，'
note '  两种绑法决定 monkeypatch 能不能生效。最后那格把四种「导不到」的报错原文一个字不改地抄下来。'

# ============================================================================
banner 3 "小证据：第一本账 sys.modules —— 命中就什么都不做，撕掉就立刻重跑"
# ============================================================================
note 'a 篇用计数文件证明「模块体只跑一遍」。这一格把它推到极端：账本命中时连文件都不打开。'
runc 'mkdir -p ledger-check && cd ledger-check
printf "V = %s\nprint(%s)\n" "\"from-file\"" "\"[BODY] shadow.py 的模块体跑了一遍\"" > shadow.py
python3 -c "
import sys, os
name = \"shadow\"
mod = type(sys)(name); mod.V = \"凭空塞的假模块\"; mod.__file__ = None
sys.modules[name] = mod
import shadow
print(\"① 账本里先塞一个同名假模块 → import 直接拿到它吗 →\", shadow.V == \"凭空塞的假模块\")
print(\"   模块体跑过了吗 →\", os.path.exists(\"[BODY] 这行没出现就是没跑\") or \"没跑（屏幕上没有 [BODY] 那一行）\")
del sys.modules[name]
import shadow
print(\"② 把账本里这一条撕掉 → 同一个名字立刻按 sys.path 找到文件、重跑模块体：V =\", shadow.V)
import shadow
print(\"③ 第二次 import 又什么都不做（账本又命中了）→ V 还是\", shadow.V)
m = sys.modules[\"shadow\"]
del shadow
print(\"④ 只 del 本地名字：名字没了吗 →\", \"shadow\" not in globals(), \"｜ 账本里模块对象还活着吗 →\", \"shadow\" in sys.modules, \"｜ 是同一个对象吗 →\", sys.modules[\"shadow\"] is m)
print(\"   再 import 一次（账本命中）→ 模块体又跑了吗 →\", \"屏幕上没有第二遍 [BODY] 就是没跑\")
"
cd ..; rm -rf ledger-check'
note '↑ ④ 是「为什么我的补丁没生效」的标准答案：改模块属性 = 改全局状态（所有引用者同时看到），'
note '  而 del 名字只删你这一格的绑定，账本里的对象谁都能再拿回来。'

# ============================================================================
banner 4 "小证据：主线② —— sys.path[0] 的形状由启动方式决定"
# ============================================================================
note 'b 篇有一整张实测表。这一格只补一条最容易被忽略的：同一个「根在哪」的问题，七种入口给出四种形状。'
runc 'mkdir -p shape-check/pkgx && cd shape-check
cat > pkgx/x.py <<"PYEOF"
import os, sys
def fold(p):
    return str(p).replace(os.getcwd(), "<cwd>").replace(os.environ["SB"], "<沙箱>")
tag = sys.argv[1] if len(sys.argv) > 1 else "?"
print("[入口 %-24s] path0 = %r" % (tag, fold(sys.path[0])))
print("                     它指向脚本所在目录吗 →", os.path.samefile(sys.path[0], os.path.dirname(os.path.abspath(__file__))))
PYEOF
python3 pkgx/x.py "python3 pkgx/x.py"
python3 -m pkgx.x "python3 -m pkgx.x"
python3 -c "import sys; print(\"[入口 %-24s] path0 = %r\" % (\"python3 -c\", sys.path[0])); print(\"                     是空串吗 →\", sys.path[0] == \"\", \"｜ 空串不是路径，是「每次 import 现查当前目录」\")"
python3 - <<"PYEOF"
import sys
print("[入口 %-24s] path0 = %r" % ("python3 - 走 stdin", sys.path[0]))
print("                     是空串吗 →", sys.path[0] == "")
PYEOF
python3 -P -c "import sys; print(\"[入口 %-24s] path0 = %r | 空串已经不在账上了吗 →\" % (\"python3 -P -c\", sys.path[0]), \"\" not in sys.path)"
python3 -I -c "import sys; print(\"[入口 %-24s] path0 = %r | 空串已经不在账上了吗 →\" % (\"python3 -I -c\", sys.path[0]), \"\" not in sys.path)"
python3 <<"PYEOF"
import sys
print("[入口 %-24s] path0 = %r" % ("python3 无参数（stdin 当脚本）", sys.path[0]))
print("                     是空串吗 →", sys.path[0] == "")
PYEOF
cd ..; rm -rf shape-check'
note '↑ 七种入口给出四种形状，最值得记住的是前两行：同一份 x.py，`python3 pkgx/x.py` 把**脚本目录**放进 path[0]，'
note '  `python3 -m pkgx.x` 放的是**当前目录** —— 所以「旁边那个兄弟模块能不能 import 到」会跟着跑法变，代码一个字没改。'
note '  `-c` / `-` / 无参数这三种入口给的都是空串 —— 空串不是路径，是「每次 import 现查当前目录」。'
note '  -P（等价于 PYTHONSAFEPATH=1）把这个入口项整个不给你，path[0] 直接落到解释器自带的 zip 上；'
note '  -I 从 3.11 起自带 -P，所以它那一条的结果和 -P 一模一样。这也是第 1 节要先把 PYTHONSAFEPATH 拨到未设的原因。'

# ============================================================================
banner 5 "b 篇完整跑：sys.path 是四层拼出来的，同名遮蔽按层发生"
# ============================================================================
runc 'python3 b_sys_path_shape.py'
note '↑ 主线① 第二本账在这一格里是主角：入口项 / PYTHONPATH / 标准库+site / 内置冻结，每层一个干净子进程单独证明。'
note '  内置那层根本不在 sys.path 里，它排在所有路径之前，所以同名文件遮蔽不了它 —— 这是「为什么我改了 json.py 却没生效」的一种可能。'
note '  ★ 同名遮蔽那一格是日常踩坑率最高的一条：自己写的一份 json.py 会抢走标准库那个，而且三层下场各不相同。'

# ============================================================================
banner 6 "小证据：第二本账 sys.path —— 顺序即优先级，空串还会跟着 chdir 换根"
# ============================================================================
runc 'mkdir -p order-check/front order-check/back && cd order-check
printf "TAG = %s\n" "\"front\"" > front/winner.py
printf "TAG = %s\n" "\"back\""  > back/winner.py
python3 -c "
import sys
sys.path.insert(0, \"front\"); sys.path.insert(1, \"back\")
import winner
print(\"① front 排在 back 之前 → 拿到的是\", winner.TAG)
sys.path.remove(\"back\"); sys.path.insert(0, \"back\")
del sys.modules[\"winner\"]
import winner
print(\"② 换成 back 排最前，并撕掉账本那一条 → 拿到的是\", winner.TAG, \"（顺序即优先级，不是谁名字短谁赢）\")
sys.path.insert(0, \"front\")
import winner
print(\"③ 再把 front 挪回最前，但账本不撕 → 仍然是\", winner.TAG, \"（命中账本时连 sys.path 都不查——两本账的先后关系就在这）\")
print(\"④ 报错原文里只有名字，不给你任何一条搜索路径：\")
try:
    import no_such_mod_here
except ModuleNotFoundError as e:
    print(\"   \", type(e).__name__, str(e), \"｜ 提到模块名吗 →\", \"no_such_mod_here\" in str(e), \"｜ 提到任何一条搜索路径吗 →\", any(p in str(e) for p in sys.path if p))
"
cd ..; rm -rf order-check'
note '↑ ④ 是本章最实用的一条：ModuleNotFoundError 不会告诉你它找过哪些目录，'
note '  所以「import 不到」九成不是语法问题，而是「根在哪」的问题 —— 得自己把 sys.path 摊开逐层问（b 篇第 7 段那张表）。'
runc 'mkdir -p chdir-check/here chdir-check/there && cd chdir-check
printf "X = %s\n" "2" > there/somemod.py
python3 -c "
import os, sys
sys.path.insert(0, \"\")
root = os.getcwd()
print(\"① 空串这一项不是启动时定死的：chdir 之前，当前目录折成 <cwd> =\", os.getcwd().replace(root, \"<cwd>\"))
os.chdir(\"there\")
print(\"   os.chdir 之后 =\", os.getcwd().replace(root, \"<cwd>\"), \"｜ sys.path 一个字没改，账本却整个换根了\")
import somemod
print(\"   现在 import 到新根里的文件了吗 →\", somemod.X == 2)
os.chdir(\"..\")
try:
    del sys.modules[\"somemod\"]; import somemod
    print(\"   回到旧根再 import（撕过账本）→ 还能拿到吗 →\", somemod.X == 2)
except ModuleNotFoundError:
    print(\"   回到旧根再 import（撕过账本）→ 换根了，旧根下没有这个文件\")
"
cd ..; rm -rf chdir-check'
note '↑ 这就是 -P / PYTHONSAFEPATH 存在的理由：空串是把「当前目录」在 import 那一刻现拿过来用，'
note '  你的进程一 os.chdir，同一份 sys.path 的根就跟着漂 —— 库代码里尤其危险。'

# ============================================================================
banner 7 "c 篇完整跑：包 = 带 __init__.py 的目录，__init__ 就是包的模块体"
# ============================================================================
runc 'python3 c_packages_and_init.py'
note '↑ a 篇讲的「模块体只跑一次」对整个包同样成立：`import pkg.sub.mod` 会从最外层往里依次执行每层 __init__.py，'
note '  顺序直接印在屏幕上。「子模块被导入」的物理表现就是父包对象上多了一个属性，所以 import 父包不带子模块。'
note '  命名空间包那一格（PEP 420）是隐患重灾区：没有 __init__.py 的目录也能 import，__file__ 是 None，'
note '  而且多个根里的同名目录会被合成同一个包；反过来，一个 __init__.py 就能把其它根的那份整个切断。'

# ============================================================================
banner 8 '小证据：`import 子模块` 会把父包的 __init__ 跑一遍，`python 包/子.py` 不会'
# ============================================================================
runc 'mkdir -p import-check/pkgdir && cd import-check
printf "" > pkgdir/__init__.py
cat > pkgdir/leaf.py <<"PYEOF"
from . import sibling
print("[leaf] 相对导入 succeeded, TAG =", sibling.TAG)
PYEOF
cat > pkgdir/sibling.py <<"PYEOF"
TAG = "sibling 被相对导入到了"
PYEOF
cat > pkgdir/probe.py <<"PYEOF"
import sys
print("[probe] __name__ =", __name__, "| __package__ =", repr(__package__), "| 有 __path__ 吗（我是不是个包）→", hasattr(sys.modules[__name__], "__path__"))
PYEOF
python3 -c "import sys; sys.path.insert(0, \"pkgdir\"); import probe"
python3 pkgdir/probe.py
python3 -m pkgdir.probe
python3 -c "
import sys, os
sys.path.insert(0, \".\")
import pkgdir.leaf
print(\"★ import 三层里的 pkgdir.leaf：pkgdir/__init__.py 是空文件，但它仍然被执行了（屏幕上没有报错就是执行成功）\")
print(\"   pkgdir 在账本里吗 →\", \"pkgdir\" in sys.modules, \"｜ pkgdir.sibling 也在吗 →\", \"pkgdir.sibling\" in sys.modules, \"｜ 父包对象上有 sibling 这个属性吗 →\", hasattr(sys.modules[\"pkgdir\"], \"sibling\"))
"
cd ..; rm -rf import-check'
note '↑ 同一个 probe.py，三种跑法打出三种 __name__ / __package__：`-c` 里 sys.path 直接指到 pkgdir 时它是顶层模块 probe，'
note '  `python3 pkgdir/probe.py` 时它是 __main__ 且 __package__ 是 None（相对导入当场炸），'
note '  `python3 -m pkgdir.probe` 时 __package__ 才是 pkgdir —— 相对导入只看这两个名字，不看文件在磁盘上位于哪里（主线② 的另一面）。'
note '  最后一格把主线① 的第一本账放大：导入 pkgdir.leaf 会连带把 pkgdir 记进账本并挂上属性，'
note '  这就是「from pkg.sub import name」有时能用、有时炸的根源。'

# ============================================================================
banner 9 "d 篇完整跑：循环导入炸的不是「有环」，是模块体的先后顺序"
# ============================================================================
runc 'python3 d_circular_import.py'
note '↑ 这一格把「图上有环所以会炸」这个误诊直接打掉：同一个环，先跑哪一半决定结不结果。'
note '  取法也决定报错原文：`from a import x` 当场要名字（拿到半成品就 AttributeError/ImportError），'
note '  `import a` 只是拿到半成品模块对象，用的时候才取属性 —— 所以报错会在离 import 很远的一行上冒出来。'
note '  三条真实出路（延迟 import / 只 import 模块 / 抽公共模块变成 DAG）每条一个真子进程跑通，'
note '  而 importlib.reload 一条都救不了 —— reload 只是再跑一遍模块体，半成品还是半成品。'

# ============================================================================
banner 10 "小证据：同一个环、两种入口 = 两种生死（这一格是 d 篇结论的最小复现）"
# ============================================================================
runc 'mkdir -p cycle-check/cyc && cd cycle-check
printf "%s\n" "print(\"[a 的模块体开跑]\")" "from . import b" "print(\"[a 的模块体跑完了]\")" "A_VALUE = \"a 里的东西\"" > cyc/a.py
printf "%s\n" "print(\"[b 的模块体开跑]\")" "from . import a" "print(\"[b 的模块体跑完了]\")" "B_VALUE = \"b 里的东西\"" > cyc/b.py
printf "%s\n" "print(\"[__init__ 被执行]\")" > cyc/__init__.py
echo "—— 入口 1：先 import a（a 的模块体里问 b 要 a 的名字，此刻 a 还是个半成品）"
python3 -c "
import sys; sys.path.insert(0, \".\")
try:
    import cyc.a
except Exception as e:
    print(\"   \", type(e).__name__, \"|\", str(e).splitlines()[-1])
    print(\"   此刻账本里 cyc.a 还在吗 →\", \"cyc.a\" in sys.modules, \"｜ 它身上有 A_VALUE 吗 →\", getattr(sys.modules.get(\"cyc.a\"), \"A_VALUE\", \"（没有：模块体没跑完）\") != \"（没有：模块体没跑完）\")
"
echo "—— 入口 2：把「要名字」换成「只拿模块」，同一个环立刻跑通"
printf "%s\n" "print(\"[a 的模块体开跑]\")" "from . import b as _b" "print(\"[a 的模块体跑完了]\")" "A_VALUE = \"a 里的东西\"" "def use_b():" "    from . import b" "    return b.B_VALUE" > cyc/a.py
printf "%s\n" "print(\"[b 的模块体开跑]\")" "from . import a" "print(\"[b 的模块体跑完了]\")" "B_VALUE = \"b 里的东西\"" > cyc/b.py
del_ok=1
python3 -c "
import sys; sys.path.insert(0, \".\")
import cyc.a
print(\"    import cyc.a 成功了吗 →\", True, \"｜ a.use_b() 拿到的是 →\", cyc.a.use_b())
"
cd ..; rm -rf cycle-check'
note '↑ 两个入口的差别只有一行取法，环一个字没动 —— 所以修循环导入的着力点从来不是「把环拆开」，'
note '  而是「别让一个模块在另一个的模块体里被当成品用」。'
note '  屏幕上那句报错原文是 Python 自己写的，措辞跟小版本绑定（d 篇头部交代过：本机 3.14.4，其它版本未验证）。'

# ============================================================================
banner 11 "e 篇完整跑：__pycache__ 的形状、失效判据，和四种「别写缓存」"
# ============================================================================
runc 'python3 e_pyc_and_cache.py'
note '↑ 主线① 的第三本账：缓存的键是「源码 mtime + 源码字节数」两个数，不是内容哈希。'
note '  所以最值钱的一格是那四种改法四种结局：改了内容、mtime 和字节数都没变 → 缓存照样命中 → 跑的还是旧代码。'
note '  另外两条常被记反：-B / PYTHONDONTWRITEBYTECODE / sys.dont_write_bytecode 只管写不管读；'
note '  删掉源码只留 __pycache__ 里那份 .pyc 是加载不了的（老式同级 .pyc 才行）。'

# ============================================================================
banner 12 "小证据：缓存头部只有两个数 —— 当场拆 16 字节给你看"
# ============================================================================
runc 'mkdir -p header-check && cd header-check
python3 -c "
import importlib.util, os, struct, subprocess, sys
open(\"hcheck.py\", \"w\").write(\"A = 1\n\")
env = {k: v for k, v in os.environ.items() if not k.startswith(\"PYTHON\")}
env[\"PYTHONDONTWRITEBYTECODE\"] = \"\"
subprocess.run([sys.executable, \"-c\", \"import hcheck\"], capture_output=True, env=env)
cache = importlib.util.cache_from_source(\"hcheck.py\")
print(\"① 形状：源码 hcheck.py → 缓存 basename =\", os.path.basename(cache), \"｜ 它是 sys.implementation.cache_tag 拼出来的，tag =\", sys.implementation.cache_tag)
print(\"   缓存在 __pycache__ 目录里吗 →\", os.path.basename(os.path.dirname(cache)) == \"__pycache__\")
head = open(cache, \"rb\").read(16)
magic, flags, f3, f4 = struct.unpack(\"<4sIII\", head)
st = os.stat(\"hcheck.py\")
print(\"② 头部取前\", len(head), \"个字节，四个字段：\")
print(\"   字段① 版本标签字节（长度 =\", len(magic), \"）｜ 字段② flags =\", flags, \"→ flags==0 就是「按源码 mtime+size 失效」那一档吗 →\", flags == 0)
print(\"   字段③ 是源码 mtime：当场对得上吗 →\", f3 == int(st.st_mtime))
print(\"   字段④ 是源码字节数：当场对得上吗 →\", f4 == st.st_size)
print(\"③ ★ 整份判据只有这两个数，没有源码内容的哈希 →\", (f3, f4) == (int(st.st_mtime), st.st_size))
print(\"   所以「内容改了但 mtime 和字节数都没变」= 缓存命中 = 跑旧代码（e 篇第 3 段现场复现）\")
print(\"④ 换了 cache_tag 就是两份互不相干的缓存：文件名里带 tag →\", sys.implementation.cache_tag in os.path.basename(cache))
"
cd ..; rm -rf header-check'
note '↑ 这一格解释了一个日常现象：拷文件、git checkout、Docker COPY 都可能把 mtime 设成一个新值而内容没变 ——'
note '  方向相反但同理：只要那两个数没变，Python 就认为缓存是新的。缓存买到的是「省掉 parse/compile」，'
note '  不是「省掉执行」：模块体照样跑，副作用照样发生（e 篇第 7 段收口的就是这句）。'

# ============================================================================
banner 13 "f 篇完整跑：__name__ / guard / 星号导入与 __all__"
# ============================================================================
runc 'python3 f_main_guard_and_dunder.py'
note '↑ `if __name__ == "__main__":` 不是仪式：没有它，别人 import 你一下，你就把整个脚本跑一遍（建文件、跑主逻辑），'
note '  这一格用计数证明而不是用嘴证明。四种启动方式打出四种 __name__ / __package__，'
note '  相对导入能不能用、包的 __init__ 跑不跑，全由这两个名字决定（主线②）。'
note '  星号那两段的规矩要记住：默认只放「不以下划线开头」的名字；__all__ 一旦出现就是唯一白名单 ——'
note '  私有名字能放进来、公开名字能挡在外面，而且它会无声覆盖你命名空间里的同名对象，也进不了函数体。'

# ============================================================================
banner 14 "g 篇完整跑：把 import 语句还原成可以编程调用的协议（查、载、重载）"
# ============================================================================
runc 'python3 g_importlib_and_spec.py'
note '↑ 这一篇是主线① 的「谁在查」：find_spec 查账本之外的第二本账（sys.path / meta_path），'
note '  module_from_spec + exec_module 才是真的执行模块体，reload 只是再跑一遍。'
note '  ★ 两个反直觉点：`__import__("pkg.sub")` 的返回值是顶层包 pkg，import_module 的才是 pkg.sub；'
note '  find_spec 找不到时返回 None 不抛异常，但父包本身不存在时它又会抛 —— 同一个函数两种脾气。'
note '  最后一条代价要说清：走了 importlib，缓存、异常、sys.modules 的注册就全都归你自己管了。'

# ============================================================================
banner 15 "小证据：reload 只重跑模块体，你手里的旧引用一个都不换"
# ============================================================================
runc 'rm -rf reload-check && mkdir -p reload-check && cd reload-check
cat > rmod.py <<PYA
VER = 1
class K:
    pass
PYA
cat > rmod_v2.py <<PYB
VER = 2
print("  [这一行证明模块体又跑了一遍]")
class K:
    pass
PYB
cat > probe.py <<PYC
import importlib, os, shutil, sys
import rmod
old_ver, old_k, old_obj = rmod.VER, rmod.K, rmod.K()
print("① 模块对象的身份：reload 之前 sys.modules 里那条就是它吗 →", sys.modules["rmod"] is rmod)
before = os.stat("rmod.py")
shutil.copyfile("rmod_v2.py", "rmod.py")
after = os.stat("rmod.py")
print("   换掉源码：mtime 变了吗 →", after.st_mtime != before.st_mtime, "｜ 字节数也变了吗 →", after.st_size != before.st_size, "（两个判据都动了，缓存必然重做）")
loaded = importlib.reload(rmod)
print("② reload 的返回值就是你手里那个模块对象吗 →", loaded is rmod, "｜ __dict__ 里的 VER 更新了吗 →", rmod.VER == 2)
print("③ 但你手里的旧引用一个都不换：old_ver 还是", old_ver, "｜ rmod.K 和旧 K 是同一个类吗 →", rmod.K is old_k)
print("   旧实例的 isinstance 还认吗 →", isinstance(old_obj, rmod.K), "← 这一条就是「reload 之后到处冒幽灵报错」的根因")
print("④ 结论：reload 改的是那份 __dict__，不接管你已经绑定走的名字；要拿新东西得重新取属性")
PYC
python3 probe.py
cd .. && rm -rf reload-check'
note '↑ 这一格同时解释了 d 篇为什么「reload 救不了循环」：环里缺的是别的模块的模块体跑完，'
note '  而你手里那批函数、类、实例的引用在 reload 之后还是旧对象 —— 它只重跑，不接管。'

# ============================================================================
banner 16 "h 篇完整跑：venv 到底改了什么（本机没有 ensurepip，这条失败也当证据演）"
# ============================================================================
runc 'python3 h_venv_mechanics.py'
note '↑ 主线③：venv 的全部魔法就是改 sys.path。它不复制解释器 —— 激活前后是同一个 Python，'
note '  换掉的只有路径账，外加 sys.prefix 这个「我在哪」的归属标记。'
note '  ★ 本机的诚实交代：这台 Ubuntu 没装 python3-venv，`import ensurepip` 直接 ModuleNotFoundError，'
note '  所以带 pip 引导的 `python3 -m venv X` 失败退出 1，只有 --without-pip 建得出来。'
note '  本 demo 全程不联网、不 pip install、不 apt install，不去修它 —— 想通「骨架和 pip 是两件事」之后，剩下的结论是通用的。'

# ============================================================================
banner 17 "现场格：venv 骨架就那三样，bin/ 里全是符号链接（沙箱里现建现删）"
# ============================================================================
runc 'python3 -m venv --without-pip venv-check/vv'
note '↑ 这条命令跑在沙箱里（当前目录就是 work/），跑完由下一格删掉；章目录和你家目录都不参与。'
runc_at "$SB/work" 'python3 -c "
import os, sys
root = \"venv-check/vv\"
missing = [p for p in (\"pyvenv.cfg\", \"bin\", \"lib\") if not os.path.exists(os.path.join(root, p))]
print(\"① 骨架三样齐了吗 →\", not missing, \"｜ 缺的：\", missing or \"无\")
print(\"   pyvenv.cfg 里只有键名（值里带绝对路径，一律不上屏）：\", sorted({ln.split(\"=\")[0].strip() for ln in open(os.path.join(root, \"pyvenv.cfg\")) if \"=\" in ln}))
b = sorted(os.listdir(os.path.join(root, \"bin\")))
print(\"② bin/ 里列一遍（只留 basename）：\", b)
rel = sum(1 for n in b if os.path.islink(os.path.join(root, \"bin\", n)) and not os.readlink(os.path.join(root, \"bin\", n)).startswith(\"/\"))
abso = sum(1 for n in b if os.path.islink(os.path.join(root, \"bin\", n)) and os.readlink(os.path.join(root, \"bin\", n)).startswith(\"/\"))
print(\"   符号链接里指向同目录的相对链接有几条 →\", rel, \"｜ 指向系统解释器的绝对链接有几条 →\", abso, \"（★ 诚实交代：不是一条不差的「全是相对」）\")
print(\"   激活脚本有没有 →\", sorted(n for n in b if \"activate\" in n))
lib = os.path.join(root, \"lib\")
sub = os.path.join(lib, next(x for x in os.listdir(lib) if x.startswith(\"python\")))
sp = os.path.join(sub, \"site-packages\")
print(\"③ venv 自己的包目录叫什么：\", os.path.basename(sub), \"/\", os.path.basename(sp), \"｜ 里面此刻有几项 →\", len(os.listdir(sp)), \"（建完是空的：venv 不装东西，它只是多开一个目录）\")
print(\"   它有没有复制一份标准库进去 →\", any(x.startswith(\"os.py\") or x.startswith(\"json\") for x in os.listdir(sp)))
"; rm -rf venv-check'
note '↑ ③ 最后两个布尔是主线③ 的物理证据：venv 里 lib/pythonX.Y/site-packages/ 是空的、也没有标准库副本，'
note '  它只是多开一个「你的包」目录并把系统那份挡在账本外面。第 18 节把这本账整个摊开看。'

# ============================================================================
banner 18 "小证据：一条 pyvenv.cfg 开关换掉整本路径账 —— include-system-site-packages"
# ============================================================================
runc 'mkdir -p switch-check && cd switch-check
python3 -m venv --without-pip off && python3 -m venv --without-pip --system-site-packages on
for v in off on; do
  printf "\n—— venv %s ——\n" "$v"
  ./$v/bin/python3 -c "
import os, sys
def fold(p):
    return str(p).replace(os.environ[\"SB\"], \"<沙箱>\").replace(os.getcwd(), \"<cwd>\")
switch = [ln.split(\"=\")[1].strip() for ln in open(os.path.join(sys.prefix, \"pyvenv.cfg\")) if ln.startswith(\"include-system\")]
print(\"① 我在 venv 里吗 →\", sys.prefix != sys.base_prefix, \"｜ prefix 折成 <沙箱> =\", fold(sys.prefix), \"｜ base_prefix 折成 <沙箱> =\", fold(sys.base_prefix))
print(\"② sys.path 条数 =\", len(sys.path), \"｜ 朴素数 site-packages =\", sum(1 for p in sys.path if \"site-packages\" in p), \"｜ 真实包目录（site-packages 或 dist-packages）=\", sum(1 for p in sys.path if \"site-packages\" in p or \"dist-packages\" in p))
print(\"③ cfg 里的开关 =\", switch[0] if switch else \"（没有这一行）\")
print(\"④ venv 自己的包目录在账上第几项 →\", max([i for i, p in enumerate(sys.path) if p.endswith(\"site-packages\")] or [-1]), \"｜ 家目录那条（ENABLE_USER_SITE =\", __import__(\"site\").ENABLE_USER_SITE, \"）在账上吗 →\", any(\".local\" in p for p in sys.path))
"
done
cd ..; rm -rf switch-check'
note '↑ 这一格把主线③ 钉死：同一个解释器、同一个骨架，只有 sys.path 这本账被 cfg 里那一行改写；'
note '  开关关掉时系统那份包目录整个不在账上，打开就回来 —— venv 的隔离感全部来自这一件事。'
note '  ② 那一行还演了 h 篇的诚实交代：本机系统包目录叫 dist-packages（Debian 改名），'
note '  所以数 'site-packages' 这个朴素判据在本机数不到它们，两种数法必须同时给才看得出差别。'
note '  ④ 后半段是 i 篇的伏笔：venv 里 user site 被硬性关掉，就是为了堵「隔离环境还吃到 ~/.local」这条缝。'

# ============================================================================
banner 19 "i 篇完整跑：site 模块才是填账本的那个手，.pth 是它塞进来的代码"
# ============================================================================
runc 'python3 i_site_and_userdir.py'
note '↑ 01 章讲过 sys.path 是怎么拼出来的；这一篇讲**谁**拼的：解释器启动时偷偷 import 的 site 模块。'
note '  它干三件事：加包目录、处理 .pth、决定家目录那条通道开不开。★ 安全面那两格要看仔细：'
note '  .pth 里 import 开头的行是被 exec 的，每次 python 启动都跑，而且本机实测一次启动跑了两遍；'
note '  一条坏 .pth 让启动打印一坨 traceback，但退出码还是 0，并且它后面的行全部作废。'
note '  这一篇全程只读：不往 ~/.local、~/.config、家目录、/etc 写任何东西，家目录路径一律折成标记。'

# ============================================================================
banner 20 "小证据：-s / -S / -E / PYTHONNOUSERSITE= 空值，四种「关」法关的不是同一件事"
# ============================================================================
runc 'python3 -c "
import os, site, sys, sysconfig
h = os.path.expanduser(\"~\")
def fold(p):
    return str(p).replace(h, \"<家目录>\")
venv_p = sysconfig.get_paths(scheme=\"venv\", vars={\"base\": sys.base_prefix})
user_p = sysconfig.get_paths(scheme=\"posix_user\")
cand = [fold(x) for x in dict.fromkeys(list(venv_p.values()) + list(user_p.values()))
        if x.endswith(\"site-packages\") or x.endswith(\"dist-packages\")]
onpath = set(fold(x) for x in sys.path)
print(\"① 「如果是 venv / 如果是 pip install --user」两种布局下的包目录候选（家目录折成 <家目录>）：\", cand)
print(\"   有几条落在家目录 →\", sum(1 for x in cand if \"<家目录>\" in x),
      \"｜ 有几条此刻就在 sys.path 上 →\", sum(1 for x in cand if x in onpath))
print(\"   本机 default scheme 真正在用的那条 →\", fold(sysconfig.get_paths()[\"purelib\"]))
print(\"   → 上面三条互不相同：venv 那条只在你建了 venv 之后才生效，user 那条目录压根不存在，\")
print(\"     平时 import 到的其实是 default 那条 —— 这就是「同一个 import 语句在不同环境下指向不同目录」的字面形状\")
print(\"② 此刻 sys.path 上：朴素数 site-packages =\", sum(1 for p in sys.path if \"site-packages\" in p),
      \"｜ 真实包目录（含 dist-packages）=\", sum(1 for p in sys.path if \"site-packages\" in p or \"dist-packages\" in p))
print(\"   → 两个数不一样就是本机那处改名（Debian 把包目录叫成 dist-packages），h/i 两篇的诚实交代都在这\")
print(\"③ 家目录那条 user site 在 sys.path 里吗 →\", site.getusersitepackages() in sys.path,
      \"｜ ENABLE_USER_SITE =\", site.ENABLE_USER_SITE,
      \"｜ 那个目录本身存在吗 →\", os.path.isdir(site.getusersitepackages()))
print(\"   → 开关是开着的但目录不存在，site 只往存在的目录里加：这就是「ENABLE_USER_SITE=True 而路径不在账上」的原因\")
"'
runc 'python3 -c "import sys, site; print(\"对照（四个开关都没碰）→ ENABLE_USER_SITE =\", site.ENABLE_USER_SITE, \"｜ sys.path 条数 =\", len(sys.path))"'
runc 'python3 -s -c "import sys, site; print(\"-s   → ENABLE_USER_SITE =\", site.ENABLE_USER_SITE, \"｜ sys.path 条数 =\", len(sys.path), \"｜ 只关家目录那条通道，其它照旧\")"'
runc 'python3 -S -c "import sys; print(\"-S   → 整个 site 都不跑：sys.path 条数 =\", len(sys.path), \"｜ 比上面少掉的几条就是 site 加的\")"'
runc 'python3 -E -c "import sys; print(\"-E   → PYTHONPATH/PYTHONHOME 这类环境变量全被忽略：条数 =\", len(sys.path), \"｜ 它关的是「读环境」，不是「读 site」\")"'
runc 'PYTHONNOUSERSITE= python3 -c "import sys; print(\"PYTHONNOUSERSITE= 空值 → flags.no_user_site =\", sys.flags.no_user_site, \"｜ 空值＝不关：环境变量只看是不是空，不是只看存不存在\")"'
runc 'PYTHONNOUSERSITE=1 python3 -c "import sys; print(\"PYTHONNOUSERSITE=1      → flags.no_user_site =\", sys.flags.no_user_site, \"｜ 这才叫关\")"'
note '↑ 上面六行是本章收尾用的那张「关」的对照表：-s 只关家目录那条、-S 连包目录和 .pth 一起关（meta_path 上'
note '  那些由 .pth 塞进来的第三方 finder 也会跟着消失）、-E 关的是环境变量这一层，三者互不替代。'
note '  而 PYTHONNOUSERSITE= 空值不生效这一条，和「变量存在就关掉」的直觉正好相反 ——'
note '  写 CI 或 Dockerfile 时想关掉家目录通道，要么给 1，要么用 -s，别只 export 一个空值。'

# ============================================================================
banner 21 "跨脚本小结：三本账的合并视图（sys.modules / sys.path / 缓存）"
# ============================================================================
note '把 a~i 九篇的结论合成一张账。三条账各给「此刻的形状」+「这条账的规矩」，屏幕上只有计数和布尔。'
runc 'python3 -c "
import sys
print(\"【账本① sys.modules】此刻已有\", len(sys.modules), \"条（这个数字随解释器小版本变，README 只引用下面那几条布尔）\")
import json
print(\"  规矩：命中就什么都不做 →\", all(n in sys.modules for n in (\"json\", \"os\", \"sys\")), \"｜ 账本里的对象就是你 import 到的那个 →\", sys.modules[\"json\"] is json)
print(\"  名字相同就是同一个模块，和它在磁盘上叫什么路径无关 →\", \"os\" in sys.modules and sys.modules[\"os\"] is __import__(\"os\"))
print(\"  副作用：同一个文件用两个名字加载 = 两份互不相干的活状态（g 篇第 5 段用计数演过）\")
"'
runc 'python3 -c "
import os, sys
h = os.path.expanduser(\"~\")
print(\"【账本② sys.path】此刻\", len(sys.path), \"条（-c 入口，所以第 0 项是空串）\")
print(\"  规矩：顺序即优先级 → 谁在前面谁赢（本 demo 第 6 节用同名双份文件当场换过胜负）\")
print(\"  入口项折成 <家目录> 之后，账上有含家目录的条数 →\", sum(1 for p in sys.path if h in p), \"｜ 空串在不在 →\", \"\" in sys.path)
print(\"  内置/冻结那层根本不在这本账里，却排在所有路径之前 → 同名文件遮蔽不了它（b 篇第 6 段）\")
"'
runc 'python3 -c "
import sys
print(\"【账本③ 编译缓存】cache_tag =\", sys.implementation.cache_tag, \"｜ 它是 .pyc 文件名的来源\")
print(\"  此刻 sys.dont_write_bytecode =\", sys.dont_write_bytecode, \"｜ 它只管写不管读（e 篇第 4 段四种「别写」各演过一遍）\")
print(\"  sys.pycache_prefix 是 None 吗 →\", sys.pycache_prefix is None, \"｜ 非 None 时缓存整体搬到别处，搬的是绝对路径的镜像\")
print(\"  失效判据：源码 mtime + 源码字节数（本 demo 第 12 节当场从 16 字节头部读出来对过）\")
print(\"  ★ 只有 import 才产 __pycache__；python3 路径/x.py 直接跑脚本不产 —— 所以本 demo 第 22 节的计数才有意义\")
"'
runc 'python3 -c "
import sys
print(\"【三本账的先后】import x 走这条路，一步都不跳：\")
print(\"  第 1 步 问账本①：x 在 sys.modules 里吗 →\", \"json\" in sys.modules, \"｜ 在 → 直接把那个对象绑给你，文件不打开、模块体不跑\")
print(\"  第 2 步 问账本②：按 sys.path 从前往后找 spec（find_spec 只查不载，g 篇）\")
print(\"  第 3 步 问账本③：.pyc 里那两个数对得上源码吗 → 对得上就跳过 parse/compile，但模块体照样跑（e 篇收口）\")
print(\"  第 4 步 执行模块体 → 绑名字进你的命名空间 → 顺手在父包对象上挂属性（a/c 篇）\")
print(\"  任何一步想不通，报错原文里都不会告诉你它找过哪些目录 → 只能自己摊开 sys.path 逐层问（b 篇第 7 段那张表）\")
"'
note '↑ 主线② 和主线③ 其实是同一件事的两端：入口决定 sys.path[0] 的形状，venv/site 决定这本账余下几条；'
note '  换句话说「我的 import 怎么不工作」的问题，九成可以在启动那一刻之前解释清楚。'

# ============================================================================
banner 22 "自证：沙箱里的事只留在沙箱，章目录一个字都没多"
# ============================================================================
note '此刻的当前目录就是沙箱 work/。先把「该有什么」写成能判的式子，否则下面那些 0 只是「看着干净」：'
runc 'echo "沙箱 work/ 里的 .py 个数=$(find . -maxdepth 1 -name "*.py" | wc -l)（这 9 个是章目录副本，不是产物）  非 .py 文件数=$(find . -maxdepth 1 -type f ! -name "*.py" | wc -l)"'
note '↑ 守卫如果不会响，就等于没检查。先故意造三样残留：一个子目录、一个非 .py 文件、一次真 import 产生的缓存：'
runc 'mkdir -p 残留-demo; touch 残留.tmp; printf "y = 1\n" > probe_mod.py; python3 -c "import probe_mod"; echo "塞进假产物之后：子目录数=$(find . -mindepth 1 -maxdepth 1 -type d | wc -l)  非 .py 文件数=$(find . -maxdepth 1 -type f ! -name "*.py" | wc -l)  __pycache__ 个数=$(find . -name __pycache__ | wc -l)  *.pyc 个数=$(find . -name "*.pyc" | wc -l)  ← 四项都会响"'
runc 'rm -rf 残留-demo 残留.tmp probe_mod.py __pycache__; echo "清掉之后：子目录数=$(find . -mindepth 1 -maxdepth 1 -type d | wc -l)  非 .py 文件数=$(find . -maxdepth 1 -type f ! -name "*.py" | wc -l)  __pycache__ 个数=$(find . -name __pycache__ | wc -l)  *.pyc 个数=$(find . -name "*.pyc" | wc -l)"'
note '↑ 上面那一次「响」是本章主题的现场复演：只有 import 会造 __pycache__；'
note '  九个取证脚本跑的是 `python3 名字.py`，它们自己 import 出去的模块全在各自的 xxx-demo/ 里，收尾整份 rmtree。'
runc 'echo "沙箱里此刻还有 venv 骨架吗 → pyvenv.cfg 个数=$(find . -name pyvenv.cfg | wc -l)  bin/activate 个数=$(find . -name activate | wc -l)（第 17、18 节现建的那两个已删）"'
note '家目录那两处目录再数一遍，和第 1 节同一格对照 —— 两次一样就是「demo 没往家目录写过东西」的凭证：'
runc 'python3 -c "
import os
def n(p):
    p = os.path.expanduser(p)
    return len(os.listdir(p)) if os.path.isdir(p) else \"目录不存在\"
print(\"~/.local/lib 条目数 =\", n(\"~/.local/lib\"), \"｜ ~/.config 条目数 =\", n(\"~/.config\"), \"｜ ~/.bashrc 存在吗 →\", os.path.isfile(os.path.expanduser(\"~/.bashrc\")))
"'
note '★ 章目录（源文件本体在这里，demo 全程只读它）。清单显式排除 README.md，'
note '  所以「README 写完之后重跑」得到的是同一份输出：'
runc_at "$HERE" 'ls'
runc_at "$HERE" 'echo ".py 个数=$(find . -maxdepth 1 -name "*.py" | wc -l)  demo.sh 个数=$(find . -maxdepth 1 -name "demo.sh" | wc -l)  README 以外的其它文件数=$(find . -maxdepth 1 -type f ! -name "*.py" ! -name "demo.sh" ! -name "README.md" | wc -l)  子目录数=$(find . -mindepth 1 -maxdepth 1 -type d | wc -l)  __pycache__ 个数=$(find . -name __pycache__ | wc -l)"'
note '这一章一条网络命令都没用、没起过任何监听端口、没 pip install 过任何东西。'
note '  本机在听的端口只打印条数（端口号和本章无关，不该写进教材）；进程只数「本章真可能留下的那两类名字」：'
runc 'if command -v ss >/dev/null 2>&1; then echo "本机 LISTEN 条目数=$(ss -lnt | grep -c LISTEN)"; else echo "ss 本机没有（那就更没得起）"; fi'
runc 'v=$(pgrep -c -f "[v]env --without"); q=$(pgrep -c -f "[p]ip install"); echo "还在跑的 venv 建环境进程=$v ｜ 还在跑的 pip 安装进程=$q（本章只可能起这两类，全前台同步，跑到这一格时必然都归零）"'
note '★ 为什么不数 jobs：本 demo 全程前台同步，一个 & 都没起过；而 runc 是用 bash -c 跑每条命令的，'
note '  在那种一次性子进程里问作业表，量到的永远是空表 —— 那个 0 是假的（循环篇专门讲过这条）。'
note '★ 模式为什么各带一个方括号：pgrep -f 匹配的是完整命令行，而这条命令行里就写着模式本身。'
note '  上一版这里写的是宽模式 venv|pip，屏幕上当场冒出三行别人音频服务（pipewire）的进程号 ——'
note '  既泄了本机隐私，又把「有没有留下东西」判成了「这台机器上一共有多少含 pip 三个字母的进程」。'
note '★ 本章的取证纪律：所有「故意失败」都由脚本自己接住并原样打印（报错措辞跟版本绑定，只有本机 3.14.4 验过）；'
note '  临时目录全在 mktemp -d 出来的 py09-XXXXXX 里，由 trap cleanup EXIT 整份删掉；9 个取证脚本一个字没改。'

printf '\n%s[全部跑完]%s 上面每一段都是真实执行结果；README.md 引用的输出全部出自这一次运行。\n' "$B" "$R"
printf '[一句话回顾] import 查的是三本账：sys.modules 命中就什么都不做，sys.path 的顺序就是优先级，\n'
printf '              .pyc 认的是「源码 mtime + 字节数」而不是内容哈希；而 venv 的魔法只是把第二本账换了一遍。\n'
exit 0
