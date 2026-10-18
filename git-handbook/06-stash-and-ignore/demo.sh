#!/usr/bin/env bash
# 第 06 章 demo：stash 暂存与 .gitignore 忽略
#
# 用法：
#   bash demo.sh          跑完自动删除沙箱
#   bash demo.sh --keep   跑完保留沙箱并打印路径，你可以进去自己敲命令
#
# 安全说明（请先看这一段）：
#   * 所有 git 操作只在 `mktemp -d` 出来的临时沙箱里做，绝不碰你机器上任何已有仓库。
#   * 脚本一开始就把 HOME 指到沙箱里的假家目录 fakehome/：
#       export HOME="$SB/fakehome"
#     所以任何一条 git 命令都不可能读到你真实的 ~/.gitconfig，更不可能写它。
#     演示「全局忽略文件 core.excludesFile」时，那份假 .gitconfig 是用 cat 手写在
#     fakehome/ 里的 —— 脚本里【没有任何一条 git config --global】，也不碰 ~/.ssh。
#   * 提交身份只在命令行上给：git -c user.name=学习者 -c user.email=learn@example.com，
#     不写进任何配置文件。屏幕上为了好读会把这两个 -c 隐掉，真实执行的命令里它们是存在的。
#   * 不联网：本章一个 clone / push / fetch 都没有，也不需要远程仓库。
#   * 故意【不加 set -e】：这一章要给你看命令失败长什么样（原文报错 + 非 0 退出码）。
#   * git clean 全程只跑 dry-run（-n / -nd / -nxd），脚本不会真的删掉任何文件。
#   * 时间戳固定从 2026-01-01T10:01:00 +0800 起逐条递增，所以 hash 可以复现。
#     （唯一例外：沙箱绝对路径每次随机，带路径的那几行会对不上，其余完全一致。）

KEEP=0
for a in "$@"; do
  case "$a" in
    --keep) KEEP=1 ;;
    -h|--help) sed -n '1,32p' "$0"; exit 0 ;;
    *) echo "未知参数：$a（只支持 --keep）" >&2; exit 2 ;;
  esac
done

SB=$(mktemp -d "${TMPDIR:-/tmp}/git-handbook-06-stash-ignore-XXXXXXXX")
cleanup() {
  local rc=$?
  if [ "$KEEP" = 1 ]; then
    echo
    echo "沙箱已保留，可以进去继续敲：cd $SB"
    echo "各场景目录：$(ls -1 "$SB" | tr '\n' ' ')"
    echo "看完记得删：rm -rf $SB"
  else
    rm -rf "$SB"
    echo
    echo "沙箱已清理：$SB"
  fi
  exit "$rc"
}
trap cleanup EXIT

# ── 环境：把 HOME 关进沙箱；输出固定用英文，不受你系统语言影响 ──────────────
mkdir -p "$SB/fakehome"
export HOME="$SB/fakehome"
export LC_ALL=C.UTF-8
export GIT_PAGER=cat PAGER=cat GIT_EDITOR=true GIT_MERGE_AUTOEDIT=no
unset GIT_DIR GIT_WORK_TREE GIT_INDEX_FILE GIT_AUTHOR_DATE GIT_COMMITTER_DATE 2>/dev/null
unset GIT_CONFIG_GLOBAL GIT_CONFIG_SYSTEM 2>/dev/null

# ── 颜色：只有输出到终端（stdout 是 tty）才上色；重定向到文件时是纯文本 ─────
if [ -t 1 ]; then
  C_TITLE=$'\033[1;36m'; C_SUB=$'\033[1;35m'; C_CMD=$'\033[1;32m'
  C_OK=$'\033[2m'; C_BAD=$'\033[1;31m'; C_NOTE=$'\033[33m'; C_OFF=$'\033[0m'
else
  C_TITLE=''; C_SUB=''; C_CMD=''; C_OK=''; C_BAD=''; C_NOTE=''; C_OFF=''
fi

GID='-c user.name=学习者 -c user.email=learn@example.com'
G="git $GID"

# 隐掉身份参数 —— 只影响显示，不影响执行
pretty() { printf '%s' "$1" | sed "s/ $GID//g"; }

say()  { printf '\n%s════════════════════════════════════════════════════════════════%s\n' "$C_TITLE" "$C_OFF"
         printf '%s  %s%s\n' "$C_TITLE" "$*" "$C_OFF"
         printf '%s════════════════════════════════════════════════════════════════%s\n' "$C_TITLE" "$C_OFF"; }
sub()  { printf '\n  %s---- %s ---%s\n' "$C_SUB" "$*" "$C_OFF"; }
note() { printf '%s  ↳ %s%s\n' "$C_NOTE" "$*" "$C_OFF"; }

# R <命令字符串>：打印 $ 命令 → 原样打印 stdout+stderr → 报告退出码
R() {
  local out ec
  printf '\n%s$ %s%s\n' "$C_CMD" "$(pretty "$*")" "$C_OFF"
  out=$(eval "$*" 2>&1); ec=$?
  [ -n "$out" ] && printf '%s\n' "$out"
  if [ "$ec" = 0 ]; then
    printf '%s[退出码 0]%s\n' "$C_OK" "$C_OFF"
  else
    printf '%s[退出码 %s]  ★ 上面这段失败是本章故意让你看的%s\n' "$C_BAD" "$ec" "$C_OFF"
  fi
  return 0
}
q() { eval "$*" >/dev/null 2>&1; }   # 安静执行（搭场景用，不刷屏）

