### Prerequisites

1. Python 3.6

### Usage - pre-release

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

### Usage - release

1. Pack the game data back: `./pack --repack` (this will repack the whole thing
   from scratch, super slow)

### To do

1. Scripts:
    - word wrapping
    - unicode hack
2. TLG to PNG
3. PNG to TLG (maybe won't be needed?)

---

:warning: Caution: **make backups at all times!**
