#!/usr/bin/env bash
# 07 远程与多人协作 —— 全程在临时沙箱里跑，不联网、不碰你任何真实仓库
# 用法： bash demo.sh          跑完自动清理沙箱
#        bash demo.sh --keep   保留沙箱并打印路径，方便自己进去翻
#
# 说明：为了让输出可复现，脚本固定了提交时间，并且每条提交都用
#       -c user.name=学习者 -c user.email=learn@example.com 这个沙箱身份，
#       绝不读取也不写入任何机器上的真实 git 配置。屏幕上打印的命令省略了这两个 -c，
#       实际执行的就是带 -c 的版本。
set -euo pipefail

KEEP=0
[ "${1:-}" = "--keep" ] && KEEP=1

SB=$(mktemp -d)
cleanup() {
  if [ "$KEEP" = 1 ]; then
    printf '\n[--keep] 沙箱保留在：%s\n' "$SB"
    printf '         可以自己进去翻，例如：cd %s/s3/alice && git log --graph --oneline\n' "$SB"
  else
    rm -rf "$SB"
    printf '\n沙箱已清理：%s\n' "$SB"
  fi
}
trap cleanup EXIT

export LC_ALL=C LANG=C
# 屏蔽机器上的 git 配置：否则你 ~/.gitconfig 里的 pull.rebase / init.defaultBranch /
# advice.* 会混进输出，本文的围栏就复现不出来了（详见 README「运行 demo」一节）
export GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_SYSTEM=/dev/null GIT_PAGER=cat
export GIT_AUTHOR_DATE="2026-01-01T10:00:00 +0800"
export GIT_COMMITTER_DATE="2026-01-01T10:00:00 +0800"
GID='-c user.name=学习者 -c user.email=learn@example.com'
G="git $GID"

# 打印时把冗长的身份参数隐掉（真实执行的命令里是有这两个 -c 的）
pretty() { printf '%s' "$1" | sed "s/ $GID//g"; }

R() { # R <命令字符串>：打印命令，再原样打印它的 stdout+stderr，并报告非 0 退出码
  local out ec
  printf '\n$ %s\n' "$(pretty "$*")"
  out=$(eval "$*" 2>&1) && ec=0 || ec=$?
  printf '%s\n' "$out"
  if [ $ec -ne 0 ]; then printf '[命令退出码 = %s —— 上面的报错是故意让它发生的]\n' "$ec"; fi
  return 0
}
say()  { printf '\n=============== %s ===============\n' "$*"; }
note() { printf -- '--- %s\n' "$*"; }

commit_in() { # commit_in <目录> <文件名> <内容> <提交信息>
  ( cd "$1" && printf '%s\n' "$3" > "$2" && eval "$G add -A" && eval "$G commit -q -m \"$4\"" )
}

# 造「同一个 bare 远程 + 两个克隆 + 已经分叉」的现场：
#   alice 本地有 1 个没推的提交；bob 的提交已经在远程上；两边都从 init 长出来
mkpair() { # mkpair <目录名>
  local root="$SB/$1"
  mkdir -p "$root"
  git init --bare -q -b main "$root/origin.git"
  git init -q -b main "$root/seed"
  printf 'handbook\n' > "$root/seed/README.md"
  ( cd "$root/seed" && eval "$G add -A" && eval "$G commit -q -m \"init: 建仓库\"" \
      && eval "$G remote add origin $root/origin.git" && eval "$G push -q origin main" )
  eval "$G clone -q $root/origin.git $root/alice"
  eval "$G clone -q $root/origin.git $root/bob"
  commit_in "$root/alice" alice.txt "alice 的新功能" "alice: 我的功能提交"
  commit_in "$root/bob"   bob.txt   "bob 的改动"     "bob: 同事的紧急修复"
  ( cd "$root/bob" && eval "$G push -q origin main" )
}

say "0. 本次沙箱位置"
printf 'SB=%s\n接下来所有仓库都建在这里面，跑完自动删（除非加 --keep）。\n' "$SB"

