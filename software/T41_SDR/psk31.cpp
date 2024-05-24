// modified from: libcsdr.c from https://github.com/ha7ilm/csdr
// the library doesn't provide much detail on using the psk functions
// more detail is available here: https://sdr.hu/static/msc-thesis.pdf

#include "SDT.h"

#include "Demod.h"
#include "Display.h"
#include "psk31.h"
#include "Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

uint8_t psk31Buffer[256];

typedef struct psk31_varicode_item_s
{
    unsigned long long code;
    int bitcount;
    unsigned char ascii;
} psk31_varicode_item_t;

PROGMEM psk31_varicode_item_t psk31_varicode_items[] =
{
  { .code = 0b1010101011, .bitcount=10,   .ascii=0x00 }, //NUL, null
  { .code = 0b1011011011, .bitcount=10,   .ascii=0x01 }, //SOH, start of heading
  { .code = 0b1011101101, .bitcount=10,   .ascii=0x02 }, //STX, start of text
  { .code = 0b1101110111, .bitcount=10,   .ascii=0x03 }, //ETX, end of text
  { .code = 0b1011101011, .bitcount=10,   .ascii=0x04 }, //EOT, end of transmission
  { .code = 0b1101011111, .bitcount=10,   .ascii=0x05 }, //ENQ, enquiry
  { .code = 0b1011101111, .bitcount=10,   .ascii=0x06 }, //ACK, acknowledge
  { .code = 0b1011111101, .bitcount=10,   .ascii=0x07 }, //BEL, bell
  { .code = 0b1011111111, .bitcount=10,   .ascii=0x08 }, //BS, backspace
  { .code = 0b11101111,   .bitcount=8,    .ascii=0x09 }, //TAB, horizontal tab
  { .code = 0b11101,      .bitcount=5,    .ascii=0x0a }, //LF, NL line feed, new line
  { .code = 0b1101101111, .bitcount=10,   .ascii=0x0b }, //VT, vertical tab
  { .code = 0b1011011101, .bitcount=10,   .ascii=0x0c }, //FF, NP form feed, new page
  { .code = 0b11111,      .bitcount=5,    .ascii=0x0d }, //CR, carriage return (overwrite)
  { .code = 0b1101110101, .bitcount=10,   .ascii=0x0e }, //SO, shift out
  { .code = 0b1110101011, .bitcount=10,   .ascii=0x0f }, //SI, shift in
  { .code = 0b1011110111, .bitcount=10,   .ascii=0x10 }, //DLE, data link escape
  { .code = 0b1011110101, .bitcount=10,   .ascii=0x11 }, //DC1, device control 1
  { .code = 0b1110101101, .bitcount=10,   .ascii=0x12 }, //DC2, device control 2
  { .code = 0b1110101111, .bitcount=10,   .ascii=0x13 }, //DC3, device control 3
  { .code = 0b1101011011, .bitcount=10,   .ascii=0x14 }, //DC4, device control 4
  { .code = 0b1101101011, .bitcount=10,   .ascii=0x15 }, //NAK, negative acknowledge
  { .code = 0b1101101101, .bitcount=10,   .ascii=0x16 }, //SYN, synchronous idle
  { .code = 0b1101010111, .bitcount=10,   .ascii=0x17 }, //ETB, end of trans. block
  { .code = 0b1101111011, .bitcount=10,   .ascii=0x18 }, //CAN, cancel
  { .code = 0b1101111101, .bitcount=10,   .ascii=0x19 }, //EM, end of medium
  { .code = 0b1110110111, .bitcount=10,   .ascii=0x1a }, //SUB, substitute
  { .code = 0b1101010101, .bitcount=10,   .ascii=0x1b }, //ESC, escape
  { .code = 0b1101011101, .bitcount=10,   .ascii=0x1c }, //FS, file separator
  { .code = 0b1110111011, .bitcount=10,   .ascii=0x1d }, //GS, group separator
  { .code = 0b1011111011, .bitcount=10,   .ascii=0x1e }, //RS, record separator
  { .code = 0b1101111111, .bitcount=10,   .ascii=0x1f }, //US, unit separator
  { .code = 0b1,          .bitcount=1,    .ascii=0x20 }, //szóköz
  { .code = 0b111111111,  .bitcount=9,    .ascii=0x21 }, //!
  { .code = 0b101011111,  .bitcount=9,    .ascii=0x22 }, //"
  { .code = 0b111110101,  .bitcount=9,    .ascii=0x23 }, //#
  { .code = 0b111011011,  .bitcount=9,    .ascii=0x24 }, //$
  { .code = 0b1011010101, .bitcount=10,   .ascii=0x25 }, //%
  { .code = 0b1010111011, .bitcount=10,   .ascii=0x26 }, //&
  { .code = 0b101111111,  .bitcount=9,    .ascii=0x27 }, //'
  { .code = 0b11111011,   .bitcount=8,    .ascii=0x28 }, //(
  { .code = 0b11110111,   .bitcount=8,    .ascii=0x29 }, //)
  { .code = 0b101101111,  .bitcount=9,    .ascii=0x2a }, //*
  { .code = 0b111011111,  .bitcount=9,    .ascii=0x2b }, //+
  { .code = 0b1110101,    .bitcount=7,    .ascii=0x2c }, //,
  { .code = 0b110101,     .bitcount=6,    .ascii=0x2d }, //-
  { .code = 0b1010111,    .bitcount=7,    .ascii=0x2e }, //.
  { .code = 0b110101111,  .bitcount=9,    .ascii=0x2f }, ///
  { .code = 0b10110111,   .bitcount=8,    .ascii=0x30 }, //0
  { .code = 0b10111101,   .bitcount=8,    .ascii=0x31 }, //1
  { .code = 0b11101101,   .bitcount=8,    .ascii=0x32 }, //2
  { .code = 0b11111111,   .bitcount=8,    .ascii=0x33 }, //3
  { .code = 0b101110111,  .bitcount=9,    .ascii=0x34 }, //4
  { .code = 0b101011011,  .bitcount=9,    .ascii=0x35 }, //5
  { .code = 0b101101011,  .bitcount=9,    .ascii=0x36 }, //6
  { .code = 0b110101101,  .bitcount=9,    .ascii=0x37 }, //7
  { .code = 0b110101011,  .bitcount=9,    .ascii=0x38 }, //8
  { .code = 0b110110111,  .bitcount=9,    .ascii=0x39 }, //9
  { .code = 0b11110101,   .bitcount=8,    .ascii=0x3a }, //:
  { .code = 0b110111101,  .bitcount=9,    .ascii=0x3b }, //;
  { .code = 0b111101101,  .bitcount=9,    .ascii=0x3c }, //<
  { .code = 0b1010101,    .bitcount=7,    .ascii=0x3d }, //=
  { .code = 0b111010111,  .bitcount=9,    .ascii=0x3e }, //>
  { .code = 0b1010101111, .bitcount=10,   .ascii=0x3f }, //?
  { .code = 0b1010111101, .bitcount=10,   .ascii=0x40 }, //@
  { .code = 0b1111101,    .bitcount=7,    .ascii=0x41 }, //A
  { .code = 0b11101011,   .bitcount=8,    .ascii=0x42 }, //B
  { .code = 0b10101101,   .bitcount=8,    .ascii=0x43 }, //C
  { .code = 0b10110101,   .bitcount=8,    .ascii=0x44 }, //D
  { .code = 0b1110111,    .bitcount=7,    .ascii=0x45 }, //E
  { .code = 0b11011011,   .bitcount=8,    .ascii=0x46 }, //F
  { .code = 0b11111101,   .bitcount=8,    .ascii=0x47 }, //G
  { .code = 0b101010101,  .bitcount=9,    .ascii=0x48 }, //H
  { .code = 0b1111111,    .bitcount=7,    .ascii=0x49 }, //I
  { .code = 0b111111101,  .bitcount=9,    .ascii=0x4a }, //J
  { .code = 0b101111101,  .bitcount=9,    .ascii=0x4b }, //K
  { .code = 0b11010111,   .bitcount=8,    .ascii=0x4c }, //L
  { .code = 0b10111011,   .bitcount=8,    .ascii=0x4d }, //M
  { .code = 0b11011101,   .bitcount=8,    .ascii=0x4e }, //N
  { .code = 0b10101011,   .bitcount=8,    .ascii=0x4f }, //O
  { .code = 0b11010101,   .bitcount=8,    .ascii=0x50 }, //P
  { .code = 0b111011101,  .bitcount=9,    .ascii=0x51 }, //Q
  { .code = 0b10101111,   .bitcount=8,    .ascii=0x52 }, //R
  { .code = 0b1101111,    .bitcount=7,    .ascii=0x53 }, //S
  { .code = 0b1101101,    .bitcount=7,    .ascii=0x54 }, //T
  { .code = 0b101010111,  .bitcount=9,    .ascii=0x55 }, //U
  { .code = 0b110110101,  .bitcount=9,    .ascii=0x56 }, //V
  { .code = 0b101011101,  .bitcount=9,    .ascii=0x57 }, //W
  { .code = 0b101110101,  .bitcount=9,    .ascii=0x58 }, //X
  { .code = 0b101111011,  .bitcount=9,    .ascii=0x59 }, //Y
  { .code = 0b1010101101, .bitcount=10,   .ascii=0x5a }, //Z
  { .code = 0b111110111,  .bitcount=9,    .ascii=0x5b }, //[
  { .code = 0b111101111,  .bitcount=9,    .ascii=0x5c }, //backslash
  { .code = 0b111111011,  .bitcount=9,    .ascii=0x5d }, //]
  { .code = 0b1010111111, .bitcount=10,   .ascii=0x5e }, //^
  { .code = 0b101101101,  .bitcount=9,    .ascii=0x5f }, //_
  { .code = 0b1011011111, .bitcount=10,   .ascii=0x60 }, //`
  { .code = 0b1011,       .bitcount=4,    .ascii=0x61 }, //a
  { .code = 0b1011111,    .bitcount=7,    .ascii=0x62 }, //b
  { .code = 0b101111,     .bitcount=6,    .ascii=0x63 }, //c
  { .code = 0b101101,     .bitcount=6,    .ascii=0x64 }, //d
  { .code = 0b11,         .bitcount=2,    .ascii=0x65 }, //e
  { .code = 0b111101,     .bitcount=6,    .ascii=0x66 }, //f
  { .code = 0b1011011,    .bitcount=7,    .ascii=0x67 }, //g
  { .code = 0b101011,     .bitcount=6,    .ascii=0x68 }, //h
  { .code = 0b1101,       .bitcount=4,    .ascii=0x69 }, //i
  { .code = 0b111101011,  .bitcount=9,    .ascii=0x6a }, //j
  { .code = 0b10111111,   .bitcount=8,    .ascii=0x6b }, //k
  { .code = 0b11011,      .bitcount=5,    .ascii=0x6c }, //l
  { .code = 0b111011,     .bitcount=6,    .ascii=0x6d }, //m
  { .code = 0b1111,       .bitcount=4,    .ascii=0x6e }, //n
  { .code = 0b111,        .bitcount=3,    .ascii=0x6f }, //o
  { .code = 0b111111,     .bitcount=6,    .ascii=0x70 }, //p
  { .code = 0b110111111,  .bitcount=9,    .ascii=0x71 }, //q
  { .code = 0b10101,      .bitcount=5,    .ascii=0x72 }, //r
  { .code = 0b10111,      .bitcount=5,    .ascii=0x73 }, //s
  { .code = 0b101,        .bitcount=3,    .ascii=0x74 }, //t
  { .code = 0b110111,     .bitcount=6,    .ascii=0x75 }, //u
  { .code = 0b1111011,    .bitcount=7,    .ascii=0x76 }, //v
  { .code = 0b1101011,    .bitcount=7,    .ascii=0x77 }, //w
  { .code = 0b11011111,   .bitcount=8,    .ascii=0x78 }, //x
  { .code = 0b1011101,    .bitcount=7,    .ascii=0x79 }, //y
  { .code = 0b111010101,  .bitcount=9,    .ascii=0x7a }, //z
  { .code = 0b1010110111, .bitcount=10,   .ascii=0x7b }, //{
  { .code = 0b110111011,  .bitcount=9,    .ascii=0x7c }, //|
  { .code = 0b1010110101, .bitcount=10,   .ascii=0x7d }, //}
  { .code = 0b1011010111, .bitcount=10,   .ascii=0x7e }, //~
  { .code = 0b1110110101, .bitcount=10,   .ascii=0x7f }, //DEL
};

