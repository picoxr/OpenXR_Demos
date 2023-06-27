/* Copyright (2021-2023) Bytedance Ltd. and/or its affiliates, All rights reserved. */
#include <dirent.h>
#include "pch.h"
#include "common.h"
#include "options.h"
#include "application.h"
#include "controller.h"
#include "hand.h"
#include <common/xr_linear.h>
#include "logger.h"
#include "gui.h"
#include "ray.h"
#include "text.h"
#include "player.h"
#include "utils.h"
#include "graphicsplugin.h"
#include "cube.h"

class Application : public IApplication {
public:
    Application(const std::shared_ptr<struct Options>& options, const std::shared_ptr<IGraphicsPlugin>& graphicsPlugin);
    virtual ~Application() override;
    virtual bool initialize(const XrInstance instance, const XrSession session, Extentions* extentions) override;
    virtual void setHapticCallback(void* arg, hapticCallback hapticCb) override;
    virtual void setControllerPose(int leftright, const XrPosef& pose) override;
    virtual void setControllerPower(int leftright, int power) override;
    virtual void setGazeLocation(XrSpaceLocation& gazeLocation, std::vector<XrView>& views, float ipd, XrResult result = XR_SUCCESS) override;
    virtual void setHandJointLocation(XrHandJointLocationEXT* location) override;
    virtual void inputEvent(int leftright, const ApplicationEvent& event) override;
    virtual void renderFrame(const XrPosef& pose, const glm::mat4& project, const glm::mat4& view, int32_t eye) override;
private:
    void layout();
    void showDashboard(const glm::mat4& project, const glm::mat4& view);
    void showDashboardController();
    void showDeviceInformation(const glm::mat4& project, const glm::mat4& view);
    void renderEyeTracking(const glm::mat4& project, const glm::mat4& view, int32_t eye);
    void renderHandTracking(const glm::mat4& project, const glm::mat4& view);
    void getAllVideoFiles(const std::string& path, std::vector<std::string>& files);
    void startPlayVideo(const std::string& file);
    void haptic(int leftright, float amplitude, float frequency, float duration/*seconds*/);
    // Calculate the angle between the vector v and the plane normal vector n
    float angleBetweenVectorAndPlane(const glm::vec3& vector, const glm::vec3& normal);


    hapticCallback mHapticCallback;
    void* mHapticCallbackArg;

private:
    std::shared_ptr<IGraphicsPlugin> mGraphicsPlugin;
    std::shared_ptr<Controller> mController;
    std::shared_ptr<Ray> mEyeTrackingRay;
    std::shared_ptr<Gui> mPanel;
    std::shared_ptr<Text> mTextRender;
    std::shared_ptr<Player> mPlayer;
    glm::mat4 mControllerModel;
    XrPosef mControllerPose[HAND_COUNT];
    std::shared_ptr<CubeRender> mCubeRender;

    //openxr
    XrInstance m_instance;          //Keep the same naming as openxr_program.cpp
    XrSession m_session;
    Extentions* m_extentions;
    XrSpaceLocation m_gazeLocation;
    std::vector<XrView> m_views;
    float mIpd;
    XrHandJointLocationEXT m_jointLocations[HAND_COUNT][XR_HAND_JOINT_COUNT_EXT];

    //app data
    std::string mDeviceModel;
    std::string mDeviceOS;

    bool mIsShowDashboard = true;

    std::vector<std::string> mAllVideoFiles;
    int32_t mCount = 0;

    const ApplicationEvent *mControllerEvent[HAND_COUNT];

};

std::shared_ptr<IApplication> createApplication(const std::shared_ptr<struct Options>& options, const std::shared_ptr<IGraphicsPlugin>& graphicsPlugin) {
    return std::make_shared<Application>(options, graphicsPlugin);
}

