/* Copyright (2021-2023) Bytedance Ltd. and/or its affiliates, All rights reserved. */
#include <memory>
#include "controller.h"


ControllerBase::ControllerBase(std::string name) {
    mController = std::make_shared<Model>(name);
    mControllerRay = std::make_shared<Ray>();
}
ControllerBase::~ControllerBase() { 
}
bool ControllerBase::initialize() {
    mController->initialize();
    mControllerRay->initialize();
    return true;
}
void ControllerBase::setModelFile(const std::string& modelFile) {
    mModelFile = modelFile;
}
bool ControllerBase::loadModelFile() {
    return mController->loadModel(mModelFile);
}
void ControllerBase::setModel(const glm::mat4& model) {
    mControllerModel = model;
    mRayModel = model;
}
bool ControllerBase::render(const glm::mat4& p, const glm::mat4& v) {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::scale(mControllerModel, glm::vec3(mControllerDefaultScale, mControllerDefaultScale, mControllerDefaultScale));
    mController->render(p, v, model);

    model = glm::mat4(1.0f);
    model = glm::scale(mControllerModel, glm::vec3(mControllerRayDefaultScale, mControllerRayDefaultScale, mControllerRayDefaultScale));
    mControllerRay->render(p, v, model);
    return true;
}
glm::vec3 ControllerBase::getRayDirection() {
    std::vector<glm::vec3> raypoints = mControllerRay->getPoints();
    if (raypoints.size() > 1) {
        glm::vec4 p1 = mRayModel * glm::vec4(raypoints[0], 1.0f);
        glm::vec4 p2 = mRayModel * glm::vec4(raypoints[1], 1.0f);
        return glm::normalize(glm::vec3(p2 - p1));
    }
    return glm::vec3(0.0f, 0.0f, 0.0f);
}

/// @brief /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Controller::Controller() {
    mRightController = std::make_shared<ControllerBase>("right");
    mLeftController = std::make_shared<ControllerBase>("left");
    mControllerType = controllerTypeNone;
}

Controller::~Controller() {
}

