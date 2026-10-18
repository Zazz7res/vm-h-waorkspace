CC      ?= gcc
CFLAGS  := -Wall -Wextra -Wshadow -std=c11 -g
BIN     := build

# 自动发现所有叶子目录：形如 01-array/dynamic-array/main.c
RAW     := $(shell find . -mindepth 3 -maxdepth 3 -name main.c -not -path './$(BIN)*' 2>/dev/null)
LEAVES  := $(sort $(patsubst %/,%,$(patsubst ./%,%,$(dir $(RAW)))))
TARGETS := $(addprefix $(BIN)/,$(LEAVES:%=%/main))

# 所有编号章节（00-... 到 11-...），status 目标用它检查"该有没有"
CHAPTERS := $(sort $(patsubst %/,%,$(dir $(wildcard [0-9]*/))))

.PHONY: all run run-tolerant one asan check dscheck status clean list

all: $(TARGETS)

# 一个叶子目录下的所有 .c 一起编译成一个可执行文件
$(BIN)/%/main:
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) $(wildcard $*/*.c) -o $@
	@printf "  编译 %-34s -> %s\n" "$*" "$@"

# 编译并按目录顺序全部运行，任何一步失败就停下
run: all
	@for d in $(LEAVES); do \
	  echo; echo "################  $$d  ################"; \
	  $(BIN)/$$d/main || { echo "!! 运行失败: $$d"; exit 1; }; \
	done; \
	echo; echo "===== 全部运行结束 ====="

# 不因失败中断，用于批量查看输出
run-tolerant: all
	@for d in $(LEAVES); do \
	  echo; echo "################  $$d  ################"; \
	  $(BIN)/$$d/main || echo "!! 运行失败: $$d"; \
	done

one:
	@test -n "$(DIR)" || { echo "用法: make one DIR=01-array/dynamic-array"; exit 1; }
	@mkdir -p $(BIN)/$(DIR)
	@$(CC) $(CFLAGS) $(wildcard $(DIR)/*.c) -o $(BIN)/$(DIR)/main
	@$(BIN)/$(DIR)/main

# 带 AddressSanitizer 重新编译并运行，专门抓内存错误（野指针、越界、泄漏、double free）
asan:
	@fail=0; \
	for d in $(LEAVES); do \
	  mkdir -p $(BIN)-asan/$$d; \
	  if ! $(CC) $(CFLAGS) -fsanitize=address -static-libasan $$d/*.c -o $(BIN)-asan/$$d/main 2>/tmp/asan_build.err; then \
	    echo "!! 编译失败 $$d"; cat /tmp/asan_build.err; fail=1; continue; \
	  fi; \
	  printf "  %-34s " "$$d"; \
	  if $(BIN)-asan/$$d/main >/dev/null 2>/tmp/asan_run.err; then echo "ok"; \
	  else echo "内存错误!"; head -25 /tmp/asan_run.err; fail=1; fi; \
	done; \
	if [ $$fail = 0 ]; then echo "== ASan 检查：全部通过 =="; else echo "== ASan 检查：存在问题 =="; fi

list:
	@for d in $(LEAVES); do echo $$d; done

# 一条命令看清"还差什么"：章节齐不齐 + 每节有没有 README + 有没有 main.c
status:
	@tot=0; ok=0; \
	echo "== 各章叶子目录体检（每个都要同时有 main.c 和 README.md）=="; \
	for ch in $(CHAPTERS); do \
	  for d in $$ch/*/; do \
	    d=$${d%/}; [ -d "$$d" ] || continue; \
	    tot=$$((tot+1)); \
	    m=-; r=-; [ -f "$$d/main.c" ] && m=Y; [ -f "$$d/README.md" ] && r=Y; \
	    if [ "$$m" = Y ] && [ "$$r" = Y ]; then ok=$$((ok+1)); st=齐; else st=缺; fi; \
	    printf "  %-2s %-36s main.c=%s README=%s\n" "$$st" "$$d" "$$m" "$$r"; \
	  done; \
	done; \
	echo; printf "完整章节：%d / %d\n" $$ok $$tot; \
	echo; echo "== 有 .c 但没有 main.c 的目录（写完了但没被 Makefile 发现）=="; \
	found=0; \
	for d in $(CHAPTERS); do \
	  for s in $$d/*/; do \
	    s=$${s%/}; [ -d "$$s" ] || continue; \
	    if ls $$s/*.c >/dev/null 2>&1 && [ ! -f "$$s/main.c" ]; then echo "  !! $$s"; found=1; fi; \
	  done; \
	done; \
	if [ $$found = 0 ]; then echo "  （无）"; fi; true

# 编译 + 运行 + ASan 三合一体检，任何一节不干净就退出非 0（可直接当验收闸门）
check:
	@fail=0; \
	for d in $(LEAVES); do \
	  mkdir -p $(BIN)-check/$$d; \
	  if ! $(CC) $(CFLAGS) -Werror -fsanitize=address -static-libasan $$d/*.c -o $(BIN)-check/$$d/main 2>/tmp/chk_c.err; then \
	    printf "  !! 编译有问题 %-30s\n" "$$d"; head -8 /tmp/chk_c.err | sed 's/^/       /'; fail=1; continue; fi; \
	  $(BIN)-check/$$d/main >/tmp/chk_o.txt 2>/tmp/chk_r.err; rc=$$?; \
	  if [ $$rc -ne 0 ] || [ -s /tmp/chk_r.err ]; then \
	    printf "  !! 运行有问题 %-28s (退出码 %d, stderr %d 字节)\n" "$$d" $$rc $$(wc -c </tmp/chk_r.err); \
	    head -8 /tmp/chk_r.err | sed 's/^/       /'; fail=1; continue; fi; \
	  printf "  ok %-38s %4d 行输出\n" "$$d" $$(wc -l </tmp/chk_o.txt); \
	done; \
	if [ $$fail = 0 ]; then echo "== check：全部编译零告警 + 运行 + ASan 通过 =="; \
	else echo "== check：存在问题，看上面的 !! 行 =="; exit 1; fi

# 引文保真：README「运行结果」段里每一行围栏，都要能用 build-check 里的二进制当场跑出来
# （先 make check 建好 build-check，再跑这条；工具不在就明说，不装成通过）
dscheck:
	@if [ ! -f "$(HOME)/.local/bin/ds_output_check.py" ]; then \
	  echo "!! 找不到 $(HOME)/.local/bin/ds_output_check.py —— 这一条要的是仓库作者的核对工具，"; \
	  echo "   没有它就不能假装核对过。人工做法：对每节 make one DIR=… 再逐行比对 README。"; exit 1; fi
	@python3 $(HOME)/.local/bin/ds_output_check.py --selftest || exit 1
	@python3 $(HOME)/.local/bin/ds_output_check.py || exit 1

clean:
	@rm -rf $(BIN) $(BIN)-asan $(BIN)-check
	@echo "已清理编译产物"
