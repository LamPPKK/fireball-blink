package com.fireball.j2me;

import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;
import javax.microedition.lcdui.Font;

/**
 * Custom LCDUI Canvas rendering the compressed Fireball Server stream,
 * virtual pointer cursor, title bar, tab indicators, and D-Pad softkeys.
 */
public class BrowserCanvas extends Canvas implements BeamStreamListener {
    private final FireballMIDlet midlet;
    private final BeamJ2MEClient beamClient;

    // Viewport & Cursor State
    private Image currentFrame;
    private int cursorX = 120;
    private int cursorY = 160;
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
        setFullScreenMode(true);
    }

    public void start() {
        beamClient.connect();
    }

    public void pause() {
        // Pause render loop
    }

    public void stop() {
        // Stop render loop
    }

    protected void paint(Graphics g) {
        int width = getWidth();
        int height = getHeight();

        // 1. Background
        g.setColor(COLOR_BG_DEEP);
        g.fillRect(0, 0, width, height);

        // 2. Render Stream Frame
        if (currentFrame != null) {
            g.drawImage(currentFrame, 0, 24, Graphics.TOP | Graphics.LEFT);
        } else {
            // Placeholder Canvas
            g.setColor(COLOR_TEXT_MUTED);
            Font font = Font.getFont(Font.FACE_SYSTEM, Font.STYLE_PLAIN, Font.SIZE_SMALL);
            g.setFont(font);
            g.drawString(statusMessage, width / 2, height / 2, Graphics.HCENTER | Graphics.BASELINE);
        }

        // 3. Top Title Bar (Omnibox & Space Badge)
        g.setColor(COLOR_TITLE_BAR);
        g.fillRect(0, 0, width, 22);

        // Bottom border of title bar
        g.setColor(COLOR_METEOR_ORANGE);
        g.drawLine(0, 22, width, 22);

        // Title text
        g.setColor(COLOR_TEXT_PRIMARY);
        Font smallBold = Font.getFont(Font.FACE_SYSTEM, Font.STYLE_BOLD, Font.SIZE_SMALL);
        g.setFont(smallBold);
        String displayTitle = currentTitle.length() > 18 ? currentTitle.substring(0, 15) + "..." : currentTitle;
        g.drawString(displayTitle, 4, 16, Graphics.LEFT | Graphics.BASELINE);

        // Tab count pill [ 1 ]
        g.setColor(COLOR_ELECTRIC_LIME);
        g.drawRoundRect(width - 24, 3, 20, 15, 4, 4);
        g.drawString(String.valueOf(tabCount), width - 14, 15, Graphics.HCENTER | Graphics.BASELINE);

        // 4. Virtual Mouse / Touch Cursor Pointer
        g.setColor(COLOR_ELECTRIC_LIME);
        g.fillTriangle(cursorX, cursorY, cursorX + 8, cursorY + 14, cursorX + 14, cursorY + 8);
        g.setColor(0x000000);
        g.drawLine(cursorX, cursorY, cursorX + 8, cursorY + 14);
        g.drawLine(cursorX + 8, cursorY + 14, cursorX + 14, cursorY + 8);
        g.drawLine(cursorX + 14, cursorY + 8, cursorX, cursorY);

        // 5. Bottom Softkey Bar
        g.setColor(COLOR_TITLE_BAR);
        g.fillRect(0, height - 18, width, 18);
        g.setColor(COLOR_TEXT_MUTED);
        g.drawString("Menu", 4, height - 4, Graphics.LEFT | Graphics.BASELINE);
        g.drawString("⚡ Beam Stream", width / 2, height - 4, Graphics.HCENTER | Graphics.BASELINE);
        g.drawString("Exit", width - 4, height - 4, Graphics.RIGHT | Graphics.BASELINE);
    }

    protected void keyPressed(int keyCode) {
        int action = getGameAction(keyCode);
        int step = 12;

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
                // Primary Click / Tap at cursor position
                beamClient.sendClick(getNormalizedX(), getNormalizedY());
                break;
            default:
                // Number keypad shortcuts
                if (keyCode == KEY_NUM1) {
                    beamClient.sendKey("Home");
                } else if (keyCode == KEY_NUM2) {
                    beamClient.sendScroll(0, -60); // Scroll Up
                } else if (keyCode == KEY_NUM8) {
                    beamClient.sendScroll(0, 60);  // Scroll Down
                } else if (keyCode == KEY_NUM5) {
                    beamClient.sendClick(getNormalizedX(), getNormalizedY());
                } else if (keyCode == KEY_STAR) {
                    // Open New Tab
                    beamClient.sendNewTab();
                } else if (keyCode == KEY_POUND) {
                    // Reload
                    beamClient.sendReload();
                }
                break;
        }
        repaint();
    }

    protected void pointerPressed(int x, int y) {
        cursorX = x;
        cursorY = y;
        beamClient.sendClick(getNormalizedX(), getNormalizedY());
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

    // --- BeamStreamListener Implementation ---
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