# =========================================================
say "1. remote 只是一个『别名』：-v / show / set-url / rename / remove"
# =========================================================
note "第 1 步：沙箱里造一个 bare 仓库当『服务器上的仓库』（bare = 只有 .git 那些对象，没有工作区）"
R "git init --bare -q -b main $SB/origin.git && ls $SB/origin.git"
R "git --git-dir=$SB/origin.git rev-parse --is-bare-repository"
note "第 2 步：一个普通仓库，提交一点东西"
$G init -q -b main "$SB/seed"
printf 'handbook\n' > "$SB/seed/README.md"
R "cd $SB/seed && $G add -A && $G commit -qm 'init: 建仓库'"
note "第 3 步：remote add 只是起别名。origin 这三个字母没有任何魔法，叫 dog/backup/upstream 都行"
R "cd $SB/seed && $G remote add origin $SB/origin.git && $G remote -v"
R "cd $SB/seed && $G remote get-url origin"
R "cd $SB/seed && $G push -qu origin main"
note "第 4 步：remote -v 只会告诉你 URL，看不到对方有什么分支。想看清对方状态用 remote show"
R "cd $SB/seed && $G remote show origin"
note "第 5 步：仓库搬家 / http 换 ssh —— set-url 只改配置里那一行 URL，提交、分支、跟踪关系全都不动"
R "cd $SB/seed && $G remote set-url origin $SB/origin.git && $G remote -v"
note "第 6 步：改名。rename 会顺手把 remote.* 配置、branch.*.remote、以及 refs/remotes/* 一起改名"
R "cd $SB/seed && $G remote rename origin upstream && $G remote -v && $G show-ref"
R "cd $SB/seed && $G remote rename upstream origin && $G remote -v"
note "第 7 步：remove 只删别名。你的提交一个字节都不会少，随时可以再 add 回来"
R "cd $SB/seed && $G remote remove origin && $G remote -v && echo '(别名没了)'"
R "cd $SB/seed && $G log --oneline -1 && echo '(提交还在)'"
R "cd $SB/seed && $G remote add origin $SB/origin.git"

# =========================================================
say "2. 关键认知：fetch 不改你的工作区；refs/remotes/* 只是『我上次看到的对方状态』"
# =========================================================
note "alice 克隆下来，此刻本地 = 远程，完全一致"
eval "$G clone -q $SB/origin.git $SB/alice"
R "cd $SB/alice && $G log --oneline -3 && $G status"
note "同事（bob 目录模拟）改了代码并推上去 —— 注意 alice 此刻对此一无所知"
R "cd $SB && $G clone -q $SB/origin.git $SB/bob"
commit_in "$SB/bob" report.md "同事写的报告" "同事: 加了 report.md"
R "cd $SB/bob && $G push -q origin main && echo '(同事推送成功)'"
note "alice 视角：工作区文件、自己的分支，全都纹丝不动"
R "cd $SB/alice && $G log --oneline -3"
R "cd $SB/alice && $G status && ls"
note "fetch 干的事只有两件：把对方对象下载进本地对象库 + 把 refs/remotes/origin/main 这根快照指针往前挪"
R "cd $SB/alice && $G fetch"
R "cd $SB/alice && $G log --oneline -3 && echo '↑ 你的 main 一点没动'"
R "cd $SB/alice && $G log --oneline -3 origin/main && echo '↑ 远程快照已经前进'"
R "cd $SB/alice && $G show-ref"
note "远程跟踪引用在磁盘上就是 .git/refs/remotes/origin/main 这么个 41 字节的文本文件（和分支指针同一种东西）"
R "cat $SB/alice/.git/refs/remotes/origin/main"
note "于是可以随时只读对比：对方多了什么、我和他差在哪"
R "cd $SB/alice && $G log --oneline HEAD..origin/main"
R "cd $SB/alice && $G diff HEAD origin/main --stat"
note "真正把改动落进工作区的是 merge（或 rebase），fetch 从来不干这事"
R "cd $SB/alice && $G merge origin/main && ls && $G log --graph --oneline -3"

