#!/usr/bin/env bash
# ============================================================================
# 第 10 章《故障现场：出事之后怎么查、怎么救》—— demo.sh
#
#   bash demo.sh                 跑完自动删沙箱（默认）
#   bash demo.sh --keep          跑完保留沙箱并打印路径，可以自己进去接着敲
#   bash demo.sh -h | --help     打印这段用法说明
#   bash demo.sh > cap.txt 2>&1  重定向时自动关颜色（README 引用的就是这种纯文本）
#
# 八个故障现场，每一节都是同一套动作：先真把东西弄坏 → 取证 → 修 → 验证修好了
#   第 1 节  游离 HEAD（detached HEAD）上做了提交，切走之后不见了
#   第 2 节  .git/index.lock 残留，凡是写索引的命令全灭
#   第 3 节  提交身份没配：git 要么当场罢工，要么默默把本机账号写进提交
#   第 4 节  git branch -D 误删本地分支
#   第 5 节  合并冲突收场：--ours / --theirs / merge --abort
#   第 6 节  暂存区被搞乱：restore --staged 动索引，restore 动工作区
#   第 7 节  pre-commit 钩子拦住提交（以及更常见的「钩子装了根本没跑」）
#   第 8 节  仓库变慢变胖：构建产物进了版本库，.gitignore 对已跟踪文件无效
#
# 安全说明（请先看这一段）：
#   * 所有 git 写操作只发生在 mktemp -d 出来的沙箱目录里，绝不碰本机任何已有仓库，
#     也不碰本手册所在的目录。收尾时 trap 会把沙箱删掉（除非 --keep）。
#   * HOME 被改写到沙箱里的 fakehome/，GIT_CONFIG_SYSTEM 指到 /dev/null：任何一条 git
#     命令都读不到、也写不到你真实的 ~/.gitconfig 和 /etc/gitconfig。脚本里【没有任何一条
#     git config --global】，也不碰 ~/.ssh。出现的 git config 全是 --local（只写沙箱仓库）。
#   * 提交身份一律走命令行：git -c user.name=学习者 -c user.email=learn@example.invalid。
#     屏幕上打印的命令里这两段 -c 是原样展开的（不隐掉），你一眼就能看到身份只来自命令行。
#     唯一不带这两个参数的，是第 3 节故意演的那几次失败。
#   * 不联网：没有 clone / fetch / pull / push / curl，这一章连「远程」都不需要。
#   * 故意【不加 set -e】，只开 -u：这一章的主角就是失败。每条命令的真实退出码都会打在下面，
#     所有报错文本都是 git 自己打印的原文，一行都没有手写。
#   * 唯一的一处文字替换：git 找不到身份时，会把「本机登录名@本机主机名」写进报错最后一行
#     （unable to auto-detect email address (got 'xxx@yyy.(none)')）；第 3 节还有一处 git 会
#     把登录名当作者名写进提交。这本手册的捕获里不许出现真实机器信息，所以那两条命令的输出
#     只把登录名 / 主机名这两个字符串换成 <本机登录名> / <本机主机名> 打印，其余逐行原文。
#     你在自己机器上跑 demo，看到的就是你自己机器的真实值。
#   * 提交时间用一个固定时钟递进（从 2026-01-01T09:00 起每笔 +3 分钟），文件内容全是定值，
#     所以同一台机器重复跑 hash 一致；每次都会变的只有沙箱绝对路径和文件时间戳。
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
unset GIT_DIR GIT_WORK_TREE GIT_INDEX_FILE GIT_CONFIG_COUNT 2>/dev/null
unset GIT_CONFIG_GLOBAL GIT_CONFIG_SYSTEM 2>/dev/null
unset GIT_AUTHOR_NAME GIT_AUTHOR_EMAIL GIT_COMMITTER_NAME GIT_COMMITTER_EMAIL 2>/dev/null
unset GIT_AUTHOR_DATE GIT_COMMITTER_DATE 2>/dev/null

# ── 沙箱：所有仓库都建在这里面 ─────────────────────────────────────────────
SB=$(mktemp -d "${TMPDIR:-/tmp}/git-handbook-10-XXXXXXXX")
mkdir -p "$SB/fakehome"
export HOME="$SB/fakehome"                 # 家目录关进沙箱：真实 ~/.gitconfig 读写不到
export XDG_CONFIG_HOME="$SB/fakehome/.config"
export GIT_CONFIG_SYSTEM=/dev/null         # 系统级配置不参与，保证第 3 节在任何机器都能复现

cleanup() {
    if [ "$KEEP" -eq 1 ]; then
        printf '\n[沙箱已保留] %s\n' "$SB"
        printf '  九个目录分别是：\n'
        printf '    01-detached  游离 HEAD      02-lock  index.lock 残留   03-identity  提交身份\n'
        printf '    04-branch    误删分支        05-merge  合并冲突         06-index     暂存区\n'
        printf '    06-never     从没 add 过      07-hooks  钩子             08-slow      仓库变胖\n'
        printf '  进去接着敲：cd "%s/01-detached" && git reflog\n' "$SB"
        printf '  或者直接看：git -C "%s/05-merge" status\n' "$SB"
        printf '  看完记得删：rm -rf "%s"\n' "$SB"
    else
        rm -rf "$SB"
        printf '\n[沙箱已删除] %s\n' "$SB"
    fi
}
trap cleanup EXIT

# ── 颜色：只有 stdout 是终端才上色；重定向到文件时是纯文本 ──────────────────
if [ -t 1 ]; then
    B=$'\033[1m'; C=$'\033[36m'; Y=$'\033[33m'; RED=$'\033[31m'; R=$'\033[0m'
else
    B=''; C=''; Y=''; RED=''; R=''
fi

banner() { printf '\n%s════════════════════════════════════════════════════════════════%s\n' "$B" "$R"
           printf '%s  %s%s\n' "$B" "$*" "$R"
           printf '%s════════════════════════════════════════════════════════════════%s\n' "$B" "$R"; }
sub()    { printf '\n  %s---- %s ---%s\n' "$Y" "$*" "$R"; }
note()   { printf '%s  ↳ %s%s\n' "$Y" "$*" "$R" ; }
acc()    { printf '%s  ★ %s%s\n' "$RED" "$*" "$R" ; }

