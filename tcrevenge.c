#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const unsigned int header_size = 0x100;
const unsigned int magic_number = 0x32524448;
const unsigned int magic_device = 0x00000100;	// maybe the header_size
const char *firmware_version = "7.0.1.0\n";	// this looks that is not actually used
const char *model="3 6035 122 74\n";		// this is used to prevent downgrades. It is the actual version

const unsigned int magic_number_offset = 0;
const unsigned int magic_device_offset = 4;
const unsigned int tclinux_size_offset = 8;
const unsigned int tclinux_checksum_offset = 0x0C;
const unsigned int firmware_version_offset = 0x10;
const unsigned int squashfs_offset_offset = 0x50;
const unsigned int squashfs_size_offset = 0x54;
const unsigned int model_offset = 0x5C;

const char crc32_c[] = "0000000077073096ee0e612c990951ba076dc419706af48fe963a5359e6495a30edb883279dcb8a4e0d5e91e97d2d98809b64c2b7eb17cbde7b82d0790bf1d911db710646ab020f2f3b9714884be41de1adad47d6ddde4ebf4d4b55183d385c7136c9856646ba8c0fd62f97a8a65c9ec14015c4f63066cd9fa0f3d638d080df53b6e20c84c69105ed56041e4a26771723c03e4d14b04d447d20d85fda50ab56b35b5a8fa42b2986cdbbbc9d6acbcf94032d86ce345df5c75dcd60dcfabd13d5926d930ac51de003ac8d75180bfd0611621b4f4b556b3c423cfba9599b8bda50f2802b89e5f058808c60cd9b2b10be9242f6f7c8758684c11c1611dabb6662d3d76dc419001db710698d220bcefd5102a71b1858906b6b51f9fbfe4a5e8b8d4337807c9a20f00f9349609a88ee10e98187f6a0dbb086d3d2d91646c97e6635c016b6b51f41c6c6162856530d8f262004e6c0695ed1b01a57b8208f4c1f50fc45765b0d9c612b7e9508bbeb8eafcb9887c62dd1ddf15da2d498cd37cf3fbd44c654db261583ab551cea3bc0074d4bb30e24adfa5413dd895d7a4d1c46dd3d6f4fb4369e96a346ed9fcad678846da60b8d044042d7333031de5aa0a4c5fdd0d7cc95005713c270241aabe0b1010c90c20865768b525206f85b3b966d409ce61e49f5edef90e29d9c998b0d09822c7d7a8b459b33d172eb40d81b7bd5c3bc0ba6cadedb883209abfb3b603b6e20c74b1d29aead547399dd277af04db261573dc1683e3630b1294643b840d6d6a3e7a6a5aa8e40ecf0b9309ff9d0a00ae277d079eb1f00f93448708a3d21e01f2686906c2fef762575d806567cb196c36716e6b06e7fed41b7689d32be010da7a5a67dd4accf9b9df6f8ebeeff917b7be4360b08ed5d6d6a3e8a1d1937e38d8c2c44fdff252d1bb67f1a6bc57673fb506dd48b2364bd80d2bdaaf0a1b4c36034af641047a60df60efc3a867df55316e8eef4669be79cb61b38cbc66831a256fd2a05268e236cc0c7795bb0b4703220216b95505262fc5ba3bbeb2bd0b282bb45a925cb36a04c2d7ffa7b5d0cf312cd99e8b5bdeae1d9b64c2b0ec63f226756aa39c026d930a9c0906a9eb0e363f720767850500571395bf4a82e2b87a147bb12bae0cb61b3892d28e9be5d5be0d7cdcefb70bdbdf2186d3d2d4f1d4e24268ddb3f81fda836e81be16cdf6b9265b6fb077e118b7477788085ae6ff0f6a7066063bca11010b5c8f659efff862ae69616bffd3166ccf45a00ae278d70dd2ee4e0483543903b3c2a7672661d06016f74969474d3e6e77dbaed16a4ad9d65adc40df0b6637d83bf0a9bcae53debb9ec547b2cf7f30b5ffe9bdbdf21ccabac28a53b3933024b4a3a6bad03605cdd7069354de572923d967bfb3667a2ec4614ab85d681b022a6f2b94b40bbe37c30c8ea15a05df1b2d02ef8d";
#define crc32_c_size ((sizeof(crc32_c) - 1) / sizeof(char))
unsigned char crc32_m[crc32_c_size >> 1];
#define crc32_size sizeof(crc32_m) / sizeof(char)

