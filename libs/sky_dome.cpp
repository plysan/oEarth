#include <stdio.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include "common.h"
#include "../utils/coord.h"
#include "sky_dome.h"
#include "../vars.h"

float integrantLengthCoefficient = 0.9f;
int integrantLengthInitChunks = 5;
float integrantLengthMin = 0.00001f * earthRadius;
float integrantLengthMax = 0.003f * earthRadius;
float height0Rayleigh = 7.994 * earthRadius / 6371;
// decrease/increase to be more/less blue
float opticalLengthCoefficient = 6.37f / earthRadius;
float colorCiexyzClampCoefficient = 1.0/526.6095581;
float sunIntensity = 1000000.0f;
float maxIntensityCiexyz = 0.0f;
float minIntensitySrgb = 0.0f;
float maxIntensitySrgb = 0.0f;

/*
 * CIE 1931 color matching functions data
 * obtained from http://www.cvrl.org/database/data/cmfs/ciexyzjv.csv
 */
int cieMatrixLength = 45;
float cieMatrix[][4] = {
    {0.38f, 0.0026899000f, 0.00020000000f, 0.01226000000000f},
    {0.39f, 0.0107810000f, 0.00080000000f, 0.04925000000000f},
    {0.40f, 0.0379810000f, 0.00280000000f, 0.17409000000000f},
    {0.41f, 0.0999410000f, 0.00740000000f, 0.46053000000000f},
    {0.42f, 0.2294800000f, 0.01750000000f, 1.06580000000000f},
    {0.43f, 0.3109500000f, 0.02730000000f, 1.46720000000000f},
    {0.44f, 0.3333600000f, 0.03790000000f, 1.61660000000000f},
    {0.45f, 0.2888200000f, 0.04680000000f, 1.47170000000000f},
    {0.46f, 0.2327600000f, 0.06000000000f, 1.29170000000000f},
    {0.47f, 0.1747600000f, 0.09098000000f, 1.11380000000000f},
    {0.48f, 0.0919440000f, 0.13902000000f, 0.75596000000000f},
    {0.49f, 0.0317310000f, 0.20802000000f, 0.44669000000000f},
    {0.50f, 0.0048491000f, 0.32300000000f, 0.26437000000000f},
    {0.51f, 0.0092899000f, 0.50300000000f, 0.15445000000000f},
    {0.52f, 0.0637910000f, 0.71000000000f, 0.07658500000000f},
    {0.53f, 0.1669200000f, 0.86200000000f, 0.04136600000000f},
    {0.54f, 0.2926900000f, 0.95400000000f, 0.02004200000000f},
    {0.55f, 0.4363500000f, 0.99495000000f, 0.00878230000000f},
    {0.56f, 0.5974800000f, 0.99500000000f, 0.00404930000000f},
    {0.57f, 0.7642500000f, 0.95200000000f, 0.00227710000000f},
    {0.58f, 0.9163500000f, 0.87000000000f, 0.00180660000000f},
    {0.59f, 1.0230000000f, 0.75700000000f, 0.00123480000000f},
    {0.60f, 1.0550000000f, 0.63100000000f, 0.00090564000000f},
    {0.61f, 0.9923900000f, 0.50300000000f, 0.00042885000000f},
    {0.62f, 0.8434600000f, 0.38100000000f, 0.00025598000000f},
    {0.63f, 0.6328900000f, 0.26500000000f, 0.00009769400000f},
    {0.64f, 0.4406200000f, 0.17500000000f, 0.00005116500000f},
    {0.65f, 0.2786200000f, 0.10700000000f, 0.00002423800000f},
    {0.66f, 0.1616100000f, 0.06100000000f, 0.00001190600000f},
    {0.67f, 0.0857530000f, 0.03200000000f, 0.00000560060000f},
    {0.68f, 0.0458340000f, 0.01700000000f, 0.00000279120000f},
    {0.69f, 0.0221870000f, 0.00821000000f, 0.00000131350000f},
    {0.70f, 0.0110980000f, 0.00410200000f, 0.00000064767000f},
    {0.71f, 0.0056531000f, 0.00209100000f, 0.00000033304000f},
    {0.72f, 0.0028253000f, 0.00104700000f, 0.00000017026000f},
    {0.73f, 0.0013994000f, 0.00052000000f, 0.00000008710700f},
    {0.74f, 0.0006684700f, 0.00024920000f, 0.00000004316200f},
    {0.75f, 0.0003207300f, 0.00012000000f, 0.00000002155400f},
    {0.76f, 0.0001597300f, 0.00006000000f, 0.00000001120400f},
    {0.77f, 0.0000795130f, 0.00003000000f, 0.00000000583400f},
    {0.78f, 0.0000395410f, 0.00001498900f, 0.00000000303830f},
    {0.79f, 0.0000195970f, 0.00000746560f, 0.00000000157780f},
    {0.80f, 0.0000096700f, 0.00000370280f, 0.00000000081565f},
    {0.81f, 0.0000047706f, 0.00000183650f, 0.00000000042138f},
    {0.82f, 0.0000023534f, 0.00000091092f, 0.00000000021753f},
};