SEQ=0
tick() {  # 每造一个提交 / 每一个 stash 换一个固定时间，保证 hash 可复现
  SEQ=$((SEQ + 1))
  local total=$((600 + SEQ)) hh mm ts
  hh=$((total / 60)); mm=$((total % 60))
  printf -v ts '2026-01-01T%02d:%02d:00 +0800' "$hh" "$mm"
  export GIT_AUTHOR_DATE="$ts" GIT_COMMITTER_DATE="$ts"
}
newrepo() { # newrepo <目录名> [现场说明]
  local dir="$SB/$1"
  mkdir -p "$dir"
  R "git init -q -b main '$dir'"
  cd "$dir" || exit 1
  [ -n "${2:-}" ] && note "$2"
}
qc() { # qc <提交说明>：add -A + commit（安静），然后把这条提交打印出来
  tick
  q "$G add -A"
  q "$G commit -q -m '$1'"
  note "已提交：$(git log -1 --format='%h %s')"
}
w() { printf '%b' "$2" > "$1"; }     # w <文件> <内容>，内容里可以用 \n

echo "沙箱位置：$SB"
echo "假家目录（HOME 已经指到这里）：$SB/fakehome"

###############################################################################
say "第 1 节 stash 到底是什么：一根 ref + 三个 commit，全部拍在桌上给你看"
###############################################################################
newrepo s1-anatomy "现场：基线已提交；改了 2 个文件（其中一个 add 过）；还新建了 1 个未跟踪文件"
w README.md '项目说明\n'
w a.txt 'a 原始\n'
w b.txt 'b 原始\n'
qc "c1 基线：三个文件"

w a.txt 'a 原始
a 的改动一（没 add）
'
w b.txt 'b 原始
b 的改动二（这个 add 过）
'
q "$G add b.txt"
w pay-draft.txt '未跟踪：支付草稿，还没想好要不要留\n'

sub "1A stash 之前：工作区 / 暂存区 / 未跟踪各是什么状态"
R 'git status --short'
R 'git diff --stat'
R 'git diff --cached --stat'
note "上面两屏差异就是 stash 马上要吃掉的东西（外加未跟踪的 pay-draft.txt，因为下面用了 -u）。"

sub "1B 存起来"
R "$G stash push -u -m '登录页改到一半，先让开位置'"
R 'git status --short'
note "★ 工作区唰地干净了。这不是「藏起来」的假象：你的改动被做成了真的 Git 对象。"

sub "1C git stash list：它就是个栈"
R 'git stash list'

sub "1D 物理证据一：refs/stash 就是一个 41 字节的文本文件（和分支指针同一种东西）"
R 'ls -l .git/refs'
R 'cat .git/refs/stash'
R "git cat-file -t '$(git rev-parse stash@{0})'"
note "01 章说过：分支就是一个写着 hash 的文本文件。refs/stash 一模一样，只不过它不在 refs/heads 下面。"

sub "1E 物理证据二：stash@{0} 本身是一个 commit，而且不止一个父亲"
R "git log -1 --format='hash：%H%n父亲们：%P%n说明：%s' 'stash@{0}'"
R "git show --stat 'stash@{0}'"
note "这个 commit 的 tree 就是你 stash 那一刻【工作区】的样子。"

sub "1F 三个父亲分别是谁（本节最该记住的三行）"
R "git show --stat 'stash@{0}^'"
note "^1 = 你 stash 时 HEAD 所在的那个提交 —— 干净基线。pop 时 git 就是拿它当参照物做三方合并。"
R "git show --stat 'stash@{0}^2'"
note "^2 = 暂存区快照（说明是 index on main: ...）。你 add 过的东西在这，所以一点不丢。"
R "git show --stat 'stash@{0}^3'"
note "^3 = 未跟踪文件快照（untracked files on main: ...）。只有带 -u / -a 时才有这个父亲。"
cat <<'ART'

  stash 的物理形状（三个普通 commit + 一根指着它的 ref）：

      .git/refs/stash ─── 文本文件，写着下面这个 commit 的 hash
             │
             ▼
      stash@{0}  commit "On main: 登录页改到一半，先让开位置"
        ├─ ^1 ── 你 stash 时 HEAD 所在的提交          ← 干净基线（参照物）
        ├─ ^2 ── index on main: ...                  ← 暂存区长什么样
        └─ ^3 ── untracked files on main: ...        ← 未跟踪文件（要 -u / -a 才有）

      .git/logs/refs/stash ── 栈的流水账（git stash list 读的就是它）
ART

sub "1G 栈的流水账：git stash list 其实就是 git log -g refs/stash"
R 'git log -g stash --oneline'
R 'cat .git/logs/refs/stash'
note "旧 hash、新 hash、身份和时间、最后是你那句 -m 说明。栈的顺序就是靠这个文件维持的。"

sub "1H 不 pop 也能先看内容：git stash show"
R 'git stash show --stat'
R 'git stash show -p'
note "不带参数默认看 stash@{0}；看栈里别的：git stash show -p 'stash@{1}'。"

###############################################################################
say "第 2 节 apply 和 pop 差一个字：pop 会顺手把栈里那条删掉"
###############################################################################
newrepo s2-apply-pop "现场：三个已跟踪文件，先只改一个"
w base.txt '第一行\n'
w f1.txt 'f1 基线\n'
w f2.txt 'f2 基线\n'
qc "c1 基线"
printf '第一行\n第二行 我的改动\n' > base.txt
R "$G stash push -m '先只改一半'"

sub "2A apply：改动回到工作区，栈里那条【还在】"
R 'git stash apply'
R 'git status --short'
R 'git stash list'
note "apply 之后同一份改动同时存在于工作区和 stash 栈里，所以 apply 是安全的那个：弄错了还能重来。"

