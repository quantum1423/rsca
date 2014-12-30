OBJS	= soundchange.tab.o lex.yy.o automaton.o apply.o

it:	$(OBJS)
	g++ -O3 -o rsca $^

soundchange.tab.o: soundchange.tab.c
	g++ -O3 -c -g $<
soundchange.tab.c: soundchange.y
	bison -d soundchange.y
lex.yy.o: lex.yy.c
	g++ -O3 -c -O3 $<
lex.yy.c: soundchange.l
	flex -Cfe soundchange.l

.c.o:
	gcc -O3 -c $<
.cc.o:
	g++ -O3 -c $<

clean:
	rm -f *.o *.rpo lex.yy.c soundchange.tab.c soundchange.tab.h

