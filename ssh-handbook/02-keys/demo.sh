#!/usr/bin/env bash
# ============================================================================
# 02 密钥的生成与使用 —— 一键复现整章场景
#
# 用法：
#   bash demo.sh            # 跑完自动删掉沙箱
#   bash demo.sh --keep     # 跑完保留沙箱并打印路径，你可以进去自己敲命令
#
# 安全声明（照着抄到你自己的实验里）：
#   * 所有 key / known_hosts / agent socket 全在 mktemp 出来的沙箱目录里。
#     绝不读写 ~/.ssh/，绝不改 /etc/ssh/。
#   * 每条 ssh 都带 -F /dev/null（不读你真实的 ~/.ssh/config）
#     和 -o IdentityAgent=none（不把你真实 agent 里的钥匙递出去）。
#     需要演示 agent 时，用的是沙箱里自己起的 agent，socket 在 $SB 下。
#   * 只连 127.0.0.1，不连任何外部主机；公钥不在任何 authorized_keys 里，
#     所以每一脚都会自然停在认证失败 —— 这正是要演示的。
#   * **远端用户名一律写 `learner@127.0.0.1`**（本机不存在这个用户），证书 principals 也签
#     `-n learner`。不写 user@ 时 ssh 会拿你本地的登录名去登，那会把真实姓名印进报错和
#     `ls -l` 的属主列；换成不存在的用户，本章结论一个字都不变（照样停在 Permission denied）。
#     `ls -l` 的属主列和 `ssh-keygen -e` 的 `converted by <用户>@<主机名>` 是 shell/程序自己
#     展开的，demo 拦不住 —— README 里统一替换成 `<本机用户>` / `<主机名>`。
#   * **本脚本从头到尾不会 cat、也不会 tail/head/dd 任何一个私钥文件**。
#     需要「看私钥」的地方一律用 ssh-keygen -y（只输出公钥）代替。
#   * 起了一个沙箱内的 ssh-agent，脚本退出时 trap 会 kill 掉它，最后自证。
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

SB=$(mktemp -d "${TMPDIR:-/tmp}/ssh02-XXXXXX")
chmod 700 "$SB"
AGENT_PID=""
cleanup() {
    # 收尾：先把后台 agent 干掉，再决定留不留沙箱
    if [ -n "${AGENT_PID}" ] && kill -0 "$AGENT_PID" 2>/dev/null; then
        kill "$AGENT_PID" 2>/dev/null || true
        printf '\n[已杀掉沙箱 ssh-agent] pid=%s\n' "$AGENT_PID"
    fi
    if [ "$KEEP" -eq 1 ]; then
        printf '[沙箱已保留] %s\n  进去自己敲： cd "%s" && ls -l\n  看完自己删： rm -rf "%s"\n' "$SB" "$SB" "$SB"
    else
        rm -rf "$SB"
        printf '[沙箱已删除] %s\n' "$SB"
    fi
}
trap cleanup EXIT

scene() { printf '\n\n================ 场景 %s ================\n' "$*"; }
say()   { printf '\n--- %s\n' "$*"; }
raw()     { printf '\n$ %s\n' "$*"; bash -c "$*" || printf '  [退出码 %s：多半是 grep 没匹配到，不是脚本报错]\n' "$?"; }
mayfail() { printf '\n$ %s\n' "$*"; bash -c "$*" || printf '  （退出码 %s —— 这条就是要看它失败/报错）\n' "$?"; }
# rssh：跑一条"理应走到认证阶段"的 ssh。sshd 被并发握手淹了会临时回
#   "Not allowed at this time" / RST，那不是配置问题，重试就好。
rssh() {
    local i out
    for i in 1 2 3 4 5 6 7 8 9 10; do
        out=$(bash -c "$*" 2>&1) || true
        case "$out" in
            *"Not allowed at this time"*|*"Connection reset by peer"*|*"Connection closed by"*|*"No route to host"*)
                printf '  （sshd 正忙，第 %s 次重试…）\n' "$i"; sleep 1; continue ;;
        esac
        printf '%s\n' "$out"
        return 0
    done
    printf '%s\n' "$out"
    return 0
}
export -f rssh

