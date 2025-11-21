###
###  Makefile mediathek-api
###

UID := $(shell id -u)
ifeq ($(UID), 0)
all: warn
clean: warn
install: warn
warn:
	@echo ""
	@echo "You are running as root. Don't do this, it's dangerous."
	@echo "Refusing to build. Good bye."
else

PROG_SOURCES = \
	src/mt-api.cpp \
	src/common/helpers.cpp \
	src/html.cpp \
	src/json.cpp \
	src/net.cpp \
	src/sql.cpp

CSS_SOURCES = \
	src/css/index.scss \
	src/css/error.scss \
	src/css/main.scss

PROGNAME	 = mt-api
BUILD_DIR	 = build
TMP_OBJS	 = ${PROG_SOURCES:.cpp=.o}
TMP_DEPS	 = ${PROG_SOURCES:.cpp=.d}
PROG_OBJS	 = $(addprefix $(BUILD_DIR)/,$(TMP_OBJS))
PROG_DEPS	 = $(addprefix $(BUILD_DIR)/,$(TMP_DEPS))

TMP_CSS		 = ${CSS_SOURCES:.scss=.css}
PROG_CSS	 = $(addprefix $(BUILD_DIR)/,$(TMP_CSS))

## (optional) private definitions for DEBUG, EXTRA_CXXFLAGS etc.
## --------------------------------

-include config.mk

DEBUG			?= 0
ENABLE_SANITIZER	?= 0
QUIET			?= 1
DESTDIR			?= 
EXTRA_CXXFLAGS		?= 
EXTRA_LDFLAGS		?= 
EXTRA_INCLUDES		?= 
EXTRA_LIBS		?= 

MT_API_VERSION ?= $(shell git describe --tags --always --dirty 2>/dev/null)
MT_API_VERSION := $(strip $(MT_API_VERSION))
MT_API_VERSION := $(patsubst v%,%,$(MT_API_VERSION))
ifeq ($(MT_API_VERSION),)
MT_API_VERSION := $(strip $(shell sed -n 's/^VERSION="\([^"]*\)"/\1/p' VERSION 2>/dev/null))
endif
ifeq ($(MT_API_VERSION),)
MT_API_VERSION := 0.0.0
endif
EXTRA_CXXFLAGS		+= -DPROGVERSION=\"$(MT_API_VERSION)\"

CXX			?= g++
LD			?= g++
STRIP			?= strip
SASS			?= sass
STDC++			?= -std=c++11

## --------------------------------

ifeq ($(USE_COMPILER), CLANG)
	COMPX		= CLANG++
	LNKX		= CLANGLD
else
	COMPX		= CXX
	LNKX		= CXXLD
endif

ifneq ($(DEBUG), 1)
ENABLE_SANITIZER	 = 0
endif

INCLUDES	 =
INCLUDES	+= -I./src
INCLUDES	+= -I/usr/include/mariadb
INCLUDES	+= -I/usr/include/tidy
INCLUDES	+= -I/usr/include/boost
INCLUDES	+= $(EXTRA_INCLUDES)

CXXFLAGS	 = $(INCLUDES) -pipe -fno-strict-aliasing
ifeq ($(DEBUG), 1)
CXXFLAGS	+= -O0 -g -ggdb3
else
CXXFLAGS	+= -O3
endif
CXXFLAGS	+= $(STDC++)
ifneq ($(USE_COMPILER), CLANG)
CXXFLAGS	+= -fmax-errors=10
endif
CXXFLAGS	+= -Wall
CXXFLAGS	+= -Wextra
CXXFLAGS	+= -Wshadow
CXXFLAGS	+= -Warray-bounds
CXXFLAGS	+= -Werror
CXXFLAGS	+= -Werror=format-security
CXXFLAGS	+= -Werror=array-bounds
CXXFLAGS	+= -fexceptions
CXXFLAGS	+= -Wformat
CXXFLAGS	+= -Wformat-security
CXXFLAGS	+= -Wuninitialized
CXXFLAGS	+= -funsigned-char
CXXFLAGS	+= -Wstrict-overflow
CXXFLAGS	+= -Woverloaded-virtual
CXXFLAGS	+= -Wunused
CXXFLAGS	+= -Wunused-value
CXXFLAGS	+= -Wunused-variable
CXXFLAGS	+= -Wunused-function
CXXFLAGS	+= -fno-omit-frame-pointer
CXXFLAGS	+= -fstack-protector-all
CXXFLAGS	+= -fstack-protector-strong
CXXFLAGS	+= -Wno-long-long
CXXFLAGS	+= -Wno-narrowing
CXXFLAGS	+= -Winit-self
CXXFLAGS	+= -Wpedantic

