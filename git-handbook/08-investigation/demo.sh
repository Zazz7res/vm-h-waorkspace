#!/usr/bin/env bash
# 第 08 章 排查取证 —— demo.sh
# 演示：git log 检索全家桶 / git show / git blame / git bisect（含合并提交）/ git reflog 救场 / git fsck + gc
#
# 用法：  bash demo.sh            跑完自动删掉沙箱
#         bash demo.sh --keep     跑完保留沙箱，并打印沙箱路径，可以自己进去翻
#
# 安全说明（很重要）：
#   * 全程在 mktemp 出来的临时目录里跑，不联网、不碰你机器上任何真实仓库。
#   * 提交一律用 -c user.name=学习者 -c user.email=learn@example.com 的临时身份，
#     不读也不写你的全局配置。
#   * 屏幕上为了好读，会把这两个 -c 省略掉（真实执行的命令里它们是存在的）。
#   * 检索「时间」相关的那一节（--since / @{yesterday}）用的是『相对运行当天』的日期，
#     所以你看到的日期会和文档里不一样；但每条命令命中几条提交、命中哪几条，完全一致。
set -euo pipefail

KEEP=0
[ "${1:-}" = "--keep" ] && KEEP=1
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

SB=$(mktemp -d)
cleanup() {
  if [ "$KEEP" = 1 ]; then
    printf '\n[--keep] 沙箱保留在：%s\n' "$SB"
    printf '         可以进去自己玩，例如：cd %s/inv && git log --oneline -G"MAX_RETRY"\n' "$SB"
    printf '         调查现场：%s/inv\n' "$SB"
    printf '         二分现场：%s/bis（24 个提交）%s/bm（含合并提交）\n' "$SB" "$SB"
    printf '         回收站现场：%s/gc\n' "$SB"
  else
    rm -rf "$SB"
    printf '\n沙箱已清理：%s\n' "$SB"
  fi
}
trap cleanup EXIT

export LC_ALL=C LANG=C
GID='-c user.name=学习者 -c user.email=learn@example.com'
G="git $GID"
pretty() { printf '%s' "$1" | sed "s/ $GID//g"; }
R() {
  local out ec
  printf '\n$ %s\n' "$(pretty "$*")"
  out=$(eval "$*" 2>&1) && ec=0 || ec=$?
  printf '%s\n' "$out"
  if [ $ec -ne 0 ]; then printf '[命令退出码 = %s —— 上面的报错是故意让它发生的]\n' "$ec"; fi
  return 0
}
say()  { printf '\n=============== %s ===============\n' "$*"; }
note() { printf -- '--- %s\n' "$*"; }

# 造一个提交：mkc <几天前> <作者名> <作者邮箱> <提交信息>（文件内容由调用方先写好）
# 把日期做成「相对今天」，git reflog 里记录的时间也跟着走，这样 @{yesterday} 这类语法才有东西可查
mkc() {
  local ts
  ts=$(date -d "$1" +"%Y-%m-%dT%H:%M:%S %z")
  GIT_AUTHOR_DATE="$ts" GIT_COMMITTER_DATE="$ts" git -c user.name="$2" -c user.email="$3" commit -q -m "$4"
}

# =========================================================
say "0. 先造一个『像真实项目』的调查现场（15 个提交，3 个作者，5 周跨度）"
# =========================================================
$G init -q "$SB/inv"
cd "$SB/inv"
printf '# 调查手册示例项目\n\n一个用来练检索的小项目。\n' > README.md
mkdir -p src docs
cat > src/calc.c <<'EOF'
#include <stdio.h>
/* 把等级换算成奖励分 */
int bonus(int level) {
    int v = level * 10;
    return v;
}
EOF
cat > src/net.py <<'EOF'
def connect(sock):
    for i in range(MAX_RETRY):
        if sock.ok():
            return True
    return False
EOF
cat > src/cart.py <<'EOF'
def total_price(items, discount):
    subtotal = 0
    for it in items:
        subtotal += it.price
    if discount < 0:
        raise ValueError('bad discount')
    return subtotal * (1 - discount)

def count_items(items):
    n = 0
    for it in items:
        n += 1
    return n

def shipping_fee(weight, zone):
    base = 8
    if zone > 3:
        base = base + 5
    return base + weight

def gift_wrap(items):
    return 3 * len(items)
EOF
printf '# 设计说明\n\n模块划分：calc / net / cart。\n' > docs/design.md
printf 'MAX_RETRY = 3\n' > src/config.py
$G add -A && mkc "35 days ago" "同事甲" "jia@example.com" "init: 项目骨架"

printf 'def retry_policy():\n    return MAX_RETRY\n' >> src/net.py
$G add -A && mkc "30 days ago" "同事乙" "yi@example.com" "feat: 给 net.py 加 MAX_RETRY 的引用 (#101)"

printf '\n/* 修一下负数等级导致的崩溃 */\nint safe_bonus(int level) {\n    if (level < 0) return 0;\n    return bonus(level);\n}\n' >> src/calc.c
$G add -A && mkc "28 days ago" "学习者" "learn@example.com" "fix: 修登录页等级为负时的崩溃 (#102)"

# 关键道具：这一次提交只是把两个函数换了个位置，MAX_RETRY 出现次数一次没增没减
cat > src/net.py <<'EOF'
def retry_policy():
    return MAX_RETRY

