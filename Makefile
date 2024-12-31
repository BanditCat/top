CC = clang
EMCC = emcc
CFLAGS = -Wall
CFLAGS_RELEASE = -O3
CFLAGS_DEBUG = -g -DDEBUG
CPPFLAGS = -MMD -MP -DSDL_MAIN_HANDLED
LDFLAGS = -ldwmapi -lopengl32 -lSDL2-static -luser32 -lgdi32 -lshell32 -lwinmm -lsetupapi -lole32 -ladvapi32 -limm32 -lversion -loleaut32 -Wl,-nodefaultlib:msvcrt -Wl,-subsystem:windows -Wl,-entry:mainCRTStartup
LDFLAGS_DEBUG= -g
EMCCFLAGS = --preload-file main.atl@main.atl --preload-file inc/font.bmp@inc/font.bmp --preload-file inc/mandelbrot.atl@inc/mandelbrot.atl --preload-file inc/fire.atl@inc/fire.atl -O3 -s WASM=1 -s USE_WEBGL2=1 -s USE_SDL=2 -s MAX_WEBGL_VERSION=3 -s MIN_WEBGL_VERSION=2 --shell-file shell.html
TARGET = Atlas.exe
HTML = demos.html
JS = $(HTML:.html=.js)
WASM = $(HTML:.html=.wasm)
DATA = $(HTML:.html=.data)

HDRS = Atlas.h tensor.h trie.h program.h
MSRCS = main.c tensor.c tensorPrint.c program.c trie.c
SRCS = main.c tensor.c glew.c tensorPrint.c program.c trie.c
OBJS = $(SRCS:.c=.o)

.PHONY: all rall clean backup release tidy

rdall: debug
	./$(TARGET)
rall: release 
	./$(TARGET)


debug: CFLAGS += $(CFLAGS_DEBUG)
debug: LDFLAGS += $(LDFLAGS_DEBUG)
debug: $(TARGET)

release: CFLAGS += $(CFLAGS_RELEASE)
relese: $(TARGET)



tidy:
	clang-tidy $(MSRCS) -- $(CFLAGS) $(CPPFLAGS)

icon.res: icon.rc
	llvm-rc icon.rc

release: $(TARGET)

$(HTML): $(SRCS) $(ATLHS) 
	$(EMCC) $(EMCCFLAGS) -o $(HTML) $(SRCS)

$(TARGET): $(OBJS) $(ATLHS) icon.res
	$(CC) $(LDFLAGS) $(OBJS) icon.res -o $@

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(OBJS:.o=.d) $(HTML) $(TARGET) $(JS) $(WASM) $(ATLHS) icon.res /bin/inc

backup:
	$(MAKE) release
	$(MAKE) $(HTML)
	cp -rf $(TARGET) $(HTML) $(JS) $(WASM) $(DATA) ./inc ./bin
	upx -9 ./bin/$(TARGET)
	$(MAKE) clean
	git add -A
	git commit -m 'Pinch to zoom'
	git push -u origin main

# Include dependency files
-include $(OBJS:.o=.d)