sub "2A-2 手滑现场：apply 完又 pop，被 Git 当场拒绝"
R 'git stash pop'
R 'git stash list'
note "error: Your local changes ... would be overwritten by merge —— 工作区已经有同一份改动了。"
note "★ 最后一行仍然是 The stash entry is kept in case you need it again.：pop 失败不吞东西（第 3 节整节讲这件事）。"

sub "2B pop：先把工作区退回干净，再看 pop 真正做的两件事"
R 'git restore base.txt'
R 'git status --short'
R 'git stash pop'
R 'git status --short'
R 'git stash list'
note "pop = apply + drop，一条命令两步。它打印的 Dropped refs/stash@{0} (<hash>) 里那个 hash，请养成习惯扫一眼。"

sub "2C 空栈上再 pop / drop：手滑按第二次上箭头时的画面"
R 'git stash pop'
R 'git stash drop'
R 'git stash apply stash@{3}'
note "三条都是退出码 1，但【什么都没丢】—— 空栈上 pop/drop 是无害的，别慌。"

sub "2D 进阶：暂存区也想原样回来，用 apply --index"
R 'git restore base.txt'
printf 'f1 改动（马上 add）\n' > f1.txt
q "$G add f1.txt"
printf 'f1 又改了（没 add）\n' > f1.txt
printf 'f2 改动（没 add）\n' > f2.txt
R 'git status --short'
note "MM f1.txt = 暂存区里存着一版、工作区又改了一版。stash 会怎么收这种现场？"
R "$G stash push -m '暂存区和工作区各有一版'"
R 'git status --short'
R "git show --stat 'stash@{0}^2'"
note "^2 那份快照里只有 f1 的第一版 —— 这就是「暂存区也被一起存下来了」的铁证。"

sub "2D-1 apply 默认【不】还原暂存状态：所有改动都退回未暂存"
R 'git stash apply'
R 'git status --short'
note "f1 也变成了 ' M'（M 在第 2 格）。它曾经 add 过这件事被抹平了。"

sub "2D-2 换成 apply --index：MM 的形状原样还原"
R 'git restore .'
R 'git status --short'
R 'git stash apply --index'
R 'git status --short'
note "MM 回来了。要连暂存状态一起恢复就得带 --index；pop 也一样：git stash pop --index。"

###############################################################################
say "第 3 节 pop 撞车现场：★ stash 不会被丢弃（保命条款，务必看完）"
###############################################################################
newrepo s3-conflict "现场：你改了第 2 行并 stash，然后同一行又被上游提交了新版本"
w app.txt 'line1
line2 原始
line3
line4
'
qc "c1 基线"
printf 'line1\nline2 我改的（本地实验）\nline3\nline4\n' > app.txt
R "$G stash push -m '本地实验先收起来'"
printf 'line1\nline2 上游把这一行改了\nline3\nline4\n' > app.txt
qc "c2 上游改了第 2 行"
R 'git stash list'

sub "3A pop：内容冲突 —— git 把栈里那条留着了"
R 'git stash pop'
R 'git stash list'
note "★★ 请记住这句英文：The stash entry is kept in case you need it again."
note "pop 失败时 Git 不会吞掉你的东西。这就是「pop 撞冲突」和「pop 成功」唯一的区别。"

sub "3B 冲突现场长这样：标记两端都写着来源"
R 'git status --short'
R 'cat app.txt'
note "<<<<<<< Updated upstream = 你现在脚下这份（含 c2）；>>>>>>> Stashed changes = stash 里那份。"

sub "3C 千万别在没解决之前再 pop 一次"
R 'git stash pop'
R 'git stash apply'
note "error: could not write index + needs merge —— 工作区还带着未完成的合并。先解决，别硬来。"

sub "3D 出路一：整份文件取 stash 那一侧 → add → 确认后再 drop"
R 'git checkout --theirs app.txt'
R 'cat app.txt'
R "$G add app.txt"
R 'git status --short'
note "改动已经在工作区了，栈里那条现在只是多余的副本 —— 确认无误再删："
R 'git stash drop'
R 'git stash list'

sub "3E 出路二：退回干净状态，把 stash 留在栈里改天再说"
printf 'line1\nline2 我改的（本地实验）\nline3\nline4\n' > app.txt
R "$G stash push -m '再收一次'"
printf 'line1\nline2 上游又改了一次\nline3\nline4\n' > app.txt
q "$G add -A"; q "$G commit -q -m 'c3 上游再改一次'"
R 'git stash pop'
R 'git stash list'
note "先看这条 stash list：东西还在。【确认还在之后】才谈清理工作区："
R 'git reset -q --hard HEAD'
R 'git status --short'
R 'git stash list'
note "★ 顺序很重要：先确认 stash 还在，再 reset --hard。反过来就是自己砸自己（03 章）。"
note "另一条更稳的路是 6D 的 git stash branch —— 回到 stash 的基线上再解决冲突。"

sub "3F 另一种翻车：未跟踪文件撞名，pop 直接不干活"
R 'git stash clear'
printf '我的半成品草稿\n' > draft.txt
R "$G stash push -u -m 'draft.txt 是未跟踪的'"
printf '别人新建的同名文件\n' > draft.txt
R 'git stash pop'
R 'cat draft.txt'
R 'git status --short'
R 'git stash list'
note "draft.txt already exists, no checkout —— Git 不肯覆盖工作区那个同名文件，整个 apply 失败、stash 留着。"
note "解法：先把工作区那个 draft.txt 改名或删掉（它不是你要的那份），再 pop。"

###############################################################################
say "第 4 节 -u 和 -a：默认只收「Git 认识的文件」"
###############################################################################
newrepo s4-u-and-a "现场：已跟踪文件有改动、有未跟踪新文件、还有被 .gitignore 掉的编译产物"
w .gitignore 'build/\n*.log\n'
w src.txt 'src 原始\n'
qc "c1 基线（含 .gitignore）"
mkdir -p build
printf '编译产物\n' > build/app.o
printf '日志内容\n' > run.log
printf 'src 改了一版\n' > src.txt
printf '未跟踪的临时脚本\n' > scratch.sh