bool Controller::initialize(const std::string& deviceModel) {

    mRightController->initialize();
    mLeftController->initialize();

    std::string device = deviceModel;
    std::transform(device.begin(), device.end(), device.begin(), [](char& c) {
        return std::tolower(c);
    });
    if (device.find("Neo3") != std::string::npos) {
        mControllerType = controllerTypeNeo3;
    } else {
        mControllerType = controllerTypePico4;
    }

    if (mControllerType == controllerTypeNeo3) {
        mRightController->setModelFile("neo3_controller/Neo3_Controller_Right.fbx");
        mLeftController->setModelFile("neo3_controller/Neo3_Controller_Left.fbx");

        //mesh 名称需要提前得知 
        mRightController->mController->bindMeshTexture("PICO3_Right", "neo3_controller/controller5_idle.jpg");
        //mRightController->mController->bindMeshTexture("Power", "pico4_controller/PICO4_ControllerPower_01.png");
        //mRightController->mController->bindMeshTexture("Power", "pico4_controller/PICO4_ControllerPower_02.png");
        //mRightController->mController->bindMeshTexture("Power", "pico4_controller/PICO4_ControllerPower_03.png");
        //mRightController->mController->bindMeshTexture("Power", "pico4_controller/PICO4_ControllerPower_04.png");
        //mRightController->mController->bindMeshTexture("Power", "pico4_controller/PICO4_ControllerPower_05.png");
        mRightController->loadModelFile();
        mRightController->mController->activeMeshTexture("PICO3_Right", "neo3_controller/controller5_idle.jpg");

        mLeftController->mController->bindMeshTexture("PICO3_Left", "neo3_controller/controller5_idle.jpg");
        //mLeftController->mController->bindMeshTexture("Power", "pico4_controller/PICO4_ControllerPower_01.png");
        //mLeftController->mController->bindMeshTexture("Power", "pico4_controller/PICO4_ControllerPower_02.png");
        //mLeftController->mController->bindMeshTexture("Power", "pico4_controller/PICO4_ControllerPower_03.png");
        //mLeftController->mController->bindMeshTexture("Power", "pico4_controller/PICO4_ControllerPower_04.png");
        //mLeftController->mController->bindMeshTexture("Power", "pico4_controller/PICO4_ControllerPower_05.png");
        mLeftController->loadModelFile();
        mLeftController->mController->activeMeshTexture("PICO3_Left", "neo3_controller/controller5_idle.jpg");

    } else {
        mRightController->setModelFile("pico4_controller/PICO4_Controller_Right.fbx");
        mLeftController->setModelFile("pico4_controller/PICO4_Controller_Left.fbx");

        mRightController->mController->bindMeshTexture("Controller", "pico4_controller/PICO4_Controller_Albedo.png");
        mRightController->mController->bindMeshTexture("Power", "pico4_controller/PICO4_ControllerPower_01.png");
        mRightController->mController->bindMeshTexture("Power", "pico4_controller/PICO4_ControllerPower_02.png");
        mRightController->mController->bindMeshTexture("Power", "pico4_controller/PICO4_ControllerPower_03.png");
        mRightController->mController->bindMeshTexture("Power", "pico4_controller/PICO4_ControllerPower_04.png");
        mRightController->mController->bindMeshTexture("Power", "pico4_controller/PICO4_ControllerPower_05.png");
        mRightController->loadModelFile();
        mRightController->mController->activeMeshTexture("Controller", "pico4_controller/PICO4_Controller_Albedo.png");
        
        mLeftController->mController->bindMeshTexture("Controller", "pico4_controller/PICO4_Controller_Albedo.png");
        mLeftController->mController->bindMeshTexture("Power", "pico4_controller/PICO4_ControllerPower_01.png");
        mLeftController->mController->bindMeshTexture("Power", "pico4_controller/PICO4_ControllerPower_02.png");
        mLeftController->mController->bindMeshTexture("Power", "pico4_controller/PICO4_ControllerPower_03.png");
        mLeftController->mController->bindMeshTexture("Power", "pico4_controller/PICO4_ControllerPower_04.png");
        mLeftController->mController->bindMeshTexture("Power", "pico4_controller/PICO4_ControllerPower_05.png");
        mLeftController->loadModelFile();
        mLeftController->mController->activeMeshTexture("Controller", "pico4_controller/PICO4_Controller_Albedo.png");
    }

    setRightPowerValue(79);
    setLeftPowerValue(81);
	return true;
}

void Controller::setRightPowerValue(int power) {
    if (mControllerType == controllerTypeNeo3) {
        return;
    }
    int p = (100 - power)/ 20 + 1;
    p = p > 5 ? 5 : p;
    std::string textureName = "pico4_controller/PICO4_ControllerPower_0" + std::to_string(p) + ".png";
    mRightController->mController->activeMeshTexture("Power", textureName);
}

void Controller::setLeftPowerValue(int power) {
    if (mControllerType == controllerTypeNeo3) {
        return;
    }
    int p = (100 - power)/ 20 + 1;
    p = p > 5 ? 5 : p;
    std::string textureName = "pico4_controller/PICO4_ControllerPower_0" + std::to_string(p) + ".png";
    mLeftController->mController->activeMeshTexture("Power", textureName);
}

void Controller::setPowerValue(int leftright, int power) {
    if (leftright == HAND_LEFT) {
        setLeftPowerValue(power);
    } else {
        setRightPowerValue(power);
    }
}

void Controller::setModel(int leftright, const glm::mat4& m) {
    if (leftright == HAND_LEFT) {
        mModel[HAND_LEFT] = m;
        mLeftController->setModel(m);
    } else {
        mModel[HAND_RIGHT] = m;
        mRightController->setModel(m);
    }
}

void Controller::render(const glm::mat4& p, const glm::mat4& v) {
    mLeftController->render(p, v);
    mRightController->render(p, v);
}

glm::vec3 Controller::getRayDirection(int leftright) {
    if (leftright == HAND_LEFT) {
        return mLeftController->getRayDirection();
    } else {
        return mRightController->getRayDirection();
    }
}
