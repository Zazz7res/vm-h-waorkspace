#!/usr/bin/env bash
# 第 03 章 demo：撤销与回退
#
# 用法：
#   bash demo.sh          跑完自动删沙箱
#   bash demo.sh --keep   跑完保留沙箱并打印路径，你可以进去自己敲命令
#
# 安全说明：
#   * 全部操作都在 mktemp -d 出来的临时沙箱里，绝不碰你机器上任何已有仓库。
#   * 不访问网络；需要「远程」时用本地 bare 仓库冒充。
#   * 提交身份只写进沙箱仓库的本地配置（git config，不带 --global）。
#   * 时间戳从 2026-01-01T10:01:00 +0800 起逐条递增，输出可复现。
#   * 输出语言由 LANG 决定，这里固定成中文方便和 README 对照；
#     英文环境下同样的命令输出英文，含义一致。
set -euo pipefail

KEEP=0
for a in "$@"; do
  case "$a" in
    --keep) KEEP=1 ;;
    -h|--help) sed -n '1,17p' "$0"; exit 0 ;;
    *) echo "未知参数：$a（只支持 --keep）" >&2; exit 2 ;;
  esac
done

SB=$(mktemp -d "${TMPDIR:-/tmp}/git-handbook-03-undo-XXXXXXXX")

cleanup() {
  local rc=$?
  if [ "$KEEP" = 1 ]; then
    echo
    echo "沙箱已保留，可以进去继续敲：cd $SB"
    echo "各场景目录：$(ls -1 "$SB" | tr '\n' ' ')"
    echo "看完记得删：rm -rf $SB"
  else
    rm -rf "$SB"
  fi
  exit "$rc"
}
trap cleanup EXIT

export LANG=zh_CN.UTF-8 LC_ALL=zh_CN.UTF-8
export GIT_PAGER=cat PAGER=cat GIT_MERGE_AUTOEDIT=no
unset GIT_DIR GIT_WORK_TREE GIT_INDEX_FILE GIT_AUTHOR_DATE GIT_COMMITTER_DATE 2>/dev/null || true

say() {
  echo
  echo "════════════════════════════════════════════════════════════════"
  echo "  $*"
  echo "════════════════════════════════════════════════════════════════"
}
note() { echo "  ↳ $*"; }
sub()  { echo; echo "  ---- $* ----"; }

# 静默执行（搭场景用，不刷屏）
qrun() { "$@" >/dev/null 2>&1; }

# 打印并执行（管道/重定向写在字符串里）
sh_show() { printf '$ %s\n' "$1"; eval "$1" || true; }

# 打印并执行；这一步预期会失败，把报错原样留在输出里
sh_fail() {
  printf '$ %s\n' "$1"
  local rc=0
  eval "$1" || rc=$?
  echo "← 退出码 $rc（这一步就是专门给你看它报错的）"
}

SEQ=0
tick() {
  SEQ=$((SEQ + 1))
  local total=$((600 + SEQ)) hh mm ts
  hh=$((total / 60)); mm=$((total % 60))
  printf -v ts '2026-01-01T%02d:%02d:00 +0800' "$hh" "$mm"
  export GIT_AUTHOR_DATE="$ts" GIT_COMMITTER_DATE="$ts"
}

newrepo() {
  local dir="$SB/$1"
  mkdir -p "$dir"
  qrun git init -b main "$dir"
  qrun git -C "$dir" config user.name "学习者"
  qrun git -C "$dir" config user.email "learn@example.com"
  cd "$dir"
  note "新建沙箱仓库 $1/（已给它单独配好本地身份：学习者 <learn@example.com>）"
}

qc() { tick; qrun git add -A; qrun git commit -m "$1"; }
w()  { printf '%s' "$2" > "$1"; }

echo "沙箱位置：$SB"

###############################################################################
say "场景 ① 工作区文件改坏了：退回上一次提交时的样子"
###############################################################################
note "正在演示：手改乱了工作区的文件 → 一条命令把它还原成仓库里的版本"
newrepo s1
w note.txt '第一行：目标
第二行：做法
第三行：注意事项
'
qc "写好提纲"
sh_show 'git log --oneline'

