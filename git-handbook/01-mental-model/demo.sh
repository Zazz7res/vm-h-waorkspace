#!/usr/bin/env bash
# ============================================================================
# 01 心智模型 —— 一键复现整章场景
#
# 用法：
#   bash demo.sh            # 跑完自动删掉沙箱
#   bash demo.sh --keep     # 跑完保留沙箱并打印路径，你可以进去自己敲命令
#
# 所有 git 写操作都发生在一个 mktemp 出来的临时沙箱里：
# 不碰你机器上任何真实仓库，也不读写你的 ~/.gitconfig（GIT_CONFIG_GLOBAL=/dev/null）。
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

# ---- 让输出可复现：时间戳、身份、语言、分页器全部固定 ----------------------
export GIT_AUTHOR_DATE="2026-01-01T10:00:00 +0800"
export GIT_COMMITTER_DATE="2026-01-01T10:00:00 +0800"
export GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_SYSTEM=/dev/null   # 不读也不写任何全局配置
export LC_ALL=C.UTF-8         # git 提示语固定英文（你手敲时可能是中文，README 里有对照表）
export GIT_PAGER=cat          # 不分页
export GIT_MERGE_AUTOEDIT=no  # 合并时不弹编辑器

# ---- 沙箱（在任意目录运行都可以，脚本内部一律用绝对路径）------------------
SB=$(mktemp -d "${TMPDIR:-/tmp}/git01-XXXXXX")
cleanup() {
    if [ "$KEEP" -eq 1 ]; then
        printf '\n[沙箱已保留] %s\n  进去自己敲： cd "%s/demo" && git status\n  看完自己删： rm -rf "%s"\n' "$SB" "$SB" "$SB"
    else
        rm -rf "$SB"
        printf '\n[沙箱已删除] %s\n' "$SB"
    fi
}
trap cleanup EXIT

# ---- 打印小工具：让日志读起来像讲解 ----------------------------------------
# 三个助手都接收「一整条命令字符串」，打印出来的和你手敲的一模一样。
ID='-c user.name="学习者" -c user.email="learn@example.com"'   # 提交身份，只作用于单条命令
scene() { printf '\n\n======== 场景 %s ========\n' "$*"; }
say()   { printf '\n--- %s\n' "$*"; }
raw()   { printf '\n$ %s\n' "$*"; bash -c "$*"; }               # 任意 shell 命令
g()     { printf '\n$ git %s\n' "$*"; bash -c "git $*"; }       # 不需要提交身份的 git 命令
gc()    { printf '\n$ git %s %s\n' "$ID" "$*"; bash -c "git $ID $*"; }  # 需要提交身份的
# gf：故意跑一条会失败的命令，好让读者看到 git 是怎么报错的（不让脚本挂掉）
gf()    { printf '\n$ git %s   （这条会失败，就为了看 git 的报错措辞）\n' "$*"; bash -c "git $* || true"; }

printf '沙箱：%s\n' "$SB"
cd "$SB"

# ============================================================================
scene "1：新建仓库，先看一眼 .git 里长什么样"
# ============================================================================
raw "git init -b main demo"
cd "$SB/demo"

raw "ls -a"
raw "ls .git"
raw "find .git/objects .git/refs | sort"
say "本册最重要的一个 cat：HEAD 只是一份写着字符串的文本文件"
raw "cat .git/HEAD"
say "git config --local 写的就是这个 config 文件"
raw "cat .git/config"
say "hooks 目录里全是不生效的 .sample 模板"
raw "ls .git/hooks | head -4"
raw "printf '  （一共 %s 个 .sample，去掉 .sample 后缀才会生效）\n' \"$(ls .git/hooks/*.sample | wc -l)\""
gf "rev-parse HEAD"

# ============================================================================
scene "2：三个区对号入座 —— status 的每种措辞分别指哪个区"
# ============================================================================
say "造现场：note.txt 已跟踪，todo.txt 未跟踪，junk.log 被忽略"
raw "printf '第一行\n第二行\n' > note.txt"
raw "printf '*.log\n' > .gitignore"
raw "echo '临时日志' > junk.log"
g 'add note.txt .gitignore'
gc 'commit -m "提交1：note.txt 和 .gitignore 进仓库"'