def connect(sock):
    for i in range(MAX_RETRY):
        if sock.ok():
            return True
    return False
EOF
$G add -A && mkc "21 days ago" "同事乙" "yi@example.com" "refactor: retry_policy 和 connect 换个位置（MAX_RETRY 一次没增没减）"

printf '\nLIMIT = MAX_RETRY * 2\nprint(MAX_RETRY)\n' >> src/net.py
$G add -A && mkc "18 days ago" "学习者" "learn@example.com" "feat: 多处引用 MAX_RETRY（出现次数变多了）"

sed -i 's/MAX_RETRY/ATTEMPTS/g' src/net.py
$G add -A && mkc "17 days ago" "同事甲" "jia@example.com" "refactor: MAX_RETRY 统一改名 ATTEMPTS (#104)"

# 关键道具：纯空白改动，只把第 10 行的空格换成 Tab（blame -w 的用武之地）
sed -i '10s/^    n = 0$/\t    n = 0/' src/cart.py
$G add -A && mkc "14 days ago" "同事乙" "yi@example.com" "style: 调了一下 count_items 的缩进（纯空白改动）"

# 关键道具：把 cart.py 的运费函数抄成 ship.py，并且在同一个提交里动一下 cart.py
# （blame -C 只能在『这个提交同时改过的文件』里找拷贝来源，所以这里必须一起改 cart.py）
printf 'def shipping_fee(weight, zone):\n    base = 8\n    if zone > 3:\n        base = base + 5\n    return base + weight\n' > src/ship.py
sed -i 's|^def shipping_fee(weight, zone):$|def shipping_fee(weight, zone):  # 这段运费逻辑已抄到 src/ship.py|' src/cart.py
sed -i "s/# 设计说明/# 设计说明（已补充）/" docs/design.md
$G add -A && mkc "12 days ago" "学习者" "learn@example.com" "refactor: 把运费逻辑抄一份到 src/ship.py"

$G switch -q -c feature/export
printf '\ndef export_csv(rows):\n    return ",".join(rows)\n' >> src/cart.py
$G add -A && mkc "11 days ago" "同事甲" "jia@example.com" "feat: 支持导出 CSV (#103)"
$G switch -q main
printf '\n# 集成记录\n' >> README.md
$G add -A && mkc "10 days ago" "学习者" "learn@example.com" "chore: 主干上的一次普通改动"
ts=$(date -d "9 days ago" +"%Y-%m-%dT%H:%M:%S %z")
GIT_AUTHOR_DATE="$ts" GIT_COMMITTER_DATE="$ts" $G merge -q --no-ff feature/export -m "Merge branch 'feature/export' (#103 落地)"
$G branch -q -D feature/export

printf '\nprint("debug: 上线前删掉我")\n' >> src/net.py
$G add -A && mkc "7 days ago" "同事乙" "yi@example.com" "wip: 临时调试打印"
sed -i '/debug: 上线前删掉我/d' src/net.py
$G add -A && mkc "6 days ago" "同事乙" "yi@example.com" "chore: 删掉调试打印"

mv docs/design.md docs/architecture.md
printf '\n架构补充：calc 不依赖任何 IO。\n' >> docs/architecture.md
$G add -A && mkc "4 days ago" "学习者" "learn@example.com" "docs: design.md 改名 architecture.md 并补一句"

sed -i '/^def gift_wrap/i cache = {}' src/cart.py
$G add -A && mkc "2 days ago" "同事甲" "jia@example.com" "feat: 给 total_price 加缓存

Co-authored-by: 同事甲 <jia@example.com>
Signed-off-by: 同事乙 <yi@example.com>"

sed -i 's/return base + weight$/return base + weight * 2/' src/cart.py
$G add -A && mkc "1 days ago" "同事甲" "jia@example.com" "fix: 运费按重量加倍 (#205)"
note "现场长这样（merge 那一支是 feature/export 里的提交）："
$G log --graph --oneline --date=short --format='%h %ad %an %s'