# runc <命令字符串>：打印 $ 命令 → 用 bash -c 真跑（所以能写管道 / && / glob）→
# 原样打印 stdout+stderr → 退出码非 0 时在下面标一行
runc() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    bash -c "$1" 2>&1
    local ec=$?          # 必须写成一条语句：local ec; ec=$? 会被 local 自己把 $? 清成 0
    if [ "$ec" != 0 ]; then
        printf '%s[退出码 %s]  这段失败是本章故意演的，上面几行是 git 打印的原文%s\n' "$RED" "$ec" "$R"
    fi
    return 0
}

# 脱敏：git 的自动身份会把本机登录名 / 主机名带进报错或提交里，捕获里不留真实机器信息
LU=$(id -un 2>/dev/null || printf '')
LH=$(hostname 2>/dev/null || uname -n 2>/dev/null || printf '')
if [ -n "$LU" ] && [ -n "$LH" ]; then
    mask_id() { sed -e "s/${LU}@${LH}/<本机登录名>@<本机主机名>/g" -e "s/${LU}/<本机登录名>/g"; }
else
    mask_id() { cat; }
fi

# runc_id：和 runc 完全一样，只多一层脱敏；仅用在会打印自动身份的那几条命令上
runc_id() {
    printf '\n%s$ %s%s（这一条的输出做过脱敏：只把登录名/主机名换成占位符，其余原文）%s\n' "$C" "$1" "$Y" "$R"
    bash -c "$1" 2>&1 | mask_id
    local ec=${PIPESTATUS[0]}   # 同上：写成一条语句，否则 local 会把 PIPESTATUS 洗掉
    if [ "$ec" != 0 ]; then
        printf '%s[退出码 %s]  这段失败是本章故意演的，上面几行是 git 打印的原文%s\n' "$RED" "$ec" "$R"
    fi
    return 0
}

quiet() { bash -c "$1" >/dev/null 2>&1 || printf '  [搭场景失败：%s]\n' "$1"; }   # 搭场景，不刷屏

# ── 固定身份 + 固定时钟：身份只出现在命令行 -c 上，时钟只在环境变量里，不落任何配置 ──
Q="git -c user.name=学习者 -c user.email=learn@example.invalid"
TICK=$(( $(date -u -d '2026-01-01T09:00:00' +%s) ))
tick() {
    TICK=$((TICK + 180))
    local ts; ts=$(date -u -d "@$TICK" +%Y-%m-%dT%H:%M:%S+0000)
    export GIT_AUTHOR_DATE="$ts" GIT_COMMITTER_DATE="$ts"
}
mkrepo() {  # mkrepo <目录名>：在沙箱里新建一个仓库并 cd 进去
    mkdir -p "$SB/$1"
    quiet "$Q init -q -b main '$SB/$1'"
    cd "$SB/$1" || exit 1
}
commit() {  # commit <提交说明>：add -A + commit（安静），时间戳走固定时钟
    tick
    quiet "$Q add -A && $Q commit -q -m '$1'"
}

printf '%s第 10 章 demo：八个故障现场，每一个都先弄坏再救回来%s\n' "$B" "$R"
printf '本次沙箱：%s\n' "$SB"
printf '假家目录（HOME 已指到这里）：%s/fakehome\n' "$SB"
printf '跑法：bash demo.sh（自动删）｜ bash demo.sh --keep（保留）｜ bash demo.sh > cap.txt 2>&1（纯文本）\n'

# ============================================================================
banner "第 0 节 开工前先确认这台机器：版本 + git 到底在读哪份配置"
# ============================================================================
runc "git --version"
runc "echo 'HOME='\$HOME && ls -1A \"\$HOME\" | tr '\\n' ' ' && echo '(这一栏空 = 假家目录里没有 .gitconfig，你的那份也不在这里)'"
runc "git config --list --show-origin | sed -n '1,6p' ; echo '(上面一行都没有 = 此刻 git 读不到任何用户级配置)'"
runc_id "git var GIT_COMMITTER_IDENT"
note "上面这两条就是本章全部安全前提的证据：HOME 在沙箱里、系统级配置指向 /dev/null，"
note "所以 git 既读不到你的 ~/.gitconfig，也没有任何东西能写进去（第 3 节整节讲这件事）。"
note "打印出来的命令就是真实执行的命令：那两段 -c 原样留在上面，身份只来自命令行、没进任何配置文件。"

