CC = gcc
CFLAGS = -Wall -g -Isrc -I.
LEX = flex
YACC = bison
YFLAGS = -d -y

TARGET = parser

OBJS = main.o ast.o y.tab.o lex.yy.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main.o: src/main.c src/ast.h y.tab.h
	$(CC) $(CFLAGS) -c src/main.c

ast.o: src/ast.c src/ast.h
	$(CC) $(CFLAGS) -c src/ast.c

y.tab.o: y.tab.c
	$(CC) $(CFLAGS) -c y.tab.c

lex.yy.o: lex.yy.c
	$(CC) $(CFLAGS) -c lex.yy.c

y.tab.c y.tab.h: src/parser.y
	$(YACC) $(YFLAGS) src/parser.y

lex.yy.c: src/lexer.l y.tab.h
	$(LEX) src/lexer.l

clean:
	rm -f $(TARGET) *.o y.tab.c y.tab.h lex.yy.c