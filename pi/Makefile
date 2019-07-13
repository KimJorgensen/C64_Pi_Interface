CFLAGS += -O3 -Wall -Wextra -I. $(shell sdl2-config --cflags)
LIBS += $(shell sdl2-config --libs)
DEPS = 6502.cpp 6569.cpp 6569.h 6581.cpp 6581.h display.cpp display.h sound.cpp fake_pi.cpp pi.cpp pi.h
OBJ = main.o
RM := rm -f

%.o: %.cpp $(DEPS)
	$(CXX) -c -o $@ $< $(CFLAGS)

emulator: $(OBJ)
	$(CXX) -o $@ $^ $(CFLAGS) $(LIBS)

all: test

.PHONY: clean
clean:
	$(RM) $(OBJ)
	$(RM) emulator

