#!/usr/bin/env bash
# ============================================================================
# case_dispatch.sh —— case 的模式匹配与三种终止符（版本/后缀分派的主战场）
#
# 这个文件在演什么：
#   ① case 的模式是 **glob**，不是正则：`a.c` 里的点号就是字面点，`*` 才是任意串；
#   ② 三种终止符 `;;` / `;&` / `;;&` 的差别，用同一组输入跑出三张不同的轨迹；
#   ③ `|` 是「或」，`*)` 兜底必须放最后（放最前会吞掉后面所有分支）；
#   ④ 版本分派：case 版和 if-elif 版对同一组输入给出一致结论，但 case 少一半行数，
#      而且 case 的匹配是「从上往下第一个命中的模式」—— 顺序即优先级；
#   ⑤ 后缀分派 + `shopt -s nocasematch`（大小写不敏感）；
#   ⑥ case 语句整体退出码：分支体跑完取最后一条命令，一个都不命中时是 0（坑）。
#
# 为什么要演：
#   数据结构作业里「按前缀/后缀分派」到处要用。if-elif 串下来既长又容易写错比较符，
#   case 是 bash 里唯一「模式即语法」的地方，值得单独一章演清楚。
#
# 安全说明：只在当前目录（沙箱）造 cd_ 前缀的样本文件名，不改动任何已有文件。
# ============================================================================
set -u

# ----------------------------------------------------------------------------
printf '\n════ 1. 三种终止符：同一段代码，三种轨迹 ════\n'
# ----------------------------------------------------------------------------
cat > cd_term.sh <<'CASEBLOCK'
dispatch() {
  case "$1" in
    1.*) echo "    [命中 1.*]";;
    2.*) echo "    [命中 2.*]"; ;&          # 落穿：不测试下面的模式，直接执行它的体
    2.9) echo "    [执行 2.9 的体（因为上面是 ;&）]";;
    [0-9].*) echo "    [命中 [0-9].*]"; ;;
    *) echo "    [兜底 *]"; ;;
  esac
}
for v in 1.0.0 2.5.1 2.9 9.9 xyz; do
  echo "  输入 $v："
  dispatch "$v"
done
CASEBLOCK
bash cd_term.sh

echo "  ── 换成 ;;&（每个后续分支都仍然要测试模式）──"
cat > cd_term2.sh <<'CASEBLOCK'
tally() {
  local tags=""
  case "$1" in
    *error*) tags="$tags[error]"; ;&        # 命中后继续测试下一个模式
    *slow*)  tags="$tags[slow]";  ;;
    v1.*)    tags="$tags[v1]";   ;;
    *)       ;;
  esac
  echo "  $1 → ${tags:-（无标签）}"
}
for v in "v1 error here" "v2 slow" "error slow" "clean"; do tally "$v"; done
CASEBLOCK
bash cd_term2.sh

# ----------------------------------------------------------------------------
printf '\n════ 2. 模式是 glob 不是正则 ════\n'
# ----------------------------------------------------------------------------
cat > cd_glob.sh <<'CASEBLOCK'
show() {
  case "$2" in
    $1) echo "  模式「$1」匹配「$2」→ 命中" ;;
    *)  echo "  模式「$1」匹配「$2」→ 不命中" ;;
  esac
}
show 'a.c' 'abc'      # 正则里会命中，glob 里点号是字面点 → 不命中
show 'a.c' 'a.c'
show 'a*c' 'abc'
show 'a*c' 'ac'
show '[0-9]*' '9 lives'
show '[0-9]*' 'nine'
show 'v[0-9]' 'v1'
show '*.log' 'app.log'
CASEBLOCK
bash cd_glob.sh