# =========================================================
say "1. git log 检索全家桶：按时间 / 按人 / 按内容 / 按说明 / 按路径"
# =========================================================
note "① 按时间：--since / --until（= --after / --before），后面可以写人话"
R "cd $SB/inv && $G log --oneline --since='2 weeks ago' --format='%h %ad %an %s' --date=short"
R "cd $SB/inv && $G log --oneline --since='2026-01-01' --until='3 weeks ago' --format='%h %ad %s' --date=short"
R "cd $SB/inv && $G log --oneline --after='last monday' --format='%h %ad %s' --date=short"
note "② 按人：--author 匹配的是『作者字段』，可以只写一部分，也能和 --grep 合起来用"
R "cd $SB/inv && $G log --oneline --author='同事甲' --format='%h %an %s'"
R "cd $SB/inv && $G log --oneline --author='yi@example.com' --format='%h %an %s'"
R "cd $SB/inv && $G log --oneline --author='甲' --grep='CSV' --all-match --format='%h %an %s'"
R "cd $SB/inv && $G shortlog -sne HEAD"
R "cd $SB/inv && echo '### 只算最近两周（注意：shortlog 必须显式给一个范围，否则它会去等标准输入）：' && $G shortlog -sne HEAD --since='2 weeks ago'"
note "③ 按改动内容：-G = diff 里有匹配行就算；-S = 这个字符串的出现次数『变了』才算"
R "cd $SB/inv && $G log --oneline -G'MAX_RETRY' --format='%h %s'"
R "cd $SB/inv && $G log --oneline -S'MAX_RETRY' --format='%h %s'"
note "两边差的那一条（换个位置那次）就是全部区别：-G 命中 5 条、-S 命中 4 条。证据在下面："
R "cd $SB/inv && $G show HEAD~11 --format='%h %s' -- src/net.py"
R "cd $SB/inv && echo -n '改之前 MAX_RETRY 出现次数 = ' && $G show 'HEAD~12:src/net.py' | grep -c MAX_RETRY && echo -n '改之后 MAX_RETRY 出现次数 = ' && $G show 'HEAD~11:src/net.py' | grep -c MAX_RETRY"
note "次数没变 → -S 看不见它；diff 里确实有含 MAX_RETRY 的行 → -G 看得见"
R "cd $SB/inv && $G log --oneline -S'range(ATTEMPTS)' --format='%h %s'"
R "cd $SB/inv && $G log --oneline -S'range\(ATTEMPTS\)' --pickaxe-regex --format='%h %s'"
note "上面第一条证明：-S 默认按『纯文本』理解，括号就是字符本身；要正则得加 --pickaxe-regex"
R "cd $SB/inv && $G log --oneline -G'debug' --format='%h %s'"
note "④ 按提交信息：--grep 是『包含』，加 -E 就能用正则，--all-match 让多个条件相与"
R "cd $SB/inv && $G log --oneline --grep='fix' --format='%h %s'"
R "cd $SB/inv && $G log --oneline -E --grep='(fix|feat).*#10[0-9]' --format='%h %s'"
R "cd $SB/inv && $G log --oneline --grep='#' --grep='CSV' --all-match --format='%h %s'"
R "cd $SB/inv && $G log --oneline --grep='缓存' --author='甲' --format='%h %an %s'"
note "⑤ 按路径：-- 之后跟的是路径（不写 -- 的话，路径名和分支名撞车时 git 就懵了）"
R "cd $SB/inv && $G log --oneline -- src/net.py"
R "cd $SB/inv && $G log --oneline -8 --format='%h %s' -- src/ docs/"
note "⑥ --follow：跨重命名追一个文件的前世今生（git 只存改名后的快照，不写『改名日志』）"
R "cd $SB/inv && $G log --oneline -- docs/architecture.md"
R "cd $SB/inv && $G log --oneline --follow -- docs/architecture.md"
R "cd $SB/inv && $G log --follow --name-status --format='%h %s' -- docs/architecture.md"
note "⑦ --first-parent：只看主干那一列，别人分支内部的提交不算数"
R "cd $SB/inv && $G log --oneline -8 --format='%h %s'"
R "cd $SB/inv && $G log --oneline --first-parent -8 --format='%h %s'"
note "对比一下：少掉的那一条就是 feature/export 分支内部的提交，它被合并进来了，但不在主干那一列上"
note "⑧ --format：9 个最常用的占位符（写周报、做报表全靠它）"
R "cd $SB/inv && $G log -3 --format='[%h] 全哈希=%H 作者=%an 邮箱=%ae 作者日期=%ad 提交日期=%cd 距今=%ar 指针=%D 说明=%s'"
R "cd $SB/inv && $G log -2 --format='提交 %h：%s%n  正文（%b）  父亲们：%p  树对象：%T'"
R "cd $SB/inv && echo '### 只有带 trailer 的那次提交才看得出效果（这里看 HEAD~1）：' && $G log -1 --format='%h %s%n--- trailers ---%n%(trailers:only=true)' HEAD~1"
note "⑨ --numstat：左边新增行数、右边删除行数、第三列文件名（机器可读，专门给脚本用）"
R "cd $SB/inv && $G log -3 --numstat --format='COMMIT %h %s'"
R "cd $SB/inv && echo '### 每个文件总共被加/删了多少行：' && $G log --pretty=tformat: --numstat | awk '{a[\$3]+=\$1; d[\$3]+=\$2} END {for (f in a) printf \"%-18s +%s -%s\\n\", f, a[f], d[f]}' | sort"
note "看上面那行 docs/{design.md —— 这就是 numstat 的坑：改名会被写成 docs/{a => b}，中间有空格，按空格切列就断了"
R "cd $SB/inv && echo '### 想按列统计，就关掉改名检测（或者用 -z 自己解析）：' && $G log --pretty=tformat: --numstat --no-renames | awk '{a[\$3]+=\$1; d[\$3]+=\$2} END {for (f in a) printf \"%-18s +%s -%s\\n\", f, a[f], d[f]}' | sort"
R "cd $SB/inv && echo '### 每个人写了多少行（新增+删除，合并提交不算）：' && $G log --no-merges --pretty=tformat:'AUTHOR %an' --numstat | awk '/^AUTHOR /{w=\$2;next} NF==3{t[w]+=\$1+\$2} END{for (k in t) printf \"%-8s %d\\n\", k, t[k]}' | sort -k2 -rn"

