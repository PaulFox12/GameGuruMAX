// GG: Rock Triplanar (CustomShaders.cpp): the opaque object shader with the material's textures laid on in world space from
// three sides, as the terrain's Steep Rock (objectHF.hlsli, GGRockTriplanar)
#define OBJECTSHADER_COMPILE_PS
#define OBJECTSHADER_LAYOUT_COMMON
#define SHADOW_MASK_ENABLED
#define OUTPUT_GBUFFER
#define TILEDFORWARD
#define DISABLE_ALPHATEST
#define ROCKTRIPLANAR
#include "objectHF.hlsli"
