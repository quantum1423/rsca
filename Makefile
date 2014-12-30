OBJS	= soundchange.tab.o lex.yy.o automaton.o apply.o

it:	$(OBJS)
	clang++ -O3 -o rsca -fsanitize=address $^

soundchange.tab.o: soundchange.tab.c
	clang++ -c -g -fsanitize=address $<
soundchange.tab.c: soundchange.y
	bison -d soundchange.y
lex.yy.o: lex.yy.c
	clang++ -c -g -fsanitize=address $<
lex.yy.c: soundchange.l
	flex -Cfe soundchange.l

.c.o:
	clang -O3 -c -g -fsanitize=address $<
.cc.o:
	clang++ -O3 -c -g -fsanitize=address $<

clean:
	rm -f *.o *.rpo lex.yy.c soundchange.tab.c soundchange.tab.h