# =========================================================
say "2. git show 与 diff 的目标写法：不动工作区就能看历史版本"
# =========================================================
note "① 看某个历史版本里的某个文件长什么样（git show <版本>:<路径>，注意有冒号）"
R "cd $SB/inv && $G log --oneline --format='%h %ad %s' --date=short -- src/calc.c"
R "cd $SB/inv && echo '### 30 天前那次提交（HEAD~13）里的 src/calc.c：' && $G show 'HEAD~13:src/calc.c'"
R "cd $SB/inv && echo '### 现在（HEAD）的 src/calc.c：' && $G show 'HEAD:src/calc.c'"
note "② 看某一次提交只对某个文件干了什么（git show <提交> -- <路径>，注意有 --）"
R "cd $SB/inv && $G show HEAD --format='%h %s' -- src/cart.py"
R "cd $SB/inv && $G show HEAD~11 --stat --format='%h %s'"
note "③ 直接比两个版本里的同一个文件"
R "cd $SB/inv && $G diff 'HEAD~13:src/calc.c' 'HEAD:src/calc.c'"
note "④ diff 的目标写法复盘（三张区：工作区 / 暂存区 / HEAD）—— 全部只读"
R "cd $SB/inv && printf '临时改动\n' >> src/net.py && echo '### 工作区 vs 暂存区（git diff 默认）：' && $G diff --stat"
R "cd $SB/inv && echo '### 暂存区 vs HEAD（--staged，等价 --cached）：' && $G add src/net.py && $G diff --staged --stat"
R "cd $SB/inv && echo '### 工作区+暂存区 vs HEAD：' && $G diff HEAD --stat"
R "cd $SB/inv && echo '### 收尾：把刚才的假改动清干净（第 03 章细讲）' && $G reset -q && $G checkout -- src/net.py && $G status --short && echo '(status 输出为空 = 干净了)'"
R "cd $SB/inv && echo '### 两个提交之间：A B 和 A..B 是一回事' && $G diff --stat HEAD~2 HEAD"
R "cd $SB/inv && echo '### 三个点 A...B = 从它俩的共同祖先到 B（评审分支时用这个，不算别人合进来的改动）：' && $G diff --stat HEAD~3...HEAD"
R "cd $SB/inv && echo '### HEAD^! = 只看这一个提交自己引入的改动（不累计父辈）：' && $G diff --stat 'HEAD^!'"

# =========================================================
say "3. blame：这一行到底是谁、哪次提交、什么时候写的"
# =========================================================
note "① 整份文件"
R "cd $SB/inv && $G blame src/cart.py"
note "② 只看第 9~13 行：-L"
R "cd $SB/inv && $G blame -L 9,13 src/cart.py"
note "③ -w：忽略空白改动。第 10 行的缩进被那次 style 提交动过，所以默认 blame 把它算给了那次提交"
R "cd $SB/inv && echo '### 不加 -w：' && $G blame -L 9,11 src/cart.py && echo '### 加 -w（回到真正写这行内容的人）：' && $G blame -w -L 9,11 src/cart.py"
note "④ -C：跨文件追『这段是抄哪儿的』。src/ship.py 整份都是从 cart.py 抄的"
R "cd $SB/inv && echo '### 不加 -C：五行全归给抄的那次提交' && $G blame src/ship.py"
R "cd $SB/inv && echo '### 加 -C：文件名那一列变成了 cart.py —— 那才是代码的出生地' && $G blame -C src/ship.py"
note "⑤ blame 只是起点：拿到哈希之后 log -1 / log -p 才能看懂『为什么要这么写』"
R "cd $SB/inv && $G blame -L 19,19 src/cart.py"
BLAME19=$($G blame -L 19,19 --porcelain src/cart.py | sed -n '1,1p' | cut -d' ' -f1)
R "cd $SB/inv && echo \"第 19 行出自提交 $BLAME19，先看它是什么：\" && $G log -1 --format='%h | %an | %ad | %s' --date=short \"$BLAME19\""
R "cd $SB/inv && echo '### 再看它完整的 diff（这一步才叫理解意图）：' && $G log -p -1 \"$BLAME19\" -- src/cart.py"
note "⑥ blame 最容易被误解的一点：它报的是『最后动这一行的人』，不是『这行想法最早是谁的』"
R "cd $SB/inv && echo '### 看 docs/architecture.md 第 1 行：内容来自 init 时同事甲写的，只是有人加了个尾巴，整行就换了主人：' && $G blame -L 1,3 docs/architecture.md"
R "cd $SB/inv && echo '### 这一行真正的原文（改名之前、HEAD~8 那个版本里的 docs/design.md）：' && $G show 'HEAD~8:docs/design.md'"
R "cd $SB/inv && echo '### 好消息：blame 默认会跟随改名（输出里第二列就是旧路径），组合拳是 log --follow 找旧路径 → blame <旧提交> -- <旧路径>：' && $G log --follow --name-status --format='%h %s' -- docs/architecture.md"
R "cd $SB/inv && echo '### 用旧路径 + 改名之前的提交，原作者就回来了：' && $G blame 'HEAD~8' -- docs/design.md"
note "另外三个必须知道的局限：① 大文件慢（blame 要为每一行追溯历史）；② 它只认『这一份文件』这一条线，一次提交里把代码搬进另一个文件就要加 -C 才看得见；③ 哈希前面带 ^ 表示追到了你这次追溯的边界（默认是仓库的根提交，浅克隆则是浅边界），不等于『这就是作者』。"
R "cd $SB/inv && echo '### ^ 的样子（第一列哈希带 ^ = 追到了边界提交）：' && $G blame -L 1,1 src/cart.py"
R "cd $SB/inv && echo '### 要脚本处理就用 --porcelain（第一条记录里有 commit/author/time/边界等机器可读字段）：' && $G blame -L 15,15 --porcelain src/cart.py | sed -n '1,5p'"
R "cd $SB/inv && echo '### 只想看最近两周谁动过这两行：--since 直接把更早的历史当边界：' && $G blame --since='2 weeks ago' -L 15,16 src/cart.py"

