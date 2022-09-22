#include <jni.h>
#include <string>
#include <GLES2/gl2.h>

class OpenGlDisplay {
public:
    OpenGlDisplay();

    bool Init(int windowWidth, int windowHeight, int videoWidth, int videoHeight);

    void SetVideoSize(int videoWidth, int videoHeight);

    void Destroy();

    void Render(uint8_t *yuv420Data[3], int lineSize[3]);

private:
    static const int TEXTURE_COUNT = 3;
    GLuint mProgram;
    GLuint mVertShader;
    GLuint mFragShader;

    int mVideoWidth;
    int mVideoHeight;
    int mWindowWidth;
    int mWindowHeight;

    GLuint mTextures[TEXTURE_COUNT];

    GLuint createProgram(const std::string &vShaderSource, const std::string &fShaderSource);

    GLuint loadShader(GLenum shaderType, const std::string &source);

    GLuint createLuminanceTexture(int index, const std::string &texName, int width, int height);

    void setTexture(
            int index,
            const std::string &texName,
            const uint8_t *yuvData,
            int width,
            int height
    );
};