# =========================================================
say "3. upstream 与 -u：branch -vv 那三段怎么读，ahead/behind 是怎么算出来的"
# =========================================================
note "clone 时 git 顺手写好的跟踪关系（这就是『upstream』的全部内容）"
R "cd $SB/alice && git config --local --get-regexp '^branch\\.'"
R "sed -n '/^\[branch/,/^\$/p' $SB/alice/.git/config"
note "你自己再提交一个，于是本地和远程快照两边各有对方没有的提交"
commit_in "$SB/alice" todo.md "alice 的待办" "alice: 加 todo.md"
R "cd $SB/alice && $G branch -vv"
note "读法：[origin/main: ahead 1, behind 1] = 我跟踪的是 origin/main；我比它多 1 个提交；它比我多 1 个提交"
R "cd $SB/alice && $G status -sb"
note "ahead/behind 的算法就是『对称差集两头各数一遍』，自己数一次心里就有底了"
R "cd $SB/alice && git rev-list --left-right --count origin/main...main"
R "cd $SB/alice && echo '↑ 左边=origin/main 独有=behind，右边=main 独有=ahead'"
R "cd $SB/alice && echo -n 'ahead: ' && $G rev-list --count main ^origin/main && echo -n 'behind: ' && $G rev-list --count origin/main ^main"
R "cd $SB/alice && echo 'behind 的具体是哪些提交：' && $G log --oneline origin/main ^main"
note "划重点：behind 是按【本地那根快照指针】算的，不 fetch 就永远看不到对方新推的东西"
R "cd $SB/bob && echo '同事又改了' > extra.md && $G add -A && $G commit -qm '同事: 又改了一次' && $G push -q origin main && echo '(同事又推了一个，但你没 fetch)'"
R "cd $SB/alice && $G branch -vv && echo '↑ 数字完全没变，因为它压根不知道'"
R "cd $SB/alice && $G fetch -q && $G branch -vv && echo '↑ fetch 之后 behind 才对上'"
note "-u（--set-upstream）的全部作用：push 成功之后写两行配置，以后裸 git push / git pull 就知道去哪"
R "cd $SB/alice && $G switch -q -c feature/login && printf 'void login(){}\n' > login.c && $G add -A && $G commit -qm 'alice: 登录骨架' && $G branch -vv"
R "cd $SB/alice && $G push -u origin feature/login"
R "cd $SB/alice && git config --local --get-regexp '^branch\\.feature'"
R "cd $SB/alice && $G branch -vv"

# =========================================================
say "4. fetch / pull / pull --rebase：同一份分叉，能造出三种完全不同的历史形状"
# =========================================================
mkpair s4
cd "$SB/s4/alice"
A_HASH=$($G rev-parse main)          # alice 那个还没推的提交
note "分叉现场：你 1 个提交、同事 1 个提交，两个都从 init 长出来"
R "cd $SB/s4/alice && $G fetch -q && $G log --oneline --graph --all -5"
R "cd $SB/s4/alice && $G branch -vv"
note "形状 A：git pull（= fetch + merge）→ 多出一个 merge 提交，历史变菱形"
R "cd $SB/s4/alice && $G -c pull.rebase=false pull"
R "cd $SB/s4/alice && $G log --graph --oneline -5"
R "cd $SB/s4/alice && echo '这个 merge 提交有两个 parent，这就是『合并痕迹』：' && $G cat-file -p HEAD | head -5"
note "回到分叉现场（把 main 指针挪回你那个提交；对象和快照都还在）"
R "cd $SB/s4/alice && $G reset -q --hard $A_HASH && $G log --oneline --graph --all -5"
note "形状 B：git pull --rebase → 把你的提交摘下来重贴到同事提交后面，历史是一条直线"
R "cd $SB/s4/alice && $G pull --rebase"
R "cd $SB/s4/alice && $G log --graph --oneline -5"
note "注意：你那个提交的哈希变了（内容一样但 parent 变了，所以是新对象）—— 这就是『rebase 会改写历史』"
R "cd $SB/s4/alice && echo '重贴前的提交：' && $G log --oneline -1 $A_HASH && echo '重贴后的提交：' && $G log --oneline -1 HEAD && $G reflog -4"
note "形状 C：git fetch 之后自己看清楚，再手动 merge（日常最推荐：看清楚再动手）"
R "cd $SB/s4/alice && $G reset -q --hard $A_HASH && $G merge origin/main"
R "cd $SB/s4/alice && $G log --graph --oneline -5"
note "那能不能直接 git pull 让它自己看着办？新版 git（2.34 起）在分叉时拒绝替你决定："
R "cd $SB/s4/alice && $G reset -q --hard $A_HASH && $G pull"
note "结论：想自动 merge 必须显式 pull.rebase=false（或 git pull --no-rebase），否则它就停下来问你"

