CC = g++
CXXFLAGS = -Wall -std=c++20 -DUNICODE -D_UNICODE -lShcore $(shell pkg-config --cflags opencv4) -I/mingw64/include/tesseract -I/mingw64/include/leptonica
LDFLAGS = $(shell pkg-config --libs opencv4) -L/mingw64/lib -lgdi32 -lmsimg32 -lleptonica -ltesseract -mconsole

SRCS = $(wildcard *.cpp) $(wildcard libs/*.cpp)
OBJS = $(addprefix $(BUILD_DIR)/, $(SRCS:.cpp=.o))
TARGET = Main_Program
BUILD_DIR = build

all: $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR)/$(TARGET): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

run:
	$(BUILD_DIR)/$(TARGET)

setup:
	pacman -Syu --noconfirm
	pacman -S --noconfirm mingw-w64-x86_64-opencv mingw-w64-x86_64-tesseract mingw-w64-x86_64-leptonica mingw-w64-x86_64-pkg-config mingw-w64-x86_64-wget
	echo "export TESSDATA_PREFIX=/mingw64/share/tessdata" >> ~/.bashrc
	source ~/.bashrc
	echo "MSYS2 setup complete."