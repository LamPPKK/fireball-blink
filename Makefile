.PHONY: check test chromium-builder-preflight macos-preview macos-preview-media clean

clean:
	rm -rf out

build: clean macos-preview check

check:
	python3 tools/check_pins.py
	python3 tools/url_cleaner_rules.py check
	python3 tools/fireball_overlay.py validate
	python3 tools/fireball_patches.py validate
	python3 tools/network_policy.py check
	python3 tools/security_rebases.py check
	python3 -m unittest discover -s tests -v
	python3 tools/run_cpp_tests.py
	python3 tools/run_adblock_tests.py
	python3 fireball-beam/test_beam_protocol.py
	swift run --package-path fireball-ios FireballTestRunner

test: check

chromium-builder-preflight:
	python3 tools/chromium_builder.py preflight \
		--workspace "$${RUNNER_TEMP:-/tmp}" \
		--output "$${RUNNER_TEMP:-/tmp}/fireball-builder-preflight.json"

macos-preview:
	./tools/build_macos_preview.sh

macos-preview-media:
	./tools/capture_macos_preview.sh