Application::Application(const std::shared_ptr<struct Options>& options, const std::shared_ptr<IGraphicsPlugin>& graphicsPlugin) {
    mGraphicsPlugin = graphicsPlugin;
    mController = std::make_shared<Controller>();
    mEyeTrackingRay = std::make_shared<Ray>();
    mPanel = std::make_shared<Gui>("dashboard");
    mTextRender = std::make_shared<Text>();
    mPlayer = std::make_shared<Player>();
    mHapticCallback = nullptr;
    mCubeRender = std::make_shared<CubeRender>();
}

Application::~Application() {
}

void Application::getAllVideoFiles(const std::string& path, std::vector<std::string>& allFiles) {
    DIR *dir = opendir(path.c_str());
    if (dir == nullptr) {
        errorf("opendir %s error %d", path.c_str(), errno);
        return;
    }
    struct dirent *file;
    while ((file = readdir(dir)) != nullptr) {
        if (strcmp(file->d_name, ".") == 0 || strcmp(file->d_name, "..") == 0) {
            continue;
        }
        if (file->d_type == DT_DIR) {
            std::string path_next = path + "/" + file->d_name;
            getAllVideoFiles(path_next, allFiles);
        } else {
            std::string fileFullName = path + "/" + file->d_name;
            std::string extension = fileFullName.substr(fileFullName.find_last_of('.') + 1);
            std::transform(extension.begin(), extension.end(), extension.begin(), [](char& c) {
                return std::tolower(c);
            });
            if (extension == "mp4" || extension == "mkv" || extension == "avi") {
                allFiles.push_back(fileFullName);
            }
            //infof("count:%d file:%s", mCount++, fileFullName.c_str());
        }
    }
}

bool Application::initialize(const XrInstance instance, const XrSession session, Extentions* extentions) {
    m_instance = instance;
    m_session = session;
    m_extentions = extentions;

    // get device model
    char buffer[64] = {0};
    __system_property_get("sys.pxr.product.name", buffer);
    mDeviceModel = buffer;

    //get OS version
    __system_property_get("ro.build.id", buffer);
    //__system_property_get("ro.system.build.id", buffer); // You can also call this function, the result is the same
    mDeviceOS = buffer;

    mController->initialize(mDeviceModel);
    mEyeTrackingRay->initialize();
    mPanel->initialize(600, 800);  //set resolution
    mTextRender->initialize();
    mCubeRender->initialize();

    const XrGraphicsBindingOpenGLESAndroidKHR *binding = reinterpret_cast<const XrGraphicsBindingOpenGLESAndroidKHR*>(mGraphicsPlugin->GetGraphicsBinding());
    mPlayer->initialize(binding->display);

    getAllVideoFiles("/sdcard", mAllVideoFiles);

    //copyFile("/sdcard/Pictures/Screenshots/20230426-105301.jpg", "/sdcard/Pictures/2.jpg");
    //refreshMedia("/sdcard/Pictures/");

    return true;
}

void Application::setHapticCallback(void* arg, hapticCallback hapticCb) {
    mHapticCallbackArg = arg;
    mHapticCallback = hapticCb;
}

void Application::setControllerPower(int leftright, int power) {
    mController->setPowerValue(leftright, power);
}

void Application::setControllerPose(int leftright, const XrPosef& pose) {
    XrMatrix4x4f model{};
    XrVector3f scale{1.0f, 1.0f, 1.0f};
    XrMatrix4x4f_CreateTranslationRotationScale(&model, &pose.position, &pose.orientation, &scale);
    glm::mat4 m = glm::make_mat4((float*)&model);
    mController->setModel(leftright, m);
    mControllerPose[leftright] = pose;
}

void Application::setGazeLocation(XrSpaceLocation& gazeLocation, std::vector<XrView>& views, float ipd, XrResult result) {
    mIpd = ipd;
    memcpy(&m_gazeLocation, &gazeLocation, sizeof(gazeLocation));
    m_views = views;
}

void Application::setHandJointLocation(XrHandJointLocationEXT* location) {
    memcpy(&m_jointLocations, location, sizeof(m_jointLocations));
}

void Application::startPlayVideo(const std::string& file) {
    //mPlayer->stop();
    mPlayer->start(file);
}

