# Chromium B0 control build

The B0 lane builds the exact unmodified Chromium revision in
`pins/upstream.json` before any Fireball overlay, override or direct patch is
allowed into the same milestone. A workflow definition is not build evidence:
B0 remains open until a protected runner produces a green run and its immutable
artifact is inspected.

## Trust boundary

`.github/workflows/chromium-control.yml` accepts only `workflow_dispatch` and a
nightly schedule from `refs/heads/main` of `LamPPKK/fireball-blink`. The job uses
the protected `chromium-builder` environment and requires all four runner labels:

- `self-hosted`
- `linux`
- `x64`
- `fireball-chromium`

Do not attach `fireball-chromium` to a runner that executes pull requests or
code from forks. Repository permissions are read-only, checkout credentials are
not persisted, and automatic `depot_tools` updates are disabled. The nightly job
is skipped unless the repository variable
`FIREBALL_CHROMIUM_NIGHTLY_ENABLED=true` is set after provisioning.

## Builder contract

The runner must be a non-root Ubuntu 24.04 x86_64 host with at least 8 logical
CPUs, 32 GiB physical RAM and 300 GiB free under `RUNNER_TEMP`. The repository
chooses a 300 GiB release budget even though Chromium's official documentation
quotes a lower minimum, leaving room for source, generated files, link output
and the Debian artifact.

Provision the host from a reviewed checkout of the same pinned Chromium
revision by running Chromium's official `build/install-build-deps.sh` once.
Keep this privileged provisioning outside GitHub Actions. The workflow itself
runs as the unprivileged runner user and the preflight rejects root, the wrong
OS/architecture, insufficient capacity or missing base commands before cloning
Chromium.

Run the local preflight on the builder with:

```sh
python3 tools/chromium_builder.py preflight \
  --workspace "$RUNNER_TEMP" \
  --output "$RUNNER_TEMP/fireball-builder-preflight.json"
```

## Reproducible control

The workflow pins both Chromium and `depot_tools`, synchronizes dependencies at
the exact Chromium commit, then verifies both checkout origins and `HEAD`
revisions. `build-config/chromium-control.gn` is an exact allowlist:

- unbranded upstream Chromium, not Fireball;
- release-mode, non-component, generic x86_64;
- PGO and ThinLTO disabled so later optimization experiments have a control;
- no remote execution or Siso dependency;
- Chromium's default PartitionAlloc, process model and sandbox remain intact.

The build target is `chrome/installer/linux:stable_deb`. The smoke test loads a
local `data:` document with the sandbox enabled; `--no-sandbox` is forbidden by
the repository test. The uploaded tar contains the `.deb`, checkout report,
preflight report, smoke output and `manifest.json`. A sibling SHA-256 file
protects the exact tar. Artifacts are retained for 14 days and the large source
tree is removed from the protected runner even when the job fails.

## Gate evidence

B0 can claim an upstream control artifact only after all of the following exist:

1. A green `chromium-control` run URL from the protected main branch.
2. The uploaded tar and matching sibling SHA-256.
3. `manifest.json` with the pinned revisions, exact GN-args digest, package
   digest, passing preflight and passing sandboxed smoke test.
4. Manual inspection that the run did not execute a Fireball overlay or patch.

Current repository status: **workflow implemented; protected builder run not yet
recorded; no B0 control artifact claimed**.
