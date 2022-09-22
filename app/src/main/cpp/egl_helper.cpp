#include <android/native_window_jni.h>
#include "egl_helper.h"
#include "common.h"

static EGLConfig chooseEglConfig(EGLDisplay display) {
    int attribList[] = {
            EGL_BUFFER_SIZE, 32,
            EGL_ALPHA_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_NONE
    };
    EGLConfig configs[1];
    int numConfigs[1];

    if (!eglChooseConfig(display, attribList, configs, 1, numConfigs)) {
        return NULL;
    }
    return configs[0];
}

static EGLContext createEglContext(EGLDisplay display, EGLConfig config) {
    int contextList[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
    };

    return eglCreateContext(display, config, EGL_NO_CONTEXT, contextList);
}

static EGLSurface
createEGLSurface(JNIEnv *env, EGLDisplay display, EGLConfig config, jobject surface) {
    int attribList[] = {EGL_NONE};

    ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
    return eglCreateWindowSurface(display, config, window, attribList);
}

bool EGLHelper::Init(JNIEnv *env, jobject jSurface) {
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    if (display == EGL_NO_DISPLAY) {
        LOGD("can't get eglGetDisplay");
        return false;
    }

    if (!eglInitialize(display, NULL, NULL)) {
        LOGD("eglInitialize failed");
        return false;
    }

    config = chooseEglConfig(display);
    context = createEglContext(display, config);
    if (context == EGL_NO_CONTEXT) {
        return false;
    }

    surface = createEGLSurface(env, display, config, jSurface);
    MakeCurrent();
    return true;
}

EGLHelper::EGLHelper() :
        display(EGL_NO_DISPLAY),
        config(NULL),
        context(EGL_NO_CONTEXT),
        surface(EGL_NO_SURFACE) {

}

void EGLHelper::Destroy() {
    if (surface != EGL_NO_SURFACE) {
        eglDestroySurface(display, surface);
        surface = EGL_NO_SURFACE;
    }

    if (context != EGL_NO_CONTEXT) {
        eglDestroyContext(display, context);
        context = EGL_NO_CONTEXT;
    }

    if (config != NULL) {
        config = NULL;
    }

    if (display != EGL_NO_DISPLAY) {
        display = EGL_NO_DISPLAY;
    }
}

void EGLHelper::SwapBuffers() {
    eglSwapBuffers(display, surface);
}

void EGLHelper::MakeCurrent() {
    eglMakeCurrent(display, surface, surface, context);
}
