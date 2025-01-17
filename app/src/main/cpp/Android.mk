LOCAL_PATH := $(call my-dir)

# Add prebuilt libopenxr_loader library
include $(CLEAR_VARS)
LOCAL_MODULE := openxr_loader
LOCAL_SRC_FILES := $(LOCAL_PATH)/openxr_loader/lib/$(TARGET_ARCH_ABI)/libopenxr_loader.so
include $(PREBUILT_SHARED_LIBRARY)

# Add prebuilt assimp library
include $(CLEAR_VARS)
LOCAL_MODULE := assimp
LOCAL_SRC_FILES := $(LOCAL_PATH)/third/assimp/lib/$(TARGET_ARCH_ABI)/libassimp.so
include $(PREBUILT_SHARED_LIBRARY)

# Add imgui library
include $(CLEAR_VARS)
LOCAL_MODULE := imgui
LOCAL_CFLAGS += -W -Wall
LOCAL_C_INCLUDES += $(LOCAL_PATH)/third
LOCAL_SRC_FILES := third/imgui/imgui_widgets.cpp \
                   third/imgui/imgui_draw.cpp \
                   third/imgui/imgui_tables.cpp \
                   third/imgui/imgui.cpp
include $(BUILD_SHARED_LIBRARY)

# Add freetype library
include $(CLEAR_VARS)
LOCAL_MODULE := freetype
LOCAL_C_INCLUDES += $(LOCAL_PATH)/third/freetype-2.13.0/include
LOCAL_SRC_FILES := third/freetype-2.13.0/src/autofit/autofit.c \
                   third/freetype-2.13.0/src/base/ftbase.c \
                   third/freetype-2.13.0/src/base/ftbbox.c \
                   third/freetype-2.13.0/src/base/ftbdf.c \
                   third/freetype-2.13.0/src/base/ftbitmap.c \
                   third/freetype-2.13.0/src/base/ftcid.c \
                   third/freetype-2.13.0/src/base/ftdebug.c \
                   third/freetype-2.13.0/src/base/ftfstype.c \
                   third/freetype-2.13.0/src/base/ftgasp.c \
                   third/freetype-2.13.0/src/base/ftglyph.c \
                   third/freetype-2.13.0/src/base/ftgxval.c \
                   third/freetype-2.13.0/src/base/ftinit.c \
                   third/freetype-2.13.0/src/base/ftmm.c \
                   third/freetype-2.13.0/src/base/ftotval.c \
                   third/freetype-2.13.0/src/base/ftpatent.c \
                   third/freetype-2.13.0/src/base/ftpfr.c \
                   third/freetype-2.13.0/src/base/ftstroke.c \
                   third/freetype-2.13.0/src/base/ftsynth.c \
                   third/freetype-2.13.0/src/base/ftsystem.c \
                   third/freetype-2.13.0/src/base/fttype1.c \
                   third/freetype-2.13.0/src/base/ftwinfnt.c \
                   third/freetype-2.13.0/src/bdf/bdf.c \
                   third/freetype-2.13.0/src/bzip2/ftbzip2.c \
                   third/freetype-2.13.0/src/cache/ftcache.c \
                   third/freetype-2.13.0/src/cff/cff.c \
                   third/freetype-2.13.0/src/cid/type1cid.c \
                   third/freetype-2.13.0/src/gzip/ftgzip.c \
                   third/freetype-2.13.0/src/lzw/ftlzw.c \
                   third/freetype-2.13.0/src/pcf/pcf.c \
                   third/freetype-2.13.0/src/pfr/pfr.c \
                   third/freetype-2.13.0/src/psaux/psaux.c \
                   third/freetype-2.13.0/src/pshinter/pshinter.c \
                   third/freetype-2.13.0/src/psnames/psmodule.c \
                   third/freetype-2.13.0/src/raster/raster.c \
                   third/freetype-2.13.0/src/sdf/sdf.c \
                   third/freetype-2.13.0/src/sfnt/sfnt.c \
                   third/freetype-2.13.0/src/smooth/smooth.c \
                   third/freetype-2.13.0/src/svg/svg.c \
                   third/freetype-2.13.0/src/tools/apinames.c \
                   third/freetype-2.13.0/src/truetype/truetype.c \
                   third/freetype-2.13.0/src/type1/type1.c \
                   third/freetype-2.13.0/src/type42/type42.c \
                   third/freetype-2.13.0/src/winfonts/winfnt.c
LOCAL_CFLAGS += -W -Wall
LOCAL_CFLAGS += "-DDARWIN_NO_CARBON"
LOCAL_CFLAGS += "-DFT2_BUILD_LIBRARY"
include $(BUILD_SHARED_LIBRARY)

# Add demos library
include $(CLEAR_VARS)
LOCAL_MODULE := openxr_demos
LOCAL_CFLAGS += -DXR_USE_PLATFORM_ANDROID=1 -DXR_USE_GRAPHICS_API_OPENGL_ES=1 -DXR_USE_TIMESPEC=1
LOCAL_C_INCLUDES := $(OBOE_SDK_ROOT)/prefab/modules/oboe/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/openxr_loader/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/third
LOCAL_C_INCLUDES += $(LOCAL_PATH)/third/assimp/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/third/freetype-2.13.0/include
LOCAL_SRC_FILES := main.cpp \
                   logger.cpp \
                   platformplugin_factory.cpp \
                   platformplugin_android.cpp \
                   graphicsplugin_factory.cpp \
                   graphicsplugin_opengles.cpp \
                   openxr_loader/include/common/gfxwrapper_opengl.c \
                   openxr_program.cpp \
                   demos/shader.cpp \
                   demos/utils.cpp \
                   demos/mesh.cpp \
                   demos/model.cpp \
                   demos/controller.cpp \
                   demos/hand.cpp \
                   demos/cube.cpp \
                   demos/ray.cpp \
                   demos/guiBase.cpp \
                   demos/gui.cpp \
                   demos/text.cpp \
                   demos/player.cpp \
                   demos/application.cpp

LOCAL_LDLIBS := -llog -landroid -lGLESv3 -lEGL -lmediandk -laaudio
LOCAL_STATIC_LIBRARIES := android_native_app_glue
LOCAL_SHARED_LIBRARIES := openxr_loader assimp imgui freetype
include $(BUILD_SHARED_LIBRARY)

$(call import-module, android/native_app_glue)
