INCLUDE_DIR=$(shell echo ~)/local/include
LIBRARY_DIR=$(shell echo ~)/local/lib
DESDTIR=/
PREFIX=/usr

CXX=clang++
CXXFLAGS=-L${LIBRARY_DIR} -I${INCLUDE_DIR} -O2 -g -std=c++11 -fPIC -Wall -Wextra -march=native

all: sls libsls.so libsls.a test_client convert fsck

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

sls: src/sls.cc sls.pb.o server.o config.o serialize.o src/config.h
	${CXX} ${CXXFLAGS} src/sls.cc server.o config.o sls.pb.o serialize.o -o sls -lprotobuf -lpthread -lhgutil -lstdc++ -lboost_program_options -lcurl -ljsoncpp -lsmplsocket

fsck: src/fsck.cc sls.pb.o
	${CXX} ${CXXFLAGS} src/fsck.cc sls.pb.o -o fsck -lprotobuf -lpthread -lhgutil -lstdc++ -lboost_program_options -lcurl -ljsoncpp

convert: src/convert.cc legacy.pb.o
	${CXX} ${CXXFLAGS} src/convert.cc legacy.pb.o -o convert -lprotobuf -lpthread -lhgutil -lstdc++ -lboost_program_options -lcurl -ljsoncpp

test_client: src/test_client.cc sls.pb.o slsc.o client.o serialize.o
	${CXX} ${CXXFLAGS} src/test_client.cc sls.pb.o slsc.o client.o serialize.o -o test_client -lprotobuf -lpthread -lhgutil -lstdc++ -lcurl -ljsoncpp -lboost_program_options -lsmplsocket -lsls

libsls.so: slsc.o client.o error.o sls.pb.o serialize.o
	${CXX} ${CXXFLAGS} -shared -Wl,-soname,libsls.so -o libsls.so slsc.o client.o error.o sls.pb.o serialize.o

libsls.a: slsc.o client.o error.o sls.pb.o serialize.o
	ar rcs libsls.a slsc.o client.o error.o sls.pb.o serialize.o

server.o: src/server.cc
	${CXX} ${CXXFLAGS} -c src/server.cc -o server.o

serialize.o: src/serialize.cc
	${CXX} ${CXXFLAGS} -c src/serialize.cc -o serialize.o

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

legacy.pb.o: src/legacy.pb.cc
	${CXX} ${CXXFLAGS} -c src/legacy.pb.cc -o legacy.pb.o

src/legacy.pb.cc: legacy.proto
	protoc --cpp_out=src/ legacy.proto

src/sls.pb.cc: sls.proto
	protoc --cpp_out=src/ sls.proto

clean:
	rm -f *.o
	rm -f *.so *.a
	rm -f test_client
	rm -rf sls
	rm -rf src/*.pb.cc
	rm -rf src/*.pb.h
	rm -rf src/*_pb2.py
	rm -rf src/*.pyc
