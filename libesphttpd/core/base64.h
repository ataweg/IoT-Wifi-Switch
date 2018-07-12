// --------------------------------------------------------------------------
//
// Project       IoT - Internet of Things
//
// File         base64.h
//
// Author        Axel Werner
//
// --------------------------------------------------------------------------
// Changelog
//
//    2018-05-04  AWe   map function names to avoid conflict with libssl.a( ssl_crypto_misc.o )
//
// --------------------------------------------------------------------------

#ifndef __BASE64_H__
#define __BASE64_H__

#define base64_decode      my_base64_decode
#define base64_encode      my_base64_encode

int ICACHE_FLASH_ATTR base64_decode( size_t in_len, const char *in, size_t out_len, unsigned char *out );
int ICACHE_FLASH_ATTR base64_encode( size_t in_len, const unsigned char *in, size_t out_len, char *out );

#endif // __BASE64_H__