//define variables used in both cpu and gpu
const float earthRadius = 1.0f;
const float atmosphereTopRadius = 1.011f;
const float atmosphereThickness = atmosphereTopRadius - earthRadius;
const int scatterTextureSunAngleSize = 13;
const int scatterTextureHeightSize = 5;
const float colorSrgbOffset = 0.0091240;
const float colorSrgbCoefficient = 0.7906017;
const float redAvgWlength = 0.685;
const float greenAvgWlength = 0.5325;
const float blueAvgWlength = 0.4725;