PROGMEM unsigned long long psk31_varicode_masklen_helper[] =
{
  0b0000000000000000000000000000000000000000000000000000000000000000,
  0b0000000000000000000000000000000000000000000000000000000000000001,
  0b0000000000000000000000000000000000000000000000000000000000000011,
  0b0000000000000000000000000000000000000000000000000000000000000111,
  0b0000000000000000000000000000000000000000000000000000000000001111,
  0b0000000000000000000000000000000000000000000000000000000000011111,
  0b0000000000000000000000000000000000000000000000000000000000111111,
  0b0000000000000000000000000000000000000000000000000000000001111111,
  0b0000000000000000000000000000000000000000000000000000000011111111,
  0b0000000000000000000000000000000000000000000000000000000111111111,
  0b0000000000000000000000000000000000000000000000000000001111111111,
  0b0000000000000000000000000000000000000000000000000000011111111111,
  0b0000000000000000000000000000000000000000000000000000111111111111,
  0b0000000000000000000000000000000000000000000000000001111111111111,
  0b0000000000000000000000000000000000000000000000000011111111111111,
  0b0000000000000000000000000000000000000000000000000111111111111111,
  0b0000000000000000000000000000000000000000000000001111111111111111,
  0b0000000000000000000000000000000000000000000000011111111111111111,
  0b0000000000000000000000000000000000000000000000111111111111111111,
  0b0000000000000000000000000000000000000000000001111111111111111111,
  0b0000000000000000000000000000000000000000000011111111111111111111,
  0b0000000000000000000000000000000000000000000111111111111111111111,
  0b0000000000000000000000000000000000000000001111111111111111111111,
  0b0000000000000000000000000000000000000000011111111111111111111111,
  0b0000000000000000000000000000000000000000111111111111111111111111,
  0b0000000000000000000000000000000000000001111111111111111111111111,
  0b0000000000000000000000000000000000000011111111111111111111111111,
  0b0000000000000000000000000000000000000111111111111111111111111111,
  0b0000000000000000000000000000000000001111111111111111111111111111,
  0b0000000000000000000000000000000000011111111111111111111111111111,
  0b0000000000000000000000000000000000111111111111111111111111111111,
  0b0000000000000000000000000000000001111111111111111111111111111111,
  0b0000000000000000000000000000000011111111111111111111111111111111,
  0b0000000000000000000000000000000111111111111111111111111111111111,
  0b0000000000000000000000000000001111111111111111111111111111111111,
  0b0000000000000000000000000000011111111111111111111111111111111111,
  0b0000000000000000000000000000111111111111111111111111111111111111,
  0b0000000000000000000000000001111111111111111111111111111111111111,
  0b0000000000000000000000000011111111111111111111111111111111111111,
  0b0000000000000000000000000111111111111111111111111111111111111111,
  0b0000000000000000000000001111111111111111111111111111111111111111,
  0b0000000000000000000000011111111111111111111111111111111111111111,
  0b0000000000000000000000111111111111111111111111111111111111111111,
  0b0000000000000000000001111111111111111111111111111111111111111111,
  0b0000000000000000000011111111111111111111111111111111111111111111,
  0b0000000000000000000111111111111111111111111111111111111111111111,
  0b0000000000000000001111111111111111111111111111111111111111111111,
  0b0000000000000000011111111111111111111111111111111111111111111111,
  0b0000000000000000111111111111111111111111111111111111111111111111,
  0b0000000000000001111111111111111111111111111111111111111111111111,
  0b0000000000000011111111111111111111111111111111111111111111111111,
  0b0000000000000111111111111111111111111111111111111111111111111111,
  0b0000000000001111111111111111111111111111111111111111111111111111,
  0b0000000000011111111111111111111111111111111111111111111111111111,
  0b0000000000111111111111111111111111111111111111111111111111111111,
  0b0000000001111111111111111111111111111111111111111111111111111111,
  0b0000000011111111111111111111111111111111111111111111111111111111,
  0b0000000111111111111111111111111111111111111111111111111111111111,
  0b0000001111111111111111111111111111111111111111111111111111111111,
  0b0000011111111111111111111111111111111111111111111111111111111111,
  0b0000111111111111111111111111111111111111111111111111111111111111,
  0b0001111111111111111111111111111111111111111111111111111111111111,
  0b0011111111111111111111111111111111111111111111111111111111111111,
  0b0111111111111111111111111111111111111111111111111111111111111111
};

