#!/usr/bin/env bash
# ============================================================================
# 第 09 章《submodule 与 worktree：一个仓库多个工作区 / 仓库里套仓库》—— 整章证据一键复现
#
#   bash demo.sh                 跑完自动删沙箱（默认）
#   bash demo.sh --keep          跑完保留沙箱并打印路径，可以自己进去接着敲
#   bash demo.sh -h | --help     打印本用法说明
#   bash demo.sh > cap.txt 2>&1  重定向时自动关颜色，README 引用的就是这种纯文本
#
# 本脚本按 README 的小节顺序，把每一节的真实终端输出打出来。
# 屏幕上出现的每一行都是这台机器上这条命令的真实结果，没有一行是手写的。
#
# 安全说明（请先看这一段）：
#   * 所有 git 写操作只发生在 mktemp 出来的沙箱目录里，绝不碰本机任何已有仓库，
#     也不碰本手册所在的目录。
#   * HOME 被改写到沙箱里的 fakehome/：任何 git 命令都读不到、也写不到你真实的
#     ~/.gitconfig。脚本里【没有任何一条 git config --global】；出现的 git config
#     要么带 --local（只写沙箱仓库自己），要么带 -f .gitmodules（只写那个文件），
#     要么干脆只在一个命令行 -c 参数里出现一次（不落任何配置）。
#   * 提交身份用环境变量 GIT_AUTHOR_NAME / GIT_COMMITTER_NAME（值就是「学习者」
#     learn@example.com），等价于每条命令加 -c user.name=... -c user.email=...，
#     但不往任何配置文件里写东西。屏幕上打印的命令就是真实执行的命令。
#   * 不联网：没有任何外网访问。所谓「远程」全是沙箱里的 bare 仓库目录。
#     正因为全程用本地路径，第 7 / 10 节会让你撞上 git 对 file 协议的安全限制，
#     脚本统一在命令上带 -c protocol.file.allow=always（只对本次调用生效）。
#   * 故意【不加 set -e】：本章要给你看好几次命令失败长什么样（原文报错 + 非 0 退出码），
#     加了 -e 脚本就中途死了，后面全看不到。
#   * 提交时间用一个固定时钟递进（从 2026-01-05 09:03 起，每笔 +3 分钟），
#     数据文件内容是定值伪随机，所以同一台机器重复跑 hash 一致；
#     唯一每次都不同的是沙箱的绝对路径（带 /tmp/git-handbook-09-XXXXXX 的那些行）。
#   * 本章要量 du -sh 和 count-objects 的体积，所以用环境变量把 gc.auto 压成 0
#     （见下面 GIT_CONFIG_* 那三行）：git commit 会【偷偷在后台】起一个 git gc --auto，
#     它有没有跑完是竞态。不关掉的话，同一份脚本连跑两次，.git 能从 6.1M 变成 13M、
#     in-pack 从 309 变成 525 —— 本章第 1 节和第 14 节的体积结论就没法复核了。
#     这三行只对本进程树生效，不落任何配置文件；显式的 git gc 不受它影响。
# ============================================================================

KEEP=0
for arg in "$@"; do
    case "$arg" in
        --keep) KEEP=1 ;;
        -h|--help) sed -n '1,42p' "$0"; exit 0 ;;
        *) echo "未知参数：$arg（本脚本只认 --keep 和 -h）" >&2; exit 2 ;;
    esac
done

set -u                  # 只开 -u；故意不开 -e / pipefail，理由见头部

export LC_ALL=C.UTF-8 LANG=C
export GIT_PAGER=cat PAGER=cat GIT_EDITOR=true GIT_MERGE_AUTOEDIT=no
unset GIT_DIR GIT_WORK_TREE GIT_INDEX_FILE GIT_CONFIG_GLOBAL GIT_CONFIG_COUNT 2>/dev/null
# 关掉后台 auto-gc（理由见开头）：不落任何文件，只是这一棵进程树的配置视角
export GIT_CONFIG_COUNT=1 GIT_CONFIG_KEY_0=gc.auto GIT_CONFIG_VALUE_0=0

# ── 沙箱：所有仓库都建在这里面 ─────────────────────────────────────────────
SB=$(mktemp -d "${TMPDIR:-/tmp}/git-handbook-09-XXXXXX")
export SB                   # 必须 export：runc/quiet 里的命令是在 bash -c 子进程里跑的，要能看到沙箱路径
QLOG="$SB/.quiet-log"       # quiet() 的输出暂存处：成功就丢掉，失败才把报错亮出来
export QLOG
mkdir -p "$SB/fakehome"
export HOME="$SB/fakehome"                     # 家目录关进沙箱，真实 ~/.gitconfig 不可能被读写
export XDG_CONFIG_HOME="$SB/fakehome/.config"

cleanup() {
    if [ "$KEEP" -eq 1 ]; then
        printf '\n[沙箱已保留] %s\n' "$SB"
        printf '  worktree 现场：cd "%s/app" && git worktree list\n' "$SB"
        printf '  submodule 现场：%s/sm/host（宿主）、%s/sm/math.git（库的假远程）、%s/sm/fresh（新克隆）\n' "$SB" "$SB" "$SB"
        printf '  subtree 现场：%s/st/app\n' "$SB"
        printf '  看完记得删：rm -rf "%s"\n' "$SB"
    else
        rm -rf "$SB"
        printf '\n[沙箱已删除] %s\n' "$SB"
    fi
}
trap cleanup EXIT

# ── 颜色：只有 stdout 是终端才上色；重定向到文件时是纯文本 ──────────────────
if [ -t 1 ]; then B=$'\033[1m'; C=$'\033[36m'; Y=$'\033[33m'; R=$'\033[0m'
else B=''; C=''; Y=''; R=''; fi

banner() { printf '\n%s══════ %s %s\n' "$B" "$1" "══════════════════════════$R"; }

# 打印命令 → 用 bash -c 执行（所以能自然写管道、glob、cd）→ 原样打印输出 → 打退出码
runc() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    bash -c "$1" 2>&1
    printf '[退出码 %d]\n' "$?"
}

