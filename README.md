# streams

Experiments in what I might like a replacement for C++ iostreams to be like. (Also an opportunity to try applying some libraries like the GSL.)

# What is wrong with iostreams?

I am not a fan of iostreams for the following reasons:

* The overloading of &lt;&lt; and &gt;&gt; do not seem like an appropriate use of operator overloading.
* Formatting code using iostreams is much harder to read or write than the equivalent using printf or Python/Ruby/Scheme.
* It is harder to create your own streams than it should be.
* No out-of-the-box ability to create a stream from a FILE\*. (Or to get a FILE\* or OS-specific file descriptor/handle from a stream.)

It has also been said that iostream conflates formatting, buffering, and I/O.

# What are the requirements for a replacement?

* Provide extensible I/O formatting.
* Make it easy to create new stream types.
* Provide buffering options in a simple manner.
  * Does buffering need to be extensible?
* Perhaps not a _requirement_, but we ought to support stream composition as well.
* It should be trivial to make a stream from a stdio FILE\*.

# wchar\_t &amp; friends...

I will only work in terms of char for now. I'm not going to bother templating everything to support wchar\_t and other types yet.

(Can't we all just agree to use UTF-8 everywhere? _sigh_ I didn't think so.)

Although such issues are mostly/entirely the purview of formatting, and since I'm currently just using {fmt} for output formatting, it already handles this.

# Notes

## Formatting

