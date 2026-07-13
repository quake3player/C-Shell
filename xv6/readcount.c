#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int initial_count = getreadcount();
  printf("Initial read count: %d\n", initial_count);

  // Open a file and read some data to increment the count
  int fd = open("README", 0);
  if (fd < 0) {
    printf("Cannot open README\n");
    exit(1);
  }

  char buf[100];
  int n = read(fd, buf, sizeof(buf));
  if (n < 0) {
    printf("Read failed\n");
    exit(1);
  }

  close(fd);

  int final_count = getreadcount();
  printf("Final read count: %d\n", final_count);
  printf("Bytes read: %d\n", final_count - initial_count);

  exit(0);
}