note()  { printf '%s  ↳ %s%s\n' "$Y" "$*" "$R"; }        # 解释句（README 作者请直接引用）
# 搭场景的命令：成功就一个字都不打；失败就把命令原文和它的报错一起亮出来，绝不静默埋雷
quiet() {
    if bash -c "$1" >"$QLOG" 2>&1; then return 0; fi
    printf '  [搭建失败：%s]\n' "$1"
    sed 's/^/      │ /' "$QLOG"
}

# ── 固定身份：只在环境变量里，不落任何配置文件 ──────────────────────────────
export GIT_AUTHOR_NAME='学习者'    GIT_AUTHOR_EMAIL='learn@example.com'
export GIT_COMMITTER_NAME='学习者' GIT_COMMITTER_EMAIL='learn@example.com'

# ── 固定时钟：每调一次 tick，时间往后走 3 分钟，保证 hash 可复现 ─────────────
D=$(( $(date -u -d '2026-01-05T09:00:00' +%s) ))
tick() { D=$((D + 180))
         GIT_AUTHOR_DATE=$(date -u -d "@$D" +%Y-%m-%dT%H:%M:%S+0000)
         GIT_COMMITTER_DATE=$GIT_AUTHOR_DATE
         export GIT_AUTHOR_DATE GIT_COMMITTER_DATE; }

PF='git -c protocol.file.allow=always'  # 本地路径 submodule 必带的开关（只对本次命令生效）

echo "本次沙箱：$SB"
echo "跑法：bash demo.sh（跑完自动删）｜ bash demo.sh --keep（保留）｜ bash demo.sh > cap.txt 2>&1（纯文本）"

# ============================================================================
banner "0. 开工前先看清这台机器（版本决定你能不能这么用）"
# ============================================================================
runc 'git --version'
runc 'test -x "$(git --exec-path)/git-subtree" && echo "git-subtree：本机有（第 13 节会真跑）" || echo "本机无 git-subtree，第 13 节只说明不验证"'
runc 'echo "HOME=$HOME"; git config --global --list 2>&1 | head -3'
note 'HOME 已经指到沙箱里的 fakehome，所以最后这条一定是「找不到文件」级别的报错 —— '
note '这正是我们要的：脚本没有任何一条命令能读到你真实的全局配置，更别说写它。'

# ============================================================================
banner "1. 为什么需要 worktree：一个仓库只有一个工作目录"
# ============================================================================
note '场景：app 仓库正在 main 上干活（有一处没提交的改动），你现在想同时看一眼 hotfix 分支的代码。'
mkdir -p "$SB/app/src"
cd "$SB/app" || exit 1
quiet 'git init -q -b main .'
# 造现场用的定值伪随机数据：同一个种子永远生成同一份内容，所以 hash 可复现
gendata() { awk -v s="$2" 'BEGIN{srand(s); for(j=0;j<2600;j++) printf "%d %d %d\n", int(rand()*1000000), int(rand()*100000), int(rand()*1000)}' > "$1"; }
for i in $(seq -w 1 40); do gendata "src/part$i.txt" "9$(printf %02d $((10#$i)))01"; done
printf '# app\n一个有 40 个数据文件的假项目\n' > README.md
tick; quiet 'git add -A && git commit -q -m "init: 建项目，40 个数据文件"'
# 再改 6 轮，每轮 40 个文件全部重写：工作目录大小不变，历史却一份份堆起来（真实项目就是这样）
for v in $(seq 1 6); do
    for i in $(seq -w 1 40); do gendata "src/part$i.txt" "9$(printf %02d $((10#$i)))0$((v+1))"; done
    tick; quiet "git commit -q -am '第 ${v} 轮：40 个数据文件全部改写'"
done
tick; quiet 'git checkout -q -b hotfix && printf "hotfix 的修复\n" >> src/part01.txt && git commit -q -am "hotfix: 改一处"'
tick; quiet 'git checkout -q main && printf "main 上的新功能\n" >> README.md && git commit -q -am "main: 新功能"'
quiet 'git -c gc.cruftPacks=false gc -q'   # 先打包，免得拿「松散对象」和「pack」比体积，比出来的没意义
note '顺带说一句：这里给 gc 加了 -c gc.cruftPacks=false（只对本次命令生效，不落任何配置）。'
note '  本机 git 2.55.0 跑默认 git gc 时，repack 的 cruft pack 那条路径会偶发失败，报错是'
note '  fatal: could not find pack 后面跟一个 .tmp-进程号-pack-临时包名（手工重试往往就好）。'
note '  本章只想要一个稳定的「打包」，所以绕开 cruft；加上 gc.auto=0，连跑两次的 du 才会一模一样。'

runc 'git log --oneline --all --graph | head -12'
runc 'git branch -a'
runc 'echo "main 上正在写的半成品，还没打算提交" >> src/part02.txt; git status --short'
note '工作目录已经被 main 分支 + 这份未提交改动占着。想同时看 hotfix，最直觉的做法是再 clone 一份：'
runc 'git clone --no-local -q . ../app-clone && echo "clone 完成"'
note '这里特意加了 --no-local：本地路径的 clone 默认会【硬链接】对象库里的 pack，du 甚至看不出成本。'
note '真实场景是从远端服务器 clone，对象必须重新落一份盘，所以用 --no-local 模拟出那个真实代价。'
runc 'cd .. && echo "=== 整目录体积 ==="; du -sh app app-clone
       echo "=== 各自的对象库 .git（第二份历史就是这里的差额）==="; du -sh app/.git
       echo "（clone 的 .git 单独量：）"; du -sh app-clone/.git'
runc 'cd .. && echo "--- 主仓库对象数：" && git -C app count-objects -v | head -3
       echo "--- clone 副本的对象数（一条不少，全抄了一遍）：" && git -C app-clone count-objects -v | head -3'
note 'clone 副本 = 工作文件 + 又一份完整历史；而且两份 .git 之后要各自 fetch、各自 gc。'
note 'worktree 干的是另一件事：只多一个工作目录，对象库和分支全部共用。第 2 节建一个，第 14 节量给你看。'

