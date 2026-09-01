package com.fireball.j2me;

import javax.microedition.midlet.MIDlet;
import javax.microedition.display.Display;
import javax.microedition.display.Displayable;

/**
 * Fireball J2ME Edition (MIDP 2.0 / CLDC 1.1)
 * Ultra-lightweight thin streaming client connecting to Fireball Server.
 */
public class FireballMIDlet extends MIDlet {
    private Display display;
    private BrowserCanvas browserCanvas;
    private BeamJ2MEClient beamClient;

    public FireballMIDlet() {
        // Constructor
    }

    protected void startApp() {
        if (display == null) {
            display = Display.getDisplay(this);
            beamClient = new BeamJ2MEClient("http://127.0.0.1:9090");
            browserCanvas = new BrowserCanvas(this, beamClient);
            beamClient.setListener(browserCanvas);
        }
        display.setCurrent(browserCanvas);
        browserCanvas.start();
    }

    protected void pauseApp() {
        if (browserCanvas != null) {
            browserCanvas.pause();
        }
    }

    protected void destroyApp(boolean unconditional) {
        if (beamClient != null) {
            beamClient.disconnect();
        }
        if (browserCanvas != null) {
            browserCanvas.stop();
        }
    }

    public void exit() {
        destroyApp(true);
        notifyDestroyed();
    }
}
