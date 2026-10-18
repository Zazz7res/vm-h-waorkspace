#!/usr/bin/env bash
# 第 04 章 demo：分支与合并冲突
#
# 用法：
#   bash demo.sh          跑完自动删沙箱
#   bash demo.sh --keep   跑完保留沙箱并打印路径
#
# 安全说明：
#   * 全部在 mktemp -d 的临时沙箱里，绝不碰你机器上任何已有仓库。
#   * 不联网；需要远程时用本地 bare 仓库冒充。
#   * 提交身份只写进沙箱仓库的本地配置（git config，不带 --global）。
#   * 时间戳从 2026-01-01T10:01:00 +0800 起逐条递增，输出可复现。
#   * GIT_CONFIG_GLOBAL/SYSTEM 指向 /dev/null：你机器上 ~/.gitconfig 里的
#     pull.rebase、init.defaultBranch、advice.* 都不会混进这份输出。
set -euo pipefail

KEEP=0
for a in "$@"; do
  case "$a" in
    --keep) KEEP=1 ;;
    -h|--help) sed -n '1,15p' "$0"; exit 0 ;;
    *) echo "未知参数：$a（只支持 --keep）" >&2; exit 2 ;;
  esac
done

SB=$(mktemp -d "${TMPDIR:-/tmp}/git-handbook-04-branch-merge-XXXXXXXX")
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
export GIT_PAGER=cat PAGER=cat GIT_EDITOR=true GIT_MERGE_AUTOEDIT=no
export GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_SYSTEM=/dev/null
unset GIT_DIR GIT_WORK_TREE GIT_INDEX_FILE GIT_AUTHOR_DATE GIT_COMMITTER_DATE 2>/dev/null || true

say() { echo; echo "════════════════════════════════════════════════════════════════"; echo "  $*"; echo "════════════════════════════════════════════════════════════════"; }
note() { echo "  ↳ $*"; }
sub()  { echo; echo "  ---- $* ----"; }
qrun() { "$@" >/dev/null 2>&1; }
sh_show() { printf '$ %s\n' "$1"; eval "$1" || true; }
sh_fail() { printf '$ %s\n' "$1"; local rc=0; eval "$1" || rc=$?; echo "← 退出码 $rc（这一步就是专门给你看它报错的）"; }

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
  note "新建沙箱仓库 $1/"
}
qc() { tick; qrun git add -A; qrun git commit -m "$1"; }
w()  { printf '%s' "$2" > "$1"; }

echo "沙箱位置：$SB"

###############################################################################
say "A 分支管理：先学会看清局面，再谈合并"
###############################################################################
newrepo a-branches
w plan.md '第1行：项目说明
第2行：目标
第3行：分工
'
qc "c1 初始提交"
git switch -q -c feature/login
w login.js '登录页
'
qc "c2 加登录页"
w login.js '登录页
表单校验
'
qc "c3 加表单校验"
git switch -q main
w plan.md '第1行：项目说明
第2行：目标
第3行：分工
第4行：里程碑
'
qc "c4 main 往前走了一步"
qrun git init --bare "$SB/a-origin.git"
qrun git remote add origin ../a-origin.git
qrun git push -q -u origin main
qrun git push -q origin feature/login

sub "A1 git branch：只看本地分支，信息太少了"
sh_show 'git branch'
note "只有名字和 HEAD 在哪，看不出谁合并过、看不出和远程差多少。"

sub "A2 git branch -a：把远程跟踪分支也列出来"
sh_show 'git branch -a'
note "remotes/origin/... 是你本地缓存的「远程长什么样」的快照，不是实时状态。"

sub "A3 推荐用 git branch -vv：一行同时给出上游和领先/落后几个"
sh_show 'git branch -vv'
note "方括号里 [origin/main] 是上游（这条分支跟谁同步）。"
note "没有方括号 = 这条分支没有上游，push/pull 时你得自己说清楚推到哪。"

sub "A4 想看每个分支顶端那条提交的完整信息：git branch -v"
sh_show 'git branch -v'

sub "A5 git log --graph：分支形状只有画出来才看得懂"
sh_show 'git log --oneline --graph --decorate --all'
note "记住这一条：git log --oneline --graph --decorate --all，本章后面全靠它。"

sub "A6 谁已经合并进当前分支了 / 谁还没有"
sh_show 'git branch --merged'
sh_show 'git branch --no-merged'
note "--merged 是「这条分支的内容已经全部包含在当前分支里」，删掉不心疼；"
note "--no-merged 是有内容还没进当前分支，删之前要三思。"

sub "A7 改名字：git branch -m 旧名 新名"
sh_show 'git branch -m feature/login feature/login-ui'
sh_show 'git branch'

sub "A8 给分支挂上游：git branch -u origin/main feature/login-ui"
sh_show 'git branch -u origin/main feature/login-ui'
sh_show 'git branch -vv'
note "现在 feature/login-ui 的上游变成 origin/main 了 —— 这是故意挂错的，演示「上游挂错很坑」。"
sh_show 'git branch -u origin/feature/login feature/login-ui'
sh_show 'git branch -vv'

