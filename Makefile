CXX      ?= g++
CXXFLAGS += -g -Wall -std=c++11 -DLOCAL
LIBS     := -lhcs -lgmp -lmicrohttpd -lcurl

all: SetupServer VoterClient PublicBoard DecryptServer

SetupServer: src/SetupServer.cpp
	$(CXX) $(CXXFLAGS) -o SetupServer src/SetupServer.cpp $(LIBS)

VoterClient: src/VoterClient.cpp
	$(CXX) $(CXXFLAGS) -o VoterClient src/VoterClient.cpp $(LIBS)

PublicBoard: src/PublicBoard.cpp
	$(CXX) $(CXXFLAGS) -o PublicBoard src/PublicBoard.cpp $(LIBS)

DecryptServer: src/DecryptServer.cpp
	$(CXX) $(CXXFLAGS) -o DecryptServer src/DecryptServer.cpp $(LIBS)

clean:
	rm -f SetupServer VoterClient PublicBoard DecryptServer

.PHONY: all clean
