include padkit/compile.mk

INCLUDE_DIRS=-Iinclude -Ipadkit/include
OBJECTS=obj/ast.o obj/gfuzzer.o

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

obj/ast.o: .FORCE                       \
    obj                                 \
    include/ast.h                       \
    include/bnf.h                       \
	padkit/include/padkit/arraylist.h   \
	padkit/include/padkit/chunk.h       \
	padkit/include/padkit/hash.h        \
	padkit/include/padkit/implication.h \
	padkit/include/padkit/indextable.h  \
	padkit/include/padkit/invalid.h     \
	padkit/include/padkit/item.h        \
	padkit/include/padkit/size.h        \
	padkit/include/padkit/verbose.h   	\
    ; ${COMPILE} ${INCLUDE_DIRS} src/ast.c -c -o obj/ast.o

obj/gfuzzer.o: .FORCE                 	\
    obj                                 \
    include/ast.h                       \
    include/bnf.h                       \
	padkit/include/padkit/memalloc.h    \
	padkit/include/padkit/verbose.h   	\
    src/gfuzzer.c                     	\
    ; ${COMPILE} ${INCLUDE_DIRS} src/gfuzzer.c -c -o obj/gfuzzer.o

padkit/lib/libpadkit.a: .FORCE          \
    ; make -C padkit clean lib/libpadkit.a
