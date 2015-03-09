INCLUDE_DIR=$(shell echo ~)/local/include
LIBRARY_DIR=$(shell echo ~)/local/lib
DESDTIR=/
PREFIX=/usr

CXX=clang++
CXXFLAGS=-L${LIBRARY_DIR} -I${INCLUDE_DIR} -O2 -g -std=c++11 -fPIC -Wall -Wextra -march=native

all: sls libsls.so libsls.a src/slsfsck.py src/sls_pb2.py fsck

install: libsls.so libsls.a src/sls.h src/sls.pb.h
	cp *.a ${DESTDIR}/${PREFIX}/lib
	cp *.so ${DESTDIR}/${PREFIX}/lib
	cp src/sls.h ${DESTDIR}/${PREFIX}/include
	cp src/sls.pb.h ${DESTDIR}/${PREFIX}/include

uninstall:
	rm ${DESTDIR}/${PREFIX}/lib/libsls.a
	rm ${DESTDIR}/${PREFIX}/lib/libsls.so
	rm ${DESTDIR}/${PREFIX}/include/sls.h
	rm ${DESTDIR}/${PREFIX}/include/sls.pb.h


sls: src/sls.cc sls.pb.o server.o config.o src/config.h
	${CXX} ${CXXFLAGS} src/sls.cc server.o config.o sls.pb.o -o sls -lprotobuf -lpthread -lhgutil -lstdc++ -lgnutls -lboost_program_options -lcurl -ljsoncpp

fsck: src/fsck.cc
	${CXX} ${CXXFLAGS} src/fsck.cc sls.pb.o -o fsck -lprotobuf -lpthread -lhgutil -lstdc++ -lgnutls -lboost_program_options -lcurl -ljsoncpp

test_client: src/test_client.cc sls.pb.o
	${CXX} ${CXXFLAGS} src/test_client.cc sls.pb.o -o test_client -lprotobuf -lpthread -lhgutil -lstdc++ -lsls -lgnutls

libsls.so: slsc.o client.o error.o sls.pb.o
	${CXX} ${CXXFLAGS} -shared -Wl,-soname,libsls.so -o libsls.so slsc.o client.o error.o sls.pb.o

libsls.a: slsc.o client.o error.o sls.pb.o
	ar rcs libsls.a slsc.o client.o error.o sls.pb.o

server.o: src/server.cc
	${CXX} ${CXXFLAGS} -c src/server.cc -o server.o

client.o: src/client.cc
	${CXX} ${CXXFLAGS} -c src/client.cc -o client.o

error.o: src/error.cc
	${CXX} ${CXXFLAGS} -c src/error.cc -o error.o

slsc.o: src/slsc.cc
	${CXX} ${CXXFLAGS} -c src/slsc.cc -o slsc.o

config.o: src/config.cc
	${CXX} ${CXXFLAGS} -c src/config.cc -o config.o

sls.pb.o: src/sls.pb.cc
	${CXX} ${CXXFLAGS} -c src/sls.pb.cc -o sls.pb.o

src/sls.pb.cc: sls.proto
	protoc --cpp_out=src/ sls.proto

src/slsfsck.py: src/sls_pb2.py

src/sls_pb2.py: sls.proto
	protoc --python_out=src/ sls.proto

clean:
	rm -f *.o
	rm -f *.so *.a
	rm -f test_client
	rm -rf sls
	rm -rf src/*.pb.cc
	rm -rf src/*.pb.h
	rm -rf src/*_pb2.py
	rm -rf src/*.pyc
