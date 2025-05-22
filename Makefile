include padkit/compile.mk

INCLUDE_DIRS=-Iinclude -Ipadkit/include
OBJECTS=obj/gfuzzer.o

default: bin/gfuzzer

.FORCE:

.PHONY: .FORCE clean default

bin: ; mkdir bin

bin/gfuzzer: .FORCE                   	\
    bin                                 \
	padkit/lib/libpadkit.a              \
    ${OBJECTS}                          \
	; ${COMPILE} ${OBJECTS} padkit/lib/libpadkit.a -o bin/gfuzzer

clean: ; rm -rf obj bin *.gcno *.gcda *.gcov html latex

obj: ; mkdir obj

obj/gfuzzer.o: .FORCE                 	\
    obj                                 \
	padkit/include/padkit/verbose.h   	\
    src/gfuzzer.c                     	\
    ; ${COMPILE} ${INCLUDE_DIRS} src/gfuzzer.c -c -o obj/gfuzzer.o

padkit/lib/libpadkit.a: .FORCE          \
    ; make -C padkit clean lib/libpadkit.a
