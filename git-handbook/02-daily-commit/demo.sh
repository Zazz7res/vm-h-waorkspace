#!/usr/bin/env bash
# ============================================================================
# 第 02 章《日常提交闭环》—— 整章证据一键复现
#
#   bash demo.sh          跑完自动删沙箱（默认）
#   bash demo.sh --keep   跑完保留沙箱并打印路径，方便自己进去敲
#
# 安全说明：
#   * 所有写操作都发生在 mktemp 出来的临时沙箱里，绝不碰你机器上任何真实目录
#   * 不读也不写你的 ~/.gitconfig（GIT_CONFIG_GLOBAL 被指向 /dev/null）
#   * 唯一一条不带身份的 git commit 会让 git 拿「本机登录名@主机名」凑邮箱，
#     那条输出经过 mask_id 脱敏（换成 <本机登录名>@<本机主机名>），屏幕上不留真实机器信息
#   * 全程不联网：需要"远程"时用沙箱里的本地 bare 仓库冒充 origin
#   * 提交身份统一用 -c 传，时间戳固定，所以哈希可复现
#     唯一例外是场景 4 的 nomit 仓库：那里要演的是"身份有了但提交没成"，
#     所以用 git config --local 把身份写进沙箱仓库自己的 .git/config（绝不 --global）
# ============================================================================
set -euo pipefail

KEEP=0
for arg in "$@"; do
    case "$arg" in
        --keep) KEEP=1 ;;
        -h|--help) echo "用法: bash demo.sh [--keep]"; exit 0 ;;
        *) echo "未知参数：$arg（本脚本只认 --keep）" >&2; exit 2 ;;
    esac
done

# 固定时间戳：同样的命令序列必然得到同样的哈希
export GIT_AUTHOR_DATE="2026-01-01T10:00:00 +0800"
export GIT_COMMITTER_DATE="2026-01-01T10:00:00 +0800"
# 屏蔽用户自己的全局/系统配置（不改任何配置文件，只是这一进程的视角）
export GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_SYSTEM=/dev/null
# 固定成英文输出，方便和 README 对照（你本机装了中文语言包，不设就是中文）
export LC_ALL=C.UTF-8
export GIT_PAGER=cat
export GIT_MERGE_AUTOEDIT=no

SB=$(mktemp -d "${TMPDIR:-/tmp}/git02-XXXXXX")
cleanup() {
    if [ "$KEEP" -eq 1 ]; then
        printf '\n[沙箱已保留] %s\n  进去自己敲： ls "%s"，再 cd 进 daily / loglab / oops / iolab / w / w2 / patchlab / nomit / declab / rmcache 任意一个\n  看完自己删： rm -rf "%s"\n' "$SB" "$SB" "$SB"
    else
        rm -rf "$SB"
        printf '\n[沙箱已删除] %s\n' "$SB"
    fi
}
trap cleanup EXIT

# 提交身份：只用 -c 传，绝不写进任何配置文件
ID='-c user.name="学习者" -c user.email="learn@example.com"'

# ---- 打印小工具：让每一行输出都能看懂是在演示什么 ---------------------
scene() { printf '\n\n======== 场景 %s ========\n' "$*"; }
say()   { printf '\n--- %s\n' "$*"; }
raw()   { printf '\n$ %s\n' "$*"; bash -c "$*"; }
g()     { printf '\n$ git %s\n' "$*"; bash -c "git $*"; }
gc()    { printf '\n$ git %s %s\n' "$ID" "$*"; bash -c "git $ID $*"; }
go()    { printf '\n$ cd %s\n' "${2:-${1:-.}}"; cd "$SB/${1:-.}"; }
# grc：跑一条可能"失败"的命令，顺便把退出码打出来（很多 git 判断就靠退出码）
grc()   { printf '\n$ %s\n' "$*"; local rc=0; bash -c "$*" || rc=$?; printf '[退出码 %s]\n' "$rc"; }
# gf：故意跑一条会失败的命令，只为看 git 的报错措辞
gf()    { printf '\n$ %s   （这条故意会失败，就为了看 git 怎么说）\n' "$*"; bash -c "$*" || true; }
# 脱敏：git 凑不出身份时会把【本机登录名@主机名】打进报错里，屏幕上不能留真实机器信息
LU=$(id -un 2>/dev/null || printf '')
LH=$(hostname 2>/dev/null || uname -n 2>/dev/null || printf '')
if [ -n "$LU" ]; then
    mask_id() { sed -e "s/${LU}@${LH}/<本机登录名>@<本机主机名>/g" -e "s/${LU}/<本机登录名>/g"; }
