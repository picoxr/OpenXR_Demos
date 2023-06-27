/* Copyright (2021-2023) Bytedance Ltd. and/or its affiliates, All rights reserved. */
#pragma once
#include <memory>
#include "glm/glm.hpp"
#include <openxr/openxr.h>

#define CONTROLLER_EVENT_BIT_value_trigger    0x00000001
#define CONTROLLER_EVENT_BIT_value_squeeze    0x00000002
#define CONTROLLER_EVENT_BIT_value_thumbstick 0x00000004

#define CONTROLLER_EVENT_BIT_click_trigger    0x00000010
#define CONTROLLER_EVENT_BIT_click_thumbstick 0x00000020
#define CONTROLLER_EVENT_BIT_click_squeeze    0x00000040
#define CONTROLLER_EVENT_BIT_click_a          0x00000080
#define CONTROLLER_EVENT_BIT_click_b          0x00000100
#define CONTROLLER_EVENT_BIT_click_x          0x00000200
#define CONTROLLER_EVENT_BIT_click_y          0x00000400
#define CONTROLLER_EVENT_BIT_click_menu       0x00000800

#define CONTROLLER_EVENT_BIT_touch_trigger    0x00001000
#define CONTROLLER_EVENT_BIT_touch_thumbstick 0x00002000
#define CONTROLLER_EVENT_BIT_touch_a          0x00004000
#define CONTROLLER_EVENT_BIT_touch_b          0x00008000
#define CONTROLLER_EVENT_BIT_touch_x          0x00010000
#define CONTROLLER_EVENT_BIT_touch_y          0x00020000

//system and shot button cannot be use in application
typedef struct {
    uint32_t controllerEventBit;
    float trigger;         // 0.0f - 1.0f
    float squeeze;         // 0.0f - 1.0f
    float thumbstick_x;    //-1.0f - 1.0f
    float thumbstick_y;    //-1.0f - 1.0f
    bool click_trigger;    //true:donw false:up
    bool click_thumbstck;  //true:donw false:up  Surprise, the thumbstick can be pressed
    bool click_squeeze;    //true:donw false:up
    bool click_a;          //true:donw false:up
    bool click_b;          //true:donw false:up
    bool click_x;          //true:donw false:up
    bool click_y;          //true:donw false:up
    bool click_menu;       //true:donw false:up
    // seldom use
    bool touch_trigger;    //true:touch false:none touch
    bool touch_thumbstick; //true:touch false:none touch
    bool touch_a;          //true:touch false:none touch
    bool touch_b;          //true:touch false:none touch
    bool touch_x;          //true:touch false:none touch
    bool touch_y;          //true:touch false:none touch
}ApplicationEvent;

#define PFN_DECLARE(pfn) PFN_##pfn pfn = nullptr
#define PFN_INITIALIZE(pfn) CHECK_XRCMD(xrGetInstanceProcAddr(m_instance, #pfn, (PFN_xrVoidFunction*)(&pfn)))

typedef struct Extentions_tag{
    //XR_FB_display_refresh_rate
    PFN_DECLARE(xrEnumerateDisplayRefreshRatesFB);
    PFN_DECLARE(xrGetDisplayRefreshRateFB);
    PFN_DECLARE(xrRequestDisplayRefreshRateFB);

    bool activePassthrough;     //XR_FB_passthrough
    bool isSupportEyeTracking;  //eye tracking
    bool activeEyeTracking;
    
    Extentions_tag(): activePassthrough(true), isSupportEyeTracking(false), activeEyeTracking(false) {}

    void initialize(XrInstance m_instance) {
        //XR_FB_display_refresh_rate
        PFN_INITIALIZE(xrEnumerateDisplayRefreshRatesFB);
        PFN_INITIALIZE(xrGetDisplayRefreshRateFB);
        PFN_INITIALIZE(xrRequestDisplayRefreshRateFB);
    }
}Extentions;

typedef void (*hapticCallback)(void *arg, int controllerIndex, float amplitude, float seconds, float frequency);

class IApplication {
public:
    virtual ~IApplication() = default;
    virtual bool initialize(const XrInstance instance, const XrSession session, Extentions* extention) = 0;
    virtual void setHapticCallback(void* arg, hapticCallback hapticCb) = 0;
    virtual void setControllerPose(int leftright, const XrPosef& pose) = 0;
    virtual void setControllerPower(int leftright, int power) = 0;
    virtual void setGazeLocation(XrSpaceLocation& gazeLocation, std::vector<XrView>& views, float ipd, XrResult result = XR_SUCCESS) = 0;
    virtual void setHandJointLocation(XrHandJointLocationEXT* location) = 0;
    virtual void inputEvent(int leftright, const ApplicationEvent& event) = 0;
    virtual void renderFrame(const XrPosef& pose, const glm::mat4& project, const glm::mat4& view, int32_t eye) = 0;
};

struct Options;
class IGraphicsPlugin;
std::shared_ptr<IApplication> createApplication(const std::shared_ptr<struct Options>& options, const std::shared_ptr<IGraphicsPlugin>& graphicsPlugin);
