.PHONY: check test

check:
	python3 tools/check_pins.py
	python3 tools/fireball_patches.py validate
	python3 tools/network_policy.py check
	python3 tools/security_rebases.py check
	python3 -m unittest discover -s tests -v
	python3 tools/run_cpp_tests.py

test: check
