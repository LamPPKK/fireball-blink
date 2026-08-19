#ifndef FIREBALL_COMPONENTS_ADBLOCK_INCLUDE_FIREBALL_ADBLOCK_FFI_H_
#define FIREBALL_COMPONENTS_ADBLOCK_INCLUDE_FIREBALL_ADBLOCK_FFI_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FireballAdblockEngine FireballAdblockEngine;

typedef bool (*FireballDomainResolver)(const uint8_t* hostname_data,
                                       size_t hostname_length,
                                       size_t* domain_start,
                                       size_t* domain_end);

enum {
  FIREBALL_ADBLOCK_STATUS_OK = 0,
  FIREBALL_ADBLOCK_STATUS_INVALID_INPUT = 1,
  FIREBALL_ADBLOCK_STATUS_INTERNAL_ERROR = 2,
};

enum {
  FIREBALL_ADBLOCK_FLAG_BLOCK = 1u << 0,
  FIREBALL_ADBLOCK_FLAG_EXCEPTION = 1u << 1,
  FIREBALL_ADBLOCK_FLAG_IMPORTANT = 1u << 2,
  FIREBALL_ADBLOCK_FLAG_REDIRECT = 1u << 3,
  FIREBALL_ADBLOCK_FLAG_REWRITE = 1u << 4,
};

typedef struct FireballAdblockDecision {
  int32_t status;
  uint32_t flags;
  char* redirect;
  char* rewritten_url;
} FireballAdblockDecision;

// Must be registered once before creating an engine. Chromium's registry-
// controlled-domain service supplies exact eTLD+1 byte offsets. A second
// registration attempt or NULL callback fails closed.
bool fireball_adblock_set_domain_resolver(FireballDomainResolver resolver);

// The public key is the release-embedded 32-byte Ed25519 key. A malformed,
// unsigned, incompatible or checksum-mismatched manifest returns NULL.
FireballAdblockEngine* fireball_adblock_engine_create_verified(
    const uint8_t* rules_data,
    size_t rules_length,
    const uint8_t* manifest_data,
    size_t manifest_length,
    const uint8_t* public_key_data,
    size_t public_key_length,
    const uint8_t* current_app_version_data,
    size_t current_app_version_length);

void fireball_adblock_engine_destroy(FireballAdblockEngine* engine);

// Chromium supplies already-parsed hostnames. The engine is single-sequence;
// callers must not invoke a handle concurrently or from another sequence.
FireballAdblockDecision fireball_adblock_check_network(
    const FireballAdblockEngine* engine,
    const uint8_t* url_data,
    size_t url_length,
    const uint8_t* hostname_data,
    size_t hostname_length,
    const uint8_t* source_hostname_data,
    size_t source_hostname_length,
    const uint8_t* request_type_data,
    size_t request_type_length,
    const uint8_t* method_data,
    size_t method_length,
    bool third_party);

// Returned strings are UTF-8 JSON and must be released exactly once with
// fireball_adblock_string_destroy. NULL indicates invalid input/internal error.
char* fireball_adblock_cosmetic_resources(
    const FireballAdblockEngine* engine,
    const uint8_t* url_data,
    size_t url_length);

char* fireball_adblock_hidden_selectors(
    const FireballAdblockEngine* engine,
    const uint8_t* classes_json_data,
    size_t classes_json_length,
    const uint8_t* ids_json_data,
    size_t ids_json_length,
    const uint8_t* exceptions_json_data,
    size_t exceptions_json_length);

void fireball_adblock_string_destroy(char* value);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // FIREBALL_COMPONENTS_ADBLOCK_INCLUDE_FIREBALL_ADBLOCK_FFI_H_