# =========================================================
say "4. bisect 完整实战：24 个提交里自动揪出改坏的那一个"
# =========================================================
note "现场：一个 C 小程序，规定 bonus(7) 必须等于 70。现在它算出 73，中间 24 个提交里有一个把逻辑改坏了。"
$G init -q "$SB/bis"
cd "$SB/bis"
cat > calc.c <<'EOF'
#include <stdio.h>

/* 等级换算奖励分：每级 10 分 */
int bonus(int level) {
    int v = level * 10;
    return v;
}
EOF
cat > main.c <<'EOF'
#include <stdio.h>

int bonus(int level);

int main(void) {
    printf("%d\n", bonus(7));
    return 0;
}
EOF
cp "$HERE/bisect-check.sh" ./test.sh
chmod +x test.sh
mkdir -p notes
$G add -A
GIT_AUTHOR_DATE="2026-01-01T10:00:00 +0800" GIT_COMMITTER_DATE="2026-01-01T10:00:00 +0800" $G commit -q -m "c1: 初始版本（检查脚本 test.sh 一起提交）"
$G tag v1.0
for i in $(seq 2 24); do
  printf '/* 第 %d 次日常改动：补一个工具函数 */\nint helper_%d(int n) { return n + %d; }\n' "$i" "$i" "$i" >> calc.c
  printf 'day %d\n' "$i" > "notes/n$i.md"
  if [ "$i" = "17" ]; then
    sed -i 's/    int v = level \* 10;/    int v = level * 10 + 3;/' calc.c
  fi
  $G add -A
  GIT_AUTHOR_DATE="2026-01-01T10:00:00 +0800" GIT_COMMITTER_DATE="2026-01-01T10:00:00 +0800" $G commit -q -m "c$i: 日常改动"
done
R "cd $SB/bis && echo -n '提交总数 = ' && $G rev-list --count HEAD && $G log --oneline | sed -n '1,3p' && echo '...（中间省略）...'"
note "先自己确认现在确实是坏的：跑一次判定脚本，看它的退出码"
R "cd $SB/bis && ./test.sh; echo \"退出码 = \$?（约定：0=好，1=坏）\""
note "为什么 test.sh 必须在『第一个提交』里就存在：bisect 会把工作区切到每一个历史版本去测；
note 如果检查脚本是最后才加的，那前面的版本里根本没有这个文件，run 会当场以 127 报错。"
note "第一步（手工版）：告诉 git 两端。bad = 现在，good = 已知没问题的 v1.0（就是 c1）"
R "cd $SB/bis && $G bisect start HEAD v1.0"
note "注意它已经把工作区切到『中间那个提交』了，并告诉你还剩几个候选、大概还要几步。自己测一下再决定标什么："
R "cd $SB/bis && ./test.sh"
R "cd $SB/bis && $G bisect good"
R "cd $SB/bis && $G bisect bad"
note "每标一次，git 就再取一次中点。当前两端存在 refs/bisect/ 下：git bisect terms 可以看"
R "cd $SB/bis && $G bisect terms && ls .git/refs/bisect/"
note "中途想放弃 / 想回到原来的位置：git bisect reset"
R "cd $SB/bis && $G bisect reset && echo '### 回到了二分之前待的那个提交：' && $G log --oneline -1"
note "第二步（自动版）：把判断交给脚本。git 会自己反复 checkout → 跑 test.sh → 记 good/bad → 再取中点，一路问到答案"
R "cd $SB/bis && $G bisect start HEAD v1.0"
R "cd $SB/bis && $G bisect run ./test.sh"
note "最后那行『xxx is the first bad commit』就是结论，git 还顺手把那个提交的 diff 打了出来。"
R "cd $SB/bis && echo '### 整个决策过程（可以复制下来当笔记）：' && $G bisect log"
R "cd $SB/bis && echo -n '24 个提交，二分实际问了几次：' && $G bisect log | grep -cE '^git bisect (good|bad)'"
R "cd $SB/bis && $G bisect reset"

say "4b. 有合并提交时二分为什么会翻车（以及 --first-parent 怎么救）"
note "现场：主干上 21 个『无辜』提交 + 分支里 3 个提交（真凶在分支里）+ 一次合并，共 26 个提交"
$G init -q "$SB/bm"
cd "$SB/bm"
cat > calc.c <<'EOF'
int bonus(int level) { return level * 10; }
EOF
cat > main.c <<'EOF'
#include <stdio.h>

int bonus(int level);

