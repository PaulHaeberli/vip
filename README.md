# vip
vip - fast editing of big source code

Copy any number of source code files into a single file for editing.
When any changes are saved from vi, a process rapidly parses the single file,
finds files that need changing, and writes out the individual files.

At the same time, if any of the individual source files is changed by another 
process (like Xcode), then the single file is regenerated.

This makes it insanely fast and easy to search and edit hundreds of source files
as a single file of 0..500,000 lines of C, C++, Swift, etc.

usage:

    vip app/*.c app/*.h libgfx/*.c libgfx/*.h libios/*.m libios/*.h

SERIOUS WARNING: Please only use on source code that has been backed up, and is  
under source control.  There may be a few bugs in this implementation.  I am 
very afraid of this damaging your source code!
