# tcrevenge
Hacking TrendChip Firmware (ADSL modem/router ZTE H108NS)

##Purpose
**tcrevenge** is a command line utility that helps you analyze and compose TrendChip firmware images (tclinux.bin). It reimplements the **CRC** algorithm found in firmware that prohibits broken or custom firmware uploads.

With this utility you can heavily customize your TrendChip firmware as beeing demonstrated in the https://vasvir.wordpress.com blog. All the tests have been made with ZTE H108NS but it should apply to other modem/routers that employ TrendChip firmware. For a possible list check https://wikidevi.com/wiki/Special:Ask?title=Special%3AAsk&q=%3Cq%3E[[CPU1+model::~TC3162U*]]

###Warning and Disclaimer
Although running **tcrevenge** is a non destructive operation if you apply the procedure blindly without safeguards you may end up with a brick instead of a modem router. Furthermore, when in compose mode **tcrevenge** is creating two files  named:
* heading
* padding

If you have files named like this in your current working directory it will be overriden.

##Build
**tcrevenge** is a very simple command line utility. It does only need file access. It doesn't link with fancy libraries so no config is needed for the time beeing.

If you have GNU Make you can do
```
make
```
it should be enough. If it isn't (you don't have make or something equally weird) try to compile and link it your own. It is just a file.

##Usage
###Binwalk
Let's run binwalk to get an idea of what's going on.
```
$binwalk -e tclinux.bin
DECIMAL HEXADECIMAL DESCRIPTION
--------------------------------------------------------------------------------
256 0x100 LZMA compressed data, properties: 0x5D, dictionary size: 8388608 bytes, uncompressed size: 3232020 bytes
955744 0xE9560 Squashfs filesystem, little endian, version 3.0, size: 5036741 bytes, 986 inodes, blocksize: 65536 bytes, created: Tue Feb 11 09:50:54 2014
$ls _tclinux.bin.extracted/
100 100.7z _100.extracted E9560 E9560.squashfs
$cd _tclinux.bin.extracted
$binwalk 100
DECIMAL HEXADECIMAL DESCRIPTION
--------------------------------------------------------------------------------
1430825 0x15D529 LZMA compressed data, properties: 0xD0, dictionary size: 131072 bytes, uncompressed size: 277750168 bytes
2646072 0x286038 Linux kernel version "2.6.22.15 (root@mbjsbbcf01) (gcc version 3.4.6) #1 Tue Feb 11 1c version 3.4.6) #1 Tue Feb 11 10:17:49 CST 2014"
```
###Flash
By looking inside the modem with telnet we can see how these all endup in the flash
```
$ telnet 192.168.1.1
Trying 192.168.1.1...
Connected to 192.168.1.1.
Escape character is '^]'.
 login: admin
Password:
# cat /proc/mtd 
dev:    size   erasesize  name
mtd0: 00010000 00010000 "bootloader"
mtd1: 00010000 00010000 "romfile"
mtd2: 000e9560 00010000 "kernel"
mtd3: 004cf000 00010000 "rootfs"
mtd4: 007a0000 00010000 "tclinux"
mtd5: 00040000 00010000 "reservearea"
```

* p0 (partition 0) is 64k has the primary bootloader. Running strings against it is instructive indeed. It includes a minimal web server and it should be (never done it) possible to recover the modem from a bad firmware image. That means it must have the same CRC algorithm with the normal software for uploading firmware. The contents of this partition do not change with a firmware update.
* p1 is the configuration file as saved by the WEB interface. For some reason it is called romfile.
* p2 is the secondary bootloader and probably the kernel and some initrd image. The contents of the partition are exact match with the firmware image tclinux.bin starting at position 0x15D529 as extracted by binwalk above.
* p3 is the squashfs starting at 0x9560 of the tclinux.bin.
* p4 holds the tclinux.bin verbatim so the primary bootloader can recover the modem
* p5 is a reserved partition

###Firmware layout
|Offset | Section |
|-------|---------|
| 1     | 2       |

```
$dd if=tclinux.bin.orig of=kernel skip=256 count=`binwalk ../tclinux.bin.orig | awk '/Squash/ {print $1 - 256;}'` bs=1
```
See https://vasvir.wordpress.com for more information.

###Analying firmware
###Composing firmware
