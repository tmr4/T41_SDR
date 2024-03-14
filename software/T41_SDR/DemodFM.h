
extern const uint32_t WFM_BLOCKS;
extern float32_t UKW_buffer_1[];

void setupFM();
void GetFMSample();
void DemodFM();

float deemphasis_wfm_ff (float* input, float* output, int input_size, int sample_rate, float last_output);