else
    mask_id() { cat; }
fi
# grcm：和 grc 一模一样，只多一层脱敏；只用在会打印自动身份的那条命令上
grcm()  { printf '\n$ %s\n' "$*"; local rc=0
          bash -c "$*" 2>&1 | mask_id || rc=$?
          printf '[退出码 %s]\n' "$rc"
          printf '[上面 fatal 里的 <本机登录名>@<本机主机名> 是脚本换掉的占位符，其余原文]\n'; }

printf '沙箱：%s\n' "$SB"
cd "$SB"

# ============================================================================
scene "1：建仓库的两条路 —— init 与（本地）clone"
# ============================================================================
say "1a. git init：从零建一个空仓库"
raw "git init -b main newrepo"
raw "ls -a newrepo"

say "1b. 不联网冒充一个远程：先造普通仓库 seed，再复制成 bare 仓库 origin.git"
raw "git init -q -b main seed"
go seed
raw "echo 'hello from origin' > app.txt"
g add app.txt
gc 'commit -m "origin 的第一个提交"'
g 'log --oneline'
go . '<沙箱根>'
raw "git clone --bare seed origin.git"
raw "ls origin.git"
say "bare 仓库里没有工作区，所以看不见 .git 目录 —— 它整个就是 .git"

say "1c. git clone：从本地 bare 仓库拉一个工作副本"
raw "git clone origin.git work"
go work
raw "ls -a"
say "clone 出来的仓库，status 自带远程信息（init 出来的不会有这行）"
g status
g remote -v
raw "cat .git/config"
say "多了一个 origin/main：那是远程分支在本地的只读快照"
g 'log --oneline --decorate --all'
g 'branch -a'
say "上面输出的区别只有 * ：* 标的是 HEAD 当前所在分支，remotes/ 开头的是快照"

say "1d. 加餐：那句 up to date 会怎么变脏（远程还是沙箱里的本地 bare，全程不联网）"
go . '<沙箱根>'
raw "git init -q --bare -b main update.git"
raw "git init -q -b main w"
go w
raw "echo '第一条' > note.txt"
g add note.txt
gc 'commit -m "W1 本地第一条"'
raw "git remote add origin $SB/update.git"
grc "git push -u origin main   # 远程那边还没有 main，所以是 [new branch]"
say "刚推完：本地和快照指的是同一条，## 后面什么都不带"
g "status -sb"
say "本地再走一条提交 → [ahead 1]"
raw "echo '+本地第二条' >> note.txt"
gc 'commit -qam "W2 本地又走了一条"'
g "status -sb"
say "现在冒充别人：另开一份 w2，从同一个 origin 推一条上去"
go . '<沙箱根>'
raw "git clone -q update.git w2"
go w2
raw "echo '别人那条' > other.txt"
g add other.txt
gc 'commit -m "O1 别人先推了一步"'
grc "git push -q origin main   # -q 静默：推成功时一个字都不打"
say "回到 w —— 关键对照：不 fetch，你永远看不见别人那条"
go w '<w 目录>'
g "status -sb   # w 还没 fetch"
g "fetch -q origin"
g "status -sb   # fetch 之后才看得见落后"
say "中间只隔了一条 git fetch：origin/main 只是本地存的快照，Git 不会替你自动 fetch"


# ============================================================================
scene "2：git status -s 短格式，每个字母都要认得"
# ============================================================================
go . '<沙箱根>'
raw "git init -q -b main daily"
go daily
say "先建 7 个文件提交一次，作为干净的基线"
raw "echo '内容-两边都改' > both.txt"
raw "echo '内容-暂存删' > del_staged.txt"
raw "echo '内容-工作区删' > del_worktree.txt"
raw "echo '内容-工作区改' > modified.txt"
raw "echo '内容-只暂存' > stagedonly.txt"
raw "echo '内容-要改名' > ren.txt"
raw "printf '*.tmp\n' > .gitignore"
raw "echo '垃圾' > junk.tmp"
g add -A
gc 'commit -m "基线：7 个文件"'
g "status -s"
say "基线是干净的：status -s 什么都不输出"

