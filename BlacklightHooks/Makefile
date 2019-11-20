CC=g++
INCLUDES = -I../../../include -Iinclude
CXXFLAGS=-std=c++17 $(INCLUDES) $(DEFINES)
CXXOBJECTS = src/Memory/Allocators.o src/Memory/MemoryProtectionGuard.o src/Hooks/Detour.cpp src/Hooks/VirtualModification.o src/Hooks/VirtualReplacement.o
OUTPUT = ../../../build/BlacklightHooks
LDLIBS = -lZydis

#lib: $(CXXOBJECTS)
#	$(CC) -o $(OUTPUT).a -shared $(INCLUDES) $(CXXOBJECTS) $(LDLIBS)

test: $(CXXOBJECTS) Driver.o
	$(CC) -o $(OUTPUT)Test $(INCLUDES) $(CXXOBJECTS) Driver.o $(LDLIBS)

clean:
	rm -f *.o src/Memory/*.o src/Hooks/*.o ../../../build/BlacklightHooksTest ../../../build/BlacklightHooks.a
