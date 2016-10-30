# streams

Experiments in replacing standard C++ iostreams. Comments welcome.

# Overview

* Use {fmt} for output formatting
* Easy to create new streams
  * Override no more than two functions to create an output stream:
    * `ptrdiff_t ostream::_write(span<const byte>)`
    * `void ostream::_flush()`
  * Override one function to create an input stream:
    * `span<byte> istream::_read(span<byte>)`
* Streams can be composed
  * Buffering provided as streams that can be composed with other streams
  * Filtering streams can be created
* Easy to interoperate with stdio
  * Implement one function (`FILE* file()`) to create stdio istream/ostream subclasses
  * Non-owning ostream and istream classes to create a stream from a `FILE*`
  * Get the `FILE*` from a stdio-based stream with `FILE* file()`
* Seekability as a mix-in class
* No overloading of the shift operators
* Formatted input is still an open question
  * I need to learn more about char traits and localization

This code uses...

* [Microsoft GSL](https://github.com/Microsoft/GSL)
* [{fmt}](http://fmtlib.net/latest/index.html)
* [Catch](https://github.com/philsquared/Catch)

# Classes and functions

Anything marked "TBD" hasn't been implemented yet.

## Common

* **seekable**: Base class with seek and tell member functions
* **stdio\_seekable**: Seekable base class for seeking on `std::FILE*`

## Unformatted output

A span of bytes can be written to any of the ostream classes via `write()`. The `put()` member function can be used to write individual binary objects (in host endianess.)

* **ostream**: Base class for unformatted output
* **buf\_ostream**: Add buffering to another ostream
* **span\_ostream**: Write output to a span&lt;byte&gt;
* **vector\_ostream**: Write output to a vector&lt;byte&gt;
* **stdio\_base\_ostream**: Base class for stdio-based ostreams
* **stdio\_ostream**: Stdio-based ostream that doesn't own its `FILE*`
* **stdio\_file\_ostream**: Seekable stdio-based file ostream
* **file\_ostream**: TBD Typically an alias for a platform-specific file ostream

## Formatted output

Free functions that take ostream classes.

* **print**: Formatted output via the {fmt} library
  * Note that fmt::format can be used for formatting directly to strings. No string-stream needed.
* **basic\_put\_string**: Output a string (without using a format string)
  * **put\_string** and **put\_wstring**
* **basic\_put\_line**: Output a string followed by a newline.
  * **put\_line** and **put\_wline**
* **basic\_put\_char**: Output a character
  * **put\_char** and **put\_wchar**

## Unformatted input

A span of bytes can be read from these istream classes with `read()`. The `get()` member function can be used to read individual binary objects (in host endianess).

* **istream**: Base class for unformatted input
* **buf\_istream**: Add buffering to another istream
* **span\_istream**: Read input from a span
* **unget\_istream**: Add an arbitrary unget buffer to another istream
* **stdio\_base\_istream**: Base class for stdio-based istreams
* **stdio\_istream**: Stdio-based ostream that doesn't own its `FILE*`
* **stdio\_file\_istream**: Seekable stdio-based file istream
* **file\_istream**: TBD Typically an alias for a platform-specific file istream

## Formatted input

Free functions that take istream classes.

* **basic\_get\_regex**: TBD Read a string using an regular expression
* **basic\_get\_line**: Read a string up to a delimiter
  * **get\_line** and **get\_wline**
* **basic\_get\_char**: Read a character
  * **get\_char** and **get\_wchar**

## Standard streams

The standard streams wrapped in stdio\_istream/stdio\_ostream.

Since the standard names are macros, I couldn't just use, e.g., `streams::stdout`. So I added an extra "s". (For "stream".)

* **stdins**: stdin
* **stdouts**: stdout
* **stderrs**: stderr

## Platform specific

* **stdio\_pipe\_ostream**: Output pipe to a command
* **stdio\_pipe\_istream**: Input pipe from a command
* **posix\_fd\_seekable**: Base class for seek/tell on POSIX file descriptors
* **posix\_base\_ostream**: A base class of ostreams using a POSIX file descriptor.
* **posix\_fd\_ostream**: A file descriptor ostream that doesn't own its fd
* **posix\_file\_ostream**: A seekable file ostream that uses the POSIX file APIs
* **posix\_base\_istream**: A base class of istreams using a POSIX file descriptor
* **posix\_fd\_istream**: A file descriptor istream that doesn't own its fd
* **posix\_file\_istream**: A seekable file istream that uses the POSIX file APIs

## Example streams

Found in examples/example.cpp.

* **Shout\_ostream**: A filter to be used with another ostream that converts all output to upper case.
* **Line\_number\_ostream**: A filter to be used with another ostream that prefixes each line with a line number.
* **Reverse\_line\_ostream**: A filter to be used with another ostream that reverses each line of output.

## Possible expansions

* Input-formatting : scanf :: {fmt} : printf
* Some quoting/unquoting functions