sub "混乱现场：把第二行改掉，末尾再加一行乱码"
w note.txt '第一行：目标
第二行：做法（这里被我改乱了）
第三行：注意事项
乱码乱码乱码
'
sh_show 'git status'
sh_show 'git diff --stat'

sub "撤销：git restore <文件> —— 拿仓库里的版本覆盖工作区这个文件"
sh_show 'git restore note.txt'
sh_show 'git status'
sh_show 'cat note.txt'

sub "老写法完全等价，只是名字绕：git checkout -- <文件>"
w note.txt '第一行：目标
第二行：又被我改乱了
第三行：注意事项
'
sh_show 'git checkout -- note.txt'
sh_show 'cat note.txt'

sub "坑：restore / checkout 只管 Git 认识的文件，新建从没 add 过的它管不了"
w scratch.txt '随手写的草稿，还没 add 过
'
sh_show 'git status --short'
sh_fail 'git restore scratch.txt'
note "所以未跟踪的文件只能靠 git clean（场景 ⑥）或者手动 rm"

###############################################################################
say "场景 ② 已经 git add 又反悔：退回工作区状态，但改动必须留着"
###############################################################################
note "正在演示：git add 之后反悔 → restore --staged 只退回暂存区，工作区一个字不动"
newrepo s2
w a.txt 'A 原始
'
w b.txt 'B 原始
'
qc "初始"

sub "混乱现场：两个文件都改了，但只 add 了 a.txt"
w a.txt 'A 原始
A 的新改动，我还想留着
'
w b.txt 'B 原始
B 的新改动
'
sh_show 'git add a.txt'
sh_show 'git status --short'
note "读法：git status --short 一行有两个格子。第一格 = 暂存区 vs 仓库，第二格 = 工作区 vs 暂存区。"
note "a.txt 是 M␣（已暂存），b.txt 是 ␣M（改了但没暂存）。"

sub "撤销：git restore --staged a.txt"
sh_show 'git restore --staged a.txt'
sh_show 'git status --short'
sh_show 'cat a.txt'
note "对比：a.txt 从「M  a.txt」变成「 M a.txt」，但 cat 出来的新改动一行没少。这才是安全撤回。"

sub "只有连工作区的改动也不想要了，才再加一条不带 --staged 的 restore（这条会真丢东西）"
sh_show 'git restore a.txt'
sh_show 'cat a.txt'
sh_show 'git status --short'

sub "更老派的等价写法：git reset HEAD <文件>（同样只动暂存区，不动工作区）"
w a.txt 'A 原始
A 又改了
'
sh_show 'git add a.txt'
sh_show 'git status --short'
sh_show 'git reset HEAD a.txt'
sh_show 'git status --short'
sh_show 'cat a.txt'

###############################################################################
say "场景 ③ 提交说明写错了：git commit --amend 就地重写最后一条"
###############################################################################
note "正在演示：最后一条提交的说明打错字 → amend -m 改掉"
newrepo s3
w a.txt '登录逻辑
'
qc "初始"
w a.txt '登录逻辑
修一下超时
'
qc "fixd 登录"
note "上面那条提交说明写错了：fixd 应该是 fix: "
sh_show 'git log --oneline'
BEFORE=$(git rev-parse HEAD)

sub "撤销：git commit --amend -m <新说明> —— 撤掉上一条提交，用新说明重做一条"
tick
sh_show "git commit --amend -m 'fix: 修复登录超时'"
sh_show 'git log --oneline'
AFTER=$(git rev-parse HEAD)
note "hash 对照：改之前 $BEFORE"
note "　　　　　　改之后 $AFTER"
note "提交对象是不可变的（见 01 章）。改说明 = 造一个新对象，所以 hash 必变。"
sh_show 'git reflog | head -5'
note "旧提交没有消失，只是没人引用它了 —— 这正是场景 ⑧ 能捞回来的原因。"

###############################################################################
say "场景 ④ 提交完发现漏了文件：add 之后 --amend 并进同一条"
###############################################################################
note "正在演示：漏文件 → git add 漏的那个 → git commit --amend --no-edit"
newrepo s4
w app.js 'console.log(1)
'
qc "feat: 加登录功能"
note "刚才忘了 README.md。先看这条提交现在只含 1 个文件："
sh_show 'git show --stat --oneline HEAD'