sub "A9 ★ 删除分支：-d 会保护你，-D 不会"
note "先看一条已经并进 main 的分支（用当前 main 建的 topic，没有新内容）："
git branch -q topic
sh_show 'git branch --merged'
sh_show 'git branch -d topic'
note "已合并的分支，-d 直接给你删了。现在造一条【只有它自己有、别处都没有】的分支："
git switch -q -c wip/experiment
w secret.txt '这条改动只存在于 wip/experiment
'
qc "c5 只在 wip/experiment 上的提交"
git switch -q main
sh_show 'git branch --no-merged'
note "wip/experiment 的内容既没进 main，也没有上游。用安全的 -d 删它，git 会拒绝："
DELETED_SHA=$(git rev-parse wip/experiment)
sh_fail 'git branch -d wip/experiment'
note "↑ 这就是 git 在替你把关：这条分支上有尚未合并的内容，删掉就找不回来了。"
note "确认自己真的不要了，才用 -D（大写 = 强制，不问你）："
sh_show 'git branch -D wip/experiment'
note "上面「已删除分支 ...（原为 <hash>）」里的 hash 请复制下来 —— 那是你唯一的后悔药。"

sub "A9b 顺带记住：-d 的判断标准是【相对 HEAD 或它的上游】"
note "刚才 feature/login-ui 明明没并进 main，但 -d 却直接删掉了，为什么？"
git branch -q feature/ghost
git switch -q feature/ghost
w ghost.js '幽灵功能
'
qc "c6 feature/ghost 的提交"
sh_show 'git push -q -u origin feature/ghost'
sh_show 'git branch -vv | grep ghost'
git switch -q main
note "feature/ghost 没并进 main，但它已经推到 origin、本地有 origin/feature/ghost 这个上游快照："
sh_show 'git branch -vv'
sh_show 'git branch -d feature/ghost'
note "-d 认为「它已经被上游包含了，删了也不亏（远程还有一份）」。"
note "★ 所以 -d 不是「绝对安全」：如果你的本地分支只是没合并、但已经推上去了，它会放行。"
note "  真正确认安全的做法：删之前先看 git log --oneline main..<分支名>，看有没有还没并进去的提交。"
sh_show 'git log --oneline main..feature/login-ui 2>/dev/null || echo "（分支已经不在了，看 A10 怎么救）"'

sub "A10 删错了怎么办：-D 打印的「原为 <hash>」就是后悔药"
note "分支自己的 reflog 随分支一起没了，但 -D 那行报的 hash 还在。用它原地复活："
sh_show "git branch wip/experiment $DELETED_SHA"
sh_show 'git log --oneline -1 wip/experiment'
sh_show "git show wip/experiment:secret.txt"
note "分支和它上面的文件内容都还在（用 git show <分支>:<文件> 直接读那条分支上的文件，不用切过去）。"
note "以后要养成习惯：删分支之前先 git branch -v 把 hash 看一眼；删完那条报错别急着划走。"
sh_show 'git branch -D wip/experiment'

###############################################################################
say "B 合并的三种结局"
###############################################################################
sub "B1 结局一：快进合并（fast-forward）—— 对方没动，直接推进指针"
newrepo b-ff
w doc.md '第1行
第2行
第3行
'
qc "c1 初始"
git switch -q -c feature
w extra.txt '新功能
'
qc "c2 加 extra.txt"
w doc.md '第1行
第2行
第3行
第4行
'
qc "c3 改 doc.md"
git switch -q main
sh_show 'git log --oneline --graph --decorate --all'
note "main 一条没动，feature 在它前面 3 步 —— 这种形状叫「能快进」。"
sh_show 'git merge feature'
sh_show 'git log --oneline --graph --decorate --all'
note "没有新提交产生，main 指针直接往前挪了 3 格。git 自己也说了「Fast-forward」。"
note "优点：历史是一条直线，干净。缺点：看不出这里曾经有过一次「合并」。"

sub "B1b --no-ff：技术上能快进，也强制造一个合并提交"
newrepo b-noff2
w doc.md '第1行
第2行
第3行
'
qc "c1 初始"
git switch -q -c feature
w extra.txt '新功能
'
qc "c2 加 extra.txt"
git switch -q main
w doc.md '第1行
第2行
第3行
第4行主线
'
qc "c3 main 也动了"
sh_show 'git log --oneline --graph --decorate --all'
sh_show 'git merge --no-ff feature -m "合并 feature 功能分支"'
sh_show 'git log --oneline --graph --decorate --all'
note "★ 即使技术上能快进，--no-ff 也强制造一个合并提交，把这一坨改动包成一个菱形。"
note "为什么功能分支常用 --no-ff：整个功能能被当成一个整体回退（revert -m 1 一次撤掉整个功能），"
note "而且 log 里看得见「这是某个功能合进来的」，而不是散落的 12 条提交。"

sub "B1c --ff-only 真正的用处：确认「不会造出多余合并提交」"
newrepo b-ffonly
w doc.md '第1行
'
qc "c1 初始"
git switch -q -c feature
w extra.txt 'a
'
qc "c2 功能第一步"
git switch -q main
w other.txt '主线也动了
'
qc "c3 主线改动"
sh_show 'git log --oneline --graph --decorate --all'
note "两条分支分叉了，这时候快进是不可能的。想安全地确认一下："
sh_fail 'git merge --ff-only feature'
note "git 直接中止，不留下任何半吊子状态。这就是 --ff-only 的价值：把「只许快进」变成硬约束。"
note "常见用法：git pull --ff-only，宁可拉取失败也不要 git 偷偷给你造一个合并提交。"

