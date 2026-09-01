package com.fireball.j2me;

import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;
import javax.microedition.io.Connector;
import javax.microedition.io.HttpConnection;
import javax.microedition.lcdui.Image;

/**
 * Lightweight binary streaming client over HTTP / Socket connecting to Fireball Server.
 */
public class BeamJ2MEClient implements Runnable {
    private final String serverUrl;
    private BeamStreamListener listener;
    private boolean isRunning = false;
    private Thread workerThread;

    public BeamJ2MEClient(String serverUrl) {
        this.serverUrl = serverUrl;
    }

    public void setListener(BeamStreamListener listener) {
        this.listener = listener;
    }

    public void connect() {
        if (!isRunning) {
            isRunning = true;
            workerThread = new Thread(this);
            workerThread.start();
        }
    }

    public void disconnect() {
        isRunning = false;
    }

    public void run() {
        if (listener != null) {
            listener.onStatusChanged("Connecting to " + serverUrl + "...");
        }

        while (isRunning) {
            HttpConnection conn = null;
            InputStream is = null;
            try {
                // Poll stream frame from Fireball Server Transcoder endpoint
                conn = (HttpConnection) Connector.open(serverUrl + "/stream/frame?format=jpeg&q=70");
                conn.setRequestMethod(HttpConnection.GET);
                conn.setRequestProperty("User-Agent", "Fireball-J2ME/1.0 (MIDP-2.0; CLDC-1.1)");

                int responseCode = conn.getResponseCode();
                if (responseCode == HttpConnection.HTTP_OK) {
                    is = conn.openInputStream();
                    Image frame = Image.createImage(is);
                    if (listener != null) {
                        listener.onFrameReceived(frame);
                    }
                } else {
                    if (listener != null) {
                        listener.onStatusChanged("Server response: " + responseCode);
                    }
                }
            } catch (Exception e) {
                if (listener != null) {
                    listener.onStatusChanged("Stream offline: " + e.getMessage());
                }
            } finally {
                try {
                    if (is != null) is.close();
                    if (conn != null) conn.close();
                } catch (IOException ignored) {}
            }

            try {
                // 15 FPS polling interval on 2G/3G EDGE networks (~66ms)
                Thread.sleep(66);
            } catch (InterruptedException e) {
                break;
            }
        }
    }

    public void sendClick(final float normX, final float normY) {
        new Thread(new Runnable() {
            public void run() {
                postEvent("/input/click?x=" + normX + "&y=" + normY);
            }
        }).start();
    }

    public void sendMove(final float normX, final float normY) {
        new Thread(new Runnable() {
            public void run() {
                postEvent("/input/move?x=" + normX + "&y=" + normY);
            }
        }).start();
    }

    public void sendScroll(final int dx, final int dy) {
        new Thread(new Runnable() {
            public void run() {
                postEvent("/input/scroll?dx=" + dx + "&dy=" + dy);
            }
        }).start();
    }

    public void sendKey(final String keyName) {
        new Thread(new Runnable() {
            public void run() {
                postEvent("/input/key?name=" + keyName);
            }
        }).start();
    }

    public void sendNewTab() {
        new Thread(new Runnable() {
            public void run() {
                postEvent("/tabs/new");
            }
        }).start();
    }

    public void sendReload() {
        new Thread(new Runnable() {
            public void run() {
                postEvent("/navigation/reload");
            }
        }).start();
    }

    private void postEvent(String path) {
        HttpConnection conn = null;
        try {
            conn = (HttpConnection) Connector.open(serverUrl + path);
            conn.setRequestMethod(HttpConnection.POST);
            conn.getResponseCode();
        } catch (Exception ignored) {
        } finally {
            try {
                if (conn != null) conn.close();
            } catch (IOException ignored) {}
        }
    }
}
