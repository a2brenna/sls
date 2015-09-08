INCLUDE_DIR=$(shell echo ~)/local/include
LIBRARY_DIR=$(shell echo ~)/local/lib
DESDTIR=/
PREFIX=/usr

CXX=clang++
CXXFLAGS=-L${LIBRARY_DIR} -I${INCLUDE_DIR} -O2 -g -std=c++11 -fPIC -Wall -Wextra -march=native

all: sls libsls.so libsls.a migrate fsck sls_query

test: test_client

install: libsls.so libsls.a src/sls.h src/sls.pb.h
	mkdir -p ${DESTDIR}/${PREFIX}/lib
	mkdir -p ${DESTDIR}/${PREFIX}/include/sls
	cp *.a ${DESTDIR}/${PREFIX}/lib
	cp *.so ${DESTDIR}/${PREFIX}/lib
	cp src/sls.h ${DESTDIR}/${PREFIX}/include/sls/
	cp src/sls.pb.h ${DESTDIR}/${PREFIX}/include/sls/
	cp src/client.h ${DESTDIR}/${PREFIX}/include/sls/
	cp src/archive.h ${DESTDIR}/${PREFIX}/include/sls/
	cp src/file.h ${DESTDIR}/${PREFIX}/include/sls/

uninstall:
	rm ${DESTDIR}/${PREFIX}/lib/libsls.a
	rm ${DESTDIR}/${PREFIX}/lib/libsls.so
	rm ${DESTDIR}/${PREFIX}/include/sls.h
	rm ${DESTDIR}/${PREFIX}/include/sls.pb.h
	rm -r ${DESTDIR}/${PREFIX}/include/sls/

sls: src/sls.cc sls.pb.o server.o config.o archive.o active_key.o index.o file.o util.o src/config.h
	${CXX} ${CXXFLAGS} src/sls.cc server.o config.o sls.pb.o archive.o file.o util.o active_key.o index.o -o sls -lprotobuf -lpthread -lstdc++ -lboost_program_options -lcurl -ljsoncpp -lsmplsocket -lslog

fsck: src/fsck.cc sls.pb.o index.o archive.o file.o
	${CXX} ${CXXFLAGS} src/fsck.cc sls.pb.o index.o archive.o file.o -o fsck -lprotobuf -lpthread -lstdc++ -lboost_program_options -lcurl -ljsoncpp

migrate: src/migrate.cc legacy.pb.o file.o
	${CXX} ${CXXFLAGS} src/migrate.cc legacy.pb.o file.o -o migrate -lprotobuf -lpthread -lstdc++ -lboost_program_options -lcurl -ljsoncpp

sls_query: src/sls_query.cc client.o slsc.o sls.pb.o archive.o file.o
	${CXX} ${CXXFLAGS} src/sls_query.cc client.o slsc.o sls.pb.o archive.o file.o -o sls_query -lprotobuf -lpthread -lstdc++ -lboost_program_options -lcurl -ljsoncpp -lsmplsocket

test_client: src/test_client.cc util.o
	${CXX} ${CXXFLAGS} src/test_client.cc util.o -o test_client -lprotobuf -lpthread -lstdc++ -lcurl -ljsoncpp -lboost_program_options -lsmplsocket -lsls

libsls.so: slsc.o client.o sls.pb.o archive.o file.o
	${CXX} ${CXXFLAGS} -shared -Wl,-soname,libsls.so -o libsls.so slsc.o client.o sls.pb.o archive.o file.o

libsls.a: slsc.o client.o sls.pb.o archive.o file.o
	ar rcs libsls.a slsc.o client.o sls.pb.o archive.o file.o

server.o: src/server.cc
	${CXX} ${CXXFLAGS} -c src/server.cc -o server.o

archive.o: src/archive.cc
	${CXX} ${CXXFLAGS} -c src/archive.cc -o archive.o

active_key.o: src/active_key.cc
	${CXX} ${CXXFLAGS} -c src/active_key.cc -o active_key.o

client.o: src/client.cc
	${CXX} ${CXXFLAGS} -c src/client.cc -o client.o

index.o: src/index.cc
	${CXX} ${CXXFLAGS} -c src/index.cc -o index.o

file.o: src/file.cc
	${CXX} ${CXXFLAGS} -c src/file.cc -o file.o

util.o: src/util.cc
	${CXX} ${CXXFLAGS} -c src/util.cc -o util.o

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
	rm -f test
	rm -f sls_query
	rm -rf sls
	rm -rf migrate
	rm -rf indexer
	rm -rf fsck
	rm -rf src/*.pb.cc
	rm -rf src/*.pb.h
	rm -rf src/*_pb2.py
	rm -rf src/*.pyc
