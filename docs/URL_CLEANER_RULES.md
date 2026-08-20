# URL Cleaner data contract

Fireball's built-in URL Cleaner rules are generated from the versioned,
first-party manifest at
`rules/url-cleaner/rules-v1.json`. The checked-in manifest is the authority;
`fireball/components/navigation/generated_url_cleaner_rules.h` is a derived
build input and embeds the SHA-256 of the exact manifest bytes.

The v1 contract is intentionally narrow:

- it lists exact, lower-case query-parameter names only;
- parameters must be unique and bytewise sorted;
- automatic Brave, Helium or third-party imports are forbidden;
- the runtime removes names case-insensitively while preserving raw values,
  remaining order and fragments;
- policy remains Profile-scoped and supports an exact-host site exemption;
- malformed URLs and hostname mismatch fail closed.

The companion `false-positive-corpus-v1.json` pins expected runtime behavior
for exact-name matching, encoded names, duplicate parameters, raw values,
fragments, IPv6, disabled Profiles, site exemptions and malformed input. CI
verifies that `tests/generated/url_cleaner_corpus.h` exactly matches that file,
then executes the cases against the same C++ implementation used by the
overlay.

Regenerate both deterministic headers after an intentional data change:

```sh
python3 tools/url_cleaner_rules.py generate
python3 tools/url_cleaner_rules.py check
```

The JSON Schemas are the closed structural contract. JSON Schema cannot express
bytewise array order, uniqueness by one object property, or equality between
two sibling fields, so each schema lists those requirements under
`x-fireball-executable-invariants`. `tools/url_cleaner_rules.py` is the
normative executable validator for those invariants. It also recomputes both
source checksums, reads only bounded regular non-symlink inputs, rejects
input/output path aliasing, stages both outputs before publishing either, and
rejects a missing or stale generated header. `make check` runs it before
validating the checksum-pinned Chromium overlay.

This is an embedded baseline, not a remote update channel. A future downloaded
rules artifact must define rollback, minimum-version and signature verification
before it can replace this table. The existing signed adblock artifact contract
does not implicitly authorize URL Cleaner updates.
