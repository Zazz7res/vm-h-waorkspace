#!/usr/bin/env bash
# ============================================================================
# and_or_trap.sh —— && || ! 与 if/elif：`A && B || C` 为什么不是 if-else
#
# 这个文件在演什么：
#   ① if 后面跟的是**任意命令**，不是布尔值：grep、函数、甚至 exit 都能当条件；
#   ② `A && B || C` 的真语义是「A 成功则跑 B；（A 失败 或 B 失败）则跑 C」——
#      B 自己失败时也会跑 C，这一条必须用真实函数复现；
#   ③ 同一段逻辑，&& || 版和 if-else 版在 set -e 下的**存活情况不一样**；
#   ④ && 和 || 是「从左往右依次判断」，没有「&& 优先级高于 ||」这回事；
#   ⑤ ! 取反的两个副作用：让 set -e 放弃保护、以及 `if ! cmd` 和 `if cmd; then…else…`
#      的可读性差别（后者多一层：cmd 内部失败时 set -e 同样失效）；
#   ⑥ 短路之后 `$?` 是谁的：三种写法的最终退出码各不相同。
#
# 为什么要演：
#   `cmd && ok || fail` 是网上最流行的「一行 if」，它在 B 会失败的场景里静默走错分支，
#   而且永远以 0 收场 —— CI 只看退出码时，这类 bug 不会被发现。
#
# 安全说明：所有「失败」都是真函数 return 1，不杀进程、不写系统文件；
#   要跑 set -e 就用 `bash -ec` 起独立子 shell，主脚本自己不受影响。
# ============================================================================
set -u

try() {
    printf '\n  $ %s\n' "$1"
    bash -c "$1" 2>&1 | sed 's/^/      /'
    printf '      ↪ rc=%d\n' "${PIPESTATUS[0]}"
}

printf '\n════ 1. if 后面跟的是任意命令（条件 = 退出码 = 0 吗）════'
try 'printf "name=alice\nport=8080\n" > rp_conf.ini
if grep -q "name=alice" rp_conf.ini; then echo "grep 当条件：找到了 → then"; else echo "没找到 → else"; fi'
try 'printf "name=alice\n" > rp_conf.ini
if grep -q "name=bob" rp_conf.ini; then echo then; else echo "bob 不在配置里 → else（grep 返回 1）"; fi'
try 'printf "name=alice\n" > rp_conf.ini
if grep -q alice rp_MISSING.ini; then echo then; else echo "grep 返回 2（干不了活）也算条件不成立 → else"; fi'
try 'if bash -c "exit 3"; then echo then; else echo "子命令退出码 3 ≠ 0 → else；整条 if 的 rc=$?"; fi'
try 'ok(){ return 0; }; if ok; then echo "函数也能当条件"; fi'
try 'if :; then echo "连冒号（什么都不做）都能当条件，它的退出码是 0"; fi'

printf '\n════ 2. 经典反例：A && B || C 里 B 失败也会跑 C ════'
try 'A(){ echo "  A 成功"; return 0; }
B(){ echo "  B 开始干活…然后失败"; return 1; }
C(){ echo "  C（兜底）被调用"; }
echo "── 写法一：A && B || C"
A && B || C
echo "── 写法二：等价于 if-else 吗？看 if 版"
if A; then B; else C; fi
echo "  ↑ if 版里 C 没有被调用，&& || 版里 C 被调用：两者不等价"'

printf '\n════ 3. 同一件事，两种写法在 set -e 下的结局 ════'
try 'cat > ao_chain.sh <<'"'"'SCRIPT'"'"'
set -e
A(){ return 0; }
B(){ return 1; }
C(){ echo "  C 跑了"; }
A && B || C
echo "  && || 版：脚本还活着（整条链最后是 C 的 0）"
SCRIPT
bash ao_chain.sh; echo "  ao_chain.sh 退出码=$?"'
try 'cat > ao_if.sh <<'"'"'SCRIPT'"'"'
set -e
A(){ return 0; }
B(){ return 1; }
C(){ echo "  C 跑了"; }
if A; then B; else C; fi
echo "  if 版：不该走到这里"
SCRIPT
bash ao_if.sh; echo "  ao_if.sh 退出码=$?（B 失败直接终止脚本）"'

printf '\n════ 4. && 和 || 是从左往右依次执行，没有优先级 ════'
try 'true || false && echo "X 被打印（说明 && 不比 || 优先，纯左结合）"'
try 'false && true || echo "Y 被打印"'
try 'echo 1 && echo 2 || echo 3 && echo 4'
try 'echo 1 && false || echo 3 && echo 4'
try 'false && echo A || echo B && echo C'

printf '\n════ 5. ! 取反：判据对了，但 set -e 的保护没了 ════'
try 'printf "%s\n" "set -e" "false" "echo 不该走到这里" > plain_e.sh; bash plain_e.sh; echo "  plain_e.sh 退出码=$?"'
try 'printf "%s\n" "set -e" "! true" "echo 被 ! 保护：还活着" "echo 到这里退出码=\$?" > bang_e2.sh; bash bang_e2.sh; echo "  bang_e2.sh 退出码=$?"'
try 'bash -ec "if ! grep -q zzz rp_conf.ini; then echo 活着; fi; echo if 里用 ! 也照样不受 set -e 影响"'
try 'cat > bang_e.sh <<'"'"'SCRIPT'"'"'
set -e
worker(){ echo "  worker 第 1 步"; false; echo "  worker 第 3 步（本来不该执行）"; }
if ! worker; then echo "  走了 ! 分支"; fi
echo "  脚本活着，退出码=$?"
SCRIPT
bash bang_e.sh; echo "  bang_e.sh 退出码=$?"'
try 'cat > nobang_e.sh <<'"'"'SCRIPT'"'"'
set -e
worker(){ echo "  worker 第 1 步"; false; echo "  worker 第 3 步"; }
if worker; then echo "  走了 then"; fi
echo "  不该走到这里"
SCRIPT
bash nobang_e.sh; echo "  nobang_e.sh 退出码=$?"'
try 'if ! grep -q zzz rp_MISSING.ini 2>/dev/null; then echo "! 之后：grep 失败被取反 → 走 then（注意 grep 的 2 也被吃掉了）"; fi'

printf '\n════ 6. if ! cmd 与 if cmd; then…else…：真值表相同，读法不同 ════'
try 'has(){ grep -q alice rp_conf.ini 2>/dev/null; }
if ! has; then echo "! 版：没有 alice"; fi
if has; then echo "正版：有 alice"; else echo "正版：没有 alice"; fi
echo "  两条结论一致；差别在 else 之后还能不能继续判断（推荐正着写）"'
try 'if ! [[ -e rp_MISSING.ini ]]; then echo "![[ ]] 与 [[ ! ]] 都能用"; fi
if [[ ! -e rp_MISSING.ini ]]; then echo "  两种否定写法等价"; fi'

printf '\n════ 7. 短路之后 $? 是谁的 ════'
try 'false && true; echo "false && true → rc=$?"'
try 'true || false; echo "true || false → rc=$?（false 根本没跑）"'
try 'false || true; echo "false || true → rc=$?"'
try 'false && false; echo "false && false → rc=$?（第二个 false 没跑，码是第一个的）"'
try 'true && false || true && false; echo "长链 → rc=$?"'
try '! true; echo "! true → rc=$?"
! false; echo "! false → rc=$?"'
