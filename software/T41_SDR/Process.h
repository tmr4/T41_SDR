
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern float32_t biquad_lowpass1_coeffs[];

extern uint8_t ANR_notch;
extern uint8_t ANR_notchOn;
extern int audioYPixel[];
extern float32_t audioMaxSquaredAve;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void InitAMDemodBiquadFilter();

bool ProcessIQData(bool updateSpectrumData = false);
void ProcessControls();
float32_t CalcSignalStrength();
