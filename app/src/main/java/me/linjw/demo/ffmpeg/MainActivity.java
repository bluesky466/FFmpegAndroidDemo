package me.linjw.demo.ffmpeg;

import androidx.appcompat.app.AppCompatActivity;

import android.graphics.SurfaceTexture;
import android.opengl.EGL14;
import android.opengl.GLES20;
import android.os.Bundle;
import android.os.FileUtils;
import android.util.Log;
import android.view.Surface;
import android.view.TextureView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

import me.linjw.demo.ffmpeg.databinding.ActivityMainBinding;

public class MainActivity extends AppCompatActivity {
    static {
        System.loadLibrary("ffmpegdemo");
    }

    //需要自己搭建rtmp服务器
    private static final String SERVER_IP = XXX.XXX.XXX.XXX;

    private ActivityMainBinding binding;
    private TextureView mPreview;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        File file = new File(getFilesDir(), "video.flv");

        try {
            InputStream is = getAssets().open("video.flv");
            OutputStream os = new FileOutputStream(file);
            FileUtils.copy(is, os);
        } catch (Exception e) {
            Log.d("FFmpegDemo", "err", e);
        }

        new Thread(new Runnable() {
            @Override
            public void run() {
                send(file.getAbsolutePath(), "rtmp://" + SERVER_IP + "/live/livestream");
            }
        }).start();

        mPreview = findViewById(R.id.preview);
        mPreview.setSurfaceTextureListener(new TextureView.SurfaceTextureListener() {
            @Override
            public void onSurfaceTextureAvailable(final SurfaceTexture surface, final int width, final int height) {
                new Thread(new Runnable() {
                    @Override
                    public void run() {
                        play("rtmp://" + SERVER_IP + "/live/livestream", new Surface(surface), width, height);
                    }
                }).start();
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

    public native void send(String srcFile, String destUrl);

    public native void play(String url, Surface surface, int width, int height);
}