# ============================================================================
banner "第 1 节 游离 HEAD：切走之后，那两笔提交从所有分支上消失了"
# ============================================================================
note "这一步在演的事故：为了赶线上问题直接 checkout 到某个历史提交上改代码，连提两笔，"
note "然后 git switch main 去干别的活 —— 那两笔提交不属于任何分支，看起来人间蒸发。"
mkrepo 01-detached
printf '# 订单服务\n' > README.md
printf 'def total(items):\n    return sum(items)\n' > shop.py
commit "init: 订单服务骨架"
printf 'def total(items):\n    return sum(items) * 1\n' > shop.py
commit "feat: 接入折扣系数"
printf 'def refund(items):\n    return -sum(items)\n' > shop.py
commit "feat: 支持退款"
runc "git log --oneline && echo '(上面是 main 的三笔提交)'"
sub "1A 事故是怎么开始的：直接踩到一个历史提交上"
runc "$Q switch --detach HEAD~1 && echo '(这就是游离 HEAD：.git/HEAD 里写的不是分支名，而是裸 hash)'"
runc "git status --short --branch && git branch --show-current && echo '(这一栏空 = 当前不在任何分支上)'"
runc "cat .git/HEAD && git rev-parse HEAD && git rev-parse --abbrev-ref HEAD"
runc "git symbolic-ref HEAD"
note "symbolic-ref 报 not a symbolic ref —— 这是判断游离 HEAD 最干脆的一条命令。"
runc "$Q branch"
note "git branch 里那行 * (HEAD detached at ...) 说的是同一件事。"
sub "1B 在游离状态上真的做了两笔提交"
printf 'def total(items):\n    return sum(items) * 0\n' > shop.py
runc "$Q add shop.py && $Q commit -m 'fix: 线上紧急把折扣归零（第一笔）' && git log --oneline -2"
printf '# 紧急热修记录\n' > hotfix.md
runc "$Q add hotfix.md && $Q commit -m 'docs: 热修说明（第二笔）' && git log --oneline -3"
TIP=$(git rev-parse HEAD)
sub "1C 事故真正发生的时刻：切回 main"
runc "$Q switch main"
note "★ Warning: you are leaving 2 commits behind 是 git 给的最后一次提醒。它没有拦住你，只是告诉了你。"
runc "git log --oneline && echo '(main 上还是原来三笔，热修那两笔不见了)'"
runc "git branch -a && git log --all --oneline --grep='热修' && echo '(--all 也搜不到 = 所有分支上都看不见了)'"
sub "1D 取证：reflog 记的就是 HEAD 走过的每一步"
runc "git reflog | sed -n '1,7p'"
runc "git log -g --oneline --grep='热修'"
runc "echo 'reflog 在磁盘上就是这个纯文本文件：' && tail -3 .git/logs/HEAD"
note "动作发生在游离状态也照样记录：带 commit: 的那几行就是刚做的两笔。"
note "文件里第三段是「身份 + 时间」，那一栏的时间戳每次跑都不一样，属于正常现象。"
sub "1E 修复：给它挂一根分支指针，一步都不用重做"
runc "$Q branch rescue/线上急救 'HEAD@{1}' && git log --oneline -3 rescue/线上急救"
runc "git rev-parse --short 'HEAD@{1}' && git rev-parse --short rescue/线上急救 && echo '(两个写法指向同一个提交 = 确认没找错)'"
sub "1F 验证修复成功"
runc "$Q switch rescue/线上急救 && git status --short --branch && git log --oneline -1"
runc "git cat-file -t '$TIP' && git branch --contains '$TIP' && cat hotfix.md"
note "切回来之后文件内容还在（hotfix.md），游离期间写的东西一个字节都没少。"
note "如果当时还踩在游离状态没切走，更省事：git switch -c rescue/线上急救 一步就够了。"
runc "$Q switch -q main && $Q switch -q -c scratch/试一下 && $Q switch -q main && git rev-parse --abbrev-ref '@{-1}'"
note "@{-1} = 上一个待过的分支。切走之前先建分支，是这一节要的肌肉记忆。"

# ============================================================================
banner "第 2 节 .git/index.lock 残留：凡是写索引的命令，全部当场死掉"
# ============================================================================
note "这一步在演的事故：上一次 git add / commit 被 Ctrl-C 打断（或者断电、容器被杀），"
note "留下一个 0 字节的 .git/index.lock。之后 git status 还能跑，但 add / commit / stash 全灭。"
mkrepo 02-lock
printf 'v1\n' > notes.txt
printf 'src\n' > main.c
commit "c1 基线"
printf 'v2 改了还没提交\n' > notes.txt
quiet "touch .git/index.lock"           # ← 这就是那个残留
sub "2A 先看报错：git 到底在说什么"
runc "$Q add notes.txt"
runc "$Q commit -m 'chore: 想提交但被锁挡住'"
runc "$Q stash push -m '先收起来'"
runc "git write-tree"
note "★ 这四段说的是同一件事：Unable to create '.../.git/index.lock': File exists."
note "git 把两种可能都列了出来：别的进程正在跑，或者这个锁已经过期。它不敢替你决定，"
note "所以能不能删，得由下一步的取证来判断。"
sub "2B 取证：判断这到底是残留还是别人正在用（这一步决定你敢不敢删）"
runc "stat -c '%A %s 字节 %y %n' .git/index.lock"
note "git 写索引时先创建这个空文件、写完再改名成 index，所以 0 字节 + 时间戳对得上就是关键线索。"
runc "pgrep git >/dev/null && echo '★ 本机此刻确实有 git 进程在跑 → 不要删，等它结束' || echo '本机没有任何 git 进程在跑 → 这是上次崩掉的残留，可以删'"
note "这一步是唯一的判断题：CI 上、或者你开着两个终端同时操作同一个仓库时，答案会变。"
runc "git status --short && git log -1 --format='%h %s' && git diff --name-only && echo '(只读命令全部正常 → 仓库本身没坏，只是索引被锁住)'"
note "这句很重要：只读命令还能跑，就说明不是对象库损坏，别往 fsck / 重新 clone 那方向想。"
sub "2C 修复：只删这一个文件"
runc "rm -f .git/index.lock && echo '(index.lock 已删除；.git 下面其它东西一个都没碰)'"
sub "2D 验证修复成功"
runc "$Q add notes.txt && $Q commit -m 'chore: 锁清掉之后正常提交' && git log --oneline && git status --short && echo '(status 空 = 工作区干净，恢复正常)'"
sub "2E 同一类锁不止 index.lock：HEAD.lock 和 config.lock 长什么样"
runc "touch .git/HEAD.lock && $Q switch -c tmp/被挡住"
runc "rm -f .git/HEAD.lock && $Q switch -q -c tmp/能建了 && git branch --list 'tmp/*'"
runc "touch .git/config.lock && git config --local core.autocrlf input"
runc "rm -f .git/config.lock && git config --local core.autocrlf input && git config --local --list | tail -2"
acc "三条铁律：只删 .git 下面 *.lock 这一个文件；绝不 rm -rf .git；绝不删 .git/index 本体。"
acc "如果同一时间真有别的 git 在跑（比如 IDE 正在刷新索引），删锁会写坏索引 —— 先让它结束。"

