### Compile & Run

```
mkdir build
cd build
cmake ..
make clean all
```

### Examples

1. Measure "open" syscall (around 1 min)

```
./syscallmeter 
```

2. Measure "rename" syscall (around 5-10 mins)

```
./syscallmeter -m rename
```

3. Measure "write_unlink" syscall (around few seconds)

```
./syscallmeter -m write_unlink
```

4. Measure "write_sync" syscall