say "现在一口气造出 8 种状态（注释里是预期的短格式）"
raw "echo '+1' >> both.txt && git add both.txt && echo '+2' >> both.txt"
g 'rm del_staged.txt'
raw "rm del_worktree.txt"
raw "echo '+工作区' >> modified.txt"
raw "echo '新文件' > fresh.txt && git add fresh.txt && rm fresh.txt"
raw "echo '+暂存' >> stagedonly.txt && git add stagedonly.txt"
g 'mv ren.txt ren_new.txt'
raw "echo '没人管' > brandnew.txt"
say "短格式：一次看全"
g "status -s"
say "同一现场的长格式说法"
g status
say "被 .gitignore 拦住的文件默认彻底不出现，加 --ignored 才看得见（!!）"
g "status -s --ignored"

# ============================================================================
scene "3：add 家族 —— add / add -A / add -u / add -p（可脚本化）"
# ============================================================================
say "先把场景 2 的脏现场提交掉，免得噪音干扰后面"
g add -A
gc 'commit -m "收拾场景 2 的现场"'
raw "seq 1 14 | sed 's/^/line /' > app.c"
g add app.c
gc 'commit -m "app.c 14 行"'

say "第 2 行是真修复，第 12 行是随手加的调试打印：只想提交前者"
raw "sed -i '2s|.*|line 2 重要修复|;12s|.*|line 12 临时调试|' app.c"
g 'diff -- app.c'
say "git add -p：逐块确认。这里把答案用管道喂进去（y=暂存这块，n=先不要）"
printf '\n$ printf "y\\nn\\n" | git add -p -- app.c\n'
printf 'y\nn\n' | git add -p -- app.c
say "暂存区里只剩第一块（--cached 和 --staged 是同一个意思的两种写法）"
g 'diff --cached -- app.c'
say "工作区里第二块还在，没被暂存"
g 'diff -- app.c'
say "status 一行的两列：MM = 暂存区有变化、工作区也有变化"
g "status -s -- app.c"
say "★ 上面三处 index 行的哈希：add 过的那版是真对象，工作区那版此刻根本不在 objects 里"
grc "git cat-file -t d9810e1   # git diff --cached 的第二列：add -p 真写进去的"
grc "git cat-file -t 8991395   # git diff 的第二列：工作区那版，diff 只是现场算给你看"
say "把当前工作区内容喂给 hash-object（不带 -w 就只算哈希、不写盘），得到的正是 8991395 那个全哈希"
raw "git hash-object --stdin < app.c"
say "于是这次提交只带上第 2 行"
gc 'commit -m "只提交第 2 行的重要修复"'
g "status -s"
say "调试那行留在工作区，等真改完再处理"
g "diff -- app.c"

say "加餐：add -p 的键不止 y/n —— 三处改动挨得近会被并成一个 hunk，这时问句方括号里多一个 s"
go . '<沙箱根>'
raw "git init -q -b main patchlab"
go patchlab
raw "printf 'L1\nL2\nL3\nL4\nL5\nL6\nL7\nL8\nL9\n' > multi.txt"
g add multi.txt
gc 'commit -m "multi.txt 基线 9 行"'
raw "sed -i '2s|.*|L2 真修复|;4s|.*|L4 临时调试|;6s|.*|L6 也想留下|' multi.txt"
say "先问一次 ?：git 把这一轮真能按的键连含义一起打出来；问完按 q，什么都没暂存"
printf '\n$ printf "?\\nq\\n" | git add -p -- multi.txt\n'
printf '?\nq\n' | git add -p -- multi.txt
g "status -s -- multi.txt"
say "这一次喂的按键序列是 s y n y：s 把整个 hunk 拆成 3 块，再逐块决定"
printf '\n$ printf "s\\ny\\nn\\ny\\n" | git add -p -- multi.txt\n'
printf 's\ny\nn\ny\n' | git add -p -- multi.txt
say "拆完逐块回答的结果：暂存区里只留下 L2、L6 两处，L4 那行在暂存区里还是原样"
g 'diff --cached -- multi.txt'
g "status -s -- multi.txt"
go daily
say "回到 daily，接着比 add 家族的三种收法"

