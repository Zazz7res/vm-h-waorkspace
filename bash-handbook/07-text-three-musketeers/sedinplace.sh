#!/usr/bin/env bash
# ============================================================================
# sedinplace.sh —— sed -i 不是「就地改」：它新建一个文件、写完、再把名字挪过去
#                （第 07 章 sed 半区·之二；立主线②「没匹配上在各工具里的下场不一样」）
#   demo.sh 里这样调：bash sedinplace.sh
#   安全：只在本目录下建 sipdemo/ 沙箱（含 nodir/、glob/、sd/ 三个子目录），样本全用 here-doc 当场造；
#         跑完先把改过的权限复原再删，trap 兜底，第 8 格当场自证残留 0 个。不碰家目录、不联网、不开端口。
#   故意不加 set -e：-i 要读的码是 0（改了）、0（一个字没改）、1（脚本没编译过）、2（文件读不到）、4（临时文件建不出来）—— 一遇非 0 就停，后面每格都看不到。
# ★ 本机不拿 inode 号、时间当证据（inode 每跑必变、耗时每跑必抖）：全换成「是不是同一个」「内容一样吗」
#   「几个」这类判定。目录不可写那一格的报错原文里带着每跑都不同的临时文件名，所以那一格只印退出码。
# ============================================================================

rm -rf sipdemo; mkdir -p sipdemo
trap 'chmod -R u+rwX sipdemo 2>/dev/null; rm -rf sipdemo' EXIT
cat > sipdemo/cfg.txt <<'EOF'
key=old
port=80
EOF
cat > sipdemo/three.txt <<'EOF'
alpha
beta
gamma
EOF

echo '=== 1. -i.bak / -i / -i 空格 .bak：三种写法三种下场 ==='
cp sipdemo/cfg.txt sipdemo/b1.txt; sed -i.bak 's/key=old/key=new/' sipdemo/b1.txt; R=$?
printf '  -i.bak ｜ rc=%s ｜ 备份文件在吗 %s ｜ 备份里就是改前那份吗 %s ｜ 主文件现在是：\n' "$R" "$([ -f sipdemo/b1.txt.bak ] && echo 在 || echo 不在)" "$(cmp -s sipdemo/b1.txt.bak sipdemo/cfg.txt && echo 是 || echo 不是)"
sed 's/^/      | /' sipdemo/b1.txt
cp sipdemo/cfg.txt sipdemo/b2.txt; sed -i 's/key=old/key=new/' sipdemo/b2.txt; R=$?
printf '  -i（不带后缀）｜ rc=%s ｜ 多出任何备份了吗 %s ｜ 目录里跟 b2 有关的文件 %s 个 —— 改坏了当场没有任何退路\n' "$R" "$([ -e sipdemo/b2.txt.bak ] && echo 有 || echo 没有)" "$(ls -A sipdemo | grep -c '^b2')"
cp sipdemo/cfg.txt sipdemo/b3.txt; E=$(sed -i .bak 's/key=old/key=new/' sipdemo/b3.txt 2>&1); R=$?
printf '  -i .bak（带空格）｜ rc=%s ｜ 原文[%s]\n' "$R" "$E"
printf '      主文件动了吗 %s ｜ 有没有真留下一个叫 .bak 的东西 %s 个\n' "$(cmp -s sipdemo/b3.txt sipdemo/cfg.txt && echo 没动 || echo 动了)" "$(ls -A sipdemo | grep -c '^\.bak$')"
echo '  → 直觉「-i 后面跟备份名，空格无所谓」：错。GNU sed 的 -i 只认**贴在一起**的后缀，分开写时 `.bak` 被当成'
echo '    sed 脚本、`s/key=old/key=new/` 被当成文件名 —— 上面那行英文就是它的原话，一个字节都没改。'

