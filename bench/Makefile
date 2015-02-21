all : plot stat

CXX_EXTRA?=""

plot : plot.cpp
	clang++ -std=c++1y $< -I../bricks -o plot -g

stat : stat.cpp
	clang++ -std=c++1y $< -I../bricks -o stat -Wall -Wextra -Weffc++ -g $(CXX_EXTRA)