say "把三个区同时搞脏"
raw "printf '第一行\n第二行改过了\n' > note.txt"     # 工作区改了，没 add
raw "echo '新增文件' > todo.txt"                     # 工作区多出一个没跟踪的
g 'rm --cached .gitignore'                          # 索引里删掉，仓库里还有 => deleted
say "git ls-files 看的是索引（暂存区），此刻里面只剩 note.txt"
g "ls-files"

say "完整 status：三类措辞 = 三个区各自的变化"
g "status"

say "短格式 status -sb：第一列=索引 vs 仓库，第二列=工作区 vs 索引"
g "status -sb"

say "把未跟踪的 todo.txt 加入索引：?? 变成 A"
g 'add todo.txt'
g "status -sb"

say "把索引里这份退回，工作区文件一个字都不动：git restore --staged"
g "restore --staged todo.txt"
g "status -sb"

say "add -A 全收，再提交，三方就一致了"
g 'add -A'
gc 'commit -m "提交2：todo.txt 进仓库，.gitignore 停止跟踪"'
g "status"

# ============================================================================
scene "3：git add 到底往 .git 里写了什么"
# ============================================================================
say "索引里存的是一行行的：模式 哈希 stage 文件名"
g "ls-files -s"

say "哈希不是编号，是内容算出来的：顺着哈希就能捞出内容"
raw "git cat-file -p \"$(git rev-parse 'HEAD:note.txt')\""

say "改内容，先别 add —— 看 objects 里现在是这些"
raw "printf '第一行\n第二行改过了\n第三行\n' > note.txt"
raw "find .git/objects -type f | sort"

say "现在 add：注意 objects 里立刻多了一个文件，但还没提交"
g 'add note.txt'
raw "find .git/objects -type f | sort"
g "ls-files -s"

say "把索引里那串哈希 cat 出来 —— 就是工作区那份内容"
NOTE_HASH=$(git ls-files -s note.txt | awk '{print $2}')   # 脚本内部取哈希，不参与打印
raw "git cat-file -p $NOTE_HASH"
say "git 自己怎么称呼这个对象"
raw "git cat-file -t $NOTE_HASH"

say "同样内容再 add 一次，哈希一字不变（内容寻址天然去重）"
g 'add note.txt'
g 'rev-parse :note.txt'

say "加餐翻车：手滑把 .git/index 删了，status 立刻开始骗人"
raw "rm .git/index"
g "status -sb"
say "别慌，git reset（默认 mixed）只重建清单，工作区一个字都不动"
g reset
g "status -sb"
say "把本场景原来的现场恢复回去，好让后面的接着演"
g 'add note.txt'

# ============================================================================
scene "4：commit 是快照，不是差异"
# ============================================================================
raw "seq 1 9 | sed 's/^/第 /;s/$/ 行/' > big.txt"
g 'add big.txt'
gc 'commit -m "提交3：加上 9 行的 big.txt"'

say "第 1 层：提交对象本身（tree 指针 + 父提交 + 作者/时间 + 说明）"
g 'cat-file -p HEAD'
say "对比：整条历史最底下那个提交（root-commit），根本没有 parent 这一行"
g 'cat-file -p $(git rev-list --max-parents=0 HEAD)'
say "第 2 层：顺着 tree 指针摸到目录清单"
g 'cat-file -p "HEAD^{tree}"'
say "第 3 层：再顺着 blob 指针摸到文件内容本体"
raw 'git cat-file -p "$(git rev-parse HEAD:big.txt)"'

say "改一行之前，objects 里躺着的文件数"
raw "find .git/objects -type f | wc -l"
g "count-objects -v"

say "现在只改 big.txt 第 5 行，再提交"
raw "sed -i '5s/.*/第 5 行改了/' big.txt"
gc 'commit -qam "提交4：只改第 5 行"'
raw "find .git/objects -type f | wc -l"
g "count-objects -v"

