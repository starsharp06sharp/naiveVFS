## naiveVFS

a young, simple and naive virtual file system based on fuse

to let it work, you will need

1. install requirements
```bash
$ sudo apt-get install libfuse-dev
```

2. build
```bash
$ make all
```

3. and run
```bash
$ ./naivevfs [mount-point] [-d] # '-d' means 'debug'(strongly recommended)
```
