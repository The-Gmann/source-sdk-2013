#ifndef __DYNAMICSKY_H
#define __DYNAMICSKY_H

#pragma once

void R_UnloadSkys(void);
void R_LoadSkys(void);
void R_DrawSkyBox(float zFar, int nDrawFlags = 0x3F);

#endif