typedef enum {HELP, CHECK, CREATE} Mode;

static int be2int(unsigned char *c) {
  return c[3] | (c[2] << 8) | (c[1] << 16) | c[0] <<24;
}

static void set_int(void *base, int offset, unsigned int value) {
  const unsigned int swapped = ((value>>24)&0xff) | // move byte 3 to byte 0
                            ((value<<8)&0xff0000) | // move byte 1 to byte 2
                            ((value>>8)&0xff00) | // move byte 2 to byte 1
                            ((value<<24)&0xff000000); // byte 0 to byte 3
  * (unsigned int *) (base + offset) = swapped;
}

static void set_string(void *base, int offset, const char *value) {
  strcpy(base + offset, value);
}

unsigned int calc_crc32(unsigned int sum, const char *filename, int offset) {
  unsigned char buffer[4096];
  int fd = open(filename, O_RDONLY);
  lseek(fd, offset, SEEK_SET); /* skip header */
  int rc;

  while ((rc = read(fd, buffer, sizeof(buffer))) > 0) {
    int i;
    for(i=0; i< rc; i++) {
      sum = be2int(crc32_m + (((buffer[i] ^ sum) & 0xFF) << 2)) ^ sum >> 8;
      //printf("byte %02x, sum %08x\n", buffer[i], sum);
    }
  }
    
  close(fd);
  return sum;
}

char *strip_newline(const char *src) {
  char* dst = strdup(src);
  char *p = dst;
  while (*p != '\0') {
    if (*p == '\n') {
      *p = '\0';
      break;
    }
    p++;
  }
  return dst;
}

char *add_newline(const char *src) {
  const int l = strlen(src);
  char *dst = malloc(l + 2);
  strcpy(dst, src);
  dst[l] = '\n';
  dst[l+1] = '\0';
  return dst;
}

