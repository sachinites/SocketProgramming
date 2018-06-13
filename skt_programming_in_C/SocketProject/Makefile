target: gen.exe pim_recv.exe igmp_recv.exe sniffer.exe

gen.exe: src/pkt_generator.o src/common.o
	@echo "Building gen.exe" 
	gcc -g src/pkt_generator.o src/common.o -o gen.exe -lpthread
pkt_generator.o: src/pkt_generator.c
	@echo "compiling src/pkt_generator.c"
	gcc -g -c -I include/ src/pkt_generator.c -o src/pkt_generator.o 
common.o: src/common.c
	@echo "compiling src/common.o"
	gcc -g -c -I include/ src/common.c -o src/common.o
pim_recv.exe: src/pim_pkt_reciever.o src/common.o
	@echo "Building pim_recv.exe"
	gcc -g src/pim_pkt_reciever.o src/common.o -o pim_recv.exe
igmp_recv.exe: src/igmp_pkt_reciever.o src/common.o
	@echo "Building igmp_recv.exe"
	gcc -g src/igmp_pkt_reciever.o src/common.o -o igmp_recv.exe
igmp_pkt_reciever.o: src/igmp_pkt_reciever.c
	@echo "compiling igmp_pkt_reciever.o"
	gcc -g -c -I include/ src/igmp_pkt_reciever.c -o src/igmp_pkt_reciever.o
sniffer.exe: src/sniffer.o src/common.o
	@echo "Building sniffer.exe"
	gcc -g src/sniffer.o src/common.o -o sniffer.exe
sniffer.o: src/sniffer.c
	@echo "compiling src/sniffer.o"
	gcc -g -c -I include/ src/sniffer.c -o src/sniffer.o
pim_pkt_reciever.o: src/pim_pkt_reciever.c
	@echo "compiling src/pim_pkt_reciever.c"
	gcc -g -c -I -Wall include/ src/pim_pkt_reciever.c -o src/pim_pkt_reciever.o
clean:
	@echo "Cleaning ...."
	rm src/*.o *.exe