# wait_sshd：ssh-keyscan 没有 rssh 那种重试 —— sshd 被并发握手淹了（MaxStartups）时它
#   直接「静默返回空」，后面的 -F/-R/-H 整段就全塌了。所以每次 keyscan 前先确认
#   sshd 此刻能正常回 banner（这一步只是被动观察 22 端口，不碰 sshd 本身）。
wait_sshd() {
    local i banner=""
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
        banner=$(timeout 5 bash -c 'exec 3<>/dev/tcp/127.0.0.1/22; head -n 1 <&3' 2>/dev/null || true)
        case "$banner" in
            SSH-2.0*) return 0 ;;
            *) printf '  （sshd 现在回：%s —— 1 秒后重试，第 %s 次）\n' "${banner:-没有响应}" "$i"; sleep 1 ;;
        esac
    done
    printf '  【提醒】sshd 持续限流，keyscan 可能拿到空结果；重跑一遍通常就好。\n'
    return 1
}

SSHD_OK=1
if ! timeout 5 bash -c 'exec 3<>/dev/tcp/127.0.0.1/22' 2>/dev/null; then
    SSHD_OK=0
    echo "【提醒】本机 127.0.0.1:22 上没有可用的 sshd：涉及「连一下」的演示会跳过，"
    echo "        纯离线的（生成密钥、指纹、权限、known_hosts）全部照常。"
fi
export SB SSHD_OK

printf '沙箱：%s\n' "$SB"

# ============================================================================
scene "1：三种密钥类型全真跑一遍（生成时间、文件权限、默认位数）"
# ============================================================================
say "推荐款 ed25519：注意 -C 是注释（人名/邮箱/用途都行），-N 是口令（这里给空串表示不设口令）"
raw "cd $SB && time ssh-keygen -t ed25519 -C 'me@laptop' -f $SB/id_ed25519 -N ''"

say "ecdsa（默认曲线 nistp256，可以 -b 256/384/521）"
raw "cd $SB && ssh-keygen -t ecdsa -b 384 -C 'me@laptop' -f $SB/id_ecdsa -N '' -q"

say "rsa 4096：留兼容性用。看它慢在哪 —— 生成素数要花时间"
raw "cd $SB && time ssh-keygen -t rsa -b 4096 -C 'compat@example.com' -f $SB/id_rsa -N '' -q"

say "生成物长什么样：私钥 600，公钥 644，目录 700（这就是标准答案，场景 6 会演示不遵守会怎样）"
raw "ls -l $SB | awk '{print \$1, \$5, \$9}'"

say "指纹一眼看穿类型和位数"
raw "for f in id_ed25519 id_ecdsa id_rsa; do ssh-keygen -lf $SB/\$f.pub; done"

say "不写 -b 时各类型的默认值（RSA 默认 3072，不是 4096 —— 别想当然）"
raw "cd $SB && ssh-keygen -t rsa -C 'default-bits' -f $SB/id_rsadef -N '' -q && ssh-keygen -lf $SB/id_rsadef.pub"

say "踩坑预告：ed25519 的位数不可选，-b 2048 会被静默忽略（不报错！）"
raw "cd $SB && ssh-keygen -t ed25519 -b 2048 -C 'bit-ignored' -f $SB/id_b2k -N '' -q; ssh-keygen -lf $SB/id_b2k.pub"
mayfail "cd $SB && ssh-keygen -t rsa -b 512 -f $SB/id_tiny -N '' -q"

# ============================================================================
scene "2：公钥那一行到底是什么（逐字段拆开），以及为什么它能公开"
# ============================================================================
say "先看真实的一行公钥"
raw "cat $SB/id_ed25519.pub"

say "逐字段拆：ssh-keygen -y 只从【私钥】算出公钥（这一步是单向的）"
raw "ssh-keygen -y -f $SB/id_ed25519"
say "它和 .pub 文件逐字节相同吗？（除了注释）"
raw "diff <(ssh-keygen -y -f $SB/id_ed25519) $SB/id_ed25519.pub && echo '完全相同 —— 公钥就是私钥的公开部分，不是另一套秘密'"