sub "B2 结局二：三方合并自动成功（两边改的是不同地方）"
newrepo b-auto
w doc.md '第1行：标题
第2行：背景
第3行：目标
第4行：现状
第5行：方案
第6行：风险
第7行：排期
第8行：附录
'
qc "c1 共同的起点"
git switch -q -c feature
w doc.md '第1行：标题
第2行：背景
第3行：目标
第4行：现状
第5行：方案
第6行：风险（功能分支在这里补了一句）
第7行：排期
第8行：附录
'
qc "c2 功能分支改了第6行"
git switch -q main
w doc.md '第1行：标题
第2行：背景（主线在这里加了说明）
第3行：目标
第4行：现状
第5行：方案
第6行：风险
第7行：排期
第8行：附录
'
qc "c3 主线改了第2行"
sh_show 'git log --oneline --graph --decorate --all'
sh_show 'git merge feature'
sh_show 'git log --oneline --graph --decorate'
note "上面「自动合并 doc.md」说明 git 自己处理了，还造了一个合并提交。"
note "合并提交有两个父亲，用 ^1 ^2 就能分别取到："
sh_show 'git log --format="%h %s" -1 HEAD'
sh_show "git log -1 --format='%h 的父亲们：%p'"
sh_show 'echo "  HEAD^1 = $(git rev-parse --short HEAD^1)  HEAD^2 = $(git rev-parse --short HEAD^2)"'
sh_show 'git log --oneline -1 HEAD^1'
sh_show 'git log --oneline -1 HEAD^2'
note "HEAD^1 = 我原来站的地方（主线），HEAD^2 = 我合进来的那条线。"
note "两边共同的最早祖先前，用 git merge-base 找："
sh_show 'echo "两条线的共同祖先 = $(git merge-base HEAD^1 HEAD^2)"'
sh_show 'git log --oneline -1 $(git merge-base HEAD^1 HEAD^2)'
sh_show 'cat doc.md'
note "第2行和第6行的改动都在 —— 这就是三方合并：git 拿「共同祖先」当参照，"
note "发现两边各改各的地方，就都保留。"

sub "B3 结局三：冲突 —— 两边改了同一行"
newrepo b-conflict
w config.ini '端口 = 3000
超时 = 30
'
qc "c1 初始配置"
git switch -q -c feature
w config.ini '端口 = 3000
超时 = 30
最大连接 = 100
线程 = 8
'
qc "feature: 线程 = 8"
git switch -q main
w config.ini '端口 = 9090
超时 = 30
最大连接 = 100
线程 = 4
'
qc "main: 线程 = 4，顺手把端口也改了"
sh_show 'git log --oneline --graph --decorate --all'
sh_fail 'git merge feature'
sh_show 'git status'
sh_show 'cat config.ini'
note "<<<<<<< HEAD 到 ======= 是【你自己这边】的内容，======= 到 >>>>>>> feature 是【对方分支】的内容。"
note "git 把两边都塞进文件，等你亲手删掉不要的那半 —— 它不替你决定。"

###############################################################################
say "C 冲突解决全流程（把 B3 那个现场接着做完）"
###############################################################################
cd "$SB/b-conflict"
note "当前就在冲突状态里。正在演示：看清 → 手工改 → 标记已解决 → 提交。"

sub "C1 在冲突状态下 git diff 输出长什么样（这是最难读的一屏）"
sh_show 'git diff'
note "冲突状态下 git diff 用的是「组合差异」（--cc），每个文件只有一份差异、两列前缀："
note "  第1列 = 相对 HEAD（你自己），第2列 = 相对 feature（对方）。"
note "  所以 -1 / +1 只和第一列有关就是只改了你这侧；-2 / +2 只改了那侧。"
sh_show 'git diff --cc config.ini'

sub "C2 更硬核的看法：暂存区里有三个版本，1=共同祖先 2=我们的 3=他们的"
sh_show 'git ls-files -u'
note "三个数字前缀就是「阶段号」：:1: 共同祖先、:2: 我们（HEAD 那一侧）、:3: 他们（被合进来那一侧）。"
note "把三个版本分别读出来对比："
sh_show 'echo "== :1: 共同祖先 =="; git cat-file -p ":1:config.ini"'
sh_show 'echo "== :2: ours（HEAD/我们自己） =="; git cat-file -p ":2:config.ini"'
sh_show 'echo "== :3: theirs（feature 那一侧） =="; git cat-file -p ":3:config.ini"'

sub "C3 偷懒写法：直接整份取某一侧（不是拼行，是整份文件覆盖）"
sh_show 'git checkout --ours config.ini'
sh_show 'echo "--- 取 ours 之后 ---"; cat config.ini'
sh_show 'git checkout --theirs config.ini'
sh_show 'echo "--- 取 theirs 之后 ---"; cat config.ini'
note "★ 记住：--ours/--theirs 是「整份文件选一边」，不是「这一行选一边」。"
note "  要逐行挑，就老老实实编辑文件、把 <<<<<<< ======= >>>>>>> 那些标记行删干净。"

