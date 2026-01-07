TARGET = output
OBJ_DIR = build

SRCS = index.cpp semantics/scheduling-semantics.cpp input-parser/input-parser.cpp
OBJS = $(SRCS:%.cpp=$(OBJ_DIR)/%.o)

CXX = g++
CXXFLAGS ?= -Wall -DNDEBUG -O3 -std=c++17

IPAMIRSOLVER ?= solver2022
IPASIRSOLVER ?= minisat220
RUSTSAT = ./../../rustsat/target/release/librustsat_capi.a

DEPS = ../../maxsat/$(IPAMIRSOLVER)/libipamir$(IPAMIRSOLVER).a
LIBS = -L../../maxsat/$(IPAMIRSOLVER)/ -lipamir$(IPAMIRSOLVER)
LIBS += $(shell cat ../../maxsat/$(IPAMIRSOLVER)/LIBS 2>/dev/null)
LIBS += $(shell cat ../../maxsat/$(IPASIRSOLVER)/LIBS 2>/dev/null)
LIBS += $(RUSTSAT)
LIBS += -ldl -lpthread -lm

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
