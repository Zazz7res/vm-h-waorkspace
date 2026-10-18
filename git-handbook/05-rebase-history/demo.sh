#!/usr/bin/env bash
# ============================================================================
# 第 05 章《改写历史：rebase / cherry-pick / amend / reflog》—— 整章证据一键复现
#
#   bash demo.sh          跑完自动删沙箱（默认）
#   bash demo.sh --keep   跑完保留沙箱并打印路径，方便进去自己接着敲
#
# 这一章一键复现什么：
#   A 先看形状再动手：git log --graph + merge-base + main..topic
#   B 为什么 rebase 之后哈希全变了（打开 commit 对象逐行对照 + patch-id）
#   C 变基冲突的三种结局：--continue / --skip / --abort（外加 --quit 那个坑）
#   D git rebase -i：本机没有图形编辑器，全靠 GIT_SEQUENCE_EDITOR 非交互跑完
#   E git rebase --onto：摘掉中间一段 / 给分支换个底本
#   F git cherry-pick（含 -x / -n / 范围 / 合并提交 / 冲突三件套）
#   G git commit --amend，以及「改写已发布历史」被拒的真实现场
#   H ★ 被 rebase 弄丢的提交怎么捞回来（reflog / ORIG_HEAD，本章最值钱的一节）
#
# 安全说明（请务必先读完这 10 行再按下回车）：
#   * 所有仓库都建在 mktemp -d 出来的临时沙箱里。本脚本不碰你机器上任何已有仓库，
#     不碰你的工作目录，也不碰你的家目录。
#   * 不访问网络。需要「服务器上的仓库」时在沙箱里 git init --bare 冒充 origin；
#     本章出现的所有 push 都只推向那个沙箱 bare 仓库，只在本地文件系统里写。
#   * 绝不写全局配置：GIT_CONFIG_GLOBAL / GIT_CONFIG_SYSTEM 指到 /dev/null
#     （只是本进程不去读，不改你的任何文件），提交身份用 GIT_AUTHOR_* / GIT_COMMITTER_*
#     环境变量显式给成「学习者 <learn@example.com>」。
#   * ★ 本章要演示「手滑丢提交」这类事故，所以故意【不加 set -e】：
#     失败现场（冲突、被拒的 push、报错的 cherry-pick）本身就是教材的一部分。
#     每条命令后面都会打 [退出码 N]，非 0 的那些是专门给你看的。
#   * 结尾自动 rm -rf 沙箱（除非你加 --keep）。
#   * 时间戳逐条递增并固定，所以同样的命令序列必然得到同样的哈希（已连跑两次核对）。
#   * 只在输出打到终端时上色（[ -t 1 ] 判断）；重定向到文件就是纯文本，方便和 README 对照。
#   * 唯一一处「整理」：git 进度行结尾的原地回车 \r 被换成真换行（内容一字未改，退出码是 git 的），
#     这样输出重定向到文件后不会出现两行连在一起。除此之外全是原样打印。
# ============================================================================

KEEP=0
for arg in "$@"; do
  case "$arg" in
    --keep) KEEP=1 ;;
    -h|--help) sed -n '1,34p' "$0"; exit 0 ;;
    *) echo "未知参数：$arg（本脚本只认 --keep）" >&2; exit 2 ;;
  esac
done

SB=$(mktemp -d "${TMPDIR:-/tmp}/git-handbook-05-rebase-XXXXXXXX")
cleanup() {
  local rc=$?
  echo
  if [ "$KEEP" = 1 ]; then
    echo "沙箱已保留，可以进去继续敲：cd $SB"
    echo "各场景目录：$(ls -1 "$SB" | tr '\n' ' ')"
    echo "看完记得删：rm -rf $SB"
  else
    rm -rf "$SB"
    echo "沙箱已清理：$SB"
  fi
  exit "$rc"
}
trap cleanup EXIT

# ---- 运行环境：可控、可复现、不读不写任何真实配置 --------------------------
export LC_ALL=C.UTF-8 LANG=C.UTF-8                 # 固定英文输出，方便和 README 逐行对照
export GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_SYSTEM=/dev/null
export GIT_AUTHOR_NAME='学习者' GIT_AUTHOR_EMAIL='learn@example.com'
export GIT_COMMITTER_NAME='学习者' GIT_COMMITTER_EMAIL='learn@example.com'
export GIT_PAGER=cat PAGER=cat GIT_EDITOR=true     # 关键：没有任何图形编辑器也能跑完整章
export GIT_MERGE_AUTOEDIT=no
unset GIT_DIR GIT_WORK_TREE GIT_INDEX_FILE 2>/dev/null
export SB_TODO_COPY="$SB/todo-seen-by-editor.txt"  # 交互式清单的副本落在这

# ---- 上色：只在输出到终端时上色，重定向到文件就是纯文本 --------------------
if [ -t 1 ]; then
  BLD=$'\033[1m'; YEL=$'\033[33m'; GRN=$'\033[32m'; DIM=$'\033[2m'; OFF=$'\033[0m'
else
  BLD=''; YEL=''; GRN=''; DIM=''; OFF=''
fi

say()  { printf '\n════════════════════════════════════════════════════════════════\n  %s\n════════════════════════════════════════════════════════════════\n' "$BLD$*$OFF"; }
sub()  { printf '\n  ---- %s ----\n' "$BLD$*$OFF"; }
note() { printf '  ↳ %s\n' "$GRN$*$OFF"; }
hint() { printf '  ·  %s\n' "$DIM$*$OFF"; }