int main(void) {
    printf("%d\n", bonus(7));
    return 0;
}
EOF
cp "$HERE/bisect-check.sh" ./test.sh
chmod +x test.sh
mkdir -p notes
$G add -A
GIT_AUTHOR_DATE="2026-01-01T10:00:00 +0800" GIT_COMMITTER_DATE="2026-01-01T10:00:00 +0800" $G commit -q -m "m1 起点"
$G tag base_ok
for i in $(seq 2 12); do
  printf 'x%d\n' "$i" > "notes/n$i.txt"; $G add -A
  GIT_AUTHOR_DATE="2026-01-01T10:00:00 +0800" GIT_COMMITTER_DATE="$GIT_AUTHOR_DATE" $G commit -q -m "m$i 主干上的普通改动"
done
$G switch -q -c experimental
printf 'double rate = 1.5;\n' > feature.c
$G add -A
GIT_AUTHOR_DATE="2026-01-01T10:00:00 +0800" GIT_COMMITTER_DATE="$GIT_AUTHOR_DATE" $G commit -q -m "x1 新分支：feature.c"
sed -i 's/return level \* 10;/return level * 10 + 3;/' calc.c
GIT_AUTHOR_DATE="2026-01-01T10:00:00 +0800" GIT_COMMITTER_DATE="$GIT_AUTHOR_DATE" $G commit -q -a -m "x2 调整 bonus（真凶在这）"
printf '// 更多开发\n' >> feature.c
GIT_AUTHOR_DATE="2026-01-01T10:00:00 +0800" GIT_COMMITTER_DATE="$GIT_AUTHOR_DATE" $G commit -q -a -m "x3 feature 继续开发"
$G switch -q main
for i in $(seq 13 22); do
  printf 'y%d\n' "$i" > "notes/n$i.txt"; $G add -A
  GIT_AUTHOR_DATE="2026-01-01T10:00:00 +0800" GIT_COMMITTER_DATE="$GIT_AUTHOR_DATE" $G commit -q -m "m$i 主干上的普通改动"
done
GIT_AUTHOR_DATE="2026-01-01T10:00:00 +0800" GIT_COMMITTER_DATE="$GIT_AUTHOR_DATE" $G merge -q --no-ff experimental -m "Merge branch 'experimental'"
R "cd $SB/bm && $G log --graph --oneline | sed -n '1,9p' && echo -n '提交总数 = ' && $G rev-list --count HEAD"
note "先看普通 bisect 怎么走：注意它第一次就问到了 experimental 分支内部的 x1（那个提交从来没在主干上待过）"
R "cd $SB/bm && $G bisect start HEAD base_ok && $G bisect run ./test.sh"
R "cd $SB/bm && echo '### 它问过的顺序（主干和分支混在一起）：' && $G bisect log | grep -E '^# (good|bad):' | sed 's/\[[0-9a-f]*\] //' && $G bisect reset"
note "换成 --first-parent：只在主干那一列里二分，合并进来的整块被当成一个单元"
R "cd $SB/bm && $G bisect start --first-parent HEAD base_ok && $G bisect run ./test.sh"
R "cd $SB/bm && echo '### 结论停在合并提交上（这才是符合直觉的答案：它就是『坏代码进入主干』的那一刻）：' && $G bisect log | grep -E '^# (good|bad|first)' | sed 's/\[[0-9a-f]*\] //' && $G bisect reset"
note "最后一击：bad 取合并的第二父亲（分支那一侧），good 取第一父亲（主干那一侧），再二分一次就钻进分支了"
R "cd $SB/bm && $G bisect start 'HEAD^2' 'HEAD^1' && $G bisect run ./test.sh && $G bisect reset"

