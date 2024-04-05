
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define input_gulp_size 1024

extern q15_t ft8_dsp_buffer[] __attribute__ ((aligned (4)));

extern int ft8_flag, ft8_decode_flag;

extern bool ft8Init;

extern int DSP_Flag;
extern int master_decoded;

extern int num_decoded_msg;

extern int FT_8_counter;
extern const int kMax_decoded_messages;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void process_FT8_FFT();
int ft8_decode(void);
void display_details(int decoded_messages, int message_limit);
void update_synchronization();

void setupFT8();

void auto_sync_FT8();

bool readWave(float32_t *buf, int sizeBuf);
void setupFT8Wav();
