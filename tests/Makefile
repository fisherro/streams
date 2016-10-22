CXX=clang++

CXXFLAGS+=-std=c++14 -stdlib=libc++
CXXFLAGS+=-I../../GSL
CXXFLAGS+=-I../../fmt
CXXFLAGS+=-I..

CXXFLAGS+=-I../../Catch/single_include

LDFLAGS+=-L../../fmt/build/fmt -lfmt

all: tests

clean:
	rm -f tests

