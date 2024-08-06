COMPILER := clang++
COMPILER_FLAGS := -std=c++23 -fsanitize=address,undefined -g 
LD_FLAGS := 
OBJ_DIR := objs
BIN_DIR := bin

SRCS := $(wildcard *.cpp)
OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(wildcard *.cpp))

all: directories interpreter

directories:
	mkdir -p $(OBJ_DIR) $(BIN_DIR)

interpreter: $(OBJS)
	$(COMPILER) $(COMPILER_FLAGS) -o $(BIN_DIR)/$@ $^ $(LD_FLAGS)

$(OBJ_DIR)/%.o: %.cpp
	mkdir -p $(@D)
	$(COMPILER) $(COMPILER_FLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

run: all interpreter
	./$(BIN_DIR)/interpreter