# ============================================================================
banner "第 3 节 提交身份没配：要么当场罢工，要么默默把本机账号写进提交"
# ============================================================================
note "这一步在演的事故：新机器 / CI 容器里从来没配过 user.name 和 user.email，"
note "改动都 add 好了，git commit 直接给你一整屏 *** Please tell me who you are.。"
mkrepo 03-identity
printf 'def main():\n    pass\n' > app.py
quiet "$Q add -A"                       # 只有这一条带临时身份，为的是把「已 add 未提交」搭出来
sub "3A 取证：先确认这个仓库、这台机器（沙箱）到底有没有身份配置"
runc "git config --local --list && echo '(上面没有 user.* 这一行 = 仓库里也没配)'"
runc_id "git var GIT_COMMITTER_IDENT"
note "git var 是「问 git 你自己是谁」。它先死给你看，说明身份根本解析不出来。"
sub "3B 事故本体：git commit 的完整原文"
runc_id "git commit -m 'feat: 我的第一个提交'"
note "逐行拆这段报错：Author identity unknown 是结论；*** Please tell me who you are. 是提示；"
note "中间那两行 git config --global 是 git 给的『建议』——那会写你机器上所有仓库共用的配置；"
note "最后一行 fatal 是 git 试着用 登录名@主机名 凑一个邮箱，凑不出来才死给你看。"
acc "本脚本一条 git config --global 都不执行：全局配置属于你机器，救火不该顺手改它。"
sub "3C 就地救火：命令行 -c，只对这一次调用生效"
runc "$Q commit -m 'feat: 就地救火提交（身份只出现在命令行上）' && echo '(退出码 0 = 这一次提交成功了)'"
runc "git log -1 --format='%h 作者=%an <%ae> 提交者=%cn <%ce>%n%s'"
runc "git config --local --list && echo '(仍然没有 user.* → 说明 -c 没写进任何配置文件)' && ls -1A \"\$HOME\" | tr '\\n' ' ' && echo '(假家目录还是空的 → 也没有 ~/.gitconfig)'"
note "这就是救火该有的形状：这一次提交做掉，一点配置都不留。要长期生效就用 --local（只写这个仓库）。"
sub "3D 变体一：配了 user.name 但是空字符串（CI 脚本里极常见）"
runc "git config --local user.email learn@example.invalid && git config --local user.name '' && git config --local --list | grep '^user'"
printf 'x\n' > second.py
runc "git add second.py && git commit -m 'feat: 空字符串身份'"
note "★ 最后一行 fatal: empty ident name (for <...>) not allowed —— 邮箱有效、名字是空串，"
note "git 连自动检测都不试了。修法：git config --local --unset user.name，或者用 -c 顶掉。"
runc "git config --local --unset user.name"
runc "$Q add -A && $Q commit -m 'feat: 用命令行身份补上这一笔' && git log --oneline -2"
sub "3E 变体二（更阴的一种）：只配了 email 忘了 name —— git 不报错，默默用登录名"
runc "git config --local user.email learn@example.invalid && git config --local --list | grep '^user' && echo '(只有 user.email，没有 user.name)'"
printf 'y\n' > third.py
runc_id "git add third.py && git commit -m 'feat: 只配了 email 的那一笔' && git log -1 --format='%h 作者名=%an <%ae> 提交者名=%cn'"
acc "注意它没有报任何错，直接把本机登录名当作者名写进了提交 —— 一旦推到公开仓库，机器账号就进了历史。"
sub "3F 修：--amend --reset-author 把已经写错的身份换掉"
runc "$Q commit --amend --no-edit --reset-author"
runc "git log -1 --format='%h 作者=%an <%ae> 提交者=%cn <%ce> %s'"
runc "git config --local --remove-section user && git log --oneline && git status --short && echo '(user.* 全清掉，工作区干净)' && ls -1A \"\$HOME\""
note "这一节记住两条命令：临时救火 git -c user.name=... -c user.email=... commit；"
note "身份写错了 git commit --amend --reset-author（只在还没推给任何人时用）。"

# ============================================================================
banner "第 4 节 git branch -D：误删本地分支之后，从 reflog 里把它捞回来"
# ============================================================================
note "这一步在演的事故：一条分支上只有你自己知道的两个提交，git branch -D 顺手删了，"
note "连哈希都没记。区别在于分支删掉了、对象还在库里，reflog 里那几行记录一条都没少。"
mkrepo 04-branch
printf 'int main(void) { return 0; }\n' > core.c
commit "c1 基线"
runc "$Q switch -c wip/大改造 && printf 'step1\n' >> core.c && $Q commit -qam 'refactor: 大改造第一步' && printf 'step2\n' >> core.c && $Q commit -qam 'refactor: 大改造第二步（还没测）'"
LOST=$(git rev-parse HEAD)
runc "git log --oneline -3 && echo '(这条分支上有两笔只有我自己知道的提交)'"
sub "4A 先看护栏：git branch -d（小写）本来是想救你的"
runc "$Q switch main"
runc "git branch -d wip/大改造"
note "the branch is not fully merged —— 这一挡就是防止手滑；很多人嫌它烦，直接换 -D。"
sub "4B 事故：-D 强删"
runc "git branch -D wip/大改造"
note "★ 它打印的 Deleted branch ... (was <hash>) 括号里那个就是答案 —— 请当场复制下来。"
runc "git branch -a && git log --oneline -2 && echo '(分支没了，main 上也没有那两笔)'"
runc "git log --all --oneline --grep='大改造' && echo '(--all 搜不到 = 没有任何分支指向它们)'"
sub "4C 取证：reflog 里两条 commit 记录都在"
runc "git reflog | sed -n '1,5p'"
runc "git log -g --oneline --grep='大改造'"
runc "git cat-file -t '$LOST' && git show --stat --format='%h %s' '$LOST'"
note "cat-file 回答 commit —— 对象从没被删过，只是没人引用它了。"
sub "4D 修：拿 reflog 的下标直接建分支，不用手抄哈希"
runc "$Q branch wip/大改造 'HEAD@{1}' && git log --oneline -3 wip/大改造"
runc "git rev-parse --short 'HEAD@{1}' && git rev-parse --short wip/大改造 && git branch -v"
sub "4E 验证修复成功：切过去继续干活"
runc "$Q switch wip/大改造 && git log --oneline -1 && tail -2 core.c"
note "两笔提交全数回来，工作区内容也一样 —— 大改造第二步那行重新出现在 core.c 末尾。"
sub "4F 另一条路：reflog 也靠不住的时候，用 fsck 找悬空提交"
runc "$Q switch -q main && git branch -D wip/大改造 && git fsck --no-reflogs"
runc "DANGLING=\$(git fsck --no-reflogs | awk '/dangling commit/ {print \$3}') && echo \"悬空提交 = \$DANGLING\" && git log --oneline -3 \"\$DANGLING\""
runc "$Q branch wip/大改造 \$(git fsck --no-reflogs | awk '/dangling commit/ {print \$3}') && git branch -v"
note "git branch -D 只删了 refs/heads 下面那个小文件，对象库和父链一根没动，所以 fsck 还报得出来。"
sub "4G 什么时候真的救不回来（把这条当红线记住）"
runc "$Q branch -D wip/大改造 >/dev/null && git reflog expire --expire=now --all && git prune --expire=now && echo '(引用记录和不可达对象都被清了)'"
runc "git fsck --no-reflogs && echo '(空 = 再也没有悬空对象)' && git log -g --oneline 2>&1 | sed -n '1,3p'"
runc "git cat-file -t '$LOST'"
acc "默认宽限期：reflog 90 天（gc.reflogExpire）、不可达对象 2 周（gc.prune 的默认过期时间）。"
acc "所以：正在救场的时候，绝对不要顺手跑 git gc / git prune / reflog expire。"

