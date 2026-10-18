#!/bin/sh
# 08 章 bisect demo 用的「好/坏判定脚本」。
# 约定（git bisect run 就靠这个约定工作）：
#   退出码 0    = 这个版本是好的（good）
#   退出码 1-49 = 这个版本是坏的（bad）
#   退出码 125  = 这个版本没法测（skip，比如编译不了、依赖缺失）
#   其它退出码  = 认为出了事故，bisect 立刻停止（比如 126/127 = 脚本没权限/找不到）
#
# 它做两件事：编译当前 checkout 出来的源码，然后检查 bonus(7) 是不是 70。
set -u

# 编译失败：这是个「测不了」的版本，交还给 git 自己决定（125 = skip）
if ! cc -O0 -o /tmp/bisect-calc.$$ calc.c main.c 2>/dev/null; then
    echo "SKIP 编译不过"
    rm -f /tmp/bisect-calc.$$
    exit 125
fi

out=$(/tmp/bisect-calc.$$ 2>/dev/null)
rc=$?
rm -f /tmp/bisect-calc.$$

if [ $rc -ne 0 ]; then
    echo "BAD  程序退出码 $rc"
    exit 1
fi
if [ "$out" = "70" ]; then
    echo "GOOD bonus(7) = $out"
    exit 0
else
    echo "BAD  bonus(7) = $out（期望 70）"
    exit 1
fi
