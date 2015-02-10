Web-Proxy.app : csapp.o proxy.o
	gcc -o Web-Proxy proxy.o csapp.o -pthread
	
proxy.o : proxy.c
	gcc -c proxy.c -pthread -w
	
csapp.o : csapp.c
	gcc -c csapp.c -pthread
	
all : Web-Proxy.app

clean : 
	-rm Web-Proxy proxy.o csapp.o
