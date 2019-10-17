// Splash's 50P extension
// A0-A23 : PF0-PF7, PK0-PK7, PL0-PL7
// D0-D15 : PC0-PC7, PA0-PA7
// C0:PH0, C1:PE4, C2:PH4, C3:PH3
// C4:PE3, C5:PG5, C6:PH6, C7:PH5

static const char spa50pMenuItemReset[] PROGMEM = "Reset";
static const char spa50pMenuItemBack[] PROGMEM = "Back";

static const char spa50pMenuItem1[] PROGMEM = "16Bits";
static const char* const menuOptionsSPA50P[] PROGMEM = {spa50pMenuItemReset, spa50pMenuItem1};

static const char spa50p_16BitsMenuItem1[] PROGMEM = "Read";
static const char spa50p_16BitsMenuItem2[] PROGMEM = "Write";
static const char spa50P_16BitsMenuItem3[] PROGMEM = "Chip Erase";
static const char* const menuOptionsSPA50P_16Bits[] PROGMEM = {spa50p_16BitsMenuItem1, spa50p_16BitsMenuItem2, spa50P_16BitsMenuItem3, spa50pMenuItemBack};

static uint8_t spa50PMenu;

void setup_SPA50P()
{
  // A0 - A7
  DDRF = 0xff;
  // A8 - A15
  DDRK = 0xff;
  // A16 - A23
  DDRL = 0xff;

  // D0 - D15
  DDRC = 0x00;
  DDRA = 0x00;

  // controls
  DDRH |= ((1 << 0) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6));
  PORTH |= ((1 << 0) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6));
  
  DDRE |= ((1 << 3) | (1 << 4));
  PORTE |= ((1 << 3) | (1 << 4));

  DDRG |= (1 << 5);
  PORTG |= (1 << 5);

  mode = mode_SPA50P;
  spa50PMenu = 0;
}

void spa50pMenu()
{
  switch (spa50PMenu)
  {
    case 0: spa50p_Main_Menu(); return;
    case 1: spa50p_16Bits_Menu(); return;
  }
}

void spa50p_Main_Menu()
{
  uint8_t mainMenu;

  convertPgm(menuOptionsSPA50P, 2);
  mainMenu = question_box(F("SPA50P Menu"), menuOptions, 2, 1);

  switch (mainMenu)
  {
    case 1:
    {
      spa50PMenu = 1;
      break;
    }
    default: asm volatile ("  jmp 0"); break;
  }  
}

void spa50p_16Bits_Menu()
{
  uint32_t flashSize = spa50p_16Bits_DetectFlashMemory();
  
  if (flashSize == 0)
  {
    print_Error(F("E28XXXJ3A not detected"), false);
    spa50PMenu = 0;
    return;
  }
  
  uint8_t subMenu;
  convertPgm(menuOptionsSPA50P_16Bits, 4);
  subMenu = question_box(F("16Bits Menu"), menuOptions, 4, 0);

  switch (subMenu)
  {
    case 0:
    {
      spa50p_16Bits_ReadFlashMemory(flashSize);
      break;
    }
    case 1:
    {
      spa50p_16Bits_ProgramFlashMemory(flashSize);
      break;
    }
    case 2:
    {
      spa50p_16Bits_EraseFlashMemory(0x0, flashSize);
      break;
    }
    default: spa50PMenu = 0; return;
  }

  println_Msg(F(""));
  println_Msg(F("Press Button..."));
  display_Update();
  wait();
}