const int n_psk31_varicode_items = sizeof(psk31_varicode_items) / sizeof(psk31_varicode_item_t);

unsigned long long status_shr = 0;
unsigned char output;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

//char psk31_varicode_decoder_push(unsigned long long* status_shr, unsigned char symbol)
char psk31_varicode_decoder_push(unsigned char symbol)
{
  status_shr = (status_shr << 1) | (!!symbol); //shift new bit in shift register

  Serial.print("\n     At decoder push ... "); Serial.println(status_shr);

  if((status_shr & 0xFFF) == 0) {
    Serial.print("\n     decoder push overflow "); Serial.print(status_shr); Serial.print(" ");
    return 0;
  }

  for(int i = 0; i < n_psk31_varicode_items; i++) {
    unsigned long long tmp1 = (psk31_varicode_items[i].code << 2);
    //unsigned long long tmp1 = (psk31_varicode_items[i].code << 0);
    unsigned long long tmp2 = (status_shr & psk31_varicode_masklen_helper[(psk31_varicode_items[i].bitcount + 4) & 63]);

    if(status_shr == 13) {
      Serial.print("          tmp1 = "); Serial.print(tmp1); Serial.print(" tmp2 = "); Serial.println(tmp2);
    }
    //if((psk31_varicode_items[i].code << 2) == (status_shr & psk31_varicode_masklen_helper[(psk31_varicode_items[i].bitcount + 4) & 63])) {
    if(tmp1 == tmp2) {
      status_shr = 0; // reset shift register
      Serial.print("****** Found a Symbol ******");
      return psk31_varicode_items[i].ascii;
    }
  }

  Serial.print(" ... Didn't find a symbol "); Serial.println(status_shr);
  return 0;
}

