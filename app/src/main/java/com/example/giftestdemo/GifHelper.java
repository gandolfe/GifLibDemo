package com.example.giftestdemo;

import android.graphics.Bitmap;
import android.os.Handler;
import android.os.HandlerThread;

public class GifHelper {

    public static long sgifFile;

    private static Handler handler;

    public static void initGif(String gifPath){
        sgifFile = loadGif(gifPath);
        HandlerThread handlerThread = new HandlerThread(gifPath);
        handlerThread.start();
        handler = new Handler(handlerThread.getLooper());
    }

    public static void refreshBitmap(final long gifFile, final Bitmap bitmap, final Runnable runnable){


        handler.post(new Runnable() {
            @Override
            public void run() {
                updateBitmap(gifFile,bitmap,runnable);
            }
        });


    }


    private static native long loadGif(String gifPath);

    public static native int getWidth(long gifFile);

    public static native int getHeight(long gifFile);

    private static native int updateBitmap(long gifFile, Bitmap bitmap, Runnable runnable);

    public static native void destroy(long gifFile);

}