# ============================================================================
banner "2. git worktree add：给同一个仓库再开一个工作目录"
# ============================================================================
cd "$SB/app" || exit 1
runc 'git worktree add ../app-hotfix hotfix'
note '上面这几行是原样输出：Preparing worktree 那行、HEAD is now at 那行，都是 git 自己打的。'
runc 'find ../app-hotfix -maxdepth 1 -printf "%y %s %p\n" | sort -k3'
note '第一列的字母：d = 目录，- = 普通文件。别错过 ../app-hotfix/.git 那行的 -，第 4 节就靠它。'
runc 'git -C ../app-hotfix log --oneline -3'
runc 'echo "在 hotfix 工作区里继续改" >> ../app-hotfix/src/part03.txt
       git -C ../app-hotfix status --short'
tick; runc 'git -C ../app-hotfix commit -q -am "hotfix: 在工作区里再改一笔" && git -C ../app-hotfix log --oneline -2'
note '主工作区 app/ 里的 main 一点没被动过（那份半成品还在）；而刚才那笔新提交，主仓库马上能看见：'
runc 'git log --oneline -1 hotfix; git status --short'

# ============================================================================
banner "3. git worktree list，以及「一个分支不能被两个工作区同时 checkout」"
# ============================================================================
runc 'git worktree list'
runc 'git worktree add ../app-main2 main'
note '★ 这条是故意失败的。规矩：一个分支最多只能被一个工作区检出，否则两边同时提交会互相踩指针。'
note '  想在同一份代码上来回切，用 --detach（第 6 节），或者再开一个分支。'
runc 'ls -d ../app-main2 2>&1; echo "（失败后没留下半个目录，不用手工收拾）"'
runc 'git worktree add ../app-typo does-not-exist'
note '★ 这条也是故意失败的：第二个参数必须是已存在的 ref（分支 / 标签 / commit sha），写错就 fatal。'
runc 'git worktree add -q -b feature/report ../app-report main && echo "带 -b 可以在建工作区时顺手新建分支"'
runc 'git worktree list'
runc 'git branch'

# ============================================================================
banner "4. worktree 里的 .git 是【文件】不是目录：对象库和分支都是共享的"
# ============================================================================
runc 'find .git ../app-hotfix/.git -maxdepth 0 -printf "%y %s %p\n"'
runc 'file ../app-hotfix/.git; echo "--- 内容就一行："; cat ../app-hotfix/.git'
note '一个 60 字节左右的文本指针，指向主仓库 .git/worktrees/<名字>。工作区自己不带 objects/。'
runc 'echo "--- 主仓库里给每个工作区留的登记处："; ls .git/worktrees/'
runc 'echo "--- 这个登记处里装的是该工作区私有的东西："; ls .git/worktrees/app-hotfix'
runc 'echo "--- 两个工作区各自的 git 目录（不同）："; git rev-parse --git-dir; git -C ../app-hotfix rev-parse --git-dir'
runc 'echo "--- 公共目录（同一个）："; git rev-parse --git-common-dir; git -C ../app-hotfix rev-parse --git-common-dir'
runc 'echo "=== 主仓库 git branch -a ==="; git branch -a
       echo "=== hotfix 工作区 git branch -a（一模一样）==="; git -C ../app-hotfix branch -a'
tick; runc 'git -C ../app-hotfix branch 从-hotfix-工作区新建 && echo "在 worktree 里建的分支，主仓库能不能马上看到？"'
runc 'git branch --list "从*"'
runc 'echo "--- 对象也是共享的：从 hotfix 工作区问那笔新提交，主仓库直接答得上来"; git cat-file -t "$(git -C ../app-hotfix rev-parse HEAD)"'
note '共享：对象库、所有 refs（分支/标签/远程跟踪）、仓库配置、remote、stash 列表、hooks。'
note '各自独立：HEAD（你在哪个分支）、工作目录内容、index 暂存区、以及 .git/worktrees/<名字> 那份登记。'

# ============================================================================
banner "5. 用完怎么收尾：git worktree remove 与 git worktree prune"
# ============================================================================
runc 'git worktree remove ../app-report && echo "干净的工作区可以直接 remove，登记一起清掉"; git worktree list'
quiet 'git worktree add -q --detach ../app-tmp main && echo "未提交的脏东西" >> ../app-tmp/src/part05.txt'
runc 'git worktree remove ../app-tmp'
note '★ 故意失败：里面有未提交的改动，git 不肯悄悄扔掉你的工作。'
runc 'git worktree remove --force ../app-tmp && echo "确认不要了才加 --force"; ls -d ../app-tmp 2>&1; git worktree list'
note '接下来制造最常见的脏状态：目录被 rm -rf 了，但主仓库的登记还在。'
quiet 'git worktree add -q --detach ../app-bad main'
runc 'rm -rf ../app-bad && echo "已经 rm -rf 掉工作区目录，但没走 git worktree remove"'
runc 'git worktree list'
runc 'git worktree list --verbose'
note '↑ list 已经把这条标成 prunable；--verbose 多打一行原因。这些残留登记就躺在 .git/worktrees/ 里：'
runc 'ls .git/worktrees/'
runc 'git worktree prune --dry-run --verbose'
runc 'git worktree prune --verbose; echo "--- prune 之后："; git worktree list; ls .git/worktrees/'
note 'prune 只做一件事：把「目录已经不存在」的登记删掉。它不会替你删任何真实存在的目录。'

# ============================================================================
banner "6. --detach 与 git -C：CI / code review 场景，不切分支跑另一份代码"
# ============================================================================
runc 'git worktree add --detach ../app-review main~1'
note '主工作区正在 main 上写着半成品；这里用 --detach 检出「main 的上一个提交」，HEAD 不挂任何分支，'
note '既不占用分支，也绝不会误把分支指针推走 —— CI 拉一份代码跑测试、review 一眼旧版本，都是这个套路。'
runc 'git -C ../app-review log --oneline -1; git -C ../app-review status | head -3'
runc 'echo "--- 全程不 cd，用 git -C 在别处跑命令："; git -C ../app-review ls-files | head -3
       echo "--- 只读地比较它和当前 main 的差别："; git -C ../app-review diff --stat main main~1'