# =========================================================
say "5. reflog：HEAD 的移动史 + 误删分支的完整救场"
# =========================================================
cd "$SB/inv"
note "① reflog 是『HEAD 待过哪些地方』的流水账：只存本地，不推不拉，别人看不到"
R "cd $SB/inv && $G reflog | sed -n '1,14p'"
R "cd $SB/inv && echo '### 它在磁盘上就是一个纯文本文件：旧哈希 新哈希 身份 <时间戳> TAB 动作' && cat \"$SB/inv/.git/logs/HEAD\" | sed -n '1,4p'"
note "② git log -g 是同一份东西的『log 视图』（-g = --walk-reflogs），所以能加 --oneline / --grep"
R "cd $SB/inv && $G log -g --oneline | sed -n '1,8p'"
R "cd $SB/inv && $G log -g --format='%h %gd %gs' | sed -n '1,4p'"
R "cd $SB/inv && echo '### 带时间的写法（--date 控制 @{...} 里显示成什么）：' && $G reflog --date=iso | sed -n '1,4p'"
note "③ 索引语法：HEAD@{0} = 最新，HEAD@{1} = 上一次……；分支@{n} 是这根分支指针自己的历史"
R "cd $SB/inv && $G rev-parse 'HEAD@{0}' 'HEAD@{1}' 'HEAD@{2}'"
R "cd $SB/inv && $G log --oneline -1 'HEAD@{2}'"
R "cd $SB/inv && echo '### 分支自己的 reflog（磁盘上叫 .git/logs/refs/heads/main）：' && $G log -g --oneline main | sed -n '1,5p'"
R "cd $SB/inv && echo '### @{-1} = 上一个待过的分支（来回切的时候特别好用）：' && $G switch -q -c tmp/scratch && $G switch -q main && $G rev-parse --abbrev-ref '@{-1}'"
note "@{u} = 当前分支的 upstream，必须先有跟踪关系（第 07 章细讲）。这里现搭一个本地 bare 当远程："
$G init --bare -q "$SB/inv-origin.git"
$G -C "$SB/inv" remote add origin "$SB/inv-origin.git"
$G -C "$SB/inv" push -q -u origin main
R "cd $SB/inv && echo '### 配好跟踪之后 @{u} 就能用了：' && $G rev-parse --abbrev-ref '@{u}' && $G status -sb | sed -n '1,1p'"
R "cd $SB/inv && $G switch -q tmp/scratch && echo '### 没配跟踪的分支上问 @{u} 会怎样：' && $G rev-parse '@{u}'"
R "cd $SB/inv && $G switch -q main"
note "④ 时间语法：@{...} 里还能写日期 —— 这就是『回到昨天那个时刻的 HEAD』"
R "cd $SB/inv && $G reflog --date=iso | sed -n '1,6p' && echo \"### 今天是 $(date +%F)，上面这些时间点就是提交时间\" && $G rev-parse 'HEAD@{yesterday}'"
R "cd $SB/inv && $G log --oneline -1 'HEAD@{yesterday}'"
R "cd $SB/inv && echo '### 提醒：时间点超出 reflog 覆盖范围时 git 不报错，它直接把最新那条给你 —— 别把 @{...} 当时光机' && $G rev-parse 'HEAD@{100 years ago}' && $G rev-parse 'HEAD@{0}'"
note "⑤ --since / --until 里也能写人话日期，但注意粒度：'yesterday' = 昨天『这一刻』，不是昨天 0 点"
R "cd $SB/inv && echo '### 现在是 $(date +%T)，所以『昨天这一刻』比最后一次提交还晚一点点 → 空：' && $G log --oneline --since='yesterday' --format='%h %ad %s' --date=short"
R "cd $SB/inv && echo '### 想要『从昨天 0 点开始』，就写清楚时间：' && $G log --oneline --since='yesterday 00:00' --format='%h %ad %s' --date=short"
R "cd $SB/inv && $G log --oneline --since='2026-01-01 00:00' --until='3 weeks ago' --format='%h %ad %s' --date=short | sed -n '1,4p'"
note "⑥ 硬核实战：误删分支（连哈希都没记）→ 从 reflog 找回 → 原地复活"
$G switch -q -c secret/大改造
printf '\n/* 大改造第一步 */\nint big_change(void) { return 1; }\n' >> src/calc.c
$G add -A && mkc '0 days ago' '学习者' 'learn@example.com' 'refactor: 大改造第一步'
printf '\n/* 大改造第二步 */\nint big_change2(void) { return 2; }\n' >> src/calc.c
$G add -A && mkc '0 days ago' '学习者' 'learn@example.com' 'refactor: 大改造第二步（还没测）'
R "cd $SB/inv && echo '### 这条分支上有两个只有我自己知道的提交：' && $G log --oneline -3"
LOST=$($G rev-parse HEAD)
R "cd $SB/inv && echo \"### 顶点哈希（真实场景你不会去记它，这里只是待会儿用来对照答案）：$LOST\""
R "cd $SB/inv && $G switch -q main && $G branch -D secret/大改造"
R "cd $SB/inv && echo '### 现在分支列表里没有它了（tmp/scratch 和 remotes/origin/main 是前面几步留下的）：' && $G branch -a && echo '### main 的历史里也搜不到这两个提交：' && $G log --oneline --grep='大改造' | sed -n '1,3p' && echo '(上面空 = 从分支上彻底看不见了)' "
R "cd $SB/inv && echo '### 但 reflog 里全都记着（刚才那条 Deleted branch ... 提示本身就给了哈希）：' && $G reflog | sed -n '1,8p'"
R "cd $SB/inv && echo '### 找法一：reflog 里按动作搜（-g 走的就是 reflog）' && $G log -g --oneline --grep='大改造'"
R "cd $SB/inv && echo '### 找法二：直接翻流水账' && $G reflog | grep '大改造'"
R "cd $SB/inv && echo '### 顺手确认它真的还在对象库里：' && $G cat-file -t $LOST && $G show --stat --format='%h %s' $LOST | sed -n '1,6p'"
R "cd $SB/inv && $G branch secret/大改造 $LOST && echo '### 复活了：' && $G branch --list 'secret/*' && $G log --oneline -3 secret/大改造"
R "cd $SB/inv && echo '### 全程不记哈希也能救：reflog 的下标直接当起点用。上面写着 HEAD@{1}: commit: 大改造第二步，那就用它：' && $G log --oneline -3 'HEAD@{1}'"
R "cd $SB/inv && echo '### 两种写法指向同一个提交（这才叫确认无误）：' && $G rev-parse --short 'HEAD@{1}' && $G rev-parse --short secret/大改造"

