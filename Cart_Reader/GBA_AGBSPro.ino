//******************************************
// GBA AGBS PRO+G MODULE
// http://web.archive.org/web/2006083180814/http://twdiy.idv.tw/gold/agbs_pro.html
//******************************************

#include "options.h"
#ifdef enable_GBX

#define AGBS_PRO_CAPACITY   0x1000000
#define AGBS_PRO_MAX_GAMES  2
#define AGBS_PRO_MAX_GAME_CAPACITY  (AGBS_PRO_CAPACITY / AGBS_PRO_MAX_GAMES)

#define AGBS_PRO_FILEPATH_LENGTH 80

typedef struct
{
  char filePath[AGBS_PRO_FILEPATH_LENGTH];
  char gameCode[4];
  byte needPatch;
  uint32_t fileSize;
  uint32_t fileBegin;
} AGBSProGameInfo;

static const char gbaAGBSProMenuItem1[] PROGMEM = "Write Flash";
static const char gbaAGBSProMenuItem2[] PROGMEM = "Back";
static const char* const menuOptionsGBAAGBSPro[] PROGMEM = {gbaAGBSProMenuItem1, gbaAGBSProMenuItem2};

static const char gbaAGBSProYesNoMenuItem1[] PROGMEM = "Yes";
static const char gbaAGBSProYesNoMenuItem2[] PROGMEM = "No";
static const char* const menuOptionsFlash2ndGame[] PROGMEM = {gbaAGBSProYesNoMenuItem1, gbaAGBSProYesNoMenuItem2};

bool setup_AGBSPro()
{
  display_Clear();
  display_Update();

  setROM_GBA();
  unlockAGBSPro();

  // now we can write to flash memory directly
  // check flash device type
  writeWord_GBA(0xaaa, 0xaaaa);
  writeWord_GBA(0x555, 0x5555);
  writeWord_GBA(0xaaa, 0x9090);
  word manufacture = readWord_GBA(0x000000);
  word device = readWord_GBA(0x000002);

  // back to read mode
  writeWord_GBA(0x0, 0xf0);

  if (manufacture != 0x0004 || device != 0x227e)
    return false;

  // test if gba.txt exists
  if (myFile.open("gba.txt", O_READ))
    myFile.close();
  else
  {
    println_Msg(F("GBA.txt missing..."));
    println_Msg(F("Game with 1024K Flash save"));
    println_Msg(F("won't work properly."));
    print_Error(F("Press button..."), false);
  }

  return true;
}

void gbaAGBSProMenu()
{
  uint8_t mainMenu;

  // Copy menuOptions out of progmem
  convertPgm(menuOptionsGBAAGBSPro, 2);
  mainMenu = question_box(F("AGBS PRO+G Menu"), menuOptions, 2, 0);

  // wait for user choice to come back from the question box menu
  switch (mainMenu)
  {
    case 0:
    {
      flashAGBSPro();
      break;
    }
    default:
    {
      setup_GBA();
      mode = mode_GBA;
      break;
    }
  }
}

