# filmFS
A FUSE filesystem for Linux that allows you to log your viewing habits.

## Features
* Allows read-only access to video files in library path within mountpoint
* Logs film title, times watched, and last watched in ~/.filmfs/films.db
* Inspect logs using watchlistViewer*

\* Not yet implemented

## Configuration
The configuration file is stored at ~/.config/filmfs/config, the variable LIBRARY_PATH must be set before filmFS can be used.

```
LIBRARY_PATH=/mnt/media/films/
DEBUG=FALSE
```

## Dependencies
* GCC
* GNU make
* gzip
* FUSE development libraries
* SQLite development libraries

## Installation
Compile the project
```
make
```
Install the compiled binary
```
make install
```

### Make Targets 
- `make` - Compile the binary
- `make install` – Install binary
- `make clean` – Remove build objects
- `make fclean` - Remove build objects and binary

## Usage
```
filmfs [MOUNT_POINT]
```

See FUSE documentation for additional supported arguments.

## Intended Usecase
* Create an empty directory for the mountpoint (e.g. ~/Films/)
* Set LIBRARY_PATH in the config file to your film collection's directory
* Include the following in your startup script:
```
filmfs ~/Films/
```
* Play films through the mountpoint rather than LIBRARY_PATH itself

## License
GNU General Public License V2

Copyright (c) 2025 Jacob Niemeir
