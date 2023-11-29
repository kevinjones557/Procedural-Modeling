sources = \
	src/main.cpp \
	src/lsystem.cpp \
	src/util.cpp \
	src/gl_core_3_3.c
libs = \
	-lGL \
	-lglut
outname = base_freeglut

all:
	g++ -std=c++17 -O3 $(sources) $(libs) -o $(outname)
clean:
	rm $(outname)
