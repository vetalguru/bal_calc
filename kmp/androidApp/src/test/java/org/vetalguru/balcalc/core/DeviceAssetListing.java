package org.vetalguru.balcalc.core;

import android.content.res.AssetManager;
import android.os.Build;
import java.io.IOException;
import java.util.Arrays;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowArscAssetManager14;

/**
 * AssetManager.list as a phone does it: the entries of the folder itself.
 * Robolectric's lists the whole subtree ("ammo/x.ammo" under "seed"),
 * which would hide whether androidSeed walks into the folders. Java: a
 * static native method is replaced by hiding it, which Kotlin cannot do.
 */
@Implements(value = AssetManager.class, minSdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
public class DeviceAssetListing extends ShadowArscAssetManager14 {
    @Implementation
    protected static String[] nativeList(long ptr, String path) throws IOException {
        String[] all = ShadowArscAssetManager14.nativeList(ptr, path);
        if (all == null) return null;
        return Arrays.stream(all).map(e -> e.split("/", 2)[0]).distinct().toArray(String[]::new);
    }
}
