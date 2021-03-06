//
// Copyright (c) 2018 The WebGLNativePorts Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Aquarium.cpp: Create context for specific graphics API.
// Data preparation, load vertex and index buffer, images and shders.
// Implements logic of rendering background, fishes, seaweeds and
// other models. Calculate fish count for each type of fish.
// Update uniforms for each frame.

#include <fstream>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <direct.h>
#include "Windows.h"
#elif __APPLE__
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

#include <algorithm>
#include <cmath>

#include "ASSERT.h"
#include "Aquarium.h"
#include "ContextFactory.h"
#include "FishModel.h"
#include "Matrix.h"
#include "Program.h"
#include "SeaweedModel.h"
#include "Texture.h"
#include "OpenGL/ContextGL.h"
#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

static const char *shaderFolder = "shaders";
static const char *resourceFolder = "assets";

Aquarium::Aquarium()
    : mModelEnumMap(),
      mTextureMap(),
      mProgramMap(),
      mAquariumModels(),
      context(nullptr),
      fpsTimer(),
      mFishCount(1),
      mBackendpath(""),
      mShaderVersion(""),
      mPath(""),
      factory(nullptr),
      enableMSAA(false)
{
    g.then = 0.0f;
    g.mclock = 0.0f;
    g.eyeClock = 0.0f;

    lightUniforms.lightColor[0] = 1.0f;
    lightUniforms.lightColor[1] = 1.0f;
    lightUniforms.lightColor[2] = 1.0f;
    lightUniforms.lightColor[3] = 1.0f;

    lightUniforms.specular[0] = 1.0f;
    lightUniforms.specular[1] = 1.0f;
    lightUniforms.specular[2] = 1.0f;
    lightUniforms.specular[3] = 1.0f;

    fogUniforms.fogColor[0] = g_fogRed;
    fogUniforms.fogColor[1] = g_fogGreen;
    fogUniforms.fogColor[2] = g_fogBlue;
    fogUniforms.fogColor[3] = 1.0f;

    fogUniforms.fogPower    = g_fogPower;
    fogUniforms.fogMult     = g_fogMult;
    fogUniforms.fogOffset   = g_fogOffset;

    lightUniforms.ambient[0] = g_ambientRed;
    lightUniforms.ambient[1] = g_ambientGreen;
    lightUniforms.ambient[2] = g_ambientBlue;
    lightUniforms.ambient[3] = 0.0f;
}

Aquarium::~Aquarium()
{
    for (auto &tex : mTextureMap)
    {
        if (tex.second != nullptr)
        {
            delete tex.second;
            tex.second = nullptr;
        }
    }

    for (auto &program : mProgramMap)
    {
        if (program.second != nullptr)
        {
            delete program.second;
            program.second = nullptr;
        }
    }

    for (int i = 0; i < MODELNAME::MODELMAX; ++i)
    {
        delete mAquariumModels[i];
    }

    delete factory;
}

void Aquarium::init(int argc, char **argv)
{
    factory = new ContextFactory();

    // Create context of different backends through the cmd args.
    // "--backend" {backend}: create different backends. currently opengl is supported.
    // "--num-fish" {numfish}: imply rendering fish count.
    char* pNext;
    for (int i = 1; i < argc; ++i)
    {
        std::string cmd(argv[i]);
        if (cmd == "--num-fish")
        {
            mFishCount = strtol(argv[i++ + 1], &pNext, 10);
        }
        else if (cmd == "--backend")
        {
            mBackendFullpath = argv[i++ + 1];
            if (mBackendFullpath.find("dawn") != std::string::npos)
            {
                mBackendpath = "dawn";
            } else
            {
                mBackendpath = mBackendFullpath;
            }
            context      = factory->createContext(mBackendpath);
        }
        else if (cmd == "--enable-msaa")
        {
            enableMSAA = true;
        }
        else
        {
        }
    }

    if (context == nullptr)
    {
        mBackendpath = "opengl";
        context      = factory->createContext(mBackendFullpath);
    }

    if (mBackendpath == "opengl")
    {
        #ifndef EGL_EGL_PROTOTYPES
        mShaderVersion = "450";
        #else
        mShaderVersion = "100";
        #endif
    }

    if (!context->createContext(mBackendFullpath, enableMSAA))
    {
        return;
    }

    // Init general buffer and binding groups for dawn backend.
    context->initGeneralResources(this);

    setupModelEnumMap();

    loadReource();

    calculateFishCount();
}

