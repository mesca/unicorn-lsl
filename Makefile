BUILD = build
TARGET = $(BUILD)/unicorn-lsl
OBJECT = $(BUILD)/main.o
SOURCE = src/main.cpp

all: $(TARGET)

$(TARGET): $(OBJECT)
	g++ -Llib -o $@ $< -lunicorn -llsl

$(OBJECT):
	@#g++ -Ilib -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"tmp/main.d" -MT"tmp/main.o" -o tmp/main.o src/main.cpp
	mkdir $(BUILD)
	g++ -Ilib -Wall -c -o $@ $(SOURCE)

clean:
	rm -f $(BUILD)/*
	rmdir $(BUILD)

install:
	cp lib/*.so /usr/local/lib/
	cp $(TARGET) /usr/local/bin/
	ldconfig /usr/local/lib

uninstall:
	rm /usr/local/bin/unicorn-lsl
	rm /usr/local/lib/libunicorn.so