# ============================================================================
banner "第 5 节 合并冲突收场：git status 的三段式提示、--ours / --theirs、merge --abort"
# ============================================================================
note "这一步在演的事故：主干和 hotfix 改了 app.conf 的同一行，git merge 把冲突摊在桌上就走人了。"
mkrepo 05-merge
printf 'timeout=30\nretries=3\nlog_level=info\n' > app.conf
printf '# 服务配置说明\n' > README.md
commit "c1 基线"
runc "$Q switch -q -c hotfix && printf 'timeout=5\nretries=3\nlog_level=info\n' > app.conf && $Q commit -qam 'hotfix: 线上超时从 30 秒改成 5 秒'"
runc "$Q switch -q main && printf 'timeout=30\nretries=9\nlog_level=info\n' > app.conf && $Q commit -qam 'main: 重试次数改成 9'"
sub "5A 事故：git merge 报冲突并退出"
runc "$Q merge hotfix"
note "CONFLICT (content) 只说哪个文件冲突；最后一行 Automatic merge failed 才是结论："
note "它没做完，把现场留在桌上等你。"
sub "5B 取证一：git status 的三段式提示（这是最好的说明书）"
runc "git status"
note "第一段 You have unmerged paths 说状态；两个括号给出两条出路："
note "fix conflicts and run git commit = 做完它；use git merge --abort = 整场放弃。"
note "第二段 Unmerged paths 里 both modified: 那行才是需要动手的文件清单。"
sub "5C 取证二：三个只读命令，看清冲突的三个版本"
runc "git diff --name-only --diff-filter=U"
runc "git diff --diff-filter=U"
runc "git ls-files -u"
note "★ diff --diff-filter=U 只看未合并（U = unmerged）的文件，冲突多时比 git status 好用。"
note "ls-files -u 里的 1 / 2 / 3 是三方合并的三个版本：1 = 共同祖先，2 = 我们（HEAD 这侧），3 = 他们（hotfix 那侧）。"
runc "sed -n '1,8p' app.conf && echo '(工作区里那份已经带上冲突标记了)'"
runc "ls .git | grep -i merge && git rev-parse MERGE_HEAD && git log -1 --format='合并进来的那一侧：%h %s' MERGE_HEAD"
note "MERGE_HEAD 存在 = 这场合并还没结束；它此刻就是版本 3 的出处。"
sub "5D 出路一：整份文件取某一侧 —— --ours 和 --theirs"
runc "$Q checkout --ours app.conf && cat app.conf"
note "ours = stage 2 = 你脚下这一侧（main）。冲突标记消失，文件从索引里取出来。"
runc "$Q checkout --theirs app.conf && cat app.conf"
note "theirs = stage 3 = 合并进来的那一侧（hotfix）。注意这一步只改工作区，索引里仍是未合并状态。"
runc "git status --short"
sub "5E 出路二：整场合并不要了 —— git merge --abort"
runc "$Q merge --abort"
runc "git status --short --branch && git log -1 --format='%h %s' && cat app.conf && (ls .git | grep -i merge || echo '(MERGE_* 全部消失)')"
note "abort 干的事：索引和工作区都退回合并前，HEAD 一笔没动 —— 这四样就是它成功的证据。"
sub "5F 再来一次，这回真的解决掉：手工合并 → add → commit"
runc "$Q merge hotfix"
printf 'timeout=5\nretries=9\nlog_level=info\n' > app.conf
runc "grep -c '<<<<<<<' app.conf ; echo '(上面是 0 = 冲突标记已经清干净)'"
runc "$Q merge --continue"
note "★ error: Committing is not possible because you have unmerged files —— 手工改完文件还不够，"
note "必须 git add 把「我决定了用这一版」写进索引，git 才认为这一处冲突解决完了。"
runc "$Q add app.conf && git status --short"
runc "$Q commit --no-edit && git log --graph --oneline -6 && git diff --name-only --diff-filter=U && echo '(再查一次未合并文件 = 空 → 冲突收场完成)'"
runc "cat app.conf"
note "--no-edit 让 git 直接用 .git/MERGE_MSG 里那句现成的 Merge branch 'hotfix'。"
acc "顺带记一句：rebase 里的 ours / theirs 方向和 merge 相反（rebase 时 ours = 你正在落地的那个分支）。"

