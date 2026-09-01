package com.fireball.j2me;

import java.util.Vector;
import javax.microedition.lcdui.Font;
import javax.microedition.lcdui.Graphics;

/**
 * Lightweight Built-in HTML / WAP Engine for Java ME.
 * Parses and renders basic HTML tags (<p>, <a>, <b>, <h1>, <h2>, <hr>, <input>)
 * locally onto LCDUI Graphics without requiring an external server.
 */
public class MicroHtmlEngine {

    public static class RenderElement {
        public static final int TYPE_TEXT = 1;
        public static final int TYPE_HEADING = 2;
        public static final int TYPE_LINK = 3;
        public static final int TYPE_LINE = 4;

        public int type;
        public String text;
        public String targetUrl;
        public int x;
        public int y;
        public int width;
        public int height;

        public RenderElement(int type, String text, String targetUrl) {
            this.type = type;
            this.text = text;
            this.targetUrl = targetUrl;
        }
    }

    private final Vector elements = new Vector();
    private int totalHeight = 0;
    private int selectedLinkIndex = -1;

    public void parseHtml(String htmlContent, int viewportWidth) {
        elements.removeAllElements();
        totalHeight = 10;
        selectedLinkIndex = -1;

        Font plainFont = Font.getFont(Font.FACE_SYSTEM, Font.STYLE_PLAIN, Font.SIZE_SMALL);
        Font boldFont = Font.getFont(Font.FACE_SYSTEM, Font.STYLE_BOLD, Font.SIZE_MEDIUM);

        // Simple tokenization of tags and plain text
        int cursor = 0;
        int length = htmlContent.length();
        int currentY = 10;

        while (cursor < length) {
            int tagStart = htmlContent.indexOf('<', cursor);
            if (tagStart == -1) {
                String remaining = htmlContent.substring(cursor).trim();
                if (remaining.length() > 0) {
                    RenderElement el = new RenderElement(RenderElement.TYPE_TEXT, remaining, null);
                    el.x = 4;
                    el.y = currentY;
                    elements.addElement(el);
                    currentY += plainFont.getHeight() + 4;
                }
                break;
            }

            if (tagStart > cursor) {
                String text = htmlContent.substring(cursor, tagStart).trim();
                if (text.length() > 0) {
                    RenderElement el = new RenderElement(RenderElement.TYPE_TEXT, text, null);
                    el.x = 4;
                    el.y = currentY;
                    elements.addElement(el);
                    currentY += plainFont.getHeight() + 4;
                }
            }

            int tagEnd = htmlContent.indexOf('>', tagStart);
            if (tagEnd == -1) break;

            String tag = htmlContent.substring(tagStart + 1, tagEnd).trim().toLowerCase();
            cursor = tagEnd + 1;

            if (tag.startsWith("h1") || tag.startsWith("h2") || tag.startsWith("title")) {
                int closeTag = htmlContent.toLowerCase().indexOf("</", cursor);
                if (closeTag != -1) {
                    String headingText = htmlContent.substring(cursor, closeTag).trim();
                    RenderElement el = new RenderElement(RenderElement.TYPE_HEADING, headingText, null);
                    el.x = 4;
                    el.y = currentY;
                    elements.addElement(el);
                    currentY += boldFont.getHeight() + 6;
                    cursor = htmlContent.indexOf('>', closeTag) + 1;
                }
            } else if (tag.startsWith("a ")) {
                int hrefIdx = tag.indexOf("href=");
                String href = "";
                if (hrefIdx != -1) {
                    int quoteStart = tag.indexOf('"', hrefIdx);
                    if (quoteStart != -1) {
                        int quoteEnd = tag.indexOf('"', quoteStart + 1);
                        if (quoteEnd != -1) {
                            href = tag.substring(quoteStart + 1, quoteEnd);
                        }
                    }
                }
                int closeA = htmlContent.toLowerCase().indexOf("</a>", cursor);
                if (closeA != -1) {
                    String linkText = htmlContent.substring(cursor, closeA).trim();
                    RenderElement el = new RenderElement(RenderElement.TYPE_LINK, linkText, href);
                    el.x = 4;
                    el.y = currentY;
                    el.width = plainFont.stringWidth(linkText) + 4;
                    el.height = plainFont.getHeight() + 2;
                    elements.addElement(el);
                    currentY += plainFont.getHeight() + 4;
                    cursor = closeA + 4;
                }
            } else if (tag.equals("hr")) {
                RenderElement el = new RenderElement(RenderElement.TYPE_LINE, "", null);
                el.x = 4;
                el.y = currentY;
                el.width = viewportWidth - 8;
                elements.addElement(el);
                currentY += 8;
            }
        }
        totalHeight = currentY + 20;
    }

    public void render(Graphics g, int scrollY, int viewportWidth, int viewportHeight) {
        Font plainFont = Font.getFont(Font.FACE_SYSTEM, Font.STYLE_PLAIN, Font.SIZE_SMALL);
        Font boldFont = Font.getFont(Font.FACE_SYSTEM, Font.STYLE_BOLD, Font.SIZE_MEDIUM);

        for (int i = 0; i < elements.size(); i++) {
            RenderElement el = (RenderElement) elements.elementAt(i);
            int drawY = el.y - scrollY;

            if (drawY + 30 < 0 || drawY > viewportHeight) continue;

            if (el.type == RenderElement.TYPE_HEADING) {
                g.setFont(boldFont);
                g.setColor(0xD8FF3E); // Electric Lime
                g.drawString(el.text, el.x, drawY, Graphics.LEFT | Graphics.TOP);
            } else if (el.type == RenderElement.TYPE_LINK) {
                g.setFont(plainFont);
                if (i == selectedLinkIndex) {
                    g.setColor(0xFF5A1F); // Meteor Orange background highlight
                    g.fillRect(el.x, drawY, el.width, el.height);
                    g.setColor(0x000000);
                } else {
                    g.setColor(0x5599FF); // Hyperlink Blue
                }
                g.drawString(el.text, el.x + 2, drawY + 1, Graphics.LEFT | Graphics.TOP);
                g.drawLine(el.x, drawY + el.height, el.x + el.width, drawY + el.height);
            } else if (el.type == RenderElement.TYPE_LINE) {
                g.setColor(0x333344);
                g.drawLine(el.x, drawY, el.x + el.width, drawY);
            } else {
                g.setFont(plainFont);
                g.setColor(0xEEEEEE);
                g.drawString(el.text, el.x, drawY, Graphics.LEFT | Graphics.TOP);
            }
        }
    }

    public int getTotalHeight() {
        return totalHeight;
    }

    public void selectNextLink() {
        for (int i = selectedLinkIndex + 1; i < elements.size(); i++) {
            RenderElement el = (RenderElement) elements.elementAt(i);
            if (el.type == RenderElement.TYPE_LINK) {
                selectedLinkIndex = i;
                return;
            }
        }
    }

    public void selectPrevLink() {
        for (int i = selectedLinkIndex - 1; i >= 0; i--) {
            RenderElement el = (RenderElement) elements.elementAt(i);
            if (el.type == RenderElement.TYPE_LINK) {
                selectedLinkIndex = i;
                return;
            }
        }
    }

    public String getSelectedLinkUrl() {
        if (selectedLinkIndex >= 0 && selectedLinkIndex < elements.size()) {
            RenderElement el = (RenderElement) elements.elementAt(selectedLinkIndex);
            return el.targetUrl;
        }
        return null;
    }
}
