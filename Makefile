# Package metadata.
TITLE       := AvPlayer Example Homebrew
VERSION     := 0.01
TITLE_ID    := KALA00009
CONTENT_ID  := IV0000-KALA00009_00-AVPLAYER00000000

# Libraries linked into the ELF.
LIBS        := -lc -lkernel -lc++ -lSceSystemService -lSceAvPlayer \
			   	-lSceVideoOut -lSceSysmodule -lScePad -lSceUserService -lSceAudioOut

# Additional compile flags.
#EXTRAFLAGS  := 

# Asset and module directories.
ASSETS 		:= $(wildcard assets/**/*)
LIBMODULES  := $(wildcard sce_module/*)

# Root vars
TOOLCHAIN   := $(OO_PS4_TOOLCHAIN)
PROJDIR     := src
COMMONDIR   := $(TOOLCHAIN)/samples/_common
INTDIR      := $(PROJDIR)/x64/Debug

# Define objects to build
CFILES      := $(shell find src -name "*.c")
CPPFILES    := $(shell find src -name "*.cpp")
OBJS        := $(patsubst %.cpp,$(INTDIR)/%.o, $(CPPFILES)) $(patsubst %.c,$(INTDIR)/%.o,$(CFILES))
# Define final C/C++ flags
CFLAGS      := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -O3 -march=btver2 -mtune=generic -mno-sse4a \
				-c $(EXTRAFLAGS) -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include -Isrc 
CXXFLAGS    := $(CFLAGS) -isystem $(TOOLCHAIN)/include/c++/v1 -fexceptions -fcxx-exceptions
LDFLAGS     := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crt1.o

# Create the intermediate directory incase it doesn't already exist.
_unused     := $(shell mkdir -p $(INTDIR))

# Check for linux vs macOS and account for clang/ld path
UNAME_S     := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
		CC      := clang
		CCX     := clang++
		LD      := ld.lld
		CDIR    := linux
endif
ifeq ($(UNAME_S),Darwin)
		CC      := /usr/local/opt/llvm/bin/clang
		CCX     := /usr/local/opt/llvm/bin/clang++
		LD      := /usr/local/opt/llvm/bin/ld.lld
		CDIR    := macos
endif

all: $(CONTENT_ID).pkg

$(CONTENT_ID).pkg: pkg.gp4
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core pkg_build $< .

sce_sys/param.sfo: Makefile
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_new $@
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1 
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0  
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gd'  
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0 
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0x05050000
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'

pkg.gp4: eboot.bin sce_sys/about/right.sprx sce_sys/param.sfo sce_sys/icon0.png $(LIBMODULES) $(ASSETS) video_short.mp4
	$(TOOLCHAIN)/bin/$(CDIR)/create-gp4 -out $@ --content-id=$(CONTENT_ID) --files "$^"

eboot.bin: $(INTDIR) $(OBJS)
	$(LD) $(OBJS) -o $(INTDIR)/$(PROJDIR).elf $(LDFLAGS)
	$(TOOLCHAIN)/bin/$(CDIR)/create-fself -in=$(INTDIR)/$(PROJDIR).elf -out=$(INTDIR)/$(PROJDIR).oelf --eboot "eboot.bin" --paid 0x3800000000000011

$(INTDIR)/%.o: %.cpp
	mkdir -p $(dir $@)
	clang++ $(CXXFLAGS) -c $< -o $@

$(INTDIR)/%.o: %.c
	mkdir -p $(dir $@)
	clang $(CFLAGS) -c $< -o $@

clean:
	rm -f $(CONTENT_ID).pkg pkg.gp4 pkg/sce_sys/param.sfo eboot.bin \
		$(INTDIR)/$(PROJDIR).elf $(INTDIR)/$(PROJDIR).oelf $(OBJS)
	rm -rf $(INTDIR)