void Aquarium::display()
{
    while (!context->ShouldQuit())
    {
        context->KeyBoardQuit();
        render();

        context->DoFlush();
    }

    context->Terminate();
}

void Aquarium::updateUrls()
{
    // Get path of current build.
    #if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
    TCHAR temp[200];
    GetModuleFileName(NULL, temp, MAX_PATH);
    std::wstring ws(temp);
    mPath = std::string(ws.begin(), ws.end());
    size_t nPos = mPath.find_last_of(slash);
    mPath = mPath.substr(0, nPos) + slash + ".." + slash + ".." + slash;
    #elif __APPLE__
    char temp[200];
    uint32_t size = sizeof(temp);
    _NSGetExecutablePath(temp, &size);
    mPath             = std::string(temp);
    int nPos = mPath.find_last_of(slash);
    mPath = mPath.substr(0, nPos) + slash + ".." + slash + ".." + slash;
    #else
    char temp[200];
    readlink("/proc/self/exe", temp, sizeof(temp));
    mPath             = std::string(temp);
    int nPos = mPath.find_last_of(slash);
    mPath = mPath.substr(0, nPos) + slash + ".." + slash + ".." + slash;
    #endif
}

void Aquarium::setUpSkyBox(std::vector<std::string> *skyUrls)
{
    for (const auto v : g_skyBoxUrls)
    {
        std::ostringstream url;
        //url << mPath << ".." << slash << resourceFolder << slash
        url << mPath << resourceFolder << slash
        << v;

        skyUrls->push_back(url.str());
    }
}

void Aquarium::loadReource()
{
    updateUrls();

    // set up skybox
    std::vector<std::string> skyUrls;
    setUpSkyBox(&skyUrls);
    mTextureMap["skybox"] = context->createTexture("skybox", skyUrls);

    loadModels();
    loadPlacement();
}

void Aquarium::setupModelEnumMap()
{
    for (auto &info : g_sceneInfo)
    {
        mModelEnumMap[info.namestr] = info.name;
    }
}

// Load world matrices of models from json file.
void Aquarium::loadPlacement()
{
    std::ostringstream oss;
    oss << mPath <<  resourceFolder << slash
        << "PropPlacement.js";
    std::string proppath = oss.str();
    std::ifstream PlacementStream(proppath, std::ios::in);
    rapidjson::IStreamWrapper isPlacement(PlacementStream);
    rapidjson::Document document;
    document.ParseStream(isPlacement);

    ASSERT(document.IsObject());

    ASSERT(document.HasMember("objects"));
    const rapidjson::Value &objects = document["objects"];
    ASSERT(objects.IsArray());

    for (rapidjson::SizeType i = 0; i < objects.Size(); ++i)
    {
        const rapidjson::Value &name        = objects[i]["name"];
        const rapidjson::Value &worldMatrix = objects[i]["worldMatrix"];
        ASSERT(worldMatrix.IsArray() && worldMatrix.Size() == 16);

        std::vector<float> matrix;
        for (rapidjson::SizeType j = 0; j < worldMatrix.Size(); ++j)
        {
            matrix.push_back(worldMatrix[j].GetFloat());
        }

        MODELNAME modelname = mModelEnumMap[name.GetString()];
        // MODELFIRST means the model is not found in the Map
        if (modelname != MODELNAME::MODELFIRST)
        {
            mAquariumModels[modelname]->worldmatrices.push_back(matrix);
        }
    }
}

void Aquarium::loadModels()
{
    for (const auto &info : g_sceneInfo)
    {
        loadModel(info);
    }
}

