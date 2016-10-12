# streams
Thoughts for replacing C++ iostreams.

# What is wrong with iostreams?

I am not a fan of iostreams for the following reasons:

* The overloading of << and >> do not seem like an appropriate use of operator overloading.
* Formatting code using iostreams is much harder to read or write than the equivalent using printf or Python/Ruby/Scheme.
* It is harder to create your own streams than it should be.

It has also been said that iostream conflates formatting, buffering, and I/O.

# What are the requirements for a replacement?

* Provide extensible I/O formatting.
* Make it easy to create new stream types.
* Provide buffering options in a simple manner.
  * Does buffering need to be extensible?

# Thoughts

The [fmt](http://fmtlib.net/latest/index.html) library provides good output formatting. As long as we can make it work with our streams, output formatting is done.

Input formatting I have no thoughts about yet.

Formatting is extended in iostreams by overloading operator<< and operator>>. Can we just choose actual function names to use in their stead?

Can we skip the multiple inheritence of iostreams? Let us keep input streams and output streams separate. Users can use multiple inheritence or composition to combine them if they want.

Can buffering simply be provided by stream compositing?

Does Boost.Iostreams offer us anything?

# Possible classes

*Note:* I will only work in terms of char. Everything could be templated to also support wchar\_t and other types.

* Output\_stream
  * Output\_buffer
  * String\_output\_stream
  * FILE\_output\_stream (for stdio interop)
  * POSIX\_output\_stream (for POSIX interop on POSIX systems)
  * (Do we need file streams beyond FILE?)
  * Tcp\_output\_stream
* Input\_stream
  * _Input versions of all the Output\_stream subclasses_

