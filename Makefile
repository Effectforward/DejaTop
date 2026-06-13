CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall

TARGET = dejatop
SRC = dejatop.cpp

PREFIX = $(HOME)/.local/bin
APPS_DIR = $(HOME)/.local/share/applications
MAN_DIR = $(HOME)/.local/share/man/man1

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

install: $(TARGET)
	mkdir -p $(PREFIX)
	mkdir -p $(APPS_DIR)
	mkdir -p $(HOME)/.local/share/DejaTop
	mkdir -p $(MAN_DIR)
	cp $(TARGET) $(PREFIX)/$(TARGET)
	cp $(TARGET).1 $(MAN_DIR)/$(TARGET).1
	@echo "Installation complete!"

clean:
	rm -f $(TARGET)

uninstall:
	rm -f $(PREFIX)/$(TARGET)
	rm -f $(MAN_DIR)/$(TARGET).1
	rm -f $(APPS_DIR)/dejatop.desktop

.PHONY: all install clean uninstall