# ============================================================================
banner "第 6 节 暂存区被搞乱：restore --staged 动索引，restore 动工作区"
# ============================================================================
note "这一步在演的事故：想『把 add 取消』，结果漏敲 --staged，git restore 直接覆盖工作区。"
note "两张区（索引 / 工作区）各丢一次、各救一次，你才知道哪一步真的能救。"
mkrepo 06-index
printf 'line1\nline2\nline3\n' > pay.py
printf '其他文件\n' > other.txt
commit "c1 基线"
sub "6A 开工前先看清三张区（后面所有判断都靠这三条命令）"
printf 'line1\nline2 改了但没 add\nline3\n' > pay.py
runc "git status --short"
runc "git diff --stat && echo '(git diff = 工作区 vs 索引)'"
runc "$Q add pay.py other.txt && git status --short && git diff --cached --stat && echo '(--cached = 索引 vs HEAD)'"
runc "git diff HEAD --stat && echo '(git diff HEAD = 工作区+索引 vs HEAD)' && cat pay.py"
sub "6B 事故一：整个目录的暂存一次退回（索引内容丢了，工作区没丢）"
runc "git restore --staged . && git status --short && git diff --cached --stat ; echo '(--cached 空了 = 暂存区被打回原形)'"
note "看 status --short：M 从第 1 格（已暂存）跑到了第 2 格（未暂存）。"
note "内容一个字没丢：restore --staged 只重写索引，工作区的文件它碰都不碰。"
runc "cat pay.py && echo '(工作区还是刚才那份 → 这一丢是可逆的)'"
runc "$Q add pay.py other.txt && git status --short && git diff --cached --stat && echo '(重新 add 一遍就恢复原状 —— 修复成功)'"
sub "6C 事故二：想取消 add，漏了 --staged（工作区那份真的从磁盘上消失）"
runc "git restore --staged pay.py && git status --short && tail -1 pay.py"
note "现在索引 = HEAD，工作区 = 我刚写好的那一版。这一步之后，磁盘上那一版是全世界唯一一份。"
runc "git restore pay.py && git status --short && echo '(status 全空 = 索引和工作区都和 HEAD 一致)' && tail -1 pay.py && git diff HEAD --stat ; echo '(diff HEAD 也空 = 我那行改动已经不在磁盘上了)'"
note "git restore 不带 --staged 时动的是工作区（--worktree 是默认值），它拿索引里的版本覆盖磁盘；"
note "刚才索引已经退回 HEAD，所以覆盖完就是 HEAD 那份 —— 我那一行没了。"
sub "6D 取证：这一版曾经 add 过，所以对象库里还留着它的 blob"
runc "git fsck --no-reflogs"
runc "for h in \$(git fsck --no-reflogs | awk '/dangling blob/ {print \$3}'); do echo \"--- blob \$h\"; git cat-file -p \"\$h\"; done"
note "★ 这就是 restore 之后唯一的活路：git add 写进对象库的那些 blob 还在，只是没人引用（dangling）。"
note "blob 多的时候靠内容认：git fsck --no-reflogs | awk '/blob/{print \$3}' | xargs -I{} git cat-file -p {} | grep -n 关键字"
sub "6E 修复 + 验证"
runc "git cat-file -p \$(git fsck --no-reflogs | awk '/dangling blob/ {print \$3}' | head -1) > pay.py && tail -1 pay.py && git status --short && git diff --stat"
acc "回来了：磁盘上那份和我当初 add 的一字不差，status 重新出现 M —— 这一丢救回来了。"
sub "6F 反面对照：add 过哪怕一秒 vs 从没 add 过"
mkrepo 06-never
printf 'base\n' > a.txt
commit "c1 基线"
printf '只在磁盘上、从没 add 过的一行：GONE-FOREVER-9999\n' > draft.txt
runc "git status --short && git fsck --no-reflogs | wc -l && echo '(0 = 对象库里根本没有它：从没 add 过的东西 git 从没见过)' && $Q add draft.txt"
runc "git fsck --no-reflogs | wc -l && echo '(现在是 1：只 add 了一秒钟，它就已经有了自己的对象)' && git restore --staged draft.txt && rm draft.txt"
runc "git fsck --no-reflogs ; git fsck --no-reflogs | awk '/dangling blob/ {print \$3}' | sed -n '1,1p' | while read -r h; do echo '把悬空 blob 的内容捞出来：'; git cat-file -p \"\$h\"; done"
note "上面把那句 GONE-FOREVER-9999 原样吐了出来 —— 唯一的条件就是你 add 过哪怕一次。"
acc "从没进过 git add 的文件，reflog / fsck 都救不了。预防只有一个：动手前先 git stash 或 git diff > 备份。"
sub "6G 顺手分清三兄弟：restore --staged / reset / checkout --"
cd "$SB/06-index"                       # 回到 pay.py 已经在 HEAD 里的那个仓库：不在 HEAD 时这三条命令的行为完全不同
quiet "$Q checkout -q HEAD -- pay.py"    # 把 6E 捞回来的那份先退回 HEAD 版，保证从这里开始工作区是干净的
runc "printf 'A 版本（马上 add）\n' >> pay.py && $Q add pay.py && printf 'B 版本（add 之后又写）\n' >> pay.py && git status --short"
note "MM pay.py = 索引里存着 A、磁盘上写着 A+B —— 这一格一格的状态就是本章全部判断的依据。"
runc "git restore --staged pay.py && git status --short && tail -2 pay.py && echo '(索引退回 HEAD，磁盘上的 A+B 完好)' && git diff --cached --stat ; echo '(--cached 空 → 索引确实被动了)'"
runc "$Q add pay.py && git reset -q && git status --short && tail -1 pay.py && echo '(git reset（不带 --hard）和 restore --staged 在这里等价：只动索引)'"
runc "$Q add pay.py && git checkout -- pay.py && git status --short && tail -2 pay.py && echo '(★ checkout -- 取的是索引里那一份，不是 HEAD：索引 = A+B，所以磁盘一个字没伤)'"
runc "git restore --source=HEAD --worktree pay.py && tail -2 pay.py && git status --short && echo '(明确 --source=HEAD 才是拿 HEAD 覆盖工作区：B 那行没了，但索引里还留着 A+B)' && git diff --stat && git diff --cached --stat"

# ============================================================================
banner "第 7 节 hooks 拦住提交：先演『装了没跑』，再演『真被拦住了』"
# ============================================================================
note "这一步在演的事故：团队要求不许把 DEBUG 开关提交上来，.git/hooks/pre-commit 已经写好，"
note "结果有人提交成功了（钩子压根没执行）；另有人提交被拒，却不知道是被谁拒的。"
mkrepo 07-hooks
printf 'DEBUG = False\nprint("hi")\n' > app.py
commit "c1 基线"
cat > .git/hooks/pre-commit <<'HOOK'
#!/usr/bin/env bash
# 团队规矩：不许把 DEBUG = True 提交上来
if grep -rqE '^[[:space:]]*DEBUG[[:space:]]*=[[:space:]]*True' --include='*.py' . 2>/dev/null; then
    echo 'pre-commit: 发现 DEBUG = True，这次提交被拒绝' >&2
    echo 'pre-commit: 把它改成 False；确认只是本地临时调试再考虑 git commit --no-verify' >&2
    exit 1