say "base64 解出来的内部结构：4 字节长度 + 算法名 + 4 字节长度 + 公钥本体"
raw "awk '{print \$2}' $SB/id_ed25519.pub | base64 -d | xxd | head -4"

say "为什么公钥可以随便贴：从公钥反推私钥 = 解离散对数/椭圆曲线难题，目前没有可行办法。"
say "反过来证明一下可行性：从私钥导出公钥是毫秒级的（见下面 time 的真实输出），也就是说私钥一旦泄露，等于公钥 + 签名能力全泄露。"
raw "time ssh-keygen -y -f $SB/id_ed25519 > /dev/null"

say "另一种「泄露」：把 .pub 里的 base64 直接改一个字母会怎样？（校验会失败，不用担心手滑贴错）"
mayfail "printf 'ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAI改一个字符 x\n' > $SB/broken.pub; ssh-keygen -lf $SB/broken.pub"

say "格式互转（贴到某些老系统/云面板要的 RFC 4716 格式时用）"
raw "ssh-keygen -e -f $SB/id_ed25519.pub"
# 反向转换必须用 -f 指文件：`ssh-keygen -i -m RFC4716` 从管道读会报 `-: No such file or directory`
# （实测：-f - 也不行，它把 "-" 当文件名）。所以这里先落成文件再转回来。
raw "ssh-keygen -e -f $SB/id_ed25519.pub > $SB/id_ed25519.rfc4716 && ssh-keygen -i -m RFC4716 -f $SB/id_ed25519.rfc4716"

# ============================================================================
scene "3：指纹与比对 —— 认钥匙靠指纹，不靠肉眼比 base64"
# ============================================================================
say "同一条命令对 公钥 / 私钥 / 证书 都能算，结果必须一致（不一致就是两把钥匙）"
raw "ssh-keygen -lf $SB/id_ed25519.pub; ssh-keygen -lf $SB/id_ed25519"

say "老的 MD5 冒号格式 vs 现在的 SHA256 base64（-E 指定）"
raw "for e in md5 sha256 sha512; do ssh-keygen -lf $SB/id_ed25519.pub -E \$e; done"
say "MD5 那串为什么被换掉：短、易撞（MD5 已被实际攻击），而且冒号分隔没法一眼看出长度。"
say "SHA256 base64 只有 43 个字符，去掉了 padding，比 32 个冒号十六进制短一半，还更抗撞。"

say "实战：怎么确认你要连的服务器指纹没变？（服务端本机跑 ssh-keygen -lf /etc/ssh/ssh_host_*_key.pub，客户端跑下面这条比对）"
if [ "$SSHD_OK" = 1 ]; then
    wait_sshd || true
    raw "ssh-keyscan -t ed25519 127.0.0.1 2>/dev/null | ssh-keygen -lf -"
    say "上面用的是「从 stdin 读公钥」这个技巧：ssh-keygen -lf - 可以直接吃管道。"
else
    say "（sshd 不可用，跳过 keyscan 比对）"
fi

# ============================================================================
scene "4：口令（passphrase）—— 私钥文件被拷走 ≠ 账号被登进去"
# ============================================================================
say "造一把带口令的 key。-N 就是口令；这里用 shell 传参只是为了演示可复现，你自己敲会在交互里输入。"
raw "cd $SB && ssh-keygen -t ed25519 -C 'with-passphrase' -f $SB/id_lock -N 'hunter2-demo'"

say "证据一：加口令后私钥文件变大了（里面多了一层加密信封 + KDF 参数）"
raw "printf '未加密私钥 %s 字节 / 加密后 %s 字节\n' \$(wc -c < $SB/id_ed25519) \$(wc -c < $SB/id_lock)"

say "证据二：文件变了，但密钥本身没变 —— 公钥和指纹完全一致"
raw "ssh-keygen -lf $SB/id_lock.pub; ssh-keygen -lf $SB/id_ed25519.pub | sed 's/with-passphrase/（注释不同不影响）/'"

