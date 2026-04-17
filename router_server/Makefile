NAME     = webserv_p3
CXX      = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++17

SRCS = main.cpp \
       Router.cpp \
       Handler.cpp \
       FileSystem.cpp

OBJS = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