echo
echo '=== 2. ★ -i 到底是「改这个文件」还是「换个文件」：改前改后是不是同一个 ==='
cp sipdemo/cfg.txt sipdemo/i1.txt; I0=$(stat -c %i sipdemo/i1.txt); sed -i 's/key=old/key=new/' sipdemo/i1.txt; I1=$(stat -c %i sipdemo/i1.txt)
printf '  名字没变、内容变了 —— 改前改后是同一个 inode 吗 → %s（★ 屏幕上不印号，号每跑必变）\n' "$([ "$I0" = "$I1" ] && echo 相同 || echo 不同)"
cp sipdemo/cfg.txt sipdemo/h1.txt; ln sipdemo/h1.txt sipdemo/h2.txt; H0=$(stat -c %h sipdemo/h1.txt)
sed -i 's/key=old/key=new/' sipdemo/h1.txt
printf '  硬链接：起先这个文件有 %s 个名字 ｜ sed -i h1 之后 h1 的链接数 → %s ｜ h1 和 h2 内容还一样吗 → %s\n' "$H0" "$(stat -c %h sipdemo/h1.txt)" "$(cmp -s sipdemo/h1.txt sipdemo/h2.txt && echo 一样 || echo 不一样)"
printf '      h2 停在改前的值 → %s ← ★ 另一个名字从此和 h1 分家：-i 会断硬链接\n' "$(head -1 sipdemo/h2.txt)"
cp sipdemo/cfg.txt sipdemo/real.txt; ln -s real.txt sipdemo/lk.txt; ( cd sipdemo && sed -i 's/key=old/key=new/' lk.txt )
printf '  符号链接：跑完 lk.txt 还是符号链接吗 %s ｜ 它指着的 real.txt 内容动了吗 %s（-i 把链接本身换成了普通文件）\n' "$([ -L sipdemo/lk.txt ] && echo 是 || echo 不是)" "$(cmp -s sipdemo/real.txt sipdemo/cfg.txt && echo 没动 || echo 动了)"
( cd sipdemo && rm -f lk.txt && ln -s real.txt lk.txt && sed --follow-symlinks -i 's/key=old/key=NEW/' lk.txt )
printf '  加 --follow-symlinks 再跑一遍：还是链接吗 %s ｜ real.txt 里有 key=NEW 了吗 %s\n' "$([ -L sipdemo/lk.txt ] && echo 是 || echo 不是)" "$(grep -q 'key=NEW' sipdemo/real.txt && echo 有 || echo 没有)"
echo '  → 一句话：-i 的方法是「新建临时文件 → 写完 → 改名盖住原名」。老 inode 退休、新 inode 上岗，所以断链接、'
echo '    换 inode；本章没演（要真起进程、要别的用户）：别的进程正打开着它时看不到改动、属主不是自己的文件、跨'
echo '    文件系统的 mv、NFS 上的 -i。'

echo
echo '=== 3. 权限与属主：-i 之后 mode 是什么，只读拦不拦得住 ==='
cp sipdemo/cfg.txt sipdemo/p1.txt; chmod 640 sipdemo/p1.txt; M0=$(stat -c %a sipdemo/p1.txt)
sed -i 's/port=80/port=8080/' sipdemo/p1.txt; R=$?; M1=$(stat -c %a sipdemo/p1.txt)
printf '  640 的样本 ｜ rc=%s ｜ mode 改前 %s、改后 %s ｜ 和原来一样吗 → %s（属主同理没变：还是同一个人在改）\n' "$R" "$M0" "$M1" "$([ "$M0" = "$M1" ] && echo 一样 || echo 不一样)"
cp sipdemo/cfg.txt sipdemo/p2.txt; chmod 444 sipdemo/p2.txt
sed -i 's/key=old/key=new/' sipdemo/p2.txt; R=$?
printf '  ★ 444 只读样本 ｜ rc=%s ｜ 内容改了吗 → %s ｜ mode 还是 %s —— 文件的写权限拦不住 -i\n' "$R" "$(cmp -s sipdemo/p2.txt sipdemo/cfg.txt && echo 没改 || echo 改了)" "$(stat -c %a sipdemo/p2.txt)"
mkdir -p sipdemo/nodir; cp sipdemo/cfg.txt sipdemo/nodir/q.txt; chmod 555 sipdemo/nodir
sed -i 's/key=old/key=new/' sipdemo/nodir/q.txt >/dev/null 2>&1; R=$?
printf '  目录不可写 ｜ rc=%s ｜ 文件内容 → %s ｜ 报错原文故意不印：那行里有每跑都不同的临时文件名\n' "$R" "$(head -1 sipdemo/nodir/q.txt)"
chmod -R u+rwX sipdemo/nodir
echo '  → 直觉「-i 要文件能写」：反了，它要的是**目录能写**（要建新文件、要改名）。所以 mode 被照抄（640 还是 640、'
echo '    444 还是 444），而能不能改成功只看目录 —— 这两件事分开记。'

