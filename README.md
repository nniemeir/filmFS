# filmFS
A FUSE filesystem that allows you to log your viewing habits.

## Features
- [x] Represent video files from library on mountpoint
- [x] Log reads from media players
- [] Configuration support*

\* currently hardcoded in common.h

## Dependencies
* GCC
* GNU make
* gzip
* FUSE development libraries

## Installation
Compile the project
```
make
```
Install the compiled binary
```
sudo make install
```

### Make Targets 
- `make` - Compile the binary
- `make install` – Install binary
- `make clean` – Remove build objects
- `make fclean` - Remove build objects and binary

## License
GNU GENERAL PUBLIC LICENSE V2
Copyright (c) 2025 Jacob Niemeir