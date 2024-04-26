
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define CW_TEXT_START_X             5
#define CW_TEXT_START_Y             449                   // 480 * 0.97 = 465 - height = 465 - 16 = 449
#define CW_MESSAGE_WIDTH            WATERFALL_W   // 512
#define CW_MESSAGE_HEIGHT           16                    // tft.getFontHeight()

#define IIR_CW_ORDER              8
#define IIR_CW_NUMSTAGES          4

extern arm_biquad_cascade_df2T_instance_f32 S1_CW_Filter;
extern arm_biquad_cascade_df2T_instance_f32 S1_CW_AudioFilter1;
extern arm_biquad_cascade_df2T_instance_f32 S1_CW_AudioFilter2;
extern arm_biquad_cascade_df2T_instance_f32 S1_CW_AudioFilter3;
extern arm_biquad_cascade_df2T_instance_f32 S1_CW_AudioFilter4;
extern arm_biquad_cascade_df2T_instance_f32 S1_CW_AudioFilter5;
extern float32_t CW_Filter_state[];
extern float32_t CW_AudioFilter1_state[];
extern float32_t CW_AudioFilter2_state[];
extern float32_t CW_AudioFilter3_state[];
extern float32_t CW_AudioFilter4_state[];
extern float32_t CW_AudioFilter5_state[];

extern float32_t CW_AudioFilterCoeffs1[];
extern float32_t CW_AudioFilterCoeffs2[];
extern float32_t CW_AudioFilterCoeffs3[];
extern float32_t CW_AudioFilterCoeffs4[];
extern float32_t CW_AudioFilterCoeffs5[];

extern float32_t CW_Filter_Coeffs[];
//extern float32_t HP_DC_Filter_Coeffs[];
extern float32_t HP_DC_Filter_Coeffs2[];

extern float32_t aveCorrResult;
extern float32_t aveCorrResultR;
extern float32_t aveCorrResultL;

extern float goertzelMagnitude;

extern arm_fir_instance_f32 FIR_CW_DecodeL;
extern arm_fir_instance_f32 FIR_CW_DecodeR;

extern unsigned long ditLength;
extern unsigned long transmitDitLength;

extern int dahLength;

extern float32_t combinedCoeff;
extern float CWLevelTimer;
extern float CWLevelTimerOld;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void SetWPMFollowup();
void SetKeyType();
void SetKeyPowerUp();
void SelectCWFilter();
void DoPaddleFlip();
void SetSideToneVolumeSetup();
void SetSideToneVolumeValue();
void SetSideToneVolumeFollowup();
void SetTransmitDelayFollowup();

void DoCWReceiveProcessing();
void SetDitLength(int wpm);
void SetTransmitDitLength();

void ResetHistograms();
void DoGapHistogram(long gapLen);

void initCW(void);
