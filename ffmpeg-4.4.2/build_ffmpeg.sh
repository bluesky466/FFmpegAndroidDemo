#!/bin/bash
API=21
NDK=/Users/linjw/Library/Android/sdk/ndk/21.1.6352462
TOOLCHAIN=$NDK/toolchains/llvm/prebuilt/darwin-x86_64
OUTPUT=$(pwd)/android
ABIS=(armeabi-v7a arm64-v8a x86 x86_64)

SYSROOT=$TOOLCHAIN/sysroot

function build
{
echo "Build FFmpeg for $CPU..."
./configure \
    --prefix=$PREFIX \
    --enable-small \
    --enable-shared \
    --enable-neon \
    --enable-jni \
    --enable-mediacodec \
    --enable-decoder=h264_mediacodec \
    --enable-hwaccel=h264_mediacodec \
    --enable-cross-compile \
    --disable-static \
    --disable-gpl \
    --disable-postproc \
    --disable-programs \
    --disable-asm \
    --disable-ffmpeg \
    --disable-ffplay \
    --disable-ffprobe \
    --disable-avdevice \
    --disable-iconv \
    --disable-doc \
    --disable-symver \
    --cross-prefix=$CROSS_PREFIX \
    --target-os=android \
    --arch=$ARCH \
    --cpu=$CPU \
    --cc=$CC \
    --cxx=$CXX \
    --sysroot=$SYSROOT \
    --extra-cflags="-Os -fpic $OPTIMIZE_CFLAGS" 
make clean
make -j 8
make install
echo "Buld FFmpeg for $CPU success"
}

function build_armeabi-v7a
{
ARCH=arm
CPU=armv7-a
CC=$TOOLCHAIN/bin/armv7a-linux-androideabi$API-clang
CXX=$TOOLCHAIN/bin/armv7a-linux-androideabi$API-clang++
SYSROOT=$TOOLCHAIN/sysroot
CROSS_PREFIX=$TOOLCHAIN/bin/arm-linux-androideabi-
PREFIX=$OUTPUT/armeabi-v7a
OPTIMIZE_CFLAGS="-mfloat-abi=softfp -mfpu=vfp -marm -march=$CPU "
build
}

function build_arm64-v8a
{
ARCH=arm64
CPU=armv8-a
CC=$TOOLCHAIN/bin/aarch64-linux-android$API-clang
CXX=$TOOLCHAIN/bin/aarch64-linux-android$API-clang++
SYSROOT=$TOOLCHAIN/sysroot
CROSS_PREFIX=$TOOLCHAIN/bin/aarch64-linux-android-
PREFIX=$OUTPUT/arm64-v8a
OPTIMIZE_CFLAGS="-march=$CPU"
build
}

function build_x86
{
ARCH=x86
CPU=x86
CC=$TOOLCHAIN/bin/i686-linux-android$API-clang
CXX=$TOOLCHAIN/bin/i686-linux-android$API-clang++
SYSROOT=$TOOLCHAIN/sysroot
CROSS_PREFIX=$TOOLCHAIN/bin/i686-linux-android-
PREFIX=$OUTPUT/x86
OPTIMIZE_CFLAGS="-march=i686 -mtune=intel -mssse3 -mfpmath=sse -m32"
build
}

function build_x86_64
{
ARCH=x86_64
CPU=x86-64
CC=$TOOLCHAIN/bin/x86_64-linux-android$API-clang
CXX=$TOOLCHAIN/bin/x86_64-linux-android$API-clang++
SYSROOT=$TOOLCHAIN/sysroot
CROSS_PREFIX=$TOOLCHAIN/bin/x86_64-linux-android-
PREFIX=$OUTPUT/x86_64
OPTIMIZE_CFLAGS="-march=$CPU -msse4.2 -mpopcnt -m64 -mtune=intel"
build
}

function merge_output
{
    ABI=${1}
    ABI_OS_DIR=$OUTPUT/jniLibs/$ABI

    rm -rf $ABI_OS_DIR
    mkdir -p $ABI_OS_DIR
    cp -rf $OUTPUT/$ABI/lib/*.so $ABI_OS_DIR/

    rm -rf $OUTPUT/include
    cp -r $OUTPUT/$ABI/include $OUTPUT/include
}

rm -rf $OUTPUT/jniLibs
mkdir -p $OUTPUT/jniLibs

for ABI in ${ABIS[@]}
do
    eval build_$ABI
    merge_output $ABI
done