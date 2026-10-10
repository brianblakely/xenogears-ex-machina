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
}