// lots dirty hacks here to make file and save type selection easier
void flashAGBSPro()
{
  byte i = 0;
  byte gamesToFlash = AGBS_PRO_MAX_GAMES;
  AGBSProGameInfo games[AGBS_PRO_MAX_GAMES];
  char gameInfo[11];

  do
  {
agbs_pro_select_file:
    filePath[0] = '\0';
    sd.chdir("/");
    fileBrowser((i == 0 ? F("Select 1st gba file") : F("Select 2nd gba file")));
    display_Clear();
    display_Update();

    snprintf(games[i].filePath, AGBS_PRO_FILEPATH_LENGTH, "%s/%s", filePath, fileName);

    if (!myFile.open(games[i].filePath, O_READ))
    {
      println_Msg(F("Can't open file"));
      println_Msg(games[i].filePath);
      print_Error(F(""), true);
    }

    games[i].fileSize = myFile.fileSize();
    games[i].fileBegin = 0;

    // read gamecode
    myFile.seekCur(0xac);
    myFile.read(games[i].gameCode, 4);
    myFile.close();

    print_Msg(F("File size: "));
    print_Msg(games[i].fileSize / 0x20000);
    println_Msg(F(" Mbits"));

    if (games[i].fileSize > AGBS_PRO_CAPACITY)
    {
      println_Msg(F("File size exceeded!"));
      print_Error(F("Press Button..."), false);
      display_Update();
      wait();
      goto agbs_pro_select_file;
    }

    if (games[i].fileSize > AGBS_PRO_MAX_GAME_CAPACITY)
    {
      if (i == 0)
      {
        gamesToFlash = 1;
      }
      else
      {
        // 2nd game size should be <= 64Mbits
        println_Msg(F("File size exceeded!"));
        print_Error(F("Press Button..."), false);
        display_Update();
        wait();
        goto agbs_pro_select_file;
      }
    }

    // check save type
    if (myFile.open("gba.txt", O_READ))
    {
      while (myFile.available())
      {
        myFile.read(gameInfo, 11); // 11 bytes per game

        if (strncmp(gameInfo, games[i].gameCode, 4) == 0)
        {
          if (gameInfo[8] == 0x35)  // '5' means 1Mbits flash
          {
            // if selecting 2nd game, makes it as the 1st game
            if (i == 1)
            {
              strncpy(games[0].filePath, games[1].filePath, 64);
              strncpy(games[0].gameCode, games[1].gameCode, 4);
            }

            games[0].needPatch = 0x02;
            gamesToFlash = 1;

            println_Msg("");
            println_Msg(F("The save type of this"));
            println_Msg(F("game is 1Mbits Flash."));
            println_Msg(F("Only this game will"));
            println_Msg(F("going to be flashed."));
            println_Msg("");
          }
          else
          {
            games[i].needPatch = 0x00;
          }

          break;
        }
      }

      myFile.close();
    }

    println_Msg(F("Press Button..."));
    display_Update();
    wait();

    i++;

    if (i < gamesToFlash)
    {
      display_Clear();
      display_Update();

      convertPgm(menuOptionsFlash2ndGame, 2);
      if (question_box(F("Flash 2nd game?"), menuOptions, 2, 0) == 1)
        gamesToFlash = 1;
    }

  } while (i < gamesToFlash);

  // split into two game entry if game size > 64Mbits
  if (gamesToFlash == 1)
  {
    if (games[0].fileSize > AGBS_PRO_MAX_GAME_CAPACITY)
    {
      strncpy(games[1].filePath, games[0].filePath, AGBS_PRO_FILEPATH_LENGTH - 1);
      strncpy(games[1].gameCode, games[0].gameCode, 4);
      games[1].fileSize = games[0].fileSize - AGBS_PRO_MAX_GAME_CAPACITY;
      games[1].fileBegin = AGBS_PRO_MAX_GAME_CAPACITY;
      games[1].needPatch = 0x00;
      games[0].fileSize = AGBS_PRO_MAX_GAME_CAPACITY;
      gamesToFlash = 2;
    }
  }

  // start programming
  uint32_t base_addr, pos;

  for (i = 0, base_addr = 0; i < gamesToFlash; i++, base_addr += AGBS_PRO_MAX_GAME_CAPACITY)
  {
    display_Clear();
    display_Update();

    if (!myFile.open(games[i].filePath, O_READ))
      print_Error(F("Can't open file"), true);

    myFile.seekSet(games[i].fileBegin);
    pos = 0;

    println_Msg(games[i].filePath);
    println_Msg(F("Erasing..."));
    display_Update();

    erase29DL640E_AGBSPro(base_addr, (games[i].fileSize / 0x10000));
    setFastMode29DL640E_AGBSPro(base_addr);

    // prepare first block to flash
    // see if we need to patch header
    myFile.read(sdBuffer, 512);

    if (games[i].needPatch != 0x00)
    {
      println_Msg(F("Patching header..."));
      display_Update();
      patchRomHeader(sdBuffer, games[i].needPatch);
    }

    println_Msg(F("Programming... "));
    display_Update();

    do
    {
      // blink LED
      if ((pos & 0xfff) == 0)
        PORTB ^= (1 << 4);

      for (uint32_t s = 0; s < 512; s += 2)
        fastProgram29DL640E_AGBSPro(base_addr, pos + s, *((word*)(sdBuffer + s)));

      myFile.read(sdBuffer, 512);
      pos += 512;

    } while (pos < games[i].fileSize);

    resetFromFastMode29DL640E_AGBSPro(base_addr);

    myFile.close();
  }

  println_Msg(F("Program finished!"));
  println_Msg("");
  println_Msg(F("Press Button..."));
  display_Update();
  wait();
}

void fastProgram29DL640E_AGBSPro(uint32_t base_addr, uint32_t pa, uint16_t pd)
{
  pa += base_addr;

  writeWord_GBA(pa, 0xa0a0);
  writeWord_GBA(pa, pd);
  while (readWord_GBA(pa) != pd);
}