// Load vertex and index buffers, textures and program for each model.
void Aquarium::loadModel(const G_sceneInfo &info)
{
    std::ostringstream oss;
    oss << mPath << resourceFolder << slash;
    std::string imagePath = oss.str();
    oss << info.namestr << ".js";
    std::string modelPath = oss.str();
    oss.str("");
    oss << mPath << shaderFolder << slash
        << mBackendpath << slash << mShaderVersion << slash;
    std::string programPath = oss.str();

    std::ifstream ModelStream(modelPath, std::ios::in);
    rapidjson::IStreamWrapper is(ModelStream);
    rapidjson::Document document;
    document.ParseStream(is);
    ASSERT(document.IsObject());
    const rapidjson::Value &models = document["models"];
    ASSERT(models.IsArray());

    Model *model               = context->createModel(this, info.type, info.name, info.blend);
    mAquariumModels[info.name] = model;

    for (auto &value : models.GetArray())
    {
        // set up textures
        const rapidjson::Value &textures = value["textures"];
        for (rapidjson::Value::ConstMemberIterator itr = textures.MemberBegin();
             itr != textures.MemberEnd(); ++itr)
        {
            std::string name  = itr->name.GetString();
            std::string image = itr->value.GetString();

            if (mTextureMap.find(image) == mTextureMap.end())
            {
                mTextureMap[image] = context->createTexture(name, imagePath + image);
            }
            else
            {
                std::cout << "the texture is loaded." << std::endl;
            }
            model->textureMap[name] = mTextureMap[image];
        }

        // set up vertices
        const rapidjson::Value &arrays = value["fields"];
        for (rapidjson::Value::ConstMemberIterator itr = arrays.MemberBegin();
             itr != arrays.MemberEnd(); ++itr)
        {
            std::string name  = itr->name.GetString();
            int numComponents = itr->value["numComponents"].GetInt();
            std::string type  = itr->value["type"].GetString();
            Buffer *buffer;
            if (name == "indices")
            {
                std::vector<unsigned short> vec;
                for (auto &data : itr->value["data"].GetArray())
                {
                    vec.push_back(data.GetInt());
                }
                buffer = context->createBuffer(numComponents, vec, true);
            }
            else
            {
                std::vector<float> vec;
                for (auto &data : itr->value["data"].GetArray())
                {
                    vec.push_back(data.GetFloat());
                }
                buffer = context->createBuffer(numComponents, vec, false);
            }

            model->bufferMap[name] = buffer;
        }

        // setup program
        // There are 3 programs
        // DM
        // DM+NM
        // DM+NM+RM
        std::string vsId;
        std::string fsId;

        vsId = info.program[0];
        fsId = info.program[1];

        if (vsId != "" && fsId != "")
        {
            model->textureMap["skybox"] = mTextureMap["skybox"];
        }
        else if (model->textureMap["reflection"] != nullptr)
        {
            vsId = "reflectionMapVertexShader";
            fsId = "reflectionMapFragmentShader";

            model->textureMap["skybox"] = mTextureMap["skybox"];
        }
        else if (model->textureMap["normalMap"] != nullptr)
        {
            vsId = "normalMapVertexShader";
            fsId = "normalMapFragmentShader";
        }
        else
        {
            vsId = "diffuseVertexShader";
            fsId = "diffuseFragmentShader";
        }

        Program *program;
        if (mProgramMap.find(vsId + fsId) != mProgramMap.end())
        {
            program = mProgramMap[vsId + fsId];
        }
        else
        {
            program = context->createProgram(programPath + vsId, programPath + fsId);
            mProgramMap[vsId + fsId] = program;
        }

        model->setProgram(program);
        model->init();
    }
}

void Aquarium::calculateFishCount()
{
    // Calculate fish count for each type of fish
    int numLeft = mFishCount;
    for (int i = 0; i < FISHENUM::MAX; ++i)
    {
        for (auto &fishInfo : fishTable)
        {
            if (fishInfo.type != i)
            {
                continue;
            }
            int numfloat = numLeft;
            if (i == FISHENUM::BIG)
            {
                int temp = mFishCount < g_numFishSmall ? 1 : 2;
                numfloat = std::min(numLeft, temp);
            }
            else if (i == FISHENUM::MEDIUM)
            {
                if (mFishCount < g_numFishMedium)
                {
                    numfloat = std::min(numLeft, mFishCount / 10);
                }
                else if (mFishCount < g_numFishBig)
                {
                    numfloat = std::min(numLeft, g_numFishLeftSmall);
                }
                else
                {
                    numfloat = std::min(numLeft, g_numFishLeftBig);
                }
            }
            numLeft      = numLeft - numfloat;
            fishInfo.num = numfloat;
        }
        }
}

float Aquarium::degToRad(float degrees)
{
    return static_cast<float>(degrees * M_PI / 180.0);
}

