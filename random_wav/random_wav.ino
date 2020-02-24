/*
 * This example plays every .WAV file it finds on the SD card in a loop
 */
#include <WaveHC.h>
#include <WaveUtil.h>

SdReader card;    // This object holds the information for the card
FatVolume vol;    // This holds the information for the partition on the card
FatReader root;   // This holds the information for the volumes root directory
WaveHC wave;      // This is the only wave (audio) object, since we will only play one at a time

uint8_t dirLevel; // indent level for file/dir names    (for prettyprinting)
dir_t dirBuf;     // buffer for directory reads

int MAX_WAVS = 60;
int MAX_FILENAME_LENGTH = 11;
char **wavNames;
int wavCount = 0;

/*
 * Define macro to put error messages in flash memory
 */
#define error(msg) error_P(PSTR(msg))

// Function definitions (we define them here, but the code is below)
void play(FatReader &dir);

//////////////////////////////////// SETUP
void setup() {
  Serial.begin(9600);           // set up Serial library at 9600 bps for debugging
  
  putstring("Free RAM: ");       // This can help with debugging, running out of RAM is bad
  Serial.println(FreeRam());

  //  if (!card.init(true)) { //play with 4 MHz spi if 8MHz isn't working for you
  if (!card.init()) {         //play with 8 MHz spi (default faster!)  
    error("Card init. failed!");  // Something went wrong, lets print out why
  }
  
  // enable optimize read - some cards may timeout. Disable if you're having problems
  card.partialBlockRead(true);
  
  // Now we will look for a FAT partition!
  uint8_t part;
  for (part = 0; part < 5; part++) {   // we have up to 5 slots to look in
    if (vol.init(card, part)) 
      break;                           // we found one, lets bail
  }
  if (part == 5) {                     // if we ended up not finding one  :(
    error("No valid FAT partition!");  // Something went wrong, lets print out why
  }
  
  // Lets tell the user about what we found
  putstring("Using partition ");
  Serial.print(part, DEC);
  putstring(", type is FAT");
  Serial.println(vol.fatType(), DEC);     // FAT16 or FAT32?
  
  // Try to open the root directory
  if (!root.openRoot(vol)) {
    error("Can't open root dir!");      // Something went wrong,
  }
  
  // Whew! We got past the tough parts.
  putstring_nl("Files found (* = fragmented):");

  // Print out all of the files in all the directories.
//  root.ls(LS_R | LS_FLAG_FRAGMENTED);
  root.ls();
  
  wavCount = countWavs(root);
  
  Serial.print("Found ");
  Serial.print(wavCount);
  Serial.println(" wav files.");
  

  // Random seed relies on noise from analog inputs.
  int r = 0;
  for(int i = A0; i <= A7; i++) {
    r += analogRead(i);
  }
  r += millis();
  Serial.print("Using random seed: ");
  Serial.println(r);
  randomSeed(r);
}

//////////////////////////////////// LOOP
void loop() {
  int index = random(wavCount);
  Serial.print("Selected wav #");
  Serial.print(index);
  Serial.print(" of ");
  Serial.println(wavCount);
  playWav(root, index);
}

/////////////////////////////////// HELPERS
/*
 * print error message and halt
 */
void error_P(const char *str) {
  PgmPrint("Error: ");
  SerialPrint_P(str);
  sdErrorCheck();
  while(1);
}
/*
 * print error message and halt if SD I/O error, great for debugging!
 */
void sdErrorCheck(void) {
  if (!card.errorCode()) return;
  PgmPrint("\r\nSD I/O error: ");
  Serial.print(card.errorCode(), HEX);
  PgmPrint(", ");
  Serial.println(card.errorData(), HEX);
  while(1);
}

int countWavs(FatReader &root) {
  root.rewind();

  FatReader file;
  int i = 0;
  // Read every file in the directory one at a time.
  while (root.readDir(dirBuf) > 0 && i < MAX_WAVS) {
    // Skip it if it's not a .wav file.
    if (strncmp_P((char *)&dirBuf.name[8], PSTR("WAV"), 3)) {
      continue;
    }
    
    Serial.print("Checking wav ");
    printEntryName(dirBuf);
    Serial.print(": ");
    
    if (!file.open(vol, dirBuf) || !wave.create(file)) {
      Serial.println("error!");
      continue;
    }

    Serial.println("ok.");
    i++;
  }
  return i;
}

void playWavFile(FatReader &file) {
  if (!wave.create(file)) {            // Figure out, is it a WAV proper?
    putstring(" Not a valid WAV");     // ok skip it
  } else {
    Serial.println();                  // Hooray it IS a WAV proper!
    wave.play();  

    uint8_t n = 0;
    while (wave.isplaying) {// playing occurs in interrupts, so we print dots in realtime
      putstring(".");
      if (!(++n % 32))Serial.println();
      delay(100);
    }       
    sdErrorCheck();
  }
}

void playWav(FatReader &root, int index) {
  root.rewind();

  FatReader file;
  int i = 0;
  // Read every wav file until we reach the specified index.
  while (root.readDir(dirBuf) > 0 && i < index) {
    // Skip it if it's not a .wav file.
    if (strncmp_P((char *)&dirBuf.name[8], PSTR("WAV"), 3)) {
      continue;
    }
    if (!file.open(vol, dirBuf) || !wave.create(file)) {
      continue;
    }
    
    i++;
  }

  if (!file.open(vol, dirBuf)) {        // open the file in the directory
    Serial.println("file.open failed");
    return;
  }
  // Play the wav file.
  Serial.print("Playing wav: ");
  printEntryName(dirBuf);
  Serial.println();
  playWavFile(file);
}