sub "4A 普通 stash：未跟踪的和被忽略的都留在原地"
R 'git status --short'
R "$G stash push -m '只收已跟踪的'"
R 'git status --short'
note "对比上面两屏：?? scratch.sh 纹丝不动。stash 默认只管 Git 认识的文件。"
R 'git stash pop >/dev/null'

sub "4B 反例：只剩未跟踪文件时，普通 stash 什么都不做（但退出码是 0）"
printf 'src 原始\n' > src.txt
R 'git status --short'
R "$G stash push -m '我以为收走了'"
R 'git stash list'
note "★ No local changes to save，而退出码还是 0 —— 脚本里别拿退出码判断「stash 成功了吗」。"

sub "4C -u：连未跟踪文件一起收走（第三个父亲就是这么来的）"
printf 'src 又改了一版\n' > src.txt
R 'git status --short'
R "$G stash push -u -m '连未跟踪一起收'"
R 'git status --short'
R 'ls -1'
note "scratch.sh 从磁盘上消失了 —— 它被写进了 stash@{0}^3 那个对象里，不是被删了。"
R "git log -1 --format='父亲们：%p' 'stash@{0}'"
R "git show --stat 'stash@{0}^3'"

sub "4D -a：连被 .gitignore 掉的也一起收（build/ 和 run.log 都没了）"
R 'git stash pop >/dev/null'
R 'git status --ignored --short'
R "$G stash push -a -m '连 gitignore 掉的也收'"
R 'ls -1'
R "git show --stat 'stash@{0}^3'"
note "被忽略的编译产物也进了 ^3。这个选项用处小、风险大：回来时容易和你重新产出的文件撞名（见 3F）。"
R 'git stash list'
R 'git stash pop >/dev/null; ls -1'

###############################################################################
say "第 5 节 只想收其中几个文件：git stash push -- <路径>"
###############################################################################
newrepo s5-paths "现场：同时改着三个文件，但只想给其中两个让开位置"
printf 'A\n' > app.js; printf 'M\n' > model.js; printf 'V\n' > view.js
qc "c1 三个文件"
printf 'A2\n' > app.js; printf 'M2\n' > model.js; printf 'V2\n' > view.js
printf '未跟踪的新配置文件\n' > extra.conf
R 'git status --short'

sub "5A -- 后面列路径：只收 app.js 和 model.js"
R "$G stash push -m '只收 app 和 model' -- app.js model.js"
R 'git status --short'
note "view.js 的改动和 extra.conf 都还在原地 —— 这就是「部分让位」。"
R 'git stash list'
R 'git stash show --stat'

sub "5B 路径 + -u：把某个未跟踪文件也一起带走"
R "$G stash push -u -m '把 extra.conf 和 view.js 带走' -- extra.conf view.js"
R 'git status --short'
R 'git stash list'
note "栈里现在有两条：stash@{0} 是刚这条，stash@{1} 是 5A 那条。编号从栈顶往下数，再 push 一条全都往后挪一格。"
R "git stash show --stat 'stash@{1}'"

sub "5C 先把两条都取回来、落一条提交，再演示按目录收"
R 'git stash pop >/dev/null'
R 'git stash pop >/dev/null'
R 'git status --short'
qc "c2 上面这些改动我先提交掉"
mkdir -p lib
printf 'L 基线\n' > lib/util.js
printf 'L 基线\n' > lib/io.js
qc "c3 新增 lib 目录"
printf 'L 改了 util\n' > lib/util.js
printf 'L 改了 io\n' > lib/io.js
printf 'view 又改了\n' > view.js
R 'git status --short'
R "$G stash push -m '整个 lib 目录先让开' -- lib/"
R 'git status --short'
R 'git stash list'

sub "5D --keep-index：暂存区留着，把工作区那部分收走"
R 'git stash pop >/dev/null'
printf 'A3 已 add 的版本\n' > app.js
q "$G add app.js"
printf 'A4 add 之后又改了\n' > app.js
R 'git status --short'
R "$G stash push --keep-index -m '只把工作区那部分收走'"
R 'git status --short'
R 'git stash list'
note "app.js 已暂存的那版（A3）留在原地（M 在第 1 格），工作区里 A4 那次修改进了 stash —— 常用于「提交前想再跑一遍测试」。"
note "★ 注意最后一屏只剩 M app.js：lib/ 和 view.js 那些没暂存的改动也被一并收走了。"
note "--keep-index 只是「暂存区那份别动」，不是「只 stash 这个文件」；只挑文件要用 -- <路径>。"

###############################################################################
say "第 6 节 完整流程：半成品在手，线上着火"
###############################################################################
newrepo s6-firefight "场景：你在 feature/pay 上改到一半，线上出 bug 必须马上切回 main 修"
w plan.md '项目计划
第2行：目标
'
w login.js '登录页
'
qc "c1 项目起步"
R "$G switch -q -c feature/pay"
printf '支付页\n' > pay.js
q "$G add pay.js"
w login.js '登录页
登录页顺手改了校验
'
printf '支付对账脚本（半成品）\n' > reconcile.sh
R 'git log --oneline --decorate --graph --all'
R 'git status --short'
note "三种状态混在一起：pay.js 已 add、login.js 改了没 add、reconcile.sh 未跟踪。"

sub "6A 第一步：全部收干净（用 -u，那个未跟踪脚本也得带走）"
R "$G stash push -u -m 'feature/pay 半成品：支付对账'"
R 'git status --short'
R 'git stash list'
note "工作区干净了才谈得上安全切分支。03 章的 reset --hard 也能清空，但它把未提交的改动真砸掉。"