say "add -u / add . / add <file> 到底差在哪"
raw "echo '未跟踪的新文件' > untracked.txt"
raw "echo '+改动' >> app.c"
raw "rm -f stagedonly.txt"
g "status -s"
g add -u
g "status -s"
say "add -u 只处理已跟踪文件（改动 + 删除），untracked.txt 它不收"
g add .
g "status -s"
say "add . 才把 untracked.txt 收进来"
g reset -q
say "git reset（不带 --hard）= 暂存区整个退回，工作区一个字都不动"
g "status -s"
g add -A
gc 'commit -m "把现场提交干净：untracked.txt 进仓库，stagedonly.txt 删掉"'
g "status -s"

# ============================================================================
scene "4：commit 家族 —— -m / -am / --amend"
# ============================================================================
say "陷阱：commit -am 只覆盖已跟踪文件，未跟踪的新文件它带不上"
raw "echo '我是新文件' > brand_new.txt"
raw "echo '+改动' >> app.c"
g "status -s"
gc 'commit -am "改了 app.c（本来还想带上 brand_new.txt，带不上）"'
g "status -s"
say "正确做法：新文件先 add 一次，之后才归 -am 管"
g add brand_new.txt
gc 'commit -m "brand_new.txt 这次进来了"'
g "status -s"

say "--amend：替换掉最后一次提交（说明和内容都能改）"
g 'log --oneline -2'
raw "echo '刚才忘了写这句' >> brand_new.txt"
g add brand_new.txt
gc 'commit --amend -m "brand_new.txt（补过内容，也改了说明）"'
say "提交数没变多，最后一条被换掉了 —— 但哈希变了"
g 'log --oneline -3'
say "被换掉的那个提交没消失，在 reflog 里躺着"
g 'reflog -3'

say "加餐：没提交成功时 git 到底怎么说（另起一个仓库，从「还没有任何提交」演起）"
go . '<沙箱根>'
raw "git init -q -b main nomit"
go nomit
say "这个仓库要演的是「身份有了、但提交没成」，所以身份用 --local 写进沙箱仓库自己（绝不 --global）"
g 'config --local user.name "学习者"'
g 'config --local user.email "learn@example.com"'
raw "echo 'a 的内容' > a.txt"
g add a.txt
grc "GIT_EDITOR=true git commit   # 进了编辑器，一个字没写就退出来"
grc "git log --oneline"
g "status -sb"
say "先把 a.txt 真提交一次，好演第二种脏：改了但没 add，而且没带 -a"
g 'commit -qm "a.txt 第一次进仓库"'
raw "echo '+改动' >> a.txt"
raw "echo '我是野文件' > b.txt"
g "status -sb"
grc 'git commit -m "改了 a.txt"   # 改了但没 add，而且没带 -a'
say "同一条命令，补一句 git add a.txt 之后就成功了"
g add a.txt
g 'commit -m "改了 a.txt"'
g "status -sb"
go daily
say "回到 daily，继续场景 5 的三个区两两对比"

# ============================================================================
scene "5：diff / show —— 三个区两两对比，一条都不能记错"
# ============================================================================
raw "printf 'L1\nL2\nL3\n' > note.md"
g add note.md
gc 'commit -m "note.md 进仓库"'
say "现场：先把 L2 改了并 add（进暂存区），再把 L3 改了（只在工作区）"
raw "printf 'L1\nL2改并暂存\nL3\n' > note.md"
g add note.md
raw "printf 'L1\nL2改并暂存\nL3也改了\n' > note.md"
g "status -s -- note.md"
say "① git diff = 工作区 vs 暂存区（我 add 之后又改了什么）"
g 'diff -- note.md'
say "② git diff --staged = 暂存区 vs 仓库（这次 commit 会带上什么）"
g 'diff --staged -- note.md'
say "③ git diff HEAD = 工作区 vs 仓库（离上次提交总共改了多少）"
g 'diff HEAD -- note.md'
say "④ git diff <A> <B> = 两个提交之间比"
gc 'commit -qam "note.md 两行都提交"'
g 'diff HEAD~1 HEAD -- note.md'
say "git show：一个提交的全套信息（元信息 + 差异）"
g 'show --stat --oneline HEAD'
say "git show <提交>:<路径> = 直接取那个版本的文件内容"
g 'show HEAD~1:note.md'
g 'show HEAD:note.md'
say "顺手把 daily 到这里的历史数一遍：正好 9 条，顶端就是 note.md 那条"
g 'log --oneline'

