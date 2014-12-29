OBJS	= soundchange.tab.o lex.yy.o automaton.o apply.o

it:	$(OBJS)
	clang++ -O3 -o rsca $^

soundchange.tab.o: soundchange.tab.c
	clang++ -O3 -c $<
soundchange.tab.c: soundchange.y
	bison -d soundchange.y
lex.yy.o: lex.yy.c
	clang++ -O3 -c $<
lex.yy.c: soundchange.l
	flex -Cfe soundchange.l

.c.o:
	clang -O3 -c $<
.cc.o:
	clang++ -O3 -c $<

clean:
	rm -f *.o *.rpo lex.yy.c soundchange.tab.c soundchange.tab.h
