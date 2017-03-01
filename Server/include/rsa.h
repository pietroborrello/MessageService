#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ACCURACY 5
#define SINGLE_MAX 10000
#define EXPONENT_MAX 1000
#define BUF_SIZE 1024

char *decodeMessage(int len, int bytes, int *cryptogram, int exponent, int modulus);
int *encodeMessage(int len, int bytes, char *message, int exponent, int modulus);