/*
 * cieXYZ to sRGB space matrix with reference white of D50.
 * data obtained from http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
 */
float ciexyz2SrgbData[9] = {3.1338561f, -0.9787684f, 0.0719453f, -1.6168667f, 1.9161415f, -0.2289914f, -0.4906146f, 0.0334540f, 1.4052427f};
glm::mat3 ciexyz2SrgbMatrix = glm::make_mat3(ciexyz2SrgbData);

float getIntegrantLength(glm::vec3& integratDirNormal, glm::vec3& upNormal, float posHeightSealevel) {
    float upIntegratDirCos = glm::dot(upNormal, integratDirNormal);
    float integrantLength = posHeightSealevel / (glm::abs(upIntegratDirCos) + 0.00000000001f) / integrantLengthInitChunks;
    integrantLength = glm::clamp(integrantLength, integrantLengthMin, integrantLengthMax);
    bool goingDown = upIntegratDirCos < 0.0f;
    integrantLength = goingDown ? integrantLength * integrantLengthCoefficient : integrantLength / integrantLengthCoefficient;
    return integrantLength;
}

float integralOpticalLength(glm::vec3 scatterPos, glm::vec3 sunDirNormal, float viewToScatterOpticalLength) {
    float integralScatterOpticalLength = viewToScatterOpticalLength;
    float integrantLength;
    glm::vec3 integrantDirLength;
    glm::vec3 integrantPos = scatterPos;
    float integrantPosHeight = glm::length(integrantPos);
    float integrantPosHeightSealevel = integrantPosHeight - earthRadius;
    float integrantPosHeightCoefficient = pow(glm::e<float>(), -(integrantPosHeightSealevel)/height0Rayleigh);
    float lastIntegrantPosHeight = integrantPosHeight;
    float lastIntegrantPosHeightCoefficient = integrantPosHeightCoefficient;
    while(integrantPosHeight <= atmosphereTopRadius){
        if(integrantPosHeight <= earthRadius){
            return 999999999.9f;// for sunlight that is blocked by earth, return a big value to approach infinite optical depth, ie, zero intensity
        }
        glm::vec3 integrantPosNormal = glm::normalize(integrantPos);
        integrantLength = getIntegrantLength(sunDirNormal, integrantPosNormal, integrantPosHeightSealevel);
        integrantDirLength = sunDirNormal * integrantLength;
        integrantPos += integrantDirLength;
        integrantPosHeight = glm::length(integrantPos);
        integrantPosHeightSealevel = integrantPosHeight - earthRadius;
        integrantPosHeightCoefficient = pow(glm::e<float>(), -(integrantPosHeightSealevel)/height0Rayleigh);
        if(integrantPosHeight > lastIntegrantPosHeight) {
            integralScatterOpticalLength += lastIntegrantPosHeightCoefficient * integrantLength;
        } else {
            integralScatterOpticalLength += integrantPosHeightCoefficient * integrantLength;
        }
        lastIntegrantPosHeight = integrantPosHeight;
        lastIntegrantPosHeightCoefficient = integrantPosHeightCoefficient;
    }
    return integralScatterOpticalLength;
}

