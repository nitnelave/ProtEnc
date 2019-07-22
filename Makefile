CXXFLAGS = -I src/ -std=c++2a -Wall -pedantic -O3
all: binary

binary: example/http_connection

example/http_connection: example/http_connection.cc src/protenc.h
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@ $<

clean:
	$(RM) example/http_connection
