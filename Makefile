CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall

TARGET = dejatop
SRC = $(wildcard src/*.cpp)
OBJ = $(patsubst src/%.cpp,obj/%.o,$(SRC))

PREFIX = $(HOME)/.local/bin
APPS_DIR = $(HOME)/.local/share/applications
MAN_DIR = $(HOME)/.local/share/man/man1
COMP_DIR = $(HOME)/.local/share/bash-completion/completions

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

obj/%.o: src/%.cpp
	@mkdir -p obj
	$(CXX) $(CXXFLAGS) -c $< -o $@

install: $(TARGET)
	mkdir -p $(PREFIX)
	mkdir -p $(APPS_DIR)
	mkdir -p $(HOME)/.local/share/DejaTop
	mkdir -p $(MAN_DIR)
	mkdir -p $(COMP_DIR)
	cp $(TARGET) $(PREFIX)/$(TARGET)
	cp $(TARGET).1 $(MAN_DIR)/$(TARGET).1
	cp completions/dejatop.bash $(COMP_DIR)/dejatop
	@echo "Installation complete!"

clean:
	rm -rf obj/
	rm -f $(TARGET)

uninstall:
	rm -f $(PREFIX)/$(TARGET)
	rm -f $(MAN_DIR)/$(TARGET).1
	rm -f $(COMP_DIR)/dejatop
	rm -f $(APPS_DIR)/dejatop.desktop

.PHONY: all install clean uninstall