glm::vec4 calculateColorCiexyz(float height, float viewAngle, float sunAngleVertical, float sunAngleHorizontal, bool debugColor) {
    static float integralScatterOpticalLength[1024];
    static float scatterPosCoefficients[1024];
    float viewToScatterOpticalLength = 0.0f;
    glm::vec3 viewPos = coord2Pos(90.0f, 0.0f, height);
    glm::vec3 viewDirNormal = glm::normalize(coord2Pos(90.0f-viewAngle, 0.0f, 0.0f));
    glm::vec3 scatterPos = viewPos;
    glm::vec3 sunDirNormal = glm::normalize(coord2Pos(90.0f-sunAngleVertical, sunAngleHorizontal, 0.0f));
    glm::vec3 scatterPosNormal;
    float integrantValue;
    float integrantLength;
    float scatterPosHeight = glm::length(scatterPos);
    float scatterPosHeightSealevel = scatterPosHeight - earthRadius;
    float scatterPosHeightCoefficient = pow(glm::e<float>(), -(scatterPosHeightSealevel)/height0Rayleigh);
    float lastScatterPosHeight = scatterPosHeight;
    float lastScatterPosHeightCoefficient = scatterPosHeightCoefficient;
    int integralScatterOpticalLengthIdx = 0;
    int integrantCount = 0;
    while(scatterPosHeight <= atmosphereTopRadius && scatterPosHeight >= earthRadius) {
        integrantCount++;
        scatterPosNormal = glm::normalize(scatterPos);
        integrantLength = getIntegrantLength(viewDirNormal, scatterPosNormal, scatterPosHeightSealevel);
        scatterPos += viewDirNormal * integrantLength;
        scatterPosHeight = glm::length(scatterPos);
        scatterPosHeightSealevel = scatterPosHeight - earthRadius;
        scatterPosHeightCoefficient = pow(glm::e<float>(), -(scatterPosHeightSealevel)/height0Rayleigh);
        if(scatterPosHeight > lastScatterPosHeight) {
            integrantValue = integrantLength * lastScatterPosHeightCoefficient;
        } else {
            integrantValue = integrantLength * scatterPosHeightCoefficient;
        }
        viewToScatterOpticalLength += integrantValue;
        lastScatterPosHeight = scatterPosHeight;
        lastScatterPosHeightCoefficient = scatterPosHeightCoefficient;
        integralScatterOpticalLength[integralScatterOpticalLengthIdx] = integralOpticalLength(scatterPos, sunDirNormal, viewToScatterOpticalLength);
        scatterPosCoefficients[integralScatterOpticalLengthIdx++] = integrantValue;
    }
    if (debugColor) std::cout << integrantCount << ':';

    glm::vec3 colorCiexyz = glm::vec3(0.0f, 0.0f, 0.0f);
    for (int i=0; i<cieMatrixLength; i++) {
        float lambda = cieMatrix[i][0];
        float lambdaP4 = pow(lambda, 4);
        float lambdaPN2 = pow(lambda, -2);
        float integralIntensityLambda = 0.0f;
        for(int j=0; j<integralScatterOpticalLengthIdx; j++) {
            integralIntensityLambda += pow(glm::e<float>(), -integralScatterOpticalLength[j] * opticalLengthCoefficient / lambdaP4) * scatterPosCoefficients[j] * 637;
        }
        // air refraction formula referenced from http://refractiveindex.info
        float airRefraction = 0.05792105f/(238.0185f-lambdaPN2) + 0.00167917f/(57.362f-lambdaPN2) + 1.0f;
        integralIntensityLambda *= sunIntensity * pow(pow(airRefraction, 2)-1, 2) / lambdaP4;
        colorCiexyz.x += integralIntensityLambda * cieMatrix[i][1];
        colorCiexyz.y += integralIntensityLambda * cieMatrix[i][2];
        colorCiexyz.z += integralIntensityLambda * cieMatrix[i][3];
    }

    float scatterAngleCos = glm::dot(viewDirNormal, sunDirNormal);
    static float redAvgWlengthP4 = pow(redAvgWlength, 4);
    float viewPathOpticalDepth = pow(glm::e<float>(), -viewToScatterOpticalLength * opticalLengthCoefficient / redAvgWlengthP4);
    return glm::vec4(colorCiexyz * (float)(1 + pow(scatterAngleCos, 2)), viewPathOpticalDepth);
}

