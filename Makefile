# Compiler and linker
CXX := g++
CXXFLAGS := -std=c++11 -Wall -Wextra -pedantic
LDFLAGS := -static -static-libgcc -static-libstdc++
LDLIBS := -lstdc++fs

# Directories
SRC_DIR := Src
INC_DIR := Inc

# Target executable
APP := PRG-TOOLBOX-DFU

# Source files and object files
SOURCES := $(SRC_DIR)/DisplayManager.cpp $(SRC_DIR)/FileManager.cpp $(SRC_DIR)/ProgramManager.cpp $(SRC_DIR)/DFU.cpp $(SRC_DIR)/main.cpp
OBJECTS := $(SOURCES:.cpp=.o)

# Default target
all: $(APP)

# Linking the executable
$(APP): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) $(LDLIBS) -o $@

# Compiling source files with pattern rule
$(SRC_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) -c $< -o $@

# Clean target
clean:
ifeq ($(OS),Windows_NT)
	del /Q /F $(subst /,\,$(SRC_DIR)\*.o) $(APP).exe $(APP)
else
	rm -f $(SRC_DIR)/*.o $(APP).exe $(APP)
endif

.PHONY: all clean