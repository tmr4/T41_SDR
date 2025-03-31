
enum RA8875sizes { RA8875_480x272, RA8875_800x480, RA8875_800x480ALT, Adafruit_480x272, Adafruit_800x480 };
enum RA8875tsize { X16=0,X24,X32 };//0,1,2
enum RA8875writes { L1=0, L2, CGRAM, PATTERN, CURSOR };
enum RA8875boolean { LAYER1, LAYER2, TRANSPARENT, LIGHTEN, OR, AND, FLOATING };

#ifdef __cplusplus

// Colors preset (RGB565)
const uint16_t	RA8875_BLACK            = 0x0000;
const uint16_t 	RA8875_WHITE            = 0xFFFF;

const uint16_t	RA8875_RED              = 0xF800;
const uint16_t	RA8875_GREEN            = 0x07E0;
const uint16_t	RA8875_BLUE             = 0x001F;

const uint16_t 	RA8875_CYAN             = RA8875_GREEN | RA8875_BLUE;//0x07FF;
const uint16_t 	RA8875_MAGENTA          = 0xF81F;
const uint16_t 	RA8875_YELLOW           = RA8875_RED | RA8875_GREEN;//0xFFE0;
const uint16_t 	RA8875_LIGHT_GREY 		= 0xB5B2; // the experimentalist
const uint16_t 	RA8875_LIGHT_ORANGE 	= 0xFC80; // the experimentalist
const uint16_t 	RA8875_DARK_ORANGE 		= 0xFB60; // the experimentalist
const uint16_t 	RA8875_PINK 			= 0xFCFF; // M.Sandercock
const uint16_t 	RA8875_PURPLE 			= 0x8017; // M.Sandercock
const uint16_t 	RA8875_GRAYSCALE 		= 2113;//grayscale30 = RA8875_GRAYSCALE*30

class RA8875 {
 public:
  RA8875() {}
	void begin(const enum RA8875sizes s,uint8_t colors=16, uint32_t SPIMaxSpeed = (uint32_t)-1, uint32_t SPIMaxReadSpeed = (uint32_t)-1 ) {}
  void print(const char s[]) {}
  void println(const char s[]) {}
  void printf(const char s[]) {}
  void print(int) {}
  void print(unsigned int) {}
  void print(long) {}
  void print(double, int=2) {}
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {}
	void setTextColor(uint16_t fcolor, uint16_t bcolor) {}
	void setTextColor(uint16_t fcolor) {}
	void clearScreen(uint16_t fcolor) {}
	void setCursor(int16_t x, int16_t y,bool autocenter=false) {}
  void writeTo(enum RA8875writes d) {}
	void setFontScale(uint8_t scale) {}
	void setFontScale(uint8_t xscale,uint8_t yscale) {}
	void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {}
	uint8_t getFontWidth(boolean inColums=false) { return 0; }
	uint8_t getFontHeight(boolean inRows=false) { return 0; }
	void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {}
	void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {}
	void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {}
	void BTE_move(int16_t SourceX, int16_t SourceY, int16_t Width, int16_t Height, int16_t DestX, int16_t DestY, uint8_t SourceLayer=0, uint8_t DestLayer=0) {}
  uint8_t readStatus(void) { return 0; }
  void writeRect(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *pcolors) {}
	void fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {}
	void useLayers(boolean on) {}
  void layerEffect(enum RA8875boolean efx) {}
	uint16_t width(bool absolute=false) const { return 0; }
	uint16_t height(bool absolute=false) const { return 0; }
  void drawPixels(uint16_t p[], uint16_t count, int16_t x, int16_t y) {}
  void drawLineAngle(int16_t x, int16_t y, int16_t angle, uint16_t length, uint16_t color,int offset = -90) {}
  void clearMemory() {}
  void fillWindow(uint16_t color=0) {}
  inline uint16_t Color565(uint8_t r,uint8_t g,uint8_t b) { return 0; }
  void setForegroundColor(uint16_t color) {}
  void setRotation(uint8_t) {}
  void drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {}
};
#endif // __cplusplus
