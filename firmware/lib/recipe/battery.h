#pragma once

// Charge left in a single Li-ion cell, 0-100, from its voltage in millivolts. Uses a typical
// discharge curve (flat around 3.7-3.8 V), so it is an estimate, best read at rest.
int batteryPercent(int millivolts);
