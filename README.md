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
| ln        |            | probably touch later?      |       |
| lz4       |            |                            |       |
| man       |            |                            |       |
| mkdir     |            | probably touch later?      |       |
| mount     |            |                            |       |
| mv        |            |                            |       |
| ping      |            |                            |       |
| ps        |            |                            |       |
| sha256sum |            |                            |       |
| smolsh    |            |                            |       |
| sntp      |            |                            |       |
| startup   |            |                            |       |
| touch     |            |                            |       |
| umount    |            | mount                      |       |
| uname     |            |                            |       |
| xxd       |            |                            |       |


## Design

### No config files

The system is configured using the kernel command line, for example:

```
console=ttyGF0 root=/dev/vda -- smolinit.getty=/dev/ttyGF0 smolinit.hostname=tripleO smolinit.dhcpif=eth0
```

The first part is setting the kernel's console and root parameters,
the `--` tells the kernel to pass the remining junk to init.

Currently these parameters are implemented:
- `smolinit.getty=<tty device>` - This causes a getty to be spawned on that tty, this can be specified multiple times
- `smolinit.hostname=<string>` - Set the hostname, only the first instance is used.
- `smolinit.dhcpif=<network interface>` - Causes DHCP to be used to configure this interface, for now only the first instance is used

### DNS resolver

Since nolibc has no DNS resolver and I don't want something massive like
that in every binary that just so happens to need to do DNS lookups
that stuff is deligated to another program called resolv. To avoid awful
string parsing stuff the process that needs DNS resolution creates a
memfd and resolv puts the struct with the results in there.


## Running in QEMU


```
qemu-system-x86_64 -kernel bzImage -drive file=x86_64.erofs,format=raw,if=virtio -nic user,model=virtio-net-pci -serial mon:stdio -append "console=ttyS0 root=/dev/vda ro -- smolinit.getty=/dev/ttyS0 smolinit.hostname=eighty6 smolinit.dhcpif=eth0"
```


## TODO

Fix terminal control handling stuff so ctrl-c works..