say "改口令（不用重新生成、不用重新部署到服务器）：ssh-keygen -p"
raw "cd $SB && ssh-keygen -p -P 'hunter2-demo' -N 'newpass-demo' -f $SB/id_lock"
say "换完口令后公钥变了吗？（没变，所以服务器的 authorized_keys 一个字都不用动）"
raw "ssh-keygen -lf $SB/id_lock.pub"

say "口令错了是什么提示"
mayfail "ssh-keygen -y -f $SB/id_lock -P 'wrong-pass'"
say "口令对了才能取出公钥"
raw "ssh-keygen -y -f $SB/id_lock -P 'newpass-demo'"

say "KDF 轮数 -a（拖慢暴力破解；默认 16 轮，这里演示 100 轮的耗时差别）"
raw "cd $SB && ssh-keygen -t ed25519 -C 'kdf' -f $SB/id_kdf -N 'pw-demo' -a 100 -q && time ssh-keygen -p -P 'pw-demo' -N 'pw2-demo' -a 100 -f $SB/id_kdf"
say "结论：口令的强度 >> 轮数。轮数是「防拷贝后离线爆破」的最后一层，不是用来补弱口令的。"

# ============================================================================
scene "5：ssh-agent —— 把口令交出去一次，后面都不用管"
# ============================================================================
say "在自己的沙箱里起一个私有 agent（socket 在沙箱内，不碰 /run/user/1000 那个系统 agent）"
raw "cd $SB && ssh-agent -a $SB/agent.sock -s > $SB/agent.env && cat $SB/agent.env"
AGENT_PID=$(sed -n 's/^SSH_AGENT_PID=\([0-9]*\).*/\1/p' "$SB/agent.env")
export SSH_AUTH_SOCK="$SB/agent.sock"
say "agent 起来了吗（PID $AGENT_PID）"
raw "kill -0 $AGENT_PID && echo '在跑，稍后 trap 会杀掉它'"

say "agent 现在是空的"
mayfail "ssh-add -l"

say "加载一把带口令的 key。ssh-add 要读 /dev/tty 拿口令，脚本里用 SSH_ASKPASS_REQUIRE=force 喂（你交互用不着这招）"
# 注意：askpass 辅助脚本必须输出「口令 + 换行」。这里用 echo（自带换行），不要写成
# printf %s\n —— 在 dash 下那串转义最终输出的是 "口令n"，行尾没有换行，
# ssh-add 等不到行尾会 100% CPU 死循环挂住（本机实测踩过，所以改成 heredoc）。
cat > "$SB/askpass" <<'EOS'
#!/bin/sh
echo newpass-demo
EOS
raw "chmod 700 $SB/askpass && cat $SB/askpass"
raw "SSH_ASKPASS=$SB/askpass SSH_ASKPASS_REQUIRE=force ssh-add $SB/id_lock"

say "现在 agent 里有谁"
raw "ssh-add -l"
say "ssh-add -L 直接给出可贴到 authorized_keys 的内容（这就是 ssh-copy-id 干的活的内核）"
raw "ssh-add -L"

say "加载没口令的 key（不需要 askpass）"
raw "ssh-add $SB/id_ed25519 && ssh-add -l"

if [ "$SSHD_OK" = 1 ]; then
    say "真连一次：注意 -o IdentityAgent=\$SB/agent.sock —— 只用我这个沙箱 agent，日志里会出现 'agent' 字样"
    # 最后那个不存在的 -o IdentityFile 是「挡箭牌」：只要显式给了 IdentityFile，ssh 就不再
    # 挨个探测 ~/.ssh/id_rsa、id_ed25519 …（本手册红线：绝不读写你真实的 ~/.ssh）
    raw "rssh \"ssh -F /dev/null -vv -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=$SB/kh -o IdentityAgent=$SB/agent.sock -o IdentityFile=$SB/_no-default-probe learner@127.0.0.1 true 2>&1 | grep -E 'Server host key|Will attempt key|Offering public key|Permission denied'\""
    say "看点：'Offering public key: ... agent' = 签名是在 agent 里做的，ssh 进程自己没读私钥文件。"
    say "为什么最后仍然 Permission denied：因为这些公钥不在服务器的 authorized_keys 里（本章红线：不制造登录成功）。"
