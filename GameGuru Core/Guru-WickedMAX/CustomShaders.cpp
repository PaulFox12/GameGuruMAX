#include <string>
#include "Utility/stb_image.h"
#include "CFileC.h"
#include "CStr.h"
#include "wiGraphicsDevice.h"
#include "wiScene.h"
#include "wiRenderer.h"
#include "wiProfiler.h"
#include "wiECS.h"
#include "Utility/tinyddsloader.h"

#include "master.h"

#include "..\GameGuru\Imgui\imgui.h"

// redefines MAX_PATH to 1050
#include "preprocessor-moreflags.h"

#ifdef OPTICK_ENABLE
#include "optick.h"
#endif

// Externl for debugging
extern void timestampactivity(int i, char* desc_s);

#include "GGThread.h"
using namespace GGThread;

#include "DirectXTex.h"
#include "..\..\..\..\GameGuru\Imgui\imgui_gg_dx11.h"
#include "M-UndoSys-Terrain.h"
#include <wiRenderer.h>


using namespace wiGraphics;
using namespace wiScene;
using namespace wiRenderer;

Shader shaderCustom1PS;


enum OBJECTRENDERING_DOUBLESIDED
{
	OBJECTRENDERING_DOUBLESIDED_DISABLED,
	OBJECTRENDERING_DOUBLESIDED_ENABLED,
	OBJECTRENDERING_DOUBLESIDED_BACKSIDE,
	OBJECTRENDERING_DOUBLESIDED_COUNT
};
enum OBJECTRENDERING_TESSELLATION
{
	OBJECTRENDERING_TESSELLATION_DISABLED,
	OBJECTRENDERING_TESSELLATION_ENABLED,
	OBJECTRENDERING_TESSELLATION_COUNT
};
enum OBJECTRENDERING_ALPHATEST
{
	OBJECTRENDERING_ALPHATEST_DISABLED,
	OBJECTRENDERING_ALPHATEST_ENABLED,
	OBJECTRENDERING_ALPHATEST_COUNT
};

PipelineState PSO_custom_object
[MaterialComponent::SHADERTYPE_COUNT]
[RENDERPASS_COUNT]
[BLENDMODE_COUNT]
[OBJECTRENDERING_DOUBLESIDED_COUNT]
[OBJECTRENDERING_TESSELLATION_COUNT]
[OBJECTRENDERING_ALPHATEST_COUNT];

Shader shaderMainTreeAnimateVS;
Shader shaderPrepassTreeAnimateVS;
Shader shaderShadowTreeAnimateVS;
Shader shaderWaterPS;
Shader shaderRiverWaterPS;
Shader shaderRockTriplanarPS;
Shader shaderGlassPS;
Shader shaderGridPS;
Shader damageBloodPS;
Shader damageBloodVS;

