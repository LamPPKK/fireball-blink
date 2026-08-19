# Browser shell overrides

These rules override `../MASTER.md` for the four-layout model preview.

- A 78px top identity rail contains the meteor mark, product name, selected
  layout and explicit `MODEL PREVIEW` boundary.
- The left rail owns Profiles, Spaces, Burner state and startup-network policy.
- The address row owns navigation, location, Shields state and build boundary.
- Classic, Floating, Vertical and Grid are views over the same tab collection;
  switching them must not recreate or reload a tab.
- Grid cards use index, active/background text and URL; the active card needs a
  border plus an `ACTIVE` label, not color alone.
- The UI must continue to say `NO CHROMIUM ENGINE` and `PREVIEW · NOT A BROWSER
  BUILD` until a real pinned Chromium artifact exists.
