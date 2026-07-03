// ============================================================================
//  Display.h  -  0,96" SSD1306 OLED (I2C): Live-Werte, Uhr, CO2-Verlaufsgraph
// ============================================================================
#pragma once
#include "config.h"
#include "Data.h"

bool displayBegin();
void displayShowBoot(const char* line1, const char* line2);
void displayRender(const StatusSnapshot& snap);   // komplettes UI zeichnen