say "多出来正好 3 个对象：新 commit + 新 tree + 新 blob（三个类型各查一次）"
g 'cat-file -t HEAD'
g 'cat-file -t "HEAD^{tree}"'
g 'cat-file -t HEAD:big.txt'

say "老对象没变大：改之前那版 big.txt 多少字节"
g 'cat-file -s HEAD~1:big.txt'
say "改之后这版多少字节（就是差了几个汉字）"
g 'cat-file -s HEAD:big.txt'

say "两个提交之间没存任何 diff，差异是现场算的"
g 'diff --stat HEAD~1 HEAD'

say "极端验证：把第 5 行原样改回去再提交"
raw "sed -i '5s/.*/第 5 行/' big.txt"
gc 'commit -qam "提交5：改回去了"'
say "提交3 里 big.txt 的哈希"
g 'rev-parse HEAD~2:big.txt'
say "提交5 里 big.txt 的哈希 —— 一模一样，说明没有产生新内容对象"
g 'rev-parse HEAD:big.txt'
say "对象数只 +1（只有新 commit，tree 和 blob 全都复用）"
g "count-objects -v"

# ============================================================================
scene "5：分支是一个写着 commit hash 的文本文件"
# ============================================================================
say "当前分支名 + 每个分支文件里写的内容（grep 在多个文件上会带上文件名）"
g "branch --show-current"
raw "grep '' .git/refs/heads/*"
say "HEAD 里写的不是哈希，而是「去哪个文件读哈希」"
raw "cat .git/HEAD"

say "建分支 experiment = 在 refs/heads 下新建一个文本文件"
g 'branch experiment'
raw "ls .git/refs/heads"
raw "grep '' .git/refs/heads/*"

say "切分支 = 只改 HEAD 那一行"
g 'checkout experiment'
raw "cat .git/HEAD"

say "在新分支上提交：被改写的字符串是 experiment 那一个文件"
raw "echo 'experiment 专属' > exp.txt"
g 'add exp.txt'
gc 'commit -m "experiment 上的提交"'
raw "grep '' .git/refs/heads/*"
raw "cat .git/HEAD"

say "main 一动没动：分支隔离的全部秘密就这么多"
g "log --oneline --decorate --graph --all"

# ============================================================================
scene "6：detached HEAD —— 你现在不在任何分支上"
# ============================================================================
say "checkout 一个提交哈希（而不是分支名），git 会警告你"
raw "git checkout $(git rev-parse --short 'HEAD~1')"

say "看 HEAD 文件：里面是裸哈希，不再是 ref: 开头"
raw "cat .git/HEAD"
say "对比场景 5 里的 ref: refs/heads/experiment"
g "status"

say "在游离状态下老老实实提交一个"
raw "echo '游离状态下写的东西' > loose.txt"
g 'add loose.txt'
gc 'commit -m "游离提交：没有任何分支指向它"'
LOOSE=$(git rev-parse HEAD)     # 脚本内部记下这个哈希（切走后要靠它自救）
say "刚提交的游离提交哈希是 $LOOSE —— 请记住它，全仓库没有任何分支指向它"

say "各分支文件里的哈希一点没动"
raw "grep '' .git/refs/heads/*"
raw "cat .git/HEAD"

say "切回分支：git 会明确警告你把提交丢在了后面"
g 'checkout experiment'
g "log --oneline -1"
say "问：哪些分支包含那个游离提交？（下面空白 = 一个都没有）"
raw "git branch --contains $LOOSE"
say "整个仓库所有分支加起来也找不到它"
g 'log --oneline --all'

say "自救第一步：reflog 记着 HEAD 走过的每一步（包括那个游离提交）"
g 'reflog'

say "自救第二步 A：用刚才记下的哈希建分支保住"
raw "git branch rescue $LOOSE"
raw "git log --oneline -1 rescue"
raw "git branch --contains $LOOSE"

say "自救第二步 B：没记哈希也不慌，HEAD@{1} 这种位置引用能直接捞"
raw "git rev-parse 'HEAD@{1}'"
raw "git branch rescue2 'HEAD@{1}'"
raw "git log --oneline -1 rescue2"

printf '\n以上 6 个场景就是 README 里 6 个小节的证据链。\n'
