# Browser overlay

Fireball browser UI and product services live here. Prefer a GN dependency or a `chromium_src` override before adding a direct Chromium patch. The overlay must not replace Chromium's sandbox, process model or PartitionAlloc.

`domain_model` defines stable UUID-backed Profile, Space and Tab identities without owning Chromium runtime objects. A Profile is the storage boundary; multiple regular Spaces may reference one persistent Profile. A Burner Space is accepted only when it references an off-the-record Profile and is never restorable. Tab layout is presentation state and changing it does not recreate domain tabs.
