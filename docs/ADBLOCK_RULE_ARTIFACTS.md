# Signed adblock rule artifacts

Fireball builds the network and cosmetic rule database outside the browser, then
loads it only after the Rust boundary verifies its size, SHA-256 digest, Ed25519
signature, provenance and minimum application version. The builder never fetches
the network and never accepts a moving branch: another trusted release job must
check out each source at an exact commit and create a source lock.

## Source lock

`schemas/adblock-source-lock-v1.schema.json` defines a closed input with, for
each source:

- an ASCII name and license identifier;
- an HTTPS repository URL and exact lowercase 40-character commit;
- a relative, non-symlink path below the lock file; and
- the SHA-256 digest of the source bytes before normalization.

The production release is expected to pin EasyList and EasyPrivacy commits and
retain their GPL-3.0-or-later attribution. The files under
`tests/fixtures/adblock` are intentionally small synthetic fixtures, not a
shippable filter database.

## Build and verify

Use an unencrypted Ed25519 PEM key held by the protected release environment.
The key file must be a regular non-symlink file with no group or world access.
It must never be committed, placed in an artifact, accepted from a pull request,
or printed in a build log.

```sh
python3 tools/adblock_rules.py build \
  --source-lock release/adblock/source-lock.json \
  --signing-key /secure/offline/fireball-adblock-ed25519.pem \
  --artifact-url https://updates.fireball.example/adblock/rules-v1.txt \
  --minimum-app-version 0.1.0 \
  --created-at 2026-08-20T00:00:00Z \
  --output-rules out/adblock/rules.txt \
  --output-manifest out/adblock/manifest.json \
  --output-public-key out/adblock/public-key.bin
```

All output paths must be new and have existing parent directories. The compiler
validates every source digest before normalizing UTF-8 and line endings, rejects
unresolved preprocessor directives, sorts sources by name and self-verifies the
new artifact before publishing any output. Identical sources, key, timestamp and
arguments produce byte-identical outputs.

Consumers or release promotion can independently verify the three files:

```sh
python3 tools/adblock_rules.py verify \
  --rules out/adblock/rules.txt \
  --manifest out/adblock/manifest.json \
  --public-key out/adblock/public-key.bin
```

The raw 32-byte public key is a build input to Fireball; it is not downloaded
from the same channel as the rules. Production promotion must compare its digest
to the reviewed embedded key before upload.

## Signed contract

Manifest schema v1 uses signing context
`fireball-adblock-rules-v1-signature-v2`. The signed message covers the schema,
creation time, minimum app version, engine name/version, artifact URL/size/hash,
signature algorithm/key ID, source count and every sorted source name, URL,
commit and license. Changing any of those fields without the release key makes
Rust engine creation fail closed.

`tests/test_adblock_rules.py` proves deterministic output, checksum pinning,
private-key permissions, no overwrite and rules/provenance tamper rejection.
`tools/run_adblock_tests.py` then loads the Python compiler's output through the
real Rust C ABI, which prevents the two implementations from silently drifting.

This pipeline does not yet publish a production ruleset or key. Those remain a
release operation after the Chromium B0 control build and license review.