tick; runc 'git -C ../app-review checkout -q -b 评审留下来的分支 && git -C ../app-review status | head -2'
note 'detach 状态下想留下东西，就地开个分支即可（注意：这时这个分支就被这个工作区占用了）。'
runc 'git worktree remove --force ../app-review && git worktree list'

# ============================================================================
banner "7. submodule 起步：先有一个库仓库、一个库的假远程、一个宿主仓库"
# ============================================================================
note '库叫 math，宿主叫 host，宿主把库挂在 libs/math 下。库的「远程」= 沙箱里的 bare 仓库 math.git。'
quiet 'mkdir -p "$SB/sm"'
quiet 'git init -q -b main "$SB/sm/math"
       cd "$SB/sm/math"
       printf "int add(int a,int b){return a+b;}\n" > calc.c
       printf "# math\n一个假的数学库\n" > README.md
       git add -A'
tick; quiet 'git -C "$SB/sm/math" commit -q -m "math: add()"'
tick; quiet 'cd "$SB/sm/math" && printf "int mul(int a,int b){return a*b;}\n" >> calc.c && git commit -q -am "math: mul()"'
quiet 'git clone -q --bare "$SB/sm/math" "$SB/sm/math.git"
       git -C "$SB/sm/math" remote add origin "$SB/sm/math.git"'
note '↑ 库这边：math = 库作者的工作副本，math.git = 假远程（bare），HEAD 现在指着含 mul() 的提交。'
runc 'cd "$SB/sm" && echo "--- 假远程 math.git 的分支：" && git -C math.git log --oneline'
quiet 'git init -q -b main "$SB/sm/host"
       printf "# host\n一个用到 math 的项目\n" > "$SB/sm/host/README.md"
       git -C "$SB/sm/host" add -A'
tick; quiet 'git -C "$SB/sm/host" commit -q -m "host: 建仓库"'
quiet 'git clone -q --bare "$SB/sm/host" "$SB/sm/host.git"'
quiet 'git -C "$SB/sm/host" remote add origin "$SB/sm/host.git"'
cd "$SB/sm/host" || exit 1
runc 'git submodule add ../math.git libs/math'
note '★ 上面这条是故意失败的。git 从 2.38.1 的安全修复起，默认不允许「子进程」用 file 协议克隆仓库，'
note '  而 submodule 的克隆正是父进程 spawn 出来的子进程，于是被拦。'
note '  真实项目用 https://... 的 URL 不会有这个问题；本章全程本地路径，所以每条都补一个'
note '  -c protocol.file.allow=always（只对本次调用生效，不写任何配置）。'
runc "$PF"' submodule add ../math.git libs/math'
note '↑ 这次成功。注意 ../math.git 是相对宿主仓库写的 —— .gitmodules 里存的是相对路径（下一行就能看到）。'
runc 'git status --short'
runc 'echo "--- .gitmodules 的全部内容："; cat .gitmodules'
runc 'find libs/math -maxdepth 1 -printf "%y %s %p\n" | sort -k3; echo "--- 库里的文件就是真实文件："; cat libs/math/calc.c'
runc 'echo "--- 库的工作区 .git 是文件指针："; cat libs/math/.git
       echo "--- 真正的 git 目录被塞进了宿主里："; ls .git/modules/libs/math | tr "\n" " "; echo
       echo "--- 库自己的远程："; git -C libs/math remote -v'
note '再注意两个细节：.gitmodules 里存的是相对路径 ../math.git（相对宿主仓库的位置算），别人 clone 宿主时'
note '就是靠它 + 宿主的 origin 位置去定位库；但 git 顺手把库自己 remote origin 解析成了绝对路径（上面那行），'
note '所以本章所有命令都在同一个沙箱里跑，换机器就得重新 set-url 或删了重加。真实项目这里写 https:// URL。'
note '关键点：submodule 的 git 目录在宿主的 .git/modules/<name> 下，不是藏在 libs/math/.git 里。'
note '这样 rm -rf libs/math 也只是删工作区，库的历史还在宿主手里（第 12 节 deinit 就靠这点）。'

# ============================================================================
banner "8. 宿主里记的不是文件内容，而是一条 160000 的 gitlink（= 库的 commit sha）"
# ============================================================================
runc 'git ls-files -s'
note '↑ 看模式列：README.md / .gitmodules 是 100644（普通文件），libs/math 是 160000。'
note '  160000 是一种特殊的索引项：只存一个 commit sha，不存任何文件内容，术语叫 gitlink。'
runc 'echo "--- 只看这一条："; git ls-files -s -- libs/math'
runc 'echo "--- diff --cached 里它的样子（new file mode 160000 + Subproject commit）："; git diff --cached -- libs/math'
tick; runc 'git commit -q -m "host: 引入 submodule libs/math" && git log --oneline -1'
runc 'echo "--- 提交之后，宿主的树里 libs/ 目录长这样（注意第二行是 commit 而不是 blob）："; git ls-tree HEAD libs; git ls-tree HEAD libs/math'
runc 'echo "--- 再往里看一层：libs 这个 tree 里只有一个条目："; git cat-file -p "HEAD^{tree}:libs"'
runc 'echo "--- 宿主自己能不能拿出那个 commit 对象？"; git cat-file -t "$(git rev-parse HEAD:libs/math)"'
note '★ 这条是故意失败的，而且失败得非常有说服力：宿主的对象库里【根本没有】那个 commit 对象，'
note '  索引里那行 160000 只存了一个 40 字节的 sha 数字而已。'
runc 'echo "--- 同一个 sha，问库自己："; git -C libs/math cat-file -t "$(git rev-parse HEAD:libs/math)"
       echo "--- 库的文件内容只在库的对象库里："; git -C libs/math cat-file -p HEAD:calc.c | head -2'
