# Makefile - ECMA-55 BASIC
# Interpretador didatico aderente a norma ECMA-55 (Minimal BASIC)

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2
TARGET   := ecma55basic

SRC := $(wildcard src/*.cpp)
OBJ := $(SRC:.cpp=.o)
DEP := $(OBJ:.o=.d)

TEST_TARGET := test_lexer
PARSER_TEST_TARGET := test_parser
INTERP_TEST_TARGET := test_interpreter
NBS_TEST_TARGET := test_nbs
# tudo em src/ exceto main.cpp entra nos binarios de teste, junto com o
# proprio arquivo de teste
LIB_SRC := $(filter-out src/main.cpp,$(SRC))
LIB_OBJ := $(LIB_SRC:.cpp=.o)

.PHONY: all clean run test

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

tests/%.o: tests/%.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(DEP)
-include tests/test_lexer.d
-include tests/test_parser.d
-include tests/test_interpreter.d
-include tests/test_nbs.d

$(TEST_TARGET): $(LIB_OBJ) tests/test_lexer.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(PARSER_TEST_TARGET): $(LIB_OBJ) tests/test_parser.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(INTERP_TEST_TARGET): $(LIB_OBJ) tests/test_interpreter.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(NBS_TEST_TARGET): $(LIB_OBJ) tests/test_nbs.o
	$(CXX) $(CXXFLAGS) -o $@ $^

test: $(TEST_TARGET) $(PARSER_TEST_TARGET) $(INTERP_TEST_TARGET)
	./$(TEST_TARGET)
	./$(PARSER_TEST_TARGET)
	./$(INTERP_TEST_TARGET)

nbs: $(NBS_TEST_TARGET)
	./$(NBS_TEST_TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f src/*.o src/*.d tests/*.o tests/*.d $(TARGET) $(TEST_TARGET) $(PARSER_TEST_TARGET) $(INTERP_TEST_TARGET) $(NBS_TEST_TARGET)
