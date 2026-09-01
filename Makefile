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
	python3 fireball-server/test_beam_protocol.py
	python3 fireball-server/sync_relay.py
	python3 fireball-server/media_remuxer.py
	swift run --package-path fireball-mini/ios FireballTestRunner
	node fireball-extension/test/extension_test.js
	mkdir -p out && $(CXX) -std=c++20 -Wall -Wextra -Werror -I./fireball-mini/windows/include fireball-mini/windows/src/domain_models.cpp fireball-mini/windows/src/search_engines.cpp fireball-mini/windows/src/shields_engine.cpp fireball-mini/windows/src/password_vault.cpp fireball-mini/windows/src/sync_engine.cpp fireball-mini/windows/src/beam_client.cpp fireball-mini/windows/src/webview2_host.cpp fireball-mini/windows/src/app_window.cpp fireball-mini/windows/tests/test_runner.cpp -o out/fireball_win_tests && ./out/fireball_win_tests
	python3 tools/package_ecosystem.py


test: check

package:
	python3 tools/package_ecosystem.py


chromium-builder-preflight:
	python3 tools/chromium_builder.py preflight \
		--workspace "$${RUNNER_TEMP:-/tmp}" \
		--output "$${RUNNER_TEMP:-/tmp}/fireball-builder-preflight.json"

macos-preview:
	./tools/build_macos_preview.sh

macos-preview-media:
	./tools/capture_macos_preview.sh