sub "6B 第二步：切回 main，开 hotfix 修线上"
R 'git switch main'
R 'git status --short'
R "$G switch -q -c hotfix/order"
printf '线上紧急修复：超时从 30 秒改成 5 秒\n' > hotfix.txt
qc "hotfix: 修超时配置"
R "$G switch -q main"
printf '项目计划
第2行：目标
第3行：hotfix 之后的收尾
' > plan.md
qc "c2 main 收下这次修复"
R 'git log --oneline --decorate --graph --all'
note "★★ 这张图是本章最值钱的一屏：refs/stash 那一坨就是第 1 节那张 ASCII 图的实物版 —— "
note "一个 merge commit，下面挂着 index 和 untracked files 两个快照，根上连着 feature/pay 那条线。"

sub "6C 第三步：回到 feature/pay，把半成品取回来"
R "$G switch -q feature/pay"
R 'git stash list'
R 'git stash pop'
R 'git status --short'
note "改动回来了、stash 也空了，接着干。整条路上你没弄脏任何一条提交。"
note "顺带看见：pop 之后 pay.js 还保持在暂存区（A 在第 1 格），因为 stash 里 ^2 那份快照记得它 add 过。"

sub "6D 加分招：git stash branch —— 这个半成品其实该另开一条线做"
printf '要单独整理的改动\n' > refactor.md
R "$G stash push -u -m '这块要单开分支慢慢做' -- refactor.md"
R "git log -1 --format='HEAD 现在在：%h %s'"
R 'git stash list'
R "$G stash branch wip/refactor 'stash@{0}'"
R 'git branch'
R 'git status --short'
R 'git stash list'
note "git stash branch <新分支名> <stash>：用这条 stash 的基线（^1）建分支、切过去、apply、再 drop。"
note "存了很久、和现在冲突很大的 stash 就用它 —— 在它自己的起点上解决冲突，比在别的分支上硬 pop 舒服得多。"

###############################################################################
say "第 7 节 drop / clear 之后：用 git fsck --unreachable 把 stash 捞回来"
###############################################################################
newrepo s7-recover "现场：你手滑 git stash clear，一整条重要的 stash 从列表里消失了"
printf '基线\n' > core.txt
qc "c1 基线"
printf '写了整整一天的改动\n' > core.txt
printf '同一天的未跟踪笔记\n' > notes-day1.md
R "$G stash push -u -m '★★★ 一天的活儿，千万别丢'"
STASH_HASH=$(git rev-parse 'stash@{0}')
note "先记下栈顶 hash（正常开发时不用记，drop 那一行会打印给你）：$STASH_HASH"
R 'git stash list'

sub "7A 先看 drop 这个动作：它会打印 hash，那是你唯一的后悔药"
printf '第二条改动\n' > second.txt
R "$G stash push -u -m '第二条：稍后被误 drop'"
R 'git stash list'
R 'git stash drop'
note "上面 Dropped refs/stash@{0} (<完整 hash>) 括号里那一串 —— 请立刻复制，别让它划走。"

sub "7B 然后手滑：clear 把整栈全清了"
R 'git stash list'
R 'git stash clear'
R 'git stash list'
note "列表空了、工作区也干净了，看起来一天白干了。"
R 'git reflog show stash'
note "★ refs/stash 这根 ref 本身已经没了，所以连 stash 的 reflog 都查不到 —— 这条路断了，换 fsck。"

sub "7C 先试一手：拿 hash 直接 pop —— 不行，这条要记牢"
R "git stash pop $STASH_HASH"
note "error: '<hash>' is not a stash reference —— pop / apply 里的 stash@{n} 是【reflog 位置】，"
note "一个裸 hash 不算 stash 引用，pop 拒收。但 apply 收，看 7E。"

sub "7D 救援第一步：确认对象还在库里（Git 从没删过它，只是没人引用了）"
R "git cat-file -t $STASH_HASH"
R "git log -1 --format='%h %s' $STASH_HASH"
R "git show --stat $STASH_HASH"
R "git show --stat ${STASH_HASH}^3"
note "第三个父亲 ^3 是未跟踪文件那份快照 —— 和 1F 那张图完全对得上。"

sub "7E 救援第二步：git fsck --unreachable 把没人引用的对象全列出来"
R 'git fsck --unreachable'
note "输出很长（blob / tree / commit 三种），只盯 unreachable commit 那几行就行。"
R 'git fsck --unreachable | grep "unreachable commit" | cut -d" " -f3 | sort | while read h; do echo "$h | $(git log -1 --format=%s "$h")"; done'
note "看说明就能认出哪条是你的：On main: ★★★ 一天的活儿，千万别丢。"
R 'git fsck'
note "不带参数的 git fsck 只报 dangling commit（两条被丢弃的栈顶都在），够你认 hash；"
note "--unreachable 连 ^2 / ^3 这些中间快照一起列出来，看得更全。"

sub "7F 救援第三步：用 hash apply，改动整个回来"
R "git stash apply $STASH_HASH"
R 'git status --short'
R 'cat core.txt'
R 'cat notes-day1.md'
note "已跟踪文件的改动 + 未跟踪文件都回来了。7C 那个 pop 的错不是因为 hash 错了，而是 pop 只认 stash@{n}。"

sub "7G 顺手把找回来的东西重新挂回栈里（可选）"
R "$G stash push -u -m '找回来了，重新存一次'"
R 'git stash list'
note "为什么不用 git update-ref 硬挂回 refs/stash？因为 git stash list 读的是 .git/logs/refs/stash 那份流水账，"
note "手工 update-ref 只改 refs/stash，list 里看不见它。老老实实在在 push 一条新的最省心。"

