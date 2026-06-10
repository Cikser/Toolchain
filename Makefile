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

SRCS = $(wildcard $(SRC_DIR)/*.cpp)

OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRCS)) \
       $(BUILD_DIR)/lexer.o \
       $(BUILD_DIR)/parser.o

TARGET = asembler

.PHONY: all clean

all: $(BUILD_DIR) $(TARGET)

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

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -rf $(BUILD_DIR) asembler