void Aquarium::updateGlobalUniforms()
{
    // Update our time
#ifdef _WIN32
    float now = GetTickCount64() / 1000.0f;
#else
    float now = clock() / 1000000.0f;
#endif
    float elapsedTime = 0.0f;
    if (g.then == 0.0f)
    {
        elapsedTime = 0.0f;
    }
    else
    {
        elapsedTime = now - g.then;
    }
    g.then = now;

    fpsTimer.update(elapsedTime);

    std::string text =
        "Aquarium FPS: " + std::to_string(static_cast<unsigned int>(fpsTimer.getAverageFPS()));
    context->setWindowTitle(text);

    g.mclock += elapsedTime * g_speed;
    g.eyeClock += elapsedTime * g_eyeSpeed;

    g.eyePosition[0] = sin(g.eyeClock) * g_eyeRadius;
    g.eyePosition[1] = g_eyeHeight;
    g.eyePosition[2] = cos(g.eyeClock) * g_eyeRadius;
    g.target[0]      = static_cast<float>(sin(g.eyeClock + M_PI)) * g_targetRadius;
    g.target[1]      = g_targetHeight;
    g.target[2]      = static_cast<float>(cos(g.eyeClock + M_PI)) * g_targetRadius;

    float nearPlane = 1;
    float farPlane  = 25000.0f;
    float aspect    = static_cast<float>(context->getClientWidth()) /
                   static_cast<float>(context->getclientHeight());
    float top    = tan(degToRad(g_fieldOfView * g_fovFudge) * 0.5f) * nearPlane;
    float bottom = -top;
    float left   = aspect * bottom;
    float right  = aspect * top;
    float width  = abs(right - left);
    float height = abs(top - bottom);
    float xOff   = width * g_net_offset[0] * g_net_offsetMult;
    float yOff   = height * g_net_offset[1] * g_net_offsetMult;

    // set frustm and camera look at
    matrix::frustum(g.projection, left + xOff, right + xOff, bottom + yOff, top + yOff, nearPlane,
                    farPlane);
    matrix::cameraLookAt(viewUniforms.viewInverse, g.eyePosition, g.target, g.up);
    matrix::inverse4(g.view, viewUniforms.viewInverse);
    matrix::mulMatrixMatrix4(viewUniforms.viewProjection, g.view, g.projection);
    matrix::inverse4(g.viewProjectionInverse, viewUniforms.viewProjection);

    memcpy(g.skyView, g.view, 16 * sizeof(float));
    g.skyView[12] = 0.0;
    g.skyView[13] = 0.0;
    g.skyView[14] = 0.0;
    matrix::mulMatrixMatrix4(g.skyViewProjection, g.skyView, g.projection);
    matrix::inverse4(g.skyViewProjectionInverse, g.skyViewProjection);

    matrix::getAxis(g.v3t0, viewUniforms.viewInverse, 0);
    matrix::getAxis(g.v3t1, viewUniforms.viewInverse, 1);
    matrix::mulScalarVector(20.0f, g.v3t0, 3);
    matrix::mulScalarVector(30.0f, g.v3t1, 3);
    matrix::addVector(lightWorldPositionUniform.lightWorldPos, g.eyePosition, g.v3t0, 3);
    matrix::addVector(lightWorldPositionUniform.lightWorldPos, lightWorldPositionUniform.lightWorldPos, g.v3t1, 3);
}

void Aquarium::render()
{
    updateGlobalUniforms();

    matrix::resetPseudoRandom();

    context->preFrame();

    drawBackground();

    drawFishes();
    
    drawInner();

    drawSeaweed();

    drawOutside();

}

void Aquarium::drawBackground()
{
    Model *model = mAquariumModels[MODELNAME::MODELRUINCOlOMN];
    for (int i = MODELNAME::MODELRUINCOlOMN; i <= MODELNAME::MODELTREASURECHEST; ++i)
    {
        model = mAquariumModels[i];
        updateWorldMatrixAndDraw(model);
    }
}

void Aquarium::drawSeaweed()
{
    SeaweedModel *model = static_cast<SeaweedModel *>(mAquariumModels[MODELNAME::MODELSEAWEEDA]);
    for (int i = MODELNAME::MODELSEAWEEDA; i <= MODELNAME::MODELSEAWEEDB; ++i)
    {
        //model->updateSeaweedModelTime(g.mclock);
        model = static_cast<SeaweedModel *>(mAquariumModels[i]);
        updateWorldMatrixAndDraw(model);
    }
}

