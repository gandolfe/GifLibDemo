package com.example.giftestdemo;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;

import android.Manifest;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.widget.ImageView;
import android.widget.TextView;

import java.io.File;

public class MainActivity extends AppCompatActivity {

    public static String TAG = "GIFDEMO";

    public static String filePath = "/sdcard/Pictures/fage.gif";

    public static String filePath2 = "/sdcard/Pictures/weixin_20190820160322.jpg";

    private static String[] permissions = {Manifest.permission.READ_EXTERNAL_STORAGE};

    private ImageView imageView;

    private Bitmap bitmap;

    private MyHandler handler;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        imageView = findViewById(R.id.imgview);
        TextView tv = findViewById(R.id.sample_text);
        tv.setText(stringFromJNI());

        handler = new MyHandler();

        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE)
                != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, permissions, 1);
            return;
        }

        handler.postDelayed(new Runnable() {
            @Override
            public void run() {
                loadGif();
            }
        },3000);


    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        handler.postDelayed(new Runnable() {
            @Override
            public void run() {
                loadGif();
            }
        },3000);
    }


    private void loadGif() {

        File file = new File(filePath);
        if (!file.exists()) {
            Log.e(TAG, "file not exist");
            return;
        } else {
            Log.i(TAG, "file exist");
        }

        GifHelper.initGif(filePath);
        Log.i(TAG, "addr ：" + GifHelper.sgifFile);
        int gWidth = GifHelper.getWidth(GifHelper.sgifFile);
        Log.i(TAG, "gwidth ：" + gWidth);
        int gHeight = GifHelper.getHeight(GifHelper.sgifFile);
        Log.i(TAG, "gHeight ：" + gHeight);

        bitmap = Bitmap.createBitmap(gWidth, gHeight, Bitmap.Config.ARGB_8888);
        GifHelper.refreshBitmap(GifHelper.sgifFile, bitmap, new Runnable() {
            @Override
            public void run() {
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        Log.d(TAG,"update bitmap!");
                        imageView.setImageBitmap(bitmap);
                    }
                });
            }
        });

    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        GifHelper.destroy(GifHelper.sgifFile);
    }

    static {
        System.loadLibrary("mygif-lib");
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();


    static class MyHandler extends Handler{
        @Override
        public void handleMessage(@NonNull Message msg) {
            super.handleMessage(msg);
        }
    }

}