void psk31_varicode_encoder_u8_u8(unsigned char* input, unsigned char* output, int input_size, int output_max_size, int* input_processed, int* output_size)
{
  (*output_size)=0;
  for((*input_processed)=0; (*input_processed)<input_size; (*input_processed)++)
  {
    //fprintf(stderr, "ii = %d, input_size = %d, output_max_size = %d\n", *input_processed, input_size, output_max_size);
    for(int ci=0; ci<n_psk31_varicode_items; ci++) //ci: character index
    {
      psk31_varicode_item_t current_varicode = psk31_varicode_items[ci];
      if(input[*input_processed]==current_varicode.ascii)
      {
        //fprintf(stderr, "ci = %d\n", ci);
        if(output_max_size<current_varicode.bitcount+2) return;
        for(int bi=0; bi<current_varicode.bitcount+2; bi++) //bi: bit index
        {
          //fprintf(stderr, "bi = %d\n", bi);
          output[*output_size] = (bi<current_varicode.bitcount) ? (psk31_varicode_items[ci].code>>(current_varicode.bitcount-bi-1))&1 : 0;
          (*output_size)++;
          output_max_size--;
        }
        break;
      }
    }
  }
}

//void dbpsk_decoder_c_u8(complexf* input, unsigned char* output, int input_size)
void dbpsk_decoder_c_u8(float32_t* input, uint8_t* output, int input_size) {
  static float last_phase;
  for(int i = 0; i < input_size; i++)
  {
    //float phase = atan2(qof(input,i), iof(input,i));
    float phase = ApproxAtan2(input[i + 1], input[i]);
    float dphase = phase - last_phase;

    while(dphase < -PI) dphase += 2 * PI;
    while(dphase >= PI) dphase -= 2 * PI;
    if( (dphase > (PI / 2)) || (dphase < (-PI / 2)) ) {
      output[i] = 0;
    } else {
      output[i] = 1;
    }
    last_phase = phase;
  }
}

