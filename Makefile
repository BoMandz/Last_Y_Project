CC = g++
CXXFLAGS = -Wall -std=c++20 -DUNICODE -D_UNICODE -lShcore $(shell pkg-config --cflags opencv4 tesseract lept) -I/mingw64/include/tesseract -I/mingw64/include/leptonica
LDFLAGS = $(shell pkg-config --libs opencv4 tesseract lept) -L/mingw64/lib -lgdi32 -lmsimg32 -mconsole

SRCS = $(wildcard *.cpp)
OBJS = $(addprefix $(BUILD_DIR)/, $(SRCS:.cpp=.o))
TARGET = Main_Program
BUILD_DIR = build
USER = Bo

BUILD_TYPE ?= release

ifeq ($(BUILD_TYPE), debug)
    CXXFLAGS += -g -O0
else
    CXXFLAGS += -O3
endif

all: $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR)/$(TARGET): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CXXFLAGS) -c $< -o $@

# Add dependencies for object files
DEPS = $(OBJS:.o=.d)
-include $(DEPS)

$(BUILD_DIR)/%.d: %.cpp
	@mkdir -p $(dir $@)
	@$(CC) $(CXXFLAGS) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,$(BUILD_DIR)/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

clean:
	rm -rf $(BUILD_DIR)

run:
	@echo "Running $(TARGET) as administrator..."
	@runas /user:$(USER) "$(BUILD_DIR)/$(TARGET)"

setup:
	pacman -Syu --noconfirm
	pacman -S --noconfirm mingw-w64-x86_64-opencv mingw-w64-x86_64-tesseract mingw-w64-x86_64-leptonica mingw-w64-x86_64-pkg-config mingw-w64-x86_64-wget
	echo "export TESSDATA_PREFIX=/mingw64/share/tessdata" >> ~/.bashrc
	source ~/.bashrc
	echo "MSYS2 setup complete."