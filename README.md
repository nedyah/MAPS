# Project 3: Memory Allocator

## ABOUT 
This is a really fun program that allocates memory using mmap and determines whether we need to create an entirely new block
or reallocate old memory. It will also reimplement a free function that will delete either an block of memory or just the small 
part within the memory block. It also reimplements calloc and realloc. 

## INCLUDED FILES 
- allocator.c
- allocator.h 

## EXAMPLE VISUALIZATION
![VISUAL](https://github.com/usf-cs326-fa19/P3-nedyah/blob/master/output.png)

## RUN VIZ SCRIPT 
```bash
./tests/viz/visualize-mem.bash [file with mem data].txt [file to output].png 
```


## to compile
```bash
make
LD_PRELOAD=$(pwd)/allocator.so ls /
```

(in this example, the command `ls /` is run with the custom memory allocator instead of the default).

## Testing

To execute the test cases, use `make test`. To pull in updated test cases, run `make testupdate`. You can also run a specific test case instead of all of them:

```
# Run all test cases:
make test

# Run a specific test case:
make test run=4

# Run a few specific test cases (4, 8, and 12 in this case):
make test run='4 8 12'
```
