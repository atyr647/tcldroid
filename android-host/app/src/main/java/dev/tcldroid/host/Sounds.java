package dev.tcldroid.host;

import android.media.AudioAttributes;
import android.media.SoundPool;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;

/**
 * Short sound effects for Tk scripts (::tkdroid::sound), on SoundPool: low latency, many
 * overlapping streams, decoded once and kept. Files are loaded on first use, or ahead of time
 * with preload; a play request that arrives while a file is still decoding is played the moment
 * it is ready.
 */
final class Sounds {
    private final SoundPool pool;
    private final Map<String, Integer> ids = new HashMap<>();
    private final HashSet<Integer> ready = new HashSet<>();
    private final Map<Integer, float[]> waiting = new HashMap<>();	// id -> {volume, rate}

    Sounds() {
        pool = new SoundPool.Builder()
                .setMaxStreams(8)
                .setAudioAttributes(new AudioAttributes.Builder()
                        .setUsage(AudioAttributes.USAGE_GAME)
                        .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                        .build())
                .build();
        pool.setOnLoadCompleteListener(new SoundPool.OnLoadCompleteListener() {
            @Override
            public void onLoadComplete(SoundPool sp, int sampleId, int status) {
                synchronized (Sounds.this) {
                    if (status != 0) {
                        return;
                    }
                    ready.add(sampleId);
                    float[] w = waiting.remove(sampleId);
                    if (w != null) {
                        sp.play(sampleId, w[0], w[0], 1, 0, w[1]);
                    }
                }
            }
        });
    }

    private int idFor(String path) {
        Integer id = ids.get(path);
        if (id == null) {
            id = pool.load(path, 1);
            ids.put(path, id);
        }
        return id;
    }

    synchronized void preload(String path) {
        idFor(path);
    }

    synchronized void play(String path, float volume, float rate) {
        int id = idFor(path);
        if (ready.contains(id)) {
            pool.play(id, volume, volume, 1, 0, rate);
        } else {
            waiting.put(id, new float[] {volume, rate});
        }
    }

    synchronized void stopAll() {
        waiting.clear();
        for (int id : ids.values()) {
            pool.stop(id);
        }
    }
}
