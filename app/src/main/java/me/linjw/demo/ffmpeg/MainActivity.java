package me.linjw.demo.ffmpeg;

import android.graphics.SurfaceTexture;
import android.os.Bundle;
import android.os.FileUtils;
import android.util.Log;
import android.view.Surface;
import android.view.TextureView;

import androidx.appcompat.app.AppCompatActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

import me.linjw.demo.ffmpeg.databinding.ActivityMainBinding;

public class MainActivity extends AppCompatActivity {
    static {
        System.loadLibrary("ffmpegdemo");
    }

    private ActivityMainBinding binding;
    private TextureView mPreview;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        File file = new File(getFilesDir(), "media.mp4");
        try {
            InputStream is = getAssets().open("media.mp4");
            OutputStream os = new FileOutputStream(file);
            FileUtils.copy(is, os);
        } catch (Exception e) {
            Log.d("FFmpegDemo", "err", e);
        }

        mPreview = findViewById(R.id.preview);
        mPreview.setSurfaceTextureListener(new TextureView.SurfaceTextureListener() {
            @Override
            public void onSurfaceTextureAvailable(final SurfaceTexture surface, final int width, final int height) {
                long mediaPtr = load(file.getAbsolutePath());
                new Thread(() -> playAudio(mediaPtr)).start();
                new Thread(() -> playVideo(mediaPtr, new Surface(surface), width, height)).start();
            }

            @Override
            public void onSurfaceTextureSizeChanged(SurfaceTexture surface, int width, int height) {

            }

            @Override
            public boolean onSurfaceTextureDestroyed(SurfaceTexture surface) {
                return false;
            }

            @Override
            public void onSurfaceTextureUpdated(SurfaceTexture surface) {

            }
        });
    }

    public native long load(String url);
    public native void release(long mediaPtr);
    public native void playVideo(long mediaPtr, Surface surface, int width, int height);
    public native void playAudio(long mediaPtr);
}