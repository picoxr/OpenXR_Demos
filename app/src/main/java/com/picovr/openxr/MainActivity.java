package com.picovr.openxr;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.media.MediaScannerConnection;
import android.net.Uri;
import android.util.Log;
import android.content.Context;
import android.support.v4.app.ActivityCompat;
import android.Manifest;
import java.util.ArrayList;
import java.util.List;

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
        getPermission(this);
    }

    public void scanFile(String path) {
        MediaScannerConnection.scanFile(MainActivity.this, new String[] { path }, null, new MediaScannerConnection.OnScanCompletedListener() {
            public void onScanCompleted(String path, Uri uri) {
                Log.i("TAG", "Finished scanning " + path);
            }
        });
    }

    private List<String> checkPermission(Context context, String[] checkList) {
        List<String> list = new ArrayList<>();
        for (int i = 0; i < checkList.length; i++) {
            if (PackageManager.PERMISSION_GRANTED != ActivityCompat.checkSelfPermission(context, checkList[i])) {
                list.add(checkList[i]);
            }
        }
        return list;
    }

    private void requestPermission(Activity activity, String requestPermissionList[]) {
        ActivityCompat.requestPermissions(activity, requestPermissionList, 100);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        if (requestCode == 100) {
            for (int i = 0; i < permissions.length; i++) {
                if (permissions[i].equals(Manifest.permission.WRITE_EXTERNAL_STORAGE)) {
                    if (grantResults[i] == PackageManager.PERMISSION_GRANTED) {
                        Log.i("TAG", "Successfully applied for storage permission!");
                    } else {
                        Log.e("TAG", "Failed to apply for storage permission!");
                    }
                }
            }
        }
    }

    private void getPermission(Activity activity) {
        String[] checkList = new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE, Manifest.permission.READ_EXTERNAL_STORAGE};
        List<String> needRequestList = checkPermission(activity, checkList);
        if (needRequestList.isEmpty()) {
            Log.i("TAG", "No need to apply for storage permission!");
        } else {
            requestPermission(activity, needRequestList.toArray(new String[needRequestList.size()]));
        }
    }

}
