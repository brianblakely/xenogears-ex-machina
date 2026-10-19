package dev.xem.app;

import android.content.Intent;
import android.net.Uri;
import android.util.Log;

import org.libsdl.app.SDLActivity;

/**
 * SDL's activity running libmain.so (xem-android). It adds what SDL does not
 * do itself: a persistable read grant on the document SDL's file dialog
 * (ACTION_OPEN_DOCUMENT) returns, so an imported disc stays readable after
 * the app restarts; the document handed to native code even when Android
 * recreated the activity while the picker was shown (SDL's own dialog
 * callback is lost then); and test arguments from the {@code xem.args} extra.
 */
public class XemActivity extends SDLActivity {
    private static final String TAG = "xem";

    /** Hands a picked document to libmain (xem-android's disc import). */
    private static native void nativeDocumentPicked(String uri);

    @Override
    protected String[] getLibraries() {
        return new String[] {"SDL3", "main"};
    }

    /**
     * Android recreates the activity on changes no configChanges entry
     * covers (its asset paths: an overlay changing on a device's first
     * boot), at any time, before SDL_main has started too. SDL ends the
     * process on a recreation unless it is allowed, and then starts SDL_main
     * again in the same process. Allowed as soon as the libraries are loaded,
     * before any recreation is checked; SDL keeps it through SDL_Quit.
     */
    @Override
    public void loadLibraries() {
        super.loadLibraries();
        nativeSetenv("SDL_ANDROID_ALLOW_RECREATE_ACTIVITY", "1");
    }

    /** {@code am start --esa xem.args --open,<uri>} passes arguments to SDL_main. */
    @Override
    protected String[] getArguments() {
        String[] arguments = getIntent().getStringArrayExtra("xem.args");
        return arguments != null ? arguments : new String[0];
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        Uri document = data != null ? data.getData() : null;
        if (resultCode == RESULT_OK && document != null) {
            try {
                getContentResolver().takePersistableUriPermission(
                        document, Intent.FLAG_GRANT_READ_URI_PERMISSION);
                Log.i(TAG, "persisted read permission for " + document);
            } catch (SecurityException e) {
                // Readable now, but not after a restart.
                Log.w(TAG, "no persistable permission for " + document, e);
            }
            nativeDocumentPicked(document.toString());
        }
        // SDL ends its file dialog (its callback only reports a cancel).
        super.onActivityResult(requestCode, resultCode, data);
    }

    /** How long a quitting SDL_main has to end before SDL's own onDestroy. */
    private static final long QUIT_WAIT_MS = 5000;

    /**
     * SDL's onDestroy sends SDL_main its quit and waits a second, then drops
     * the quit with the lifecycle queue: a run busy longer than that (booting
     * the game when Android recreates the activity twice in a row) missed it,
     * kept its window, and the next activity's run could not create its own
     * ("Android only supports one window"). The quit goes first here, with
     * time to be taken; the run ends as it would have, SDL's wait then
     * returns at once, and the next activity starts on this thread after.
     */
    @Override
    protected void onDestroy() {
        Thread thread = mSDLThread;
        if (thread != null && !mBrokenLibraries) {
            nativeSendQuit();
            try {
                thread.join(QUIT_WAIT_MS);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
        super.onDestroy();
    }
}