# ============================================================================
scene "6：log 家族 —— 历史怎么看，区间语法 A..B 与 A...B"
# ============================================================================
go . '<沙箱根>'
raw "git init -q -b main loglab"
go loglab
say "造一个两条分支真正分叉过的历史（分叉才好演示三个点）"
raw "echo v1 > app.txt"
g add app.txt
gc 'commit -m "A1 初始"'
raw "echo v2 >> app.txt"
gc 'commit -qam "A2 main 第二行"'
g 'checkout -q -b feature'
raw "echo f1 > feat.txt"
g add feat.txt
gc 'commit -m "F1 feature 第一次"'
raw "echo f2 >> feat.txt"
gc 'commit -qam "F2 feature 第二次"'
g checkout main
raw "echo m3 >> app.txt"
gc 'commit -qam "A3 main 第三行"'
say "现在 main 和 feature 是分叉的：main 多 A3，feature 多 F1/F2"
g 'log --oneline --graph --all'

say "--oneline：一行一个提交（日常最常用）"
g 'log --oneline'
say "-n 2：只看最近两个"
g 'log --oneline -n 2'
say "--graph --all：全分支拓扑图（--decorate 在 --oneline 里是默认开的）"
g 'log --oneline --graph --all'
say "--stat：每个提交动了哪些文件、各几行"
g 'log -n 2 --stat --oneline'
say "两个点 A..B 读作：'B 里凡是 A 没有的提交'，左端点不包含"
A2=$(git log --format=%h -n1 --grep='A2 main')
g "log --oneline $A2..main"
say "想把左端点也算进去，就写它的父：A2^..main"
g "log --oneline ${A2}^..main"
say "两个点还能反过来看：feature..main = main 有而 feature 没有的"
g 'log --oneline feature..main'
g 'log --oneline main..feature'
say "三个点 A...B = 对称差：两条线各自独有的提交混在一起"
g "log --oneline main...feature"
say "不加 --left-right 看不出方向（< = 左边独有，> = 右边独有）"
g "log --oneline --left-right main...feature"
say "实战最常用的三连：分叉点、领先多少、落后多少"
g 'merge-base main feature'
g 'rev-list --count main..feature'
g 'rev-list --count feature..main'
say "diff 的三个点：从共同祖先开始比，只看 feature 这条线真正改了什么"
g 'diff --stat main...feature'
say "对比一下两个点：main..feature 在 diff 里等价于直接比两端（含 main 侧的反向改动）"
g 'diff --stat main feature'

say "最后合并，再看一次图（合并完 main 就包含了 feature，三个点会退化成两个点）"
gc 'merge --no-edit -m "M 合并 feature" feature'
g 'log --oneline --graph --all'
g "log --oneline ${A2}..HEAD"
g "log --oneline $A2...HEAD"

say "加餐：为什么本章的图里没有 (HEAD -> main) —— 同一份历史，三种接法"
go . '<沙箱根>'
raw "git init -q -b main declab"
go declab
raw "echo v1 > app.txt"
g add app.txt
gc 'commit -m "A1 初始"'
g 'checkout -q -b feature'
raw "echo f1 > feat.txt"
g add feat.txt
gc 'commit -m "F1 feature 第一次"'
g checkout main
raw "echo v2 >> app.txt"
gc 'commit -qam "A2 main 第二行"'
say "① 输出连着终端才自动加装饰。这里用 script 造一个 pty 冒充终端（颜色关掉，免得转义符刷屏）"
printf '\n$ script -qec "git --no-pager -c color.ui=never log --oneline --graph --all" /dev/null\n'
script -qec "git --no-pager -c color.ui=never log --oneline --graph --all" /dev/null | tr -d '\r'
say "② 输出进管道 / 重定向：装饰就没了 —— 本章 demo 前面那些图全是这种情况"
g '--no-pager log --oneline --graph --all'
say "③ 同样在管道里，但显式写 --decorate：分支名又回来了"
g '--no-pager log --oneline --graph --decorate --all'