glm::detail::uint32 calculateColor(float height, float viewAngleCos, float sunAngleVerticalCos, float sunAngleHorizontalCos, bool debugColor) {
    glm::vec4 colorCiexyz = calculateColorCiexyz(height, acos(viewAngleCos)/glm::pi<float>()*180.0f, acos(sunAngleVerticalCos)/glm::pi<float>()*180.0f, acos(sunAngleHorizontalCos)/glm::pi<float>()*180.0f, debugColor);

    if(colorCiexyz.x > maxIntensityCiexyz)maxIntensityCiexyz = colorCiexyz.x;
    if(colorCiexyz.y > maxIntensityCiexyz)maxIntensityCiexyz = colorCiexyz.y;
    if(colorCiexyz.z > maxIntensityCiexyz)maxIntensityCiexyz = colorCiexyz.z;

    glm::vec3 colorSrgbLinear = ciexyz2SrgbMatrix * (glm::vec3(colorCiexyz) * colorCiexyzClampCoefficient);
    if(colorSrgbLinear.x < minIntensitySrgb)minIntensitySrgb = colorSrgbLinear.x;
    if(colorSrgbLinear.y < minIntensitySrgb)minIntensitySrgb = colorSrgbLinear.y;
    if(colorSrgbLinear.z < minIntensitySrgb)minIntensitySrgb = colorSrgbLinear.z;
    if(colorSrgbLinear.x > maxIntensitySrgb)maxIntensitySrgb = colorSrgbLinear.x;
    if(colorSrgbLinear.y > maxIntensitySrgb)maxIntensitySrgb = colorSrgbLinear.y;
    if(colorSrgbLinear.z > maxIntensitySrgb)maxIntensitySrgb = colorSrgbLinear.z;
    colorSrgbLinear = colorSrgbLinear * colorSrgbCoefficient + glm::vec3(colorSrgbOffset);
    int red = glm::clamp((int)(colorSrgbLinear.x * 255), 0, 255);
    int green = glm::clamp((int)(colorSrgbLinear.y * 255), 0, 255);
    int blue = glm::clamp((int)(colorSrgbLinear.z * 255), 0, 255);
    int viewPathOpticalDepthInt = glm::clamp((int)(colorCiexyz.w*255), 0, 255);

    if (debugColor) printf("%3d%3d%3d%3d|", red, green, blue, viewPathOpticalDepthInt);
    return red&0x000000ff | green<<8&0x0000ff00 | blue<<16&0x00ff0000 | viewPathOpticalDepthInt<<24&0xff000000;
}


void SkyDome::genSkyDome(glm::vec3 cameraPos) {
    vertices.clear();
    //TODO 90 dynamic
    int latCount = 100;
    int lngCount = 40;
    float latInterval = 90.0f/(latCount - 1);
    float lngInterval = 360.0f/(lngCount - 1);
    for (int i=0; i<latCount; i++) {
        for (int j=0; j<lngCount; j++) {
            glm::vec3 pos = coord2Pos(i*latInterval, j*lngInterval, atmosphereThickness);
            // north pole rotate to coord(0, 90) to align with glm view coordinate's original lookat direction
            pos = glm::rotate(pos, 90.0f/180.0f*glm::pi<float>(), glm::vec3(-1.0f, 0.0f, 0.0f));
            vertices.push_back({pos, {0.0f, 0.0f}});
        }
    }
    indices.clear();
    for (int i=0; i<latCount-1; i++) {
        for (int j=0; j<lngCount-1; j++) {
            int begin = j + lngCount * i;
            indices.push_back(begin);
            indices.push_back(begin + lngCount);
            indices.push_back(begin + lngCount + 1);
            indices.push_back(begin + lngCount + 1);
            indices.push_back(begin + 1);
            indices.push_back(begin);
        }
    }
    std::cout << "Sky indices: " << indices.size() << ", vertices: " << vertices.size() << '\n';
}

