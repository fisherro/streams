CXX=clang++

CXXFLAGS+=-std=c++14 -stdlib=libc++
CXXFLAGS+=-I../../GSL
CXXFLAGS+=-I../../fmt
CXXFLAGS+=-I../../type_safe/include
CXXFLAGS+=-I../../debug_assert
CXXFLAGS+=-I..

LDFLAGS+=-L../../fmt/build/fmt -lfmt

all: examples

clean:
	rm examples