# =========================================================
say "6. fsck / dangling / gc：Git 为什么『很难真的丢东西』"
# =========================================================
$G init -q "$SB/gc"
cd "$SB/gc"
printf 'v1\n' > a.txt; $G add -A
GIT_AUTHOR_DATE="2026-01-01T10:00:00 +0800" GIT_COMMITTER_DATE="2026-01-01T10:00:00 +0800" $G commit -q -m "g1 起点"
G1=$($G rev-parse HEAD)
printf 'v2\n' > a.txt; $G add -A
GIT_AUTHOR_DATE="2026-01-01T10:00:00 +0800" GIT_COMMITTER_DATE="2026-01-01T10:00:00 +0800" $G commit -q -m "g2: 我做了很久的工作"
G2=$($G rev-parse HEAD)
printf 'v3\n' > a.txt; $G add -A
GIT_AUTHOR_DATE="2026-01-01T10:00:00 +0800" GIT_COMMITTER_DATE="2026-01-01T10:00:00 +0800" $G commit -q -m "g3 又改了一点"
G3=$($G rev-parse HEAD)
R "cd $SB/gc && $G log --oneline --format='%h %s'"
R "cd $SB/gc && echo '### 手滑：reset --hard 回到最早的提交，g2 和 g3 从分支上掉了' && $G reset --hard \"$G1\" && $G log --oneline"
note "① 对象其实还在库里：直接问 cat-file"
R "cd $SB/gc && echo \"### g2（$($G log -1 --format='%h' $G2)) 还在不在？\" && $G cat-file -t \"$G2\" && $G log -1 --oneline \"$G2\""
note "② git fsck 检查仓库一致性。注意：默认它把 reflog 也算作『引用』，所以此刻 g2/g3 还不算悬空"
R "cd $SB/gc && $G fsck"
R "cd $SB/gc && echo '### 加上 --no-reflogs（假装 reflog 不存在）才露出真身：' && $G fsck --no-reflogs"
note "③ dangling = 悬空：没有任何指针指向它。--lost-found 会把这些提交的内容写成文件，方便逐个翻"
R "cd $SB/gc && $G fsck --lost-found --no-reflogs && ls .git/lost-found/commit/"
R "cd $SB/gc && echo '### 注意只列出 1 个：fsck 只报链条顶端（g3），g2 是它的父亲，顺着 g3 就能找到' && for f in .git/lost-found/commit/*; do $G log --oneline -3 \"\$(basename \"\$f\")\"; done"
note "④ 于是可以救：把分支指针挪回那个悬空提交，两个提交一起回来"
R "cd $SB/gc && $G reset --hard \"$G3\" && $G log --oneline"
note "⑤ 顺带认识一下 dangling blob：add 过、后来又没提交的内容，同样躺在对象库里"
R "cd $SB/gc && printf '这段草稿后来没提交\n' > draft.txt && $G add draft.txt && $G reset -q draft.txt && rm draft.txt && $G fsck --no-reflogs"
R "cd $SB/gc && echo '### 把那个悬空 blob 的内容捞出来（这就是『找回没提交过的草稿』唯一的办法，前提是 add 过）' && $G fsck --no-reflogs | awk '/^dangling blob/ {print \$3}' | sed -n '1,1p' | xargs -r $G cat-file -p"
note "⑥ 90 天回收站：gc 的默认策略是『reflog 里的记录满 90 天才删，不可达对象满 2 周才删』"
R "cd $SB/gc && echo '### 本仓库有没有配过 gc.* 选项：' && $G config --local --list | grep -i '^gc' || echo '(一条都没有 → 全部用 git 内置默认值：gc.reflogExpire=90.days / gc.reflogExpireUnreachable=30.days / gc.prune=expire,expire-2-weeks)'"
R "cd $SB/gc && echo '### 再手滑一次，把 g2/g3 从分支上甩掉：此时它们还活着（reflog + 2 周宽限期）：' && $G reset --hard \"$G1\" && $G fsck --no-reflogs"
R "cd $SB/gc && echo '### 强制倒垃圾（真实项目里别乱跑）：先让所有 reflog 立刻过期，再 gc --prune=now' && $G reflog expire --expire=now --all && $G gc --prune=now --quiet && echo '### 倒完之后 fsck 已经什么都找不出来：' && $G fsck --no-reflogs && echo '(上面空 = 再也没有悬空对象了)' && $G cat-file -t \"$G2\""
note "上面最后那个报错就是『真被删掉了』的证据 —— 所以救场要趁早，别等 gc 跑过。"
note "⑦ 唯一救不了的情况：从来没 git add 过的文件，压根没进过对象库"
R "cd $SB/gc && printf 'ONLY-ON-DISK-SECRET-42 这行从没进过对象库\n' > 只在磁盘上的文件.txt && $G status --short && $G -c core.quotepath=false status --short && $G fsck --no-reflogs | wc -l"
note "顺带一个小坑：中文文件名默认被转义成一串八进制（core.quotepath=true），加 -c core.quotepath=false 才看得舒服。"
R "cd $SB/gc && $G fsck --no-reflogs | wc -l"
R "cd $SB/gc && rm -f 只在磁盘上的文件.txt && echo '### 删掉之后，在整个对象库里搜这串字符：' && $G log --all -S'ONLY-ON-DISK-SECRET-42' --oneline | wc -l && $G fsck --unreachable --no-reflogs | wc -l"
R "cd $SB/gc && echo '### 一条都没有 → 它确实没进过对象库，git 无能为力。结论：只有 add 过的内容才谈得上恢复' && $G status --short && echo '(空 = 工作区已干净)'"

printf '\n=== demo 结束：以上每一段 \$ 命令和它下面的输出，都是刚刚真跑的 ===\n'