/*
//void timing_recovery_cc(complexf* input, complexf* output, int input_size, float* timing_error, int* sampled_indexes, timing_recovery_state_t* state)
void timing_recovery_cc(complexf* input, complexf* output, int input_size, timing_recovery_state_t* state) {
  // We always assume that the input starts at center of the first symbol cross before the first symbol.
  // Last time we consumed that much from the input samples that it is there.
  int correction_offset = state->last_correction_offset;
  int current_bitstart_index = 0;
  int num_samples_bit = state->decimation_rate;
  int num_samples_halfbit = state->decimation_rate / 2;
  int num_samples_quarterbit = state->decimation_rate / 4;
  int num_samples_earlylate_wing = num_samples_bit * state->earlylate_ratio;
  float error;
  int el_point_left_index, el_point_right_index, el_point_mid_index;
  int si = 0;

  while(true)
  {
    //the MathWorks style algorithm has correction_offset.
    //correction_offset = 0;
    if(current_bitstart_index + num_samples_halfbit * 3 >= input_size) break;

    if(correction_offset<=-num_samples_quarterbit*0.9 || correction_offset>=0.9*num_samples_quarterbit) {
      correction_offset = 0;
    }

    //should check if the sign of the correction_offset (or disabling it) has an effect on the EVM.
    //it is also a possibility to disable multiplying with the magnitude
    if(state->algorithm == TIMING_RECOVERY_ALGORITHM_EARLYLATE)
    {
      //bitstart index should be at symbol edge, maximum effect point is at current_bitstart_index + num_samples_halfbit
      el_point_right_index  = current_bitstart_index + num_samples_earlylate_wing * 3;
      el_point_left_index   = current_bitstart_index + num_samples_earlylate_wing * 1 - correction_offset;
      el_point_mid_index    = current_bitstart_index + num_samples_halfbit;
      output[si++] = input[el_point_mid_index];
    } else if(state->algorithm == TIMING_RECOVERY_ALGORITHM_GARDNER) {
      //maximum effect point is at current_bitstart_index
      el_point_right_index  = current_bitstart_index + num_samples_halfbit * 3;
      el_point_left_index   = current_bitstart_index + num_samples_halfbit * 1;
      el_point_mid_index    = current_bitstart_index + num_samples_halfbit * 2;
      output[si++] = input[el_point_left_index];
    }
    else break;

    error = ( iof(input, el_point_right_index) - iof(input, el_point_left_index) ) * iof(input, el_point_mid_index);

    if(state->use_q) {
      error += ( qof(input, el_point_right_index) - qof(input, el_point_left_index)) * qof(input, el_point_mid_index);
      error /= 2;
    }

    if(error>state->max_error) error=state->max_error;
    if(error<-state->max_error) error=-state->max_error;
    if(state->debug_every_nth>=0) {
      if(state->debug_every_nth==0 || state->debug_phase==0) {
        state->debug_phase = state->debug_every_nth;
      }
      else {
        state->debug_phase--;
      }
    }

    int error_sign = (state->algorithm == TIMING_RECOVERY_ALGORITHM_GARDNER) ? -1 : 1;
    correction_offset = num_samples_halfbit * error_sign * error * state->loop_gain;
    current_bitstart_index += num_samples_bit + correction_offset;

    if(si>=input_size) {
      break;
    }
  }

  state->input_processed = current_bitstart_index;
  state->output_size = si;
  state->last_correction_offset = correction_offset;
}
*/

#define PSK_MULT 20.0
//#define PSK_MULT 7.5
//#define PSK_MULT 6.0
//#define PSK_MULT 5.0
//#define PSK_THRESHOLD 3.0
//#define PSK_THRESHOLD 2.0
//#define PSK_THRESHOLD 1.8
//#define PSK_THRESHOLD 1.75
//#define PSK_THRESHOLD 1.5
#define PSK_THRESHOLD 1.0
#define PSK_TIME 768
//#define PSK_TIME 765
//#define PSK_TIME 762
#define PSK_WIGGLE 60
//#define PSK_WIGGLE -10
#define PSK_MIN 0.2

bool IsPhaseShift(float32_t last, float32_t current, float32_t next) {
  //if((last < 0 && last > -PSK_MIN && current > PSK_THRESHOLD && next < 0 && next > -PSK_MIN) || (last > 0 && last < PSK_MIN && current < -PSK_THRESHOLD && next > 0 && next < PSK_MIN)) {
  //if((last < 0.1 && last > -PSK_MIN && current > PSK_THRESHOLD && next < 0.1 && next > -PSK_MIN) || (last > -0.1 && last < PSK_MIN && current < -PSK_THRESHOLD && next > -0.1 && next < PSK_MIN)) {
  if((last < 0.1 && last > -PSK_MIN && current > PSK_THRESHOLD && next < 0.1 && next > -PSK_MIN) || (last > -0.1 && last < PSK_MIN && current < -PSK_THRESHOLD && next > -0.1 && next < PSK_MIN)) {
  //((last < 0.1 && last > -PSK_MIN && current > PSK_THRESHOLD && next < 0.1 && next > -PSK_MIN) ||
  // (last > -0.1 && last < PSK_MIN && current < -PSK_THRESHOLD && next > -0.1 && next < PSK_MIN)) {
    return true;
  }
  return false;
}

void PrintPSK(int psk31Count, int symCount, float32_t last, float32_t current, float32_t next) {
  char buff[10];

  Serial.print("symCount = "); Serial.print(symCount); Serial.print(" ");
  Serial.print("psk31Count = "); Serial.print(psk31Count); Serial.print(" ");
  dtostrf(last, 6, 2, buff); Serial.print(buff); Serial.print(", ");
  dtostrf(current, 6, 2, buff); Serial.print(buff); Serial.print(", ");
  dtostrf(next, 6, 2, buff); Serial.println(buff);
}

