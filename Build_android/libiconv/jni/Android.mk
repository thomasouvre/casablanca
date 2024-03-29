LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
TARGET_ARCH_ABI := armeabi-v7a
LOCAL_MODULE    := iconv
LOCAL_CFLAGS    := \
    -Wno-multichar \
    -D_ANDROID \
    -DLIBDIR="\"c\"" \
    -DBUILDING_LIBICONV \
    -DIN_LIBRARY \
    -Wno-static-in-inline \
    -Wno-tautological-compare \
    -Wno-parentheses-equality
#    -DLIBICONV_PLUG

LOCAL_C_INCLUDES := \
    ${LOCAL_PATH}/../libiconv-1.13.1 \
    ${LOCAL_PATH}/../libiconv-1.13.1/include \
    ${LOCAL_PATH}/../libiconv-1.13.1/lib \
    ${LOCAL_PATH}/../libiconv-1.13.1/libcharset/include
LOCAL_LDLIBS += -static-libstdc++
LOCAL_SRC_FILES := \
    ../libiconv-1.13.1/lib/iconv.c \
    ../libiconv-1.13.1/lib/relocatable.c \
    ../libiconv-1.13.1/libcharset/lib/localcharset.c
include $(BUILD_SHARED_LIBRARY)