note '所以：宿主记的是「libs/math 必须正好是这个 commit」，文件内容由库自己的仓库负责存。'

# ============================================================================
banner "9. 三种脏状态：(untracked content) / (modified content) / (new commits)"
# ============================================================================
runc 'git status --short; echo "--- 干净（无输出就是干净）"; git submodule status'
runc 'echo "临时草稿，没跟踪" > libs/math/NOTE.md
       echo "=== ① 库里只多了未跟踪文件 ==="; git status --short; git status | tail -4'
note '↑ (untracked content)：短状态第二位是 ?。宿主的 gitlink 一点没变。'
runc 'echo "--- 宿主的 diff 对未跟踪内容一个字都不说："; git diff -- libs/math; echo "[上面真的没有输出]"'
runc 'rm libs/math/NOTE.md
       echo "int untracked_helper(void){return 0;}" >> libs/math/calc.c
       echo "=== ② 改了库里【已跟踪】的文件 ==="; git status --short; git status | tail -4'
note '↑ (modified content)：短状态第二位是 m。这两种都只是「库里面不干净」，宿主记录的 sha 没变。'
runc 'echo "--- 这时 diff 会出现一个假 sha（后面挂了 -dirty）："; git diff -- libs/math
       echo "--- 想看清是什么，用 --submodule 只要一句话摘要："; git diff --submodule -- libs/math'
note '顺带一提：那个 -dirty 不是真 sha，只是 diff 用来表达「工作区比这个 sha 脏」的写法。'
runc 'echo "--- 嫌 untracked/modified 噪音烦？两个忽略档位现在就试一遍：";
       echo "[dirty 档]"; git status --short --ignore-submodules=dirty; echo "[上面是空的：库里的不干净全当没看见]";
       echo "[untracked 档]"; git status --short --ignore-submodules=untracked'
note 'ignore-submodules 的取值 all / dirty / untracked。dirty = 库里未提交的改动全不看；'
note 'untracked = 只忽略未跟踪文件，所以已跟踪文件的改动照样报 m。上面只在命令行上给了一次，没落配置；'
note '要长期生效就写到宿主仓库的 local 配置里（git config submodule.libs/math.ignore dirty），绝不用 --global。'
tick; runc 'git commit -q -m "宿主想把库里这些改动一起带走"'
note '★ 故意失败：宿主根本带不走库里的文件改动 —— 它管不到库的工作区，宿主的暂存区里干干净净。'
note '  这些改动只存在于你这台机器的库工作目录里；别人 clone 宿主得到的还是老 sha。'
runc 'git -C libs/math checkout -q -- calc.c && echo "--- 库里恢复干净："; git status --short; echo "（无输出）"'
tick; runc 'echo "int divi(int a,int b){return a/b;}" >> libs/math/calc.c
             git -C libs/math commit -q -am "math: divi()"
             echo "=== ③ 库里 HEAD 前进了一个提交 ==="; git -C libs/math log --oneline -2
             echo "=== 宿主这时看它："; git status --short; git status | tail -4'
note '↑ (new commits)：短状态变成 " M"。只有这一种才是宿主要记录的东西。'
runc 'echo "--- 未暂存的 gitlink diff：index 行带 160000，一个 sha 换成另一个 sha"; git diff -- libs/math'
runc 'echo "--- --submodule 默认档：只列库里多了哪些提交"; git diff --submodule -- libs/math
       echo "--- --submodule=diff：连库里的真实内容差异一起展开"; git -c diff.submodule=diff diff -- libs/math | tail -8'
runc 'git -C libs/math push -q origin main && echo "先按 submodule 的规矩：库的提交推到库的远程"; git -C ../math.git log --oneline -1'
runc 'git add libs/math; echo "--- 暂存之后 diff --cached："; git diff --cached -- libs/math'
tick; runc 'git commit -q -m "host: 把 libs/math 提到带 divi() 的提交" && git ls-tree HEAD libs/math && git submodule status'

# ============================================================================
banner "10. clone 下来 submodule 目录是空的：update --init / --depth 1 / --remote"
# ============================================================================
tick; quiet 'git -C "$SB/sm/host" push -q origin main'
cd "$SB/sm" || exit 1
runc 'git clone -q "$SB/sm/host.git" fresh && echo "clone 宿主完成（没加 --recurse-submodules）"'
runc 'cd fresh && find . -maxdepth 1 -printf "%y %s %p\n" | sort -k3; echo "--- libs/ 目录里："; find libs -maxdepth 1 -printf "%y %s %p\n"; echo "--- libs/math 里的文件数："; ls -A libs/math 2>&1 | wc -l'
runc 'cd fresh && git status --short; echo "--- 状态是干净的，可库里就是没东西"; git submodule status'
note '↑ 前缀 - 表示「还没初始化」。宿主里那条 160000 只是一个 sha，文件内容得靠 submodule 命令去拉。'
runc 'cd fresh && git submodule update --init'
note '★ 故意失败一次：不带 -c protocol.file.allow，注册（registered）成功了，克隆子进程还是被拦。'
note '  注意最后两行：它试了一次、Retry scheduled、再来一次还是失败，于是 a second time, aborting。'
runc 'cd fresh && '"$PF"' submodule update --init'
runc 'cd fresh && ls libs/math; git submodule status; git status --short; echo "--- 拉下来的文件是真的："; cat libs/math/calc.c'
runc 'cd fresh && cat libs/math/.git; echo "--- 库的 git 目录在宿主的 .git/modules 下："; ls .git/modules'
note '↑ 新克隆里也一样：submodule 的 git 目录是宿主的 .git/modules/libs/math，不是 libs/math/.git。'
runc 'cd "$SB/sm" && '"$PF"' clone -q --recurse-submodules host.git one-shot && cd one-shot && git submodule status && ls libs/math'
note 'clone --recurse-submodules = clone + submodule update --init --recursive，CI 里最常见的写法。'
note '（不带 -c protocol.file.allow=always 时，这条在本地路径上会失败，原因和第 7 节一样。）'
runc 'cd "$SB/sm" && '"$PF"' clone -q host.git shallow && cd shallow
       '"$PF"' submodule update --init --depth 1
       echo "--- 拉了几层："; git -C libs/math log --oneline | wc -l
       echo "--- 是不是 shallow 仓库："; git -C libs/math rev-parse --is-shallow-repository'