echo
echo '=== 4. 幂等：同一命令跑两遍，第二遍是「没命中」还是「命中但没变」 ==='
cp sipdemo/cfg.txt sipdemo/a1.txt; sed -i 's/key=old/key=new/' sipdemo/a1.txt; cp sipdemo/a1.txt sipdemo/a1.p1
sed -i 's/key=old/key=new/' sipdemo/a1.txt; R=$?
printf '  A 组 s/key=old/key=new/：两遍后与第一遍逐字节一样吗 → %s ｜ 第二遍 rc=%s ｜ 但第二遍其实一次都没命中\n' "$(cmp -s sipdemo/a1.p1 sipdemo/a1.txt && echo 一样 || echo 不一样)" "$R"
cp sipdemo/cfg.txt sipdemo/b4.txt; sed -i 's/^key=.*/key=new/' sipdemo/b4.txt; cp sipdemo/b4.txt sipdemo/b4.p1
sed -i 's/^key=.*/key=new/' sipdemo/b4.txt
printf '  B 组 s/^key=.*/key=new/：两遍后与第一遍一样吗 → %s ｜ 第二遍命中的行数 → %s（命中了、换回去还是同一个串）\n' "$(cmp -s sipdemo/b4.p1 sipdemo/b4.txt && echo 一样 || echo 不一样)" "$(sed -n '/^key=.*/p' sipdemo/b4.txt | wc -l)"
cp sipdemo/cfg.txt sipdemo/c4.txt
for i in 1 2 3; do sed -i 's/key=old/key=old-v2/' sipdemo/c4.txt; printf '      C 组第 %s 遍后第一行 → %s\n' "$i" "$(head -1 sipdemo/c4.txt)"; done
echo '  ★ 幂不幂等不是 sed 的性质，是**你写的模式**的性质：模式是自己的替换结果的子串时，每跑一遍叠一层；锚定'
echo '    ^…$ 的那种跑一百遍也只收敛到一个值 —— 放进定时任务里的 sed 必须写成后者。'

echo
echo '=== 5. 没命中时 -i 照样退 0：动手前先数一遍（这道守卫要先故意弄响一次）==='
cp sipdemo/cfg.txt sipdemo/g.txt; sed -i 's/key=oldd/key=new/' sipdemo/g.txt; R=$?
printf '  手一抖写成 key=oldd ｜ rc=%s ｜ 文件和改前逐字节一样吗 → %s ｜ 也没留备份 ｜ 这就是静默失败的全套形状\n' "$R" "$(cmp -s sipdemo/g.txt sipdemo/cfg.txt && echo 一样 || echo 不一样)"
sed -i 's/key=old/key=new/' sipdemo/nosuch.txt >/dev/null 2>&1; R=$?
printf '  反过来，真事故长这样：文件根本不存在 ｜ rc=%s ← 只有「读不到 / 写不成」才会给你非 0\n' "$R"
if [ "$(grep -c 'key=oldd' sipdemo/g.txt)" -gt 0 ]; then sed -i 's/key=oldd/key=new/' sipdemo/g.txt; else echo '  守卫响了：这个模式在文件里 0 处 → 拒绝动手（这一响是故意先弄给你看的）'; fi
N=$(grep -c 'key=old' sipdemo/g.txt)
printf '  换成对的模式先数一遍：grep -c 命中 %s 处 ｜ 另一种数法 sed -n "/^key=old$/p" | wc -l = %s\n' "$N" "$(sed -n '/^key=old$/p' sipdemo/g.txt | wc -l)"
sed -i 's/^key=old$/key=new/' sipdemo/g.txt; L=$(grep -c 'key=old' sipdemo/g.txt); R2=$?
printf '  动手后再数一遍残留：key=old 还剩 %s 处 ｜ grep rc=%s（没命中就是 1，不是事故）｜ 现在文件是：\n' "$L" "$R2"
sed 's/^/      | /' sipdemo/g.txt
echo '  → sed 从不报「我什么都没干」：要么改之前数一遍再决定动手，要么改完再数一遍对账；别拿 -i 的 rc 当判据。'