void PrintPSKBuffer(int count) {
  Serial.println("");
  for(int i = 0; i < count; i++) {
    Serial.print(psk31Buffer[i]);
  }
  Serial.println("\n");
}

// attributes 1s averaged over interval w/o a phase change
void Psk31PhaseShiftDetector2(float32_t* input, float32_t* output, int size) {
  static float32_t last = 0;
  static float32_t current = 0;
  static float32_t next = 0;
  static bool idle = false;
  static bool decoding = false;
  static int decodeCount = 0;
  static int psk31Count = 0;
  static int symCount = -1;
  static int lastSymbol = -1;
  static bool waiting = false;

  nfmdemod(input, output, size);

  // take the derivative of output
  for(int i = 0; i < size - 2; i++) {
    current = (output[i + 1] - output[i]) * PSK_MULT;
    next = (output[i + 2] - output[i + 1]) * PSK_MULT;

    if(IsPhaseShift(last, current, next)) {
      if(!decoding) {
        decoding = true;
        psk31Count = 0;
        symCount++;
        psk31Buffer[decodeCount++] = 0;
        lastSymbol = 0;
      } else if(waiting) {
        int tmp = psk31Count - PSK_TIME; // one time is due to the 0
        while(tmp > PSK_TIME) {
          psk31Buffer[decodeCount++] = 1;
          symCount++;
          Serial.print("1  -  from waiting loop, tmp = "); Serial.println(tmp);
          tmp -= PSK_TIME;
        }
        symCount++;
        Serial.print("0  -  from waiting loop, "); PrintPSK(tmp, symCount, last, current, next);
        psk31Buffer[decodeCount++] = 0;
        psk31Count = 0;
        waiting = false;
      } else if(psk31Count < PSK_TIME - PSK_WIGGLE) {
        Serial.print("     *** Spurious phase change ... "); PrintPSK(psk31Count, symCount, last, current, next);
      } else {
        symCount++;
        Serial.print("0  -  Phase change ... "); PrintPSK(psk31Count, symCount, last, current, next);
        psk31Buffer[decodeCount++] = 0;

        if(lastSymbol == 0 && !idle) {
          idle = true;
          PrintPSKBuffer(decodeCount);
          decodeCount = 0;
        }
        lastSymbol = 0;

        if(decodeCount == 256) {
          PrintPSKBuffer(256);
          decodeCount = 0;
        }

        psk31Count = 0;
      }
    } else {
      if(!decoding) {
        psk31Count = -1;
      } else if(psk31Count > PSK_TIME + PSK_WIGGLE) {
        waiting = true;
        if(idle) {
          idle = false;
        }
        //psk31Buffer[decodeCount++] = 1;
        //symCount++;
        //Serial.print("1  -  No phase change ... "); PrintPSK(psk31Count, symCount, last, current, next);
        //if(idle) {
        //  idle = false;
        //}
        //lastSymbol = 1;
        //psk31Count = 0;
      }
    }

    psk31Count++;
    last = current;
  }
}

// only looks for phase changes
void Psk31PhaseShiftDetector5(float32_t* input, float32_t* output, int size) {
  //static float32_t lastOutput = 0;
  static float32_t last = 0;
  static float32_t current = 0;
  static float32_t next = 0;
  static int psk31Count = 0;
  static int symCount = -1;
  int i = 0;

  nfmdemod(input, output, size);

  // take the derivative of output
  do {
    if(IsPhaseShift(last, current, next)) {
      symCount++;
      Serial.print("Phase change ... "); PrintPSK(psk31Count, symCount, last, current, next);
      psk31Count = 0;
    }

    psk31Count++;
    last = current;
    current = next;
    next = (output[i+1] - output[i]) * PSK_MULT;
  } while(++i < size);
  //lastOutput = output[255];
}

// only looks for phase changes
void Psk31PhaseShiftDetector(float32_t* input, float32_t* output, int size) {
  static float32_t t0 = 0;
  static float32_t t1 = 0;
  static float32_t t2 = 0;
  //static float32_t t3 = 0;
  static int psk31Count = 0;
  static int symCount = -1;
  float32_t last;
  float32_t current;
  float32_t next;

  nfmdemod(input, output, size);

  // take the derivative of output
  //for(int i = 0; i < size; i++) {
  for(int i = 0; i < size - 2; i++) {
    //t3 = t2;
    t2 = t1;
    t1 = t0;
    t0 = output[i];
    //last = (t3 - t2) * PSK_MULT;
    //current = (t2 - t1) * PSK_MULT;
    //next = (t1 - t0) * PSK_MULT;

    //t3 = output[i];
    //t2 = output[i];
    //t1 = output[i+1];
    //t0 = output[i+2];
    //last = (t2 - t3) * PSK_MULT;
    last = current;
    current = (t1 - t2) * PSK_MULT;
    next = (t0 - t1) * PSK_MULT;

    if(IsPhaseShift(last, current, next)) {
      symCount++;
      Serial.print("Phase change ... "); PrintPSK(psk31Count, symCount, last, current, next);
      psk31Count = 0;
    } /*
    else {
      //if(psk31Count > 17500 && psk31Count < 23000) {
      if(psk31Count < 4000) {
        Serial.print("   no phase change ... "); PrintPSK(psk31Count, symCount, last, current, next);
      }
      if(psk31Count % 768 == 0) symCount++;
    }
    */
    psk31Count++;
  }
}

