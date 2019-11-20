CC=g++
INCLUDES = -I../../../include -Iinclude -I../BlacklightCrypto/include
LIBDIR = -L/home/cpp/
CXXFLAGS= --std=c++17 $(INCLUDES) $(DEFINES) -g -O0 -fPIC -DBOOST_ERROR_CODE_HEADER_ONLY -DBL_TST
CXXOBJECTS = src/Networking/Acceptor.o src/Networking/Buffer.o src/Networking/Endpoint.o src/Networking/Error.o src/Networking/TCPSocket.o src/Threads/Worker.o
TLSOBJECTS = src/Networking/TLS/TLSSocket.o src/Networking/TLS/Context.o
BLEOBJECTS = src/Networking/BLE/BLESocket.o src/Networking/BLE/Context.o
OUTPUT = ../../../build/BlacklightSockets
LDLIBS = -lgmpxx -lgmp -lcryptopp -lssl -lcrypto -ldl -lpthread

lib: $(CXXOBJECTS)
	$(CC) -o $(OUTPUT).a -shared $(INCLUDES) $(CXXOBJECTS) $(LIBDIR) $(LDLIBS)

TLSclient: CXXFLAGS += -D_CLIENT -DBL_TLS
TLSclient: $(CXXOBJECTS) $(TLSOBJECTS) Driver.o
	$(CC) -o $(OUTPUT)Client $(INCLUDES) $(CXXOBJECTS) $(TLSOBJECTS) Driver.o $(LIBDIR) $(LDLIBS) -lpthread -g

TLSserver: CXXFLAGS += -D_SERVER -DBL_TLS
TLSserver: $(CXXOBJECTS) $(TLSOBJECTS) Driver.o
	$(CC) -o $(OUTPUT)Server $(INCLUDES) $(CXXOBJECTS) $(TLSOBJECTS) Driver.o $(LIBDIR) $(LDLIBS) -g

BLEclient: CXXFLAGS += -D_CLIENT -DBL_BLE
BLEclient: $(CXXOBJECTS) $(BLEOBJECTS) Driver.o
	$(CC) -o $(OUTPUT)Client $(INCLUDES) $(CXXOBJECTS) $(BLEOBJECTS) Driver.o $(LIBDIR) $(LDLIBS) -lpthread -g

BLEserver: CXXFLAGS += -D_SERVER -DBL_BLE
BLEserver: $(CXXOBJECTS) $(BLEOBJECTS) Driver.o
	$(CC) -o $(OUTPUT)Server $(INCLUDES) $(CXXOBJECTS) $(BLEOBJECTS) Driver.o $(LIBDIR) $(LDLIBS) -g

NOEclient: CXXFLAGS += -D_CLIENT -DBL_NOE
NOEclient: $(CXXOBJECTS) Driver.o
	$(CC) -o $(OUTPUT)Client $(INCLUDES) $(CXXOBJECTS) Driver.o $(LIBDIR) $(LDLIBS) -lpthread -g

NOEserver: CXXFLAGS += -D_SERVER -DBL_NOE
NOEserver: $(CXXOBJECTS) Driver.o
	$(CC) -o $(OUTPUT)Server $(INCLUDES) $(CXXOBJECTS) Driver.o $(LIBDIR) $(LDLIBS) -g

clean:
	rm -f *.o src/Networking/*.o src/Networking/TLS/*.o src/Networking/BLE/*.o src/Threads/*.o $(OUTPUT).a $(OUTPUT)Client $(OUTPUT)Server