# =========================================================
say "5. push 的各种形态 + push.default 的三种行为"
# =========================================================
R "cd $SB/s4/alice && $G reset -q --hard $A_HASH && $G log --oneline -2 && echo '↑ 你本地；下面看远程：' && git --git-dir=$SB/s4/origin.git log --oneline -2"
note "裸 git push：完全靠 upstream 配置决定推到哪个远程分支（等价于 git push origin 当前分支）"
R "cd $SB/s4/alice && $G -c pull.rebase=false pull -q && $G push 2>&1 | tail -3"
note "显式写法 git push <remote> <本地分支>:<远程分支>，冒号左边本地、右边远程，两边名字可以不一样"
R "cd $SB/s4/alice && $G push origin main:main-backup && $G ls-remote --heads origin"
note "推所有本地分支 / 推所有 tag —— 团队仓库里 --all 很容易把一堆临时分支甩出去，慎用（下面就是事故现场）"
R "cd $SB/s4/alice && $G branch wip/随手试验 && $G switch -q -c wip/另一件事 && printf '草稿\n' > scratch.txt && $G add -A && $G commit -qm 'wip: 草稿' && $G switch -q main && $G tag v5.0-只在这台机器上"
R "cd $SB/s4/alice && $G branch && echo '↑ 本地有这些分支，然后看 --all 会把它们全甩出去：'"
R "cd $SB/s4/alice && $G push --all origin"
R "cd $SB/s4/alice && $G ls-remote --heads origin && echo '↑ wip 垃圾分支已经躺在服务器上了'"
R "cd $SB/s4/alice && $G push --tags origin 2>&1 | tail -3"
note "删远程分支：长写法（好读）和短写法（冒号左边留空 = 把『什么都没有』推上去）其实是同一件事"
R "cd $SB/s4/alice && $G push origin --delete main-backup"
R "cd $SB/s4/alice && $G push origin --delete wip/随手试验 && echo '↑ 长写法，推荐'"
R "cd $SB/s4/alice && $G push origin :wip/另一件事 && echo '↑ 短写法：冒号左边空 = 删除远程同名分支'"
R "cd $SB/s4/alice && $G ls-remote --heads origin && $G branch -vv && echo '↑ 远程分支没了，本地分支还在（要删本地得再 git branch -D）'"
note "push.default 实测。先造一个『本地分支名 != 远程分支名』的怪现场："
git init --bare -q -b main "$SB/pd.git"
$G init -q -b main "$SB/pdseed"; printf 'x\n' > "$SB/pdseed/f.txt"
( cd "$SB/pdseed" && eval "$G add -A" && eval "$G commit -qm 'start'" && eval "$G remote add origin $SB/pd.git" && eval "$G push -q origin main" )
eval "$G clone -q $SB/pd.git $SB/pdx"
( cd "$SB/pdx" && $G switch -q -c mywork && printf 'w\n' > w.txt && eval "$G add -A" && eval "$G commit -qm 'mywork 提交'" \
    && eval "$G push -q origin mywork:other-name" && git config branch.mywork.remote origin \
    && git config branch.mywork.merge refs/heads/other-name )
