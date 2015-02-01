OBJS = ../AudioChip.o main.o

CXX = g++
CXXFLAGS = -std=c++11 -Wall -Werror -ggdb -O0 -DDEBUG
TARGET = AudioChipTest
LDFLAGS = -lasound -lpthread -lm

%.o : %.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $(TARGET)

clean:
	rm $(OBJS) $(TARGET)

all:
	$(TARGET)

