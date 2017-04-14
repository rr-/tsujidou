## Prerequisites

1. Python 3.5
2. gcc

## Usage

##### Pre-release

1. To avoid having to pass extra switches all the time, create `./config.ini`:

    ```ini
    game-dir=./game
    data-dir=./data
    ```

2. Unpack the game data: `./unpack` (please be patient)
3. Make changes to the unpacked files
4. Pack the game data back: `./pack` (this will append only the changed files
   at the end of the archive)
5. Go to step 3

##### Release

1. Pack the game data back: `./pack --repack` (this will repack the whole thing
   from scratch, super slow)

## Setting up environment

#### On Windows from scratch

Because the project needs to build Python extensions, this process is more
complicated than usual.

1. Install [`msys2`](http://www.msys2.org/) and run it.
2. Install necessary programs: `pacman -S
   mingw64/mingw-w64-x86_64-{libpng,libjpeg,python3,python3-pip,gcc}`.
3. Restart `msys2` (64-bit) or run `export PATH=/mingw64/bin:$PATH`.
4. Continue at ["setting up environment on Linux"](#on-linux).

#### On Linux

1. Navigate to the toolkit's directory: `cd "C:/example dir/"`.
2. Install the toolkit's Python dependencies: `pip3 install -r
   requirements.txt`.
3. Build the toolkit's Python extensions: `python3 setup.py build`.

Now you're ready to rock. Type `./unpack -h` - it should print help with
available switches.


## To do

1. Figure out how to get the game to accept messages longer than 80 characters
2. Write LZSS GFX compressor for release

---

:warning: Caution: **make backups at all times!**