sub "C4 正式做法：手工把文件改成想要的内容"
w config.ini '端口 = 9090
超时 = 30
最大连接 = 100
线程 = 8
'
sh_show 'cat config.ini'
note "端口取主线的 9090，线程取功能分支的 8 —— 这就是这次合并的业务决定。"

sub "C5 ★ 必须 git add：add 不是在「暂存改动」，是在报告「这个冲突我处理过了」"
sh_show 'git status --short'
sh_show 'git add config.ini'
sh_show 'git status --short'
note "UU → M 。第一格 U 就是 Unmerged（未解决）。"
note "只要还有一个文件带着 U，git 就不让你提交："
sh_show 'git commit -m "合并 feature：端口取 9090，线程取 8"'
sh_show 'git log --oneline --graph --decorate'
sh_show "git log -1 --format='合并提交 %h：%s  父亲有 %p'"
note "父亲有两个，就是这次合并的两条线："
sh_show "git log -1 --format='%h 的父亲们：%p'"
sh_show 'echo "  HEAD^1 = $(git rev-parse --short HEAD^1)  HEAD^2 = $(git rev-parse --short HEAD^2)"'

sub "C6 合并中途想放弃：git merge --abort（现场完全复原）"
note "再制造一次冲突，然后 abort："
cd "$SB"; rm -rf c-abort; qrun git init -b main c-abort >/dev/null 2>&1
cd "$SB/c-abort"
qrun git config user.name "学习者"; qrun git config user.email "learn@example.com"
w line.txt '共同的起点
第2行
第3行
第4行
第5行
第6行
第7行
第8行
'
qc "c1 共同起点"
git switch -q -c feature
w line.txt '共同的起点
第2行
第3行
第4行
第5行
第6行
第7行
feature 把第8行改了
'
qc "feature 改第8行"
git switch -q main
w line.txt '共同的起点
第2行
第3行
第4行
第5行
第6行
第7行
main 把第8行改成别的了
'
qc "main 也改第8行"
sh_fail 'git merge feature'
sh_show 'git status --short'
sh_show 'git log --oneline -1'
note "现在 abort："
sh_show 'git merge --abort'
sh_show 'git status --short'
sh_show 'git log --oneline -1'
sh_show 'cat line.txt'
note "回到合并前的那条提交，line.txt 里一个冲突标记都没有。merge --abort 是真正的后悔药。"

###############################################################################
say "D ★★★ 本章最著名的坑：--ours / --theirs 在 merge 和 rebase 里含义是反的"
###############################################################################
note "同一个冲突文件，在两种情境下分别取 --ours，看结果。"
note "现场（两次完全相同）：shared.txt 第2行，main 侧写 MAIN，feature 侧写 FEATURE，"
note "而你这个人站在 feature 分支上。"

mkswap() {
  local d=swap
  cd "$SB"; rm -rf "$d"; qrun git init -b main "$d"
  cd "$SB/$d" || return 1
  qrun git config user.name "学习者"; qrun git config user.email "learn@example.com"
  w shared.txt '共同的起点
占位内容
'
  qc "c1 共同起点"
  git switch -q -c feature
  w shared.txt '共同的起点
FEATURE 分支写的内容
'
  qc "feature: 写 FEATURE"
  git switch -q main
  w shared.txt '共同的起点
MAIN 分支写的内容
'
  qc "main: 写 MAIN"
  git switch -q feature
  note "现在站在 feature 分支上（HEAD 是 feature）"
}

sub "D1 情境一：在 feature 上执行 git merge main（合并别人进来）"
mkswap
sh_show 'git log --oneline --decorate --graph --all'
sh_fail 'git merge main'
sh_show 'cat shared.txt'
sh_show 'git ls-files -u'
sh_show 'echo "-- :2:（ours）是谁的内容 --"; git cat-file -p ":2:shared.txt"'
sh_show 'echo "-- :3:（theirs）是谁的内容 --"; git cat-file -p ":3:shared.txt"'
sh_show 'git checkout --ours shared.txt'
sh_show 'echo "-- merge 情境下取 --ours 的结果 --"; cat shared.txt'
sh_show 'git checkout --theirs shared.txt'
sh_show 'echo "-- merge 情境下取 --theirs 的结果 --"; cat shared.txt'
MERGE_OURS=$(git cat-file -p ":2:shared.txt" | tr '\n' '/')
MERGE_THEIRS=$(git cat-file -p ":3:shared.txt" | tr '\n' '/')

sub "D2 情境二：同一个现场，改成在 feature 上执行 git rebase main（把自己搬到别人上面）"
mkswap
sh_show 'git log --oneline --decorate --graph --all'
sh_fail 'git rebase main'
sh_show 'cat shared.txt'
sh_show 'git ls-files -u'
sh_show 'echo "-- :2:（ours）是谁的内容 --"; git cat-file -p ":2:shared.txt"'
sh_show 'echo "-- :3:（theirs）是谁的内容 --"; git cat-file -p ":3:shared.txt"'
sh_show 'git checkout --ours shared.txt'
sh_show 'echo "-- rebase 情境下取 --ours 的结果 --"; cat shared.txt'
sh_show 'git checkout --theirs shared.txt'
sh_show 'echo "-- rebase 情境下取 --theirs 的结果 --"; cat shared.txt'
REBASE_OURS=$(git cat-file -p ":2:shared.txt" | tr '\n' '/')
REBASE_THEIRS=$(git cat-file -p ":3:shared.txt" | tr '\n' '/')

