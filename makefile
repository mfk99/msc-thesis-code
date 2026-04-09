TARGET = output
OBJ_DIR = build

SRCS := $(wildcard src/*.cpp) \
		$(wildcard src/*/*.cpp) \
		$(wildcard src/*/*/*.cpp) \
 		libs/pugixml/src/pugixml.cpp
OBJS = $(SRCS:%.cpp=$(OBJ_DIR)/%.o)

CXX = g++
CXXFLAGS ?= -Wall -DNDEBUG -O3 -std=c++17
CXXFLAGS += -I/usr/include
CXXFLAGS += -Ilibs/cxxopts/include

IPAMIRSOLVER ?= solver2022#EvalMaxSAT2022 | iMaxHS | solver2022 | uwrmaxsat14 | uwrmaxsat14scip
IPASIRSOLVER ?= minisat220
RUSTSAT = ./libs/rustsat/target/release/librustsat_capi.a

DEPS = ./libs/ipamir/maxsat/$(IPAMIRSOLVER)/libipamir$(IPAMIRSOLVER).a
LIBS = -L./libs/ipamir/maxsat/$(IPAMIRSOLVER)/ -lipamir$(IPAMIRSOLVER)
LIBS += $(shell cat ./libs/ipamir/maxsat/$(IPAMIRSOLVER)/LIBS 2>/dev/null)
LIBS += $(shell cat ./libs/ipamir/maxsat/$(IPASIRSOLVER)/LIBS 2>/dev/null)
LIBS += $(RUSTSAT)
LIBS += -ldl -lpthread -lm
LIBS += -ljsoncpp

all: $(TARGET)

# Ensure build/ exists
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Pattern rule for building any .o in OBJ_DIR from a corresponding .cpp
$(OBJ_DIR)/%.o: %.cpp | $(OBJ_DIR)
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@ 

# --- Linking rule ---
$(TARGET): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LIBS)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)