void Application::inputEvent(int leftright, const ApplicationEvent& event) {
    mControllerEvent[leftright] = &event;

    if (event.controllerEventBit & CONTROLLER_EVENT_BIT_click_menu) {
        if (event.click_menu == true) {
            mIsShowDashboard = !mIsShowDashboard;
        }
    }

    if (leftright == HAND_LEFT) {
        return;
    }
    if (event.controllerEventBit & CONTROLLER_EVENT_BIT_click_trigger) {
        //infof("controllerEventBit:0x%02x, event.click_trigger:0x%d", event.controllerEventBit, event.click_trigger);
        mPanel->triggerEvent(event.click_trigger);
    }

}

void Application::layout() {
    glm::mat4 model = glm::mat4(1.0f);
    float scale = 1.0f;

    float width, height;
    mPanel->getWidthHeight(width, height);
    scale = 0.7;
    model = glm::translate(model, glm::vec3(-0.0f, -0.3f, -1.0f));
    model = glm::rotate(model, glm::radians(10.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(scale * (width / height), scale, 1.0f));
    mPanel->setModel(model);

    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(1.0f, -0.0f, -1.5f));
    model = glm::rotate(model, glm::radians(-20.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(scale*2, scale, 1.0f));
    mPlayer->setModel(model);
}

void Application::haptic(int leftright, float amplitude, float frequency/*not used now*/, float duration/*seconds*/) {
    if (mHapticCallback) {
        mHapticCallback(mHapticCallbackArg, leftright, amplitude, frequency, duration);
    }
}

void Application::showDashboardController() {
#define HAND_BIT_LEFT HAND_LEFT+1
#define HAND_BIT_RIGHT HAND_RIGHT+1
#define SHOW_CONTROLLER_ROW_float(x)    ImGui::TableNextRow();\
                                        ImGui::TableNextColumn();\
                                        ImGui::Text("%s", MEMBER_NAME(ApplicationEvent, x));\
                                        ImGui::TableNextColumn();\
                                        ImGui::Text("%f", mControllerEvent[HAND_LEFT]->x);\
                                        ImGui::TableNextColumn();\
                                        ImGui::Text("%f", mControllerEvent[HAND_RIGHT]->x);

#define SHOW_CONTROLLER_ROW_bool(hand, x)   ImGui::TableNextRow();\
                                            ImGui::TableNextColumn();\
                                            ImGui::Text("%s", MEMBER_NAME(ApplicationEvent, x));\
                                            ImGui::TableNextColumn();\
                                            if (hand & HAND_BIT_LEFT && mControllerEvent[HAND_LEFT]->x) {\
                                                ImGui::Text("true");\
                                            }\
                                            ImGui::TableNextColumn();\
                                            if (hand & HAND_BIT_RIGHT && mControllerEvent[HAND_RIGHT]->x) {\
                                                ImGui::Text("true");\
                                            }  

    //controller event
    if (ImGui::CollapsingHeader("controller")) {
        const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;
        const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();
        static ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY;
        if (ImGui::BeginTable("controller event", 3, flags, ImVec2(0.0f, TEXT_BASE_HEIGHT * 19), 0.0f)) {
            ImGui::TableSetupColumn("event name",        ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed,   0.0f);
            ImGui::TableSetupColumn("left controller",   ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed,   0.0f);
            ImGui::TableSetupColumn("right controller",  ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthStretch, 0.0f);
            ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
            ImGui::TableHeadersRow();

            SHOW_CONTROLLER_ROW_float(trigger);
            SHOW_CONTROLLER_ROW_bool(HAND_BIT_LEFT + HAND_BIT_RIGHT, click_trigger);
            SHOW_CONTROLLER_ROW_bool(HAND_BIT_LEFT + HAND_BIT_RIGHT, touch_trigger);

            SHOW_CONTROLLER_ROW_float(thumbstick_x);
            SHOW_CONTROLLER_ROW_float(thumbstick_y);
            SHOW_CONTROLLER_ROW_bool(HAND_BIT_LEFT + HAND_BIT_RIGHT, click_thumbstck);
            SHOW_CONTROLLER_ROW_bool(HAND_BIT_LEFT + HAND_BIT_RIGHT, touch_thumbstick);

            SHOW_CONTROLLER_ROW_float(squeeze);
            SHOW_CONTROLLER_ROW_bool(HAND_BIT_LEFT + HAND_BIT_RIGHT, click_squeeze);

            SHOW_CONTROLLER_ROW_bool(HAND_BIT_RIGHT, click_a);
            SHOW_CONTROLLER_ROW_bool(HAND_BIT_RIGHT, click_b);
            SHOW_CONTROLLER_ROW_bool(HAND_BIT_LEFT, click_x);
            SHOW_CONTROLLER_ROW_bool(HAND_BIT_LEFT, click_y);
            
            SHOW_CONTROLLER_ROW_bool(HAND_BIT_RIGHT, touch_a);
            SHOW_CONTROLLER_ROW_bool(HAND_BIT_RIGHT, touch_b);
            SHOW_CONTROLLER_ROW_bool(HAND_BIT_LEFT, touch_x);
            SHOW_CONTROLLER_ROW_bool(HAND_BIT_LEFT, touch_y);

            SHOW_CONTROLLER_ROW_bool(HAND_BIT_LEFT, click_menu);

            ImGui::EndTable();
        }

        for (int i = HAND_LEFT; i < HAND_COUNT; i++) {
            if (mControllerEvent[i]->squeeze > 0) {
                haptic(i, mControllerEvent[i]->squeeze, 1.0f, 0.02);
            } else {
                haptic(i, 0.0f, 0, 0.0f);
            }
        }
    }
}

void Application::showDashboard(const glm::mat4& project, const glm::mat4& view) {

    // get refreshrate
    uint32_t count = 0;
    m_extentions->xrEnumerateDisplayRefreshRatesFB(m_session, 0, &count, nullptr);
    std::vector<float> refreshRate(count);
    m_extentions->xrEnumerateDisplayRefreshRatesFB(m_session, count, &count, refreshRate.data());
    float currentreFreshRate = 0;
    m_extentions->xrGetDisplayRefreshRateFB(m_session, &currentreFreshRate);
    int currentreFreshRateTmp = (int)currentreFreshRate;

    PlayModel playModel = mPlayer->getPlayStyle();

    const XrPosef& controllerPose = mControllerPose[1];
    glm::vec3 linePoint = glm::make_vec3((float*)&controllerPose.position);
    glm::vec3 lineDirection = mController->getRayDirection(1);
    mPanel->isIntersectWithLine(linePoint, lineDirection);

    mPanel->begin();
    if (ImGui::CollapsingHeader("information")) {
        ImGui::BulletText("device model: %s", mDeviceModel.c_str());
        ImGui::BulletText("device OS: %s", mDeviceOS.c_str());
    }
    
    //test controller
    showDashboardController();

    if (ImGui::CollapsingHeader("framerate")) {
        ImGui::RadioButton("72fps", (int*)&currentreFreshRateTmp, 72); 
        if (refreshRate.size() > 1 || currentreFreshRateTmp == 92) {
            ImGui::SameLine();
            ImGui::RadioButton("90fps", (int*)&currentreFreshRateTmp, 90); 
        }
    }

    if (ImGui::CollapsingHeader("sample options")) {
        if (ImGui::BeginTable("split", 2)) {
            ImGui::TableNextColumn(); ImGui::Checkbox("XR_FB_passthrough", &m_extentions->activePassthrough);
            if (m_extentions->isSupportEyeTracking) {
                ImGui::TableNextColumn(); ImGui::Checkbox("Eye Tracking", &m_extentions->activeEyeTracking);
            }
            ImGui::EndTable();
        }
    }

    int32_t selectFileIndex = -1;
    if (ImGui::CollapsingHeader("video player")) {
        ImGui::SeparatorText("play model");
        ImGui::RadioButton("2D",         (int*)&playModel, (int)playModel_2D); ImGui::SameLine();
        ImGui::RadioButton("2D-180",     (int*)&playModel, (int)playModel_2D_180); ImGui::SameLine();
        ImGui::RadioButton("2D-360",     (int*)&playModel, (int)playModel_2D_360); ImGui::SameLine();
        ImGui::RadioButton("3D-SBS",     (int*)&playModel, (int)playModel_3D_SBS); ImGui::SameLine();
        ImGui::RadioButton("3D-SBS-360", (int*)&playModel, (int)playModel_3D_SBS_360); ImGui::SameLine();
        ImGui::RadioButton("3D-OU",      (int*)&playModel, (int)playModel_3D_OU); ImGui::SameLine();
        ImGui::RadioButton("3D-OU-360",  (int*)&playModel, (int)playModel_3D_OU_360);

        if (ImGui::CollapsingHeader("select media file")) {
            const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;
            const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();
            static ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY;
            if (ImGui::BeginTable("meida files", 2, flags, ImVec2(0.0f, TEXT_BASE_HEIGHT * 11), 0.0f)) {
                ImGui::TableSetupColumn("ID",   ImGuiTableColumnFlags_NoSort     | ImGuiTableColumnFlags_WidthFixed,   0.0f);
                ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_NoSort     | ImGuiTableColumnFlags_WidthStretch, 0.0f);
                ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
                ImGui::TableHeadersRow();
                for (int32_t i = 0; i < mAllVideoFiles.size(); i++) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(Fmt("%02d", i).c_str(), true, ImGuiSelectableFlags_SpanAllColumns)) {
                        selectFileIndex = i;
                    }
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", mAllVideoFiles[i].c_str());
                }
                ImGui::EndTable();
            }
        }
    }
    if (selectFileIndex != -1) {
        infof("item:%d, exchange video file %s", selectFileIndex, mAllVideoFiles[selectFileIndex].c_str());
        startPlayVideo(mAllVideoFiles[selectFileIndex]);
    }

    ImGui::Text("This is some useful text.");

    mPanel->end();
    mPanel->render(project, view);

    mPlayer->setPlayStyle(playModel);

    if (currentreFreshRateTmp != (int)currentreFreshRate) {  //changed
        m_extentions->xrRequestDisplayRefreshRateFB(m_session, (float)currentreFreshRateTmp);
    }
}