fi
exit 0
HOOK
quiet "chmod 644 .git/hooks/pre-commit"     # ← 故意忘了可执行位
sub "7A 事故一：钩子装了，但它根本没跑（提交照样成功）"
printf 'DEBUG = True\nprint("debug 版本")\n' > app.py
runc "stat -c '%A %s 字节 %n' .git/hooks/pre-commit && echo '(权限里没有 x → git 直接跳过这个钩子)'"
runc "git hook list pre-commit"
note "git hook list 是 2.45 之后的取证利器：它按 git 自己的理解告诉你这个事件上到底挂了几个钩子。"
runc "$Q add app.py && $Q commit -m 'wip: 带上调试开关'"
runc "git log --oneline -2 && git show --stat --format='这一笔改了：%s' HEAD"
acc "★ 上面 commit 输出里那两行 hint（hook was ignored because it is not set as executable）"
acc "  是 git 2.55 才有的好心提醒，老版本连 hint 都没有 —— 所以『提交成功了』不等于『检查通过了』。"
sub "7B 装对：补上可执行位"
runc "chmod +x .git/hooks/pre-commit && stat -c '%A %s 字节 %n' .git/hooks/pre-commit && git hook list pre-commit"
sub "7C 事故二：钩子真的把提交拦下来了"
runc "$Q reset -q --hard HEAD~1 && printf 'DEBUG = True\nprint(\"debug 版本\")\n' > app.py && $Q add app.py"
runc "$Q commit -m 'wip: 这次该被拦住了'"
runc "git log -1 --format='%h %s' && git status --short && echo '(M 在第 1 格：改动还留在索引里，只是这次提交没成 —— 被拦不等于丢失)'"
note "被拒时 git 只把钩子的 stderr 原样打出来，自己不多说一句，退出码 1。"
note "所以读报错要先认是谁在说话：pre-commit: 开头的是你（或团队）自己写的那个脚本。"
sub "7D 改对：按钩子的要求把内容改了再提交"
runc "grep -n 'DEBUG' app.py"
runc "sed -i 's/^DEBUG = True/DEBUG = False/' app.py && grep -n 'DEBUG' app.py && $Q add app.py"
runc "$Q commit -m 'fix: 关掉调试开关，让钩子放行' && git log --oneline -2 && git status --short && echo '(空 = 提交成功且工作区干净 → 修复验证通过)'"
sub "7E 临时绕过：--no-verify（知道它有用，才知道它多危险）"
runc "printf 'DEBUG = True\n' >> app.py && $Q add app.py && $Q commit --no-verify -m 'wip: 强行绕过钩子'"
runc "git log -1 --format='%h %s' && grep -c 'DEBUG = True' app.py && echo '(仓库里现在真的写着 DEBUG = True —— 绕过的代价留在了历史里)'"
runc "$Q reset -q --hard HEAD~2 && grep -n 'DEBUG' app.py && git status --short && echo '(把后面两笔一起撤掉，回到基线状态)'"
sub "7F 取证工具箱：这个仓库的钩子到底从哪儿来"
runc "git config --local core.hooksPath ; echo '(退出码 1 = 没配 hooksPath，钩子只认 .git/hooks 这个目录)'"
runc "ls .git/hooks | sed -n '1,6p' ; echo '（那些 *.sample 永远不生效：文件名不对，把 .sample 去掉才算装上）'"
acc "钩子躺在 .git 里，不会随 clone 传播。团队要统一拦截，就把 core.hooksPath 指向仓库里被跟踪的目录，"
acc "或者把检查做成一条普通命令（CI / pre-commit 框架），别指望每个人手工拷文件。"

# ============================================================================
banner "第 8 节 仓库变慢、体积异常：构建产物进了版本库，.gitignore 对已跟踪文件无效"
# ============================================================================
note "这一步在演的事故：有人把 build/ 目录一起提交了，之后每次重编都产生一整份新快照；"
note "仓库越来越胖、clone 越来越慢，而补一份 .gitignore 什么都没解决。"
mkrepo 08-slow
mkdir -p src build
printf '# 慢仓库示例\n' > README.md
printf 'int main(void) { return 0; }\n' > src/main.c
commit "c1 只有源码"
# 定值伪随机：同一个种子永远出同一份内容，所以 hash 和体积都能复现
gendata() { awk -v s="$2" 'BEGIN{srand(s); for(j=0;j<6000;j++) printf "%d %d\n", int(rand()*1000000), int(rand()*1000000)}' > "$1"; }
gendata build/app.o 8801
gendata build/app2.o 8802
printf '编译日志一行\n' > build/build.log
commit "c2 手滑把 build 目录也提交了"
for i in 1 2 3; do
    gendata build/app.o "99$i"
    commit "c$((2+i)) 重新编译了一次"
