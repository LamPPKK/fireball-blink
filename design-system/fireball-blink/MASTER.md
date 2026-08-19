# Fireball Blink design system

**Direction:** Orbital command deck
**Dials:** variance 8/10 · motion 5/10 · density 8/10

This system adapts UI/UX Pro Max desktop-browser guidance to the AppKit preview
and future Chromium overlay. Landing-page patterns do not apply to browser
chrome.

## Brand

- Use `Brand/FireballMeteorMark.png` as the canonical raster mark.
- The detached meteor core represents a Profile storage boundary; the separate
  trails represent Spaces that can share a Profile without becoming one.
- Keep at least 20% of the mark width as clear space. Never add a containing
  circle, recolor the mark, or use the mark as a security-status indicator.

## Tokens

| Role | Value |
| --- | --- |
| Background | `#060806` |
| Deep surface | `#0B0F0C` |
| Raised surface | `#121812` |
| Active surface | `#1A241A` |
| Primary text | `#F4F1E8` |
| Secondary text | `#A8B0A6` |
| Muted text | `#727C72` |
| Meteor orange | `#FF5A1F` |
| Electric lime | `#B8FF3D` |
| Border | `#283128` |
| Focus | `#F4F1E8` |
| Destructive | `#FF5A4F` |

- Display: Avenir Next Condensed Demi Bold, then system condensed bold.
- Body: Avenir Next Medium, then system UI.
- Technical labels: Menlo Bold, then monospaced system.
- Use an 8pt rhythm. Desktop controls must expose a visible focus state and a
  minimum 28px pointer target; primary browser actions target 36–44px.

## Composition

- Use an asymmetric two-column shell: compact identity/Space rail plus a large
  browser stage.
- Browser content stays visually dominant. Diagnostics and reference-browser
  provenance are secondary, never a fake rendered webpage.
- Use solid surfaces, one-pixel rules and restrained radii (8–16px). Avoid
  glass cards, excessive neon glow, pill-shaped containers everywhere and
  decorative status data.
- Orange is brand/trajectory; lime is active/verified state. Neither color alone
  may communicate meaning—pair it with text or shape.

## Motion and accessibility

- Layout switching changes presentation only and must preserve the underlying
  tab model and active tab.
- Use 160–240ms opacity/transform transitions in the real Chromium shell; the
  deterministic preview renders the final state without animation.
- Respect reduced motion, full keyboard navigation, visible focus, contrast of
  at least 4.5:1 for normal text and 3:1 for control boundaries.
- Never claim Chromium, WebContents, sandbox, Shields or security-rebase
  behavior from the AppKit model preview.