R "cd $SB/pdx && $G branch -vv && echo '↑ 本地 mywork 的 upstream 是 origin/other-name（名字不一致）'"
R "cd $SB/pdx && echo '### A) push.default=simple（现在的默认）—— 名字不一致就拒绝，逼你写明白'"
R "cd $SB/pdx && $G -c push.default=simple push"
R "cd $SB/pdx && echo '### B) push.default=current —— 无视 upstream，本地叫什么就推到远程同名分支'"
R "cd $SB/pdx && $G -c push.default=current push && $G ls-remote --heads origin"
R "cd $SB/pdx && echo '### C) push.default=upstream —— 顺着 upstream 推，哪怕远程分支名不同'"
R "cd $SB/pdx && $G -c push.default=upstream push"
note "simple 为什么成了默认：它是『current 的安全性 + upstream 的直观』的折中——"
note "只有在你跟踪的远程分支名恰好和本地不同时才拒绝，防止一不留神推到别人的分支上。"

# =========================================================
say "6. tag：轻量 vs 注解 —— cat-file -t 直接证明它俩不是同一种对象"
# =========================================================
cd "$SB/s4/alice"
R "cd $SB/s4/alice && $G checkout -q main && $G tag v0.1-light && $G tag -a v1.0 -m '第一个正式版' && $G tag --sort=creatordate"
R "cd $SB/s4/alice && echo '### 对象类型：轻量 tag = 一个直接指向 commit 的引用；注解 tag = 独立的 tag 对象' && $G cat-file -t v0.1-light && $G cat-file -t v1.0"
R "cd $SB/s4/alice && $G rev-parse v0.1-light v1.0 v1.0^{} && echo '↑ 注解 tag 要加 ^{} 才拿到 commit；轻量 tag 两个写法一样'"
R "cd $SB/s4/alice && echo '### 注解 tag 对象里存的内容（谁、什么时候、说了什么、指向谁）：' && $G cat-file -p v1.0"
R "cd $SB/s4/alice && echo '### tag -n 列注解内容，轻量 tag 那行是空的：' && $G tag -n"
R "cd $SB/s4/alice && echo '### 一次看全：名字 / 对象类型 / 指向的 commit' && $G tag --format='%(refname:short) %(objecttype) -> %(*objectname:short)'"
note "push 默认【不带】tag，这是很多人『我打了 tag 同事却说没有』的原因"
R "cd $SB/s4/alice && $G push origin v1.0 && $G ls-remote --tags origin"
R "cd $SB/s4/alice && $G push origin v0.1-light 2>&1 | tail -2 && $G ls-remote --tags origin"
note "删 tag：本地删用 tag -d，远程删必须再 push 一次（两种写法都能删）"
R "cd $SB/s4/alice && $G push origin --delete v0.1-light"
R "cd $SB/s4/alice && $G tag -d v0.1-light"
note "describe：报出『离最近的注解 tag 有多远』，格式 = tag名-落后提交数-g短哈希；CI 打版本号常用"
note "另建一个小仓库专门演 describe：它历史里没有 merge 提交 —— merge 提交的默认信息带远程 URL，"
note "URL 里有随机沙箱名，所以那种提交的哈希每跑一次都变，describe 末尾的短哈希也跟着变。"
git init -q -b main "$SB/dtag"
( cd "$SB/dtag" && printf 'app\n' > app.c && eval "$G add -A" && eval "$G commit -qm '初始版本'" \
    && eval "$G tag -a v1.0 -m '第一个正式版'" \
    && printf 'one\n' > one.c && eval "$G add -A" && eval "$G commit -qm 'tag 后的第一个提交'" \
    && printf 'two\n' > two.c && eval "$G add -A" && eval "$G commit -qm 'tag 后的第二个提交'" )