sub "D3 ★ 两个情境并排对照（同一份冲突文件，ours/theirs 完全互换）"
echo "  情境            git checkout --ours 取到的是      git checkout --theirs 取到的是"
echo "  ---------------  --------------------------------  --------------------------------"
echo "  merge 时         $MERGE_OURS"
echo "  rebase 时        $REBASE_OURS"
echo "  （theirs 一侧正好对调：merge 时 $MERGE_THEIRS ／ rebase 时 $REBASE_THEIRS）"
echo
note "为什么会反过来？因为 ours/theirs 的本义跟「我 vs 你」没关系，它们的本义是："
note "  :2:（ours）= 【当前正在被写入的那个底本】，也就是「我们此刻站的位置」"
note "  :3:（theirs）= 【这一次被重放/被合并进来的那坨改动】"
note "merge 时：底本是你脚下这条分支，被重放进来的是对方 —— 所以 ours=你脚下、theirs=对方"
note "所以本章给一句绝对不会错的口诀："
note
note "  ★★★ merge：ours = 你当前所在分支；theirs = 被你合并进来的那个分支"
note "  ★★★ rebase：ours = 你要落到的那个底本（通常是 main）；theirs = 你自己那条被重放的分支"
note
note "也就是说 rebase 把你的改动变成「外来户（theirs）」，把 main 变成「底本（ours）」—— 正好和 merge 相反。"

###############################################################################
say "E 合并冲突的其他形状：改名、删除、以及工具"
###############################################################################
sub "E1 对方改了文件名，你改了文件内容 —— 最麻烦的一种"
cd "$SB"; rm -rf e-rename; qrun git init -b main e-rename >/dev/null 2>&1
cd "$SB/e-rename"
qrun git config user.name "学习者"; qrun git config user.email "learn@example.com"
w core.txt '第1行
第2行
第3行：这行两边都会改
第4行
第5行
'
qc "c1 共同起点"
git switch -q -c mvline
git mv core.txt heart.txt
w heart.txt '第1行
第2行
第3行：改名之后这一行也被动过
第4行
第5行
'
qc "重命名 core.txt 为 heart.txt，并且改了第3行"
git switch -q main
w core.txt '第1行
第2行
第3行：主线改成了另一句
第4行
第5行
'
qc "main 改第3行"
sh_fail 'git merge mvline'
sh_show 'git status --short'
sh_show 'cat heart.txt'
note "★ 看 git status 的字母：UD core.txt = 你改了它、对方把它「删了」（改名在 git 眼里就是删+加），"
note "  A  heart.txt = 新名字被当成一个凭空多出来的新增文件。heart.txt 里一个冲突标记都没有："
sh_show 'grep -c "<<<<<<<" heart.txt'
note "也就是说这一次合并 git 【没把 core.txt 和 heart.txt 认成同一个文件】。"
note "改名不是仓库里的元数据 —— git mv 存下来的就是「删一个 + 加一个」，「同一个文件」是 git 每次现算的相似度："
sh_show 'git show mvline~1:core.txt | wc -c'
sh_show 'git show --stat mvline | tail -3'

sub "E1b 把相似度阈值降下来，同一份现场立刻换一种冲突形状"
sh_show 'git merge --abort'
sh_show 'git diff -M --find-renames=50% --stat mvline~1 mvline'
sh_show 'git diff -M --find-renames=40% --stat mvline~1 mvline'
note "50% 认不出（仍然两行 D + A），40% 就认出来了（一行 core.txt => heart.txt）。"
note "合并时用 -X rename-threshold=20 把阈值压下去，冲突立刻从「修改/删除」变成「内容」："
sh_fail 'git merge -X rename-threshold=20 mvline'
sh_show 'git status --short'
sh_show 'cat heart.txt'
note "★ 标记两端这次写着 分支名:文件名（HEAD:core.txt / mvline:heart.txt）—— 看到冒号就知道这段涉及改名。"
note "  你要做的决定也从「留哪个文件名」变成了「留哪一句话」："
sh_show 'git ls-files -u'
sh_show 'git merge --abort'

sub "E2 双方把同一个文件改成两个不同的名字（rename/rename 冲突）"
cd "$SB"; rm -rf e-rr; qrun git init -b main e-rr >/dev/null 2>&1
cd "$SB/e-rr"
qrun git config user.name "学习者"; qrun git config user.email "learn@example.com"
w old.txt '内容A
内容B
内容C
内容D
内容E
'
qc "c1 共同起点"
git switch -q -c featA
git mv old.txt heart.txt
qc "featA: 重命名为 heart.txt"
git switch -q main
git mv old.txt core.txt
qc "main: 重命名为 core.txt"
sh_fail 'git merge featA'
sh_show 'git status --short'
sh_show 'ls -1'
note "git 不知道该保留哪个名字，于是两个都留在工作区，让你自己决定："
sh_show 'git add core.txt'
sh_show 'git rm heart.txt old.txt'
sh_show 'git status --short'
sh_show 'git commit -m "改名冲突：统一用 core.txt"'
sh_show 'ls -1'
sh_show "git log -1 --format='%h %s  父亲有 %p'"

