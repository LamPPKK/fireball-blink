.PHONY: check test macos-preview macos-preview-media

check:
	python3 tools/check_pins.py
	python3 tools/fireball_patches.py validate
	python3 tools/network_policy.py check
	python3 tools/security_rebases.py check
	python3 -m unittest discover -s tests -v
	python3 tools/run_cpp_tests.py

test: check

macos-preview:
	./tools/build_macos_preview.sh

macos-preview-media:
	./tools/capture_macos_preview.sh