R "cd $SB/dtag && $G log --oneline -3 && echo '↑ v1.0 打在最底下那个提交上，上面还有 2 个提交'"
R "cd $SB/dtag && $G describe --tags"
R "cd $SB/dtag && $G describe --tags --abbrev=0 && echo '↑ --abbrev=0 只要 tag 名，CI 里取版本号常用'"
R "cd $SB/dtag && echo '临时改动' >> two.c && $G describe --tags --dirty && $G checkout -q -- two.c"
git init -q -b main "$SB/desc"
( cd "$SB/desc" && printf 'a\n' > a.txt && eval "$G add -A" && eval "$G commit -qm '第一个提交'" \
    && eval "$G tag v0-light" && printf 'b\n' > b.txt && eval "$G add -A" && eval "$G commit -qm '第二个提交'" )
R "cd $SB/desc && echo '### 这个仓库只有轻量 tag v0-light：' && $G describe"
R "cd $SB/desc && $G describe --tags && echo '↑ 加 --tags 才肯用轻量 tag'"
R "cd $SB/desc && $G tag -a v0-annotated -m '注解版' && $G describe && echo '↑ 有注解 tag 之后 describe 直接就能用了'"

# =========================================================
say "7. 双人协作完整实战：non-fast-forward 被拒 + 三条出路各自的后果"
# =========================================================
say "7.1 出路一：git pull（merge）—— 同事提交保住了，代价是历史里多一个菱形"
mkpair s1
R "cd $SB/s1/alice && $G push"
note "上面那句英文报错是全书最该背下来的一条：Updates were rejected because the remote contains work that you do not have locally"
note "人话：远程那一根指针已经往前挪了，你手上这条历史不是它的『下一代』，push 会丢掉一段历史，所以 git 拒绝。"
R "cd $SB/s1/alice && $G -c pull.rebase=false pull 2>&1 | tail -5"
R "cd $SB/s1/alice && $G log --graph --oneline -5"
R "cd $SB/s1/alice && $G push 2>&1 | tail -2"
R "cd $SB/s1/alice && echo '### 服务器上的历史（直接从 bare 仓库看，最诚实的视角）：' && git --git-dir=$SB/s1/origin.git log --graph --oneline -5"
R "cd $SB/s1/bob && $G fetch -q && echo '### 同事 fetch 之后看到的远程（他的本地 main 一点没动，status 里 behind 2）：' && $G status -sb && $G log --graph --oneline origin/main -5"

say "7.2 出路二：git pull --rebase —— 同事提交保住了，历史还是一条直线（多数人偏好）"
mkpair s2
R "cd $SB/s2/alice && $G pull --rebase 2>&1 | tail -4"
R "cd $SB/s2/alice && $G log --graph --oneline -4"
R "cd $SB/s2/alice && $G push 2>&1 | tail -2"
R "cd $SB/s2/alice && echo '### 服务器上的历史：没有菱形' && git --git-dir=$SB/s2/origin.git log --graph --oneline -4"
R "cd $SB/s2/bob && $G fetch -q && echo '### 同事 fetch 之后看到的远程：' && $G log --graph --oneline origin/main -4"

