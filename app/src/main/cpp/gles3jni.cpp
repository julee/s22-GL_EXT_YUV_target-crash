/*
 * Copyright 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "gles3jni.h"

const Vertex QUAD[4] = {
    // Square with diagonal < 2 so that it fits in a [-1 .. 1]^2 square
    // regardless of rotation.
    {{-0.7f, -0.7f}, {0x00, 0xFF, 0x00}},
    {{ 0.7f, -0.7f}, {0x00, 0x00, 0xFF}},
    {{-0.7f,  0.7f}, {0xFF, 0x00, 0x00}},
    {{ 0.7f,  0.7f}, {0xFF, 0xFF, 0xFF}},
};

bool checkGlError(const char* funcName) {
    GLint err = glGetError();
    if (err != GL_NO_ERROR) {
        ALOGE("GL error after %s(): 0x%08x\n", funcName, err);
        return true;
    }
    return false;
}

GLuint createShader(GLenum shaderType, const char* src) {
    GLuint shader = glCreateShader(shaderType);
    if (!shader) {
        checkGlError("glCreateShader");
        return 0;
    }
    glShaderSource(shader, 1, &src, NULL);

    GLint compiled = GL_FALSE;
    ALOGV("glCompileShader: %s", src);
    glCompileShader(shader);
    ALOGV("glCompileShader complete");
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    ALOGV("glGetShaderiv result: %d", compiled);
    if (!compiled) {
        GLint infoLogLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLen);
        if (infoLogLen > 0) {
            GLchar* infoLog = (GLchar*)malloc(infoLogLen);
            if (infoLog) {
                glGetShaderInfoLog(shader, infoLogLen, NULL, infoLog);
                ALOGE("Could not compile %s shader:\n%s\n",
                        shaderType == GL_VERTEX_SHADER ? "vertex" : "fragment",
                        infoLog);
                free(infoLog);
            }
        }
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint createProgram(const char* vtxSrc, const char* fragSrc) {
    GLuint vtxShader = 0;
    GLuint fragShader = 0;
    GLuint program = 0;
    GLint linked = GL_FALSE;

    vtxShader = createShader(GL_VERTEX_SHADER, vtxSrc);
    if (!vtxShader)
        goto exit;

    fragShader = createShader(GL_FRAGMENT_SHADER, fragSrc);
    if (!fragShader) {
        goto exit;
    }

    program = glCreateProgram();
    if (!program) {
        checkGlError("glCreateProgram");
        goto exit;
    }
    glAttachShader(program, vtxShader);
    glAttachShader(program, fragShader);

    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        ALOGE("Could not link program");
        GLint infoLogLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLen);
        if (infoLogLen) {
            GLchar* infoLog = (GLchar*)malloc(infoLogLen);
            if (infoLog) {
                glGetProgramInfoLog(program, infoLogLen, NULL, infoLog);
                ALOGE("Could not link program:\n%s\n", infoLog);
                free(infoLog);
            }
        }
        glDeleteProgram(program);
        program = 0;
    }

exit:
    glDeleteShader(vtxShader);
    glDeleteShader(fragShader);
    return program;
}

static void printGlString(const char* name, GLenum s) {
    const char* v = (const char*)glGetString(s);
    ALOGV("GL %s: %s\n", name, v);
}


// ----------------------------------------------------------------------------

static const char FRAGMENT_SHADER[] = R"|END(#version 300 es
#extension GL_EXT_YUV_target: require
precision mediump float;
precision highp int;

uniform highp vec4 TexFormatParam[10];
uniform highp __samplerExternal2DY2YEXT samplerImage;
uniform highp __samplerExternal2DY2YEXT samplerImageRight;

in highp vec2 uv;
out highp vec4 fragColor;

void main() {
    fragColor = texture(samplerImage, uv) + texture(samplerImageRight, uv);
}

)|END";

extern "C" {
    JNIEXPORT void JNICALL Java_com_android_gles3jni_GLES3JNILib_init(JNIEnv* env, jobject obj);
    JNIEXPORT void JNICALL Java_com_android_gles3jni_GLES3JNILib_resize(JNIEnv* env, jobject obj, jint width, jint height);
    JNIEXPORT void JNICALL Java_com_android_gles3jni_GLES3JNILib_step(JNIEnv* env, jobject obj);
};

#if !defined(DYNAMIC_ES3)
static GLboolean gl3stubInit() {
    return GL_TRUE;
}
#endif

JNIEXPORT void JNICALL
Java_com_android_gles3jni_GLES3JNILib_init(JNIEnv* env, jobject obj) {
    printGlString("Version", GL_VERSION);
    printGlString("Vendor", GL_VENDOR);
    printGlString("Renderer", GL_RENDERER);
    printGlString("Extensions", GL_EXTENSIONS);

    const char* versionStr = (const char*)glGetString(GL_VERSION);
    if (!(strstr(versionStr, "OpenGL ES 3.") && gl3stubInit())) {
        return;
    }

    const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
    if (strstr(extensions, "EXT_YUV_target") == nullptr) {
        ALOGV("support EXT_YUV_target: no");
        return;
    }
    ALOGV("support EXT_YUV_target: yes");

    GLuint fragShader = createShader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER);
    ALOGV("create shader success, id: %u", fragShader);
}

JNIEXPORT void JNICALL
Java_com_android_gles3jni_GLES3JNILib_resize(JNIEnv* env, jobject obj, jint width, jint height) {
}

JNIEXPORT void JNICALL
Java_com_android_gles3jni_GLES3JNILib_step(JNIEnv* env, jobject obj) {
}
