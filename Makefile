etc.dll : etc.c
	gcc -Wall -g --shared -o $@ $^ -I/usr/local/include -L/usr/local/bin -llua53

clean :
	rm etc.dll