fi

say "删钥匙：-d 删一把，-D 清空"
raw "ssh-add -d $SB/id_ed25519 && ssh-add -l"
raw "ssh-add -D; ssh-add -l || echo '（agent 已空）'"

say "顺手记一条：如果某个项目你想【禁止】用 agent，命令里加 -o IdentityAgent=none（本 demo 从头到尾都带着它）"
if [ "$SSHD_OK" = 1 ]; then
    raw "rssh \"ssh -F /dev/null -v -o BatchMode=yes -o IdentityAgent=none -o IdentityFile=$SB/_no-default-probe -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=$SB/kh learner@127.0.0.1 true 2>&1 | grep -E 'ssh_get_authentication_socket_path|Offering|no such identity|Permission denied'\" ; true"
    say "对比：上面没有 ssh_get_authentication_socket_path 这一行，也没有 agent 来的钥匙 —— 屏蔽成功。"
fi

# ============================================================================
scene "6：权限模型实测（最有价值的一节）—— SSH 为什么对文件权限这么苛刻"
# ============================================================================
say "先把一把私钥改成 644（别人能读）。这是最常见的翻车姿势：从别处 cp / git clone 出来的 key。"
raw "cp $SB/id_ed25519 $SB/noperm && chmod 644 $SB/noperm && ls -l $SB/noperm"

say "离线检查立刻炸：ssh-keygen 只要想读这把私钥就拒绝，报错原文如下"
mayfail "ssh-keygen -y -f $SB/noperm"

say "改口令同样拒绝（说明这是「读私钥」的统一防线，不是某个子命令的毛病）"
mayfail "ssh-keygen -p -f $SB/noperm -N 'x' -P ''"

say "那 ssh 自己呢？—— 这里有个很多人观察不到的细节"
if [ "$SSHD_OK" = 1 ]; then
    raw "rssh \"ssh -F /dev/null -vv -o BatchMode=yes -o IdentitiesOnly=yes -o IdentityAgent=none -i $SB/noperm -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=$SB/kh learner@127.0.0.1 true 2>&1 | grep -E 'identity file|loaded pubkey|Offering|we sent a publickey|Trying private key|bad permissions|Permissions|UNPROTECTED|Permission denied'\""
    say "看清楚了：ssh 先只读【公钥】部分（.pub 或从私钥文件头部取）去做 Offering；"
    say "服务器回 'Authentications that can continue'（=这把钥匙没被授权）之后，ssh 压根不需要打开私钥去签名，"
    say "所以权限检查没被触发 —— 报错也就没出现。"
    say "结论：「权限太开」的报错，出现在「服务器已经认这把公钥、ssh 真要拿私钥签名」的那一刻。"
    say "      你平时看到它，说明钥匙其实是对的、只差最后一步签名 —— 这是好消息，不是坏消息。"
fi

say "标准答案三件套（本机可背）"
raw "printf '%-34s %s\n' '~/.ssh 目录' '700 (drwx------)' '私钥 id_xxx' '600 (-rw-------，不能 644)' '公钥 id_xxx.pub' '644 (-rw-r--r--)' 'known_hosts' '644' 'authorized_keys' '600 或 644（多数发行版 600）'"
say "如果 ~/.ssh 目录本身是 777 或属主不对（比如用 sudo 建过文件），客户端也会拒绝"
raw "mkdir -p $SB/widirdir && cp $SB/id_ed25519 $SB/widirdir/k && chmod 644 $SB/widirdir/k && chmod 755 $SB/widirdir && ls -ld $SB/widirdir && ssh-keygen -y -f $SB/widirdir/k || true"
say "上面这条只改了文件权限没改目录权限 —— ssh 检查的是「私钥文件是否被同组/其他人可读写」，目录权限主要由服务端 StrictModes 管。"

