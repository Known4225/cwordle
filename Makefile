all:
	gcc cwordle.c -L./Linux -lturtle -lglfw3 -ldl -lm -lX11 -lglad -lGL -lGLU -lpthread -DOS_LINUX -DDEBUGGING_FLAG -Wall -o cwordle.o
rel:
	gcc cwordle.c -L./Linux -lturtle -lglfw3 -ldl -lm -lX11 -lglad -lGL -lGLU -lpthread -DOS_LINUX -O3 -o cwordle.o
lib:
	cp turtle.h turtlelib.c
	gcc turtlelib.c -c -DTURTLE_IMPLEMENTATION -DTURTLE_ENABLE_TEXTURES -DTURTLE_ENABLE_SERIAL -DTURTLE_ENABLE_SOCKETS -DTURTLE_ENABLE_CAMERA -DOS_LINUX -O3 -o Linux/libturtle.a
	rm turtlelib.c
win:
	gcc cwordle.c -L./Windows -lturtle -lglfw3 -lopengl32 -lgdi32 -lglad -lole32 -luuid -lwsock32 -lWs2_32 -lMf -lMfplat -lmfreadwrite -lmfuuid -DOS_WINDOWS -DDEBUGGING_FLAG -Wall -o cwordle.exe
winrel:
	gcc cwordle.c -L./Windows -lturtle -lglfw3 -lopengl32 -lgdi32 -lglad -lole32 -luuid -lwsock32 -lWs2_32 -lMf -lMfplat -lmfreadwrite -lmfuuid -DOS_WINDOWS -O3 -o cwordle.exe
winlib:
	cp turtle.h turtlelib.c
	gcc turtlelib.c -c -DTURTLE_IMPLEMENTATION -DTURTLE_ENABLE_TEXTURES -DTURTLE_ENABLE_SERIAL -DTURTLE_ENABLE_SOCKETS -DTURTLE_ENABLE_CAMERA -DOS_WINDOWS -O3 -o Windows/turtle.lib
	rm turtlelib.c
html:
	emcc cwordle.c --shell-file config/turtle_shell.html -sUSE_GLFW=3 -sMAX_WEBGL_VERSION=2 -sASYNCIFY -sINITIAL_MEMORY=1073741824 -sWASM=0 -DTURTLE_IMPLEMENTATION -DTURTLE_ENABLE_TEXTURES -DTURTLE_ENABLE_SERIAL -DTURTLE_ENABLE_SOCKETS -DTURTLE_ENABLE_CAMERA -DOS_BROWSER -Oz -o turtle.html --embed-file images
	gcc config/combine.c -o combine.exe
	./combine.exe cwordle.html
	rm combine.exe
htmlserver:
	emcc cwordle.c --shell-file config/turtle_shell.html -sUSE_GLFW=3 -sMAX_WEBGL_VERSION=2 -sASYNCIFY -sINITIAL_MEMORY=1073741824 -DTURTLE_IMPLEMENTATION -DTURTLE_ENABLE_TEXTURES -DTURTLE_ENABLE_SERIAL -DTURTLE_ENABLE_SOCKETS -DTURTLE_ENABLE_CAMERA -DOS_BROWSER -O3 -o turtle.html --embed-file images
runserver:
	emrun cwordle.html