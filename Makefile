.PHONY: check test

check:
	python3 tools/check_pins.py
	python3 tools/fireball_patches.py validate
	python3 -m unittest discover -s tests -v

test: check