say "服务端同理：sshd 的 StrictModes（默认 yes）会因为 ~/.ssh 或 authorized_keys 权限过开而直接拒绝公钥登录"
say "这段需要你有第二台机器（或服务端 root）时验证，模板："
raw "echo '服务端查配置： sshd -T | grep -i strictmodes'"
raw "echo '服务端查权限： ls -ld ~/.ssh ~/.ssh/authorized_keys'"
raw "echo '服务端看日志： sudo journalctl -u ssh -u sshd --since \"10 min ago\" | grep -i bad'"
raw "echo '典型日志原文： Authentication refused: bad ownership or modes for directory /home/you/.ssh'"

say "顺手一个真会咬人的点：配置文件权限检查只作用于【被 Include 进来的文件】和你自己的 ~/.ssh/config —— 实测门限是「不能被其他人写」"
# 注意 Include 必须写【绝对路径】：相对路径是相对 ~/.ssh 解析的（实测：Include incperm 会去找
# ~/.ssh/incperm，于是本机的 ~/.ssh 被访问到，而且这条演示会静默失效）。
raw "mkdir -p $SB/conf.d; printf 'Include $SB/incperm\n' > $SB/cfg; chmod 600 $SB/cfg; printf 'Host permprobe\n  User zzz\n' > $SB/incperm; chmod 666 $SB/incperm"
raw "ssh -F $SB/cfg -G permprobe 2>&1 | grep -iE 'bad owner|^user '"
raw "chmod 644 $SB/incperm; ssh -F $SB/cfg -G permprobe 2>&1 | grep -E '^user |Bad owner'"
say "同一路径 666 → 直接 Bad owner or permissions；644 → 正常读到。这就是为什么 conf.d 目录要让 root 持有、别人不可写。"
say "再补一条实测（很多人不知道）：-F 指定的那个文件本身【不查权限】，所以下面这条无论怎么 chmod 都能跑 —— 别把安全感放在这上面。"
raw "chmod 666 $SB/cfg; ssh -F $SB/cfg -G permprobe 2>&1 | grep -E '^user |Bad owner'; chmod 600 $SB/cfg"

# ============================================================================
scene "7：known_hosts 的日常维护 —— -F 查 / -R 删 / -H 哈希 / -c 改注释"
# ============================================================================
say "先造一个有两条记录的沙箱 known_hosts（一台机器多种 host key 是常态）"
if [ "$SSHD_OK" = 1 ]; then
    wait_sshd || true
    raw "cd $SB && ssh-keyscan -t ed25519,rsa 127.0.0.1 2>/dev/null > $SB/kh2; cat $SB/kh2"
    if ! grep -q '^127' "$SB/kh2"; then
        say "上一条 ssh-keyscan 空手而归（sshd 正在限流时它既不报错也不重试）—— 等它缓过来再跑一次。"
        wait_sshd || true
        raw "cd $SB && ssh-keyscan -t ed25519,rsa 127.0.0.1 2>/dev/null > $SB/kh2; cat $SB/kh2"
    fi
else
    raw "cd $SB && { ssh-keygen -E sha256 -y -f $SB/id_ed25519 >/dev/null; printf '127.0.0.1 ssh-ed25519 %s\n' \"\$(awk '{print \$2}' $SB/id_ed25519.pub)\"; printf '127.0.0.1 ssh-rsa %s\n' \"\$(awk '{print \$2}' $SB/id_rsa.pub)\"; } > $SB/kh2; cat $SB/kh2"
fi

say "查：这台机器我记录过哪些钥匙？"
raw "ssh-keygen -F 127.0.0.1 -f $SB/kh2"

say "删：服务器重装/换了 HostKey，'REMOTE HOST IDENTIFICATION HAS CHANGED' 之后一条命令复活"
raw "ssh-keygen -R 127.0.0.1 -f $SB/kh2"
say "-R 会打印它删了哪几行、并把原文件备份成 kh2.old（先备份再改，这是它的好习惯）"
raw "echo '--- 删完以后：'; cat $SB/kh2; echo '--- 备份：'; ls -l $SB/kh2.old"