void setFastMode29DL640E_AGBSPro(uint32_t base_addr)
{
  writeWord_GBA(base_addr + 0xaaa, 0xaaaa);
  writeWord_GBA(base_addr + 0x555, 0x5555);
  writeWord_GBA(base_addr + 0xaaa, 0x2020);
}

void resetFromFastMode29DL640E_AGBSPro(uint32_t base_addr)
{
  writeWord_GBA(base_addr, 0x9090);
  writeWord_GBA(base_addr, 0xf0f0);
}

void erase29DL640E(uint32_t base_addr, uint32_t sector_addr)
{
  sector_addr += base_addr;

  writeWord_GBA(base_addr + 0xaaa, 0xaaaa);
  writeWord_GBA(base_addr + 0x555, 0x5555);
  writeWord_GBA(base_addr + 0xaaa, 0x8080);

  writeWord_GBA(base_addr + 0xaaa, 0xaaaa);
  writeWord_GBA(base_addr + 0x555, 0x5555);
  writeWord_GBA(sector_addr, 0x3030);

  while (readWord_GBA(sector_addr) != 0xffff);
}

void erase29DL640E_AGBSPro(uint32_t base_addr, uint32_t sectors)
{
#if 1
  uint32_t i, sa = 0;

  // handle sector 0x00
  if (sectors > 0)
  {
    for (i = 0; i < 0x10000; i += 0x2000)
    {
      PORTB ^= (1 << 4);
      erase29DL640E(base_addr, sa + i);
    }
  }

  // handle sector 0x01 to 0x7e
  for (i = 1; i < sectors && i < 0x7f; i++)
  {
    PORTB ^= (1 << 4);
    sa = (i << 16);
    erase29DL640E(base_addr, sa);
  }

  // handle sector 0x7f (if needed)
  if (i < sectors)
  {
    sa = (i << 16);
    for (i = 0; i < 0x10000; i += 0x2000)
    {
      PORTB ^= (1 << 4);
      erase29DL640E(base_addr, sa + i);
    }
  }
#else
  // handle first and last 8 8KB sectors
  sa = 0x7f0000;

  do
  {
    sa ^= 0x7f0000;

    for (uint32_t offset = 0x000000; offset < 0x010000; offset += 0x2000)
    {
      PORTB ^= (1 << 4);

      writeWord_GBA(base_addr + 0xaaa, 0xaaaa);
      writeWord_GBA(base_addr + 0x555, 0x5555);
      writeWord_GBA(base_addr + 0xaaa, 0x8080);

      writeWord_GBA(base_addr + 0xaaa, 0xaaaa);
      writeWord_GBA(base_addr + 0x555, 0x5555);
      writeWord_GBA(base_addr + sa + offset, 0x3030);

      while (readWord_GBA(base_addr + sa + offset) != 0xffff);
    }
  } while (sa != 0x7f0000);

  // handle rest sectors
  for (sa = 0x010000; sa < 0x7f0000; sa += 0x010000)
  {
    PORTB ^= (1 << 4);

    writeWord_GBA(base_addr + 0xaaa, 0xaaaa);
    writeWord_GBA(base_addr + 0x555, 0x5555);
    writeWord_GBA(base_addr + 0xaaa, 0x8080);

    writeWord_GBA(base_addr + 0xaaa, 0xaaaa);
    writeWord_GBA(base_addr + 0x555, 0x5555);
    writeWord_GBA(base_addr + sa, 0x3030);

    while (readWord_GBA(base_addr + sa) != 0xffff);
  }
#endif
}

void patchRomHeader(byte* header, byte type)
{
  if (header == NULL)
    return;

  header[0xbb] = type;

  type = 0x00;
  for (byte i = 0xa0; i <= 0xbc; i++)
    type -= header[i];

  type -= 0x19;
  header[0xbd] = type;
}

void unlockAGBSPro()
{
  // set addr = 0xff8000 with /CS_ROM and /CS_RAM = L
  DDRF = 0xFF;
  DDRK = 0xFF;
  DDRC = 0xFF;

  PORTF = 0xff;
  PORTK = 0x80;
  PORTC = 0x00;

  PORTH &= ~((1<<0) | (1 << 3));

  // set /CS_ROM and /CS_RAM = H
  PORTH |= ((1 << 0) | (1 << 3));

  // write first two words with 0xffff
  writeWord_GBA(0x000000, 0xffff);
  writeWord_GBA(0x000002, 0xffff);
}

#endif
