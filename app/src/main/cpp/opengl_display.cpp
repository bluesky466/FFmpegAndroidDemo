#include <jni.h>
#include <android/native_window_jni.h>
#include "opengl_display.h"
#include "common.h"

using namespace std;


static const string VERTICES_SHADER = "attribute vec2 aPosition;\n"
                                      "attribute vec2 aCoord;\n"
                                      "attribute float aPosScaleY;\n"
                                      "attribute float aPosScaleX;\n"
                                      "attribute float aCoordScaleX;\n"
                                      "varying vec4 vColor;\n"
                                      "varying vec2 vCoord;\n"
                                      "void main() {\n"
                                      "    vCoord = vec2(aCoord.x * aCoordScaleX, aCoord.y);\n"
                                      "    gl_Position = vec4(aPosition.x * aPosScaleX, aPosition.y * aPosScaleY, 0, 1);\n"
                                      "}";

static const string FRAGMENT_SHADER = "#extension GL_OES_EGL_image_external : require\n"
                                      "precision highp float;\n"
                                      "varying vec2 vCoord;\n"
                                      "uniform sampler2D texY;\n"
                                      "uniform sampler2D texU;\n"
                                      "uniform sampler2D texV;\n"
                                      "varying vec4 vColor;\n"
                                      "void main() {\n"
                                      "    float y = texture2D(texY, vCoord).x;\n"
                                      "    float u = texture2D(texU, vCoord).x - 0.5;\n"
                                      "    float v = texture2D(texV, vCoord).x - 0.5;\n"
                                      "    float r = y + 1.4075 * v;\n"
                                      "    float g = y - 0.3455 * u - 0.7169 * v;\n"
                                      "    float b = y + 1.779 * u;\n"
                                      "    gl_FragColor = vec4(r, g, b, 1);\n"
                                      "}";
static const float VERTICES[] = {
        -1.0f, 1.0f,
        -1.0f, -1.0f,
        1.0f, -1.0f,
        1.0f, 1.0f
};

// 由于OpenGLES里面纹理坐标原点是左下角,而解码的画面原点是左上角,所以纹理坐标需要上下调换一下
static const float TEXTURE_COORDS[] = {
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
        1.0f, 0.0f
};

static const short ORDERS[] = {
        0, 1, 2, // 左下角三角形

        2, 3, 0  // 右上角三角形
};

OpenGlDisplay::OpenGlDisplay()
        : mProgram(0),
          mVertShader(0),
          mFragShader(0),
          mVideoWidth(0),
          mVideoHeight(0),
          mWindowWidth(0),
          mWindowHeight(0) {
    for (int i = 0; i < TEXTURE_COUNT; ++i) {
        mTextures[i] = 0;
    }
}

bool OpenGlDisplay::Init(int windowWidth, int windowHeight, int videoWidth, int videoHeight) {
    mWindowWidth = windowWidth;
    mWindowHeight = windowHeight;

    glClearColor(0, 0, 0, 1.0f);
    glViewport(0, 0, windowWidth, windowHeight);

    mProgram = createProgram(VERTICES_SHADER, FRAGMENT_SHADER);
    if (mProgram == 0) {
        return false;
    }
    glUseProgram(mProgram);

    GLint positionId = glGetAttribLocation(mProgram, "aPosition");
    glVertexAttribPointer(positionId, 2, GL_FLOAT, false, 0, VERTICES);
    glEnableVertexAttribArray(positionId);

    GLint coord = glGetAttribLocation(mProgram, "aCoord");
    glVertexAttribPointer(coord, 2, GL_FLOAT, false, 0, TEXTURE_COORDS);
    glEnableVertexAttribArray(coord);

    SetVideoSize(videoWidth, videoHeight);
    return true;
}

void OpenGlDisplay::SetVideoSize(int videoWidth, int videoHeight) {
    mVideoWidth = videoWidth;
    mVideoHeight = videoHeight;

    // 如果不做处理(-1.0f, 1.0f),(-1.0f, -1.0f),(1.0f, -1.0f),(1.0f, 1.0f)这个矩形会铺满整个屏幕导致图像拉伸
    // 由于坐标的原点在屏幕中央,所以只需要判断是横屏还是竖屏然后对x轴或者y轴做缩放就能让图像屏幕居中,然后恢复原始视频的长宽比
    if (mWindowHeight > mWindowWidth) {
        // 如果是竖屏的话,图像的宽不需要缩放,图像的高缩小使其竖直居中
        GLint scaleX = glGetAttribLocation(mProgram, "aPosScaleX");
        glVertexAttrib1f(scaleX, 1.0f);

        // y坐标 * mWindowWidth / mWindowHeight 得到屏幕居中的正方形
        // 然后再 * videoHeight / videoWidth 就能恢复原始视频的长宽比
        float r = 1.0f * mWindowWidth / mWindowHeight * videoHeight / videoWidth;
        GLint scaleY = glGetAttribLocation(mProgram, "aPosScaleY");
        glVertexAttrib1f(scaleY, r);
    } else {
        // 如果是横屏的话,图像的高不需要缩放,图像的宽缩小使其水平居中
        GLint scaleY = glGetAttribLocation(mProgram, "aPosScaleY");
        glVertexAttrib1f(scaleY, 1.0f);

        // x坐标 * mWindowHeight / mWindowWidth 得到屏幕居中的正方形
        // 然后再 * videoWidth / videoHeight 就能恢复原始视频的长宽比
        float r = 1.0f * mWindowHeight / mWindowWidth * videoWidth / videoHeight;
        GLint scaleX = glGetAttribLocation(mProgram, "aPosScaleX");
        glVertexAttrib1f(scaleX, r);
    }
}

