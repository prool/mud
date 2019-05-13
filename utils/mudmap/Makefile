grafor: grafor.c
	gcc -m32 -ggdb3 grafor.c -o grafor
#	gcc -ggdb3 grafor.c -lncurses -o script.cgi
map:	grafor
	cat /home/prool/MUD/lib/world/wld/*.wld > rooms.lst
	./grafor 1 > 1.wld.html
	./grafor 40 > 40.wld.html
	cp	*.html /var/www/newzerkalo/map
clean:
	rm grafor
