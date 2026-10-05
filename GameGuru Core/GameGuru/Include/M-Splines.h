//----------------------------------------------------
//--- GAMEGURU - M-Splines
//----------------------------------------------------

#pragma once

// GG: splines drawn on the terrain in the editor (Terrain Tools, Roads and Rivers), saved with the level in map.spl

void spline_imgui_panel ( float w );
bool spline_iseditmode ( void );
void spline_savedata ( void );
void spline_loaddata ( void );
void spline_deleteall ( void );
bool spline_librarypicking ( void );
bool spline_paintkeep ( void );
void spline_setpaintkeep ( bool bOn );
void spline_updatewater ( bool bForce = false );
void spline_updatemarkings ( void );
void spline_editorupdate ( void );
float spline_waterheightat ( float x, float z, int* pIsRiver );
float spline_riverturbulenceat ( float x, float z );
void spline_waterflowat ( float x, float z, float* pFlowX, float* pFlowZ );
float spline_riverwaterlevel ( float x, float z );
uint64_t spline_riverwaterinputs ( float minX, float minZ, float maxX, float maxZ );