void OpenGlDisplay::Destroy() {
    mWindowWidth = 0;
    mWindowHeight = 0;

    mVideoWidth = 0;
    mVideoHeight = 0;

    for (int i = 0; i < TEXTURE_COUNT; ++i) {
        if (0 != mTextures[i]) {
            glDeleteTextures(1, mTextures + i);
            mTextures[i] = 0;
        }
    }

    if (0 != mProgram) {
        glDeleteProgram(mProgram);
        mProgram = 0;
    }

    if (0 != mFragShader) {
        glDeleteShader(mFragShader);
        mFragShader = 0;
    }

    if (0 != mVertShader) {
        glDeleteShader(mVertShader);
        mVertShader = 0;
    }
}

void OpenGlDisplay::Render(uint8_t *yuv420Data[3], int lineSize[3]) {
    // 解码得到的YUV数据,高是对应分量的高,但是宽却不一定是对应分量的宽
    // 这是因为在做视频解码的时候会对宽进行对齐,让宽是16或者32的整数倍,具体是16还是32由cpu决定
    // 例如我们的video.flv视频,原始画面尺寸是689x405,如果按32去对齐的话,他的Y分量的宽则是720
    // 对齐之后的宽在ffmpeg里面称为linesize
    // 而对于YUV420来说Y分量的高度为原始图像的高度,UV分量的高度由于是隔行扫描,所以是原生图像高度的一半
    setTexture(0, "texY", yuv420Data[0], lineSize[0], mVideoHeight);
    setTexture(1, "texU", yuv420Data[1], lineSize[1], mVideoHeight / 2);
    setTexture(2, "texV", yuv420Data[2], lineSize[2], mVideoHeight / 2);

    // 由于对齐之后创建的纹理宽度大于原始画面的宽度,所以如果直接显示,视频的右侧会出现异常
    // 所以我们将纹理坐标进行缩放,忽略掉右边对齐多出来的部分
    GLint scaleX = glGetAttribLocation(mProgram, "aCoordScaleX");
    glVertexAttrib1f(scaleX, mVideoWidth * 1.0f / lineSize[0]);

    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    glDrawElements(GL_TRIANGLES, sizeof(ORDERS) / sizeof(short), GL_UNSIGNED_SHORT, ORDERS);
}

GLuint OpenGlDisplay::createProgram(const string &vShaderSource, const string &fShaderSource) {
    GLuint program = glCreateProgram();
    do {
        mVertShader = loadShader(GL_VERTEX_SHADER, vShaderSource);
        if (0 == mVertShader) {
            break;
        }
        glAttachShader(program, mVertShader);

        mFragShader = loadShader(GL_FRAGMENT_SHADER, fShaderSource);
        if (0 == mFragShader) {
            break;
        }
        glAttachShader(program, mFragShader);

        glLinkProgram(program);

        int linked = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (!linked) {
            char msg[1024] = {0};
            glGetProgramInfoLog(program, sizeof(msg), NULL, msg);
            LOGD("link failed : %s", msg);
            break;
        }
        return program;
    } while (0);

    glDeleteProgram(program);
    return 0;
}

GLuint OpenGlDisplay::loadShader(GLenum shaderType, const string &source) {
    const GLchar *sourceStr = source.c_str();
    GLuint shader = glCreateShader(shaderType);
    if (!shader) {
        LOGD("glCreateShader failed");
        return 0;
    }
    glShaderSource(shader, 1, &sourceStr, NULL);
    glCompileShader(shader);

    int compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char msg[1024] = {0};
        glGetShaderInfoLog(shader, sizeof(msg), NULL, msg);
        LOGD("link compile %d shader failed : %s", shaderType, msg);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint OpenGlDisplay::createLuminanceTexture(int index, const string &texName, int width, int height) {
    GLuint texture = 0;
    glGenTextures(1, &texture);
    if (texture == 0) {
        LOGD("glGenTextures failed");
        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_LUMINANCE,
                 width,
                 height,
                 0,
                 GL_LUMINANCE,
                 GL_UNSIGNED_BYTE,
                 NULL
    );

    glActiveTexture(GL_TEXTURE0 + index);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(glGetUniformLocation(mProgram, texName.c_str()), index);

    return texture;
}

void OpenGlDisplay::setTexture(int index,
                               const string &texName,
                               const uint8_t *yuvData,
                               int width,
                               int height) {
    if (0 == mTextures[index]) {
        GLuint texture = createLuminanceTexture(index, texName, width, height);
        if(0 == texture) {
            LOGD("createLuminanceTexture %s failed", texName.c_str());
            return;
        }
        mTextures[index] = texture;
    }

    glActiveTexture(GL_TEXTURE0 + index);
    glBindTexture(GL_TEXTURE_2D, mTextures[index]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_LUMINANCE,
                    GL_UNSIGNED_BYTE, yuvData);
}
