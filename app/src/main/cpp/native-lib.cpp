#include <jni.h>
#include <string>
#include <syslog.h>
#include <android/log.h>
#include <android/bitmap.h>
#include "giflib/gif_lib.h"
#include "PthreadSleep.h"

#define LOG_TAG  "C_TAG"
#define OFFSET 3
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define UNSIGNED_LITTLE_ENDIAN(lo, hi)    ((lo) | ((hi) << 8))
#define MAKE_COLOR_ARGB(r, g, b) (0xff << 24) | (b << 16) | (g << 8) | (r & 0xff )


bool isDestroy = false;

uint32_t gifColorToARGB(GifColorType color) {
    return MAKE_COLOR_ARGB(color.Red, color.Green, color.Blue);
}

void setColorARGB(uint32_t *sPixels, int imageIndex, ColorMapObject *colorMap,
                  GifByteType colorIndex, int transparentColorIndex) {
    if (imageIndex > 0 && colorIndex == transparentColorIndex) {
        return;
    }
    if (colorIndex != transparentColorIndex || transparentColorIndex == NO_TRANSPARENT_COLOR) {

        *sPixels = gifColorToARGB(colorMap->Colors[colorIndex]);
    } else {
        *sPixels = 0;
    }
}

void drawBitmap(JNIEnv *env, jobject bitmap, const GifFileType *GifFile, GifRowType *ScreenBuffer,
                int bitmapWidth, ColorMapObject *ColorMap, int imageIndex,
                SavedImage *pSaveImage, int transparentColorIndex) {
    void *pixels;  //void类型表示没有知道类型的指针，可以指向任何类型

    AndroidBitmap_lockPixels(env, bitmap, &pixels);

    //拿到像素地址
    uint32_t *sPixels = (uint32_t *) pixels;

    //数据会偏移3，这个是固定的
    int dataOffset = sizeof(int32_t) * OFFSET;
    int dH = bitmapWidth * GifFile->Image.Top;
    GifByteType colorIndex;
    for (int h = GifFile->Image.Top; h < GifFile->Image.Height; h++) {
        for (int w = GifFile->Image.Left; w < GifFile->Image.Width; w++) {
            colorIndex = (GifByteType) ScreenBuffer[h][w];

            //这里是把bitmap看做一个一维数组，挨个遍历并赋值
            setColorARGB(&sPixels[dH + w], imageIndex, ColorMap, colorIndex, transparentColorIndex);

            //这里做缓存，后面循环播放可以直接使用缓存的数据
            pSaveImage->RasterBits[dataOffset++] = colorIndex;
        }

        //遍历下一层
        dH = dH + GifFile->Image.Width;
    }

    AndroidBitmap_unlockPixels(env, bitmap);
}


