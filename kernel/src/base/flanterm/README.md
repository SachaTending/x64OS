This is a modified flanterm with ability to print UTF-8 chars

# How is this possible?
Flanterm has ability to parse UTF-8, but only to convert to cp437 encoding. I just modified so framebuffer backend can receive full UTF-8 chars, and use SSFN to print these characters