note '★ 上面那条 warning 是本节重点，不是笔误：--depth is ignored in local clones。'
note '  宿主是从本地路径（裸仓库目录）clone 来的，submodule 的 URL 也跟着解析成本地路径，'
note '  git 这时走「本地优化」，--depth 被直接忽略 —— 所以 is-shallow-repository 还是 false，层数一本没少。'
note '  结论：在本地路径上验证 --depth 是徒劳的，得让它真的走协议。'
quiet 'rm -rf "$SB/sm/shallow-net"'
runc 'cd "$SB/sm" && '"$PF"' clone -q "file://$SB/sm/host.git" shallow-net && cd shallow-net
       echo "--- 宿主的 origin（注意协议）："; git remote -v
       '"$PF"' submodule update --init --depth 1
       echo "--- 换成 file:// 之后拉了几层："; git -C libs/math log --oneline | wc -l
       echo "--- 是不是 shallow 仓库："; git -C libs/math rev-parse --is-shallow-repository
       echo "--- 对比：不用 --depth 的 fresh 里库有几层："; git -C ../fresh/libs/math log --oneline | wc -l
       echo "--- 库里的文件照样是全的："; ls libs/math'
note '把本地路径写成 file:// 就是「假装它是个远端」：一旦真的走协议，--depth 才生效，库只取最近一个提交。'
note '--depth 1 的好处是大型依赖省下载；代价是库里历史断掉（log/blame 只看得见那一笔），'
note '想补回来就 git -C libs/math fetch --unshallow。真实项目 URL 本来就是 https://，不会有本地优化这档子事。'
note '接下来演示 --remote：先让库的作者往前推一个提交。他得先同步远程（第 9 节宿主已经推过 divi 了）。'
runc 'cd math && '"$PF"' fetch -q origin && git merge -q --ff-only origin/main && git log --oneline -2'
tick; runc 'cd math && echo "int sq(int a){return a*a;}" >> calc.c
             git commit -q -am "math: sq()" && git push -q origin main && echo "库的作者推了新提交，但宿主记的 sha 还不知道它"
             git -C "$SB/sm/math.git" log --oneline -2'
runc 'cd fresh && echo "--- 宿主记的还是老的："; git submodule status'
runc 'cd fresh && '"$PF"' submodule update --remote libs/math'
runc 'cd fresh && git submodule status; git status --short; git -C libs/math log --oneline -1'
note '--remote 的意思：不看宿主记的那个 sha，去拿 .gitmodules 里 url 对应分支的尖端。'
note '  拉完宿主立刻变成 (new commits)（上一行 " M libs/math" 就是），所以 --remote 之后还得回宿主提交 bump。'
runc 'cd fresh && git submodule update libs/math && echo "--- 不想要这次 --remote，把库退回宿主记录的 sha："; git submodule status; git status --short; echo "（空 = 干净）"'
note '这条也顺手演示了 submodule update 的真正职责：把库的 HEAD 挪回宿主记录的那个 sha。'

# ============================================================================
banner "11. submodule 的 detached HEAD：提交差点丢掉，用 reflog 捞回来 + 推送的正确顺序"
# ============================================================================
cd "$SB/sm/fresh/libs/math" || exit 1
runc 'git status | head -3'
note '↑ HEAD detached at <sha>：update --init 检出的是宿主记录的那个 commit 对象，不是任何分支。'
note '  这是设计如此（宿主只认 sha），但它意味着：你在这里 commit 的东西一开始不属于任何分支。'
runc 'git branch -a; echo "--- 当前 HEAD："; git rev-parse --short HEAD'
tick; runc 'echo "int mod(int a,int b){return a%b;}" > mod.c
             git add mod.c && git commit -q -m "math: mod()（手快，在 detached HEAD 上提交了）"
             echo "--- 提交成功了，看它的 sha："; git log --oneline -2; git rev-parse HEAD'
LOST=$(git rev-parse HEAD)
runc 'git checkout -q main && echo "--- 觉得不对劲，切回 main 看看："; git log --oneline -1; git status | head -2'
runc 'echo "--- 刚才那笔 mod() 提交还在不在？"; git log --oneline -3 main; git branch --contains '"$LOST"' 2>&1; echo "（上面 branch --contains 没有任何输出 = 没有任何分支指着它）"'
note '★ 提交看着像没了。其实对象还在库里，只是没人指着它 —— 这就是 reflog 的用武之地。'
runc 'git reflog | head -6'
runc 'echo "--- 从 reflog 认出那个 sha，指个分支就救活了："; git branch 救回来的-mod '"$LOST"'; git log --oneline -2 救回来的-mod'
runc 'git checkout -q 救回来的-mod && git log --oneline -1 && git status | head -2'
note '正确的顺序永远是两步：① 在库里提交，并把库的分支推到库的远程；② 回宿主把 gitlink 更新成新 sha 并提交。'
cd "$SB/sm/fresh" || exit 1
runc 'git status --short; echo "--- 第②步：宿主这边 bump gitlink"; git add libs/math && git diff --cached -- libs/math | tail -5'
tick; runc 'git commit -q -m "host: libs/math 升到带 mod() 的提交" && git ls-tree HEAD libs/math'
runc 'echo "--- 现在直接推宿主（它会带上一个库的、还没推出去的 sha）："; git push --recurse-submodules=check origin main'
note '★ 故意失败：--recurse-submodules=check 发现 gitlink 指的 sha 在任何远程上都找不到，于是拒绝推送。'
note '  它拦的就是「推上去别人 clone 不出来的宿主」这种事故。补救只有第 ① 步：把库也推上去。'
runc 'git -C libs/math push -q origin 救回来的-mod && echo "--- 把库的分支推上去（这一步做完 sha 就公开了）"; git -C libs/math branch -r'
runc 'git push --recurse-submodules=check origin main && echo "--- 再检查就过了"; git -C ../host.git log --oneline -1'
note '也可以用 --recurse-submodules=on-demand 让 git 顺手替你推库；本章宁可手动一步，让你知道推了什么。'

