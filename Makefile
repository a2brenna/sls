CXX=g++
CXXFLAGS=-O2 -g -std=c++11

all: sls

sls: src/sls.cc sls.pb.o
	${CXX} ${CXXFLAGS} src/sls.cc sls.pb.o -o sls -lprotobuf -lpthread

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