// looks for periodic and spurious phase changes
void Psk31PhaseShiftDetector4(float32_t* input, float32_t* output, int size) {
  static float32_t last = 0;
  static float32_t current = 0;
  static float32_t next = 0;
  //static bool idle = false;
  static bool decoding = false;
  //static int decodeCount = 0;
  static int psk31Count = 0;
  static int symCount = -1;
  //static int lastSymbol = -1;

  nfmdemod(input, output, size);

  last = current;
  current = next;

  // take the derivative of output
  for(int i = 0; i < size - 1; i++) {
    next = (output[i+1] - output[i]) * PSK_MULT;

    if(IsPhaseShift(last, current, next)) {
      int tmp = abs(psk31Count) % PSK_TIME;

      if(!decoding) {
        decoding = true;
        psk31Count = 0;
        symCount++;
      } else if(tmp > PSK_WIGGLE) {
        Serial.print("     *** Spurious phase change ... "); PrintPSK(psk31Count, symCount, last, current, next);
      } else {
        symCount++;
        Serial.print("Phase change ... "); PrintPSK(psk31Count, symCount, last, current, next);
        psk31Count = 0;
      }
    }

    psk31Count++;
    last = current;
    current = next;
  }
  next = output[size-1];
  if(IsPhaseShift(last, current, next)) {
    int tmp = abs(psk31Count) % PSK_TIME;

    if(tmp > PSK_WIGGLE) {
      Serial.print("     *** Spurious phase change ... "); PrintPSK(psk31Count, symCount, last, current, next);
    } else {
      symCount++;
      Serial.print("Phase change ... "); PrintPSK(psk31Count, symCount, last, current, next);
      psk31Count = 0;
    }
  }
}

// attributes a 1 every PSK_TIME w/o a phase change
void Psk31PhaseShiftDetector1(float32_t* input, float32_t* output, int size) {
  static float32_t last = 0;
  static float32_t current = 0;
  static float32_t next = 0;
  static bool idle = false;
  static bool decoding = false;
  static int decodeCount = 0;
  static int psk31Count = 0;
  static int symCount = -1;
  static int lastSymbol = -1;

  nfmdemod(input, output, size);

  // take the derivative of output
  for(int i = 0; i < size - 2; i++) {
    current = (output[i + 1] - output[i]) * PSK_MULT;
    next = (output[i + 2] - output[i + 1]) * PSK_MULT;

    if(IsPhaseShift(last, current, next)) {
      if(!decoding) {
        decoding = true;
        psk31Count = 0;
        symCount++;
        psk31Buffer[decodeCount++] = 0;
        lastSymbol = 0;
      } else if(psk31Count < PSK_TIME - PSK_WIGGLE) {
        Serial.print("     *** Spurious phase change ... "); PrintPSK(psk31Count, symCount, last, current, next);
      } else {
        symCount++;
        Serial.print("0  -  Phase change ... "); PrintPSK(psk31Count, symCount, last, current, next);
        psk31Buffer[decodeCount++] = 0;

        if(lastSymbol == 0 && !idle) {
          idle = true;
          PrintPSKBuffer(decodeCount);
          decodeCount = 0;
        }
        lastSymbol = 0;

        if(decodeCount == 256) {
          PrintPSKBuffer(256);
          decodeCount = 0;
        }

        psk31Count = 0;
      }
    } else {
      if(!decoding) {
        psk31Count = -1;
      } else if(psk31Count == PSK_TIME) {
        psk31Buffer[decodeCount++] = 1;
        symCount++;
        Serial.print("1  -  No phase change ... "); PrintPSK(psk31Count, symCount, last, current, next);
        if(idle) {
          idle = false;
        }
        lastSymbol = 1;
        psk31Count = 0;
      }
    }

    psk31Count++;
    last = current;
  }
}

