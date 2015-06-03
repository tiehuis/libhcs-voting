CXX      ?= g++
CXXFLAGS += -g -Wall -std=c++11 -DLOCAL
LIBS     := -lhcs -lgmp -lmicrohttpd -lcurl

all: SetupServer VoterClient PublicBoard DecryptServer

SetupServer: SetupServer.cpp
	$(CXX) $(CXXFLAGS) -o SetupServer SetupServer.cpp $(LIBS)

VoterClient: VoterClient.cpp
	$(CXX) $(CXXFLAGS) -o VoterClient VoterClient.cpp $(LIBS)

PublicBoard: PublicBoard.cpp
	$(CXX) $(CXXFLAGS) -o PublicBoard PublicBoard.cpp $(LIBS)

DecryptServer: DecryptServer.cpp
	$(CXX) $(CXXFLAGS) -o DecryptServer DecryptServer.cpp $(LIBS)

clean:
	rm -f SetupServer VoterClient PublicBoard DecryptServer

.PHONY: all clean
