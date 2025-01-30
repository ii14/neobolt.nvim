CFLAGS += -Wall -Wextra -Wpedantic -Wconversion
CFLAGS += -g
ifeq ($(config),)
	CFLAGS += -O2 -march=native
endif
ifeq ($(config),debug)
	CFLAGS += -Og
endif
ifeq ($(config),sanitize)
	CFLAGS += -Og -fsanitize=address,undefined -fno-omit-frame-pointer
endif

INCLUDE = -Idep/lua/include

UNAME := $(shell uname)
ifeq ($(UNAME),Darwin)
	LDFLAGS := -undefined dynamic_lookup
endif

GPERF = gperf


all: lua

# lua module
lua: lua/libneobolt.so
lua/libneobolt.so: src/neobolt_lua.c src/neobolt.c
	$(CC) $(INCLUDE) $(CFLAGS) -o $@ $< -shared -fPIC -fvisibility=hidden $(LDFLAGS)

# standalone executable
exe: neobolt
neobolt: src/neobolt_exe.c src/neobolt.c
	$(CC) $(INCLUDE) $(CFLAGS) -o $@ $<

# fuzz test
fuzz: neobolt_fuzz
neobolt_fuzz: src/neobolt_fuzz.c src/neobolt.c
	$(CC) $(INCLUDE) -g -O1 -fsanitize=fuzzer,address,undefined -o $@ $<

# generate lookup tables
lut:
	$(GPERF) \
		--compare-lengths \
		--hash-function-name=is_data_directive_hash \
		--lookup-function-name=is_data_directive_lookup \
		--output=src/data_directives.h src/data_directives.txt


.PHONY: all lua exe fuzz lut
