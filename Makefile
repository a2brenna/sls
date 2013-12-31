CXX=g++
CXXFLAGS=-O0 -g -std=c++11

all: sls test_client

sls: src/sls.cc sls.pb.o
	${CXX} ${CXXFLAGS} src/sls.cc sls.pb.o -o sls -lprotobuf -lpthread -lhgutil

test_client: src/test_client.cc slsc.o sls.pb.o
	${CXX} ${CXXFLAGS} src/test_client.cc sls.pb.o slsc.o -o test_client -lprotobuf -lpthread -lhgutil

slsc.o: src/slsc.cc
	${CXX} ${CXXFLAGS} -c src/slsc.cc -o slsc.o

sls.pb.o: src/sls.pb.cc
	${CXX} ${CXXFLAGS} -c src/sls.pb.cc -o sls.pb.o

src/sls.pb.cc: sls.proto
	protoc --cpp_out=src/ sls.proto

clean:
	rm -f *.o
	rm -rf sls
	rm -rf src/*.pb.cc
	rm -rf src/*.pb.h
	rm -rf src/*_pb2.py
	rm -rf src/*.pyc
