#pragma once

#include "wiGraphicsDevice.h"
#include "wiScene.h"

// GG: distant lights (#220): every listed light far off drawn as a dot a pixel or two across, so a town miles away reads
// as lived in at night. One instanced draw of camera-facing dots in the transparent pass, additive, tested against the
// scene's depth, faded in past a distance (nearer, the real lights and the lamp heads show), faded into the fog and
// scaled by a global amount (the day and night). Each light carries a tag (0 to 255) that can be switched off (a power cut)
namespace DistantLights
{
	void Initialize();
	// a light at a world position, its colour (0 to 1), its size (world units, the size it is drawn at until that is
	// smaller than the smallest dot), its brightness and its tag; false when the list is full
	bool Add(float fX, float fY, float fZ, float fR, float fG, float fB, float fSize, float fBrightness, int iTag);
	// every light, or those with the tag
	void Clear(int iTag = -1);
	// all of them brighter or dimmer, 0 (none drawn) to any
	void SetAmount(float fAmount);
	float GetAmount();
	// a tag's lights drawn or not
	void SetTag(int iTag, bool bOn);
	// the smallest and largest dot in pixels, the distance they fade in from and are full by, how far each is drawn toward
	// the camera (so its own lamp's mesh doesn't hide it)
	void SetLook(float fMinPixels, float fMaxPixels, float fFadeFrom, float fFadeTo, float fPull);
	void GetLook(float* pMinPixels, float* pMaxPixels, float* pFadeFrom, float* pFadeTo, float* pPull);
	// lights listed, those whose tag is on (drawn), the most there can be
	void GetStats(int* piCount, int* piDrawn, int* piCapacity);
	extern "C" void distantlight_draw(const wiScene::CameraComponent& camera, wiGraphics::CommandList cmd);
}