say "重新灌回去（生产上更常用：让运维给你一个新指纹，核对后 accept-new 一次）"
if [ "$SSHD_OK" = 1 ]; then
    wait_sshd || true
    raw "cd $SB && ssh-keyscan -t ed25519 127.0.0.1 2>/dev/null > $SB/kh3; cat $SB/kh3"
    if ! grep -q '^127' "$SB/kh3"; then
        say "同上，sshd 限流导致空结果 —— 再跑一次。"
        wait_sshd || true
        raw "cd $SB && ssh-keyscan -t ed25519 127.0.0.1 2>/dev/null > $SB/kh3; cat $SB/kh3"
    fi
else
    say "（sshd 不可用：用沙箱里生成的钥匙自己拼一条 known_hosts，纯离线演示继续）"
    raw "cd $SB && { printf '127.0.0.1 ssh-ed25519 %s\n' \"\$(awk '{print \$2}' $SB/id_ed25519.pub)\"; } > $SB/kh3; cat $SB/kh3"
fi

say "哈希：known_hosts 里明文写着你连过哪些机器 —— 这是可以被「顺手拿走」的隐私。-H 把它打码。"
raw "cp $SB/kh3 $SB/kh_hash; ssh-keygen -H -f $SB/kh_hash; head -2 $SB/kh_hash"
say "变成 |1|<salt base64>|<hmac base64>| —— 主机名不再可读，但 ssh 自己还能查（它拿每个已知主机名算一遍 HMAC 比对）"
raw "ssh-keygen -F 127.0.0.1 -f $SB/kh_hash | head -2"
raw "grep -c '^|1|' $SB/kh_hash || true"

say "改注释：-c 只改 .pub 文件里的注释字段，不动密钥本体（私钥带口令时会先要口令）"
raw "cat $SB/id_ed25519.pub"
raw "ssh-keygen -c -C 'laptop-2026-relabel' -f $SB/id_ed25519 -P ''"
raw "cat $SB/id_ed25519.pub"
say "指纹变了吗？（没变 —— 注释是自由文本，改它不影响任何身份）"
raw "ssh-keygen -lf $SB/id_ed25519.pub"

# ============================================================================
scene "8：部署公钥的三条路 + authorized_keys 到底怎么读"
# ============================================================================
say "路线 A：ssh-copy-id --dry-run（-n）：只告诉你它会干什么，一个字都不写。这条本机可跑。"
raw "rssh \"ssh-copy-id -n -i $SB/id_ed25519.pub -F /dev/null -o UserKnownHostsFile=$SB/kh -o StrictHostKeyChecking=accept-new -o IdentityAgent=none -o PreferredAuthentications=publickey learner@127.0.0.1 2>&1\""
say "看点：它先「拿新钥匙试登录一次」来过滤已经装过的（所以 -n 也要能连上），然后打印 Would have added 的那行 —— 就是它准备 append 的内容。"

say "去掉 -n 真跑会怎样？（本 demo 故意不制造登录成功，所以它会停在认证失败并原样退出）"
raw "rssh \"ssh-copy-id -i $SB/id_ed25519.pub -F /dev/null -o UserKnownHostsFile=$SB/kh -o StrictHostKeyChecking=accept-new -o IdentityAgent=none -o PreferredAuthentications=publickey -o BatchMode=yes learner@127.0.0.1 2>&1; true\""
say "真服务器上你看到的会是：回车一次密码 → 'Number of key(s) added: 1' → 之后 ssh 免密。这段需要你自己有服务器时验证，模板："
raw "echo 'ssh-copy-id -i ~/.ssh/id_ed25519.pub -p 22 user@服务器IP'"

say "路线 B：手工贴（服务器只给了控制台、或 ssh-copy-id 不可用时）"
raw "echo '客户端： cat ~/.ssh/id_ed25519.pub'"
raw "echo '服务端： install -d -m 700 ~/.ssh && printf \"%s\\n\" \"<上面那一行>\" >> ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys'"