sub "撤销：把漏的文件并进同一条提交，说明不改"
w README.md '怎么跑起来
'
sh_show 'git add README.md'
tick
sh_show 'git commit --amend --no-edit'
sh_show 'git show --stat --oneline HEAD'
note "文件从 1 个变成 2 个，说明还是原来那句，历史里也没多出一条「漏文件」的脏提交。"

sub "危险边界：amend 改的也是历史。如果这条提交已经推给别人了，就不能这么干"
note "先接一个本地 bare 仓库冒充 origin，把上面这条推出去，制造「别人已经拉走过」："
qrun git init --bare "$SB/s4-origin.git"
sh_show "git remote add origin ../s4-origin.git"
sh_show 'git push -u origin main'
note "↑ 现在这条提交已经是公共历史。手滑又 amend 了一下（补一个文件）："
w extra.js '顺手补的
'
sh_show 'git add extra.js'
tick
sh_show 'git commit --amend --no-edit'
sh_show 'git log --oneline'
sh_fail 'git push origin main'
note "被拒了。要么用带保险绳的 push --force-with-lease（05 章教），"
note "要么老老实实用场景 ⑦ 的 revert，往后加一条反向提交。"
note "一句话判据：这段历史别人拉走过吗？拉走过就不要改写。"

###############################################################################
say "场景 ⑤ 撤销提交本身：reset --soft / --mixed / --hard 三兄弟"
###############################################################################
note "正在演示：同一份乱掉的现场复制三份，各用一种 reset，逐项对比"
echo
echo "  先记住这张表（★ = 会真丢东西）："
echo "  ┌──────────────┬────────────┬────────────┬──────────────────┐"
echo "  │              │  --soft    │  --mixed   │  --hard          │"
echo "  ├──────────────┼────────────┼────────────┼──────────────────┤"
echo "  │ 分支指针     │ 移动       │ 移动       │ 移动             │"
echo "  │ 暂存区       │ 不动       │ 重置       │ 重置             │"
echo "  │ 工作区       │ 不动       │ 不动       │ 覆盖 ★           │"
echo "  └──────────────┴────────────┴────────────┴──────────────────┘"
echo "  不带参数时默认就是 --mixed，所以 git reset HEAD~2 等于 git reset --mixed HEAD~2"

build_s5() {
  newrepo "s5-$1"
  w a.txt '第1行：骨架
'
  qc "提交 1"
  local i
  for i in 2 3 4 5; do
    printf '第%s行：新增\n' "$i" >> a.txt
    printf 'doc%s 的内容\n' "$i" > "doc$i.md"
    qc "提交 $i"
  done
  printf '第6行：还没提交的工作区改动\n' >> a.txt
  echo '未跟踪的草稿' > draft.txt
  note "混乱现场：5 条提交 + 工作区一条没提交的改动 + 一个未跟踪的 draft.txt"
}

for mode in soft mixed hard; do
  case $mode in
    soft)  impact='只有分支指针' ;;
    mixed) impact='分支指针 + 暂存区' ;;
    hard)  impact='分支指针 + 暂存区 + 工作区（会真删文件）★' ;;
  esac
  echo
  say "⑤-$mode　git reset --$mode HEAD~2　影响范围：$impact"
  build_s5 "$mode"
  sh_show 'git log --oneline'
  sh_show 'git status'

  sub "撤销：git reset --$mode HEAD~2"
  sh_show "git reset --$mode HEAD~2"
  sh_show 'git log --oneline'
  sh_show 'git status'
  note "a.txt 现在还剩几行（这是三种模式最关键的区别）："
  sh_show 'cat a.txt'
  note "doc4.md / doc5.md 还在吗（它们是提交 4、提交 5 加进来的新文件）："
  if [ -f doc4.md ]; then echo "  在：$(ls -1 doc4.md doc5.md | tr '\n' ' ')"; else echo "  不在了 —— 文件被 git 删掉了"; fi
  note "改动落在哪个区？看这两条 diff 谁有输出："
  sh_show 'echo "  [工作区 vs 暂存区]"; git diff --stat; echo "  [暂存区 vs 仓库]"; git diff --cached --stat'
done