# ============================================================================
scene "7：经典翻车 —— 文件已经提交了，才想起来要 ignore"
# ============================================================================
go . '<沙箱根>'
raw "git init -q -b main oops"
go oops
raw "printf 'DEBUG=1\nAPI_KEY=开发用的\n' > .env"
raw "echo 'code' > main.c"
g add -A
gc 'commit -m "把 .env 也一起提交了（事故现场）"'
g 'log --stat --oneline'
say "过了几天才想起来：这文件不该进仓库。于是写了 .gitignore"
raw "printf '.env\n*.env\n' > .gitignore"
raw "printf 'SECRET=漏出去了\n' >> .env"
say "但 .env 已经被跟踪了，.gitignore 对它完全无效"
g "status -s"
say "check-ignore 还会'骗'你：它默认跳过已跟踪文件，所以什么都没输出"
grc "git check-ignore -v .env"
say "加 --no-index 才看见规则本来是命中它的"
grc "git check-ignore -v --no-index .env"
say "唯一的修法：git rm --cached（从清单里除名，本地文件保留），再提交"
g 'rm --cached .env'
raw "ls -a"
say "文件还在原地，但清单里已经没它了"
g "status -s"
g add .gitignore
gc 'commit -m "停止跟踪 .env，同时把 .gitignore 提交进仓库"'
raw "printf 'SECRET=又加了一行\n' >> .env"
say "现在随便改 .env，status 一个字都不输出 = ignore 生效了"
g "status -s"
g 'log --stat --oneline'
say "反面提醒：历史里那个提交还带着 .env，删除只是从今天起不再跟踪"
g 'show HEAD^:.env'
say "对比：git rm 不加 --cached，是连工作区文件一起删掉"
g 'rm main.c'
raw "ls -a"
g "status -s"

say "加餐：MM 状态下 git rm --cached 会拒绝干活（另起一个仓库 cfg.txt）"
go . '<沙箱根>'
raw "git init -q -b main rmcache"
go rmcache
raw "printf 'v1\n' > cfg.txt"
g add cfg.txt
gc 'commit -qm "cfg.txt 的 v1 进仓库"'
raw "printf 'v2 暂存了这一版\n' > cfg.txt"
g add cfg.txt
raw "printf 'v3 工作区又改了一版\n' > cfg.txt"
g "status -s"
grc "git rm --cached cfg.txt   # 想把暂存区里那条撤掉"
say "报错的意思：暂存区那份跟工作区不一样、也跟 HEAD 不一样，它不敢自作主张扔一份"
g "status -s   # 再问一次：还是 MM，证明上面那条 git rm 根本没执行"
say "正解不是 -f（-f 是确认连工作区文件一起扔），而是先退暂存区再除名"
g "restore --staged cfg.txt"
g "status -s"
g "rm --cached cfg.txt"
g "status -s"
raw "ls"
say "git ls-files 这时候是空输出：清单里已经没有 cfg.txt 了"
g "ls-files"

# ============================================================================
scene "8：.gitignore 语法逐条实测 + check-ignore -v 排查"
# ============================================================================
go . '<沙箱根>'
raw "git init -q -b main iolab"
go iolab
raw "mkdir -p build/sub docs/a/tmp docs/a/b/tmp logs sub notes"
raw "for f in build/keep.txt build/sub/other.txt run.log important.log docs/a/tmp/one.c docs/a/b/tmp/two.c logs/nested.log tmp-only.txt sub/tmp-only.txt secretA.pem secretAB.pem tmp notes/me.txt; do printf 'x\n' > \"\$f\"; done"
cat > .gitignore <<'IGNORE'
*.log
!important.log
build/*
!build/keep.txt
/tmp-only.txt
secret?.pem
docs/**/tmp/
IGNORE
raw "cat .gitignore"
say "这份 .gitignore 一共 7 条规则，逐条问 check-ignore -v 是谁说了算"
grc "git check-ignore -v run.log important.log build/keep.txt build/sub/other.txt tmp-only.txt sub/tmp-only.txt secretA.pem secretAB.pem docs/a/tmp/one.c docs/a/b/tmp/two.c logs/nested.log"
say "输出格式：规则来自哪个文件:第几行:什么规则 <TAB> 路径"
say "没被打印出来的路径 = 一条规则都没命中（sub/tmp-only.txt、secretAB.pem）"
say "想让没命中的路径也列出来就加 -n，但 -n 必须和 -v 一起用"
grc "git check-ignore -v -n run.log sub/tmp-only.txt secretAB.pem"
gf "git check-ignore -n run.log"
g "status -s --untracked-files=all"