The [fmt](http://fmtlib.net/latest/index.html) library provides good output formatting. So we use that for output formatting.

Input formatting I have no thoughts about yet. (Regex?)

Formatting is extended in iostreams by overloading operator&lt;&lt; and operator&gt;&gt;. Could we just choose named functions to use in their stead? (The {fmt} library supports operator&lt;&lt; overloads. I'm guessing it would need to be modified in order to support a different extension method.)

(By the way, unlike printf, {fmt} also allows extensibility through user created format options.)

## Multiple inheritance

Can we skip the multiple inheritence of iostreams? Let's keep input streams and output streams separate. Users can use multiple inheritence or composition to combine them if they want.

## Buffering

Can buffering simply be provided by stream compositing?

## Virtual functions

I started to use compile-time polymorphism for Ostream and Istream, but I didn't want every function that wanted to take an Ostream& or Istream& parameter to have to be a template. So, I've used run-time polymorphism instead. (The Stdio\_\*stream class do use compile-time polymorphism.)

## Binary mode &amp; text mode

What about binary versus text mode? That seems like something that should be done by the formatting parts instead of the (binary) stream parts. (I don't use MS Windows, so I'm not worried about it...yet.)

## Seek &amp; tell

I'm not sure what to do with these yet.

Some streams don't support seeking. I'm thinking I'd like to only have those functions on a stream if they're support rather than have them on base classes but leaving them unimplemented on some leaf classes.

Seek and tell are arguably not properties of a stream at all. They are properties of the underlying data source/sink.

It is awfully convenient to have seek/tell member functions on the streams even if they are features of the underlying source/sink.

When you have a combined source + sink (e.g. FILE\*) that can be open for both reading and writing at the same time, the current position is sometimes (always?) shared between reading and writing. Which messes with trying to keep input and output strictly separated. Is it OK if we don't allow files to be simultaneously open for reading and writing? (I can't think of a time I did that in pratice.)

## Exceptions

I am using exceptions. A standard library would need to have an option for not using exceptions. But I want to design for exceptions first.

## Easy extensibility

Handling flush for an Ostream subclass is sometimes not straight-forward. Is there a way around that? Should Ostream be a concrete class that delegates write and flush to another class for customization? Would that help?

Are these prototypes the right model for the extension points? I mean, I think functions that read/write a span is the correct choice, but what about returning the number of bytes read/written? Or anything else that would make them better without making them overly complex.

* size\_t Ostream::\_write(gsl::span&lt;const gsl::byte&gt;)
* size\_t Istream::\_read(gsl::span&lt;gsl::byte&gt; s)

## Misc

Is there still a place for a synchronous I/O library? I think there is.

Does Boost.Iostreams offer us anything? It seems mainly about making it easier to work with std::iostreams.

# Implemented thus far...

Also check the examples in the examples directory.

This code depends upon...

* [Microsoft GSL](https://github.com/Microsoft/GSL)
* [{fmt}](http://fmtlib.net/latest/index.html)
* [type\_safe](https://github.com/foonathan/type_safe)
  * ...which depends upon [debug\_assert](https://github.com/foonathan/debug_assert)

It compiles with Clang in C++14 mode.

## streams::Ostream

This class is the abstract interface for an output stream. Note that it strictly does unformatted output. Use the streams::print() function to format output for on Ostream.

It has these public member functions:

* type\_safe::size\_t write(gsl::span&lt;const gsl::byte&gt;)
* void flush()
* void put\_byte(gsl::byte)
* template&lt;typename T&gt; void put\_data(const T& t) //Write binary (unformatted) data in host endianess

There are also two private virtual member functions that subclasses can override to create a custom Ostream:

* type\_safe::size\_t \_write(gsl::span&lt;const gsl::byte&gt;)
* void \_flush() //If not overridden, this is a no-op.

## streams::print

This function allows formatted output to streams::Ostream objects using the {fmt} library.

* void print(streams::Ostream& os, fmt::CStringRef format, ...)

For example:

    streams::print(streams::stdouts, "{:.2f}\n", 3.1415926);

## streams::prints

    streams::prints(the_stream, the_string)

...is a shortcut for...

    streams::print(the_stream, "{}", the_string)

## streams::String\_ostream

This class, like std::ostringstream, collects everything written to it in a std::string. The string can be obtained with the member function...

* std::string string();

## streams::Stdio\_ostream

A base class for stdio-based Ostreams.

## streams::Simple\_stdio\_ostream

This class provides an ostream wrapped around a stdio FILE pointer. Just pass its ctor a FILE\*. The Simple\_stdio\_ostream does not claim ownership of the FILE\*; you'll have to close it yourself.

## streams::File\_ostream

This class (a subclass of Stdio\_ostream) will open a file for output. Its dtor will close the file. Just pass the path to the file to its ctor. If you pass _true_ as a second parameter, the file will be opened in append mode.

## streams::Pipe\_ostream

Another subclass of Stdio\_ostream, you give this one a command. It will pass the command to popen and allow you to write to a pipe into that command. Its dtor will pclose the pipe.

## streams::Buffered\_ostream

You construct a Buffered\_ostream with another Ostream and a buffer size. Output will be collected in the buffer until it overflows or flush is called. Then the data will be sent to the other Ostream.

This is an example of stream composition.

## streams::Istream

Istream is the abstract interface for input streams. Like Ostream, it does strictly unformatted input. (Except maybe for the getline() member function, which formats input bytes into a std::string.) There should eventually be other functions/classes to help with the formatting of input.

Some of its public member functions:

* type\_safe::size\_t read(gsl::span&lt;gsl::byte&gt; s)
* template&lt;typename T&gt; type\_safe::optional&lt;T&gt; get\_data() //Read binary (unformatted) data in host endianess.
* type\_safe::optional&lt;std::string&gt; getline() //Read until the first '\n'.

Its private virtual member function to use when creating new Istreams:

* type\_safe::size\_t \_read(gsl::span&lt;gsl::byte&gt; s)

## streams::Stdio\_istream

The base class for stdio-based istreams.

## streams::Simple\_stdio\_istream

A Stdio\_istream that doesn't claim ownership of its FILE\*.

## streams::File\_istream

For reading from files.

## streams::stdins, streams::stdouts, &amp; streams::stderrs

Istream and ostream wrappers around the stdio standard streams.

Unfortunately, stdin, stdout, and stderr are macros, or I would've just called them streams::stdin, etc. For lack of better names, I just added 's' (for "stream") to the names.

# Possible future classes and functions

* Filtered\_ostream: An abstract class for creating ostream filters.
* Posix\_ostream: A wrapper around a file descriptor.
* Tcp\_output\_stream
* Seekable

