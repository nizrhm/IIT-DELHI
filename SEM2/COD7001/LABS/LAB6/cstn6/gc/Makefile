CC = gcc
CFLAGS = -Wall -Wextra -I./shell -I./ir -I./gc -I./vm -I./parser -I./ir/src

OBJS = shell/minishell.o shell/builtins.o shell/execute.o shell/parse.o \
       gc/main.o vm/main.o ir/ast.o ir/lex.yy.o ir/parser.tab.o \
       parser/debugger.o

TARGET = mysystem

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

ir/y.tab.c ir/y.tab.h: ir/src/parser.y
	cd ir && bison -dy src/parser.y

ir/lex.yy.c: ir/src/lexer.l ir/y.tab.h
	cd ir && flex src/lexer.l

ir/lex.yy.o: ir/lex.yy.c
	$(CC) $(CFLAGS) -c ir/lex.yy.c -o ir/lex.yy.o

ir/parser.tab.o: ir/y.tab.c
	$(CC) $(CFLAGS) -c ir/y.tab.c -o ir/parser.tab.o

ir/ast.o: ir/src/ast.c
	$(CC) $(CFLAGS) -c ir/src/ast.c -o ir/ast.o

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) ir/y.tab.c ir/y.tab.h ir/lex.yy.c

.PHONY: all clean