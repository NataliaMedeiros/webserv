NAME = webserv
CXX = c++
# NOTE: C++ standard flag
# On newer compilers (Linux at school, clang 16+): use -std=c++23
# On Noors macbook (Mac with clang 14):          use -std=c++2b
# Both mean the same thing - c++2b is just the draft name for C++23.
# Switch the flag below depending on the machine:
CXXFLAGS = -Wall -Wextra -Werror -std=c++2b
# CXXFLAGS = -Wall -Wextra -Werror -std=c++23

INCLUDES = -Iincludes

SRCS =	src/main.cpp \
		src/Fd.cpp \
		src/Net.cpp \
		src/Listener.cpp \
		src/EventLoop.cpp \
		src/ConnectionStore.cpp \
		src/ClientConnection.cpp \
		src/ServerManager.cpp \
		src/stubs.cpp \
		src/HttpRequestParser.cpp \
		src/ConfigParser.cpp \
		src/FileSystem.cpp \
		src/Handler.cpp \
		src/Router.cpp

OBJS = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

test_feature: src/test_features.cpp Router.o Handler.o FileSystem.o ConfigParser.o
	c++ -Wall -Wextra -Werror -std=c++17 $^ -o test_feature

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all
