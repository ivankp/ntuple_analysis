.PHONY: all clean

ifeq (0, $(words $(findstring $(MAKECMDGOALS), clean))) #############

CPPFLAGS := -std=c++20 -Iinclude
CXXFLAGS := -Wall -O3 -flto -fmax-errors=3 -fconcepts-diagnostics-depth=3
# CXXFLAGS := -Wall -g -fmax-errors=3

# generate .d files during compilation
DEPFLAGS = -MT $@ -MMD -MP -MF .build/$*.d

ROOT_CPPFLAGS := $(shell root-config --cflags \
  | sed 's/-std=[^ ]*//g;s/-I/-isystem /g')
ROOT_LIBDIR   := $(shell root-config --libdir)
ROOT_LDFLAGS  := $(shell root-config --ldflags) -Wl,-rpath,$(ROOT_LIBDIR)
ROOT_LDLIBS   := $(shell root-config --libs)

FJ_PREFIX   := $(shell fastjet-config --prefix)
FJ_CPPFLAGS := -I$(FJ_PREFIX)/include
FJ_LDLIBS   := -L$(FJ_PREFIX)/lib -Wl,-rpath=$(FJ_PREFIX)/lib -lfastjet

LHAPDF_PREFIX   := $(shell lhapdf-config --prefix)
LHAPDF_CPPFLAGS := $(shell lhapdf-config --cppflags)
LHAPDF_LDLIBS   := $(shell lhapdf-config --libs) -Wl,-rpath=$(LHAPDF_PREFIX)/lib

EXE := bin/hist bin/merge

all: $(EXE)

C_hist := $(ROOT_CPPFLAGS) $(FJ_CPPFLAGS) $(LHAPDF_CPPFLAGS)
LF_hist := $(ROOT_LDFLAGS)
L_hist := $(ROOT_LDLIBS) $(FJ_LDLIBS) $(LHAPDF_LDLIBS)
bin/hist: .build/reweighter.o .build/Higgs2diphoton.o

C_merge := $(ROOT_CPPFLAGS)
LF_merge := $(ROOT_LDFLAGS)
L_merge := $(ROOT_LDLIBS)

C_reweighter := $(ROOT_CPPFLAGS)

#####################################################################

.PRECIOUS: .build/%.o

bin/%: .build/%.o
	@mkdir -pv $(dir $@)
	$(CXX) $(LDFLAGS) $(LF_$*) $(filter %.o,$^) -o $@ $(LDLIBS) $(L_$*)

.build/%.o: src/%.cc
	@mkdir -pv $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEPFLAGS) $(C_$*) -c $(filter %.cc,$^) -o $@

-include $(shell [ -d .build ] && find .build -type f -name '*.d')

endif ###############################################################

clean:
	@rm -rfv bin .build

