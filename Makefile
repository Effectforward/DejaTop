CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra
DEPFLAGS = -MMD -MP

TARGET = dejatop
SRC = $(wildcard src/*.cpp)
OBJ = $(patsubst src/%.cpp,obj/%.o,$(SRC))
DEP = $(OBJ:.o=.d)

PREFIX = $(HOME)/.local/bin
APPS_DIR = $(HOME)/.local/share/applications
MAN_DIR = $(HOME)/.local/share/man/man1
COMP_DIR = $(HOME)/.local/share/bash-completion/completions

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

obj/%.o: src/%.cpp
	@mkdir -p obj
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

-include $(DEP)

test: $(TARGET)
	@fail=0; \
	for t in tests/t*.sh; do \
		echo "==== $$t"; \
		bash "$$t" || fail=1; \
	done; \
	exit $$fail

check: test

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

.PHONY: all install clean uninstall test check
