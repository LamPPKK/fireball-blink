package com.fireball.j2me;

import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;
import javax.microedition.lcdui.Font;

/**
 * Custom LCDUI Canvas supporting Hybrid Rendering Modes:
 * 1. MODE_BEAM_STREAM: Remote compressed video/tile stream from Fireball Server.
 * 2. MODE_LOCAL_HTML: Built-in local MicroHTML/WAP parser & renderer.
 * 3. MODE_PLATFORM_BROWSER: Native device browser delegation via MIDlet.platformRequest().
 */
public class BrowserCanvas extends Canvas implements BeamStreamListener {
    public static final int MODE_BEAM_STREAM = 1;
    public static final int MODE_LOCAL_HTML = 2;

    private final FireballMIDlet midlet;
    private final BeamJ2MEClient beamClient;
    private final MicroHtmlEngine localHtmlEngine;

    // Viewport & Cursor State
    private int engineMode = MODE_BEAM_STREAM;
    private Image currentFrame;
    private int cursorX = 120;
    private int cursorY = 160;
    private int scrollY = 0;
    private boolean isConnected = false;
    private String statusMessage = "Connecting to Fireball Server...";
    private String currentUrl = "https://duckduckgo.com";
    private String currentTitle = "DuckDuckGo";
    private int tabCount = 1;

    // Colors (Fireball Brand Theme)
    private static final int COLOR_BG_DEEP = 0x101014;
    private static final int COLOR_TITLE_BAR = 0x18181F;
    private static final int COLOR_ELECTRIC_LIME = 0xD8FF3E;
    private static final int COLOR_METEOR_ORANGE = 0xFF5A1F;
    private static final int COLOR_TEXT_PRIMARY = 0xFFFFFF;
    private static final int COLOR_TEXT_MUTED = 0x888899;

    public BrowserCanvas(FireballMIDlet midlet, BeamJ2MEClient beamClient) {
        this.midlet = midlet;
        this.beamClient = beamClient;
        this.localHtmlEngine = new MicroHtmlEngine();
        setFullScreenMode(true);

        // Pre-seed local homepage
        localHtmlEngine.parseHtml(
            "<h1>🔥 Fireball J2ME</h1>" +
            "<hr>" +
            "<p>Hybrid Engine Active: Native Micro-HTML + Remote Beam Stream</p>" +
            "<a href=\"https://duckduckgo.com\">🦆 Search with DuckDuckGo</a><br>" +
            "<a href=\"https://github.com\">🐙 GitHub Trending</a><br>" +
            "<a href=\"https://wikipedia.org\">📖 Wikipedia Quick</a><br>" +
            "<hr>" +
            "<p>Press <b>7</b> to switch engine modes. Press <b>9</b> to launch phone native browser.</p>",
            getWidth()
        );
    }

    public void start() {
        if (engineMode == MODE_BEAM_STREAM) {
            beamClient.connect();
        }
    }

    public void pause() {}

    public void stop() {
        if (beamClient != null) {
            beamClient.disconnect();
        }
    }

    public void switchEngineMode() {
        if (engineMode == MODE_BEAM_STREAM) {
            engineMode = MODE_LOCAL_HTML;
            statusMessage = "Local Micro-HTML Engine Active";
        } else {
            engineMode = MODE_BEAM_STREAM;
            statusMessage = "Beam Streamer Active";
            beamClient.connect();
        }
        repaint();
    }

    public void launchNativePlatformBrowser(String targetUrl) {
        try {
            midlet.platformRequest(targetUrl);
        } catch (Exception e) {
            statusMessage = "Platform browser unavailable";
            repaint();
        }
    }

    protected void paint(Graphics g) {
        int width = getWidth();
        int height = getHeight();

        // 1. Background
        g.setColor(COLOR_BG_DEEP);
        g.fillRect(0, 0, width, height);

        // 2. Render Body based on Engine Mode
        if (engineMode == MODE_BEAM_STREAM) {
            if (currentFrame != null) {
                g.drawImage(currentFrame, 0, 24, Graphics.TOP | Graphics.LEFT);
            } else {
                g.setColor(COLOR_TEXT_MUTED);
                Font font = Font.getFont(Font.FACE_SYSTEM, Font.STYLE_PLAIN, Font.SIZE_SMALL);
                g.setFont(font);
                g.drawString(statusMessage, width / 2, height / 2, Graphics.HCENTER | Graphics.BASELINE);
            }

            // Virtual Mouse / Touch Cursor Pointer in Stream Mode
            g.setColor(COLOR_ELECTRIC_LIME);
            g.fillTriangle(cursorX, cursorY, cursorX + 8, cursorY + 14, cursorX + 14, cursorY + 8);
            g.setColor(0x000000);
            g.drawLine(cursorX, cursorY, cursorX + 8, cursorY + 14);
            g.drawLine(cursorX + 8, cursorY + 14, cursorX + 14, cursorY + 8);
            g.drawLine(cursorX + 14, cursorY + 8, cursorX, cursorY);
        } else {
            // Local Micro-HTML Canvas Render
            localHtmlEngine.render(g, scrollY, width, height - 42);
        }

        // 3. Top Title Bar (Omnibox & Space Badge)
        g.setColor(COLOR_TITLE_BAR);
        g.fillRect(0, 0, width, 22);
        g.setColor(COLOR_METEOR_ORANGE);
        g.drawLine(0, 22, width, 22);

        // Title text
        g.setColor(COLOR_TEXT_PRIMARY);
        Font smallBold = Font.getFont(Font.FACE_SYSTEM, Font.STYLE_BOLD, Font.SIZE_SMALL);
        g.setFont(smallBold);
        String prefix = engineMode == MODE_BEAM_STREAM ? "⚡ " : "📄 ";
        String displayTitle = prefix + (currentTitle.length() > 14 ? currentTitle.substring(0, 12) + "..." : currentTitle);
        g.drawString(displayTitle, 4, 16, Graphics.LEFT | Graphics.BASELINE);

        // Tab count pill [ 1 ]
        g.setColor(COLOR_ELECTRIC_LIME);
        g.drawRoundRect(width - 24, 3, 20, 15, 4, 4);
        g.drawString(String.valueOf(tabCount), width - 14, 15, Graphics.HCENTER | Graphics.BASELINE);

        // 4. Bottom Softkey Bar
        g.setColor(COLOR_TITLE_BAR);
        g.fillRect(0, height - 18, width, 18);
        g.setColor(COLOR_TEXT_MUTED);
        g.drawString("7:Mode", 4, height - 4, Graphics.LEFT | Graphics.BASELINE);
        String badge = engineMode == MODE_BEAM_STREAM ? "⚡ Stream" : "📄 Local";
        g.drawString(badge, width / 2, height - 4, Graphics.HCENTER | Graphics.BASELINE);
        g.drawString("9:Native", width - 4, height - 4, Graphics.RIGHT | Graphics.BASELINE);
    }