sub "7H 边界说明（本章不真跑，03 章已经演示过）"
note "上面整套救援的前提是「对象还没被 gc 真删」。"
note "如果你已经跑过 git reflog expire --expire=now --all + git gc --prune=now，"
note "或者 stash 的保留期过了（gc.reflogExpire，默认 90 天），那就是真没了，神仙难救。"
note "★ 所以：stash 里有货的时候，永远不要顺手跑 gc。"

###############################################################################
say "第 8 节 .gitignore 的四层来源：仓库级、目录级、仓库私有 exclude、还有全局那份"
###############################################################################
note "本节讲到的『全局配置』只存在于沙箱假家目录（HOME 在脚本开头就被指到 $SB/fakehome）。"
note "脚本里没有、也不会有任何一条 git config --global；你真实的 ~/.gitconfig 一个字节都没动。"
newrepo s8-layers "现场：同一类文件，四份规则文件都能管它 —— 得知道是谁在起作用"
mkdir -p src
printf 'x\n' > root-build.log
printf 'x\n' > src/special.dat
printf 'x\n' > special.dat
printf 'x\n' > from-global.txt
printf 'x\n' > data.gz
printf 'x\n' > exclude-only.txt
printf 'x\n' > keep-me.log

sub "8A 第一层：仓库根 .gitignore（会被提交，全项目共享）"
w .gitignore '# 仓库根 .gitignore
*.log
!keep-me.log
'
R 'git check-ignore -v --no-index root-build.log'
R 'git check-ignore -v --no-index keep-me.log'
R 'git status --short'
note "命中来源写成 .gitignore:行号:规则 —— 以后不用再猜，直接问它。"
note "keep-me.log 命中的是 !keep-me.log（反向排除），所以它出现在 status 里、并没有被忽略。"

sub "8B 第二层：目录内的 .gitignore（只管它那棵子树）"
w src/.gitignore '# 只在 src/ 及子目录生效
special.dat
'
R 'git check-ignore -v --no-index src/special.dat'
R 'git check-ignore -v --no-index special.dat'
note "同一条 special.dat 规则，写进 src/.gitignore 就管不到仓库根那个 special.dat —— 作用域是那个目录以下。"

sub "8C 第三层：.git/info/exclude（只对这个仓库生效，而且提交不上去）"
R 'tail -3 .git/info/exclude'
printf 'exclude-only.txt\nkeep-me.log\n' >> .git/info/exclude
R 'git check-ignore -v --no-index exclude-only.txt'
R 'git check-ignore -v --no-index keep-me.log'
R 'git status --ignored --short'
note "exclude-only.txt 谁都没写它，只靠 .git/info/exclude:7 就被忽略了 —— 这是「我自己的破文件不想污染团队的 .gitignore」的正确去处。"
note "★ 注意第二条：exclude 里明明写了 keep-me.log，命中的却还是 .gitignore:3:!keep-me.log，status 里它依然是 ??（没被忽略）。"
note "  实测优先级：.gitignore 压过 .git/info/exclude。exclude 不是「管理员特权」，它只是排在 .gitignore 后面的一层。"

sub "8D 第四层：全局 core.excludesFile（你机器上所有仓库都生效）—— 只在沙箱里演示"
note "先在沙箱里手写两份纯文本：规则文件 + 指向它的假 .gitconfig（不是 git config 写的）"
w "$SB/fakehome/gitignore-global" '*.gz
from-global.txt
'
cat > "$SB/fakehome/.gitconfig" <<EOF
# 这是沙箱假家目录里的 .gitconfig，不是你的 ~/.gitconfig
[user]
	name = 学习者
	email = learn@example.com
[core]
	excludesFile = $SB/fakehome/gitignore-global
EOF
R 'ls -1A "$HOME"'
R 'git config --get core.excludesFile'
R 'git check-ignore -v --no-index data.gz'
R 'git check-ignore -v --no-index from-global.txt'
note "命中来源是沙箱里那份全局文件 —— 这就是「所有仓库都自动忽略 .DS_Store / IDE 目录」那类配置的实现方式。"

sub "8E 四层互相打架时谁赢（全部实测，别背口诀）"
R 'git status --ignored --short'
note "先立靶子：!! 那几行里 from-global.txt 和 data.gz 全靠沙箱那份全局规则才被忽略，仓库自己的文件里谁都没写它们。"
R 'git check-ignore -v --no-index from-global.txt'
printf '!from-global.txt\n' >> .git/info/exclude
R 'git check-ignore -v --no-index from-global.txt'
R 'git status --short'
note "exclude 里补一条 !from-global.txt → 命中的是 .git/info/exclude:9，文件从 !! 变回 ??。第 3 层压过第 4 层。"
printf '!*.gz\n' >> .gitignore
R 'tail -5 .gitignore'
R 'git check-ignore -v --no-index data.gz'
R 'git status --short'
note "仓库根 .gitignore 里补一条 !*.gz → 全局那条 *.gz 被压掉，data.gz 又变回未跟踪。第 1 层压过第 4 层。"
printf '!special.dat\n' >> .gitignore
R 'git check-ignore -v --no-index special.dat'
R 'git check-ignore -v --no-index src/special.dat'
note "最后一组：根 .gitignore 现在写着 !special.dat，仓库根那个 special.dat 被救回来了；"
note "但 src/special.dat 命中的还是 src/.gitignore:2:special.dat —— 越靠近这个文件的规则越优先，根上的反排除管不到子目录。"
R 'git status --ignored --short'
note "实测出来的优先级（高 → 低）：子目录 .gitignore > 仓库根 .gitignore > .git/info/exclude > 全局 core.excludesFile。"
note "★ check-ignore -v --no-index 打印的是「最后命中的那条规则」，规则带 ! 时它也会打印、退出码也是 0；到底忽略没忽略，看 9G 或者看 status。"