sub "E3 一边删文件、一边改文件（modify/delete 冲突）"
cd "$SB"; rm -rf e-md; qrun git init -b main e-md >/dev/null 2>&1
cd "$SB/e-md"
qrun git config user.name "学习者"; qrun git config user.email "learn@example.com"
w legacy.txt '旧代码第1行
旧代码第2行
旧代码第3行
'
w keep.txt '别碰我
'
qc "c1 共同起点"
git switch -q -c cleanup
qrun git rm legacy.txt
qc "cleanup: 删掉 legacy.txt"
git switch -q main
w legacy.txt '旧代码第1行
旧代码第2行 主线还在修
旧代码第3行
'
qc "main: 还在改 legacy.txt"
sh_fail 'git merge cleanup'
sh_show 'git status --short'
note "「由他们删除」= 对方把它删了，而你改了它。git 不敢自己决定，给你两个选择："
note "  要保留你的改动：git add legacy.txt（然后提交）"
note "  确认该删：git rm legacy.txt"
sh_show 'git add legacy.txt'
sh_show 'git status --short'
sh_show 'git commit -m "合并 cleanup，但 legacy.txt 我还要留着"'
sh_show 'cat legacy.txt'

sub "E4 图形化工具：git mergetool 的概念（沙箱里没装 meld，看它怎么告诉你）"
cd "$SB/e-md"
w conflict-demo.txt '甲
'
qrun git add -A; qrun git commit -qm "另一条改动" >/dev/null
git switch -q -c tooltest HEAD~1
w conflict-demo.txt '乙
'
qrun git add -A; qrun git commit -qm "tooltest 改了同一行" >/dev/null
git switch -q main
sh_fail 'git merge tooltest'
note "这是第四种形状：add/add（两边各自新加了同名文件）。暂存区里只有 :2: 和 :3:，【没有 :1:】："
sh_show 'git ls-files -u'
sh_show 'git status --short'
sh_show 'git mergetool --tool-help'
note "上面是 git 愿意告诉你的候选工具清单（没配 merge.tool 之前，mergetool 根本不肯干活）。"
note "现在给它配一个【假的合并工具】，让你亲眼看见 git mergetool 到底在做什么："
cat > "$SB/fake-merge.sh" <<'FAKE'
#!/usr/bin/env bash
# 假装自己是 meld：收到冲突文件路径，把三行冲突标记删掉，就算「解决了」
echo "[假工具] 我收到的冲突文件是：$1"
grep -v '^<<<<<<< \|^=======$\|^>>>>>>> ' "$1" > "$1.tmp"
mv "$1.tmp" "$1"
echo "[假工具] 标记行已删掉，退出码 0"
FAKE
chmod +x "$SB/fake-merge.sh"
qrun git config merge.tool mytool
qrun git config mergetool.mytool.cmd "$SB/fake-merge.sh \$MERGED"
qrun git config mergetool.mytool.trustExitCode true
qrun git config mergetool.noPrompt true
sh_show 'git config --get merge.tool'
sh_show 'git config --get mergetool.mytool.cmd'
sh_show 'git mergetool'
sh_show 'cat conflict-demo.txt'
sh_show 'git status --short'
note "★ 看清了吗：所谓 git mergetool，只是【把冲突文件交给一个外部命令去改写】而已。"
note '  git 在调用前把 $MERGED 替换成真正的冲突文件名；$BASE / $LOCAL / $REMOTE 分别是 :1: / :2: / :3: 三个版本落成的临时文件。'
note "  所以任何「三栏合并工具」（meld / vimdiff / VS Code）做的事，和这条 grep 一模一样：读文件、写文件，只是改成让你点鼠标。"
note "  工具退出码 0 + trustExitCode=true 时，git 就替你把这个文件 add 掉了 —— 但看上面的文件内容：甲乙两行都留下了，"
note "  这其实是个错误的解决结果。工具的退出码不代表你解决对了，别迷信它。"
sh_show 'git log -1 --format="%s"'
sh_show 'git status'
note "看 status：文件已经被工具 add 进去了（'所有冲突已解决'），但【合并还没提交】—— mergetool 从不替你 commit。"
note "多出来的 conflict-demo.txt.orig 是 git 留的原始副本，不想要就 git config mergetool.keepBackup false。"
sh_show 'git commit --no-edit'
sh_show 'git log --oneline --graph -3'

sub "E5 只看合并提交：git log --merges"
cd "$SB/b-auto"
sh_show 'git log --oneline --merges'
note "配合 --first-parent 看「主线都合进了哪些功能」，非常清爽："
sh_show 'git log --oneline --first-parent'

