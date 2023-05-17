package com.picovr.openxr;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.media.MediaScannerConnection;
import android.net.Uri;
import android.util.Log;

public class MainActivity extends android.app.NativeActivity {
    static {
        System.loadLibrary("openxr_loader");
        System.loadLibrary("openxr_demos");
    }

    public native void setNativeAssetManager(AssetManager assetManager);
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setNativeAssetManager(this.getAssets());
    }

    public void scanFile(String path) {
        MediaScannerConnection.scanFile(MainActivity.this, new String[] { path }, null, new MediaScannerConnection.OnScanCompletedListener() {
            public void onScanCompleted(String path, Uri uri) {
                Log.i("TAG", "Finished scanning " + path);
            }
        });
    }
}