done
sub "8A 事故的表现：仓库怎么这么大、这么慢"
runc "git count-objects -v"
note "count = 松散对象个数，size = 它们的体积（KiB），in-pack / size-pack = 已打包的部分。"
runc "git count-objects -vH | sed -n '1,3p' && du -sh .git .git/objects"
runc "git ls-files | tr '\\n' ' ' && echo && git ls-files build"
acc "★ 一眼定性：源码只有两个文件，跟踪列表里却有三个 build 产物，而且同一个产物被重编了四次 —— "
acc "  Git 存的是完整快照不是差量，每笔提交都存一整份，体积就是这么吃掉的。"
sub "8B 取证：谁在吃空间（从大到前列出 5 个 blob）"
runc "git rev-list --objects --all | git cat-file --batch-check='%(objecttype) %(objectname) %(objectsize) %(rest)' | awk '/^blob/ {print \$3, \$4}' | sort -rn | sed -n '1,5p'"
runc "echo '被跟踪的源码有多大：' && git cat-file -s \$(git rev-parse HEAD:src/main.c) && git cat-file -s \$(git rev-parse HEAD:README.md) && stat -c '%s 字节 %n' build/*"
note "cat-file --batch-check 是查体积的正解；verify-pack 得先有 pack 才能用，撞坑的样子见 8E。"
sub "8C 现场证明：.gitignore 对【已跟踪】文件完全无效"
printf 'build/\n*.o\n*.log\n' > .gitignore
runc "$Q add .gitignore && $Q commit -q -m 'chore: 补了 .gitignore，希望 build 从此消失' && git status --short && echo '(此刻还没改产物，所以 status 是空的)' && cat .gitignore"
gendata build/app.o 6601
runc "git status --short"
acc "★ 我都 ignore 了它为什么还在报？—— .gitignore 只解释【未跟踪】的文件；"
acc "  build/app.o 已经在索引里，它归索引管，忽略规则一句都插不上话。"
runc "git check-ignore -v build/app.o ; echo '(退出码 1 + 什么都没打印 —— check-ignore 在这里会骗你)'"
runc "git check-ignore -v --no-index build/app.o"
note "不带 --no-index 时 check-ignore 直接跳过已跟踪文件（它认为轮不到规则管），所以什么都没给；"
note "带 --no-index 才看见 .gitignore:1:build/ 确实命中了。规则没问题，问题在跟踪状态。"
runc "git ls-files -- build | tr '\\n' ' ' && echo '(这就是仍在跟踪的铁证)' && git status --short --ignored | tail -4"
sub "8D 处方：git rm -r --cached —— 停止跟踪，磁盘上的文件一个不动"
runc "git rm -r --cached build/"
runc "git status --short && ls -1 build | tr '\\n' ' ' && echo '(D 在第 1 格 = 已暂存的停止跟踪；三个产物文件还在磁盘上)' && head -1 build/build.log"
runc "$Q add -A && $Q commit -q -m 'chore: build 目录停止跟踪（文件留在本地）' && git status --short && echo '(空 = 干净)'"
gendata build/app.o 6602
runc "git status --short && echo '(★ 现在再改产物，status 完全不报 —— 这才是大家以为 .gitignore 一开始就该有的效果)' && git check-ignore -v build/app.o && git ls-files -- build ; echo '(索引里已经没有 build 了 → 修复验证通过)'"
sub "8E 但是：历史里那几版产物一个字节都没少（gc 也救不了这个）"
runc "git log --oneline -- build/app.o"
runc "git count-objects -vH | sed -n '1,3p' && du -sh .git"
runc "git verify-pack -v .git/objects/pack/*.idx 2>&1 | sed -n '1,2p'"
note "★ 这条是真实踩坑：一个 pack 都还没有时 glob 展不开，verify-pack 把 * 当文件名直接报 fatal。"
runc "$Q gc -q && git count-objects -vH && ls .git/objects/pack | wc -l"
runc "git verify-pack -v .git/objects/pack/*.idx | awk 'NF==5 && \$2==\"blob\" {print \$3, \$1}' | sort -rn | sed -n '1,3p'"
note "gc 只把松散对象打进 pack（size 那一栏从 KiB 级变成更小的 size-pack），它从不删历史。"
note "想让历史里那几版产物真的消失，要 git filter-repo 重写历史 + 全员重新 clone —— 那是不可逆的手术，"
note "本手册不替你演；日常预防只有一条：把产物目录写进 .gitignore，趁它【第一次 add 之前】。"
sub "8F 验证 + 日常体检三条"
runc "git fsck --no-reflogs && echo '(没有悬空 / 损坏对象 —— 体积是一回事，一致性是另一回事)'"
runc "git ls-files | tr '\\n' ' ' && echo && git status --short --ignored | tail -6"
note "体检三条，写成定时任务也行的那种：git count-objects -v（松散对象堆积没有）、"
note "git ls-files（跟踪列表里混进产物没有）、git status --ignored --short（忽略规则到底生效没有）。"

# ============================================================================
banner "第 9 节 收尾：出事之后的第一条命令"
# ============================================================================
cat <<'ART'

  症状（你看到的）                          先敲这一条                    真正的修复动作
  ----------------------------------------  ----------------------------  --------------------------------------
  提交不见了 / branch 显示 detached         git reflog                    git branch 新名字 <hash> 或 HEAD@{n}
  add/commit 报 Unable to create ...lock     stat + pgrep git              确认是残留之后 rm -f .git/index.lock
  commit 报 Please tell me who you are       git var GIT_COMMITTER_IDENT   命令行 -c 就地救火，别顺手 --global
  git branch -D 删错了                      git reflog（其次 fsck）        git branch 分支名 <hash>
  merge 撞冲突                              git status / ls-files -u        --ours / --theirs / 手工改 → add → commit
  想取消 add 却把工作区弄没了                git fsck --no-reflogs          git cat-file -p <dangling blob> > 文件
  提交被拒 / 提交居然没被拒                  git hook list pre-commit       补 chmod +x；改对内容，慎用 --no-verify
  仓库越来越胖                              count-objects -v / ls-files    git rm -r --cached 产物目录，再提交

  八句话带走：
  1) 你的提交几乎不会真的丢：先 reflog，再 git fsck --no-reflogs，两条路指着同一个对象库。
  2) 救场第一件事是取证，不是修复：报错原文、git status、退出码都还在的时候先抄下来。
  3) 动 .git 只允许动 *.lock 这一类文件；想 rm -rf .git 之前，先把整个目录复制一份。
  4) gc / prune / reflog expire 是清空回收站，救场期间一条都不要跑。
  5) restore --staged 动索引，restore 动工作区；只有 add 过的内容才谈得上恢复。
  6) 身份优先用命令行 -c 或 --local；--global 改的是整台机器，而没配 name 时 git 会默默用登录名。
  7) .gitignore 只管未跟踪文件；已跟踪的用 git rm --cached，历史里那一份得单独做手术。
  8) 每个「修复成功」都要有证据：status 干净、log 里有那一笔、文件内容和预期一致 —— 三样都对才算救回来。

ART
runc "git status --short --branch && git log --oneline -1 && echo '(最后一个现场 08-slow 收尾时的样子)'"
printf '%s=== demo 结束：以上每一段 \$ 命令和它下面的输出，都是刚刚在这台机器上真跑的 ===%s\n' "$B" "$R"