# ============================================================================
banner "12. foreach / submodule.recurse / deinit 与彻底移除 submodule"
# ============================================================================
cd "$SB/sm/fresh" || exit 1
runc 'git submodule status --recursive'
runc 'git submodule foreach "echo \"进入 name=\$name sha=\$(git rev-parse --short HEAD) 所在分支=\$(git rev-parse --abbrev-ref HEAD)\""'
runc 'git submodule foreach --recursive "git log --oneline -1"'
note 'foreach 会依次进每个【已初始化】的 submodule 跑命令，$name / $path / $sha 是它注入的变量；'
note '命令本身是给 sh 执行的，所以在 bash 里写的时候要转义 \$name，否则还没进 submodule 就被本机 shell 吃了。'
note 'submodule.recurse=true 的效果：pull / checkout 之后自动替你跑 submodule update。下面开两个仓库对比。'
quiet 'cd "$SB/sm" && git clone -q host.git r-checkout && cd r-checkout && '"$PF"' submodule update --init -q'
tick; quiet 'cd "$SB/sm/math" && git fetch -q origin && git merge -q --ff-only origin/main
             printf "int half(int a){return a/2;}\n" >> calc.c && git commit -q -am "math: half()" && git push -q origin main
             cd "$SB/sm/host" && git fetch -q origin && git merge -q --ff-only origin/main
             git -C libs/math fetch -q origin && git -C libs/math merge -q --ff-only origin/main
             git add libs/math && git commit -q -m "host: bump 给 recurse 演示（第一次）" && git push -q origin main'
runc 'cd ../r-checkout && echo "=== 没开 submodule.recurse 的仓库 ==="; git log --oneline -1; '"$PF"' pull -q origin main; echo "--- 宿主已经前进了："; git log --oneline -1; echo "--- 但库还停在旧 sha（前缀 + = 检出值和索引不一致）："; git submodule status'
runc 'cd ../r-checkout && git config --local submodule.recurse true && echo "--- 只在这个沙箱仓库里开："; git config --local --get submodule.recurse'
tick; quiet 'cd "$SB/sm/math" && git fetch -q origin && git merge -q --ff-only origin/main
             printf "int third(int a){return a/3;}\n" >> calc.c && git commit -q -am "math: third()" && git push -q origin main
             cd "$SB/sm/host" && git fetch -q origin && git merge -q --ff-only origin/main
             git -C libs/math fetch -q origin && git -C libs/math merge -q --ff-only origin/main
             git add libs/math && git commit -q -m "host: bump 给 recurse 演示（第二次）" && git push -q origin main'
runc 'cd ../r-checkout && '"$PF"' pull -q origin main && echo "=== 开了 recurse 之后，同一条 pull 顺手把库也更新了 ==="; git log --oneline -1; git submodule status; git -C libs/math log --oneline -1'
note '对比很清楚：同一条 git pull，开没开 submodule.recurse 决定库要不要你手动 update。'
note '（这里 pull 也带了 -c protocol.file.allow=always，因为更新库时要从本地路径 fetch。）'
cd "$SB/sm/fresh" || exit 1
runc 'echo "=== 现在把 submodule 删掉，看完整步骤 ==="; ls libs/math | tr "\n" " "; echo; echo "--- .gitmodules："; cat .gitmodules'
echo "int dirty" >> libs/math/calc.c
runc 'git submodule deinit libs/math'
note '★ 故意失败：库的工作区里还有未提交改动，deinit 不肯替你扔。要么先提交/清掉，要么明确加 -f。'
runc 'git submodule deinit -f libs/math; echo "--- deinit 之后："; ls -A libs/math | wc -l; echo "（0 = 工作区被清空了）"'
runc 'echo "--- .gitmodules 里那条配置还在吗："; cat .gitmodules
       echo "--- local 配置里的 submodule.<name>.url 没了："; git config --local --get-regexp "^submodule\." || echo "(空)"'
runc 'echo "--- 库的历史留在宿主的 .git/modules 里，deinit 不动它："; ls .git/modules/libs/math | tr "\n" " "; echo'
runc 'git rm libs/math; echo "--- rm 之后索引里还有吗："; git ls-files -s; echo "--- .gitmodules 现在的内容："; cat .gitmodules; echo "（这一版 git 把那一节也一起删了）"'
runc 'git status --short'
tick; runc 'git commit -q -m "host: 移除 submodule libs/math" && git log --oneline -2'
runc 'echo "--- 还嫌脏就把留在宿主里的库历史删掉："; rm -rf .git/modules/libs
       echo "[.git/modules 现在的内容，空就是删干净了]"; ls -A .git/modules 2>&1; echo "[工作区目录 libs/math 早在 git rm 那步就没了]"; ls -A libs/math 2>&1 || true'
runc 'echo "--- 想再加回来（.git/modules 已经空了，库会重新克隆）："; '"$PF"' submodule add ../math.git libs/math && git ls-files -s -- libs/math && git status --short'
note '彻底移除的四步记牢：deinit -f（清工作区 + 注销 local 配置）→ git rm <path>（删 gitlink，新版 git'
note '还会顺手删掉 .gitmodules 里那一节）→ 提交 → rm -rf .git/modules/<name>（删留在宿主里的库历史）。'
note '少任何一步都算没删干净：要么目录是空的但还登记着，要么 .gitmodules 里留着一条指向远程的 url。'

# ============================================================================
banner "13. submodule vs subtree：把库真正长进宿主里"
# ============================================================================
if [ -x "$(git --exec-path)/git-subtree" ]; then
note 'git-subtree 不是 git 内建命令，是 contrib 脚本（很多发行版随 git-all / git-subtree 包安装）。本机有，所以真跑。'
quiet 'mkdir -p "$SB/st" && git clone -q "$SB/sm/math.git" "$SB/st/libcopy"'
tick; quiet 'cd "$SB/st/libcopy" && printf "int quad(int a){return a*a*a*a;}\n" >> calc.c && git commit -q -am "math: quad() 由另一个团队写的"'
quiet 'git init -q -b main "$SB/st/app" && printf "# app2\n另一个宿主\n" > "$SB/st/app/README.md"
       git -C "$SB/st/app" add -A'
