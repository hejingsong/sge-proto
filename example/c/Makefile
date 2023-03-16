sge-proto: main.o sge_parser.o sge_proto.o sge_block.o sge_field.o sge_table.o sge_crc16.o
	gcc -g main.o sge_parser.o sge_proto.o sge_block.o sge_field.o sge_table.o sge_crc16.o -o sge-proto

main.o: main.c
	gcc -I../../src/core/ -g -c main.c -o main.o

sge_parser.o: ../../src/core/sge_parser.c
	gcc -I../../src/core/ -g -c ../../src/core/sge_parser.c -o sge_parser.o

sge_proto.o: ../../src/core/sge_proto.c
	gcc -I../../src/core/ -g -c ../../src/core/sge_proto.c -o sge_proto.o

sge_block.o: ../../src/core/sge_block.c
	gcc -I../../src/core/ -g -c ../../src/core/sge_block.c -o sge_block.o

sge_field.o: ../../src/core/sge_field.c
	gcc -I../../src/core/ -g -c ../../src/core/sge_field.c -o sge_field.o

sge_table.o: ../../src/core/sge_table.c
	gcc -I../../src/core/ -g -c ../../src/core/sge_table.c -o sge_table.o

sge_crc16.o: ../../src/core/sge_crc16.c
	gcc -I../../src/core/ -g -c ../../src/core/sge_crc16.c -o sge_crc16.o

.PHONY: clean
clean:
	rm -f core.*
	rm -f *.o
