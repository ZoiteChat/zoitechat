/*
  Copyright (c) 2020 <bakasura@protonmail.ch>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.

*/

#include <string.h>
#include <glib.h>

#include "fish.h"
#include "utils.h"

/**
 * Auxiliary function: Generate a random string
 * @param out Preallocated string to fill
 * @param len Size of bytes to fill
 */
static void
random_string(char *out, size_t len)
{
    GRand *rand = NULL;
    size_t i = 0;

    rand = g_rand_new();
    for (i = 0; i < len; ++i) {
        out[i] = g_rand_int_range(rand, 1, 256);
    }

    out[len] = 0;

    g_rand_free(rand);
}

/**
 * Check encrypt and decrypt in ECB mode
 */
static void
test_ecb(void)
{
    char *b64 = NULL;
    char *de = NULL;
    int key_len, message_len = 0;
    char key[57];
    char message[1000];

    /* Generate key 32–448 bits (Yes, I start with 8 bits) */
    for (key_len = 1; key_len < 57; ++key_len) {

        random_string(key, key_len);

        for (message_len = 1; message_len < 1000; ++message_len) {
            random_string(message, message_len);

            /* Encrypt */
            b64 = fish_encrypt(key, key_len, message, message_len, FISH_ECB_MODE);
            g_assert_nonnull(b64);

            /* Decrypt */
            /* Linear */
            de = fish_decrypt_str(key, key_len, b64, FISH_ECB_MODE);
			g_assert_cmpstr (de, ==, message);
            g_free(de);
	
            /* Mixed */
            de = fish_decrypt_str(key, key_len, b64, FISH_ECB_MODE);
			g_assert_cmpstr (de, ==, message);
            g_free(de);

            g_free(b64);
        }
    }
}

/**
 * Check encrypt and decrypt in CBC mode
 */
static void
test_cbc(void)
{
    char *b64 = NULL;
    char *de = NULL;
    int key_len, message_len = 0;
    char key[57];
    char message[1000];

    /* Generate key 32–448 bits (Yes, I start with 8 bits) */
    for (key_len = 1; key_len < 57; ++key_len) {

        random_string(key, key_len);

        for (message_len = 1; message_len < 1000; ++message_len) {
            random_string(message, message_len);

            /* Encrypt */
            b64 = fish_encrypt(key, key_len, message, message_len, FISH_CBC_MODE);
            g_assert_nonnull(b64);

            /* Decrypt */
            /* Linear */
            de = fish_decrypt_str(key, key_len, b64, FISH_CBC_MODE);
            g_assert_cmpstr (de, ==, message);
            g_free(de);

            g_free(b64);
        }
    }
}

/**
 * Check the calculation of final length from an encoded string in Base64
 */
static void
test_base64_len (void)
{
    char *b64 = NULL;
    char message[1000];
    int message_end = sizeof (message) - 1;

    random_string(message, message_end);

    for (; message_end >= 0; --message_end) {
        message[message_end] = '\0'; /* Truncate instead of generating new strings */
        b64 = g_base64_encode((const unsigned char *) message, message_end);
        g_assert_nonnull(b64);
        g_assert_cmpuint(strlen(b64), == , base64_len(message_end));
        g_free(b64);
    }
}

/**
 * Check the calculation of final length from an encoded string in BlowcryptBase64
 */
static void
test_base64_fish_len (void)
{
    char *b64 = NULL;
    int message_len = 0;
    char message[1000];

    for (message_len = 1; message_len < 1000; ++message_len) {
        random_string(message, message_len);
        b64 = fish_base64_encode(message, message_len);
        g_assert_nonnull(b64);
        g_assert_cmpuint(strlen(b64), == , base64_fish_len(message_len));
        g_free(b64);
    }
}

/**
 * Check the calculation of final length from an encrypted string in ECB mode
 */
static void
test_base64_ecb_len(void)
{
    char *b64 = NULL;
    int key_len, message_len = 0;
    char key[57];
    char message[1000];

    /* Generate key 32–448 bits (Yes, I start with 8 bits) */
    for (key_len = 1; key_len < 57; ++key_len) {

        random_string(key, key_len);

        for (message_len = 1; message_len < 1000; ++message_len) {
            random_string(message, message_len);
            b64 = fish_encrypt(key, key_len, message, message_len, FISH_ECB_MODE);
            g_assert_nonnull(b64);
            g_assert_cmpuint(strlen(b64), == , ecb_len(message_len));
            g_free(b64);
        }
    }
}

/**
 * Check the calculation of final length from an encrypted string in CBC mode
 */
static void
test_base64_cbc_len(void)
{
    char *b64 = NULL;
    int key_len, message_len = 0;
    char key[57];
    char message[1000];

    /* Generate key 32–448 bits (Yes, I start with 8 bits) */
    for (key_len = 1; key_len < 57; ++key_len) {

        random_string(key, key_len);

        for (message_len = 1; message_len < 1000; ++message_len) {
            random_string(message, message_len);
            b64 = fish_encrypt(key, key_len, message, message_len, FISH_CBC_MODE);
            g_assert_nonnull(b64);
            g_assert_cmpuint(strlen(b64), == , cbc_len(message_len));
            g_free(b64);
        }
    }
}

/**
 * Check the calculation of length limit for a plaintext in each encryption mode
 */