tick; quiet 'git -C "$SB/st/app" commit -q -m "app2: 建仓库"'
cd "$SB/st/app" || exit 1
runc "$PF"' subtree add --prefix third_party/math ../libcopy main'
note '↑ subtree add 干的就是一次 fetch + 一次 merge：打印出来的第一行就是它替你跑的命令。'
runc 'ls third_party/math; echo "--- 这些是宿主的【真实文件】，不是 160000："; git ls-files -s third_party/math'
runc 'echo "--- 宿主的历史里，库的全部提交都进来了："; git log --oneline --graph | head -8'
runc 'echo "--- clone 宿主的人什么都不用多做："; git clone -q . ../app-copy && ls ../app-copy/third_party/math && git -C ../app-copy log --oneline -1'
tick; quiet 'cd "$SB/st/libcopy" && printf "int neg(int a){return -a;}\n" >> calc.c && git commit -q -am "math: neg()" && git push -q origin main'
runc "$PF"' subtree pull --prefix third_party/math ../libcopy main'
runc 'git log --oneline --graph | head -6; echo "--- 上游的新内容已经在宿主目录里了："; tail -2 third_party/math/calc.c'
quiet 'git init -q -b main "$SB/st/app-squash" && printf "# app3\n用 squash 的宿主\n" > "$SB/st/app-squash/README.md"
       git -C "$SB/st/app-squash" add -A'
tick; quiet 'git -C "$SB/st/app-squash" commit -q -m "app3: 建仓库"'
runc 'cd ../app-squash && '"$PF"' subtree add --squash --prefix vendor/math ../libcopy main'
runc 'cd ../app-squash && echo "--- squash 之后上游历史被压成一笔："; git log --oneline | head -3; echo "--- 但文件是全的："; ls vendor/math'
note '对比总结（这一节两种都真跑了）：'
note '  submodule：宿主只存一条 160000 gitlink + 一个 sha；别人 clone 后要靠 update --init 才有文件；'
note '            改库必须「库里提交推送 → 宿主 bump gitlink」两步；库和宿主的历史互不污染。'
note '  subtree：  库的文件真的进了宿主的目录和历史，clone 就能用，不需要任何额外命令；'
note '            --squash 把上游历史压成一笔（干净，但 blame/log 到不了上游）；不 squash 就会像上面'
note '            那样多出一堆 merge，把 git log 拉长。想改这些文件回推上游，用 git subtree push。'
note '  经验：依赖只读、要紧跟上游、不想污染自己的历史 → submodule；想「拿来就当自己代码用」、'
note '        不想让别人为 submodule 操心 → subtree。'
else
    echo
    echo '本机无 git-subtree，未验证'
fi

# ============================================================================
banner "14. 收尾自检：体积对比 / git fsck / 沙箱里没留垃圾"
# ============================================================================
cd "$SB" || exit 1
runc 'echo "=== 同一份代码，三种做法的磁盘成本 ==="; du -sh app app-clone app-hotfix'
runc 'echo "--- 各自的 .git（worktree 那个只是个几十字节的指针文件）："; du -sh app/.git
       echo "    clone 副本的 .git（一整份历史，这才是真金白银）："; du -sh app-clone/.git
       echo "    worktree 工作区里的 .git：第一列 - = 文件，中间那个数字是字节数"; find app-hotfix/.git -maxdepth 0 -printf "%y %s %p\n"'
runc 'echo "--- worktree 的登记元数据一共占这么点："; du -sh app/.git/worktrees; ls app/.git/worktrees'
runc 'echo "--- 从 worktree 里数对象：和主仓库一模一样（没有第二份历史）"; git -C app-hotfix count-objects -v | head -3'
note '上面那行 warning: garbage found: .../app/.git/worktrees/app-hotfix/refs 不是本章造的垃圾：'
note '  git 自己就会在每个 linked worktree 的登记目录里建一个 refs/；而 count-objects 核对的是主仓库'
note '  .git 里那些名字，登记目录里的 refs 不在它的名单上，于是被当成 garbage 报了一句 warning。'
note '  判断仓库坏没坏看下面的 git fsck（一句报错都没有，退出码 0），这条 warning 忽略即可。'
runc 'echo "--- submodule 宿主的体积拆解（每个路径单独量：du 会把嵌套的参数跳过，一次列多个只出一行）"; du -sh sm/host
       echo "    其中 ① 库的工作区文件（历史不在这里）："; du -sh sm/host/libs
       echo "    其中 ② 宿主的整个 .git："; du -sh sm/host/.git
       echo "    其中 ② 里的 .git/modules = 被搬进来的库历史："; du -sh sm/host/.git/modules'
runc 'echo "--- 库的假远程（真正的历史在这里）："; du -sh sm/math.git
       if [ -d st ]; then
         echo "--- subtree 的做法（库内容 + 库全部历史都进了宿主）："; du -sh st/app
         echo "    其中 bring 进来的目录："; du -sh st/app/third_party
       else
         echo "（第 13 节没跑，没有 st 目录）"
       fi'
runc 'git -C app fsck'
runc 'git -C sm/host fsck'
runc 'git -C sm/fresh fsck'
runc 'echo "=== 每个仓库的 worktree 登记（确认没留残留登记）：";
       for r in app sm/host sm/fresh st/app; do [ -d "$r" ] || continue; echo "--- $r"; git -C "$r" worktree list; done'
runc 'echo "=== 沙箱顶层目录一览："; ls -1 "$SB"'
note '上面每一条输出都来自这一次运行；README 里引用的段落全部出自这份 capture。'

printf '\n[全部跑完] 本章 README 引用的每一段输出都出自这一次运行。\n'