sub "8F 一眼看清每条配置来自哪个文件"
R 'git config --list --show-origin | grep -i excludesfile'
note "--show-origin 会把来源文件写在每行前面。这里的 HOME 已经是 $SB/fakehome，所以全是沙箱路径。"

###############################################################################
say "第 9 节 规则怎么写：/ ** ! 尾斜杠，每条都用 check-ignore -v 验一遍"
###############################################################################
newrepo s9-rules "现场：一份把常用写法全用上的 .gitignore"
mkdir -p build docs/a/b src/sub dist secret/public keys/public logs nested
printf 'x\n' > build/app.o
printf 'x\n' > docs/draft.md
printf 'x\n' > docs/a/b/draft.md
printf 'x\n' > src/sub/t.tmp
printf 'x\n' > src/t.tmp
printf 'x\n' > toplevel-only.txt
printf 'x\n' > src/toplevel-only.txt
printf 'x\n' > secret/key.txt
printf 'x\n' > secret/public/readme.txt
printf 'x\n' > keys/key.txt
printf 'x\n' > keys/public/readme.txt
printf 'x\n' > dist/inner.txt
printf 'x\n' > notes.md
printf 'x\n' > important.md
printf '我是个文件，不是目录\n' > cache
printf 'x\n' > logs/app.log
printf 'x\n' > nested/inner.txt
w .gitignore '### 本章的规则示范（下面逐条用 check-ignore 验）
*.o
build/
cache/
/toplevel-only.txt
*.md
!important.md
docs/**/draft.md
src/**/*.tmp
/dist/
logs/
nested/
secret/
!secret/public/
keys/*
!keys/public/
'
R "grep -n '' .gitignore"

sub "9A 尾斜杠只匹配目录；不带斜杠的 *.o 匹配任意层"
R 'git check-ignore -v --no-index build/app.o'
R 'git check-ignore -v --no-index cache'
note "规则里有 cache/（带尾斜杠），而 cache 是个【文件】→ 匹配不上。目录里的文件才归它管："
R 'git check-ignore -v --no-index logs/app.log'

sub "9B 开头的 / = 锚定到这份 .gitignore 所在目录，不往下递归"
R 'git check-ignore -v --no-index toplevel-only.txt'
R 'git check-ignore -v --no-index src/toplevel-only.txt'
note "第一条命中、第二条不命中：/toplevel-only.txt 只管仓库根那一个。"

sub "9C ** = 跨任意层目录（可以是 0 层）"
R 'git check-ignore -v --no-index docs/draft.md'
R 'git check-ignore -v --no-index docs/a/b/draft.md'
R 'git check-ignore -v --no-index src/t.tmp'
R 'git check-ignore -v --no-index src/sub/t.tmp'
note "docs/draft.md 也归 docs/**/draft.md 管 —— ** 可以是 0 层目录。"

sub "9D 一条规则忽略了一个目录之后，Git 根本不会走进去"
R 'git check-ignore -v --no-index dist'
R 'git check-ignore -v --no-index dist/inner.txt'
note "★ /dist/ 命中目录本身，里面的文件跟着一起没了 —— 这也是 9F 那个坑的根源。"

sub "9E ! 反向排除：同一份文件里后面的规则赢"
R 'git check-ignore -v --no-index notes.md'
R 'git check-ignore -v --no-index important.md'
R 'git status --short'
note "important.md 命中的是 !important.md（写在 *.md 后面）→ 它没被忽略，出现在 status 里。同一份文件里，后面的规则赢。"

sub "9F ★★ 最著名的坑：父目录被整个忽略时，子目录反排不回来"
R 'git check-ignore -v --no-index secret/key.txt'
R 'git check-ignore -v --no-index secret/public/readme.txt'
note "secret/ 整个被忽略 → !secret/public/ 这条【完全没用】，因为 Git 不会进 secret/ 里面去看。"
note "同一份 .gitignore 里紧跟着写了正确版本 keys/* + !keys/public/，对比看："
R 'git check-ignore -v --no-index keys/key.txt'
R 'git check-ignore -v --no-index keys/public/readme.txt'
R 'git status --short'
note "keys/public/readme.txt 退出码 1 = 没被忽略，真的救回来了。规则只要一行：忽略目录【内容】(dir/*) 而不是目录本身 (dir/)。"
note "这一条能省掉无数人半小时。"

sub "9G check-ignore 的退出码语义（很容易读错）"
R 'git check-ignore -v --no-index important.md'
R 'git check-ignore --no-index important.md'
R 'git check-ignore -v --no-index Makefile'
note "带 -v 时退出码 0 = 「有一条规则命中了」，命中的是 ! 规则时其实【没被忽略】；"
note "不带 -v 时退出码 0 = 「真的被忽略」，1 = 没被忽略。想知道到底忽略没忽略，用不带 -v 的那条。"
note "--no-index 的意思是「已跟踪的文件也按规则判」；不加它时 check-ignore 会直接跳过已跟踪文件（第 10 节就是这个坑）。"

###############################################################################
say "第 10 节 已提交的文件写进 .gitignore 为什么没用 —— 正解是 git rm --cached"
###############################################################################
newrepo s10-tracked "现场：手滑把带 token 的本地配置提交了，现在补一条 .gitignore 想让它消失"
printf '{"token":"这个不该进仓库"}\n' > config.local.json
printf 'app\n' > app.py
qc "c1 手滑把本地配置也提交了"

