#ifndef __EGL_HELPER_H__
#define __EGL_HELPER_H__


#include <jni.h>
#include <EGL/egl.h>

struct EGLHelper {
    EGLHelper();
    bool Init(JNIEnv *env, jobject jSurface);
    void Destroy();
    void MakeCurrent();
    void SwapBuffers();

    EGLDisplay display;
    EGLConfig config;
    EGLContext context;
    EGLSurface surface;
};

#endif