    protected void keyPressed(int keyCode) {
        int action = getGameAction(keyCode);
        int step = 14;

        if (keyCode == KEY_NUM7) {
            switchEngineMode();
            return;
        } else if (keyCode == KEY_NUM9) {
            launchNativePlatformBrowser(currentUrl);
            return;
        }

        if (engineMode == MODE_BEAM_STREAM) {
            switch (action) {
                case UP:
                    cursorY = Math.max(24, cursorY - step);
                    beamClient.sendMove(getNormalizedX(), getNormalizedY());
                    break;
                case DOWN:
                    cursorY = Math.min(getHeight() - 20, cursorY + step);
                    beamClient.sendMove(getNormalizedX(), getNormalizedY());
                    break;
                case LEFT:
                    cursorX = Math.max(0, cursorX - step);
                    beamClient.sendMove(getNormalizedX(), getNormalizedY());
                    break;
                case RIGHT:
                    cursorX = Math.min(getWidth() - 2, cursorX + step);
                    beamClient.sendMove(getNormalizedX(), getNormalizedY());
                    break;
                case FIRE:
                    beamClient.sendClick(getNormalizedX(), getNormalizedY());
                    break;
                default:
                    if (keyCode == KEY_NUM2) {
                        beamClient.sendScroll(0, -60);
                    } else if (keyCode == KEY_NUM8) {
                        beamClient.sendScroll(0, 60);
                    } else if (keyCode == KEY_NUM5) {
                        beamClient.sendClick(getNormalizedX(), getNormalizedY());
                    } else if (keyCode == KEY_STAR) {
                        beamClient.sendNewTab();
                    } else if (keyCode == KEY_POUND) {
                        beamClient.sendReload();
                    }
                    break;
            }
        } else {
            // Local HTML Mode Navigation
            switch (action) {
                case UP:
                    localHtmlEngine.selectPrevLink();
                    scrollY = Math.max(0, scrollY - 20);
                    break;
                case DOWN:
                    localHtmlEngine.selectNextLink();
                    scrollY = Math.min(localHtmlEngine.getTotalHeight(), scrollY + 20);
                    break;
                case FIRE:
                    String selectedUrl = localHtmlEngine.getSelectedLinkUrl();
                    if (selectedUrl != null) {
                        currentUrl = selectedUrl;
                        currentTitle = selectedUrl;
                        engineMode = MODE_BEAM_STREAM;
                        beamClient.connect();
                    }
                    break;
                default:
                    if (keyCode == KEY_NUM2) {
                        scrollY = Math.max(0, scrollY - 40);
                    } else if (keyCode == KEY_NUM8) {
                        scrollY = Math.min(localHtmlEngine.getTotalHeight(), scrollY + 40);
                    }
                    break;
            }
        }
        repaint();
    }

    protected void pointerPressed(int x, int y) {
        cursorX = x;
        cursorY = y;
        if (engineMode == MODE_BEAM_STREAM) {
            beamClient.sendClick(getNormalizedX(), getNormalizedY());
        }
        repaint();
    }

    private float getNormalizedX() {
        return (float) cursorX / (float) getWidth();
    }

    private float getNormalizedY() {
        int availHeight = getHeight() - 40;
        if (availHeight <= 0) return 0.0f;
        return (float) (cursorY - 24) / (float) availHeight;
    }

    public void onFrameReceived(Image frame) {
        this.currentFrame = frame;
        this.isConnected = true;
        repaint();
    }

    public void onTitleChanged(String title) {
        this.currentTitle = title;
        repaint();
    }

    public void onUrlChanged(String url) {
        this.currentUrl = url;
        repaint();
    }

    public void onStatusChanged(String status) {
        this.statusMessage = status;
        repaint();
    }
}