echo
say "⑤ 三份对比一眼看完"
echo "  --soft  → a.txt 6 行全在，doc4/doc5 还在，改动全部排在暂存区：适合重新组织提交"
echo "  --mixed → a.txt 6 行全在，doc4/doc5 还在但变成未跟踪：默认，最常用"
echo "  --hard  → a.txt 被截回 4 行，doc4/doc5 被删掉：真的丢了"
echo
note "共同点：--hard 也不碰未跟踪文件，所以三份沙箱里 draft.txt 都活着："
sh_show 'ls -1 "$SB"/s5-*/draft.txt'
note "也就是说 reset --hard 之后 git status 里那条未跟踪还在，别看到「干净」二字就放心。"

###############################################################################
say "场景 ⑥ 一堆没用的新文件和构建产物：git clean"
###############################################################################
note "正在演示：clean 删的是未跟踪文件。先让它报名单，再真删"
newrepo s6
w app.py 'print(1)
'
qc "能跑就行"
echo 'build/' > .gitignore
qc "忽略 build 目录"
mkdir -p build
w build/out.o '编译产物
'
w try1.py '试了一下的脚本
'
w try2.py '另一个试过的脚本
'
w TEMP.log '临时日志
'
sh_show 'git status --short'

sub "保命第一步：永远先加 -n（dry-run，只报名单不删）"
sh_show 'git clean -nd'
note "被 .gitignore 忽略的 build/out.o 不在名单里。想连忽略文件一起清，要再加 -x："
sh_show 'git clean -nxd'

sub "确认名单没问题了，才真删：-fd"
sh_show 'git clean -fd'
sh_show 'git status --short'
sh_show 'echo -n "  现在目录里有："; ls -1 | tr "\n" " "; echo'
note "注意 out.o 还在，因为 -fd 不碰被忽略的文件。"
sh_show 'git clean -fdx'
sh_show 'echo -n "  现在目录里有："; ls -1 | tr "\n" " "; echo'
sh_show 'git status --short'
note "措辞对比很重要：-n 说「将删除」，真删时说「正删除」。看到正删除就已经晚了。"

###############################################################################
say "场景 ⑦-a 只想把某一条提交搬过来：git cherry-pick"
###############################################################################
note "正在演示：别的分支上有一个提交你也想要 —— 只搬那一条，别 merge 整条分支"
newrepo s7a
w cache.conf 'ttl=60
'
qc "c1 加缓存配置"
w readme.md '第一行
第二行
第三行
'
qc "c2 加 readme"
git branch -q hotfix
git switch -q hotfix
sub "hotfix 分支上做了一个只改 cache.conf 的提交"
w cache.conf 'ttl=60
max=100
'
qc "hotfix: 给缓存加上限"
HOT=$(git rev-parse hotfix)
git switch -q main
sub "与此同时 main 自己往前走了一条（改的是另一个文件，所以待会儿不会撞车）"
w guide.md "主线的进展
"
qc "c3 main 自己的改动"
sh_show 'git log --oneline --decorate --graph --all'
note "现在你在 main 上，只想要 hotfix 上面那一条，不想要整条 hotfix 分支。"

sub "搬运：git cherry-pick <hash>"
sh_show "git cherry-pick $HOT"
sh_show 'git log --oneline --decorate --graph --all'
sh_show 'cat cache.conf'
NEW=$(git rev-parse HEAD)
note "内容搬过来了，但 hash 变了（${HOT:0:7} → ${NEW:0:7}）：cherry-pick 是重新造一个提交对象，"
note "因为它的父亲变了（父亲是 c3，不再是 hotfix 上那条 c2）。hash 一变，一切就都变。"
sh_show 'git status'

sub "翻车现场：搬过来的改动和当前分支改了同一行"
git switch -q hotfix
w readme.md '第一行
第二行（hotfix 在这里加了说明）
第三行
'
qc "hotfix: readme 补一句"
git switch -q main
w readme.md '第一行
第二行（main 在这里加了另一句）
第三行
'
qc "main: readme 也补一句"
OTHER=$(git rev-parse hotfix)
sh_fail "git cherry-pick $OTHER"
sh_show 'git status --short'
sh_show 'cat readme.md'
note "撞上了。这时候三条路：修好冲突 → git add → git cherry-pick --continue；"
note "或者 git cherry-pick --skip 跳过它；或者 git cherry-pick --abort 整个当没发生。"
sh_show 'git cherry-pick --abort'
sh_show 'git log --oneline'
sh_show 'git status --short'
note "abort 之后回到撞车之前，readme.md 又是干净的了。"

