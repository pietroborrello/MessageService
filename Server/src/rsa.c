#include "rsa.h"

/**
 * Computes a^b mod c
 */
int modpow(long long a, long long b, int c) {
  int res = 1;
  while (b > 0) {
    /* Need long multiplication else this will overflow... */
    if (b & 1) {
      res = (res * a) % c;
    }
    b = b >> 1;
    a = (a * a) % c; /* Same deal here */
  }
  return res;
}

/**
 * Computes the Jacobi symbol, (a, n)
 */
int jacobi(int a, int n) {
  int twos, temp;
  int mult = 1;
  while (a > 1 && a != n) {
    a = a % n;
    if (a <= 1 || a == n)
      break;
    twos = 0;
    while (a % 2 == 0 && ++twos)
      a /= 2; /* Factor out multiples of 2 */
    if (twos > 0 && twos % 2 == 1)
      mult *= (n % 8 == 1 || n % 8 == 7) * 2 - 1;
    if (a <= 1 || a == n)
      break;
    if (n % 4 != 1 && a % 4 != 1)
      mult *= -1; /* Coefficient for flipping */
    temp = a;
    a = n;
    n = temp;
  }
  if (a == 0)
    return 0;
  else if (a == 1)
    return mult;
  else
    return 0; /* a == n => gcd(a, n) != 1 */
}

/**
 * Check whether a is a Euler witness for n
 */
int solovayPrime(int a, int n) {
  int x = jacobi(a, n);
  if (x == -1)
    x = n - 1;
  return x != 0 && modpow(a, (n - 1) / 2, n) == x;
}

/**
 * Test if n is probably prime, using accuracy of k (k solovay tests)
 */
int probablePrime(int n, int k) {
  if (n == 2)
    return 1;
  else if (n % 2 == 0 || n == 1)
    return 0;
  while (k-- > 0) {
    if (!solovayPrime(rand() % (n - 2) + 2, n))
      return 0;
  }
  return 1;
}

/**
 * Find a random (probable) prime between 3 and n - 1, this distribution is
 * nowhere near uniform, see prime gaps
 */
int randPrime(int n) {
  int prime = rand() % n;
  n += n % 2; /* n needs to be even so modulo wrapping preserves oddness */
  prime += 1 - prime % 2;
  while (1) {
    if (probablePrime(prime, ACCURACY))
      return prime;
    prime = (prime + 2) % n;
  }
}

/**
 * Compute gcd(a, b)
 */
int gcd(int a, int b) {
  int temp;
  while (b != 0) {
    temp = b;
    b = a % b;
    a = temp;
  }
  return a;
}

/**
 * Find a random exponent x between 3 and n - 1 such that gcd(x, phi) = 1,
 * this distribution is similarly nowhere near uniform
 */
int randExponent(int phi, int n) {
  int e = rand() % n;
  while (1) {
    if (gcd(e, phi) == 1)
      return e;
    e = (e + 1) % n;
    if (e <= 2)
      e = 3;
  }
}

/**
 * Compute n^-1 mod m by extended euclidian method
 */
int inverse(int n, int modulus) {
  int a = n, b = modulus;
  int x = 0, y = 1, x0 = 1, y0 = 0, q, temp;
  while (b != 0) {
    q = a / b;
    temp = a % b;
    a = b;
    b = temp;
    temp = x;
    x = x0 - q * x;
    x0 = temp;
    temp = y;
    y = y0 - q * y;
    y0 = temp;
  }
  if (x0 < 0)
    x0 += modulus;
  return x0;
}

/**
 * Encode the message m using public exponent and modulus, c = m^e mod n
 */
int encode(int m, int e, int n) { return modpow(m, e, n); }

/**
 * Decode cryptogram c using private exponent and public modulus, m = c^d mod n
 */
int decode(int c, int d, int n) { return modpow(c, d, n); }

/**
 * Encode the message of given length, using the public key (exponent, modulus)
 * The resulting array will be of size len/bytes, each index being the
 * encryption
 * of "bytes" consecutive characters, given by m = (m1 + m2*128 + m3*128^2 +
 * ..),
 * encoded = m^exponent mod modulus
 */
int *encodeMessage(int len, int bytes, char *message, int exponent,
                   int modulus) {
  int *encoded = malloc((len / bytes) * sizeof(int));
  int x, i, j;
  for (i = 0; i < len; i += bytes) {
    x = 0;
    for (j = 0; j < bytes; j++)
      x += message[i + j] * (1 << (7 * j));
    encoded[i / bytes] = encode(x, exponent, modulus);
#ifndef MEASURE
    printf("%d ", encoded[i / bytes]);
#endif
  }
  return encoded;
}

/**
 * Decode the cryptogram of given length, using the private key (exponent,
 * modulus)
 * Each encrypted packet should represent "bytes" characters as per
 * encodeMessage.
 * The returned message will be of size len * bytes.
 */
char *decodeMessage(int len, int bytes, int *cryptogram, int exponent,
                    int modulus) {
  char *decoded = malloc(len * bytes * sizeof(char));
  int x, i, j;
  for (i = 0; i < len; i++) {
    x = decode(cryptogram[i], exponent, modulus);
    for (j = 0; j < bytes; j++) {
      decoded[i * bytes + j] = (x >> (7 * j)) % 128;
    }
  }
  return decoded;
}
