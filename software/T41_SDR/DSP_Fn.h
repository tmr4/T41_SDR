
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define NB_FFT_SIZE FFT_LENGTH/2

extern AudioEffectCompressor_F32 comp1, comp2;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void SetupMyCompressors(boolean use_HP_filter, float knee_dBFS, float comp_ratio, float attack_sec, float release_sec);
void NoiseBlanker(float32_t* inputsamples, float32_t* outputsamples);
void AGCLoadValues();
void AGCPrep();
void AGC();
void SetCompressionLevel();
void SetCompressionRatio();
void SetCompressionAttack();
void SetCompressionRelease();