void Application::showDeviceInformation(const glm::mat4& project, const glm::mat4& view) {
    wchar_t text[1024] = {0};
    swprintf(text, 1024, L"model: %s, OS: %s", mDeviceModel.c_str(), mDeviceOS.c_str());

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.5f, -0.6f, -1.0f));
    model = glm::rotate(model, glm::radians(-30.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(0.5, 0.5, 1.0f));
    mTextRender->render(project, view, model, text, wcslen(text), glm::vec3(1.0, 1.0, 1.0));
}

float Application::angleBetweenVectorAndPlane(const glm::vec3& vector, const glm::vec3& normal) {
    float dotProduct = glm::dot(vector, normal);
    float lengthVector = glm::length(vector);
    float lengthNormal = glm::length(normal);
    if (lengthNormal != 1.0f) {
        lengthNormal = 1.0f;  //normalnize
    }
    float cosAngle = dotProduct / (lengthVector * lengthNormal);
    float angleRadians = std::acos(cosAngle);
    //Convert radians to degrees
    //float angleInDegrees = glm::degrees(angleRadians);
    return PI/2 - angleRadians;
}

void Application::renderEyeTracking(const glm::mat4& project, const glm::mat4& view, int32_t eye) {
    if (m_extentions->isSupportEyeTracking && m_extentions->activeEyeTracking) {
        if (m_views.size() == 0) {
            return;
        }

        XrMatrix4x4f m{};
        XrVector3f scale{1.0f, 1.0f, 1.0f};
        XrMatrix4x4f_CreateTranslationRotationScale(&m, &m_gazeLocation.pose.position, &m_gazeLocation.pose.orientation, &scale);
        glm::mat4 model = glm::make_mat4((float*)&m);
        float halfIpd = mIpd / 2;
        if (eye == EYE_LEFT) {
            halfIpd = 0 - halfIpd;
        }
        model = glm::translate(model, glm::vec3(halfIpd, 0.0f, -0.2f));
        model = glm::scale(model, glm::vec3(1.0, 1.0, 1.0f));
        mEyeTrackingRay->setColor(1.0f, 0.0f, 0.0f);

        //Maps the direction of eye gaze to a point on the screen (x, y) in percentage
        
        glm::vec3 direction = mEyeTrackingRay->getDirectionVector(model);

        XrMatrix4x4f m2{};
        XrMatrix4x4f_CreateTranslationRotationScale(&m2, &m_views[eye].pose.position, &m_views[eye].pose.orientation, &scale);
        glm::mat4 model2 = glm::make_mat4((float*)&m2);

        glm::vec3 pointO = glm::vec3(model2 * glm::vec4(0.0, 0.0, -1.0, 1.0f));
        glm::vec3 pointX = glm::vec3(model2 * glm::vec4(1.0, 0.0, -1.0, 1.0f));
        glm::vec3 pointY = glm::vec3(model2 * glm::vec4(0.0, 1.0, -1.0, 1.0f));
        glm::vec3 normalYOZ = pointX - pointO;
        glm::vec3 normalXOZ = pointY - pointO;

        float angleAndYOZ = angleBetweenVectorAndPlane(direction, normalYOZ);
        float angleAndXOZ = angleBetweenVectorAndPlane(direction, normalXOZ);

        float x, y;
        if (angleAndYOZ < 0) {
            x = (1 - tanf(angleAndYOZ) / tanf(m_views[eye].fov.angleLeft)) * 0.5;
        } else {
            x = (1 + tanf(angleAndYOZ) / tanf(m_views[eye].fov.angleRight)) * 0.5;
        }
        if (angleAndXOZ) {
            y = (1 - tanf(angleAndXOZ) / tanf(m_views[eye].fov.angleUp)) * 0.5;
        } else {
            y = (1 + tanf(angleAndXOZ) / tanf(m_views[eye].fov.angleDown)) * 0.5;
        }

        //infof("angleAndYOZ:%f, angleAndXOZ:%f", angleAndYOZ, angleAndXOZ);

        mEyeTrackingRay->render(project, view, model);

        // show the coordinates
        wchar_t text[1024] = {0};
        swprintf(text, 1024, L"x:%0.2f, y:%0.2f", x, y);
        model = glm::translate(model, glm::vec3(-0.2f, 0.0f, -1.5f));
        model = glm::scale(model, glm::vec3(0.5, 0.5, 0.5f));
        mTextRender->render(project, view, model, text, wcslen(text), glm::vec3(1.0, 1.0, 1.0));
    }
}

