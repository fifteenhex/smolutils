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

Even for the mmu enjoyers out there:

- If you are doing kernel development with an LLM and usually use buildroot
  as a minimal user space smolutils might be able to make your work faster.

## What

| name        | progress | binary name (if multicall) | notes               |
|-------------|----------|----------------------------|---------------------|
| blkwak      |          |                            | block multi-tool    |
| cat         |          | files                      |                     |
| chmod       |          | files                      |                     |
| chroot      |          | process                    |                     |
| chown       |          | files                      |                     |
| cp          |          | files                      |                     |
| devmem      |          |                            |                     |
| df          |          |                            |                     |
| dhcpc       |          |                            |                     |
| dmesg       |          |                            |                     |
| flash_erase |          | mtd                        |                     |
| flashcp     |          | mtd                        |                     |
| getty       |          |                            |                     |
| gpiodetect  |          | gpio                       |                     |
| gpioget     |          | gpio                       |                     |
| gpioinfo    |          | gpio                       |                     |
| gpiomon     |          | gpio                       |                     |
| gpioset     |          | gpio                       |                     |
| halt        |          | smold                      |                     |
| hwclock     |          | rtc                        |                     |
| i2cdetect   |          | i2c                        |                     |
| i2cdump     |          | i2c                        |                     |
| i2cget      |          | i2c                        |                     |
| i2cset      |          | i2c                        |                     |
| init        |          | smold                      |                     |
| insmod      |          | modules                    |                     |
| ip          |          |                            |                     |
| keeper      |          |                            | daemonise a program |
| kill        |          | process                    |                     |
| less        | stub     |                            |                     |
| ln          |          | files                      |                     |
| ls          |          |                            |                     |
| lz4         | stub     |                            | not in the build    |
| man         | stub     |                            |                     |
| mkdir       |          | files                      |                     |
| mount       |          | filesystem                 |                     |
| mtdinfo     |          | mtd                        |                     |
| mv          |          | files                      |                     |
| nanddump    |          | mtd                        |                     |
| nandwrite   |          | mtd                        |                     |
| ping        |          |                            |                     |
| poweroff    |          | smold                      |                     |
| ps          |          | process                    |                     |
| reboot      |          | smold                      |                     |
| resolv      |          |                            |                     |
| rm          |          | files                      |                     |
| rmdir       |          | files                      |                     |
| rmmod       |          | modules                    |                     |
| rtcwake     |          | rtc                        |                     |
| sha256sum   |          |                            |                     |
| smolsh      |          |                            |                     |
| sntp        |          |                            |                     |
| startup     |          |                            |                     |
| su          |          |                            | no auth, dangerous! |
| telnetd     |          |                            |                     |
| tftp        |          |                            |                     |
| touch       |          | files                      |                     |
| umount      |          | filesystem                 |                     |
| uname       |          |                            |                     |
| xxd         |          |                            |                     |

## Design

I'm coming up with this as I go. This started out as a userspace for a stupid
project but its turned out pretty useful for lots of my other projects and I'm
trying to make this nice now.

### "Sets", "Tags" and "Configs"

*sets* are groups of binaries that can be totally discarded to avoid
wasting space. I.e. if you don't have a flash device that needs the MTD sub
system in Linux you don't need all of the mtd tools. Configs on the other hand
are meant to tune tools that are included. This gets a bit messy where the too
overlap. On the MTD example, if you don't have MTD then you can drop all of the
MTD tools but there might be other tools that compile in support for working
with MTD devices. So we end up with a set for MTD and a config for MTD.

### Sets

The default is every set. Pass SETS="" with a list of what you want.

- base       - smold (init, reboot, poweroff, halt) and startup
- core       - getty, dmesg, uname, insmod, rmmod, ps, kill, free, chroot, su and keeper
- net        - Add networking utils
- hwhandling - blkwak, lsbus, rtc, devmem and the mtd, i2c and gpio tools, for poking at hardware
- filesystem - mount, umount, ls, df and the file tools like cp and cat
- misc       - smolsh, sha256sum, xxd, man and less

#### Tags

Tags will eventually have cross-set control over what gets included. This is WIP

### No config files

The system is configured using the kernel command line, for example:

```
console=ttyGF0 root=/dev/vda -- smolinit.getty=/dev/ttyGF0 smolinit.hostname=tripleO smolinit.dhcpif=eth0
```

The first part is setting the kernel's console and root parameters,
the `--` tells the kernel to pass the remining junk to init.

Currently these parameters are implemented:
- `smolinit.getty=<tty device>`         - This causes a getty to be spawned on that tty, this can be specified multiple times
- `smolinit.hostname=<string>`          - Set the hostname, only the first instance is used.
- `smolinit.insmod=<module>`            - Causes a module to be insmod'd, this can be specificed multiple times and happens before dhcp etc
- `smolinit.dhcpif=<network interface>` - Causes DHCP to be used to configure this interface, for now only the first instance is used
- `smolinit.minion="<program> [args]"`  - A program to start backgrounds and managed (restarted) by init.

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
Add basic auth. This will probably be random passwords that are shown on the "secure tty".