void SkyDome::genScatterTexure() {
    int scatterTextureSize = scatterTextureSunAngleSize*scatterTextureHeightSize;
    scatterTexture.data = new unsigned char[(int)pow(scatterTextureSize, 3) * 4];
    glm::detail::uint32* scatterTextureArrayData = (glm::detail::uint32*)scatterTexture.data;

    int textureSizeExp2 = pow(scatterTextureSunAngleSize*scatterTextureHeightSize, 2);
    int textureSizeExp3 = pow(scatterTextureSunAngleSize*scatterTextureHeightSize, 3);
    int textureSize = scatterTextureSunAngleSize * scatterTextureHeightSize;
    int scatterTexture4thdSize = pow(scatterTextureHeightSize, 2);
    std::stringstream scatterFileName;
    scatterFileName << "scatter_texture_" << scatterTextureSunAngleSize << "-" << scatterTextureHeightSize;
    std::ifstream scatterFileIn (scatterFileName.str().c_str());
    if(scatterFileIn) {
        printf("Reading existing scatter texture: %s\n", scatterFileName.str().c_str());
        for(int i=0; i<textureSizeExp3; i++) {
            scatterFileIn >> scatterTextureArrayData[i];
        }
        scatterFileIn.close();
        return;
    }
    clock_t before = clock();
    // height: 0 -> atmosphereThickness
    for (int i=0; i<scatterTexture4thdSize; i++) {
        int x = i%scatterTextureHeightSize * scatterTextureSunAngleSize;
        int y = i/scatterTextureHeightSize * scatterTextureSunAngleSize;
        float height = atmosphereThickness * pow((float)i/(scatterTexture4thdSize-1), 2);
        float heightHorizonAngleCos = -(sqrt(pow(earthRadius+height, 2) - pow(earthRadius, 2)) / (earthRadius+height));
        // cos of view angle: 1 -> -1
        for(int j=textureSize-1; j>-1; j--) {
            float viewAngleCos = 2.0f * (float)j/(textureSize-1) - 1.0f;
            static int colorPrintDetail = 4;
            static int jInterval = textureSize / colorPrintDetail;
            bool debugJ = j % jInterval == 0;
            if (debugJ) printf("cos(view): %.2f, Height: %d\n", viewAngleCos, i);
            if(viewAngleCos > 0.0f) {
                viewAngleCos = heightHorizonAngleCos + pow(viewAngleCos, 5)*(1-heightHorizonAngleCos);
            } else {
                viewAngleCos = heightHorizonAngleCos - pow(-viewAngleCos, 5)*(1+heightHorizonAngleCos);
            }
            // cos of sun angle vertical: 1 -> -1
            for(int k=scatterTextureSunAngleSize-1; k>-1; k--) {
                float sunAngleVerticalCos = 2.0f * (float)k/(scatterTextureSunAngleSize-1) - 1.0f;
                // cos of sun angle horizontal: 1 -> -1
                bool debugColor;
                for(int l=scatterTextureSunAngleSize-1; l>-1; l--) {
                    float sunAngleHorizontalCos = 2.0f * (float)l/(scatterTextureSunAngleSize-1) - 1.0f;
                    static int klInterval = scatterTextureSunAngleSize / colorPrintDetail;
                    debugColor = k % klInterval == 0 && l % klInterval == 0 && debugJ;
                    scatterTextureArrayData[j*textureSizeExp2 + (k+y)*textureSize + l+x] = calculateColor(height, viewAngleCos, sunAngleVerticalCos, sunAngleHorizontalCos, debugColor);
                }
                if (debugColor) printf("\n");
            }
        }
    }
    printf("Scatter texture compute time: %fs\n", (double)(clock() - before)/CLOCKS_PER_SEC);

    if(glm::abs(1.0f/maxIntensityCiexyz - colorCiexyzClampCoefficient) > 0.000001f) {
        printf("Suggestion: set colorCiexyzClampCoefficient to 1.0/%.7f then remove and regenerate(rerun) the scatter texture(program).\n", maxIntensityCiexyz);
    } else {
        float suggestColorSrgbCoefficient = 1.0f/(maxIntensitySrgb - minIntensitySrgb);
        float suggestColorSrgbOffset = -minIntensitySrgb * suggestColorSrgbCoefficient;
        if(glm::abs(suggestColorSrgbCoefficient - colorSrgbCoefficient) > 0.000001f || glm::abs(suggestColorSrgbOffset - colorSrgbOffset) > 0.000001f) {
            printf("Suggestion: set colorSrgbOffset to %.7f and colorSrgbCoefficient to %.7f then remove and regenerate(rerun) the scatter texture(program).\n",
                suggestColorSrgbOffset, suggestColorSrgbCoefficient);
        }
    }

    printf("Saving scatter texture to: %s\n", scatterFileName.str().c_str());
    std::ofstream scatterFileOut (scatterFileName.str().c_str());
    if (!scatterFileOut.is_open()) {
        printf("scatter texture open failed..\n");
        return;
    }
    for(int i=0; i<textureSizeExp3; i++) {
        scatterFileOut << scatterTextureArrayData[i] << " ";
    }
    scatterFileOut.close();
}