sub "10A 现在才想起来加 .gitignore —— 什么也没发生"
w .gitignore 'config.local.json
*.log
'
R 'git status --short'
note "列表里没有 config.local.json 那一行 —— 因为它【已经在仓库里了】。gitignore 只管未跟踪的文件。"
printf '{"token":"换了一个 token"}\n' > config.local.json
R 'git status --short'
note "改它照样被报出来。这就是「.gitignore 对已跟踪文件无效」的现场。"

sub "10B check-ignore 在这里会骗你：不带 --no-index 时它压根不看这条规则"
R 'git check-ignore -v config.local.json'
R 'git check-ignore -v --no-index config.local.json'
note "不带 --no-index 退出码 1、什么都不打印。不是规则写错了，而是 Git 认为「这文件我在跟踪，轮不到忽略规则」。"

sub "10C 正解：git rm --cached <文件> —— 从索引里摘掉，磁盘上的文件原样留着"
R 'git rm --cached config.local.json'
R 'git status --short'
R 'ls -1 config.local.json'
R 'cat config.local.json'
note "第一格 D = 已暂存的「从仓库里删除」；磁盘上那个文件还在，内容也还在。"
note "为什么没多出 ?? config.local.json？因为它已经命中 .gitignore 了 —— 未跟踪 + 被忽略 = 在 status 里隐身。"
R 'git check-ignore -v config.local.json'
note "现在不带 --no-index 也能命中了 —— 说明它已经变成未跟踪文件，归 .gitignore 管了。"

sub "10D 提交这次「停止跟踪」，之后就彻底清净"
R "$G add -A"
R "$G commit -q -m 'c2 把本地配置移出仓库跟踪（文件留在本地）'"
R 'git status --short'
printf '{"token":"第三次换 token"}\n' > config.local.json
R 'git status --short'
note "工作区里随便改，它都不再出现在 status 里 —— 这才是大家以为 .gitignore 一开始就该有的效果。"

sub "10E ★★ 但是：仓库历史里那一份还在"
R 'git log --oneline'
R 'git show --stat --oneline HEAD~1'
R "$G show HEAD~1:config.local.json"
note "git rm --cached 只是「从现在起不再跟踪」，之前那条提交里的内容一个字都没少。"
note "真泄露了 token：立刻吊销换新；要清历史得用 filter-repo（10 章）。别指望 .gitignore。"

sub "10F 顺带看一个现场：被忽略的文件也能强行提交进去"
printf 'x\n' > new-ignored.log
R 'git add new-ignored.log'
R 'git add -f new-ignored.log'
R "$G commit -q -m 'c3 明知被忽略，还是用 -f 提交了'"
R 'git status --short'
note "团队里出现「我这儿没报，你那儿报了」，八成就是有人这么干过。"

###############################################################################
say "第 11 节 看清被忽略的东西 + git clean 只报名单（本章不真删）"
###############################################################################
cd "$SB/s10-tracked"
sub "11A git status --ignored：把「隐身」的那批也拉出来"
R 'git status --short'
R 'git status --ignored --short'
R 'git status --ignored'
note "--short 里 !! 前缀就是被忽略的；长格式在 Ignored files 那一段，还顺手提示你可以 git add -f。"
note "排查「我的文件怎么不见了」时，git status --ignored 应该是第一个敲的命令。"

sub "11B 造几个未跟踪文件"
printf '试过的脚本 1\n' > try1.sh
printf '试过的脚本 2\n' > try2.sh
mkdir -p tmpwork; printf 'x\n' > tmpwork/junk
R 'git status --short'

sub "11C git clean -nd：只报名单，什么都不删（★ 永远先跑这条）"
R 'git clean -nd'
R 'git status --short'
note "输出是 Would remove ...，文件全都还在（下面这条 ls 就是证明）："
R 'ls -1 | tr "\n" " "'

sub "11D -n 不加 d：只列文件不列目录，名单会短一截，容易误判"
R 'git clean -n'
note "对比 11C：tmpwork/ 不见了 —— 少了 d 就看不见整个目录，最容易在这里低估名单长度。"

sub "11E -nxd：连被 .gitignore 忽略的一起进名单（这才是核弹预览）"
R 'git clean -nxd'
note "★ 名单里多出了 config.local.json —— 那就是你唯一一份的本地配置文件。这正是 -x 最狠的地方。"
note "这份名单和真删时的名单一字不差，所以：先把 -n 在脑子里换成 -f 读一遍，再决定敲不敲。"

sub "11F 本章【不执行】git clean -fd / -fdx"
note "真删的现场和后果 03 章已经完整演示过：被 clean 删掉的未跟踪文件 Git 从没见过，reflog 也救不回来。"
note "本章只强调一件事 —— clean 的 dry-run 必须先跑。所以这个脚本跑到现在，没删掉任何一个文件。"

echo
say "本章记住五句话"
echo "  1) stash 不是剪贴板：push 造的是真 commit（^1 基线 / ^2 暂存区 / ^3 未跟踪），refs/stash 只是栈顶指针。"
echo "  2) 拿不准就 apply，确认了再 pop；pop 失败时 Git 会把 stash 留着（The stash entry is kept...），不吞你的东西。"
echo "  3) 误 drop / clear：git fsck --unreachable 找 hash，git stash apply <hash> 全数复活 —— 前提是你没跑 gc。"
echo "  4) .gitignore 有四层来源（仓库根 / 目录内 / .git/info/exclude / 全局 excludesFile），"
echo "     谁在起作用用 git check-ignore -v 问，别猜；反排除子目录要写 dir/* + !dir/sub/，而不是整个忽略 dir/。"
echo "  5) .gitignore 管不了已跟踪文件：要停止跟踪用 git rm --cached，历史里那一份得单独处理。"
echo
echo "全部演示完成。"