void Aquarium::drawFishes()
{
    for (int i = MODELNAME::MODELSMALLFISHA; i <= MODELNAME::MODELBIGFISHB; ++i)
    {
        FishModel *model = static_cast<FishModel *>(mAquariumModels[i]);

        const Fish &fishInfo = fishTable[i - MODELNAME::MODELSMALLFISHA];
        int numFish          = fishInfo.num;
        model->updateFishCommonUniforms(fishInfo.fishLength, fishInfo.fishBendAmount,
                                        fishInfo.fishWaveLength);
        model->preDraw();

        float fishBaseClock   = g.mclock * g_fishSpeed;
        float fishRadius      = fishInfo.radius;
        float fishRadiusRange = fishInfo.radiusRange;
        float fishSpeed       = fishInfo.speed;
        float fishSpeedRange  = fishInfo.speedRange;
        float fishTailSpeed   = fishInfo.tailSpeed * g_fishTailSpeed;
        float fishOffset      = g_fishOffset;
        //float fishClockSpeed  = g_fishSpeed;
        float fishHeight      = g_fishHeight + fishInfo.heightOffset;
        float fishHeightRange = g_fishHeightRange * fishInfo.heightRange;
        float fishXClock      = g_fishXClock;
        float fishYClock      = g_fishYClock;
        float fishZClock      = g_fishZClock;

        for (int ii = 0; ii < numFish; ++ii)
        {
            float fishClock = fishBaseClock + ii * fishOffset;
            float speed = fishSpeed + static_cast<float>(matrix::pseudoRandom()) * fishSpeedRange;
            float scale = 1.0f + static_cast<float>(matrix::pseudoRandom()) * 1;
            float xRadius =
                fishRadius + static_cast<float>(matrix::pseudoRandom()) * fishRadiusRange;
            float yRadius = 2.0f + static_cast<float>(matrix::pseudoRandom()) * fishHeightRange;
            float zRadius =
                fishRadius + static_cast<float>(matrix::pseudoRandom()) * fishRadiusRange;
            float fishSpeedClock = fishClock * speed;
            float xClock         = fishSpeedClock * fishXClock;
            float yClock         = fishSpeedClock * fishYClock;
            float zClock         = fishSpeedClock * fishZClock;

            model->updateFishPerUniforms(
                sin(xClock) * xRadius, sin(yClock) * yRadius + fishHeight, cos(zClock) * zRadius,
                sin(xClock - 0.04f) * xRadius, sin(yClock - 0.01f) * yRadius + fishHeight,
                cos(zClock - 0.04f) * zRadius, scale,
                fmod((g.mclock + ii * g_tailOffsetMult) * fishTailSpeed * speed,
                     static_cast<float>(M_PI) * 2));
            model->updatePerInstanceUniforms(&viewUniforms);
            if (mBackendpath=="opengl" || mBackendpath == "angle")
            {
                model->draw();
            }
        }
        // TODO(yizhou): If backend is dawn, draw only once for every type of fish by drawInstance.
        // If backend is opengl or angle, draw for exery fish. Update the logic the same as Dawn if
        // uniform blocks are implemented for OpenGL.
        if (mBackendpath == "dawn")
        {
            model->draw();
        }
    }
}

void Aquarium::drawInner()
{
    Model *model = mAquariumModels[MODELNAME::MODELGLOBEINNER];
    updateWorldMatrixAndDraw(model);
}

void Aquarium::drawOutside()
{
    Model *model = mAquariumModels[MODELNAME::MODELENVIRONMENTBOX];
    updateWorldMatrixAndDraw(model);
}

void Aquarium::updateWorldProjections(const float *w)
{
    memcpy(viewUniforms.world, w, 16 * sizeof(float));
    matrix::mulMatrixMatrix4(viewUniforms.worldViewProjection, viewUniforms.world, viewUniforms.viewProjection);
    matrix::inverse4(g.worldInverse, viewUniforms.world);
    matrix::transpose4(viewUniforms.worldInverseTranspose, g.worldInverse);

    // update world uniforms for dawn backend
    context->updateWorldlUniforms(this);
}

void Aquarium::updateWorldMatrixAndDraw(Model *model)
{
    if (model->worldmatrices.size())
    {
        for (auto &world : model->worldmatrices)
        {
            updateWorldProjections(world.data());
            // Models of dawn keep viewUniforms for every model while opengl models use global
            // viewUniforms.
            // Update all viewUniforms on dawn backend.
            if (mBackendpath == "opengl")
            {
                model->preDraw();
                model->updatePerInstanceUniforms(&viewUniforms);
                model->draw();
            }
            else
            {
                model->updatePerInstanceUniforms(&viewUniforms);
            }
        }
    }

    // TODO(yizhou): If backend is dawn, draw only once for every model. If
    // backend is opengl or angle, draw for exery instance.
    // Update the logic the same as Dawn if uniform blocks are implemented for OpenGL.
    if (mBackendpath == "dawn" || mBackendpath == "angle")
    {
        model->preDraw();
        model->draw();
    }
}
