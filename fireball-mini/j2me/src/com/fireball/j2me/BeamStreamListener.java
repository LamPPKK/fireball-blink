package com.fireball.j2me;

import javax.microedition.lcdui.Image;

public interface BeamStreamListener {
    void onFrameReceived(Image frame);
    void onTitleChanged(String title);
    void onUrlChanged(String url);
    void onStatusChanged(String status);
}
