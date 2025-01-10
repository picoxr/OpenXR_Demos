// Copyright (c) 2017-2022, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "pch.h"
#include "common.h"
#include "options.h"
#include "platformdata.h"
#include "platformplugin.h"
#include "graphicsplugin.h"
#include "openxr_program.h"
#include <common/xr_linear.h>
#include <array>
#include <cmath>
#include <math.h>
#include "demos/application.h"

namespace {

typedef enum {
    DeviceTypeNone = 0,
    DeviceTypeNeo3,
    DeviceTypeNeo3Pro,
    DeviceTypeNeo3ProEye,
    DeviceTypePico4,
    DeviceTypePico4Pro,
}DeviceType;

#if !defined(XR_USE_PLATFORM_WIN32)
#define strcpy_s(dest, source) strncpy((dest), (source), sizeof(dest))
#endif

namespace Side {
const int LEFT = 0;
const int RIGHT = 1;
const int COUNT = 2;
}  // namespace Side

inline std::string GetXrVersionString(XrVersion ver) {
    return Fmt("%d.%d.%d", XR_VERSION_MAJOR(ver), XR_VERSION_MINOR(ver), XR_VERSION_PATCH(ver));
}

namespace Math {
namespace Pose {
XrPosef Identity() {
    XrPosef t{};
    t.orientation.w = 1;
    return t;
}

XrPosef Translation(const XrVector3f& translation) {
    XrPosef t = Identity();
    t.position = translation;
    return t;
}

XrPosef RotateCCWAboutYAxis(float radians, XrVector3f translation) {
    XrPosef t = Identity();
    t.orientation.x = 0.f;
    t.orientation.y = std::sin(radians * 0.5f);
    t.orientation.z = 0.f;
    t.orientation.w = std::cos(radians * 0.5f);
    t.position = translation;
    return t;
}
}  // namespace Pose
}  // namespace Math

inline XrReferenceSpaceCreateInfo GetXrReferenceSpaceCreateInfo(const std::string& referenceSpaceTypeStr) {
    XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::Identity();
    if (EqualsIgnoreCase(referenceSpaceTypeStr, "View")) {
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "ViewFront")) {
        // Render head-locked 2m in front of device.
        referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::Translation({0.f, 0.f, -2.f}),
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "Local")) {
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "Stage")) {
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "StageLeft")) {
        referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::RotateCCWAboutYAxis(0.f, {-2.f, 0.f, -2.f});
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "StageRight")) {
        referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::RotateCCWAboutYAxis(0.f, {2.f, 0.f, -2.f});
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "StageLeftRotated")) {
        referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::RotateCCWAboutYAxis(3.14f / 3.f, {-2.f, 0.5f, -2.f});
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    } else if (EqualsIgnoreCase(referenceSpaceTypeStr, "StageRightRotated")) {
        referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::RotateCCWAboutYAxis(-3.14f / 3.f, {2.f, 0.5f, -2.f});
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    } else {
        throw std::invalid_argument(Fmt("Unknown reference space type '%s'", referenceSpaceTypeStr.c_str()));
    }
    return referenceSpaceCreateInfo;
}

struct OpenXrProgram : IOpenXrProgram {
    OpenXrProgram(const std::shared_ptr<Options>& options, const std::shared_ptr<IPlatformPlugin>& platformPlugin,
                  const std::shared_ptr<IGraphicsPlugin>& graphicsPlugin)
        : m_options(*options), m_platformPlugin(platformPlugin), m_graphicsPlugin(graphicsPlugin) {
            m_application = createApplication(options, graphicsPlugin);
        }

    ~OpenXrProgram() override {
        if (m_input.actionSet != XR_NULL_HANDLE) {
            for (auto hand : {Side::LEFT, Side::RIGHT}) {
                xrDestroySpace(m_input.handSpace[hand]);
                xrDestroySpace(m_input.aimSpace[hand]);
            }
            xrDestroyActionSet(m_input.actionSet);
        }

        for (Swapchain swapchain : m_swapchains) {
            xrDestroySwapchain(swapchain.handle);
        }

        if (m_appSpace != XR_NULL_HANDLE) {
            xrDestroySpace(m_appSpace);
        }

        if (m_session != XR_NULL_HANDLE) {
            xrDestroySession(m_session);
        }

        if (m_instance != XR_NULL_HANDLE) {
            xrDestroyInstance(m_instance);
        }
    }

    void CheckDeviceSupportExtentions() {
        char buffer[64] = {0};
        __system_property_get("sys.pxr.product.name", buffer);
        Log::Write(Log::Level::Info, Fmt("device is: %s", buffer));
        if (std::string(buffer) == "Pico Neo 3 Pro Eye") {
            m_deviceType = DeviceTypeNeo3ProEye;
        } else if (std::string(buffer) == "Pico 4") {
            m_deviceType = DeviceTypePico4;
        }else if (std::string(buffer) == "PICO 4 Pro") {
            m_deviceType = DeviceTypePico4Pro;
        }

        __system_property_get("ro.build.id", buffer);
        int a, b, c;
        sscanf(buffer, "%d.%d.%d",&a, &b, &c);
        m_deviceROM = (a << 8) + (b << 4) + c;
        Log::Write(Log::Level::Info, Fmt("device ROM: %x", m_deviceROM));
        if (m_deviceROM < 0x540) {
            CHECK_XRRESULT(XR_ERROR_VALIDATION_FAILURE, "This demo can only run on devices with ROM version greater than 540");
        }

        XrSystemEyeGazeInteractionPropertiesEXT eyeTrackingSystemProperties{XR_TYPE_SYSTEM_EYE_GAZE_INTERACTION_PROPERTIES_EXT};
        XrSystemProperties systemProperties{XR_TYPE_SYSTEM_PROPERTIES, &eyeTrackingSystemProperties};
        CHECK_XRCMD(xrGetSystemProperties(m_instance, m_systemId, &systemProperties));

        if (eyeTrackingSystemProperties.supportsEyeGazeInteraction) {
            m_extentions.isSupportEyeTracking = true;
            Log::Write(Log::Level::Info, Fmt("support eye tracking"));
        } else {
            m_extentions.isSupportEyeTracking = false;
        }

        // Log system properties.
        Log::Write(Log::Level::Info, Fmt("System Properties: Name=%s VendorId=%d", systemProperties.systemName, systemProperties.vendorId));
        Log::Write(Log::Level::Info, Fmt("System Graphics Properties: MaxWidth=%d MaxHeight=%d MaxLayers=%d",
                                         systemProperties.graphicsProperties.maxSwapchainImageWidth,
                                         systemProperties.graphicsProperties.maxSwapchainImageHeight,
                                         systemProperties.graphicsProperties.maxLayerCount));
        Log::Write(Log::Level::Info, Fmt("System Tracking Properties: OrientationTracking=%s PositionTracking=%s",
                                         systemProperties.trackingProperties.orientationTracking == XR_TRUE ? "True" : "False",
                                         systemProperties.trackingProperties.positionTracking == XR_TRUE ? "True" : "False"));

    }

    static void LogLayersAndExtensions() {
        // Write out extension properties for a given layer.
        const auto logExtensions = [](const char* layerName, int indent = 0) {
            uint32_t instanceExtensionCount;
            CHECK_XRCMD(xrEnumerateInstanceExtensionProperties(layerName, 0, &instanceExtensionCount, nullptr));

            std::vector<XrExtensionProperties> extensions(instanceExtensionCount);
            for (XrExtensionProperties& extension : extensions) {
                extension.type = XR_TYPE_EXTENSION_PROPERTIES;
            }

            CHECK_XRCMD(xrEnumerateInstanceExtensionProperties(layerName, (uint32_t)extensions.size(), &instanceExtensionCount, extensions.data()));

            const std::string indentStr(indent, ' ');
            Log::Write(Log::Level::Verbose, Fmt("%sAvailable Extensions: (%d)", indentStr.c_str(), instanceExtensionCount));
            for (const XrExtensionProperties& extension : extensions) {
                Log::Write(Log::Level::Verbose, Fmt("%sAvailable Extensions:  Name=%s", indentStr.c_str(), extension.extensionName,
                                                    extension.extensionVersion));
            }
        };

        // Log non-layer extensions (layerName==nullptr).
        logExtensions(nullptr);

        // Log layers and any of their extensions.
        {
            uint32_t layerCount;
            CHECK_XRCMD(xrEnumerateApiLayerProperties(0, &layerCount, nullptr));

            std::vector<XrApiLayerProperties> layers(layerCount);
            for (XrApiLayerProperties& layer : layers) {
                layer.type = XR_TYPE_API_LAYER_PROPERTIES;
            }

            CHECK_XRCMD(xrEnumerateApiLayerProperties((uint32_t)layers.size(), &layerCount, layers.data()));

            Log::Write(Log::Level::Info, Fmt("Available Layers: (%d)", layerCount));
            for (const XrApiLayerProperties& layer : layers) {
                Log::Write(Log::Level::Info, Fmt("  Name=%s SpecVersion=%s LayerVersion=%d Description=%s", layer.layerName,
                                                        GetXrVersionString(layer.specVersion).c_str(), layer.layerVersion, layer.description));
                logExtensions(layer.layerName, 4);
            }
        }
    }

    void LogInstanceInfo() {
        CHECK(m_instance != XR_NULL_HANDLE);

        XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES};
        CHECK_XRCMD(xrGetInstanceProperties(m_instance, &instanceProperties));

