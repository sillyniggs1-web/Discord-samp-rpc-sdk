package net.sampdiscordrpc;

import android.app.Activity;
import com.discord.socialsdk.DiscordSocialSdkInit;

/**
 * Entry class that the AML plugin loads from classes.dex through a DexClassLoader.
 * Because THIS class calls System.loadLibrary, the native library is loaded by this class's loader
 * (the one that also holds the Discord SDK Java classes), and the SDK's JNI_OnLoad can find them.
 */
public final class Boot {
    private Boot() {}

    /** Loads libSampDiscordBackend.so (which pulls in libdiscord_partner_sdk.so) and hands the SDK an Activity. */
    public static void init(Activity activity) {
        System.loadLibrary("SampDiscordBackend");
        DiscordSocialSdkInit.setEngineActivity(activity);
    }

    /** Implemented in libSampDiscordBackend.so: returns the address of its SrpcBackendApi table. */
    public static native long nativeApi();
}