echo
echo '=== 6. 备份名的坑：-i.bak 第二遍把 .bak 覆盖成上一版；后缀里的 * 另有身份 ==='
cp sipdemo/cfg.txt sipdemo/k.txt; sed -i.bak 's/key=old/key=first/' sipdemo/k.txt
printf '  第一遍：主文件 %s ｜ k.txt.bak %s\n' "$(head -1 sipdemo/k.txt)" "$(head -1 sipdemo/k.txt.bak)"
sed -i.bak 's/key=first/key=second/' sipdemo/k.txt
printf '  第二遍：主文件 %s ｜ k.txt.bak %s ｜ .bak 里还是最初那份吗 → %s（已被覆盖成上一版）\n' "$(head -1 sipdemo/k.txt)" "$(head -1 sipdemo/k.txt.bak)" "$(cmp -s sipdemo/k.txt.bak sipdemo/cfg.txt && echo 是 || echo 不是)"
rm -f sipdemo/k.txt sipdemo/k.txt.bak; cp sipdemo/cfg.txt sipdemo/k.txt
for i in 1 2; do [ -f sipdemo/k.txt.bak ] || cp sipdemo/k.txt sipdemo/k.txt.bak; sed -i "s/key=old/key=pass$i/" sipdemo/k.txt; done
printf '  先立备份再裸 -i：跑两遍后 .bak = %s ｜ 还是最初那份吗 → %s\n' "$(head -1 sipdemo/k.txt.bak)" "$(cmp -s sipdemo/k.txt.bak sipdemo/cfg.txt && echo 是 || echo 不是)"
cp sipdemo/cfg.txt sipdemo/m.txt; cp sipdemo/cfg.txt sipdemo/n.txt; SFX='bak-*.old'
( cd sipdemo && sed -i"$SFX" 's/key=old/key=new/' m.txt )
printf '  -i'"'"'bak-*.old'"'"' 配裸文件名 m.txt：现场多出 %s ｜ 名字里有星号吗 %s（* 是「原文件名的位置」，不是通配符）\n' "$(ls -A sipdemo | grep '^bak-' | head -1)" "$(ls -A sipdemo | grep -q 'bak-\*' && echo 有 || echo 没有)"
E=$(sed -i"$SFX" 's/key=old/key=new/' sipdemo/n.txt 2>&1); R=$?
printf '  同一条命令、文件名换成带路径的 sipdemo/n.txt：rc=%s ｜ 原文[%s]\n' "$R" "$E"
printf '      文件动了吗 %s ｜ 目录里跟 n.txt 有关的东西 %s 个（没有任何备份落地）← * 塞进去的是**你给的那一长串**\n' "$(cmp -s sipdemo/n.txt sipdemo/cfg.txt && echo 没动 || echo 动了)" "$(ls -A sipdemo | grep -c '^n\.txt')"
mkdir -p sipdemo/glob; cp sipdemo/cfg.txt sipdemo/glob/cfg.txt
E=$( cd sipdemo/glob && sed -i *.orig 's/key=old/key=X/' cfg.txt 2>&1 ); R=$?
printf '  不加引号的 sed -i *.orig ｜ rc=%s ｜ 原文[%s] ｜ 文件动了吗 %s（bash 展不开它，就原样交给 sed 当脚本）\n' "$R" "$E" "$(cmp -s sipdemo/glob/cfg.txt sipdemo/cfg.txt && echo 没动 || echo 动了)"
printf '  → 两条直觉一起错：①「.bak 里躺着最初那份」——裸 -i 每跑一遍就把**上一版**当备份，两遍之后最初那份\n'
printf '    已经没了；想保住它就 [ -f 备份 ] || cp 先立备份再动手（上面第三行）。②「后缀里的 * 是通配符」——\n'
printf '    GNU sed 把 * 当**原文件名的位置**，所以 m.txt 的备份叫 bak-m.txt.old；不加引号时 bash 展不开 *，\n'
printf '    整串原样塞给 sed 当脚本，于是报 unknown command。备份名要带路径时更别指望 *。\n'

