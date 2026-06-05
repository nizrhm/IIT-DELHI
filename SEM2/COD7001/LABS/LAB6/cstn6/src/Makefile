CC = gcc
CFLAGS = -Wall -g -I. -Iir

OBJ = shell/nizshell.o vm/vm.o ir/codegen.o ir/ast.o ir/lex.yy.o ir/y.tab.o

all: nizsystem edebug_bin

nizsystem: $(OBJ)
	$(CC) $(CFLAGS) -o nizsystem $(OBJ)

edebug_bin: debugger/elf_debugger.c
	$(CC) $(CFLAGS) -o edebug_bin debugger/elf_debugger.c

# --- Dependencies ---

shell/nizshell.o: shell/nizshell.c interface.h
	$(CC) $(CFLAGS) -c shell/nizshell.c -o shell/nizshell.o

vm/vm.o: vm/vm.c interface.h
	$(CC) $(CFLAGS) -c vm/vm.c -o vm/vm.o

ir/codegen.o: ir/codegen.c ir/y.tab.c interface.h
	$(CC) $(CFLAGS) -c ir/codegen.c -o ir/codegen.o

ir/ast.o: ir/ast.c ir/ast.h
	$(CC) $(CFLAGS) -c ir/ast.c -o ir/ast.o

ir/lex.yy.c: ir/lexer.l ir/y.tab.c
	flex -o ir/lex.yy.c ir/lexer.l

ir/y.tab.c: ir/parser.y
	bison -d -o ir/y.tab.c ir/parser.y

clean:
	rm -f nizsystem edebug_bin ir/lex.yy.c ir/y.tab.* **/*.o