# R <命令字符串>：本章唯一的「看得见」执行器
# 打印 $ 命令 → 原样打印 stdout+stderr → 打印 [退出码 N]（0 也打，好和 README 对齐）
R() {
  local cmd="$*" out rc
  printf '\n%s$ %s%s\n' "$YEL" "$cmd" "$OFF"
  out=$(bash -c "$cmd" 2>&1); rc=$?
  # git 的进度行（Rebasing (2/5) 这类）结尾是一个原地回车 \r：终端里它会覆盖刷新。
  # 这里只做这一件事 —— 把 \r 变成真换行，内容一字不改、退出码取的是 git 自己的。
  out=${out//$'\r'/$'\n'}
  [ -n "$out" ] && printf '%s\n' "$out"
  printf '%s[退出码 %s]%s\n' "$DIM" "$rc" "$OFF"
  return 0
}
Q() { bash -c "$*" >/dev/null 2>&1; return 0; }    # 搭场景用的静默执行器

# 时间戳：每造一个提交、每做一次改写都往前挪一分钟，哈希就可复现
SEQ=0
tick() {
  SEQ=$((SEQ + 1))
  local total=$((600 + SEQ)) hh mm ts
  hh=$((total / 60)); mm=$((total % 60))
  printf -v ts '2026-01-01T%02d:%02d:00 +0800' "$hh" "$mm"
  export GIT_AUTHOR_DATE="$ts" GIT_COMMITTER_DATE="$ts"
}
qc() { tick; Q git add -A; Q "git commit -q -m \"$1\""; }   # 静默提交（自动带上时间戳）
w()  { printf '%s' "$2" > "$1"; }                       # w <文件> <内容>
newrepo() {                                             # 在沙箱里建一个干净仓库并切进去
  local dir="$SB/$1"
  rm -rf "$dir"; mkdir -p "$dir"
  Q git init -q -b main "$dir"
  cd "$dir" || exit 1
  note "新建沙箱仓库 $1/（身份：学习者 <learn@example.com>，不读你本机配置）"
}

# 交互式 rebase 的「假编辑器」：全部非交互，脚本本身写在沙箱里
cat > "$SB/seq-show.sh" <<'EOS'
#!/usr/bin/env bash
# 冒充 GIT_SEQUENCE_EDITOR：一个字都不改，只把清单原样打出来给你看，并留一份副本
printf '\n>>> 下面是 git rebase -i 打开的那个「编辑器」实际收到的文件内容（逐字）：\n'
cat "$1"
cp "$1" "$SB_TODO_COPY"
EOS
cat > "$SB/seq-move-last-to-first.sh" <<'EOS'
#!/usr/bin/env bash
# 冒充 GIT_SEQUENCE_EDITOR：把清单里最后一条 pick 挪到第一行（= 改变重放顺序）
f="$1"
mapfile -t L < <(grep '^pick' "$f")
n=${#L[@]}
{ printf '%s\n' "${L[n-1]}"
  for ((i = 0; i < n - 1; i++)); do printf '%s\n' "${L[i]}"; done
} > "$f"
printf '\n>>> 我已经把清单最后一条挪到了第一行（其余顺序不变），git 会照新顺序重放\n'
EOS
chmod +x "$SB/seq-show.sh" "$SB/seq-move-last-to-first.sh"

# sed 版的一条龙编辑器：只改第 N 行的命令字，不打开任何界面
SEQ_FIXUP2='sed -i "2s/^pick/fixup/"'
SEQ_SQUASH5='sed -i "5s/^pick/squash/"'
SEQ_REWORD1='sed -i "1s/^pick/reword/"'
SEQ_EDIT1='sed -i "1s/^pick/edit/"'
SEQ_DROP4='sed -i "4s/^pick/drop/"'
MSG_REWORD='sed -i "1s|.*|第1条：加登录骨架（reword 重写过的说明）|"'

printf '%s第 05 章 demo：rebase / cherry-pick / amend / reflog%s\n' "$BLD" "$OFF"
printf '沙箱位置：%s\n' "$SB"
hint "说明：git 的进度行（Rebasing (2/5) 这类）原文用的是原地回车 \\r，重定向到文件会两行连在一起。本脚本把 \\r 换成真换行，内容一字未改，退出码就是 git 自己的退出码。"

# ============================================================================
say "A 先看清分叉的形状，再决定要不要 rebase"
# ============================================================================
newrepo a-shape
w notes.txt '第1行：项目说明
第2行：目标
第3行：分工
'
qc "c1 共同的起点"
Q git switch -q -c topic
w api.js 'api()
'
qc "t1：加 api.js（main 完全没碰这个文件）"
w notes.txt '第1行：项目说明
第2行：目标（topic 改过）
第3行：分工
'
qc "t2：改 notes.txt 第2行"
Q git switch -q main
w README.md '本手册示例仓库
'
qc "m1：main 加了 README.md"

sub "A1 先问一句：现在到底是什么形状？（只看 git branch 是看不出来的）"
R 'git log --oneline --graph --decorate --all'
note "两条线从 c1 分叉，各自走了几步 —— 这种形状就是「得决定怎么合」的形状。"
note "04 章的 merge 是【造一个双亲提交，把两条线都留在历史里】。"
note "本章的 rebase 是另一种：把你这条线整条搬到别人头上，历史变成一条直线。"

sub "A2 rebase 之前必须先算清楚的三件事"
R 'git merge-base main topic'
R "git log --oneline -1 \$(git merge-base main topic)"
note "共同祖先（分叉点）：rebase 就是从这里把你这条线上的提交摘下来。"
R 'git log --oneline main..topic'
note "★ main..topic = 「topic 有、main 没有」= 这次会被搬走的提交。写 rebase 之前一定先看它。"
R 'git log --oneline topic..main'
note "topic..main = 对方独有的提交，rebase 不会搬它们（它们会变成你的新底本）。"
R "echo -n '我领先几个：' && git rev-list --count topic ^main && echo -n '我落后几个：' && git rev-list --count main ^topic"

sub "A3 记下 rebase 之前的哈希（这一步是为了待会儿做对照）"
R 'git log --format="%h %p %s" main..topic'
T1_OLD=$(git rev-parse topic~1); T2_OLD=$(git rev-parse topic)
MAIN_BEFORE=$(git rev-parse --short main); TOPIC_BEFORE=$(git rev-parse --short topic)
hint "topic 现在 = $TOPIC_BEFORE，main 现在 = $MAIN_BEFORE"

sub "A4 ★ 站在哪条分支上很关键：rebase 搬的是【你脚下这条】"
R 'git switch -q main && git rev-parse --abbrev-ref HEAD'
R 'git rebase main'
note "↑ 站在 main 上执行 git rebase main，git 只回一句 up to date —— 它搬的是当前分支，"
note "  而当前分支正是那个底本，自己搬自己当然什么都没动。这不是报错，是最容易犯的方向性错误。"
R 'git switch -q topic && git rev-parse --abbrev-ref HEAD'

sub "A5 现在才是真的 rebase：把 topic 搬到 main 上面"
tick
R 'git rebase main'
R 'git log --oneline --graph --decorate --all'

sub "A6 对照：rebase 前后的 git log --format='%h %p %s'（哈希和 parent 全变了）"
R 'git log --format="%h %p %s" main..topic'
note "搬之前是 A3 那两条（parent 指向 topic 内部/起点），搬之后 parent 链一路挂到 m1。"
note "说明文字一模一样，%h 全变了 —— 因为它们【不是同一批提交对象】，是重新造出来的副本。"
R 'git rev-parse --short topic'
hint "分支指针从 $TOPIC_BEFORE 挪到了这里；现在 topic..main 是空的："
R 'git log --oneline topic..main && echo "(空 = 你已经包含 main 的全部内容，main 那边可以快进到你)"'
R 'git status -sb | head -3'

# ============================================================================
say "B 为什么哈希全变了？打开提交对象一眼就明白了"
# ============================================================================
sub "B1 一条提交对象里到底存了哪几样东西"
T1_NEW=$(git rev-parse topic~1)
R "git cat-file -p $T1_OLD"
note "这就是 a-shape 里那条 t1 搬家【之前】的对象原文：tree / parent / author / committer / 说明。"
R "git cat-file -p $T1_NEW"
note "搬家【之后】的同一个改动，逐行对比："

sub "B2 逐行对照：什么变了，什么没变"
R "echo \"parent     旧=\$(git rev-parse --short $T1_OLD^)  新=\$(git rev-parse --short $T1_NEW^)\""
R "echo \"author     旧=\$(git log -1 --format=%ad $T1_OLD)\""
R "echo \"author     新=\$(git log -1 --format=%ad $T1_NEW)\""
R "echo \"committer  旧=\$(git log -1 --format=%cd $T1_OLD)\""
R "echo \"committer  新=\$(git log -1 --format=%cd $T1_NEW)\""
R "git rev-parse $T1_OLD^{tree} $T1_NEW^{tree}"
note "① parent 换了（这就是 rebase 的目的）② committer 时间换了（重新造对象必然刷新）"
note "③ tree 也换了 —— 别忘了一条提交是【整个目录的全量快照】，不是补丁："
note "   新底本 main 多了一个 README.md，所以这条提交的快照里也必然多一个 README.md。"
note "这三样里任何一样变，哈希必然变。author 和作者时间倒是原样保留的（对比上面两组）。"

sub "B3 ★ tree 不一样 ≠ 补丁不一样：两条提交的 patch-id 完全相同"
R "git diff --stat $T1_OLD $T1_NEW | cat"
note "↑ 这两个提交对象之间的差异，只有底本多出来的那个 README.md（不是这条提交自己改了东西）。"
R "git show $T1_OLD | git patch-id --stable"
R "git show $T1_NEW | git patch-id --stable"
note "patch-id = 「只看加了哪些行、删了哪些行，忽略行号和上下文」算出来的哈希。两个一样 = 同一份补丁。"
note "git 正是靠它判断「这份补丁上游已经有了」（见 C7），也是靠它认出重放前后是同一条提交。"

sub "B4 一句话总结：commit 的哈希是对【tree + parent + author/committer + 说明】整体算的"
R "printf 'tree %s\nparent %s\nauthor 学习者 <learn@example.com> 1767232920 +0800\ncommitter 学习者 <learn@example.com> 1767232920 +0800\n\n完全一样的说明\n' \"\$(git rev-parse main^{tree})\" \"\$(git rev-parse main)\" | git hash-object -t commit --stdin"
R "printf 'tree %s\nparent %s\nauthor 学习者 <learn@example.com> 1767232920 +0800\ncommitter 学习者 <learn@example.com> 1767232920 +0800\n\n完全一样的说明\n' \"\$(git rev-parse main^{tree})\" \"\$(git rev-parse main^)\" | git hash-object -t commit --stdin"
note "一样的 tree、一样的说明，只把 parent 那一行挪一格 → 算出来的哈希就完全不同。"
note "内容寻址仓库没有「就地修改」这回事，所以 rebase 的本质不是「移动提交」，"
note "而是【照着旧提交重做一批新提交，然后把分支指针挪过去】。旧对象并没被删除，还躺在 .git/objects 里。"
hint "记住这句话，H 节救命用的就是它。"

sub "B5 rebase 的痕迹全在 reflog 里（先混个脸熟，H 节靠它救人）"
R 'git reflog -8'

# ============================================================================
say "C rebase 冲突：--continue / --skip / --abort 三种结局各跑一遍"
# ============================================================================
# 现场：topic 三条提交，前两条和 main 改同一行，第三条干净
mkconflict() {
  newrepo "$1"
  w shared.txt '第1行：起点
第2行：占位
'
  qc "c1 共同的起点"
  Q git switch -q -c topic
  w shared.txt '第1行：起点
第2行：TOPIC 改的
'
  qc "t1：第2行改成 TOPIC"
  w shared.txt '第1行：起点
第2行：TOPIC 又改了一次
'
  qc "t2：第2行再改一次"
  w logo.png 'LOGO
'
  qc "t3：加 logo.png（main 没碰过，肯定不冲突）"
  Q git switch -q main
  w shared.txt '第1行：起点
第2行：MAIN 改的
'
  qc "m1：第2行改成 MAIN"
  Q git switch -q topic
}

sub "C1 造一个【必然冲突】的现场，然后执行 rebase"
mkconflict c-conflict
R 'git log --oneline --graph --decorate --all'
tick
R 'git rebase main'
note "退出码 1，报错的是 git，但它同时把【三条出路】原封不动写在 hint 里："
note "  ① 解决冲突 → git add → git rebase --continue"
note "  ② 这条不要了 → git rebase --skip"
note "  ③ 整个 rebase 作废 → git rebase --abort"
hint "注意报错行写的是 could not apply <你那条老提交>，不是新提交 —— 它报的是「正在重放的原件」。"

sub "C2 冲突现场读三眼：status、文件本体、暂存区里的两个版本"
R 'git status --short'
R 'cat shared.txt'
note "★ 和 merge 正好相反：<<<<<<< HEAD 那半是【你落到的底本 main】，>>>>>>> 那半才是【你自己那条提交】。"
note "  因为 rebase 是把你的改动当成「外来户」往 main 上重放（04 章 D 节讲透了这个坑）。"
R 'git ls-files -u'
R 'echo "== :2: = 底本（onto 那一侧，也就是 main）=="; git cat-file -p ":2:shared.txt"'
R 'echo "== :3: = 正在被重放的那条提交（你的 topic）=="; git cat-file -p ":3:shared.txt"'

sub "C3 结局一：手工解决 → git add → git rebase --continue"
R 'git rebase --continue'
note "↑ 还没 add 就急着 continue，git 直接拒绝（退出码 1），而且【什么都没改变】。"
w shared.txt '第1行：起点
第2行：手工合并后的最终版
'
R 'cat shared.txt'
R 'git add shared.txt'
R 'git status --short'
note "UU 变成 M —— add 在这里的语义是「这个文件的冲突我拍板了」，不是「暂存」。"
tick
R 'git rebase --continue'
note "↑ continue 之后 git 把 t1 重放成新提交，接着往下走，走到 t2 又撞了同一行。"
hint "这就是 rebase 冲突常见的样子：同一段代码可能让你解好几遍（每条被重放的提交各解一次）。"

sub "C4 结局二：这条提交我不要了 → git rebase --skip"
R 'git log --format="%h %s" -3'
tick
R 'git rebase --skip'
R 'git log --format="%h %s" -4'
note "↑ t2 被整条丢掉了，t3 顺利重放，rebase 完成。--skip 不是「跳过冲突回头再补」，是【这条不要了】。"
R 'git rev-parse --short ORIG_HEAD'
hint "ORIG_HEAD = rebase 开始前 topic 指的位置，是本节的后悔药（详见 H 节）。"
R 'git branch -v | cat'

sub "C5 结局三：整个 rebase 作废 → git rebase --abort（同场重演一次）"
mkconflict c-abort
R 'git log --format="%h %s" main..topic'
BEFORE=$(git rev-parse topic)
tick
R 'git rebase main'
R 'cat shared.txt'
tick
R 'git rebase --abort'
R 'git rev-parse --short topic'
hint "topic 搬家前 = $BEFORE，现在 = $(git rev-parse --short topic)，一模一样。"
R 'git log --format="%h %s" main..topic'
R 'cat shared.txt'
note "文件里的冲突标记没了、提交链恢复、指针回原位 —— abort 是【完全复原】，不是「尽量还原」。"
note "拿不准就先 abort，回头把 main..topic 看清楚再重来一次，成本极低。"

sub "C6 中途想退出但想【保留现在的进度】：--quit（和 --abort 的差别就这一句）"
mkconflict c-quit
tick
R 'git rebase main'
tick
R 'git rebase --quit'
R 'git status -sb | head -2'
R 'ls .git/rebase-merge'
R 'cat shared.txt'
note "★ --abort = 回到 rebase 之前；--quit = 只把状态目录删掉，HEAD 留在半路上（游离指针）+ 冲突标记留在文件里。"
note "  它的用途是「我要用别的办法收尾」（自己 reset、或者换个工具继续），日常手滑用了它就只剩懵。"
R 'git rebase --abort'
hint "（已经游离了，--abort 当场失效 —— 这就是上面那句「只剩懵」的现场版）"

sub "C7 常见现象：上游已经有了内容相同的补丁，rebase 会【自动跳过】并警告"
mkdup() {                                   # 造一份「side 和 main 加了内容完全一样的 h.txt」的仓库
  newrepo "$1"
  w g.txt 'g
'
  qc "c1 起点"
  Q git switch -q -c side
  printf 'h\n' > h.txt
  Q git add -A; qc "side1：加 h.txt"
  Q git switch -q main
  printf 'h\n' > h.txt
  Q git add -A; qc "m1：我自己也加了一份内容完全一样的 h.txt"
  Q git switch -q side
}
mkdup c-dup
R 'git log --oneline --graph --decorate --all'
R 'git cherry -v main side'
note "↑ git cherry 提前预告了结果：行首是【减号】= 这份补丁上游已经有了（待会儿会被跳过）；"
note "  行首是加号 = 会被真正重放。它比的正是 B3 那个 patch-id。"
tick
R 'git rebase main'
R 'git log --oneline --format="%h %s" --all'
note "↑ 这就是那句 warning: skipped previously applied commit —— 重复补丁被丢掉，side 直接落到 main 上。"
note "  pull --rebase 之后「我那条提交怎么不见了」，八成就是这个原因，不是丢了。"

sub "C8 想硬留着那条重复提交怎么办：光加 --reapply-cherry-picks 还不够"
mkdup c-dup-keep
tick
R 'git rebase --reapply-cherry-picks main'
note "↑ hint 里教的那个开关只解决【不再预警跳过】；补丁落到新底本上变成空改动，照样被 dropping 掉。"
tick
R 'git reset -q --hard ORIG_HEAD && git log --format="%h %s" -2'
note "↑ 先用 ORIG_HEAD 把刚才那次 rebase 撤回去（㉓ 专门讲这颗药），好让下一步单独演示第二个开关。"
tick
R 'git rebase --reapply-cherry-picks --empty=keep main'
R 'git log --oneline --format="%h %s" --all'
R 'git show --stat --format="%h %s" HEAD | cat'
note "★ 两个开关一起用才留得下来，代价是历史里多一条【什么都不改】的空提交。"
note "  日常更该做的是：先 git cherry -v 看清重复，让 rebase 把它跳掉，别硬留。"

# ============================================================================
say "D git rebase -i：本机没有图形编辑器，全靠 GIT_SEQUENCE_EDITOR 非交互跑完"
# ============================================================================
note "前提说清楚：git rebase -i 会打开两个「编辑器」——"
note "  GIT_SEQUENCE_EDITOR 改的是【那份 pick 清单】（决定每条提交怎么处理、什么顺序）；"
note "  GIT_EDITOR          改的是【提交说明】（只在 squash / reword / fixup -c 时才用）。"
note "本脚本把 GIT_EDITOR 设成 true（等于「直接接受，不改」），清单则用 sed 或小脚本改，"
note "所以全程不需要任何图形界面，也不会在中途卡住等你打字。"

mkint() {
  newrepo "$1"
  w core.txt 'core
'
  qc "c1 起点"
  Q git switch -q -c topic
  w login.js 'function login() {}
'
  qc "第1条：加登录骨架"
  printf '// 笔误已改\n' >> login.js
  Q git add -A; qc "第2条：改一处笔误"
  w form.js 'function checkForm() {}
'
  qc "第3条：加表单校验"
  w log.js 'function log() {}
'
  qc "第4条：加日志"
  w cfg.txt 'timeout=30
'
  qc "第5条：加配置项"
  R 'git log --format="%h %s" main..topic'
  note "↑ 注意：git log 从新到旧，而 rebase -i 的清单从旧到新，两者顺序【相反】。"
}

sub "D1 先把清单原样打出来看看（这是理解 -i 的唯一捷径）"
mkint d-todo
R "GIT_SEQUENCE_EDITOR='$SB/seq-show.sh' git rebase -i main"
R "git log --format='%h %s' main..topic"
note "清单第一条是最早的那个提交；下面那一大段注释块就是全部可用命令的官方说明。"
hint "其中两句是本章最该读的注释：These lines can be re-ordered（可以重排）、"
hint "以及 If you remove a line here THAT COMMIT WILL BE LOST.（删一行 = 丢一条提交）"

sub "D2 fixup：把第2条「改笔误」并进第1条，只保留第1条的说明"
mkint d-fixup
R "GIT_SEQUENCE_EDITOR='$SEQ_FIXUP2' git rebase -i main"
R 'git log --format="%h %s" main..topic'
note "↑ 第2条整个消失了，它的改动被并进第1条；说明还是「第1条：加登录骨架」，一点额外痕迹都不留。"
R 'git show --stat --format="%h %s" HEAD~3 | cat'

sub "D3 squash：把第5条并进第4条，两条说明【拼在一起】保留"
mkint d-squash
R "GIT_SEQUENCE_EDITOR='$SEQ_SQUASH5' git rebase -i main"
R 'git log --format="%h %s" main..topic'
R 'git show -s --format=%B HEAD'
note "↑ 这就是 squash 和 fixup 的唯一区别：squash 把两条说明合成一份（这里 GIT_EDITOR=true，"
note "  用的就是 git 预先写好的那份合并稿），fixup 只要前一条的说明。"

sub "D4 reword：只改说明，不动内容"
mkint d-reword
R "GIT_SEQUENCE_EDITOR='$SEQ_REWORD1' GIT_EDITOR='$MSG_REWORD' git rebase -i main"
R 'git log --format="%h %s" main..topic'
note "一条命令里两个编辑器各管各的：序列编辑器把第 1 行的 pick 换成 reword，"
note "正文编辑器（sed）把说明文件的第一行替换掉。内容一行没动，说明换了。"

sub "D5 重排顺序：把清单最后一条挪到最前面"
mkint d-reorder
R "GIT_SEQUENCE_EDITOR='$SB/seq-move-last-to-first.sh' git rebase -i main"
R 'git log --format="%h %s" main..topic'
note "↑ 「第5条：加配置项」现在成了最老的提交（在 log 最底下），因为它在清单里排第一。"
note "重排只有在【互相不依赖】时才不冲突；要是第5条改的是第4条创建的文件，挪到前面必然撞车。"

sub "D6 edit：在某条提交上停下来现场改（这就是 amend 和 rebase 的接头处）"
mkint d-edit
R "GIT_SEQUENCE_EDITOR='$SEQ_EDIT1' git rebase -i main"
R 'git status | head -12'
R "echo \"onto       = \$(git rev-parse --short \$(cat .git/rebase-merge/onto))\""
R "echo \"orig-head  = \$(git rev-parse --short \$(cat .git/rebase-merge/orig-head))\""
R "echo \"head-name  = \$(cat .git/rebase-merge/head-name)\""
note "★ rebase 的「进行中」状态不是内存里的魔法，就是 .git/rebase-merge/ 这一堆文件。"
note "  想明白这一点，你就明白为什么可以在别的终端里查、为什么中途断电也不会丢。"
tick
R 'git commit --amend -m "第1条：加登录骨架（在 rebase 中途顺手补全说明）"'
R 'git log --oneline -1'
tick
R 'git rebase --continue'
R 'git log --format="%h %s" main..topic'
note "edit 停下来时 HEAD 是游离指针，此刻的 amend 只影响这一条；后面的提交会被重新重放一遍。"

sub "D7 --autosquash：先 git commit --fixup，再让 rebase 自己认出该并到哪条"
mkint d-autosquash
FIXTARGET=$(git rev-parse --short HEAD~2)
R "git rev-parse --short HEAD~2"
printf '// 这里再补一行注释\n' >> form.js
Q git add -A
tick
R "git commit -q --fixup=$FIXTARGET && git log --format='%h %s' -3"
note "↑ --fixup 帮你造的提交说明是「fixup! 第3条：加表单校验」—— 这行字就是 autosquash 的匹配依据。"
R "GIT_SEQUENCE_EDITOR='$SB/seq-show.sh' GIT_EDITOR=true git rebase -i --autosquash main"
R 'git log --format="%h %s" main..topic'
note "看上面那份清单：fixup 那行【已经被 git 自动插到第3条后面】了，这就是 --autosquash 干的事。"
note "日常节奏就是：发现问题 → git commit --fixup=<那条提交> → 攒够了一起 git rebase -i --autosquash 收尾。"

sub "D8 drop：合法用法是清单里写 drop，事故用法是「把这行删了」"
mkint d-drop
R "GIT_SEQUENCE_EDITOR='$SEQ_DROP4' git rebase -i main"
R 'git log --format="%h %s" main..topic'
note "第4条被丢掉了 —— 这是你【主动】丢的，git 照办了。"
note "但如果丢的不是你想要丢的那条呢？那就是 H 节的主角（这里先埋伏笔）。"

# ============================================================================
say "E git rebase --onto：摘掉一段历史 / 给分支换个底本"
# ============================================================================
sub "E1 场景一：分支中间有两条提交是废的，想精准摘掉【那一段】"
newrepo e-onto
w core.txt 'core
'
qc "c1 起点"
Q git switch -q -c topic
w keep1.txt '有用的改动 1
'
qc "t1：有用的改动"
w junk.txt '试验性写法 A
'
qc "t2：试验（现在觉得不该要）"
w junk.txt '试验性写法 B
'
Q git add -A; qc "t3：试验的后续（也不该要）"
w keep2.txt '有用的改动 2
'
qc "t4：另一个有用的改动"
w keep3.txt '有用的改动 3
'
qc "t5：还有这个"
R 'git log --format="%h %s" main..topic'
R "echo \"新底本 t1 = \$(git rev-parse --short topic~4)   分界线 t3 = \$(git rev-parse --short topic~2)\""
tick
R "git rebase --onto \$(git rev-parse topic~4) \$(git rev-parse topic~2) topic"
R 'git log --format="%h %s" main..topic'
R 'ls -1'
note "★ --onto 的读法只有一个秘密：搬的是 (分界线, 分支头】这一段，然后把它们接到【新底本】后面。"
note "  写成 git rebase --onto <接到哪> <从哪开始不搬> <哪条分支>；t2、t3 落在区间外被丢掉。"
note "  第三个参数不写 = 你脚下这条分支。"
R 'git log --oneline --graph --decorate --all'

sub "E2 场景二：分支挂错底本了（从旧功能分支拉出来的修复，其实该基于 main）"
newrepo e-retarget
w core.txt 'core
'
qc "c1 起点"
Q git switch -q -c old-feature
w old1.txt '老功能第一步
'
qc "old1：老功能第一步"
w old2.txt '老功能第二步
'
qc "old2：老功能第二步"
Q git switch -q -c hotfix old-feature
w hot.js '真正的修复就这一处
'
qc "hotfix：一个跟老功能无关的独立修复"
Q git switch -q main
w new.txt '主线新代码
'
qc "m2：主线往前走"
R 'git log --oneline --graph --decorate --all'
note "局面：hotfix 是从还没合进主干的 old-feature 长出来的，但它那个改动和老功能毫无关系。"
note "要的是【只搬 hotfix 那一条】，把 old1/old2 留在原处 —— 这正是 --onto 的主场。"
tick
R "git rebase --onto main \$(git rev-parse old-feature) hotfix"
R 'git log --oneline --graph --decorate --all'
R 'git log --format="%h %s" main..hotfix'
note "↑ hotfix 现在只领先 main 一条；old-feature 一条没动（拿本节第一张图对照，形状一眼就懂）。"
R 'git log --format="%h %s" main..old-feature'

# ============================================================================
say "F git cherry-pick：只把某一条提交抄过来"
# ============================================================================
newrepo f-cherry
w core.txt 'base
'
qc "c1 起点"
Q git switch -q -c hotfix
w bugfix.c 'if (x == NULL) return;
'
qc "h1：修一个崩溃"
w tools.c 'tools-v1
'
qc "h2：加工具函数 tools.c"
w ui.css 'body { margin: 0 }
'
qc "h3：顺手改了样式"
Q git switch -q -c exp hotfix
w experiment.c '试验代码
'
qc "e1：试验性改动（别一起要）"
w tools.c 'tools-v2
'
Q git add -A; qc "e2：把 tools.c 改成 v2（这个要）"
Q git switch -q main
w core.txt 'base
mainline
'
Q git add -A; qc "m1：主线改动"

sub "F1 先看清有哪些提交可选"
R 'git log --oneline --graph --decorate --all'
R 'git log --oneline --format="%h %s" --all | cat'

sub "F2 最基础的一条：git cherry-pick <提交>"
tick
R "git cherry-pick \$(git rev-parse --short hotfix~1)"
R 'git log --format="%h %s" -3'
note "↑ 输出里 [main 新hash] 那行就是新造的那条；内容一样，哈希不一样（B 节同理）。"
note "cherry-pick 是【追加到当前分支尾巴上】，和它原来在哪个分支上无关。"

sub "F3 加 -x：在说明里留下一行「我是从哪个提交抄来的」"
tick
R "git cherry-pick -x \$(git rev-parse --short exp~1)"
R 'git show -s --format=%B HEAD'
note "跨分支搬修复时强烈建议 -x：将来想知道这改动哪来的，log 里直接就写着来源哈希。"

sub "F4 一次搬一串：范围写法（从旧到新逐个应用）"
R 'git log --format="%h %s" hotfix | cat'
tick
R 'git cherry-pick main~1..hotfix'
note "↑ main~1..hotfix = hotfix 有、c1 没有的那几条（h1 h2 h3），git 按【从旧到新】一个个抄。"
note "  抄到 h2 停住了 —— 因为 h2 那份补丁在 F2 已经抄过一遍，落到当前分支成了空改动。"
R 'git status -sb | head -8'

sub "F5 抄出空提交时，git 把出路直接写在提示里"
note "上面那段提示的原文是：If you wish to commit it anyway, use: git commit --allow-empty /"
note "Otherwise, please use 'git cherry-pick --skip' —— 想硬留一条空提交就 --allow-empty，不要就 skip。"
note "这里选 --skip（跳过这条重复的），sequencer 会自己接着把剩下的 h3 抄完。"
tick
R 'git cherry-pick --skip'
R 'git log --format="%h %s" -4'
note "↑ h1 和 h3 进来了，重复的 h2 被跳过，序列自动结束 —— 「cherry-pick 是个序列」就是这个意思。"
R 'git status -sb | head -3'

sub "F6 抄一个合并提交：不带 -m 一定报错"
Q git switch -q hotfix
Q git switch -q -c merge-host main
Q 'git merge -q --no-ff -m "把 hotfix 合进来" hotfix'
MERGESH=$(git rev-parse --short HEAD)
R "git log --format='%h %p %s' -1"
R "git cherry-pick $MERGESH"
note "↑ error: ... is a merge but no -m option was given —— 合并提交有两个父亲，git 不知道以哪个为底本。"
tick
R "git cherry-pick -m 1 $MERGESH"
R 'git log --format="%h %p %s" -2'
note "-m 1 = 以第 1 个父亲（当时你脚下的那条线）为底本，只把第 2 个父亲带来的差异重放一遍。"
note "↑ 这里它停在「空提交」上：本仓库的 hotfix 内容前面已经被 F2/F5 抄到 main 上了，"
note "  合并提交相对第 1 个父亲的差异在当前分支里一行都不剩 —— 出路和 F5 一样：--skip / --abort。"
R 'git cherry-pick --abort'
R 'git status -sb | head -2'

sub "F7 ★ -n（--no-commit）：只把改动拿过来，先不造提交"
newrepo f-nocommit
w core.txt 'base
'
qc "c1 起点"
Q git switch -q -c other
w big.patch.txt 'A 部分
B 部分
'
qc "o1：一次塞了 A+B 两部分（我只想借 A）"
Q git switch -q main
w core.txt 'base
mainline
'
Q git add -A; qc "m1：主线"
R "git cherry-pick -n \$(git rev-parse --short other)"
R 'git status --short'
R 'git diff --cached --stat | cat'
R 'git log --format="%h %s" -1'
R 'ls .git/CHERRY_PICK_HEAD'
note "↑ 三个关键信息：① 当前分支的头还是 m1，没有新提交 ② 改动已经进了【暂存区】（第一格 A）"
note "  ③ 连 CHERRY_PICK_HEAD 都没创建 —— -n 的意思就是「东西给我，剩下的我自己决定」。"
w extra.txt '我自己这边还要补一点
'
R 'git add extra.txt'
tick
R 'git commit -m "把 o1 的改动和我自己的合成一条"'
R 'git log --format="%h %s" -2'
R 'git status --short'
hint "-n 之后原提交的信息【不会】自动带进说明，得自己写；这点和默认 cherry-pick 不同。"

sub "F8 cherry-pick 撞车：冲突现场的三条出路（和 rebase 是同一套）"
newrepo f-conf
w tools.c 'tools-v1
'
qc "c1 起点"
Q git switch -q -c side
w tools.c 'tools-SIDE
'
Q git add -A; qc "s1：tools.c 改成 SIDE"
Q git switch -q main
w tools.c 'tools-MAIN
'
Q git add -A; qc "m1：tools.c 改成 MAIN"
tick
R "git cherry-pick \$(git rev-parse --short side)"
R 'git status --short'
R 'cat .git/CHERRY_PICK_HEAD && echo "↑ 这个文件里记的就是正在被抄的那个提交"'
R 'git cherry-pick --continue'
note "↑ 没 add 就 continue → 当场报错（和 rebase --continue 一模一样的话术）。"
w tools.c 'tools-手工合并后的最终版
'
R 'git add tools.c'
tick
R 'git cherry-pick --continue'
R 'git log --format="%h %s" -3'
note "记这组对照：cherry-pick 用 --continue/--skip/--abort，rebase 也用 --continue/--skip/--abort，"
note "merge 用 --continue/--abort（没有 skip）—— 全是「状态写在 .git 目录里」的同一套设计。"

# ============================================================================
say "G git commit --amend：最短的一次历史改写，以及它的代价"
# ============================================================================
newrepo g-amend
Q git init -q --bare -b main "$SB/g-origin.git"
w login.js 'function login() {}
'
qc "第一条：登录骨架"
PUBLISHED=$(git rev-parse --short HEAD)
R 'git remote add origin ../g-origin.git'
tick
R "git push -q -u origin main && echo \"本地 main = \$(git rev-parse --short main)\""
R "git --git-dir=$SB/g-origin.git log --format='%h %s' | cat"
note "↑ 这里的 origin 是沙箱里的 bare 仓库（../g-origin.git），全程没有联网，只是让它扮演服务器。"
note "  上面那条命令用 --git-dir 直接读那个 bare 目录，是【服务器视角】：这条提交已经不属于你一个人了 —— 它就是「已发布历史」。"
w utils.js 'function pad() {}
'
Q git add -A; qc "第二条：加 utils.js（还没发布）"
R 'git log --format="%h %p %s" -2'
note "↑ 本地现在有两条：$PUBLISHED 已经在服务器上（动它要三思），最顶上那条只有你自己知道 —— 随便整理。"
LOCAL_ONLY=$(git rev-parse --short HEAD)

sub "G1 最常见的两种 amend：改说明 / 补漏掉的改动"
tick
R "git commit -q --amend -m \"第二条：加 utils.js（说明改过了）\" && git log --format='%h %p %s' -2"
hint "被 amend 的是刚造的第二条：$LOCAL_ONLY → 现在换了哈希。只改说明，内容一行没动。"
note "哈希还是变了 —— 因为提交说明本身就参与哈希计算（见 B4）。"
printf 'body { color: red }\n' > style.css
R 'git status --short'
R 'git add style.css'
tick
R 'git commit -q --amend --no-edit && git show --stat --format="%h %s" HEAD | cat'
note "漏文件的标准修法：改 → add → --amend --no-edit。--no-edit = 说明保持原样，不弹编辑器。"

sub "G2 amend 只会带上【暂存区里的东西】，没 add 的不跟着走"
w wip.txt '这个我还没决定要不要
'
R 'git status --short'
tick
R 'git commit -q --amend --no-edit && git show --stat --format="%s" HEAD | cat'
R 'git status --short'
note "↑ wip.txt 还是 ??（没进暂存区），所以没被 amend 带走 —— 这正是你想要的安全边界。"

sub "G3 想 amend 更早的提交？直接给你报错"
R 'git commit --amend -m "试图改更早的提交" HEAD~1'
note "↑ error: pathspec 'HEAD~1' did not match ... —— amend 这个名字骗人，它只认 HEAD 那一条。"
note "  要改更早的提交，用 D 节的 rebase -i（edit / reword / fixup）。"
R 'git log --format="%h %s" -3'

sub "G4 ★ 已经发布出去的那条能不能 amend？先看 push 会发生什么"
note "把分支指针退回服务器上那条（03 章的 reset），模拟「你现在正在改一条已发布的提交」："
R "git reset -q --hard $PUBLISHED && git log --format='%h %s' -1"
tick
R 'git commit -q --amend -m "第一条：登录骨架（把已发布的也改了）" && git log --format="%h %s" -2'
R "echo \"本地 HEAD     = \$(git rev-parse --short HEAD)\""
R "echo \"origin/main   = \$(git rev-parse --short origin/main)\""
tick
R 'git push'
note "↑ 这就是代价：非快进（non-fast-forward）被拒。git 在替你把关 —— "
note "  你手上这条历史不再是远程那条历史的「下一代」，推上去会把服务器上那条提交挤掉。"
R "git --git-dir=$SB/g-origin.git log --format='%h %s' | cat"
note "服务器此刻还稳稳停在那条旧提交上。正确的做法只有两种："
note "  ① 认了：在尾巴上【再加一条】新提交（最老实，代价是多一条记录）；"
note "  ② 确认没有别人基于这条分支干活，才改写 + 强推（下面演示，看清楚后果）。"
tick
R 'git push --force-with-lease'
R "git --git-dir=$SB/g-origin.git log --format='%h %s' | cat"
note "↑ forced update 之后服务器指针被挪走，旧那条提交在服务器上不再属于任何分支。"
note "  --force-with-lease 比裸 -f 好的地方：它要求「远程还停在我上次看到的位置」，"
note "  同事期间推过新东西它就会拒绝（07 章有那一幕的完整现场）；但它【挡不住】你改写自己的历史。"
note "  真正的代价不是 git 报错，而是：基于旧提交干活的同事，分支会瞬间和你对不上。"
note "  ★ 团队铁律：只有【没发布过】的提交可以随便 amend / rebase；发布过的用「往上叠新提交」解决。"

# ============================================================================
say "H ★★★ 被 rebase 弄丢的提交怎么捞回来：reflog 实战（本章最值钱的一节）"
# ============================================================================
note "剧情：整理历史时想删掉清单里的一行注释，结果 sed 写宽了，把一条真提交整行删了。"
note "清单里删一行 = drop（那句警告早就写在清单注释里：THAT COMMIT WILL BE LOST）。"
newrepo h-rescue
w a.txt 'a
'
qc "c1 起点"
Q git switch -q -c feature
w one.txt '有用的改动 1
'
qc "F1：有用的改动"
w two.txt '★ 这条被我手滑删掉了（里面有重要代码）
'
qc "F2：★ 被我手滑删掉的那条"
w three.txt '有用的改动 3
'
qc "F3：也有用"
R 'git log --format="%h %s" main..feature'

sub "H1 事故就在一行命令之间"
tick
R "GIT_SEQUENCE_EDITOR='sed -i \"/F2/d\"' git rebase -i main"
R 'git log --format="%h %s" main..feature'
note "↑ rebase 成功完成，退出码 0，git 一句错误都没报 —— 它只是照办：清单里没这行，那就不重放。"

sub "H2 先确认「真的丢了」：所有 log 都找不到它"
R 'git log --oneline --all | grep F2'
note "↑ grep 没找到，退出码 1。此时 git log --all（所有引用能到的提交）里确实没有这条。"
R "git branch -a | cat"
note "但「看不见」不等于「不存在」。对象还在 .git/objects 里，只是暂时没有分支指向它（B4 讲过）。"

sub "H3 ★ git reflog：git 自己记的「你最近干过什么」流水账"
R 'git reflog -12'
note "读法（四列）：<哈希> <位置>@{序号}: <动作>: <说明>。"
note "  rebase (finish) = 整理后的结果；rebase (start) = 搬家动作的开始；"
note "  再往下的 commit: F3 / F2 / F1 就是三条原始提交 —— ★ 被删掉那条就在里面。"
R "git reflog | grep -n 'F2'"
R "git log --format='%h %s' -3 \$(git reflog | grep 'commit: F2' | head -1 | cut -d' ' -f1)"
note "↑ 顺着 reflog 拿到那个哈希，把「丢之前的完整三条」原样列了出来 —— 人已经救回来了。"

sub "H4 更快的后悔药：ORIG_HEAD（每次改写开始前 git 帮你记的位置）"
R 'git rev-parse --short ORIG_HEAD'
R 'git log --format="%h %s" -3 ORIG_HEAD'
note "ORIG_HEAD 就是「这次改写之前分支在哪」，不用数 @{几}，直接拿来核对。"
note "★ 它只在改写类动作之后有效：rebase / merge / pull / reset --hard 会写它，普通提交不写。"

sub "H5 救援方案一：整条分支原地复原（最常用，一条命令）"
R 'git branch feature-snapshot ORIG_HEAD'
R "git reset -q --hard \$(git rev-parse feature-snapshot) && git log --format='%h %s' main..feature"
R 'ls -1'
R 'cat two.txt'
note "↑ feature 又回到 rebase 之前（F1 F2 F3 三条齐全）。顺手留的 feature-snapshot 别急着删，"
note "  等确认整理结果没问题了再收拾 —— 这是零成本的保险。"
R 'git reflog -4'

sub "H6 救援方案二：只把丢的那一条重新抄回来（其它整理结果照单保留）"
newrepo h-surgical
w a.txt 'a
'
qc "c1 起点"
Q git switch -q -c feature
w one.txt '有用的改动 1
'
qc "G1：有用的改动"
w two.txt '★ 手滑删掉的那条（只要它回来就行）
'
qc "G2：★ 手滑删掉的那条"
w three.txt '有用的改动 3
'
qc "G3：也有用"
tick
Q "GIT_SEQUENCE_EDITOR='sed -i \"/G2/d\"' git rebase -i main"
R 'git log --format="%h %s" main..feature'
R "git rev-list -n 1 --all --grep='G2'"
note "↑ rev-list --all 也找不到（--all 只看引用可达的对象，reflog 不在其中），所以必须回 reflog 拿哈希。"
G2=$(git reflog | grep 'commit: G2' | head -1 | cut -d' ' -f1)
R "git log -1 --format='%h %s %cd' $G2"
tick
R "git cherry-pick $G2"
R 'git log --format="%h %s" main..feature'
R 'cat two.txt'
note "↑ 整理结果保留，只把丢的那一条贴回尾巴 —— 用 cherry-pick 捞「过去某条提交的内容」最顺手。"

sub "H7 分支自己的 reflog（比 HEAD 的更好读）"
R 'git log -g --format="%h %gd %gs" feature | cat'
R "git rev-parse --short 'feature@{1}'"
R "git log --format='%h %s' -1 'feature@{1}'"
note "HEAD@{n} 记的是「你脚下位置的流水」，切分支、checkout 都会掺进去；"
note "feature@{n} 只记这条分支指针的变动，救分支时更准。两者都是 .git/logs/ 下面的纯文本文件。"
R "tail -3 .git/logs/refs/heads/feature"

sub "H8 为什么这次 git fsck 什么都没报（一个常见误解）"
R 'git fsck --unreachable'
note "↑ 空。因为 reflog 还指着那些旧提交 —— 只要 reflog 里还有，git 就认为它们【可达】，"
note "  于是 fsck 不把它们当悬空对象，gc 也不会删。这恰恰说明 reflog 是唯一那道救生圈。"
R 'git config --get gc.reflogExpire; git config --get gc.reflogExpireUnreachable'
note "↑ 本仓库没配（所以退出码 1），走默认：可达的 reflog 条目留 90 天，不可变的留 30 天，之后 gc 才真删。"
note "★ 所以发现丢东西时的正确顺序是：立刻停手 → git reflog 找哈希 → branch/reset/cherry-pick 抢救。"
note "  千万别先跑 git gc / git reflog expire / git prune —— 那才是真把救生圈戳破的动作。"

sub "H9 三种后悔药放在一起，选哪个一目了然"
mkconflict h-pills            # 复用 C 节那个「必然停在半路」的现场
BEFORE2=$(git rev-parse --short topic)
tick
R 'git rebase main'
R "echo \"rebase 开始前，topic = $BEFORE2\""
R 'echo "状态文件 .git/rebase-merge/orig-head = $(cat .git/rebase-merge/orig-head)"'
R 'echo "ORIG_HEAD                            = $(git rev-parse ORIG_HEAD)"'
note "↑ 冲突停在半路时，搬家前的哈希同时躺在两个地方：状态目录里的 orig-head 文件，和 .git/ORIG_HEAD。"
note "  前者是「这次 rebase 还没结束」的证据，后者是「上次改写开始前在哪」—— 同一个值，两个用途。"
tick
R 'git rebase --abort'
R 'git rev-parse --short topic'
R 'git rev-parse --short ORIG_HEAD'
note "abort 之后：分支回到 $BEFORE2，ORIG_HEAD 也还停在那儿 —— 双保险。"
note "一句话分清：--abort 用于【还没完成】的 rebase；ORIG_HEAD / reflog 用于【已经完成但结果不对】。"

# ============================================================================
say "I 本章记住六句话"
# ============================================================================
echo "  1) 先看形状再动手：git log --oneline --graph --decorate --all"
echo "     + git merge-base + git log --oneline main..topic（= 这次会被搬走的到底是哪几条）。"
echo "  2) rebase 不是移动提交，是【照着旧提交重做一批新提交 + 挪分支指针】；"
echo "     parent 和时间戳变了，哈希必然全变；补丁本身没变（patch-id 一样）。"
echo "  3) 冲突三条出路永远写在报错里：解决 → git add → --continue；这条不要了 → --skip；"
echo "     整个作废 → --abort。（--quit 只删状态、把人留在半路，日常别用）"
echo "  4) 没有图形编辑器也能用 rebase -i：GIT_SEQUENCE_EDITOR 改清单，GIT_EDITOR 改说明，"
echo "     sed 或小脚本就能全自动：pick|reword|edit|squash|fixup|drop + 重排顺序。"
echo "     整理已经写好的东西，用 git commit --fixup + git rebase -i --autosquash 最省事。"
echo "  5) --onto <新底本> <分界线(不含)> <分支> = 只搬中间那一段；"
echo "     cherry-pick = 只抄一条（-n 只抄不提交，-x 留下来源，抄合并提交必须 -m）。"
echo "  6) 丢提交不可怕：git reflog / ORIG_HEAD / .git/rebase-merge/orig-head 都能拿到旧哈希，"
echo "     git reset --hard <哈希> 整条复原，或 git cherry-pick <哈希> 只捞那一条；千万别先跑 gc。"
echo "     已发布的历史别改写 —— push 被拒那一下就当交学费了。"
echo
echo "全部演示完成。"
exit 0