###############################################################################
say "场景 ⑦-b 历史已经共享出去了：别改写，用 revert 加一条反向提交"
###############################################################################
note "正在演示：revert 不动历史，只在后面追加一条把改动反向抵消的提交"
newrepo s7b
qrun git init --bare "$SB/s7b-origin.git"
qrun git remote add origin ../s7b-origin.git
for i in 1 2 3 4; do
  printf 'f%s 的内容\n' "$i" > "f$i.txt"
  qc "提交 $i"
done
qrun git push -q -u origin main
note "上面这 4 条已经推到 origin，属于别人拉走过的历史。"
sh_show 'git log --oneline'

sub "现在「提交 3」上完线就出问题。不能用 reset（会把历史改短，同事会炸），要用："
tick
sh_show 'git revert --no-edit HEAD~1'
sh_show 'git log --oneline'
note "文件层面：f3.txt 这个文件已经不在了："
sh_show '[ -e f3.txt ] && echo "  f3.txt 还在" || echo "  f3.txt 已经不存在了"'
note "历史层面：提交 3 还留在 log 里，只是后面多了一条 Revert。这就是 revert 的意义：留痕、可追溯。"
note "而且历史是往前长不是往后缩，所以 push 不会被拒："
sh_show 'git push origin main'

sub "翻车现场：要撤销的那行改动，后来又被别人改过了"
w config.txt '第1行 别碰
第2行 本来是好的
第3行 别碰
第4行 别碰
'
qc "加配置文件"
w config.txt '第1行 别碰
第2行 提交A把它改坏了
第3行 别碰
第4行 别碰
'
qc "提交A：改了第2行"
BAD=$(git rev-parse HEAD)
w config.txt '第1行 别碰
第2行 提交B又在这行叠了新改动
第3行 别碰
第4行 别碰
'
qc "提交B：在第2行上又改了一次"
note "现在要撤销提交A。可提交A当初动的那一行，已经被提交B改成别的了，反向补丁贴不上："
tick
sh_fail "git revert --no-edit $BAD"
sh_show 'git status --short'
sh_show 'cat config.txt'
note "冲突文件和普通冲突一个样（04 章细讲）。两条路：手工把文件改对 → git add → git revert --continue；"
note "或者干脆不撤了 → git revert --abort，历史一个字没动。"
sh_show 'git revert --abort'
sh_show 'git status --short'
sh_show 'git log --oneline'

sub "撤销一条合并提交：它有两个父亲，git 不知道沿哪条线回退，会直接拒绝"
newrepo s7c
w base.txt '共同的起点
'
qc "c1 起点"
git switch -q -c feat
w feat.txt '这个功能后来合进了 main
'
qc "c2 加 feat.txt"
git switch -q main
w README.md '主线的说明
'
qc "c3 main 自己的改动"
sh_show 'git merge --no-ff feat -m "Merge branch feat"'
sh_show 'git log --oneline --graph'
tick
sh_fail 'git revert --no-edit HEAD'
note "-m 1 = 保留第 1 个父亲（主线那条线），把第 2 个父亲带进来的东西撤掉："
sh_show 'git revert --no-edit -m 1 HEAD'
sh_show 'git log --oneline --graph'
sh_show 'ls -1'
note "feat.txt 没了，但 c2 和那条 Merge 提交都还在历史里。"

###############################################################################
say "场景 ⑧ 真玩坏了：reset --hard 冲掉提交，用 reflog 捞回来"
###############################################################################
note "正在演示：reflog 是全书的后悔药。作死 → 别慌 → 查表 → 定位 → 捞回"
newrepo s8
w a.txt ''
for i in 1 2 3 4 5; do
  printf '第%s行内容\n' "$i" >> a.txt
  printf '笔记 %s\n' "$i" > "note$i.md"
  qc "辛苦做的提交 $i"
done
sh_show 'git log --oneline'
LOST=$(git rev-parse HEAD)