say "路线 C：云面板导入 —— 各家控制台都有「密钥对」页，贴的就是 .pub 那一行；本质和路线 B 相同，只是它替你 append。"

say "authorized_keys 的读法：一行 = 一把被允许的钥匙。行首的选项是「这把钥匙的附加限制」，选项不能单独成行。"
# 这里用 heredoc 而不是 printf 一串带引号的参数：原写法少了一个单引号，整条命令是语法错误。
cat > "$SB/authorized_keys.example" <<'EOS'
# 一行一把钥匙；顺序不重要；# 开头是注释
ssh-ed25519 AAAA...comment1
from="10.0.0.7,192.168.1.0/24" ssh-ed25519 AAAA...只允许这些来源
command="/usr/bin/rsync --server",no-pty,no-agent-forwarding ssh-ed25519 AAAA...只允许跑 rsync（部署 key 的原理）
restrict ssh-ed25519 AAAA...关掉几乎所有能力（新版本里的总开关）
EOS
raw "cat $SB/authorized_keys.example"
say "上面这几行只是【格式演示】。真实生效需要服务端配置 AuthorizedKeysFile 指向它，并且语法要对（写错一个逗号整行失效）。"
say "这段需要你自己有服务器时验证，命令模板："
raw "echo '服务端： sshd -T | grep -iE \"authorizedkeysfile|permitrootlogin|passwordauthentication\"'"
raw "echo '限制来源： from=\\"堡垒机IP\\" ssh-ed25519 AAAA...'"
raw "echo '强制命令（deploy key / 备份账号常用）：'"
raw "echo 'command=\"/usr/bin/rsync --server --sender -logDtpre.iLsfxCIvu . /data/backup\",no-port-forwarding,no-agent-forwarding,no-pty,no-user-rc ssh-ed25519 AAAA...'"
raw "echo '客户端验证： rsync -av 服务器:/data/backup/ ./local/  —— 只能拉，登不进 shell'"

# ============================================================================
scene "9：进阶 —— 证书（把「每台机器贴一次公钥」变成「信任一个 CA」）"
# ============================================================================
say "建一个 CA（离线保管，这里只是演示）"
raw "cd $SB && ssh-keygen -t ed25519 -C 'demo-ca' -f $SB/ca_key -N '' -q && ssh-keygen -lf $SB/ca_key.pub"

say "用 CA 给公钥签一张 52 周有效期的用户证书"
raw "cd $SB && ssh-keygen -s $SB/ca_key -I 'demo-identity' -n learner -V +52w -z 42 $SB/id_ed25519.pub"

say "证书本身的可读信息 —— ssh-keygen -Lf（比 .pub 那一行多了有效期、principals、权限扩展）"
raw "ssh-keygen -Lf $SB/id_ed25519-cert.pub"

say "指纹会带 -CERT 后缀，看到它就知道服务器上是证书而不是裸公钥"
raw "ssh-keygen -lf $SB/id_ed25519-cert.pub"
say "证书旁边要放一个不带 -cert 的 .pub 同名文件（OpenSSH 靠它找原始公钥），别删。"
raw "ls -l $SB | grep id_ed25519"

# ============================================================================
scene "10：收尾自证"
# ============================================================================
say "有没有残留监听端口（本章只连别人，不监听，应该是空的）"
raw "ss -lntp 2>/dev/null | grep -E 'LISTEN' | awk '{print \$4, \$6, \$7}'"
say "有没有残留我起的 ssh-agent（只有沙箱 socket 那个；系统自带的 gcr/keyring agent 不算，本来就在）"
raw "pgrep -af 'ssh-agent' || echo '（只有系统自带的，或者为空）'"
say "沙箱里都留了什么文件（私钥全部 600，公钥 644）"
raw "find $SB -maxdepth 1 -type f -printf '%m %p\n' | sort -k2"
say "本脚本从头到尾没有任何一句 cat/head/tail/dd 作用在私钥上；要「看私钥」的地方都用了 ssh-keygen -y（只吐公钥）。"

printf '\n全部演示结束。\n'