static void
test_max_text_command_len(void)
{
    int max_encoded_len, plaintext_len;
    enum fish_mode mode;

    for (max_encoded_len = 0; max_encoded_len < 10000; ++max_encoded_len) {
        for (mode = FISH_ECB_MODE; mode <= FISH_CBC_MODE; ++mode) {
            plaintext_len = max_text_command_len(max_encoded_len, mode);
            g_assert_cmpuint(encoded_len(plaintext_len, mode), <= , max_encoded_len);
        }
    }
}

/**
 * Check the calculation of length limit for a plaintext in each encryption mode
 */
static void
test_foreach_utf8_data_chunks(void)
{
    GRand *rand = NULL;
    GString *chunks = NULL;
    int  max_chunks_len, chunks_len;
    char ascii_message[1001];
    char *data_chunk = NULL;

    rand = g_rand_new();
    max_chunks_len = g_rand_int_range(rand, 2, 301);
    random_string(ascii_message, 1000);

    data_chunk = ascii_message;

    chunks = g_string_new(NULL);

    while (foreach_utf8_data_chunks(data_chunk, max_chunks_len, &chunks_len)) {
        g_string_append(chunks, g_strndup(data_chunk, chunks_len));
        /* Next chunk */
        data_chunk += chunks_len;
    }
    /* Check data loss */
    g_assert_cmpstr(chunks->str, == , ascii_message);

    g_string_free(chunks, TRUE);
    g_rand_free (rand);
}

/**
 * ECB and CBC must produce different ciphertext for the same key and plaintext.
 * This catches accidental mode aliasing (e.g. both paths calling the same cipher).
 */
static void
test_ecb_cbc_differ(void)
{
    const char key[] = "testkey123";
    const char message[] = "Hello FiSH!!!!!!!"; /* 17 bytes, > one Blowfish block */
    size_t keylen = sizeof(key) - 1;
    size_t message_len = sizeof(message) - 1;
    char *ecb_b64 = fish_encrypt(key, keylen, message, message_len, FISH_ECB_MODE);
    char *cbc_b64 = fish_encrypt(key, keylen, message, message_len, FISH_CBC_MODE);

    g_assert_nonnull(ecb_b64);
    g_assert_nonnull(cbc_b64);
    g_assert_cmpstr(ecb_b64, !=, cbc_b64);

    g_free(ecb_b64);
    g_free(cbc_b64);
}

/**
 * CBC wire convention: the caller prepends '*' before sending and strips it
 * before decrypting (as fish_decrypt_from_nick does via ++data).
 * Verify the round-trip survives that strip.
 */
static void
test_cbc_wire_prefix(void)
{
    const char key[] = "wirekey";
    const char message[] = "test wire prefix round-trip";
    size_t keylen = sizeof(key) - 1;
    size_t message_len = sizeof(message) - 1;
    char *b64 = fish_encrypt(key, keylen, message, message_len, FISH_CBC_MODE);
    g_assert_nonnull(b64);

    /* Simulate prepending '*' then stripping it before decrypt */
    char *wire = g_strdup_printf("*%s", b64);
    const char *stripped = wire + 1; /* same as ++data in fish_decrypt_from_nick */
    char *decrypted = fish_decrypt_str(key, keylen, stripped, FISH_CBC_MODE);

    g_assert_cmpstr(decrypted, ==, message);

    g_free(decrypted);
    g_free(wire);
    g_free(b64);
}

/**
 * Two CBC encryptions of the same plaintext must produce different ciphertext
 * because of the random IV, but both must decrypt back to the original.
 */
static void
test_cbc_different_ivs(void)
{
    const char key[] = "ivtestkey";
    const char message[] = "same plaintext, different IVs";
    size_t keylen = sizeof(key) - 1;
    size_t message_len = sizeof(message) - 1;
    char *b64_a = fish_encrypt(key, keylen, message, message_len, FISH_CBC_MODE);
    char *b64_b = fish_encrypt(key, keylen, message, message_len, FISH_CBC_MODE);

    g_assert_nonnull(b64_a);
    g_assert_nonnull(b64_b);
    g_assert_cmpstr(b64_a, !=, b64_b);

    char *dec_a = fish_decrypt_str(key, keylen, b64_a, FISH_CBC_MODE);
    char *dec_b = fish_decrypt_str(key, keylen, b64_b, FISH_CBC_MODE);
    g_assert_cmpstr(dec_a, ==, message);
    g_assert_cmpstr(dec_b, ==, message);

    g_free(dec_b);
    g_free(dec_a);
    g_free(b64_b);
    g_free(b64_a);
}

int
main(int argc, char *argv[]) {

    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/fishlim/ecb", test_ecb);
    g_test_add_func("/fishlim/cbc", test_cbc);
    g_test_add_func("/fishlim/base64_len", test_base64_len);
    g_test_add_func("/fishlim/base64_fish_len", test_base64_fish_len);
    g_test_add_func("/fishlim/base64_ecb_len", test_base64_ecb_len);
    g_test_add_func("/fishlim/base64_cbc_len", test_base64_cbc_len);
    g_test_add_func("/fishlim/max_text_command_len", test_max_text_command_len);
    g_test_add_func("/fishlim/foreach_utf8_data_chunks", test_foreach_utf8_data_chunks);
    g_test_add_func("/fishlim/ecb_cbc_differ", test_ecb_cbc_differ);
    g_test_add_func("/fishlim/cbc_wire_prefix", test_cbc_wire_prefix);
    g_test_add_func("/fishlim/cbc_different_ivs", test_cbc_different_ivs);

    fish_init();
    int ret = g_test_run();
    fish_deinit();
    return ret;
}