void spa50p_16Bits_ProgramFlashMemory(uint32_t flashSize)
{
  filePath[0] = 0;
  sd.chdir("/");
  fileBrowser(F("Select a file"));
  snprintf(filePath, FILEPATH_LENGTH, "%s/%s", filePath, fileName);

  display_Clear();
  print_Msg(F("Programming "));
  print_Msg(filePath);
  println_Msg(F("..."));
  display_Update();

  if (myFile.open(filePath, O_READ))
  {
    uint32_t start_addr = (flashSize - (myFile.fileSize() >> 1));
    uint32_t i, j, ba;

    for (ba = start_addr; ba < flashSize; ba += 0x10000)
    {
      draw_progressbar(ba - start_addr, flashSize - start_addr);
      
      dataOut_SPA50P();
      writeWord_SPA50P(ba, 0x0020);
      writeWord_SPA50P(ba, 0x00d0);

      dataIn_SPA50P();
      while (readWord_SPA50P(i) != 0x0080);

      for (i = 0; i < 0x10000; i += 16)
      {
        if ((i & 0xfff) == 0)
          PORTB ^= (1 << 4);

        myFile.read(sdBuffer, 32);
      
        dataOut_SPA50P();
        writeWord_SPA50P(ba, 0x00e8);

        dataIn_SPA50P();
        while (readWord_SPA50P(ba) != 0x0080);

        dataOut_SPA50P();
        writeWord_SPA50P(ba, 0x000f);

        for (j = 0; j < 16; j++)
          writeWord_SPA50P(ba + i + j, *((uint16_t*)(sdBuffer + (j << 1))));

        writeWord_SPA50P(ba, 0x00d0);

        dataIn_SPA50P();
        while (readWord_SPA50P(ba) != 0x0080);
      }
    }

    myFile.close();    

    dataOut_SPA50P();
    writeWord_SPA50P(0x0, 0x00ff);

    draw_progressbar(flashSize - start_addr, flashSize - start_addr);
    println_Msg(F("Programming finished!"));
  }
  else
  {
    print_Error(F("File doesn't exist"), false); 
  }
}

void spa50p_16Bits_ReadFlashMemory(uint32_t flashSize)
{
  uint32_t readSize = 0;

  if (flashSize >= 0x40000)
    strncpy(menuOptions[readSize++], "4M", 19);

  if (flashSize >= 0x80000)
    strncpy(menuOptions[readSize++], "8M", 19);

  if (flashSize >= 0x100000)
    strncpy(menuOptions[readSize++], "16M", 19);

  if (flashSize >= 0x200000)
    strncpy(menuOptions[readSize++], "32M", 19);

  if (flashSize >= 0x400000)
    strncpy(menuOptions[readSize++], "64M", 19);

  if (flashSize >= 0x800000)
    strncpy(menuOptions[readSize++], "128M", 19);

  switch (question_box(F("Length?"), menuOptions, readSize, 0))
  {
    case 0: readSize = 0x040000; break;
    case 1: readSize = 0x080000; break;
    case 2: readSize = 0x100000; break;
    case 3: readSize = 0x200000; break;
    case 4: readSize = 0x400000; break;
    case 5: readSize = 0x800000; break;
    default: return;
  }

  sd.chdir("/");

  EEPROM_readAnything(0, foldern);
  sd.mkdir("SPA50P", true);
  sd.chdir("SPA50P");
  sprintf(fileName, "SPA%d", foldern);
  strcat(fileName, ".bin");
  // write new folder number back to eeprom
  foldern = foldern + 1;
  EEPROM_writeAnything(0, foldern);

  if (!myFile.open(fileName, O_RDWR | O_CREAT))
    print_Error(F("Can't create file on SD"), true);  

  display_Clear();
  print_Msg(F("Saving "));
  print_Msg((readSize >> 16));
  print_Msg(F("Mb as "));
  print_Msg(fileName);
  println_Msg(F("..."));
  display_Update();

  uint32_t start_addr = flashSize - readSize;

  dataOut_SPA50P();
  writeWord_SPA50P(start_addr, 0x00ff);
  // reset second 64M
  if (readSize == 0x800000)
      writeWord_SPA50P(0x400000, 0x00ff);

  dataIn_SPA50P();
  for (uint32_t i = start_addr; i < flashSize; i += 256)
  {
    // blink LED
    if ((i & 0x1fff) == 0)
      PORTB ^= (1 << 4);

    if ((i & 0xffff) == 0)
      draw_progressbar(i - start_addr, flashSize - start_addr);
    
     for (uint32_t j = 0; j < 256; j++)
       *((uint16_t*)(sdBuffer + (j << 1))) = readWord_SPA50P(i + j);

     myFile.write(sdBuffer, 512);
  }

  myFile.close();

  draw_progressbar(flashSize - start_addr, flashSize - start_addr);
  println_Msg(F("Completed!"));
}

