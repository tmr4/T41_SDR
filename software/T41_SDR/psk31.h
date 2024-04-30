
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern uint8_t psk31Buffer[256];

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

FLASHMEM bool setupPSK31();
bool setupPSK31Wav();
void exitPSK31();

char psk31_varicode_decoder_push(unsigned char symbol);
void dbpsk_decoder_c_u8(float32_t* input, uint8_t* output, int input_size);

void Psk31Decoder(float32_t* input, float32_t* output, int size);
void Psk31PhaseShiftDetector(float32_t* input, float32_t* output, int size);
