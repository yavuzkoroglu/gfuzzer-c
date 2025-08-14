include padkit/compile.mk

INCLUDE_DIRS=-Iinclude -Ipadkit/include
OBJECTS=obj/bnf.o obj/decisiontree.o obj/gfuzzer.o obj/grammargraph.o

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

obj/bnf.o: .FORCE                       \
    obj                                 \
    include/bnf.h                       \
    ; ${COMPILE} ${INCLUDE_DIRS} src/bnf.c -c -o obj/bnf.o

obj/decisiontree.o: .FORCE              \
    obj                                 \
    include/decisiontree.h              \
    include/grammargraph.h              \
	padkit/include/padkit/arraylist.h   \
	padkit/include/padkit/chunk.h       \
	padkit/include/padkit/indextable.h  \
	padkit/include/padkit/item.h        \
    ; ${COMPILE} ${INCLUDE_DIRS} src/decisiontree.c -c -o obj/decisiontree.o

obj/grammargraph.o: .FORCE              \
    obj                                 \
    include/bnf.h                       \
    include/grammargraph.h              \
	padkit/include/padkit/arraylist.h   \
	padkit/include/padkit/chunk.h       \
	padkit/include/padkit/indextable.h  \
	padkit/include/padkit/invalid.h     \
	padkit/include/padkit/item.h        \
	padkit/include/padkit/repeat.h      \
	padkit/include/padkit/size.h        \
	padkit/include/padkit/swap.h        \
	padkit/include/padkit/verbose.h     \
    ; ${COMPILE} ${INCLUDE_DIRS} src/grammargraph.c -c -o obj/grammargraph.o

obj/gfuzzer.o: .FORCE                 	\
    obj                                 \
    include/bnf.h                       \
    include/decisiontree.h              \
    include/grammargraph.h              \
	padkit/include/padkit/memalloc.h    \
	padkit/include/padkit/verbose.h   	\
    src/gfuzzer.c                     	\
    ; ${COMPILE} ${INCLUDE_DIRS} src/gfuzzer.c -c -o obj/gfuzzer.o

padkit/lib/libpadkit.a: .FORCE          \
    ; make -C padkit clean lib/libpadkit.a