say "7.3 出路三：git push -f —— 直接抹掉同事的提交。这是事故，不是解法"
mkpair s3
R "cd $SB/s3/alice && echo '### 先试安全的 --force-with-lease：它要求远程还停在我上次 fetch 到的位置' && $G push --force-with-lease"
note "报错里的 (stale info) 就是 lease 生效的证据：你本地的快照落后于远程真实位置，git 不肯替你按下抹除键。"
R "cd $SB/s3/alice && echo '### 现在换成裸 -f：git 不再拦你，同事的提交当场从远程历史里消失' && $G push -f 2>&1 | tail -3"
R "cd $SB/s3/alice && echo '### 远程 main 现在长这样：' && git --git-dir=$SB/s3/origin.git log --oneline -4"
BOB=$(git -C "$SB/s3/bob" rev-parse main)
R "cd $SB/s3/alice && echo \"### 同事的提交 $BOB 还在远程 main 上吗：\" && git --git-dir=$SB/s3/origin.git merge-base --is-ancestor $BOB refs/heads/main && echo '还在' || echo '不在了（退出码非 0）—— 他被覆盖了'"
R "cd $SB/s3/bob && echo '### 同事那边的视角：' && $G fetch -q && $G status -sb && $G log --graph --oneline -4"
note "好消息：对象没被物理删除（还没 gc），所以能救。正确救法是『谁的东西谁负责』——让同事基于新历史重贴自己的提交"
R "cd $SB/s3/alice && echo '### 为什么 git 拦不住你？因为你本地连同事那个对象都没有（你从没 fetch 过它）：' && $G cat-file -t $BOB"
R "cd $SB/s3/bob && $G rebase origin/main 2>&1 | tail -3 && $G log --graph --oneline -4 && $G push -q origin main && echo '(同事重新推上去了)'"
R "cd $SB/s3/alice && $G fetch -q && echo '### 你这边 fetch 之后就能看到同事的提交又回到远程了：' && $G log --graph --oneline origin/main -4"
note "如果服务器开了 reflog（bare 仓库默认不开！），运维还能从服务端把被覆盖的那个哈希捞回来："
ALICE=$(git -C "$SB/s3/alice" rev-parse main)
git init --bare -q -b main "$SB/serverlog.git"
R "git --git-dir=$SB/serverlog.git config core.logAllRefUpdates true && git --git-dir=$SB/serverlog.git config --get core.logAllRefUpdates"
( cd "$SB/s3/bob" && eval "$G push -q $SB/serverlog.git $BOB:refs/heads/main" )
( cd "$SB/s3/alice" && eval "$G push -fq $SB/serverlog.git $ALICE:refs/heads/main" )
R "git --git-dir=$SB/serverlog.git log --oneline -3 && echo '↑ 服务器上 main 已经是你的提交了，同事那个不见了'"
R "git --git-dir=$SB/serverlog.git reflog | head -6"
R "cd $SB/s3/alice && echo '### 但 reflog 里 main@{1} 就是被覆盖掉的那个提交：' && git --git-dir=$SB/serverlog.git rev-parse 'main@{1}' && echo \"### 它正好等于同事的 $BOB\" && echo '### 于是运维一条命令就能把服务器指回去'"
R "git --git-dir=$SB/serverlog.git update-ref refs/heads/main 'main@{1}' && git --git-dir=$SB/serverlog.git log --oneline -3"
R "cd $SB/s3/alice && echo '### 反过来，s3/origin.git 是默认创建的 bare 仓库，没开 reflog → 服务端什么都查不到：' && git --git-dir=$SB/s3/origin.git reflog"

