#include "hand.h"

HandBase::HandBase(std::string name) {
    mHand = std::make_shared<Model>(name, true/*hasBoneInfo*/);
}
HandBase::~HandBase() { 
}
bool HandBase::initialize() {
    mHand->initialize();
    return true;
}
void HandBase::setModelFile(const std::string& modelFile) {
    mModelFile = modelFile;
}
bool HandBase::loadModelFile() {
    return mHand->loadModel(mModelFile);
}
void HandBase::setModel(const glm::mat4& model) {
    mModel = model;
}
bool HandBase::render(const glm::mat4& p, const glm::mat4& v) {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::scale(mModel, glm::vec3(mDefaultScale, mDefaultScale, mDefaultScale));
    mHand->render(p, v, model);
    return true;
}
////////////////////////////////////////////////////////////////////////////////
Hand::Hand() {
    mRightHand = std::make_shared<HandBase>("right_hand");
    mLeftHand = std::make_shared<HandBase>("left_hand");
}

Hand::~Hand() {
}

bool Hand::initialize() {
    mLeftHand->initialize();
    mRightHand->initialize();

    mLeftHand->setModelFile("hand/Hand_L.fbx");
    mRightHand->setModelFile("hand/Hand_R.fbx");

    mLeftHand->mHand->bindMeshTexture("l_handMesh", "hand/0.png");
    mRightHand->mHand->bindMeshTexture("r_handMesh", "hand/0.png");

    mLeftHand->loadModelFile();
    mRightHand->loadModelFile();

    mLeftHand->mHand->activeMeshTexture("l_handMesh", "hand/0.png");
    mRightHand->mHand->activeMeshTexture("r_handMesh", "hand/0.png");

    return true;
}

void Hand::setModel(int leftright, const glm::mat4& m) {
    if (leftright == HAND_LEFT) {
        mModel[HAND_LEFT] = m;
        mLeftHand->setModel(m);
    } else {
        mModel[HAND_RIGHT] = m;
        mRightHand->setModel(m);
    }
}

void Hand::render(const glm::mat4& p, const glm::mat4& v) {
    mLeftHand->render(p, v);
    mRightHand->render(p, v);
}

void Hand::render(int leftright, const glm::mat4& p, const glm::mat4& v) {
    leftright == HAND_RIGHT ? mRightHand->render(p, v) : mLeftHand->render(p, v);
}

void Hand::setBoneNodeMatrices(int leftright, const std::string& bone, const glm::mat4& m) {
    leftright == HAND_RIGHT ? mRightHand->mHand->setBoneNodeMatrices(bone, m) : mLeftHand->mHand->setBoneNodeMatrices(bone, m);
}