ifeq ($(ENABLE_SANITIZER), 1)
CXXFLAGS	+= -fsanitize=address
CXXFLAGS	+= -fsanitize=leak
CXXFLAGS	+= -fsanitize=bool
CXXFLAGS	+= -fsanitize=bounds
CXXFLAGS	+= -fsanitize=enum
CXXFLAGS	+= -fsanitize=nonnull-attribute
CXXFLAGS	+= -fsanitize=return
CXXFLAGS	+= -fsanitize=returns-nonnull-attribute
CXXFLAGS	+= -fsanitize=shift
CXXFLAGS	+= -fsanitize=unreachable
CXXFLAGS	+= -fsanitize=vla-bound
CXXFLAGS	+= -fsanitize=vptr
CXXFLAGS	+= -fsanitize=float-cast-overflow
CXXFLAGS	+= -fsanitize=float-divide-by-zero
CXXFLAGS	+= -fsanitize=integer-divide-by-zero
CXXFLAGS	+= -fsanitize=signed-integer-overflow

ifeq ($(USE_COMPILER), CLANG)
CXXFLAGS	+= -fsanitize=function
CXXFLAGS	+= -fsanitize=nullability-arg
CXXFLAGS	+= -fsanitize=nullability-assign
CXXFLAGS	+= -fsanitize=nullability-return
CXXFLAGS	+= -fsanitize=unsigned-integer-overflow
## clang error: "undefined reference to __ubsan_handle_pointer_overflow"
##CXXFLAGS	+= -fsanitize=pointer-overflow
endif

ifneq ($(USE_COMPILER), CLANG)
## clang error: "undefined reference to __ubsan_handle_type_mismatch_v1"
CXXFLAGS	+= -fsanitize=alignment
## clang error: "undefined reference to __ubsan_handle_type_mismatch_v1"
CXXFLAGS	+= -fsanitize=null
endif

CXXFLAGS	+= -DSANITIZER
endif

ifeq ($(USE_COMPILER), CLANG)
CXXFLAGS	+= -DUSE_CLANG
endif

CXXFLAGS	+= -DSASS_VERSION=\"$$($(SASS) --version | cut -d' ' -f 2)\"
CXXFLAGS	+= $(EXTRA_CXXFLAGS)

LIBS		 =
ifeq ($(ENABLE_SANITIZER), 1)
LIBS		+= -lasan
LIBS		+= -lubsan
endif
LIBS		+= -ltidy
LIBS		+= -ljsoncpp
LIBS		+= -lmariadb
LIBS		+= -lboost_system
LIBS		+= -lpthread
LIBS		+= -ldl
LIBS		+= -lc
#LIBS		+= -lcppnetlib-client-connections
#LIBS		+= -lcppnetlib-server-parsers
LIBS		+= $(EXTRA_LIBS)

LDFLAGS		 = $(EXTRA_LDFLAGS)

all-build: $(BUILD_DIR)/$(PROGNAME) css
all-build-strip: all-build strip
ifeq ($(DEBUG), 1)
all: all-build
else
all: all-build-strip
endif

ifeq ($(QUIET), 1)
quiet = @
else
quiet =
endif

build/src/%.o: src/%.cpp
	@if ! test -d $$(dirname $@); then mkdir -p $$(dirname $@); fi;
	@if test "$(quiet)" = "@"; then echo "$(COMPX) $< => $@"; fi;
	$(quiet)$(CXX) $(CXXFLAGS) -MT $@ -MD -MP -c -o $@ $<

$(BUILD_DIR)/$(PROGNAME): $(PROG_OBJS)
	@if ! test -d $$(dirname $@); then mkdir -p $$(dirname $@); fi;
	@if test "$(quiet)" = "@"; then echo "$(LNKX) *.o => $@"; fi;
	$(quiet)$(CXX) $(PROG_OBJS) $(LDFLAGS) $(LIBS) -o $@

ifeq ($(DEBUG), 1)
CSS_STYLE = expanded
else
CSS_STYLE = compressed
endif

build/src/%.css: src/%.scss
	@if ! test -d $$(dirname $@); then mkdir -p $$(dirname $@); fi;
	@if test "$(quiet)" = "@"; then echo "SASS $< => $@"; fi;
	$(quiet)$(SASS) --sourcemap=none --default-encoding=utf-8 --style=$(CSS_STYLE) $< $@

css: $(PROG_CSS)

install: all
	@if test "$(DESTDIR)" = ""; then \
		echo -e "\nERROR: No DESTDIR specified.\n"; false;\
	fi
	@if test "$(quiet)" = "@"; then echo -e "\nINSTALL $(PROGNAME) => $(DESTDIR)\n"; fi;
	$(quiet)rm -fr $(DESTDIR)/www
	$(quiet)rm -fr $(DESTDIR)/data
	$(quiet)install -m 755 -d $(DESTDIR)/www
	$(quiet)install -m 755 -d $(DESTDIR)/data
	$(quiet)cp -frd src/web/www $(DESTDIR)
	$(quiet)cp -frd src/web/data $(DESTDIR)
	$(quiet)rm -f $(DESTDIR)/data/.passwd/.gitignore
	$(quiet)cp -f sqlpasswd  $(DESTDIR)/data/.passwd
	$(quiet)cp -frd build/src/css $(DESTDIR)/www
	$(quiet)install -m 755 -D $(BUILD_DIR)/$(PROGNAME) $(DESTDIR)/www/$(PROGNAME)

clean:
	rm -rf $(BUILD_DIR)

strip:
	@$(STRIP) $(BUILD_DIR)/$(PROGNAME)

-include $(PROG_DEPS)

endif # root test