# ----------------------------------------------------------------------------
printf '\n════ 3. | 或、*) 兜底位置、模式里的空格 ════\n'
# ----------------------------------------------------------------------------
cat > cd_order.sh <<'CASEBLOCK'
pick() {
  case "$1" in
    a|x) echo "  $1 → 命中 a|x" ;;
    "带 空格") echo "  $1 → 命中带引号的模式" ;;
    带\ 空格2) echo "  $1 → 命中反斜杠转义的空格" ;;
    *) echo "  $1 → 兜底" ;;
  esac
}
pick a; pick x; pick 带\ 空格; pick 带\ 空格2; pick z
echo "  ── 把 *) 放到第一位会怎样 ──"
swallow() {
  case "$1" in
    *) echo "  $1 → 被第一位的 *) 吞掉，后面的分支永远轮不到" ;;
    specific) echo "  永远不会打印" ;;
  esac
}
swallow specific
CASEBLOCK
bash cd_order.sh

# ----------------------------------------------------------------------------
printf '\n════ 4. 版本分派：case 版 vs if-elif 版（同一组输入，结论逐字一致）════\n'
# ----------------------------------------------------------------------------
cat > cd_version.sh <<'CASEBLOCK'
by_case() {
  case "$1" in
    1.*)        echo "    v1：只支持 legacy 接口" ;;
    2.[0-4]*)   echo "    v2.0-2.4：legacy + new 双轨" ;;
    2.[5-9]*)   echo "    v2.5+：默认 new 接口" ;;
    3*)         echo "    v3：只有 new 接口" ;;
    v*)         echo "    带 v 前缀的写法：去掉 v 再来一次" ;;
    *)          echo "    无法识别的版本号" ;;
  esac
}
by_if() {
  local v=$1
  if   [[ $v == 1.* ]];      then echo "    v1：只支持 legacy 接口"
  elif [[ $v == 2.[0-4]* ]]; then echo "    v2.0-2.4：legacy + new 双轨"
  elif [[ $v == 2.[5-9]* ]]; then echo "    v2.5+：默认 new 接口"
  elif [[ $v == 3* ]];       then echo "    v3：只有 new 接口"
  elif [[ $v == v* ]];       then echo "    带 v 前缀的写法：去掉 v 再来一次"
  else                            echo "    无法识别的版本号"
  fi
}
for v in 1.0 2.3 2.7 3.1 v2.7 abc; do
  echo "  输入 $v"
  echo "  case 版："; by_case "$v"
  echo "  if   版："; by_if  "$v"
done
CASEBLOCK
bash cd_version.sh

# ----------------------------------------------------------------------------
printf '\n════ 5. 后缀分派 + nocasematch ════\n'
# ----------------------------------------------------------------------------
cat > cd_suffix.sh <<'CASEBLOCK'
by_ext() {
  case "$1" in
    *.log|*.LOG) echo "  $1 → 日志：走日志轮转" ;;
    *.csv)       echo "  $1 → 表格：走 csv 解析" ;;
    *.tar.gz)    echo "  $1 → 压缩包（注意 * 能吃掉整串）" ;;
    *.gz)        echo "  $1 → 其它 gz" ;;
    *.*)         echo "  $1 → 有后缀但没专门处理" ;;
    *)           echo "  $1 → 没有后缀" ;;
  esac
}
for f in access.log data.csv archive.tar.gz notes.txt README noext.LOG; do by_ext "$f"; done
echo "  ── 开 nocasematch 之后（同一个 case，不用把大小写都列出来）──"
shopt -s nocasematch
for f in noext.LOG app.Log; do
  case "$f" in
    *.log) echo "  $f → 只写小写 *.log 也命中（nocasematch=on）" ;;
    *)     echo "  $f → 不命中" ;;
  esac
done
shopt -u nocasematch
CASEBLOCK
bash cd_suffix.sh

# ----------------------------------------------------------------------------
printf '\n════ 6. case 的退出码来自哪一支 ════\n'
# ----------------------------------------------------------------------------
cat > cd_rc.sh <<'CASEBLOCK'
case 9 in
  1) false ;;
esac
echo "  没命中任何分支（只有 1) 这条）→ \$? = $?（空跑的 case 永远 0）"
case 1 in
  1) false ;;
esac
echo "  命中且分支体最后一条失败 → \$? = $?"
case 1 in
  1) true ;;
esac
echo "  命中且分支体成功 → \$? = $?"
case x in
  x) false; ;&
  y) true ;;
esac
echo "  ;& 落穿到下一条体，最后一条是 true → \$? = $?"
CASEBLOCK
bash cd_rc.sh
