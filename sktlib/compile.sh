rm -f *.o
rm -f *.exe
cd CommandParser
make clean
cd ..
g++ -g -c tcp_mgmt.cpp -o tcp_mgmt.o
g++ -g -c tcp_cli.cpp -o tcp_cli.o
g++ -g -c main.cpp -o main.o
g++ -g -c tcp_cli_handler.cpp -o tcp_cli_handler.o
cd CommandParser
make
cd ..
g++ -g tcp_mgmt.o tcp_cli.o main.o tcp_cli_handler.o -o tcp_server.exe -L CommandParser/ -lcli -lpthread  -lrt