void AddCustomShaders(void)
{

	//PE: Tree animated.
	CustomShader customShader;
	if(!LoadShader(VS, shaderMainTreeAnimateVS, "objectVS_common_tree.cso"))
		customShader.bActive = false;
	if(!LoadShader(VS, shaderPrepassTreeAnimateVS, "objectVS_prepass_trees.cso"))
		customShader.bActive = false;
	if(!LoadShader(VS, shaderShadowTreeAnimateVS, "shadowVS_alphatest_tree.cso"))
		customShader.bActive = false;

	PipelineStateDesc desc[RENDERPASS_COUNT];
	PipelineState pso[RENDERPASS_COUNT];
	for (int i = 0; i < RENDERPASS_COUNT; i++)
	{
		wiRenderer::AddPipelineDesc(desc[i], i, PSTYPE_OBJECT, MaterialComponent::SHADERTYPE::SHADERTYPE_PBR, BLENDMODE_OPAQUE, OBJECTRENDERING_DOUBLESIDED_ENABLED, false, true, false);
		if (customShader.bActive)
		{
			if (i == RENDERPASS_MAIN)
			{
				desc[i].vs = &shaderMainTreeAnimateVS;
			}
			if (i == RENDERPASS_SHADOW || i == RENDERPASS_SHADOWCUBE)
			{
				desc[i].vs = &shaderShadowTreeAnimateVS;
			}
			if (i == RENDERPASS_PREPASS)
			{
				desc[i].vs = &shaderPrepassTreeAnimateVS;
			}
		}
		wiRenderer::GetDevice()->CreatePipelineState(&desc[i], &pso[i]);
	}
	customShader.name = "Tree Animate Doublesided";
	customShader.renderTypeFlags = RENDERTYPE_OPAQUE; // RENDERTYPE_TRANSPARENT;
	for (int i = 0; i < RENDERPASS_COUNT; i++)
		customShader.pso[i] = pso[i];

	RegisterCustomShader(customShader);

	//PE: Water object.
	PipelineState psowater;
	PipelineStateDesc descwater;
	CustomShader customWaterShader;
	if (!LoadShader(PS, shaderWaterPS, "objectPS_custom_water.cso"))
		customWaterShader.bActive = false;
	wiRenderer::AddPipelineDesc(descwater, RENDERPASS_MAIN, PSTYPE_OBJECT, MaterialComponent::SHADERTYPE::SHADERTYPE_PBR, BLENDMODE_ALPHA, OBJECTRENDERING_DOUBLESIDED_DISABLED, false, false, true);
	if (customWaterShader.bActive)
	{
		descwater.ps = &shaderWaterPS;
	}
	wiRenderer::GetDevice()->CreatePipelineState(&descwater, &psowater);
	customWaterShader.name = "Water Object";
	customWaterShader.renderTypeFlags = RENDERTYPE_TRANSPARENT; // RENDERTYPE_TRANSPARENT;
	customWaterShader.pso[RENDERPASS_MAIN] = psowater;
	RegisterCustomShader(customWaterShader);


	//PE: Glass object.
	PipelineState psoglass;
	PipelineStateDesc descglass;
	PipelineState psoglassshadow;
	PipelineStateDesc descglassshadow;
	CustomShader customglassShader;
	if(!LoadShader(PS, shaderGlassPS, "objectPS_transparent_glass.cso"))
		customglassShader.bActive = false;
	wiRenderer::AddPipelineDesc(descglass, RENDERPASS_MAIN, PSTYPE_OBJECT, MaterialComponent::SHADERTYPE::SHADERTYPE_PBR, BLENDMODE_ALPHA, OBJECTRENDERING_DOUBLESIDED_DISABLED, false, false, true);
	wiRenderer::AddPipelineDesc(descglassshadow, RENDERPASS_SHADOW, PSTYPE_OBJECT, MaterialComponent::SHADERTYPE::SHADERTYPE_PBR, BLENDMODE_ALPHA, OBJECTRENDERING_DOUBLESIDED_DISABLED, false, false, true);

	if (customglassShader.bActive)
	{
		descglass.ps = &shaderGlassPS;
	}
	wiRenderer::GetDevice()->CreatePipelineState(&descglass, &psoglass);
	wiRenderer::GetDevice()->CreatePipelineState(&descglassshadow, &psoglassshadow);
	customglassShader.name = "Glass Object";
	customglassShader.renderTypeFlags = RENDERTYPE_TRANSPARENT; // RENDERTYPE_TRANSPARENT;
	customglassShader.pso[RENDERPASS_MAIN] = psoglass;
	customglassShader.pso[RENDERPASS_SHADOW] = psoglassshadow;
	RegisterCustomShader(customglassShader);

	//PE: Grid object.
	PipelineState psogrid;
	PipelineStateDesc descgrid;
	CustomShader customgridShader;
	if (!LoadShader(PS, shaderGridPS, "objectPS_grid.cso"))
		customgridShader.bActive = false;
	wiRenderer::AddPipelineDesc(descgrid, RENDERPASS_MAIN, PSTYPE_OBJECT, MaterialComponent::SHADERTYPE::SHADERTYPE_PBR, BLENDMODE_ALPHA, OBJECTRENDERING_DOUBLESIDED_DISABLED, false, false, true);

	if (customgridShader.bActive)
	{
		descgrid.ps = &shaderGridPS;
	}
	wiRenderer::GetDevice()->CreatePipelineState(&descgrid, &psogrid);
	customgridShader.name = "grid Object";
	customgridShader.renderTypeFlags = RENDERTYPE_TRANSPARENT; // RENDERTYPE_TRANSPARENT;
	customgridShader.pso[RENDERPASS_MAIN] = psogrid;
	RegisterCustomShader(customgridShader);


	//PE: blood damage shader.
	PipelineState psoBloodDamage;
	PipelineStateDesc descBloodDamage;
	CustomShader customBloodDamageShader;
	if (!LoadShader(PS, damageBloodPS, "damageBloodPS.cso"))
		customBloodDamageShader.bActive = false;
	if (!LoadShader(VS, damageBloodVS, "damageBloodVS.cso"))
		customShader.bActive = false;


	PipelineStateDesc desc2[RENDERPASS_COUNT];
	PipelineState pso2[RENDERPASS_COUNT];
	for (int i = 0; i < RENDERPASS_COUNT; i++)
	{
		wiRenderer::AddPipelineDesc(desc2[i], i, PSTYPE_OBJECT, MaterialComponent::SHADERTYPE::SHADERTYPE_PBR, BLENDMODE_OPAQUE, OBJECTRENDERING_DOUBLESIDED_ENABLED, false, true, false);
		if (customShader.bActive)
		{
			if (i == RENDERPASS_MAIN)
			{
				desc2[i].ps = &damageBloodPS;
				desc2[i].vs = &damageBloodVS;
			}
		}
		wiRenderer::GetDevice()->CreatePipelineState(&desc2[i], &pso2[i]);
	}

	customBloodDamageShader.name = "Blood Damage";
	customBloodDamageShader.renderTypeFlags = RENDERTYPE_OPAQUE; // RENDERTYPE_TRANSPARENT;
	for (int i = 0; i < RENDERPASS_COUNT; i++)
		customBloodDamageShader.pso[i] = pso2[i];

	RegisterCustomShader(customBloodDamageShader);

	// GG: a river's water (the editor's spline rivers): the Water Object shader's flow and foam, coloured as the ocean is,
	// the scene beneath fading to the water colour with depth
	PipelineState psoriverwater;
	PipelineStateDesc descriverwater;
	CustomShader customRiverWaterShader;
	if (!LoadShader(PS, shaderRiverWaterPS, "objectPS_custom_riverwater.cso"))
		customRiverWaterShader.bActive = false;
	wiRenderer::AddPipelineDesc(descriverwater, RENDERPASS_MAIN, PSTYPE_OBJECT, MaterialComponent::SHADERTYPE::SHADERTYPE_PBR, BLENDMODE_ALPHA, OBJECTRENDERING_DOUBLESIDED_DISABLED, false, false, true);
	if (customRiverWaterShader.bActive)
	{
		descriverwater.ps = &shaderRiverWaterPS;
	}
	wiRenderer::GetDevice()->CreatePipelineState(&descriverwater, &psoriverwater);
	customRiverWaterShader.name = "River Water";
	customRiverWaterShader.renderTypeFlags = RENDERTYPE_TRANSPARENT;
	customRiverWaterShader.pso[RENDERPASS_MAIN] = psoriverwater;
	RegisterCustomShader(customRiverWaterShader);

	// GG: Rock Triplanar (customshaderid 7, after Wicked's Hologram and these six): the material's textures laid on in world space from three sides, as the
	// terrain's Steep Rock, for cliff and karst meshes that meet the terrain's rock; opaque, the standard pipelines for every
	// pass but the main pass's pixel shader
	CustomShader customRockShader;
	if (!LoadShader(PS, shaderRockTriplanarPS, "objectPS_custom_rocktriplanar.cso"))
		customRockShader.bActive = false;
	for (int i = 0; i < RENDERPASS_COUNT; i++)
	{
		PipelineStateDesc descrock;
		wiRenderer::AddPipelineDesc(descrock, i, PSTYPE_OBJECT, MaterialComponent::SHADERTYPE::SHADERTYPE_PBR, BLENDMODE_OPAQUE, OBJECTRENDERING_DOUBLESIDED_DISABLED, false, false, false);
		if (customRockShader.bActive && i == RENDERPASS_MAIN)
		{
			descrock.ps = &shaderRockTriplanarPS;
		}
		wiRenderer::GetDevice()->CreatePipelineState(&descrock, &customRockShader.pso[i]);
	}
	customRockShader.name = "Rock Triplanar";
	customRockShader.renderTypeFlags = RENDERTYPE_OPAQUE;
	RegisterCustomShader(customRockShader);


}