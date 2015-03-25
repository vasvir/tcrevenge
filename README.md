# tcrevenge
Hacking TrendChip Firmware (ADSL modem/router ZTE H108NS)

##Purpose
**tcrevenge** is a command line utility that helps you analyze and compose TrendChip firmware images (tclinux.bin). It reimplements the **CRC** algorithm found in firmware that prohibits broken or custom firmware uploads.

With this utility you can heavily customize your TrendChip firmware as beeing demonstrated in the https://vasvir.wordpress.com/2015/03/08/reverse-engineering-trendchip-firmware-zte-h108ns-part-i/ blog. All the tests have been made with ZTE H108NS but it should apply to other modem/routers that employ TrendChip firmware. For a possible list check https://wikidevi.com/wiki/Special:Ask?title=Special%3AAsk&q=%3Cq%3E[[CPU1+model::~TC3162U*]]

###Warning and Disclaimer
Although running **tcrevenge** is a non destructive operation if you apply the procedure blindly without safeguards you may end up with a brick instead of a modem router. Furthermore, when in compose mode **tcrevenge** is creating two files named (heading, padding) with user specified names.

If you have files named like this in your current working directory it will be overriden. This is clearly a user error but the software could safeguard against.

##Build
**tcrevenge** is a very simple command line utility. It does only need file access. It doesn't link with fancy libraries so no software configuration is required for the time beeing.

If you have GNU Make you can do
```
make
```
it should be enough. If it isn't (you don't have make or something equally weird) try to compile and link it your own. It is just a **C** file.

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
So the firmware image looks like this
* 0-0x100 Header
* secondary bootloader (Couldn’t figure which was it)
* Linux kernel + initrd (Not sure which order)
* 0x09560 squashfs
* padding to 4K

The header has the following fields
* 0x00 magic number = 0x32524448
* 0x04 magic device (or header size – not sure) = 0x100
* 0x08 tclinux size = actual file size in bytes with header included
* 0x0C tclinux checsum = CRC – XOR checksum see below how to calculate
* 0x10 firmware version string = “7.0.1.0\n” in my case
* 0x30 a newline = “\n”
* 0x50 squashfs offset = 0x9560 in my case
* 0x54 squashfs size = size of squashfs size as reported by binwalk 5038080 bytes padded to 4096 (0x1000) sector 5038080 (0x4CE000)
* 0x5C model string = “3 6035 122 74\n” in my case

See https://vasvir.wordpress.com/2015/03/08/reverse-engineering-trendchip-firmware-zte-h108ns-part-i/ for more information.

###Analying firmware
Running **tcrevenge** with -c enters the analyzing mode. It prints entries found in the header of the supplied firmware and where possible the computed values so a user can tell if it is safe (with full disclaimers) to use **tcrevenge** in order to customize its firmware.
```
./tcrevenge -c ../tclinux.bin.orig
Manual check (binwalk): header size must be 256 (0x0100)
Magic number: 0x32524448 found 0x32524448 ...ok
Magic device: 0x00000100 found 0x00000100 ...ok
tclinux.bin size: 5993824 found 5993824 ...ok
tclinux.bin chekcsum: 0x484DDDF4 found 0x484DDDF4 ...ok
Firmware version: 7.0.1.0
Manual check (binwalk): squashfs offset must be at 0x000E9560
Manual check (mtd partition dump): squashfs size (padded to erase_size at 4K (0x1000)) must be at 5038080 (0x004CE000)
Manual check (all tests have been done with model 3) Model: 3 6035 122 79
```

###Composing firmware
The following command line can extract the kernel and initrd from the original image
```
$dd if=tclinux.bin.orig of=kernel skip=256 count=`binwalk ../tclinux.bin.orig | awk '/Squash/ {print $1 - 256;}'` bs=1
```
The tcrevenge program will output the header and the necessary padding and a nice command line suggestion to create the new **tclinux.bin**.
```
$./tcrevenge -k kernel -s squashfs-root.sq -o header -p padding
Creating necessary squashfs paddingfile padding 1352
Magic number: 0x32524448 at 0x00
Magic device: 0x00000100 at 0x04
tclinux.bin size: 5993824 (0x005B7560) at 0x08
tclinux.bin checksum: 0xD4BEA4AA at 0x0C
Firmware version at 0x10: 7.0.1.0
squashfs offset: 955488 (0x000E9460) at 0x50
squashfs size: 5038080 (0x004CE000) at 0x54
Model at 0x5C: 3 6035 122 74
Writing header to header. Create image with
         cat header kernel squashfs-root.sq padding > tclinux.bin
$
```
##TODO
* Don't overwrite existing local files without warning.
* command line option for model instead of hard wiring it.
* warn if the modifications to the firmware lead to flash sizes bigger than 16MB (maybe also configurable flash size)