int main(int argc, const char *argv[]) {
  unsigned i;
  Mode mode = HELP;
  int arg_err = -1;
  const char *checkfile = NULL;
  const char *kernelfile = NULL;
  const char *squashfsfile = NULL;
  const char *outputheaderfile = NULL;
  const char *paddingfile = NULL;
  
  for (i = 1; i < argc; i++) {
      if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
        mode = HELP;
        arg_err = 0;
        continue;
      } else if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--check")) {
        mode = CHECK;
        if (i >= argc -1) {
          fprintf(stderr, "File to check (tclinux.bin) not specified\n");
          arg_err = 2;
          break;
        }
        i++;
        checkfile = argv[i];
        if (access(checkfile, R_OK) == -1) {
          fprintf(stderr, "File to check %s does not exist or is not readable\n", checkfile);
          arg_err = 3;
          break;
        }
        arg_err = 0;
        continue;
      } else if (!strcmp(argv[i], "-k") || !strcmp(argv[i], "--kernel")) {
        mode = CREATE;
        if (i >= argc -1) {
          fprintf(stderr, "Kernel file not specified\n");
          arg_err = 2;
          break;
        }
        i++;
        kernelfile = argv[i];
        if (access(kernelfile, R_OK) == -1) {
          fprintf(stderr, "Kernel file %s does not exist or is not readable\n", kernelfile);
          arg_err = 3;
          break;
        }
        arg_err = 0;
        continue;
      } else if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--squashfs")) {
        mode = CREATE;
        if (i >= argc -1) {
          fprintf(stderr, "SquashFS file not specified\n");
          arg_err = 2;
          break;
        }
        i++;
        squashfsfile = argv[i];
        if (access(squashfsfile, R_OK) == -1) {
          fprintf(stderr, "SquashFS file %s does not exist or is not readable\n", squashfsfile);
          arg_err = 3;
          break;
        }
        arg_err = 0;
        continue;
      } else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
        mode = CREATE;
        if (i >= argc -1) {
          fprintf(stderr, "Output header file not specified\n");
          arg_err = 2;
          break;
        }
        i++;
        outputheaderfile = argv[i];
        arg_err = 0;
        continue;
      }  else if (!strcmp(argv[i], "-p") || !strcmp(argv[i], "--padding")) {
        mode = CREATE;
        if (i >= argc -1) {
          fprintf(stderr, "Output padding file not specified\n");
          arg_err = 2;
          break;
        }
        i++;
        paddingfile = argv[i];
        arg_err = 0;
        continue;
      } else if (!strcmp(argv[i], "-m") || !strcmp(argv[i], "--model")) {
        mode = CREATE;
        if (i >= argc -1) {
          fprintf(stderr, "Model (actual version) not specified\n");
          arg_err = 2;
          break;
        }
        i++;
        model = add_newline(argv[i]);
        arg_err = 0;
        continue;
      } else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--version")) {
        mode = CREATE;
        if (i >= argc -1) {
          fprintf(stderr, "Firmware version (probably useless) not specified\n");
          arg_err = 2;
          break;
        }
        i++;
        firmware_version = add_newline(argv[i]);
        arg_err = 0;
        continue;
      } else {
        fprintf(stderr, "Unknown argument %s\n", argv[i]);
        arg_err = 1;
        break;
      }
  }
  
  if (mode == CREATE && !kernelfile) {
    fprintf(stderr, "Kernel file not specified\n");
    arg_err = 2;
  }

  if (mode == CREATE && !squashfsfile) {
    fprintf(stderr, "SquashFS file not specified\n");
    arg_err = 2;
  }

  if (mode == CREATE && !outputheaderfile) {
    fprintf(stderr, "Output header file not specified\n");
    arg_err = 2;
  }

  if (mode == CREATE && !paddingfile) {
    fprintf(stderr, "Output padding file not specified\n");
    arg_err = 2;
  }

  if (mode == HELP || arg_err) {
    fprintf(stderr, "Usage: %s [ {-h|--help} | {-c|--check tclinux.bin} | {-k|--kernel kernel.bin -s|--squashfs squashfs.bin -o|--output header.bin -p|--padding padding.bin [-m|--model 'model]' [-v|--version 'firmware_version']} ]\n", argv[0]);
    return arg_err;
  }

  unsigned int j;
  char cnv[] = {0, 0};
  for (i = 0; i < crc32_size; i++) {
    j = i << 1;
    cnv[0] = crc32_c[j];   
    const int high = (int) strtol(cnv, NULL, 16);
    cnv[0] = crc32_c[j + 1];
    const int low = (int) strtol(cnv, NULL, 16);
    const int val = 16 * high + low;
    //printf("%d %d %d %02x\n", low, high, val, val);
    crc32_m[i] = val;
  } 

  unsigned int sum = 0xFFFFFFFF;

  if (mode == CHECK) {
    sum = calc_crc32(sum, checkfile, header_size);
    
    //printf("Reading header...\n");
    printf("Manual check (binwalk): header size must be %d (0x%04X)\n", header_size, header_size);
    unsigned char header[header_size];
    int fd = open(checkfile, O_RDONLY);
    read(fd, header, header_size);
    const unsigned int tclinux_size = lseek(fd, 0, SEEK_END);
    close(fd);

    const unsigned int found_magic_number = be2int(header + magic_number_offset);
    printf("Magic number: 0x%08X found 0x%08X ...%s\n", magic_number, found_magic_number, magic_number == found_magic_number ? "ok" : "failed");
    const unsigned int found_magic_device = be2int(header + magic_device_offset);
    printf("Magic device: 0x%08X found 0x%08X ...%s\n", magic_device, found_magic_device, magic_device == found_magic_device ? "ok" : "failed");
    const unsigned int found_tclinux_size =  be2int(header + tclinux_size_offset);
    printf("tclinux.bin size: %u found %u ...%s\n", tclinux_size, found_tclinux_size, tclinux_size == found_tclinux_size ? "ok" : "failed");
    const unsigned int found_tclinux_checksum =  be2int(header + tclinux_checksum_offset);
    printf("tclinux.bin chekcsum: 0x%08X found 0x%08X ...%s\n", sum, found_tclinux_checksum, sum == found_tclinux_checksum ? "ok" : "failed");
    printf("Manual check Firmware version: %s found %s. If they differ use -v to adjust.\n", strip_newline(firmware_version), strip_newline((const char *) header + firmware_version_offset));
    const unsigned int found_squashfs_offset =  be2int(header + squashfs_offset_offset);
    printf("Manual check (binwalk): squashfs offset must be at 0x%08X\n", found_squashfs_offset + header_size);
    const unsigned int found_squashfs_size =  be2int(header + squashfs_size_offset);
    printf("Manual check (mtd partition dump): squashfs size (padded to erase_size at 4K (0x1000)) must be at %u (0x%08X)\n", found_squashfs_size, found_squashfs_size);
    printf("Manual check (all tests have been done with model 3) Model: %s found %s. If they differ use -m to adjust.\n", strip_newline(model), strip_newline((const char *) header + model_offset));
  }

  if (mode == CREATE) {
    sum = calc_crc32(sum, kernelfile, 0);
    sum = calc_crc32(sum, squashfsfile, 0);

    int fd = open(kernelfile, O_RDONLY);
    const unsigned int kernelfile_size = lseek(fd, 0, SEEK_END);
    close(fd);

    fd = open(squashfsfile, O_RDONLY);
    const unsigned int squashfsfile_size = lseek(fd, 0, SEEK_END);
    close(fd);

    const unsigned int squashfs_offset = kernelfile_size;
    const unsigned int squashfs_offset_4k_offset = squashfsfile_size % 4096;
    const unsigned int squashfs_padding = squashfs_offset_4k_offset != 0 ? 4096 - squashfs_offset_4k_offset : 0;
    printf("Creating necessary squashfs paddingfile %s %d\n", paddingfile, squashfs_padding);
    unsigned char padding_mem[squashfs_padding];
    memset(padding_mem, 0, squashfs_padding);
    fd = open(paddingfile, O_WRONLY | O_CREAT | O_TRUNC,  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    write(fd, padding_mem, squashfs_padding);
    close(fd);

    sum = calc_crc32(sum, paddingfile, 0);

    const unsigned int squashfs_size = squashfsfile_size + squashfs_padding;
    const unsigned int tclinux_size = header_size + kernelfile_size + squashfs_size;

    unsigned char header[header_size];
    memset(header, 0, header_size);

    printf("Magic number: 0x%08X at 0x%02X\n", magic_number, magic_number_offset);
    set_int(header, magic_number_offset, magic_number);
    printf("Magic device: 0x%08X at 0x%02X\n", magic_device, magic_device_offset);
    set_int(header, magic_device_offset, magic_device);
    printf("tclinux.bin size: %u (0x%08X) at 0x%02X\n", tclinux_size, tclinux_size, tclinux_size_offset);
    set_int(header, tclinux_size_offset, tclinux_size);
    printf("tclinux.bin checksum: 0x%08X at 0x%02X\n", sum, tclinux_checksum_offset);
    set_int(header, tclinux_checksum_offset, sum);
    printf("Firmware version at 0x%02X: %s\n", firmware_version_offset, strip_newline(firmware_version));
    set_string(header, firmware_version_offset, firmware_version);
    set_string(header, 0x30, "\n");
    printf("squashfs offset: %u (0x%08X) at 0x%02X\n", squashfs_offset, squashfs_offset, squashfs_offset_offset);
    set_int(header, squashfs_offset_offset, squashfs_offset);
    printf("squashfs size: %u (0x%08X) at 0x%02X\n", squashfs_size, squashfs_size, squashfs_size_offset);
    set_int(header, squashfs_size_offset, squashfs_size);
    printf("Model at 0x%02X: %s\n", model_offset, strip_newline(model));
    set_string(header, model_offset, model);    

    printf("Writing header to %s. Create image with\n\tcat %s %s %s %s > tclinux.bin\n", outputheaderfile, outputheaderfile, kernelfile, squashfsfile, paddingfile);
    fd = open(outputheaderfile, O_WRONLY | O_CREAT | O_TRUNC,  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    write(fd, header, header_size);
    close(fd);
  }

  return 0;
}