void spa50p_16Bits_EraseFlashMemory(uint32_t start_block_addr, uint32_t end_block_addr)
{
  display_Clear();
  println_Msg(F("Erasing..."));
  display_Update();

  uint32_t i;
  for (i = start_block_addr; i < end_block_addr; i += 0x10000)
  {
    // blink LED
    PORTB ^= (1 << 4);
    
    dataOut_SPA50P();
    writeWord_SPA50P(i, 0x0020);
    writeWord_SPA50P(i, 0x00d0);
    draw_progressbar(i - start_block_addr, end_block_addr - start_block_addr);

    dataIn_SPA50P();
    while (readWord_SPA50P(i) != 0x0080);
  }

  dataOut_SPA50P();
  writeWord_SPA50P(0x0, 0x00ff);  

  draw_progressbar(i - start_block_addr, end_block_addr - start_block_addr);
  println_Msg(F("Completed!"));
}

uint32_t spa50p_16Bits_DetectFlashMemory()
{
  dataOut_SPA50P();
  writeByte_SPA50P(0x000000, 0x0090);

  dataIn_SPA50P();

  if (readWord_SPA50P(0x0) == 0x89)
  {
    // return word length
    switch (readWord_SPA50P(0x1))
    {
      case 0x16: return 0x200000;
      case 0x17: return 0x400000;
      case 0x18: return 0x800000;
    }
  }

  return 0;
}

// C0:PH0, C1:PE4, C2:PH4, C3:PH3
// C4:PE3, C5:PG5, C6:PH6, C7:PH5
void writeByte_SPA50P(uint32_t addr, uint8_t data)
{
  PORTF = addr & 0xff;
  PORTK = (addr >> 8) & 0xff;
  PORTL = (addr >> 16) & 0xff;
  
  // CE = L, OE = H, WE = L
  PORTH &= ~((1 << 0) | (1 << 4));
  PORTE |= (1 << 4);
  NOP;

  PORTC = data;  

  // CE = OE = WE = H
  PORTH |= ((1 << 0) | (1 << 4));
  NOP; NOP; 
}

void writeWord_SPA50P(uint32_t addr, uint16_t data)
{
  PORTF = addr & 0xff;
  PORTK = (addr >> 8) & 0xff;
  PORTL = (addr >> 16) & 0xff;
  
  // CE = L, OE = H, WE = L
  PORTH &= ~((1 << 0) | (1 << 4));
  PORTE |= (1 << 4);
  NOP;

  PORTC = data & 0xff;
  PORTA = (data >> 8);

  // CE = OE = WE = H
  PORTH |= ((1 << 0) | (1 << 4));
  NOP; NOP;  
}

uint8_t readByte_SPA50P(uint32_t addr)
{
  PORTF = addr & 0xff;
  PORTK = (addr >> 8) & 0xff;
  PORTL = (addr >> 16) & 0xff;
  
  // CE = L, OE = H, WE = L
  PORTH &= ~((1 << 0));
  PORTH |= (1 << 4);
  PORTE &= ~(1 << 4);
  NOP; NOP; NOP;

  uint8_t ret = PINC;

  // CE = OE = WE = H
  PORTH |= (1 << 0);
  PORTE |= (1 << 4);

  return ret;
}

uint16_t readWord_SPA50P(uint32_t addr)
{
  PORTF = addr & 0xff;
  PORTK = (addr >> 8) & 0xff;
  PORTL = (addr >> 16) & 0xff;
  
  // CE = L, OE = H, WE = L
  PORTH &= ~((1 << 0));
  PORTH |= (1 << 4);
  PORTE &= ~(1 << 4);
  NOP; NOP; NOP;

  uint16_t ret = ((PINA << 8) | PINC);

  // CE = OE = WE = H
  PORTH |= (1 << 0);
  PORTE |= (1 << 4);

  return ret;  
}

void dataIn_SPA50P()
{
  DDRC = 0x00;
  PORTC = 0xff; 
  DDRA = 0x00;
  PORTA = 0xff;
}

void dataOut_SPA50P()
{
  DDRC = 0xff;
  DDRA = 0xff; 
}
