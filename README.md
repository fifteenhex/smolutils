# smolutils

Like coreutils but smol.


## Why?!

For nommu targets even uclibc + busybox isn't very usable:

- For nommu every process loads its own copy of everything because there
  is no virtual addressing to allow sharing of common physical pages so
  every process has its own copy of uclibc and busybox and even if that
  is ~100K you will end up using a lot of memory just getting booted and
  might not be able to allocate a big enough single block to run anything.
  Maybe FDPIC solves this but that doesn't seem to be supported on m68k.


## What

| name      | progress   | binary name (if multicall) | notes |
|-----------|------------|----------------------------|-------|
| cat       |            |                            |       |
| chmod     | stub       |                            |       |
| cp        |            |                            |       |
| df        |            |                            |       |
| dhcpc     |            |                            |       |
| dmesg     |            |                            |       |
| getty     |            |                            |       |
| init      |            |                            |       |
| kill      |            |                            |       |
| less      |            |                            |       |
| ln        |            |                            |       |
| lz4       |            |                            |       |
| man       |            |                            |       |
| mkdir     |            |                            |       |
| mount     |            |                            |       |
| mv        |            |                            |       |
| ping      |            |                            |       |
| ps        |            |                            |       |
| sha256sum |            |                            |       |
| smolsh    |            |                            |       |
| sntp      |            |                            |       |
| startup   |            |                            |       |
| touch     |            |                            |       |
| umount    |            |                            |       |
| uname     |            |                            |       |
| xxd       |            |                            |       |