extern "C" JNIEXPORT jstring JNICALL
Java_com_example_giftestdemo_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}
extern "C"
JNIEXPORT jlong JNICALL
Java_com_example_giftestdemo_GifHelper_loadGif(JNIEnv *env, jclass clazz, jstring gif_path) {

    const char *filePath = env->GetStringUTFChars(gif_path, 0);
    LOGD("filepath is : %s", filePath);
    int err;
    GifFileType *gifFile = DGifOpenFileName(filePath, &err);
    LOGD("err is : %d", err);
    return (long long) gifFile;

}
extern "C"
JNIEXPORT jint JNICALL
Java_com_example_giftestdemo_GifHelper_getWidth(JNIEnv *env, jclass clazz, jlong gif_file) {

    GifFileType *gifFile = (GifFileType *) gif_file;
    return gifFile->SWidth;

}
extern "C"
JNIEXPORT jint JNICALL
Java_com_example_giftestdemo_GifHelper_getHeight(JNIEnv *env, jclass clazz, jlong gif_file) {

    GifFileType *gifFile = (GifFileType *) gif_file;
    return gifFile->SHeight;

}
extern "C"
JNIEXPORT jint JNICALL
Java_com_example_giftestdemo_GifHelper_updateBitmap(JNIEnv *env, jclass clazz, jlong gif_file,
                                                    jobject bitmap, jobject runnable) {
    GifRowType *screenBuffer;
    GifRecordType recordType;
    GifFileType *gifFile = (GifFileType *) gif_file;
    ColorMapObject *colormap;
    SavedImage *pSavedImage;
    GifByteType *Extension;
    GifByteType *GifExtension;
    int32_t *user_image_data;
    int32_t delayTime = 0;
    int transparentColorIndex = 0;
    int disposalMode = DISPOSAL_UNSPECIFIED;
    int top, left, blockbwidth, blockheight, ExtCode;
    int jumpstart[] = {0, 1, 2, 3};
    int jumpscape[] = {8, 8, 4, 2};
    int width = gifFile->SWidth;
    int height = gifFile->SHeight;


    //线程睡眠相关
    PthreadSleep pthreadSleep;

    jclass runClass = env->GetObjectClass(runnable);
    jmethodID runMethod = env->GetMethodID(runClass, "run", "()V");


    //这里只是申请了存放每一行指针所用的内存空间 screenBuffer是一个指向指针的指针，就是二维数组
    //malloc方法请求内存之后，返回的是一个指针数据
    //screenBuffer是一个数组，并且它里面存放的数据全是指针
    screenBuffer = (GifRowType *) malloc(height * sizeof(GifRowType));
    if (screenBuffer == NULL) {
        goto end;
    }

    size_t rowsize; //一行像素占用的内存大小
    rowsize = width * sizeof(GifPixelType);
    screenBuffer[0] = (GifRowType) malloc(rowsize);  //这里申请每一行需要的内存空间

    //给screenBuffer设置背景色
    //先设置第一行
    for (int i = 0; i < width; i++) {
        screenBuffer[0][i] = (GifPixelType) gifFile->SBackGroundColor;
    }
    //拷贝第一行到其他行
    for (int i = 1; i < height; i++) {
        if ((screenBuffer[i] = (GifRowType) malloc(rowsize)) == NULL) {
            goto end;
        }
        memcpy(screenBuffer[i], screenBuffer[0], rowsize);
    }

    do {

        //获取下一块数据的类型
        if (DGifGetRecordType(gifFile, &recordType) == GIF_ERROR) {
            goto end;
        }

        LOGD("RECORD_TYPE: %d", recordType);

        switch (recordType) {
            case IMAGE_DESC_RECORD_TYPE: //图像数据文件
                if (DGifGetImageDesc(gifFile) == GIF_ERROR) {
                    return -1;
                }
                top = gifFile->Image.Top;
                left = gifFile->Image.Left;
                blockbwidth = gifFile->Image.Width;
                blockheight = gifFile->Image.Height;

                if (gifFile->Image.Interlace) {
                    //隔行扫描,分4次扫描完
                    for (int i = 0; i < 4; i++) {
                        for (int j = top + jumpstart[i]; j < top + blockheight; j = j + 4) {
                            if (DGifGetLine(gifFile, &screenBuffer[j][left], blockbwidth) ==
                                GIF_ERROR) {
                                goto end;
                            }
                        }
                    }
                } else {
                    for (int i = 0; i < height; i++) {
                        if (DGifGetLine(gifFile, &screenBuffer[top + i][left], blockbwidth) ==
                            GIF_ERROR) {
                            goto end;
                        }
                    }
                }

                colormap = gifFile->Image.ColorMap ? gifFile->Image.ColorMap : gifFile->SColorMap;

                //SavedImage 表示当前这一帧图片信息，GifFile->ImageCount 会从1开始递增，表示当前解析到第几张图片
                //这里获取SavedImage主要是想保存一些数据到当前解析的这帧图片中，循环播放的时候可以直接取出来用
                pSavedImage = &gifFile->SavedImages[gifFile->ImageCount - 1];
                //RasterBits 字段分配内存
                pSavedImage->RasterBits = (unsigned char *) malloc(
                        sizeof(GifPixelType) * gifFile->Image.Width * gifFile->Image.Height +
                        sizeof(int32_t) * 2);
                //RasterBits 这块内存用来保存一些数据，延时、透明颜色下标等，循环播放的时候要用到
                user_image_data = (int32_t *) pSavedImage->RasterBits;
                user_image_data[0] = delayTime;
                user_image_data[1] = transparentColorIndex;
                user_image_data[2] = disposalMode;


                //睡眠，delayTime 表示帧间隔时间
                pthreadSleep.msleep(delayTime * 10.0);
                LOGD("delayTime: %d", delayTime);
                delayTime = 0;

                drawBitmap(env, bitmap, gifFile, screenBuffer, blockbwidth, colormap,
                           gifFile->ImageCount - 1, pSavedImage, transparentColorIndex);

                env->CallVoidMethod(runnable, runMethod);
                break;

                //额外信息块，获取帧之间间隔、透明颜色下标
            case EXTENSION_RECORD_TYPE:

                //获取额外数据，这个函数只会返回一个数据块，调用这个函数后要调用DGifGetExtensionNext，
                if (DGifGetExtension(gifFile, &ExtCode, &Extension) == GIF_ERROR) {
                    goto end;
                }
                if (ExtCode == GRAPHICS_EXT_FUNC_CODE) {
                    if (Extension[0] != 4) {
                        goto end;
                    }
                    GifExtension = Extension + 1;

                    // 获取帧间隔，这些计算方法就先不追究了，必须要知道Gif格式，帧之间间隔单位是 10 ms
                    delayTime = UNSIGNED_LITTLE_ENDIAN(GifExtension[1], GifExtension[2]);
                    if (delayTime < 1) { // 如果没有时间，写个默认5
                        delayTime = 5;
                    }
                    if (GifExtension[0] & 0x01) {
                        //获取透明颜色下标，设置argb的时候要特殊处理
                        transparentColorIndex = (int) GifExtension[3];
                    } else {
                        transparentColorIndex = NO_TRANSPARENT_COLOR;
                    }
                    disposalMode = (GifExtension[0] >> 2) & 0x07;
                }
                while (Extension != NULL) {
                    //跳过其它块
                    if (DGifGetExtensionNext(gifFile, &Extension) == GIF_ERROR) {
                        goto end;
                    }
                }
                break;
            case TERMINATE_RECORD_TYPE:
                break;

            default:
                break;
        }

    } while (recordType != TERMINATE_RECORD_TYPE);


    while (!isDestroy) {
        for (int t = 0; t < gifFile->ImageCount; t++) {
            SavedImage frame = gifFile->SavedImages[t];
            GifImageDesc frameInfo = frame.ImageDesc;
            colormap = gifFile->Image.ColorMap ? gifFile->Image.ColorMap : gifFile->SColorMap;
            user_image_data = (int32_t *) frame.RasterBits;
            delayTime = user_image_data[0];
            transparentColorIndex = user_image_data[1];
            disposalMode = user_image_data[2];

            if (delayTime < 1) {
                delayTime = 5;
            }

            pthreadSleep.msleep(delayTime * 10.0f);

            void *pixels;
            AndroidBitmap_lockPixels(env, bitmap, &pixels);
            uint32_t *sPixels = (uint32_t *) pixels;
            int pointPixelIndex = sizeof(int32_t) * OFFSET;
            int dH = width * frameInfo.Top;
            for (int h = frameInfo.Top; h < frameInfo.Top + frameInfo.Height; h++) {
                for (int w = frameInfo.Left; w < frameInfo.Left + frameInfo.Width; w++) {
                    setColorARGB(&sPixels[dH + w], t, colormap, frame.RasterBits[pointPixelIndex++],
                                 transparentColorIndex);
                }
                dH += width;
            }

            AndroidBitmap_unlockPixels(env, bitmap);
            env->CallVoidMethod(runnable, runMethod);

        }
    }


    end:
    if (screenBuffer) {
        free(screenBuffer);
    }

    int Error;
    if (DGifCloseFile(gifFile, &Error) == NULL) {
        LOGD("CLOSE FILE ERROR");
    }
    gifFile = NULL;
    return 0;
}








extern "C"
JNIEXPORT void JNICALL
Java_com_example_giftestdemo_GifHelper_destroy(JNIEnv *env, jclass clazz, jlong gif_file) {

    isDestroy = true;

}