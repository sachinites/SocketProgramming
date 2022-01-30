rm -f *.o
rm -f *.exe
cd CommandParser
make clean
cd ..
g++ -g -c tcp_mgmt.cpp -o tcp_mgmt.o
g++ -g -c app_cli.cpp -o app_cli.o
g++ -g -c app_main.cpp -o app_main.o
g++ -g -c network_utils.cpp -o network_utils.o
cd CommandParser
make
cd ..
g++ -g tcp_mgmt.o app_cli.o app_main.o network_utils.o -o app.exe -L CommandParser/ -lcli -lpthread  -lrt