echo
echo '=== 7. ★ 经典自毁：sed "s/a/A/" f > f 把文件清空，而且退 0 ==='
mkdir -p sipdemo/sd; cp sipdemo/three.txt sipdemo/sd/f.txt
sed 's/a/A/' sipdemo/sd/f.txt > sipdemo/sd/f.txt; R=$?
printf '  跑完：rc=%s ｜ 剩几行 → %s ｜ 剩几字节 → %s ｜ 屏幕上有没有一句报错 → %s\n' "$R" "$(wc -l < sipdemo/sd/f.txt)" "$(wc -c < sipdemo/sd/f.txt)" "$([ -s sipdemo/sd/f.txt ] && echo 有 || echo 没有，文件已经空了)"
echo '  → 直觉「sed 先读完再写」：顺序反了。bash 在启动 sed **之前**就把输出文件打开并截成 0 字节，sed 读到的是'
echo '    一个空文件，于是它「成功」地输出了空。退 0、零报错、数据没了 —— 三件事同时发生。'
cp sipdemo/three.txt sipdemo/sd/s1.txt; sed -i 's/a/A/' sipdemo/sd/s1.txt
cp sipdemo/three.txt sipdemo/sd/s2.txt; sed 's/a/A/' sipdemo/sd/s2.txt > sipdemo/sd/s2.new && mv -f sipdemo/sd/s2.new sipdemo/sd/s2.txt
cp sipdemo/three.txt sipdemo/sd/s3.txt; V=$(sed 's/a/A/' sipdemo/sd/s3.txt); printf '%s\n' "$V" > sipdemo/sd/s3.txt
for f in s1 s2 s3; do printf '      安全写法 %s ｜ 还剩 %s 行 ｜ 第一行 → %s\n' "$f" "$(wc -l < sipdemo/sd/$f.txt)" "$(head -1 sipdemo/sd/$f.txt)"; done
echo '  → ①就是 -i（它写的是另一个临时文件，不会自己截自己）；②先写 f.new 再 mv（同一个目录里改名是一步到位的）；'
echo '    ③本机没有 sponge（command -v sponge 是空的），所以手搓两步：先把结果吃进变量再回写 —— $() 会吃掉末尾'
echo '    换行，得靠 printf 补回来；要严格的字节等价还是用 ②。'

echo
echo '=== 8. 收口：什么时候用 -i、备份三件套、本机码表、自证 ==='
echo '  什么时候用 -i：改**自己刚造出来的**文件（脚本生成的临时配置）；改别人正在用的、有版本库管的东西，就先'
echo '  sed 到 f.new、diff 看一眼、再 mv —— 因为 -i 换掉的是 inode，不是「内容」，这一步反悔的成本最低。'
echo '  备份三件套：① 动手前 grep -c 数一遍命中（第 5 格那道守卫）② -i.bak，或 [ -f 备份 ] || cp 保住最初那版'
echo '  ③ 改完 cmp 或再数一遍对账。三件里最便宜的是①，最能救命的是③。'
printf '  本机 sed 退出码实测：0 = 改成功，也 = 一个字都没改（第 4、5、7 格）｜ 1 = 脚本/命令行没编译过（第 1、6 格）｜ 2 = 文件读不到（第 5 格）｜ 4 = -i 建不出临时文件、改名失败（第 3、6 格）\n'
echo '  本章没演：别的进程正打开着这个文件时它看不到改动、属主不是自己的文件、跨文件系统 mv、NFS、任何耗时表现。'
printf '  自证：沙箱里此刻 %s 个条目（样本、两代备份、子目录 %s 个），权限已复原 → %s\n' "$(ls -A sipdemo | wc -l)" "$(find sipdemo -maxdepth 1 -type d | wc -l)" "$(chmod -R u+rwX sipdemo 2>/dev/null; echo 是)"
rm -rf sipdemo
printf '  删之后 → sipdemo 在吗：%s ｜ 本目录里叫 sipdemo 的残留条目 = %s 个（必须是 0）\n' "$([ -d sipdemo ] && echo 在 || echo 不在)" "$(find . -maxdepth 1 -name sipdemo | wc -l)"
exit 0
