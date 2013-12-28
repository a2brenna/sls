CXX=g++
CXXFLAGS=-O2 -g -std=c++11

all: sls

sls: src/sls.cc
	${CXX} ${CXXFLAGS} src/sls.cc -o sls

clean:
	rm -f *.o
	rm -rf sls
	rm -rf src/*.pb.cc
	rm -rf src/*.pb.h
	rm -rf src/*_pb2.py
	rm -rf src/*.pyc