sub "手滑时刻：本来想 git reset --hard HEAD~1，敲成了 HEAD~4"
sh_show 'git reset --hard HEAD~4'
sh_show 'git log --oneline'
sh_show 'cat a.txt'
sh_show 'echo -n "  目录里有："; ls -1 | tr "\n" " "; echo'
note "看起来全没了。这时候千万别再敲任何改历史的命令，直接："
sh_show 'git reflog'
note "读法：HEAD@{1} 那行写着 commit: 辛苦做的提交 5，就是你被冲掉之前的位置。"
note "先确认那条提交的内容确实还在对象库里："
sh_show "git show --stat --oneline $LOST"

sub "捞回来（方式一）：先建个保险分支，再 reset 过去"
sh_show "git branch rescue $LOST"
sh_show "git reset --hard $LOST"
sh_show 'git log --oneline'
sh_show 'cat a.txt'
sh_show 'echo -n "  目录里有："; ls -1 | tr "\n" " "; echo'
note "全部回来了。保险分支用完可以删："
sh_show 'git branch -D rescue'

sub "捞回来（方式二）：直接引用 reflog 位置 HEAD@{n}"
sh_show 'git reset --hard HEAD~2'
sh_show 'git log --oneline'
sh_show 'git reflog | head -6'
note "注意：每做一次会动 HEAD 的操作，HEAD@{0} 就往后挪一格。n 是相对现在的计数，"
note "所以 HEAD@{n} 只能当场用，不能写进脚本或笔记当长期名字；长期要用的是 hash。"
sh_show "git reset --hard 'HEAD@{1}'"
sh_show 'git log --oneline'

echo
say "⑧ 追加：reflog 也会过期，真正的终点是 gc"
note "顺手记一条：git log -g 是把 log 画在 reflog 上，能看到自己的操作轨迹："
sh_show 'git log -g --oneline | head -8'

note "如果连分支都被删了、reflog 也过期了，还能用 fsck 找悬空提交。新开一个仓库专门演示："
newrepo s8b
w base.txt '起点
'
qc "c1 起点"
git switch -q -c doomed
printf '只在 doomed 分支上的改动\n' > only-doomed.txt
qc "doomed 分支的独苗"
DOOM=$(git rev-parse HEAD)
git switch -q main
sh_show 'git branch -D doomed'
note "被删分支的顶端是 $DOOM。现在没有任何分支指向它，但对象还在，hash 还能读："
sh_show "git log -1 --format='%h %s' $DOOM"
note "先直接 git fsck：reflog 还兜着的时候，它认为这些提交仍可达，所以什么都不输出："
sh_show 'git fsck 2>&1'
note "把 reflog 也清掉（模拟最坏情况；正常情况下你永远不该敲这条），fsck 才列出游离提交："
sh_show 'git reflog expire --expire=now --all'
sh_show 'git fsck --lost-found'
note "上面列出的悬空 commit 就是没人引用、但还没被删的提交，用 hash 直接捞："
sh_show "git reset --hard $DOOM"
sh_show 'git log --oneline'
sh_show 'echo -n "  目录里有："; ls -1 | tr "\n" " "; echo'

note "但真正的终点是这个 —— 再让它没人引用，清掉 reflog，然后 gc --prune=now："
sh_show 'git reset --hard HEAD~1'
sh_show 'git reflog expire --expire=now --all'
sh_show 'git gc --prune=now'
sh_fail "git cat-file -t $DOOM"
note "上面报 could not get object info，就是对象真被删了，神仙难救。"
note "所以本章最值钱的一句话：动手改历史之前，先 git branch backup-日期。"

###############################################################################
say "收尾：这一章只需要记住两件事"
###############################################################################
echo "  1) 敲撤销命令前，先回答三个问题："
echo "     · 它动了哪几个区？（工作区 / 暂存区 / 仓库）"
echo "     · 会不会真丢东西？（带 --hard 的、clean、不带 --staged 的 restore 都会）"
echo "     · 这段历史别人拉走过吗？拉走过 → 只能 revert；没拉走 → 才能 reset / amend"
echo
echo "  2) 出事了：先 git reflog，别急着再敲任何命令。"
echo "     动手前：先 git branch backup-$(date +%Y%m%d)。这条习惯比任何后悔药都便宜。"
echo
echo "全部演示完成。"