# =========================================================
say "8. 工作流：主干开发 / GitHub Flow / GitFlow，以及 PR 到底是个什么东西"
# =========================================================
note "GitHub Flow / 主干开发：长期分支只有 main，其余全是活不过几天的短分支，靠 PR 落地"
git init --bare -q -b main "$SB/flow.git"
$G init -q -b main "$SB/floww"; printf 'app\n' > "$SB/floww/app.c"
( cd "$SB/floww" && eval "$G add -A" && eval "$G commit -qm 'main: 初始版本'" && eval "$G remote add origin $SB/flow.git" && eval "$G push -q origin main" )
eval "$G clone -q $SB/flow.git $SB/floww2"
R "cd $SB/floww2 && $G switch -q -c feature/dark-mode && echo '(开一条短分支，之后所有改动都提交在它上面)'"
commit_in "$SB/floww2" theme.c "dark" "feat: 深色模式骨架"
commit_in "$SB/floww2" theme.c "dark + toggle" "feat: 加开关"
commit_in "$SB/floww2" theme.c "dark + toggle + css" "fix: 改 css 笔误"
R "cd $SB/floww2 && $G log --graph --oneline -4"
note "『提 PR』在 git 层面只有两步：① 把分支推到远程 ② 请人评审。评审通过后的落地动作才是合并"
R "cd $SB/floww2 && $G push -u origin feature/dark-mode 2>&1 | tail -3"
R "cd $SB/floww2 && $G fetch -q && echo '### 评审者（这里你自己扮演）在本地看到的：' && $G log --oneline main..origin/feature/dark-mode && $G diff main...origin/feature/dark-mode"
note "落地方式一：merge --no-ff —— 保留分支形状，能看出『这三件事属于同一个功能』"
R "cd $SB/floww2 && $G checkout -q main && $G merge --no-ff -m 'Merge branch feature/dark-mode (#128)' origin/feature/dark-mode 2>&1 | tail -3 && $G log --graph --oneline -6"
note "落地方式二：squash —— 整条分支压成 1 个提交。主干历史变成『1 个提交 = 1 件事』，回滚只需 revert 一个提交"
( cd "$SB/floww2" && $G reset -q --hard HEAD~1 )
R "cd $SB/floww2 && $G merge --squash origin/feature/dark-mode 2>&1|tail -2 && $G commit -qm 'feat: 深色模式 (#128)' && $G log --graph --oneline -4"
R "cd $SB/floww2 && $G push -q origin main && echo '### 服务器上的主干：干净得像一条直线' && git --git-dir=$SB/flow.git log --oneline -3"
note "为什么很多公司强制 squash：① 主干历史可读（每条 = 一个需求/一个 bug）② revert 一个功能不用处理一堆中间提交"
note "③ 分支里那些『改笔误』『调试打印』的噪音提交不会污染主干 ④ PR 编号写进提交信息，反查很方便"
R "cd $SB/floww2 && $G branch -a"
note "GitFlow：在 main 之外再多两条长期分支 develop（日常集成）和 release/*（发版准备），hotfix 从 main 拉出、合回 main 也合回 develop"
git init --bare -q -b main "$SB/gf.git"
$G init -q -b main "$SB/gfw"; printf 'v1\n' > "$SB/gfw/app.c"
( cd "$SB/gfw" && eval "$G add -A" && eval "$G commit -qm 'main: 起点'" && eval "$G remote add origin $SB/gf.git" \
    && eval "$G push -q origin main" && eval "$G branch develop" && eval "$G push -q origin develop" )
eval "$G clone -q $SB/gf.git $SB/gf2"
R "cd $SB/gf2 && $G switch -q -c feature/search origin/develop && printf 's\n' > search.c && $G add -A && $G commit -qm 'feat: 搜索'"
commit_in "$SB/gf2" search.c "s+index" "feat: 建索引"
R "cd $SB/gf2 && $G switch -q develop && $G merge -q --no-ff feature/search -m 'Merge feature/search into develop' && $G log --oneline -2"
R "cd $SB/gf2 && $G switch -q -c release/1.0 develop && printf '1.0.0\n' > VERSION.txt && $G add -A && $G commit -qm 'chore: 版本号 1.0.0'"
R "cd $SB/gf2 && $G switch -q main && $G merge -q --no-ff release/1.0 -m 'Release 1.0.0' && $G tag -a v1.0.0 -m '发布 1.0.0'"
R "cd $SB/gf2 && $G switch -q develop && $G merge -q main -m 'Back-merge release 1.0 into develop'"
R "cd $SB/gf2 && $G switch -q -c hotfix/1.0.1 main && printf 'bugfix\n' > app.c && $G add -A && $G commit -qm 'fix: 修一个线上崩溃'"
R "cd $SB/gf2 && $G switch -q main && $G merge -q --no-ff hotfix/1.0.1 -m 'hotfix 1.0.1' && $G tag v1.0.1"
R "cd $SB/gf2 && $G switch -q develop && $G merge -q main -m 'hotfix 同步回 develop'"
R "cd $SB/gf2 && $G log --graph --oneline --all -16"
R "cd $SB/gf2 && $G branch -a && $G tag"

printf '\n=== demo 结束：以上每一段 \$ 命令和它下面的输出，都是刚刚在这台机器上真跑的 ===\n'