say "反例一：写成 build/ 时，里面的取反就失效了（git 根本不进被忽略的目录）"
raw "printf 'build/\n!build/keep.txt\n' > .gitignore"
raw "cat .gitignore"
grc "git check-ignore -v build/keep.txt"
say "换成 build/* 取反才生效"
raw "printf 'build/*\n!build/keep.txt\n' > .gitignore"
raw "cat .gitignore"
grc "git check-ignore -v build/keep.txt build/sub/other.txt"

say "反例二：结尾带 / 只匹配目录；不带 / 时文件和同名目录一起中招"
raw "printf 'tmp\n' > .gitignore"
grc "git check-ignore -v tmp docs/a/tmp/one.c"
raw "printf 'tmp/\n' > .gitignore"
grc "git check-ignore -v tmp docs/a/tmp/one.c"

say "反例三：行尾注释不合法，整行会被当成规则"
raw "printf 'notes/ # 忽略笔记目录\n' > .gitignore"
raw "cat .gitignore"
grc "git check-ignore -v notes/me.txt"
say "notes/me.txt 没被忽略；改成规规矩矩的一行才生效"
raw "printf 'notes/\n# 这行才是真正的注释\n' > .gitignore"
raw "cat .gitignore"
grc "git check-ignore -v notes/me.txt"

say "另外还有两个地方能写忽略规则：仓库私有的 .git/info/exclude、全局的 core.excludesFile。"
say "它们不进仓库、不该被提交 —— 第 06 章专门讲，这里不配（免得动了你本机的全局设置）"
grc "git check-ignore -v --no-index run.log"
say "--no-index 是对已跟踪文件排查忽略用的（场景 7 里 .env 那个坑）；这里是未跟踪文件，所以只看规则本身"
raw "git config --local core.excludesFile; echo \"（没配就是空，本机不配）\""

# ============================================================================
scene "9：别名与常用配置 —— 只用 --local 和 -c，绝不动你的全局"
# ============================================================================
go loglab
say "写法一：临时别名，只活在这一条命令里（-c）"
grc "git -c alias.w='status -sb' w"
say "写法二：写进本仓库的 .git/config（--local），只影响这个仓库"
g "config --local alias.st 'status -sb'"
g "config --local alias.ci 'commit -m'"
g "config --local alias.lg 'log --oneline --graph --decorate --all'"
raw "grep -A4 '\\[alias\\]' .git/config"
raw "echo '+一行改动' >> app.txt"
g st
g 'log --oneline -1'
say "看：别名只是文本替换，git 直接把 st 展开成 status -sb"
grc "git config --local --show-origin --get alias.st"
say "--show-origin 是排查'我这配置到底在哪写的'的利器"
g "config --local --list"
say "常用的本地开关（全都只写 .git/config）"
g "config --local core.quotepath false"
g "config --local pull.rebase false"
g "config --local init.defaultBranch main"
grc "git config --local --show-origin --get core.quotepath"
say "本仓库现在生效的身份：脚本用的是 -c，所以配置文件里查不到 user.*"
grc "git config --local --get user.email"
grc "git -c user.name='学习者' -c user.email='learn@example.com' config --get user.email"
say "别名不带身份照样提交不了 —— 这正是全书每条 commit 都带 -c 前缀的原因"
grcm "git ci '用别名 ci 提交的'"
say "加上身份前缀就通了，别名照常可用"
g add app.txt
gc "ci '用别名 ci 提交的'"
g st
g lg

printf '\n以上 9 个场景，就是 README 里 9 个小节的证据链。\n'