###############################################################################
say "F 收尾：万一你合并合坏了"
###############################################################################
note "正在演示：合并合坏了而且已经提交，怎么退回去。"
newrepo f-rescue
w a.txt '甲
'
qc "c1 main 第一条"
w b.txt '只有 main 有
'
qc "c2 main 第二条"
git switch -q -c bad
w c.txt '这个功能我还不想合进来
'
qc "c3 不该合的功能"
git switch -q main
w b.txt '只有 main 有
main 后来又动了一行
'
qc "c4 main 也动了（让合并快进不了）"
sh_show 'git log --oneline --graph --decorate --all'
note "手滑执行了 git merge bad —— 不该合的东西合进来了，而且已经自动生成合并提交："
sh_show 'git merge bad -m "手滑合并了 bad 分支"'
sh_show 'git log --oneline --graph --decorate'
sh_show 'ls -1'
note "注意 git 在合并前悄悄把你的位置记进了 ORIG_HEAD："
sh_show "cat .git/ORIG_HEAD"
sh_show 'git rev-parse --short ORIG_HEAD'
note "所以本地（没 push）最快的补救就是这条 —— ★ 但它会连工作区一起砸掉，看清 03 章再敲："
sh_show 'git reset --hard ORIG_HEAD'
sh_show 'git log --oneline --graph --decorate'
sh_show 'ls -1'
note "合并提交没了，c.txt 也没了，回到手滑之前。"
note "另一种更透明的看法（推荐先看清楚再动手）："
sh_show 'git reflog | head -5'
note "★ 如果这条合并【已经 push 出去了】，就不要 reset，改用 03 章的："
note "    git revert -m 1 <那条合并提交的 hash>"
note "  -m 1 保留第 1 个父亲（你原本的主线），把第 2 个父亲带进来的东西整体撤掉。"

###############################################################################
say "G ★★★ 合并之前的检查清单：先看清楚「要合的到底是什么」"
###############################################################################
note "现场和 A 段一模一样：feature/login 上有 2 条提交，main 也往前走了 1 条，两边分叉。"
newrepo g-checklist
w plan.md '第1行：项目说明
第2行：目标
第3行：分工
'
qc "c1 初始提交"
git switch -q -c feature/login
w login.js '登录页
'
qc "c2 加登录页"
w login.js '登录页
表单校验
'
qc "c3 加表单校验"
git switch -q main
w plan.md '第1行：项目说明
第2行：目标
第3行：分工
第4行：里程碑
'
qc "c4 main 往前走了一步"

sub "G1 第一步永远是形状"
sh_show 'git log --oneline --graph --decorate --all'
sh_show 'git branch --merged'
sh_show 'git branch --no-merged'

sub "G2 两点：对方有我没有 / 我有对方没有"
sh_show 'git log --oneline HEAD..feature/login'
sh_show 'git log --oneline feature/login..HEAD'
note "两个列表都非空 = 分叉了，这次合并一定造出合并提交（或者直接冲突）。"
note "一条命令同时看两边：< 是我独有，> 是对方独有。"
sh_show 'git log --oneline --left-right HEAD...feature/login'

sub "G3 三点与两点：想知道「对方改了啥」永远用三点"
sh_show 'echo "共同祖先 = $(git rev-parse --short "$(git merge-base HEAD feature/login)")"'
sh_show 'git diff --stat HEAD...feature/login'
sh_show 'git diff --stat HEAD..feature/login'
note "三点 = 从共同祖先出发比，只列【对方真正动过的】；两点 = 直接比两个顶端快照，"
note "  会把你自己在 plan.md 上加的那一行算成对方「删掉了一行」。"

sub "G4 能不能快进：问退出码，别看感觉"
sh_show 'git merge-base --is-ancestor HEAD feature/login; echo "退出码=$?"'
note "这条命令什么都不打印：0 = HEAD 是 feature/login 的祖先（能快进），1 = 分叉了（快进不可能）。"

sub "G5 试合：叠好、放进暂存区，但停在提交之前"
sh_show 'git merge --no-commit --no-ff feature/login'
sh_show 'git status'
sh_show 'git diff --cached --stat HEAD'
note "git diff --cached --stat HEAD = 「如果我现在提交，会多出什么」。看完不满意就 abort："
sh_show 'git merge --abort'
sh_show 'git log --oneline -1'
sh_show 'git status --short'
note "status --short 输出为空 = 现场完全复原，一个字节都没留下。"

###############################################################################
say "H 快进的物理证据：改一行文本，对象数纹丝不动"
###############################################################################
sub "H1 造一个「main 一步没动」的可快进现场，先把底层数字记下来"
newrepo h-ffobjects
w doc.md '第1行
第2行
第3行
'
qc "c1 初始"
git switch -q -c feature
w extra.txt '新功能
'
qc "c2 加 extra.txt"
w doc.md '第1行
第2行
第3行
第4行
'
qc "c3 改 doc.md"
git switch -q main
sh_show 'git log --oneline --graph --decorate --all'
sh_show 'git merge-base --is-ancestor HEAD feature; echo "退出码=$?"'
note "上面退出码 0 = 能快进。现在数一下底层有几个对象、分支文件里写着什么："
sh_show 'grep "" .git/refs/heads/*'
sh_show 'find .git/objects -type f | wc -l'

sub "H2 快进之后：分支文件换了字符串，对象数一个没多"
sh_show 'git merge feature'
sh_show 'grep "" .git/refs/heads/*'
sh_show 'find .git/objects -type f | wc -l'
note "★ 快进合并的全部成本 = 把 .git/refs/heads/main 里那行字符串改成 feature 的哈希。"
note "  没有新提交、没有新对象 —— 这也是「事后看不出这里合过东西」的物理原因。"

