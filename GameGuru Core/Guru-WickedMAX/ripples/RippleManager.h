#pragma once

#include "wiGraphicsDevice.h"
#include "wiScene.h"

// GG: rain ripples drawn as one batch (SetRippleBatching): each ring a small record, all of them drawn with one instanced
// draw in the transparent pass, where each was a decal element with its own object and material before (a draw call and
// material updates per ring each frame). The look is the water ripple decal's: its image, its animation frames and the
// fade in and out over them, its size, additive
namespace Ripples
{
	void Initialize();
	void SetEnabled(bool bEnabled);
	bool IsEnabled();
	// the ripple decal's image file and animation: its frames across and down, how many a second
	void SetLook(const char* pImageFile, int iAcross, int iDown, float fFramesPerSecond);
	// a ring at a world position, its size along its surface (world units), lying on the surface with that normal, played
	// from frame iFrameStart to before iFrameEnd
	void Add(float fX, float fY, float fZ, float fSizeX, float fSizeY, float fNX, float fNY, float fNZ, int iFrameStart, int iFrameEnd);
	// the rings age (only while the game runs, as the decals did)
	void Update(float fSeconds);
	void Clear();
	// rings showing, the most there can be, and rings not made because all were in use
	void GetStats(int* piLive, int* piCapacity, int* piDropped);
	extern "C" void ripple_draw(const wiScene::CameraComponent& camera, wiGraphics::CommandList cmd);
}