        Log::Write(Log::Level::Info, Fmt("Instance RuntimeName=%s RuntimeVersion=%s", instanceProperties.runtimeName,
                                         GetXrVersionString(instanceProperties.runtimeVersion).c_str()));
    }

    void CreateInstanceInternal() {
        CHECK(m_instance == XR_NULL_HANDLE);

        // Create union of extensions required by platform and graphics plugins.
        std::vector<const char*> extensions;
        // Transform platform and graphics extension std::strings to C strings.
        const std::vector<std::string> platformExtensions = m_platformPlugin->GetInstanceExtensions();
        std::transform(platformExtensions.begin(), platformExtensions.end(), std::back_inserter(extensions),
                       [](const std::string& ext) { return ext.c_str(); });
        const std::vector<std::string> graphicsExtensions = m_graphicsPlugin->GetInstanceExtensions();
        std::transform(graphicsExtensions.begin(), graphicsExtensions.end(), std::back_inserter(extensions),
                       [](const std::string& ext) { return ext.c_str(); });

        extensions.push_back(XR_EPIC_VIEW_CONFIGURATION_FOV_EXTENSION_NAME);
        extensions.push_back(XR_KHR_CONVERT_TIMESPEC_TIME_EXTENSION_NAME);

        //XR_FB_passthrough
        extensions.push_back(XR_FB_PASSTHROUGH_EXTENSION_NAME);
        extensions.push_back(XR_FB_TRIANGLE_MESH_EXTENSION_NAME);

        //refresh rate
        extensions.push_back(XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME);

        //eye tracking
        extensions.push_back(XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME);

        //hand tracking
        extensions.push_back(XR_EXT_HAND_TRACKING_EXTENSION_NAME);
        extensions.push_back(XR_BD_CONTROLLER_INTERACTION_EXTENSION_NAME);
        XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
        createInfo.next = m_platformPlugin->GetInstanceCreateExtension();
        createInfo.enabledExtensionCount = (uint32_t)extensions.size();
        createInfo.enabledExtensionNames = extensions.data();
        
        strcpy(createInfo.applicationInfo.applicationName, "demos");
        createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

        CHECK_XRCMD(xrCreateInstance(&createInfo, &m_instance));
    }

    void CreateInstance() override {
        LogLayersAndExtensions();
        CreateInstanceInternal();
        LogInstanceInfo();

        //XR_FB_passthrough
        PFN_INITIALIZE(xrCreatePassthroughFB);
        PFN_INITIALIZE(xrDestroyPassthroughFB);
        PFN_INITIALIZE(xrPassthroughStartFB);
        PFN_INITIALIZE(xrPassthroughPauseFB);
        PFN_INITIALIZE(xrCreatePassthroughLayerFB);
        PFN_INITIALIZE(xrDestroyPassthroughLayerFB);
        PFN_INITIALIZE(xrPassthroughLayerSetStyleFB);
        PFN_INITIALIZE(xrPassthroughLayerPauseFB);
        PFN_INITIALIZE(xrPassthroughLayerResumeFB);
        PFN_INITIALIZE(xrCreateGeometryInstanceFB);
        PFN_INITIALIZE(xrDestroyGeometryInstanceFB);
        PFN_INITIALIZE(xrGeometryInstanceSetTransformFB);
        // FB_triangle_mesh
        PFN_INITIALIZE(xrCreateTriangleMeshFB);
        PFN_INITIALIZE(xrDestroyTriangleMeshFB);
        PFN_INITIALIZE(xrTriangleMeshGetVertexBufferFB);
        PFN_INITIALIZE(xrTriangleMeshGetIndexBufferFB);
        PFN_INITIALIZE(xrTriangleMeshBeginUpdateFB);
        PFN_INITIALIZE(xrTriangleMeshEndUpdateFB);
        PFN_INITIALIZE(xrTriangleMeshBeginVertexBufferUpdateFB);
        PFN_INITIALIZE(xrTriangleMeshEndVertexBufferUpdateFB);

        //hand tracking
        PFN_INITIALIZE(xrCreateHandTrackerEXT);
        PFN_INITIALIZE(xrDestroyHandTrackerEXT);
        PFN_INITIALIZE(xrLocateHandJointsEXT);

        m_extentions.initialize(m_instance);
    }

    void LogViewConfigurations() {
        CHECK(m_instance != XR_NULL_HANDLE);
        CHECK(m_systemId != XR_NULL_SYSTEM_ID);

        uint32_t viewConfigTypeCount;
        CHECK_XRCMD(xrEnumerateViewConfigurations(m_instance, m_systemId, 0, &viewConfigTypeCount, nullptr));
        std::vector<XrViewConfigurationType> viewConfigTypes(viewConfigTypeCount);
        CHECK_XRCMD(xrEnumerateViewConfigurations(m_instance, m_systemId, viewConfigTypeCount, &viewConfigTypeCount, viewConfigTypes.data()));
        CHECK((uint32_t)viewConfigTypes.size() == viewConfigTypeCount);

        Log::Write(Log::Level::Info, Fmt("Available View Configuration Types: (%d)", viewConfigTypeCount));
        for (XrViewConfigurationType viewConfigType : viewConfigTypes) {
            Log::Write(Log::Level::Verbose, Fmt("  View Configuration Type: %s %s", to_string(viewConfigType),
                                                viewConfigType == m_options.Parsed.ViewConfigType ? "(Selected)" : ""));

            XrViewConfigurationProperties viewConfigProperties{XR_TYPE_VIEW_CONFIGURATION_PROPERTIES};
            CHECK_XRCMD(xrGetViewConfigurationProperties(m_instance, m_systemId, viewConfigType, &viewConfigProperties));

            Log::Write(Log::Level::Verbose, Fmt("  View configuration FovMutable=%s", viewConfigProperties.fovMutable == XR_TRUE ? "True" : "False"));

            uint32_t viewCount;
            CHECK_XRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId, viewConfigType, 0, &viewCount, nullptr));
            if (viewCount > 0) {
                std::vector<XrViewConfigurationView> views(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
                CHECK_XRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId, viewConfigType, viewCount, &viewCount, views.data()));

                for (uint32_t i = 0; i < views.size(); i++) {
                    const XrViewConfigurationView& view = views[i];

                    Log::Write(Log::Level::Verbose, Fmt(" View [%d]: Recommended Width=%d Height=%d SampleCount=%d", i,
                                                            view.recommendedImageRectWidth, view.recommendedImageRectHeight,
                                                            view.recommendedSwapchainSampleCount));
                    Log::Write(Log::Level::Verbose, Fmt(" View [%d]:     Maximum Width=%d Height=%d SampleCount=%d", i, view.maxImageRectWidth,
                                                            view.maxImageRectHeight, view.maxSwapchainSampleCount));
                }
            } else {
                Log::Write(Log::Level::Error, Fmt("Empty view configuration type"));
            }

            LogEnvironmentBlendMode(viewConfigType);
        }
    }

    void LogEnvironmentBlendMode(XrViewConfigurationType type) {
        CHECK(m_instance != XR_NULL_HANDLE);
        CHECK(m_systemId != 0);

        uint32_t count;
        CHECK_XRCMD(xrEnumerateEnvironmentBlendModes(m_instance, m_systemId, type, 0, &count, nullptr));
        CHECK(count > 0);

        Log::Write(Log::Level::Info, Fmt("Available Environment Blend Mode count : (%d)", count));

        std::vector<XrEnvironmentBlendMode> blendModes(count);
        CHECK_XRCMD(xrEnumerateEnvironmentBlendModes(m_instance, m_systemId, type, count, &count, blendModes.data()));

        bool blendModeFound = false;
        for (XrEnvironmentBlendMode mode : blendModes) {
            const bool blendModeMatch = (mode == m_options.Parsed.EnvironmentBlendMode);
            Log::Write(Log::Level::Info, Fmt("Environment Blend Mode (%s) : %s", to_string(mode), blendModeMatch ? "(Selected)" : ""));
            blendModeFound |= blendModeMatch;
        }
        CHECK(blendModeFound);
    }

    void InitializeSystem() override {
        CHECK(m_instance != XR_NULL_HANDLE);
        CHECK(m_systemId == XR_NULL_SYSTEM_ID);

        XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
        systemInfo.formFactor = m_options.Parsed.FormFactor;
        CHECK_XRCMD(xrGetSystem(m_instance, &systemInfo, &m_systemId));

        Log::Write(Log::Level::Verbose, Fmt("Using system %d for form factor %s", m_systemId, to_string(m_options.Parsed.FormFactor)));
        CHECK(m_instance != XR_NULL_HANDLE);
        CHECK(m_systemId != XR_NULL_SYSTEM_ID);

        LogViewConfigurations();

        // The graphics API can initialize the graphics device now that the systemId and instance
        // handle are available.
        m_graphicsPlugin->InitializeDevice(m_instance, m_systemId);
    }

    void LogReferenceSpaces() {
        CHECK(m_session != XR_NULL_HANDLE);

        uint32_t spaceCount;
        CHECK_XRCMD(xrEnumerateReferenceSpaces(m_session, 0, &spaceCount, nullptr));
        std::vector<XrReferenceSpaceType> spaces(spaceCount);
        CHECK_XRCMD(xrEnumerateReferenceSpaces(m_session, spaceCount, &spaceCount, spaces.data()));

        Log::Write(Log::Level::Info, Fmt("Available reference spaces: %d", spaceCount));
        for (XrReferenceSpaceType space : spaces) {
            Log::Write(Log::Level::Verbose, Fmt("  Name: %s", to_string(space)));
        }
    }

    struct InputState {
        std::array<XrPath, Side::COUNT> handSubactionPath;
        std::array<XrSpace, Side::COUNT> handSpace;
        std::array<XrSpace, Side::COUNT> aimSpace;

        XrActionSet actionSet{XR_NULL_HANDLE};
        XrAction gripPoseAction{XR_NULL_HANDLE};
        XrAction aimPoseAction{XR_NULL_HANDLE};
        XrAction hapticAction{XR_NULL_HANDLE};

        XrAction thumbstickValueAction{XR_NULL_HANDLE};
        XrAction thumbstickClickAction{XR_NULL_HANDLE};
        XrAction thumbstickTouchAction{XR_NULL_HANDLE};
        XrAction triggerValueAction{XR_NULL_HANDLE};
        XrAction triggerClickAction{XR_NULL_HANDLE};
        XrAction triggerTouchAction{XR_NULL_HANDLE};
        XrAction squeezeValueAction{XR_NULL_HANDLE};
        XrAction squeezeClickAction{XR_NULL_HANDLE};
        
        XrAction AAction{XR_NULL_HANDLE};
        XrAction BAction{XR_NULL_HANDLE};
        XrAction XAction{XR_NULL_HANDLE};
        XrAction YAction{XR_NULL_HANDLE};
        XrAction ATouchAction{XR_NULL_HANDLE};
        XrAction BTouchAction{XR_NULL_HANDLE};
        XrAction XTouchAction{XR_NULL_HANDLE};
        XrAction YTouchAction{XR_NULL_HANDLE};
        XrAction menuAction{XR_NULL_HANDLE};

        //eye tracking
        XrAction gazeAction{XR_NULL_HANDLE};
        XrSpace  gazeActionSpace;
        XrBool32 gazeActive;
    };

    void InitializeActions() {
        // Create an action set.
        {
            XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
            strcpy_s(actionSetInfo.actionSetName, "gameplay");
            strcpy_s(actionSetInfo.localizedActionSetName, "Gameplay");
            actionSetInfo.priority = 0;
            CHECK_XRCMD(xrCreateActionSet(m_instance, &actionSetInfo, &m_input.actionSet));
        }

        // Get the XrPath for the left and right hands - we will use them as subaction paths.
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left", &m_input.handSubactionPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right", &m_input.handSubactionPath[Side::RIGHT]));

        // Create actions.
        {
            // Create an input action getting the left and right hand poses.
            XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
            actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
            strcpy_s(actionInfo.actionName, "grip_pose");
            strcpy_s(actionInfo.localizedActionName, "Grip_pose");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.gripPoseAction));

            actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
            strcpy_s(actionInfo.actionName, "aim_pose");
            strcpy_s(actionInfo.localizedActionName, "Aim_pose");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.aimPoseAction));

            // Create output actions for vibrating the left and right controller.
            actionInfo.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
            strcpy_s(actionInfo.actionName, "haptic");
            strcpy_s(actionInfo.localizedActionName, "Haptic");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.hapticAction));

            actionInfo.actionType = XR_ACTION_TYPE_VECTOR2F_INPUT;
            strcpy_s(actionInfo.actionName, "thumbstick_value");
            strcpy_s(actionInfo.localizedActionName, "Thumbstick_value");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.thumbstickValueAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "thumbstick_click");
            strcpy_s(actionInfo.localizedActionName, "Thumbstick_click");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.thumbstickClickAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "thumbstick_touch");
            strcpy_s(actionInfo.localizedActionName, "Thumbstick_touch");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.thumbstickTouchAction));

            actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
            strcpy_s(actionInfo.actionName, "trigger_value");
            strcpy_s(actionInfo.localizedActionName, "Trigger_value");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.triggerValueAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "trigger_click");
            strcpy_s(actionInfo.localizedActionName, "Trigger_click");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.triggerClickAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "trigger_touch");
            strcpy_s(actionInfo.localizedActionName, "Trigger_touch");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.triggerTouchAction));

            actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
            strcpy_s(actionInfo.actionName, "squeeze_value");
            strcpy_s(actionInfo.localizedActionName, "Squeeze_value");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.squeezeValueAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "squeeze_click");
            strcpy_s(actionInfo.localizedActionName, "Squeeze_click");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.squeezeClickAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "akey");
            strcpy_s(actionInfo.localizedActionName, "Akey");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.AAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "bkey");
            strcpy_s(actionInfo.localizedActionName, "Bkey");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.BAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "xkey");
            strcpy_s(actionInfo.localizedActionName, "Xkey");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.XAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "ykey");
            strcpy_s(actionInfo.localizedActionName, "Ykey");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.YAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "akey_touch");
            strcpy_s(actionInfo.localizedActionName, "Akey_touch");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.ATouchAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "bkey_touch");
            strcpy_s(actionInfo.localizedActionName, "Bkey_touch");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.BTouchAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "xkey_touch");
            strcpy_s(actionInfo.localizedActionName, "Xkey_touch");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.XTouchAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "ykey_touch");
            strcpy_s(actionInfo.localizedActionName, "Ykey_touch");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.YTouchAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "menukey");
            strcpy_s(actionInfo.localizedActionName, "Menukey");
            actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
            actionInfo.subactionPaths = m_input.handSubactionPath.data();
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.menuAction));
        }

        std::array<XrPath, Side::COUNT> gripPosePath;
        std::array<XrPath, Side::COUNT> aimPosePath;
        std::array<XrPath, Side::COUNT> hapticPath;

        std::array<XrPath, Side::COUNT> thumbstickValuePath;
        std::array<XrPath, Side::COUNT> thumbstickClickPath;
        std::array<XrPath, Side::COUNT> thumbstickTouchPath;
        std::array<XrPath, Side::COUNT> squeezeValuePath;
        std::array<XrPath, Side::COUNT> squeezeClickPath;
        std::array<XrPath, Side::COUNT> triggerClickPath;
        std::array<XrPath, Side::COUNT> triggerValuePath;
        std::array<XrPath, Side::COUNT> triggerTouchPath;
        std::array<XrPath, Side::COUNT> AClickPath;
        std::array<XrPath, Side::COUNT> BClickPath;
        std::array<XrPath, Side::COUNT> XClickPath;
        std::array<XrPath, Side::COUNT> YClickPath;
        std::array<XrPath, Side::COUNT> ATouchPath;
        std::array<XrPath, Side::COUNT> BTouchPath;
        std::array<XrPath, Side::COUNT> XTouchPath;
        std::array<XrPath, Side::COUNT> YTouchPath;
        std::array<XrPath, Side::COUNT> menuPath;

        // //see https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#XR_BD_controller_interaction
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/grip/pose",  &gripPosePath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/grip/pose", &gripPosePath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/aim/pose",   &aimPosePath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/aim/pose",  &aimPosePath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/output/haptic",    &hapticPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/output/haptic",   &hapticPath[Side::RIGHT]));

        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/thumbstick",        &thumbstickValuePath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/thumbstick",       &thumbstickValuePath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/thumbstick/click",  &thumbstickClickPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/thumbstick/click", &thumbstickClickPath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/thumbstick/touch",  &thumbstickTouchPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/thumbstick/touch", &thumbstickTouchPath[Side::RIGHT]));

        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/trigger/value",  &triggerValuePath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/trigger/value", &triggerValuePath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/trigger/click",  &triggerClickPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/trigger/click", &triggerClickPath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/trigger/touch",  &triggerTouchPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/trigger/touch", &triggerTouchPath[Side::RIGHT]));

        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/squeeze/value",  &squeezeValuePath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/squeeze/value", &squeezeValuePath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/squeeze/click",  &squeezeClickPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/squeeze/click", &squeezeClickPath[Side::RIGHT]));

        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/a/click", &AClickPath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/b/click", &BClickPath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/x/click",  &XClickPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/y/click",  &YClickPath[Side::LEFT]));

        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/a/touch", &ATouchPath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/b/touch", &BTouchPath[Side::RIGHT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/x/touch",  &XTouchPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/y/touch",  &YTouchPath[Side::LEFT]));

        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/left/input/menu/click", &menuPath[Side::LEFT]));
        CHECK_XRCMD(xrStringToPath(m_instance, "/user/hand/right/input/menu/click", &menuPath[Side::RIGHT]));

        // Suggest bindings for the PICO Controller.
        {
            //see https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#XR_BD_controller_interaction
            const char* interactionProfilePath = nullptr;
            if (m_deviceType == DeviceTypeNeo3 || m_deviceType == DeviceTypeNeo3Pro || m_deviceType == DeviceTypeNeo3ProEye) {
                interactionProfilePath = "/interaction_profiles/bytedance/pico_neo3_controller";
            } else {
                interactionProfilePath = "/interaction_profiles/bytedance/pico4_controller";
            }
            if (m_deviceROM < 0x540) {
                interactionProfilePath = "/interaction_profiles/pico/neo3_controller";
            }

            XrPath picoMixedRealityInteractionProfilePath;
            CHECK_XRCMD(xrStringToPath(m_instance, interactionProfilePath, &picoMixedRealityInteractionProfilePath));
            std::vector<XrActionSuggestedBinding> bindings{{{m_input.gripPoseAction, gripPosePath[Side::LEFT]},
                                                            {m_input.gripPoseAction, gripPosePath[Side::RIGHT]},
                                                            {m_input.aimPoseAction, aimPosePath[Side::LEFT]},
                                                            {m_input.aimPoseAction, aimPosePath[Side::RIGHT]},
                                                            {m_input.hapticAction, hapticPath[Side::LEFT]},
                                                            {m_input.hapticAction, hapticPath[Side::RIGHT]},

                                                            {m_input.thumbstickValueAction, thumbstickValuePath[Side::LEFT]},
                                                            {m_input.thumbstickValueAction, thumbstickValuePath[Side::RIGHT]},
                                                            {m_input.thumbstickClickAction, thumbstickClickPath[Side::LEFT]},
                                                            {m_input.thumbstickClickAction, thumbstickClickPath[Side::RIGHT]},
                                                            {m_input.thumbstickTouchAction, thumbstickTouchPath[Side::LEFT]},
                                                            {m_input.thumbstickTouchAction, thumbstickTouchPath[Side::RIGHT]}, 

                                                            {m_input.triggerValueAction, triggerValuePath[Side::LEFT]},
                                                            {m_input.triggerValueAction, triggerValuePath[Side::RIGHT]},
                                                            {m_input.triggerClickAction, triggerClickPath[Side::LEFT]},
                                                            {m_input.triggerClickAction, triggerClickPath[Side::RIGHT]},
                                                            {m_input.triggerTouchAction, triggerTouchPath[Side::LEFT]},
                                                            {m_input.triggerTouchAction, triggerTouchPath[Side::RIGHT]},
                                                            
                                                            {m_input.squeezeClickAction, squeezeClickPath[Side::LEFT]},
                                                            {m_input.squeezeClickAction, squeezeClickPath[Side::RIGHT]},
                                                            {m_input.squeezeValueAction, squeezeValuePath[Side::LEFT]},
                                                            {m_input.squeezeValueAction, squeezeValuePath[Side::RIGHT]},

                                                            {m_input.AAction, AClickPath[Side::RIGHT]},
                                                            {m_input.BAction, BClickPath[Side::RIGHT]},
                                                            {m_input.XAction, XClickPath[Side::LEFT]},
                                                            {m_input.YAction, YClickPath[Side::LEFT]},

                                                            {m_input.ATouchAction, ATouchPath[Side::RIGHT]},
                                                            {m_input.BTouchAction, BTouchPath[Side::RIGHT]},
                                                            {m_input.XTouchAction, XTouchPath[Side::LEFT]},
                                                            {m_input.YTouchAction, YTouchPath[Side::LEFT]},

                                                            {m_input.menuAction, menuPath[Side::LEFT]}}};

            if (m_deviceType == DeviceTypeNeo3 || m_deviceType == DeviceTypeNeo3Pro || m_deviceType == DeviceTypeNeo3ProEye) {
                XrActionSuggestedBinding menuRightBinding{m_input.menuAction, menuPath[Side::RIGHT]};
                bindings.push_back(menuRightBinding);
            }

            XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            suggestedBindings.interactionProfile = picoMixedRealityInteractionProfilePath;
            suggestedBindings.suggestedBindings = bindings.data();
            suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
            CHECK_XRCMD(xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings));
        }

        // eye tracking
        if (m_extentions.isSupportEyeTracking) {
            XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
            strcpy_s(actionInfo.actionName, "user_intent");
            actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
            strcpy_s(actionInfo.localizedActionName, "User intent");
            CHECK_XRCMD(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.gazeAction));

            XrPath eyeGazeInteractionProfilePath;
            XrPath gazePosePath;
            CHECK_XRCMD(xrStringToPath(m_instance, "/interaction_profiles/ext/eye_gaze_interaction", &eyeGazeInteractionProfilePath));
            CHECK_XRCMD(xrStringToPath(m_instance, "/user/eyes_ext/input/gaze_ext/pose", &gazePosePath));

            XrActionSuggestedBinding bindings;
            bindings.action = m_input.gazeAction;
            bindings.binding = gazePosePath;

            XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            suggestedBindings.interactionProfile = eyeGazeInteractionProfilePath;
            suggestedBindings.suggestedBindings = &bindings;
            suggestedBindings.countSuggestedBindings = 1;
            CHECK_XRCMD(xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings));

            XrActionSpaceCreateInfo createActionSpaceInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
            createActionSpaceInfo.action = m_input.gazeAction;
            createActionSpaceInfo.poseInActionSpace =  Math::Pose::Identity();
            CHECK_XRCMD(xrCreateActionSpace(m_session, &createActionSpaceInfo, &m_input.gazeActionSpace));
        }

        XrActionSpaceCreateInfo actionSpaceInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        actionSpaceInfo.action = m_input.gripPoseAction;
        actionSpaceInfo.poseInActionSpace.orientation.w = 1.0f;
        actionSpaceInfo.subactionPath = m_input.handSubactionPath[Side::LEFT];
        CHECK_XRCMD(xrCreateActionSpace(m_session, &actionSpaceInfo, &m_input.handSpace[Side::LEFT]));
        actionSpaceInfo.subactionPath = m_input.handSubactionPath[Side::RIGHT];
        CHECK_XRCMD(xrCreateActionSpace(m_session, &actionSpaceInfo, &m_input.handSpace[Side::RIGHT]));

        actionSpaceInfo.action = m_input.aimPoseAction;
        actionSpaceInfo.poseInActionSpace.orientation.w = 1.0f;
        actionSpaceInfo.subactionPath = m_input.handSubactionPath[Side::LEFT];
        CHECK_XRCMD(xrCreateActionSpace(m_session, &actionSpaceInfo, &m_input.aimSpace[Side::LEFT]));
        actionSpaceInfo.subactionPath = m_input.handSubactionPath[Side::RIGHT];
        CHECK_XRCMD(xrCreateActionSpace(m_session, &actionSpaceInfo, &m_input.aimSpace[Side::RIGHT]));

        XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
        attachInfo.countActionSets = 1;
        attachInfo.actionSets = &m_input.actionSet;
        CHECK_XRCMD(xrAttachSessionActionSets(m_session, &attachInfo));
    }

    void CreateVisualizedSpaces() {
        CHECK(m_session != XR_NULL_HANDLE);
        XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        referenceSpaceCreateInfo.poseInReferenceSpace = Math::Pose::Identity();
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
        CHECK_XRCMD(xrCreateReferenceSpace(m_session, &referenceSpaceCreateInfo, &m_ViewSpace));
    }

    void InitializeSession() override {
        CHECK(m_instance != XR_NULL_HANDLE);
        CHECK(m_session == XR_NULL_HANDLE);

        {
            Log::Write(Log::Level::Verbose, Fmt("Creating session..."));
            XrSessionCreateInfo createInfo{XR_TYPE_SESSION_CREATE_INFO};
            createInfo.next = m_graphicsPlugin->GetGraphicsBinding();
            createInfo.systemId = m_systemId;
            CHECK_XRCMD(xrCreateSession(m_instance, &createInfo, &m_session));
        }

        CheckDeviceSupportExtentions();
        LogReferenceSpaces();
        InitializeActions();
        CreateVisualizedSpaces();

        {
            XrReferenceSpaceCreateInfo referenceSpaceCreateInfo = GetXrReferenceSpaceCreateInfo(m_options.AppSpace);
            CHECK_XRCMD(xrCreateReferenceSpace(m_session, &referenceSpaceCreateInfo, &m_appSpace));
        }

        //XR_FB_passthrough
        {
            XrPassthroughCreateInfoFB passthroughCreateInfo = {XR_TYPE_PASSTHROUGH_CREATE_INFO_FB};
            passthroughCreateInfo.flags = XR_PASSTHROUGH_IS_RUNNING_AT_CREATION_BIT_FB;
            CHECK_XRCMD(xrCreatePassthroughFB(m_session, &passthroughCreateInfo, &m_passthrough));

            XrPassthroughLayerCreateInfoFB passthroughLayerCreateInfo = {XR_TYPE_PASSTHROUGH_LAYER_CREATE_INFO_FB};
            passthroughLayerCreateInfo.flags = XR_PASSTHROUGH_IS_RUNNING_AT_CREATION_BIT_FB;
            passthroughLayerCreateInfo.passthrough = m_passthrough;
            passthroughLayerCreateInfo.purpose = XR_PASSTHROUGH_LAYER_PURPOSE_RECONSTRUCTION_FB;
            CHECK_XRCMD(xrCreatePassthroughLayerFB(m_session, &passthroughLayerCreateInfo, &m_passthroughLayerReconstruction));

            passthroughLayerCreateInfo.purpose = XR_PASSTHROUGH_LAYER_PURPOSE_PROJECTED_FB;
            CHECK_XRCMD(xrCreatePassthroughLayerFB(m_session, &passthroughLayerCreateInfo, &m_passthroughLayerProject));

            CHECK_XRCMD(xrPassthroughStartFB(m_passthrough));
            CHECK_XRCMD(xrPassthroughLayerResumeFB(m_passthroughLayerReconstruction));

            const XrVector3f verts[] = {{0, 0, 0}, {1, 0, 0}, {0.5, 1, 0}, {-1, 0, 0}, {-0.5, -1, 0}};
            const uint32_t indexes[] = {0, 1, 2, 0, 3, 4};
            XrTriangleMeshCreateInfoFB triangleMeshCreateInfo = {XR_TYPE_TRIANGLE_MESH_CREATE_INFO_FB};
            triangleMeshCreateInfo.vertexCount = 5;
            triangleMeshCreateInfo.vertexBuffer = &verts[0];
            triangleMeshCreateInfo.triangleCount = 2;
            triangleMeshCreateInfo.indexBuffer = &indexes[0];

            CHECK_XRCMD(xrCreateTriangleMeshFB(m_session, &triangleMeshCreateInfo, &m_triangleMesh));
            Log::Write(Log::Level::Info, Fmt("Passthrough-Create Triangle_Mesh:%d", m_triangleMesh));

            XrGeometryInstanceCreateInfoFB geometryInstanceCreateInfo = {XR_TYPE_GEOMETRY_INSTANCE_CREATE_INFO_FB};
            geometryInstanceCreateInfo.layer = m_passthroughLayerProject;
            geometryInstanceCreateInfo.mesh = m_triangleMesh;
            geometryInstanceCreateInfo.baseSpace = m_appSpace;
            geometryInstanceCreateInfo.pose.orientation.w = 1.0f;
            geometryInstanceCreateInfo.scale = {1.0f, 1.0f, 1.0f};
            CHECK_XRCMD(xrCreateGeometryInstanceFB(m_session, &geometryInstanceCreateInfo, &m_geometryInstance));
            Log::Write(Log::Level::Info, Fmt("Passthrough-Create Geometry Instance:%d", m_geometryInstance));
        }


        //hand tracking
        if (xrCreateHandTrackerEXT) {
            for (auto hand : {Side::LEFT, Side::RIGHT}) {
                XrHandTrackerCreateInfoEXT createInfo{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT};
                createInfo.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;
                createInfo.hand = (hand == Side::LEFT ? XR_HAND_LEFT_EXT : XR_HAND_RIGHT_EXT);
                CHECK_XRCMD(xrCreateHandTrackerEXT(m_session, &createInfo, &m_handTracker[hand]));
            }
        }
    }

    void InitializeApplication() override {
        m_application->initialize(m_instance, m_session, &m_extentions);
        m_application->setHapticCallback((void*)this, [](void *arg, int controllerIndex, float amplitude, float seconds, float frequency) {
            OpenXrProgram* thiz = (OpenXrProgram*)arg;
            XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
            vibration.amplitude = amplitude;
            vibration.duration = seconds * 1000 * 10000000;  //nanoseconds
            vibration.frequency = frequency;
            XrHapticActionInfo hapticActionInfo{XR_TYPE_HAPTIC_ACTION_INFO};
            hapticActionInfo.action = thiz->m_input.hapticAction;
            hapticActionInfo.subactionPath = thiz->m_input.handSubactionPath[controllerIndex];
            CHECK_XRCMD(xrApplyHapticFeedback(thiz->m_session, &hapticActionInfo, (XrHapticBaseHeader*)&vibration));});
    }

    void CreateSwapchains() override {
        CHECK(m_session != XR_NULL_HANDLE);
        CHECK(m_swapchains.empty());
        CHECK(m_configViews.empty());

        // Note: No other view configurations exist at the time this code was written. If this
        // condition is not met, the project will need to be audited to see how support should be
        // added.
        CHECK_MSG(m_options.Parsed.ViewConfigType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, "Unsupported view configuration type");

        // Query and cache view configuration views.
        uint32_t viewCount;
        CHECK_XRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId, m_options.Parsed.ViewConfigType, 0, &viewCount, nullptr));
        m_configViews.resize(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
        CHECK_XRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId, m_options.Parsed.ViewConfigType, viewCount, &viewCount, m_configViews.data()));

        // Create and cache view buffer for xrLocateViews later.
        m_views.resize(viewCount, {XR_TYPE_VIEW});

        // Create the swapchain and get the images.
        if (viewCount > 0) {
            // Select a swapchain format.
            uint32_t swapchainFormatCount;
            CHECK_XRCMD(xrEnumerateSwapchainFormats(m_session, 0, &swapchainFormatCount, nullptr));
            std::vector<int64_t> swapchainFormats(swapchainFormatCount);
            CHECK_XRCMD(xrEnumerateSwapchainFormats(m_session, (uint32_t)swapchainFormats.size(), &swapchainFormatCount, swapchainFormats.data()));
            CHECK(swapchainFormatCount == swapchainFormats.size());
            m_colorSwapchainFormat = m_graphicsPlugin->SelectColorSwapchainFormat(swapchainFormats);

            // Print swapchain formats and the selected one.
            {
                std::string swapchainFormatsString;
                for (int64_t format : swapchainFormats) {
                    const bool selected = format == m_colorSwapchainFormat;
                    swapchainFormatsString += " ";
                    if (selected) {
                        swapchainFormatsString += "[";
                    }
                    swapchainFormatsString += std::to_string(format);
                    if (selected) {
                        swapchainFormatsString += "]";
                    }
                }
                Log::Write(Log::Level::Verbose, Fmt("Swapchain Formats: %s", swapchainFormatsString.c_str()));
            }

            // Create a swapchain for each view.
            for (uint32_t i = 0; i < viewCount; i++) {
                const XrViewConfigurationView& vp = m_configViews[i];
                Log::Write(Log::Level::Info, Fmt("Creating swapchain for view %d with dimensions Width=%d Height=%d SampleCount=%d, maxSampleCount=%d", i,
                                                    vp.recommendedImageRectWidth, vp.recommendedImageRectHeight, vp.recommendedSwapchainSampleCount, vp.maxSwapchainSampleCount));

                // Create the swapchain.
                XrSwapchainCreateInfo swapchainCreateInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
                swapchainCreateInfo.arraySize = 1;
                swapchainCreateInfo.format = m_colorSwapchainFormat;
                swapchainCreateInfo.width = vp.recommendedImageRectWidth;
                swapchainCreateInfo.height = vp.recommendedImageRectHeight;
                swapchainCreateInfo.mipCount = 1;
                swapchainCreateInfo.faceCount = 1;
                swapchainCreateInfo.sampleCount = m_graphicsPlugin->GetSupportedSwapchainSampleCount(vp);
                swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
                Swapchain swapchain;
                swapchain.width = swapchainCreateInfo.width;
                swapchain.height = swapchainCreateInfo.height;
                CHECK_XRCMD(xrCreateSwapchain(m_session, &swapchainCreateInfo, &swapchain.handle));

                m_swapchains.push_back(swapchain);

                uint32_t imageCount;
                CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain.handle, 0, &imageCount, nullptr));
                // XXX This should really just return XrSwapchainImageBaseHeader*
                std::vector<XrSwapchainImageBaseHeader*> swapchainImages = m_graphicsPlugin->AllocateSwapchainImageStructs(imageCount, swapchainCreateInfo);
                CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain.handle, imageCount, &imageCount, swapchainImages[0]));

                m_swapchainImages.insert(std::make_pair(swapchain.handle, std::move(swapchainImages)));
            }
        }
    }

    // Return event if one is available, otherwise return null.
    const XrEventDataBaseHeader* TryReadNextEvent() {
        // It is sufficient to clear the just the XrEventDataBuffer header to
        // XR_TYPE_EVENT_DATA_BUFFER
        XrEventDataBaseHeader* baseHeader = reinterpret_cast<XrEventDataBaseHeader*>(&m_eventDataBuffer);
        *baseHeader = {XR_TYPE_EVENT_DATA_BUFFER};
        const XrResult xr = xrPollEvent(m_instance, &m_eventDataBuffer);
        if (xr == XR_SUCCESS) {
            if (baseHeader->type == XR_TYPE_EVENT_DATA_EVENTS_LOST) {
                const XrEventDataEventsLost* const eventsLost = reinterpret_cast<const XrEventDataEventsLost*>(baseHeader);
                Log::Write(Log::Level::Warning, Fmt("%d events lost", eventsLost));
            }
            return baseHeader;
        }
        if (xr == XR_EVENT_UNAVAILABLE) {
            return nullptr;
        }
        THROW_XR(xr, "xrPollEvent");
    }

    void PollEvents(bool* exitRenderLoop, bool* requestRestart) override {
        *exitRenderLoop = *requestRestart = false;

        // Process all pending messages.
        while (const XrEventDataBaseHeader* event = TryReadNextEvent()) {
            switch (event->type) {
                case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
                    const auto& instanceLossPending = *reinterpret_cast<const XrEventDataInstanceLossPending*>(event);
                    Log::Write(Log::Level::Warning, Fmt("XrEventDataInstanceLossPending by %lld", instanceLossPending.lossTime));
                    *exitRenderLoop = true;
                    *requestRestart = true;
                    return;
                }
                case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                    auto sessionStateChangedEvent = *reinterpret_cast<const XrEventDataSessionStateChanged*>(event);
                    HandleSessionStateChangedEvent(sessionStateChangedEvent, exitRenderLoop, requestRestart);
                    break;
                }
                case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
                    LogActionSourceName(m_input.gripPoseAction, "Pose");
                    LogActionSourceName(m_input.hapticAction, "haptic");
                    break;
                case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
                default: {
                    Log::Write(Log::Level::Verbose, Fmt("Ignoring event type %d", event->type));
                    break;
                }
            }
        }
    }

    void HandleSessionStateChangedEvent(const XrEventDataSessionStateChanged& stateChangedEvent, bool* exitRenderLoop, bool* requestRestart) {
        const XrSessionState oldState = m_sessionState;
        m_sessionState = stateChangedEvent.state;

        Log::Write(Log::Level::Info, Fmt("XrEventDataSessionStateChanged: state %s->%s session=%lld time=%lld", to_string(oldState),
                                         to_string(m_sessionState), stateChangedEvent.session, stateChangedEvent.time));

        if ((stateChangedEvent.session != XR_NULL_HANDLE) && (stateChangedEvent.session != m_session)) {
            Log::Write(Log::Level::Error, "XrEventDataSessionStateChanged for unknown session");
            return;
        }

        switch (m_sessionState) {
            case XR_SESSION_STATE_READY: {
                CHECK(m_session != XR_NULL_HANDLE);
                XrSessionBeginInfo sessionBeginInfo{XR_TYPE_SESSION_BEGIN_INFO};
                sessionBeginInfo.primaryViewConfigurationType = m_options.Parsed.ViewConfigType;
                CHECK_XRCMD(xrBeginSession(m_session, &sessionBeginInfo));
                m_sessionRunning = true;
                break;
            }
            case XR_SESSION_STATE_STOPPING: {
                CHECK(m_session != XR_NULL_HANDLE);
                m_sessionRunning = false;
                CHECK_XRCMD(xrEndSession(m_session))
                break;
            }
            case XR_SESSION_STATE_EXITING: {
                *exitRenderLoop = true;
                // Do not attempt to restart because user closed this session.
                *requestRestart = false;
                break;
            }
            case XR_SESSION_STATE_LOSS_PENDING: {
                *exitRenderLoop = true;
                // Poll for a new instance.
                *requestRestart = true;
                break;
            }
            default:
                break;
        }
    }

    void LogActionSourceName(XrAction action, const std::string& actionName) const {
        XrBoundSourcesForActionEnumerateInfo getInfo = {XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO};
        getInfo.action = action;
        uint32_t pathCount = 0;
        CHECK_XRCMD(xrEnumerateBoundSourcesForAction(m_session, &getInfo, 0, &pathCount, nullptr));
        std::vector<XrPath> paths(pathCount);
        CHECK_XRCMD(xrEnumerateBoundSourcesForAction(m_session, &getInfo, uint32_t(paths.size()), &pathCount, paths.data()));

        std::string sourceName;
        for (uint32_t i = 0; i < pathCount; ++i) {
            constexpr XrInputSourceLocalizedNameFlags all = XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT |
                                                            XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT |
                                                            XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT;

            XrInputSourceLocalizedNameGetInfo nameInfo = {XR_TYPE_INPUT_SOURCE_LOCALIZED_NAME_GET_INFO};
            nameInfo.sourcePath = paths[i];
            nameInfo.whichComponents = all;

            uint32_t size = 0;
            CHECK_XRCMD(xrGetInputSourceLocalizedName(m_session, &nameInfo, 0, &size, nullptr));
            if (size < 1) {
                continue;
            }
            std::vector<char> grabSource(size);
            CHECK_XRCMD(xrGetInputSourceLocalizedName(m_session, &nameInfo, uint32_t(grabSource.size()), &size, grabSource.data()));
            if (!sourceName.empty()) {
                sourceName += " and ";
            }
            sourceName += "'";
            sourceName += std::string(grabSource.data(), size - 1);
            sourceName += "'";
        }

        Log::Write(Log::Level::Info, Fmt("%s action is bound to %s", actionName.c_str(), ((!sourceName.empty()) ? sourceName.c_str() : "nothing")));
    }

    bool IsSessionRunning() const override { return m_sessionRunning; }

    bool IsSessionFocused() const override { return m_sessionState == XR_SESSION_STATE_FOCUSED; }

    void PollActions() override {
        // Sync actions
        const XrActiveActionSet activeActionSet{m_input.actionSet, XR_NULL_PATH};
        XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
        syncInfo.countActiveActionSets = 1;
        syncInfo.activeActionSets = &activeActionSet;
        CHECK_XRCMD(xrSyncActions(m_session, &syncInfo));

        static float joystick_x[Side::COUNT] = {0};
        static float joystick_y[Side::COUNT] = {0};
        static float trigger[Side::COUNT] = {0};
        static float squeeze[Side::COUNT] = {0};

        // Get pose and grab action state and start haptic vibrate when hand is 90% squeezed.
        for (auto hand : {Side::LEFT, Side::RIGHT}) {
            ApplicationEvent &applicationEvent = m_applicationEvent[hand];
            applicationEvent.controllerEventBit = 0x00;

            XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
            getInfo.subactionPath = m_input.handSubactionPath[hand];
            //menu click
            getInfo.action = m_input.menuAction;
            XrActionStateBoolean homeValue{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getInfo, &homeValue));
            if (homeValue.isActive == XR_TRUE) {
                if(homeValue.changedSinceLastSync == XR_TRUE) {
                    applicationEvent.controllerEventBit |= CONTROLLER_EVENT_BIT_click_menu;
                    if(homeValue.currentState == XR_TRUE) {
                        applicationEvent.click_menu = true;
                    } else{
                        applicationEvent.click_menu = false;
                    }
                }
            }

            //thumbstick value x/y
            getInfo.action = m_input.thumbstickValueAction;
            XrActionStateVector2f thumbstickValue{XR_TYPE_ACTION_STATE_VECTOR2F};
            CHECK_XRCMD(xrGetActionStateVector2f(m_session, &getInfo, &thumbstickValue));
            if (thumbstickValue.isActive == XR_TRUE) {
                if (thumbstickValue.currentState.x == 0 && thumbstickValue.currentState.y == 0 && joystick_x[hand] == 0 && joystick_y[hand] == 0) {
                } else {
                    applicationEvent.controllerEventBit |= CONTROLLER_EVENT_BIT_value_thumbstick;
                    applicationEvent.thumbstick_x = thumbstickValue.currentState.x;
                    applicationEvent.thumbstick_y = thumbstickValue.currentState.y;
                }
                joystick_x[hand] = thumbstickValue.currentState.x;
                joystick_y[hand] = thumbstickValue.currentState.y;
            }
            // thumbstick click
            getInfo.action = m_input.thumbstickClickAction;
            XrActionStateBoolean thumbstickClick{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getInfo, &thumbstickClick));
            if ((thumbstickClick.isActive == XR_TRUE) && (thumbstickClick.changedSinceLastSync == XR_TRUE)) {
                applicationEvent.controllerEventBit |= CONTROLLER_EVENT_BIT_click_thumbstick;
                if(thumbstickClick.currentState == XR_TRUE) {
                    applicationEvent.click_thumbstck = true;
                } else {
                    applicationEvent.click_thumbstck = false;
                }
            }
            // thumbstick touch
            getInfo.action = m_input.thumbstickTouchAction;
            XrActionStateBoolean thumbstickTouch{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getInfo, &thumbstickTouch));
            if (thumbstickTouch.isActive == XR_TRUE) {
                if (thumbstickTouch.changedSinceLastSync == XR_TRUE) {
                    applicationEvent.controllerEventBit |= CONTROLLER_EVENT_BIT_touch_thumbstick;
                    if (thumbstickTouch.currentState == XR_TRUE) {
                        applicationEvent.touch_thumbstick = true;
                    } else {
                        applicationEvent.touch_thumbstick = false;
                    }
                }
            }

            //trigger value
            getInfo.action = m_input.triggerValueAction;
            XrActionStateFloat triggerValue{XR_TYPE_ACTION_STATE_FLOAT};
            CHECK_XRCMD(xrGetActionStateFloat(m_session, &getInfo, &triggerValue));
            if (triggerValue.isActive == XR_TRUE) {
                if (triggerValue.currentState == 0 && trigger[hand] == 0) {
                } else {
                    applicationEvent.controllerEventBit |= CONTROLLER_EVENT_BIT_value_trigger;
                    applicationEvent.trigger = triggerValue.currentState;
                }
                trigger[hand] = triggerValue.currentState;
            }
            // trigger click
            getInfo.action = m_input.triggerClickAction;
            XrActionStateBoolean TriggerClick{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getInfo, &TriggerClick));
            if (TriggerClick.isActive == XR_TRUE) {
                if (TriggerClick.changedSinceLastSync == XR_TRUE) {
                    applicationEvent.controllerEventBit |= CONTROLLER_EVENT_BIT_click_trigger;
                    if (TriggerClick.currentState == XR_TRUE) {
                        applicationEvent.click_trigger = true;
                    } else {
                        applicationEvent.click_trigger = false;
                    }
                }
            }
            // trigger touch
            getInfo.action = m_input.triggerTouchAction;
            XrActionStateBoolean TriggerTouch{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getInfo, &TriggerTouch));
            if (TriggerTouch.isActive == XR_TRUE) {
                if (TriggerTouch.changedSinceLastSync == XR_TRUE) {
                    applicationEvent.controllerEventBit |= CONTROLLER_EVENT_BIT_touch_trigger;
                    if (TriggerTouch.currentState == XR_TRUE) {
                        applicationEvent.touch_trigger = true;
                    } else {
                        applicationEvent.touch_trigger = false;
                    }
                }
            }

            // squeeze value
            getInfo.action = m_input.squeezeValueAction;
            XrActionStateFloat squeezeValue{XR_TYPE_ACTION_STATE_FLOAT};
            CHECK_XRCMD(xrGetActionStateFloat(m_session, &getInfo, &squeezeValue));
            if (squeezeValue.isActive == XR_TRUE) {
                if (squeezeValue.currentState == 0 && squeeze[hand] == 0) {
                } else {
                    applicationEvent.controllerEventBit |= CONTROLLER_EVENT_BIT_value_squeeze;
                    applicationEvent.squeeze = squeezeValue.currentState;
                }
                squeeze[hand] = squeezeValue.currentState;
            }
            // squeeze click
            getInfo.action = m_input.squeezeClickAction;
            XrActionStateBoolean squeezeClick{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getInfo, &squeezeClick));
            if ((squeezeClick.isActive == XR_TRUE) && (squeezeClick.changedSinceLastSync == XR_TRUE)) {
                applicationEvent.controllerEventBit |= CONTROLLER_EVENT_BIT_click_squeeze;
                if(squeezeClick.currentState == XR_TRUE) {
                    applicationEvent.click_squeeze = true;
                } else{
                    applicationEvent.click_squeeze = false;
                }
            }
           
            // A button
            getInfo.action = m_input.AAction;
            XrActionStateBoolean AValue{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getInfo, &AValue));
            if ((AValue.isActive == XR_TRUE) && (AValue.changedSinceLastSync == XR_TRUE)) {
                applicationEvent.controllerEventBit |= CONTROLLER_EVENT_BIT_click_a;
                if(AValue.currentState == XR_TRUE) {
                    applicationEvent.click_a = true;
                } else {
                    applicationEvent.click_a = false;
                }
            }
            // B button
            getInfo.action = m_input.BAction;
            XrActionStateBoolean BValue{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getInfo, &BValue));
            if ((BValue.isActive == XR_TRUE) && (BValue.changedSinceLastSync == XR_TRUE)) {
                applicationEvent.controllerEventBit |= CONTROLLER_EVENT_BIT_click_b;
                if(BValue.currentState == XR_TRUE) {
                    applicationEvent.click_b = true;
                } else {
                    applicationEvent.click_b = false;
                }
            }
            // X button
            getInfo.action = m_input.XAction;
            XrActionStateBoolean XValue{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getInfo, &XValue));
            if ((XValue.isActive == XR_TRUE) && (XValue.changedSinceLastSync == XR_TRUE)) {
                applicationEvent.controllerEventBit |= CONTROLLER_EVENT_BIT_click_x;
                if(XValue.currentState == XR_TRUE) {
                    applicationEvent.click_x = true;
                } else {
                    applicationEvent.click_x = false;
                }
            }
            // Y button
            getInfo.action = m_input.YAction;
            XrActionStateBoolean YValue{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getInfo, &YValue));
            if ((YValue.isActive == XR_TRUE) && (YValue.changedSinceLastSync == XR_TRUE)) {
                applicationEvent.controllerEventBit |= CONTROLLER_EVENT_BIT_click_y;
                if(YValue.currentState == XR_TRUE) {
                    applicationEvent.click_y = true;
                } else {
                    applicationEvent.click_y = false;
                }
            }

            // A button touch
            getInfo.action = m_input.ATouchAction;
            XrActionStateBoolean ATouchValue{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getInfo, &ATouchValue));
            if ((ATouchValue.isActive == XR_TRUE) && (ATouchValue.changedSinceLastSync == XR_TRUE)) {
                applicationEvent.controllerEventBit |= CONTROLLER_EVENT_BIT_touch_a;
                if(ATouchValue.currentState == XR_TRUE) {
                    applicationEvent.touch_a = true;
                } else {
                    applicationEvent.touch_a = false;
                }
            }
            // B button touch
            getInfo.action = m_input.BTouchAction;
            XrActionStateBoolean BTouchValue{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getInfo, &BTouchValue));
            if ((BTouchValue.isActive == XR_TRUE) && (BTouchValue.changedSinceLastSync == XR_TRUE)) {
                applicationEvent.controllerEventBit |= CONTROLLER_EVENT_BIT_touch_b;
                if(BTouchValue.currentState == XR_TRUE) {
                    applicationEvent.touch_b = true;
                } else {
                    applicationEvent.touch_b = false;
                }
            }
            // X button touch
            getInfo.action = m_input.XTouchAction;
            XrActionStateBoolean XTouchValue{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getInfo, &XTouchValue));
            if ((XTouchValue.isActive == XR_TRUE) && (XTouchValue.changedSinceLastSync == XR_TRUE)) {
                applicationEvent.controllerEventBit |= CONTROLLER_EVENT_BIT_touch_x;
                if(XTouchValue.currentState == XR_TRUE) {
                    applicationEvent.touch_x = true;
                } else {
                    applicationEvent.touch_x = false;
                }
            }
            // Y button touch
            getInfo.action = m_input.YTouchAction;
            XrActionStateBoolean YTouchValue{XR_TYPE_ACTION_STATE_BOOLEAN};
            CHECK_XRCMD(xrGetActionStateBoolean(m_session, &getInfo, &YTouchValue));
            if ((YTouchValue.isActive == XR_TRUE) && (YTouchValue.changedSinceLastSync == XR_TRUE)) {
                applicationEvent.controllerEventBit |= CONTROLLER_EVENT_BIT_touch_y;
                if(YTouchValue.currentState == XR_TRUE) {
                    applicationEvent.touch_y = true;
                } else {
                    applicationEvent.touch_y = false;
                }
            }

            m_application->inputEvent(hand, applicationEvent);
        }

        if (m_extentions.isSupportEyeTracking && m_extentions.activeEyeTracking) {
            XrActionStatePose actionStatePose{XR_TYPE_ACTION_STATE_POSE};
            XrActionStateGetInfo getActionStateInfo{XR_TYPE_ACTION_STATE_GET_INFO};
            getActionStateInfo.action = m_input.gazeAction;
            CHECK_XRCMD(xrGetActionStatePose(m_session, &getActionStateInfo, &actionStatePose));
            m_input.gazeActive = actionStatePose.isActive;
        }
    }

    void RenderFrame() override {
        CHECK(m_session != XR_NULL_HANDLE);
        XrFrameWaitInfo frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
        XrFrameState frameState{XR_TYPE_FRAME_STATE};
        CHECK_XRCMD(xrWaitFrame(m_session, &frameWaitInfo, &frameState));

        XrFrameBeginInfo frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
        CHECK_XRCMD(xrBeginFrame(m_session, &frameBeginInfo));

        std::vector<XrCompositionLayerBaseHeader*> layers;
        XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        std::vector<XrCompositionLayerProjectionView> projectionLayerViews;
        if (frameState.shouldRender == XR_TRUE) {
            if (RenderLayer(frameState.predictedDisplayTime, projectionLayerViews, layer)) {
                layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer));
            }
        }

        if (m_extentions.activePassthrough) {
            XrCompositionLayerPassthroughFB compositionLayerPassthrough = {XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_FB};
            compositionLayerPassthrough.layerHandle = m_passthroughLayerReconstruction;
            //passthrough_layer.layerHandle = m_passthroughLayer_project;
            compositionLayerPassthrough.flags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            compositionLayerPassthrough.space = XR_NULL_HANDLE;
            layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&compositionLayerPassthrough));
        }

        XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
        frameEndInfo.displayTime = frameState.predictedDisplayTime;
        frameEndInfo.environmentBlendMode = m_options.Parsed.EnvironmentBlendMode;
        frameEndInfo.layerCount = (uint32_t)layers.size();
        frameEndInfo.layers = layers.data();
        CHECK_XRCMD(xrEndFrame(m_session, &frameEndInfo));
    }

    bool RenderLayer(XrTime predictedDisplayTime, std::vector<XrCompositionLayerProjectionView>& projectionLayerViews, XrCompositionLayerProjection& layer) {
        XrResult res;
        XrViewState viewState{XR_TYPE_VIEW_STATE};
        uint32_t viewCapacityInput = (uint32_t)m_views.size();
        uint32_t viewCountOutput;
        XrViewLocateInfo viewLocateInfo{XR_TYPE_VIEW_LOCATE_INFO};
        viewLocateInfo.viewConfigurationType = m_options.Parsed.ViewConfigType;
        viewLocateInfo.displayTime = predictedDisplayTime;
        viewLocateInfo.space = m_appSpace;

        res = xrLocateViews(m_session, &viewLocateInfo, &viewState, viewCapacityInput, &viewCountOutput, m_views.data());
        CHECK_XRRESULT(res, "xrLocateViews");
        if ((viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) == 0 || (viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) == 0) {
            return false;  // There is no valid tracking poses for the views.
        }

        // get ipd
        float ipd = sqrt(pow(abs(m_views[1].pose.position.x - m_views[0].pose.position.x), 2) + pow(abs(m_views[1].pose.position.y - m_views[0].pose.position.y), 2) + pow(abs(m_views[1].pose.position.z - m_views[0].pose.position.z), 2));

        CHECK(viewCountOutput == viewCapacityInput);
        CHECK(viewCountOutput == m_configViews.size());
        CHECK(viewCountOutput == m_swapchains.size());

        projectionLayerViews.resize(viewCountOutput);

        std::vector<XrPosef> handPose;
        for (auto hand : {Side::LEFT, Side::RIGHT}) {
            XrSpaceLocation spaceLocation{XR_TYPE_SPACE_LOCATION};
            res = xrLocateSpace(m_input.aimSpace[hand], m_appSpace, predictedDisplayTime, &spaceLocation);
            CHECK_XRRESULT(res, "xrLocateSpace");
            if (XR_UNQUALIFIED_SUCCESS(res)) {
                if ((spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
                    (spaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0) {
                    handPose.push_back(spaceLocation.pose);
                    m_application->setControllerPose(int(hand), spaceLocation.pose);
                }
            }
        }

        //eye tracking
        if (m_extentions.isSupportEyeTracking && m_extentions.activeEyeTracking) {
            if (m_input.gazeActive) {
                XrEyeGazeSampleTimeEXT eyeGazeSampleTime{XR_TYPE_EYE_GAZE_SAMPLE_TIME_EXT};
                XrSpaceLocation gazeLocation{XR_TYPE_SPACE_LOCATION, &eyeGazeSampleTime};
                res = xrLocateSpace(m_input.gazeActionSpace, m_appSpace, predictedDisplayTime, &gazeLocation);
                //Log::Write(Log::Level::Info, Fmt("gazeActionSpace pose(%f %f %f)  orientation(%f %f %f %f)", 
                //                                gazeLocation.pose.position.x, gazeLocation.pose.position.y, gazeLocation.pose.position.z, 
                //                                gazeLocation.pose.orientation.x, gazeLocation.pose.orientation.y, gazeLocation.pose.orientation.z, gazeLocation.pose.orientation.w));
                m_application->setGazeLocation(gazeLocation, m_views, ipd, res);
            }
        }

        //hand tracking
        XrHandJointLocationEXT jointLocations[Side::COUNT][XR_HAND_JOINT_COUNT_EXT];
        for (auto hand : {Side::LEFT, Side::RIGHT}) {
            XrHandJointLocationsEXT locations{XR_TYPE_HAND_JOINT_LOCATIONS_EXT};
            locations.jointCount = XR_HAND_JOINT_COUNT_EXT;
            locations.jointLocations = jointLocations[hand];

            XrHandJointsLocateInfoEXT locateInfo{XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT};
            locateInfo.baseSpace = m_appSpace;
            locateInfo.time = predictedDisplayTime;
            XrResult res = xrLocateHandJointsEXT(m_handTracker[hand], &locateInfo, &locations);
            if (res != XR_SUCCESS) {
                Log::Write(Log::Level::Error, Fmt("m_pfnXrLocateHandJointsEXT res %d", res));
            }
        }
        XrHandJointLocationEXT& leftIndexTip = jointLocations[Side::LEFT][XR_HAND_JOINT_INDEX_TIP_EXT];
        XrHandJointLocationEXT& rightIndexTip = jointLocations[Side::RIGHT][XR_HAND_JOINT_INDEX_TIP_EXT];
        //Log::Write(Log::Level::Error, Fmt("leftIndexTip.locationFlags:%d, rightIndexTip.locationFlags %d", leftIndexTip.locationFlags, rightIndexTip.locationFlags));
        if ((leftIndexTip.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 && (rightIndexTip.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0) {
            XrVector3f distance;
            XrVector3f_Sub(&distance, &leftIndexTip.pose.position, &rightIndexTip.pose.position);
            float len = XrVector3f_Length(&distance);
            // bring center of index fingers to within 1cm. Probably fine for most humans, unless
            // they have huge fingers.
            if (len < 0.01f) {
                Log::Write(Log::Level::Error, Fmt("len %f", len));
            }
        }
        for (auto hand : {Side::LEFT, Side::RIGHT}) {
            for (int i = 0; i < XR_HAND_JOINT_COUNT_EXT; i++) {
                XrHandJointLocationEXT& jointLocation = jointLocations[hand][i];
                if ((jointLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0) {
                    //-------------------------------------------------------------
                    XrHandJointLocationEXT reference_position = jointLocations[hand][1];
                    XrHandJointLocationEXT reference;
                    if (i == 2 || i == 7 || i == 12 || i == 17 || i == 21) {
                        reference = jointLocations[hand][1];
                    } else {
                        reference = jointLocations[hand][i - 1];
                    }
                    if (i > 1) {
                        XrQuaternionf orientation;
                        XrQuaternionf_Multiply(&orientation, &jointLocation.pose.orientation, &reference.pose.orientation);
                        jointLocation.pose.orientation = orientation;

                        //jointLocation.pose.position.x = jointLocation.pose.position.x - reference_position.pose.position.x;
                        //jointLocation.pose.position.y = jointLocation.pose.position.y - reference_position.pose.position.y;
                        //jointLocation.pose.position.z = jointLocation.pose.position.z - reference_position.pose.position.z;
                    }
                    //--------------------------------------------------------------
                }
            }
        }
        m_application->setHandJointLocation((XrHandJointLocationEXT*)jointLocations);
        //end hand tracking

        XrSpaceVelocity velocity{XR_TYPE_SPACE_VELOCITY};
        XrSpaceLocation spaceLocation{XR_TYPE_SPACE_LOCATION, &velocity};
        res = xrLocateSpace(m_ViewSpace, m_appSpace, predictedDisplayTime, &spaceLocation);
        CHECK_XRRESULT(res, "xrLocateSpace");


        XrPosef pose[Side::COUNT];
        for (uint32_t i = 0; i < viewCountOutput; i++) {
            pose[i] = m_views[i].pose;
        }

        // Render view to the appropriate part of the swapchain image.
        for (uint32_t i = 0; i < viewCountOutput; i++) {
            // Each view has a separate swapchain which is acquired, rendered to, and released.
            const Swapchain viewSwapchain = m_swapchains[i];
            XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            uint32_t swapchainImageIndex;
            CHECK_XRCMD(xrAcquireSwapchainImage(viewSwapchain.handle, &acquireInfo, &swapchainImageIndex));

            XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            waitInfo.timeout = XR_INFINITE_DURATION;
            CHECK_XRCMD(xrWaitSwapchainImage(viewSwapchain.handle, &waitInfo));

            projectionLayerViews[i] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
            projectionLayerViews[i].pose = pose[i];
            projectionLayerViews[i].fov = m_views[i].fov;
            projectionLayerViews[i].subImage.swapchain = viewSwapchain.handle;
            projectionLayerViews[i].subImage.imageRect.offset = {0, 0};
            projectionLayerViews[i].subImage.imageRect.extent = {viewSwapchain.width, viewSwapchain.height};

            const XrSwapchainImageBaseHeader* const swapchainImage = m_swapchainImages[viewSwapchain.handle][swapchainImageIndex];

            m_graphicsPlugin->RenderView(m_application, projectionLayerViews[i], swapchainImage, m_colorSwapchainFormat, i);

            XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            CHECK_XRCMD(xrReleaseSwapchainImage(viewSwapchain.handle, &releaseInfo));
        }


        layer.space = m_appSpace;
        if (m_extentions.activePassthrough) {
            layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
        }
        layer.viewCount = (uint32_t)projectionLayerViews.size();
        layer.views = projectionLayerViews.data();
        return true;
    }


   private:
    const Options m_options;
    std::shared_ptr<IPlatformPlugin> m_platformPlugin;
    std::shared_ptr<IGraphicsPlugin> m_graphicsPlugin;
    XrInstance m_instance{XR_NULL_HANDLE};
    XrSession m_session{XR_NULL_HANDLE};
    XrSpace m_appSpace{XR_NULL_HANDLE};
    XrSystemId m_systemId{XR_NULL_SYSTEM_ID};

    std::vector<XrViewConfigurationView> m_configViews;
    std::vector<Swapchain> m_swapchains;
    std::map<XrSwapchain, std::vector<XrSwapchainImageBaseHeader*>> m_swapchainImages;
    std::vector<XrView> m_views;
    int64_t m_colorSwapchainFormat{-1};

    // Application's current lifecycle state according to the runtime
    XrSessionState m_sessionState{XR_SESSION_STATE_UNKNOWN};
    bool m_sessionRunning{false};

    XrEventDataBuffer m_eventDataBuffer;
    InputState m_input;

    XrSpace m_ViewSpace{XR_NULL_HANDLE};

    std::shared_ptr<IApplication> m_application;

    //for XR_FB_passthrough
    XrPassthroughFB m_passthrough{XR_NULL_HANDLE};
    XrPassthroughLayerFB m_passthroughLayerReconstruction{XR_NULL_HANDLE};
    XrPassthroughLayerFB m_passthroughLayerProject{XR_NULL_HANDLE};
    XrTriangleMeshFB m_triangleMesh{XR_NULL_HANDLE};
    XrGeometryInstanceFB m_geometryInstance{XR_NULL_HANDLE};
    //XR_FB_passthrough PFN
    PFN_DECLARE(xrCreatePassthroughFB);
    PFN_DECLARE(xrDestroyPassthroughFB);
    PFN_DECLARE(xrPassthroughStartFB);
    PFN_DECLARE(xrPassthroughPauseFB);
    PFN_DECLARE(xrCreatePassthroughLayerFB);
    PFN_DECLARE(xrDestroyPassthroughLayerFB);
    PFN_DECLARE(xrPassthroughLayerSetStyleFB);
    PFN_DECLARE(xrPassthroughLayerPauseFB);
    PFN_DECLARE(xrPassthroughLayerResumeFB);
    PFN_DECLARE(xrCreateGeometryInstanceFB);
    PFN_DECLARE(xrDestroyGeometryInstanceFB);
    PFN_DECLARE(xrGeometryInstanceSetTransformFB);
    // FB_triangle_mesh PFN
    PFN_DECLARE(xrCreateTriangleMeshFB);
    PFN_DECLARE(xrDestroyTriangleMeshFB);
    PFN_DECLARE(xrTriangleMeshGetVertexBufferFB);
    PFN_DECLARE(xrTriangleMeshGetIndexBufferFB);
    PFN_DECLARE(xrTriangleMeshBeginUpdateFB);
    PFN_DECLARE(xrTriangleMeshEndUpdateFB);
    PFN_DECLARE(xrTriangleMeshBeginVertexBufferUpdateFB);
    PFN_DECLARE(xrTriangleMeshEndVertexBufferUpdateFB);


    //hand tracking
    PFN_DECLARE(xrCreateHandTrackerEXT);
    PFN_DECLARE(xrDestroyHandTrackerEXT);
    PFN_DECLARE(xrLocateHandJointsEXT);
    XrHandTrackerEXT m_handTracker[Side::COUNT] = {0};

    Extentions m_extentions;

    ApplicationEvent m_applicationEvent[Side::COUNT] = {0};
    DeviceType m_deviceType;
    uint32_t m_deviceROM;

};
}  // namespace

std::shared_ptr<IOpenXrProgram> CreateOpenXrProgram(const std::shared_ptr<Options>& options,
                                                    const std::shared_ptr<IPlatformPlugin>& platformPlugin,
                                                    const std::shared_ptr<IGraphicsPlugin>& graphicsPlugin) {
    return std::make_shared<OpenXrProgram>(options, platformPlugin, graphicsPlugin);
}
