#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

struct initrd_header
{
   unsigned char magic; // The magic number is there to check for consistency.
   char name[64];
   unsigned long offset; // Offset in the initrd the file starts.
   unsigned int length; // Length of the file.
};

int main(char argc, char **argv) {
   int nheaders = (argc-1);
   struct initrd_header headers[64];
   printf("size of header: %lu\n", sizeof(struct initrd_header));
   unsigned int off = sizeof(struct initrd_header) * 64 + sizeof(int);
   int i;
   for(i = 0; i < nheaders; i++) {
       printf("writing file %s->%s at 0x%x\n", argv[i+1], basename(argv[i+1]), off);
       strcpy(headers[i].name, basename(argv[i+1]));
       headers[i].offset = off;
       FILE *stream = fopen(argv[i+1], "r");
       if(stream == 0)
       {
         printf("Error: file not found: %s\n", argv[i+1]);
         return 1;
       }
       fseek(stream, 0, SEEK_END);
       headers[i].length = ftell(stream);
       off += headers[i].length;
       fclose(stream);
       headers[i].magic = 0xBF;
   }
   
   FILE *wstream = fopen("./initrd.img", "w");
   unsigned char *data = (unsigned char *)malloc(off);
   fwrite(&nheaders, sizeof(int), 1, wstream);
   fwrite(headers, sizeof(struct initrd_header), 64, wstream);
   
   for(i = 0; i < nheaders; i++)
   {
     FILE *stream = fopen(argv[i+1], "r");
     unsigned char *buf = (unsigned char *)malloc(headers[i].length);
     fread(buf, 1, headers[i].length, stream);
     fwrite(buf, 1, headers[i].length, wstream);
     fclose(stream);
     free(buf);
   }
   
   fclose(wstream);
   free(data);
   
   return 0;
}