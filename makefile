TARGET = output
OBJ_DIR = build

SRCS = \
 src/index.cpp \
 src/semantics/scheduling_semantics.cpp \
 src/semantics/distribution_encodings.cpp \
 src/input_parser/input_parser.cpp \
 src/scheduling_encoding_generator/scheduling_encoding_generator.cpp \
 src/config/config.cpp \
 src/am1/am1_encoder.cpp \
 src/logging/logging.cpp \
 libs/pugixml/src/pugixml.cpp
OBJS = $(SRCS:%.cpp=$(OBJ_DIR)/%.o)

CXX = g++
CXXFLAGS ?= -Wall -DNDEBUG -O3 -std=c++17
CXXFLAGS += -I/usr/include

IPAMIRSOLVER ?= solver2022
IPASIRSOLVER ?= minisat220
RUSTSAT = ./../../rustsat/target/release/librustsat_capi.a

DEPS = ../../maxsat/$(IPAMIRSOLVER)/libipamir$(IPAMIRSOLVER).a
LIBS = -L../../maxsat/$(IPAMIRSOLVER)/ -lipamir$(IPAMIRSOLVER)
LIBS += $(shell cat ../../maxsat/$(IPAMIRSOLVER)/LIBS 2>/dev/null)
LIBS += $(shell cat ../../maxsat/$(IPASIRSOLVER)/LIBS 2>/dev/null)
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
