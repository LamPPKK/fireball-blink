package com.fireball.j2me;

import java.util.Hashtable;
import javax.microedition.lcdui.Image;

/**
 * In-memory Tile and Frame Cache for Java ME.
 * Reduces redundant network round-trips over 2G/3G GPRS networks.
 */
public class TileCache {
    private final Hashtable cache = new Hashtable();
    private final int maxEntries;

    public TileCache(int maxEntries) {
        this.maxEntries = maxEntries;
    }

    public void put(String key, Image image) {
        if (cache.size() >= maxEntries) {
            cache.clear(); // Simple eviction strategy for constrained RAM
        }
        if (key != null && image != null) {
            cache.put(key, image);
        }
    }

    public Image get(String key) {
        if (key == null) return null;
        return (Image) cache.get(key);
    }

    public void clear() {
        cache.clear();
    }

    public int size() {
        return cache.size();
    }
}
