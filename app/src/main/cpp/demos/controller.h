/* Copyright (2021-2023) Bytedance Ltd. and/or its affiliates, All rights reserved. */
#pragma once
#include "model.h"
#include "ray.h"
#include "utils.h"
class Controller;
class ControllerBase {
public:
    ControllerBase(std::string name);
    ~ControllerBase();
    bool initialize();
    void setModelFile(const std::string& modelFile);
    bool loadModelFile();
    void setModel(const glm::mat4& model);
    bool render(const glm::mat4& p, const glm::mat4& v);
    glm::vec3 getRayDirection();
    
private:
    friend class Controller;
    std::shared_ptr<Model> mController;
    std::shared_ptr<Ray> mControllerRay;
    std::string mModelFile;
    glm::mat4 mProjection;
    glm::mat4 mView;
    glm::mat4 mControllerModel{};
    glm::mat4 mRayModel{};
    float mControllerDefaultScale = 0.01f;
    float mControllerRayDefaultScale = 1.0f;
};

typedef enum {
    controllerTypeNone = 0,
    controllerTypePico4,
    controllerTypeNeo3,
}ControllerType;

class Controller {
public:
	Controller();
	~Controller();

	bool initialize(const std::string& deviceModel);
    void setPowerValue(int leftright, int power);
	void setRightPowerValue(int power);
    void setLeftPowerValue(int power);
    void setModel(int leftright, const glm::mat4& m);
    void render(const glm::mat4& p, const glm::mat4& v);
    glm::vec3 getRayDirection(int leftright);

private:
    ControllerType mControllerType;
    glm::mat4 mModel[HAND_COUNT];
    std::shared_ptr<ControllerBase> mRightController;
    std::shared_ptr<ControllerBase> mLeftController;
};
