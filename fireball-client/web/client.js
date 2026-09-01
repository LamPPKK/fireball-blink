/**
 * Fireball Web Client
 * High-performance HTML5 Canvas streaming renderer & input dispatcher.
 */

class FireballWebClient {
  constructor() {
    this.canvas = document.getElementById('beam-canvas');
    this.ctx = this.canvas ? this.canvas.getContext('2d') : null;
    this.badge = document.getElementById('connection-badge');
    this.fpsEl = document.getElementById('fps-counter');
    this.latencyEl = document.getElementById('latency-counter');
    this.omnibox = document.getElementById('omnibox-input');
    this.btnNav = document.getElementById('btn-navigate');
    this.pairingOverlay = document.getElementById('pairing-overlay');

    this.serverUrl = 'http://localhost:9090';
    this.isConnected = false;
    this.pollInterval = null;
    this.frameCount = 0;
    this.lastFpsUpdate = Date.now();

    this.initEvents();
  }

  initEvents() {
    document.getElementById('btn-connect')?.addEventListener('click', () => {
      const urlInput = document.getElementById('server-address');
      if (urlInput && urlInput.value) {
        this.serverUrl = urlInput.value.trim().replace(/\/$/, '');
      }
      this.connect();
    });

    document.getElementById('btn-pair')?.addEventListener('click', () => {
      if (this.pairingOverlay) {
        this.pairingOverlay.style.display = 'flex';
      }
    });

    this.btnNav?.addEventListener('click', () => {
      this.navigate(this.omnibox.value);
    });

    this.omnibox?.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') {
        this.navigate(this.omnibox.value);
      }
    });

    if (this.canvas) {
      this.canvas.addEventListener('mousedown', (e) => this.handleMouseEvent(e, 'mousePressed'));
      this.canvas.addEventListener('mouseup', (e) => this.handleMouseEvent(e, 'mouseReleased'));
      this.canvas.addEventListener('mousemove', (e) => this.handleMouseEvent(e, 'mouseMoved'));
    }
  }

  connect() {
    this.isConnected = true;
    if (this.pairingOverlay) this.pairingOverlay.style.display = 'none';
    if (this.badge) {
      this.badge.textContent = 'CONNECTED';
      this.badge.className = 'badge connected';
    }
    if (this.omnibox) this.omnibox.disabled = false;
    if (this.btnNav) this.btnNav.disabled = false;

    this.startStreaming();
  }

  startStreaming() {
    if (this.pollInterval) clearInterval(this.pollInterval);

    const img = new Image();
    img.onload = () => {
      if (this.ctx && this.canvas) {
        if (this.canvas.width !== img.width || this.canvas.height !== img.height) {
          this.canvas.width = img.width || 1080;
          this.canvas.height = img.height || 1920;
        }
        this.ctx.drawImage(img, 0, 0, this.canvas.width, this.canvas.height);
      }
      this.frameCount++;
      const now = Date.now();
      if (now - this.lastFpsUpdate >= 1000) {
        if (this.fpsEl) this.fpsEl.textContent = `${this.frameCount} FPS`;
        this.frameCount = 0;
        this.lastFpsUpdate = now;
      }
    };

    this.pollInterval = setInterval(() => {
      if (!this.isConnected) return;
      img.src = `${this.serverUrl}/stream/frame?t=${Date.now()}`;
    }, 33); // ~30 FPS
  }

  handleMouseEvent(e, type) {
    if (!this.isConnected || !this.canvas) return;
    const rect = this.canvas.getBoundingClientRect();
    const nx = Math.max(0, Math.min(1, (e.clientX - rect.left) / rect.width));
    const ny = Math.max(0, Math.min(1, (e.clientY - rect.top) / rect.height));

    const payload = {
      type: 'mouse_event',
      event: type,
      x: nx,
      y: ny,
      button: e.button === 2 ? 'right' : 'left'
    };

    fetch(`${this.serverUrl}/input/mouse`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    }).catch(() => {});
  }

  navigate(url) {
    if (!url) return;
    fetch(`${this.serverUrl}/navigation/load`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ url: url })
    }).catch(() => {});
  }
}

if (typeof window !== 'undefined') {
  window.addEventListener('DOMContentLoaded', () => {
    window.fireballClient = new FireballWebClient();
  });
}

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { FireballWebClient };
}