void Psk31Decoder(float32_t* input, float32_t* output, int size) {
  static float32_t last = 0;
  static float32_t current = 0;
  static float32_t next = 0;
  static bool idle = false;
  static bool decoding = false;
  static int decodeCount = 0;
  static int psk31Count = 0;
  static int symCount = -1;
  static int lastSymbol = -1;

  //Serial.print("Starting Psk31Decoder ... ");
  nfmdemod(input, output, size);

  // take the derivative of output
  for(int i = 0; i < size - 2; i++) {
    current = (output[i + 1] - output[i]) * PSK_MULT;
    next = (output[i + 2] - output[i + 1]) * PSK_MULT;

    //if(last < 0 && current > PSK_THRESHOLD && next < 0) {
    //if((last < 0 && current > PSK_THRESHOLD && next < 0) || (last > 0 && current < -PSK_THRESHOLD && next > 0)) {
    if(IsPhaseShift(last, current, next)) {
      //if(!idle || lastSymbol == 1)
      if(!idle) {
        Serial.print("   Phase change ... "); PrintPSK(psk31Count, symCount, last, current, next);
      }

      // we had a phase shift, reset psk31Count
      if(idle) {
        // we're idle and remain so
        lastSymbol = 0;
        psk31Count = 0;
      } else if(decoding) {
        if(lastSymbol == 0) {
          // last two symbols we're zeros, we're now idling
          idle = true;
          decoding = false;
          psk31Buffer[decodeCount++] = 0;
          Serial.print("0");
          symCount++;
          psk31Buffer[decodeCount++] = 0;
          Serial.print("0");
          symCount++;
          if(decodeCount > 0) {
            Serial.print(" decodeCount = "); Serial.print(decodeCount); Serial.println(" waiting to decode ...");
          }
          lastSymbol = 0;
          psk31Count = 0;
        } else if(lastSymbol == 1) {
          // we're decoding so we either have a 0 following a 1 or we're starting a idle sequency
          // first, fill in 1's for past
          do {
            psk31Buffer[decodeCount++] = 1;
            Serial.print("1");
            symCount++;
            psk31Count -= PSK_TIME;
          } while(psk31Count > 0);
          lastSymbol = 0;
          psk31Count = 0;
        }
      }
    } else {//if (psk31Count >= PSK_TIME) {
      /*
      if (psk31Count >= PSK_TIME + PSK_WIGGLE) {
      Serial.print("   No phase change ... "); PrintPSK(psk31Count, symCount, last, current, next);
      // no phase shift, we got a 1, reset psk31Count
      if(idle) {
        idle = false;
        decoding = true;
        psk31Buffer[decodeCount++] = 1;
        Serial.print("1");
      } else if(!decoding && lastSymbol == 0) {
        idle = false;
        decoding = true;
        psk31Buffer[decodeCount++] = 1;
        Serial.print("1");
        symCount++;
      } else if(decoding && lastSymbol == 0) {
        psk31Buffer[decodeCount++] = 0;
        Serial.print("0");
        symCount++;
        psk31Buffer[decodeCount++] = 1;
        Serial.print("1");
        symCount++;
      } else {
        psk31Buffer[decodeCount++] = 1;
        Serial.print("1");
        symCount++;
      }

      lastSymbol = 1;
      psk31Count = 0;
      */
    //} else if (psk31Count >= PSK_TIME - 10 && !idle && symCount >= 3 && symCount <= 4) {
      //Serial.print("else #1 "); PrintPSK(psk31Count, symCount, last, current, next);
    //} else if (psk31Count > 0 && psk31Count < 10 && !idle && symCount >= 3 && symCount <= 4) {
      //Serial.print("else #2 "); PrintPSK(psk31Count, symCount, last, current, next);

      if(psk31Count > PSK_TIME) {
        decoding = true;
        idle = false;
        lastSymbol = 1;
      }
    }

    psk31Count++;
    last = current;

    if(!decoding && (decodeCount > 0)) {
    //if((decodeCount > 0)) {
      //Serial.print("    starting to decode ---> ");
      //for(int i = decodeStart; i < decodeCount; i++) {
      for(int i = 0; i < decodeCount; i++) {
        //Serial.println(psk31Buffer[i]);
        if(psk31Buffer[i]) {
          Serial.print("1");
        } else {
          Serial.print("0");
        }

        char tmp = psk31_varicode_decoder_push(psk31Buffer[i]);
        if(tmp) {
          Serial.print(tmp);

          Serial.println("");
        }
      }
      decodeCount = 0;
      //Serial.println(" ... finished decoding loop");
    }
  }
  /*
  if(decodeCount > 0) {
    Serial.print("    dumping extra ---> ");
    for(int i = 0; i < decodeCount; i++) {
      if(psk31Buffer[i]) {
        Serial.print("1");
      } else {
        Serial.print("0");
      }

      char tmp = psk31_varicode_decoder_push(psk31Buffer[i]);
      if(tmp) {
        Serial.print(tmp);

        Serial.println("");
      }
    }
    decodeCount = 0;
    Serial.println(" ... finished dump");
  }
  */

  //Serial.println("... Exiting Psk31Decoder ");
}

FLASHMEM bool setupPSK31() {
  // set up message area
  // Erase waterfall in decode area
  tft.fillRect(WATERFALL_L, YPIXELS - 25 * 1, WATERFALL_W, 25 * 1 + 3, RA8875_BLACK);
  tft.writeTo(L2); // it's on layer 2 as well
  tft.fillRect(WATERFALL_L, YPIXELS - 25 * 1, WATERFALL_W, 25 * 1 + 3, RA8875_BLACK);
  tft.writeTo(L1);
  wfRows = WATERFALL_H - 25 * 1 - 3;

  return true;
}

FLASHMEM bool setupPSK31Wav() {
  int result;
  uint32_t slot_period = 12;
  uint32_t sample_rate = 8000;
  uint32_t num_samples = slot_period * sample_rate;
  //float32_t buf[256];

  result = load_wav("psk31.wav", num_samples); // abc_psk31.wav

  if(result != 0) {
    Serial.println("Invalid wave file!");
    return false;
  }

  // read 1/2 a buffer in to avoid end of buffer problems
  //readWave(buf, 128);

  return true;
}

FLASHMEM void exitPSK31() {
  // restore message area
  tft.fillRect(WATERFALL_L, YPIXELS - 20 * 6, WATERFALL_W, 25 * 5 + 3, RA8875_BLACK);
  wfRows = WATERFALL_H;
}
