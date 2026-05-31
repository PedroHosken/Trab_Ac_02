CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude
SRC = src/main.cpp src/parser.cpp src/display.cpp src/classic_tomasulo.cpp src/rob_tomasulo.cpp
OBJ = $(SRC:.cpp=.o)
TARGET = tomasulo

.PHONY: all clean run-classic run-rob

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET)

run-classic: $(TARGET)
	./$(TARGET) examples/classic_exemplo.txt

run-rob: $(TARGET)
	./$(TARGET) -m rob examples/rob_exemplo.txt
