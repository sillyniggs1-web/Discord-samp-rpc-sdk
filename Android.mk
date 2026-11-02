LOCAL_PATH := $(call my-dir)
# Required: path to a checkout of RusJJ/AndroidModLoader (for <mod/amlmod.h> etc.)
#   ndk-build ... AML_PATH=/path/to/AndroidModLoader
AML_PATH ?= $(LOCAL_PATH)/third_party/AndroidModLoader
ifeq ($(AML_PATH),)
$(error AML_PATH is not set. Point it to a checkout of github.com/RusJJ/AndroidModLoader)
endif
ifeq ($(wildcard $(AML_PATH)/mod/amlmod.h),)
$(error $(AML_PATH)/mod/amlmod.h not found)
endif

# ---- libSampDiscordRPC.so : the AML plugin. Does NOT link the Discord SDK (so it always loads). ----
include $(CLEAR_VARS)
LOCAL_MODULE := SampDiscordRPC
LOCAL_SRC_FILES := main.cpp src/logger.cpp src/config.cpp src/presence_builder.cpp src/game_adapter.cpp \
                   src/rpc_manager.cpp src/log_backend.cpp src/lifecycle.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include $(AML_PATH)
LOCAL_CPPFLAGS += -std=c++17 -O2 -DNDEBUG -fvisibility=hidden
LOCAL_CPPFLAGS += -mfloat-abi=softfp
LOCAL_LDLIBS := -llog -ldl
LOCAL_DISABLE_NO_UNDEFINED := true   # GetInterface etc. are resolved by libAML at load time
# Set DISCORD_SDK_DIR (see tools/setup_sdk.sh) to enable the Discord path: adds the Java bootstrap to the plugin and
# builds libSampDiscordBackend.so below. Without it the plugin is LOG-ONLY.
ifneq ($(DISCORD_SDK_DIR),)
  LOCAL_SRC_FILES += src/java_bootstrap.cpp
  LOCAL_CPPFLAGS += -DSAMPRPC_WITH_DISCORD_SDK
endif
include $(BUILD_SHARED_LIBRARY)

# ---- libSampDiscordBackend.so : links the proprietary SDK. Loaded by Java (Boot.init), never by AML. ----
ifneq ($(DISCORD_SDK_DIR),)
  ifeq ($(wildcard $(DISCORD_SDK_DIR)/include/discordpp.h),)
    $(error DISCORD_SDK_DIR/include/discordpp.h not found)
  endif
  ifeq ($(wildcard $(DISCORD_SDK_DIR)/lib/$(TARGET_ARCH_ABI)/libdiscord_partner_sdk.so),)
    $(error No $(TARGET_ARCH_ABI) libdiscord_partner_sdk.so in $(DISCORD_SDK_DIR)/lib/$(TARGET_ARCH_ABI))
  endif
  include $(CLEAR_VARS)
  LOCAL_MODULE := SampDiscordBackend
  LOCAL_SRC_FILES := src/discord_backend.cpp
  LOCAL_C_INCLUDES := $(LOCAL_PATH)/include $(DISCORD_SDK_DIR)/include
  LOCAL_CPPFLAGS += -std=c++20 -O2 -DNDEBUG -fvisibility=hidden -mfloat-abi=softfp   # Social SDK >= 1.9 needs C++20
  LOCAL_LDLIBS := -llog -L$(DISCORD_SDK_DIR)/lib/$(TARGET_ARCH_ABI) -ldiscord_partner_sdk
  include $(BUILD_SHARED_LIBRARY)
endif
