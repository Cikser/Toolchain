CXX      = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -g
LEX      = flex
BISON    = bison

SRC_DIR   = src
INC_DIR   = inc
MISC_DIR  = misc
BUILD_DIR = build

LEXER_C  = $(BUILD_DIR)/lexer.cpp
PARSER_C = $(BUILD_DIR)/parser.cpp
PARSER_H = $(BUILD_DIR)/parser.tab.h

AS_SRCS = $(wildcard $(SRC_DIR)/as*.cpp)
LD_SRCS = $(wildcard $(SRC_DIR)/ld*.cpp)
EMU_SRCS = $(wildcard $(SRC_DIR)/emu*.cpp)

AS_OBJS = $(patsubst $(SRC_DIR)/as%.cpp, $(BUILD_DIR)/as%.o, $(AS_SRCS)) \
       $(BUILD_DIR)/lexer.o \
       $(BUILD_DIR)/parser.o

LD_OBJS = $(patsubst $(SRC_DIR)/ld%.cpp, $(BUILD_DIR)/ld%.o, $(LD_SRCS))

EMU_OBJS = $(patsubst $(SRC_DIR)/emu%.cpp, $(BUILD_DIR)/emu%.o, $(EMU_SRCS))

ASSEMBLER = assembler
LINKER = linker
EMULATOR = emulator

.PHONY: all clean

all: $(BUILD_DIR) $(ASSEMBLER) $(LINKER) $(EMULATOR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(PARSER_C) $(PARSER_H): $(MISC_DIR)/parser.y | $(BUILD_DIR)
	$(BISON) -d -o $(PARSER_C) --defines=$(PARSER_H) $(MISC_DIR)/parser.y

$(LEXER_C): $(MISC_DIR)/lexer.l $(PARSER_H) | $(BUILD_DIR)
	$(LEX) -o $(LEXER_C) $(MISC_DIR)/lexer.l

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) -I$(BUILD_DIR) -c $< -o $@

$(BUILD_DIR)/%.o: $(BUILD_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) -I$(BUILD_DIR) -c $< -o $@

$(ASSEMBLER): $(AS_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(LINKER): $(LD_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(EMULATOR): $(EMU_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -rf $(BUILD_DIR) $(ASSEMBLER) $(LINKER) $(EMULATOR)