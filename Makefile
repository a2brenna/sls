CXX=clang
CXXFLAGS=-O2 -g -std=c++11 -fPIC -Wall -Wextra
DESTDIR=/
PREFIX=/usr/local/

all: sls test_client libsls.so libsls.a src/slsfsck.py src/sls_pb2.py

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


sls: src/sls.cc sls.pb.o src/config.h
	${CXX} ${CXXFLAGS} src/sls.cc sls.pb.o -o sls -lprotobuf -lpthread -lhgutil -lstdc++

test_client: src/test_client.cc slsc.o sls.pb.o
	${CXX} ${CXXFLAGS} src/test_client.cc sls.pb.o slsc.o -o test_client -lprotobuf -lpthread -lhgutil -lstdc++

libsls.so: slsc.o sls.pb.o
	${CXX} ${CXXFLAGS} -shared -Wl,-soname,libsls.so -o libsls.so slsc.o sls.pb.o

libsls.a: slsc.o sls.pb.o
	ar rcs libsls.a slsc.o sls.pb.o

slsc.o: src/slsc.cc
	${CXX} ${CXXFLAGS} -c src/slsc.cc -o slsc.o

sls.pb.o: src/sls.pb.cc
	${CXX} ${CXXFLAGS} -c src/sls.pb.cc -o sls.pb.o

src/sls.pb.cc: sls.proto
	protoc --cpp_out=src/ sls.proto

src/slsfsck.py: src/sls_pb2.py

src/sls_pb2.py: sls.proto
	protoc --python_out=src/ sls.proto

clean:
	rm -f *.o
	rm -rf sls
	rm -rf src/*.pb.cc
	rm -rf src/*.pb.h
	rm -rf src/*_pb2.py
	rm -rf src/*.pyc