sub "H3 同一份形状改用 --no-ff：多出来的正好是那 1 个对象"
newrepo h-noffobjects
w doc.md '第1行
第2行
第3行
'
qc "c1 初始"
git switch -q -c feature
w extra.txt '新功能
'
qc "c2 加 extra.txt"
w doc.md '第1行
第2行
第3行
第4行
'
qc "c3 改 doc.md"
git switch -q main
sh_show 'find .git/objects -type f | wc -l'
sh_show 'git merge --no-ff feature -m "合并 feature"'
sh_show 'find .git/objects -type f | wc -l'
sh_show 'git cat-file -p HEAD'
sh_show 'echo "HEAD^1 = $(git rev-parse --short HEAD^1)  HEAD^2 = $(git rev-parse --short HEAD^2)"'
note "多出来的 1 个对象就是这条有两个 parent 行的提交（tree 直接复用 feature 那份，所以只多 1 个）。"

###############################################################################
say "I 「合并进行中」在磁盘上是什么，以及 --quit 的坑"
###############################################################################
newrepo i-merge-state
w config.ini '端口 = 3000
超时 = 30
'
qc "c1 初始配置"
git switch -q -c feature
w config.ini '端口 = 3000
超时 = 30
最大连接 = 100
线程 = 8
'
qc "feature: 线程 = 8"
git switch -q main
w config.ini '端口 = 9090
超时 = 30
最大连接 = 100
线程 = 4
'
qc "main: 线程 = 4，顺手把端口也改了"
sub "I1 撞一次冲突，然后看 .git 里多出来的那几个文件"
sh_fail 'git merge feature'
sh_show 'ls .git'
sh_show 'cat .git/MERGE_HEAD'
sh_show 'echo "MERGE_HEAD = $(git rev-parse --short MERGE_HEAD)   feature = $(git rev-parse --short feature)   HEAD = $(git rev-parse --short HEAD)"'
sh_show 'cat .git/MERGE_MSG'
note "MERGE_HEAD = 「对方是谁」，MERGE_MSG = 「你提交时默认给你的说明」，"
note "  MERGE_MODE = 记 --no-ff 这类开关，AUTO_MERGE = ort 写下来的工作区结果。"
note "★ git 没有记忆，「我正在合并中」这件事完全靠这几个文件存在与否。"

sub "I2 --quit：只删小抄，工作区和暂存区一个都不还原"
sh_show 'git merge --quit'
sh_show 'ls .git | grep -i merge || echo "（MERGE_* 文件已删除）"'
sh_show 'git status --short'
sh_show 'git ls-files -u'
note "标记行还在、三个 stage 还在，但 git 已经不认为你在合并中了 —— 于是 abort 也不肯帮你："
sh_fail 'git merge --abort'

sub "I3 自救：git reset 把三个 stage 砸回 HEAD（工作区那份带标记的文件仍然还在）"
sh_show 'git reset'
sh_show 'git ls-files -u'
sh_show 'git status --short'
sh_show 'cat config.ini'
note "看上面 cat 的最后四行：<<<<<<< ======= >>>>>>> 还在文件里 —— reset（mixed）只管暂存区，不管工作区。"
sh_show 'git checkout -- config.ini'
sh_show 'git status --short'
note "★ 结论：想放弃就只用 git merge --abort。--quit 是给「我知道我在干什么」的人留的逃生口。"

###############################################################################
say "J zdiff3：让冲突段里直接显示共同祖先"
###############################################################################
newrepo j-zdiff3
w config.ini '端口 = 3000
超时 = 30
'
qc "c1 初始配置"
git switch -q -c feature
w config.ini '端口 = 3000
超时 = 30
最大连接 = 100
线程 = 8
'
qc "feature: 线程 = 8"
git switch -q main
w config.ini '端口 = 9090
超时 = 30
最大连接 = 100
线程 = 4
'
qc "main: 线程 = 4，顺手把端口也改了"
sh_show 'git config --get merge.conflictStyle || echo "（没配，默认两段式）"'
sh_fail 'git -c merge.conflictStyle=zdiff3 merge feature'
sh_show 'cat config.ini'
sh_show 'git config --get merge.conflictStyle || echo "（跑完仍然是没配：-c 只活在这一条命令里）"'
note "||||||| 和 ======= 之间是空的 —— 空得有含义：共同祖先那里【根本没有这一段】，"
note "  两边都是往下新加的。要是 base 里有原值，||||||| 下面就照印那一行。"
sh_show 'git merge --abort'

echo
say "本章记住四句话"
echo "  1) 分支形状永远用 git log --oneline --graph --decorate --all 看，别看文字脑补。"
echo "  2) 合并三种结局：能快进就快进（--no-ff 可以强行留菱形）；不同地方就自动合；同一行就冲突。"
echo "  3) 冲突解决四步：看 <<<<<<< ======= >>>>>>> → 手工改 → git add（标记「我处理过了」）→ git commit。"
echo "     中途反悔：git merge --abort。"
echo "  4) ours/theirs 在 merge 和 rebase 里是反的 —— 分不清就用 git ls-files -u 和 :2:/:3: 亲眼验一次。"
echo
echo "全部演示完成。"