void Application::renderHandTracking(const glm::mat4& project, const glm::mat4& view) {
    std::vector<CubeRender::Cube> cubes;
    for (auto hand = 0; hand < HAND_COUNT; hand++) {
        for (int i = 0; i < XR_HAND_JOINT_COUNT_EXT; i++) {
            XrHandJointLocationEXT& jointLocation = m_jointLocations[hand][i];
            if (jointLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT && jointLocation.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT) {

                XrMatrix4x4f m{};
                XrVector3f scale{1.0f, 1.0f, 1.0f};
                XrMatrix4x4f_CreateTranslationRotationScale(&m, &jointLocation.pose.position, &jointLocation.pose.orientation, &scale);
                glm::mat4 model = glm::make_mat4((float*)&m);

                CubeRender::Cube cube;
                cube.model = model;
                cube.scale = 0.01f;
                cubes.push_back(cube);
            }
        }
    }
    mCubeRender->render(project, view, cubes);
}

void Application::renderFrame(const XrPosef& pose, const glm::mat4& project, const glm::mat4& view, int32_t eye) {
    layout();
    showDeviceInformation(project, view);

    mPlayer->render(project, view, eye);

    if (mIsShowDashboard) {
        showDashboard(project, view);
    }

    renderEyeTracking(project, view, eye);
    
    mController->render(project, view);

    renderHandTracking(project, view);

}
