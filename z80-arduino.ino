const char PROG_NAME[] = "Z80 control program";
const char PROG_VER[]  = "ver 2.3";
const char PROG_AUTH[] = "s.nagahara";

#undef BOARD_MEGA        //ボードがUNOの場合
//#define BOARD_MEGA  1    //ボードがMEGAの場合

// 不要かもしれないが、念のため
// HIGH/LOWをそれぞれ1/0に変換する
#define  HLto10(n)  (((n)==HIGH) ? 1 : 0)

// 非ゼロ/ゼロをHIGH/LOWに変換する。正論理（NZ2PosHL）と負論理（NZ2NegHL）
#define  NZ2PosHL(n)  (((n)!=0) ? HIGH : LOW)
#define  NZ2NegHL(n)  (((n)!=0) ? LOW : HIGH)

// true/falseをHIGH/LOWに変換する。正論理（tf2PosHL）と負論理（tf2NegHL）
#define tf2PosHL(n)  ((n) ? HIGH : LOW)
#define tf2NegHL(n)  ((n) ? LOW : HIGH)

// PIN number
//この設定は接続のしかたにあわせて変えること
const int Z80_D0    =  2;
const int Z80_D1    =  3;
const int Z80_D2    =  4;
const int Z80_D3    =  5;
const int Z80_D4    =  6;
const int Z80_D5    =  7;
const int Z80_D6    =  8;
const int Z80_D7    =  9;

const int Z80_RD    = A4;
const int Z80_WR    = A5;
const int Z80_M1    = 10;
const int Z80_RESET = 11;
const int Z80_MREQ  = 12;
const int Z80_CLK   = 13;

const int Z80_A0    = A0;
const int Z80_A1    = A1;
const int Z80_A2    = A2;
const int Z80_A3    = A3;

#ifdef BOARD_MEGA
const int Z80_INT   = 22;
const int Z80_RFSH  = 23;
const int Z80_NMI   = 24;
const int Z80_BUSRQ = 25;
const int Z80_WAIT  = 26;
const int Z80_BUSAK = 27;
const int Z80_HALT  = 28;
const int Z80_IORQ  = 29;

const int Z80_A4    = 42;
const int Z80_A5    = 43;
const int Z80_A6    = 44;
const int Z80_A7    = 45;
const int Z80_A8    = 46;
const int Z80_A9    = 47;
const int Z80_A10   = 48;
const int Z80_A11   = 49;
const int Z80_A12   = 50;
const int Z80_A13   = 51;
const int Z80_A14   = 52;
const int Z80_A15   = 53;
#endif

// メインループの状態値
enum enum_cmd {
  CMD_IDLE,
  CMD_HALF,
  CMD_CYCLE1,
  CMD_STEP1,
  CMD_STEP2,
  CMD_GO1,
};
typedef enum enum_cmd cmd_t;

// データバスに書き込み中かどうかを表すフラグ
boolean WRITING_Z80_data = false;

// バス状態を出力するかどうかを表すフラグ
boolean OUTPUT_BUS_STATE = true;

// 割り込み応答
// ここでは決め打ちの値を返している
unsigned char BUS_INT_DATA = 0xf7; // RST 30H

// 各バスの状態を保持
struct {
  unsigned int  z_addr;
  unsigned char z_data;
  boolean z_mreq;
  boolean z_rd;
  boolean z_wr;
  boolean z_m1;
  boolean z_clk;
  boolean z_rfsh;
  boolean z_int;
  boolean z_busrq;
  boolean z_nmi;
  boolean z_busak;
  boolean z_wait;
  boolean z_iorq;
  boolean z_halt;
} Z80_stat;

// メモリ領域、I/O領域
#ifdef BOARD_MEGA
const int MAIN_MEM_SIZE = 1024;
// メモリ
unsigned char MAIN_mem[MAIN_MEM_SIZE];
const unsigned char MAIN_mem_org[MAIN_MEM_SIZE] = {
  0xFD, 0x2A, 0x1A, 0x00,  // LD IY, (001AH)   ;IYレジスタにカウンタのアドレスを設定
  0x31, 0x80, 0x00,        // LD SP, 0080H     ;スタックポインタを設定
  0x3E, 0x55,              // LD A, 55H        ;保存データをセット
  0xFD, 0x77, 0x01,        // LD (IY+01H), A   ;比較用に保存
  0xFD, 0x36, 0x01, 0x00,  // LD (IY+01H), 00H ;カウンタクリア
  0xFB,                    // EI               ;割り込み許可
  0xCD, 0x16, 0x00,        // CALL 0016H       ;カウントアップ・サブルーチン呼び出し
  0x18, 0xFB,              // JR 0011H         ;繰り返し
  0xFD, 0x34, 0x01,        // INC (IY+01H)     ;カウンタを+1
  0xC9,                    // RET              ;サブルーチン終了
  0x1C, 0x00,              // DW 001CH         ;IYレジスタの初期値
  0xAA,                    // DB 0AAH          ;空き領域
  0x33,                    // DB 33H           ;カウンタ
};

// I/O領域
const int MAIN_IO_SIZE = 256;
unsigned char MAIN_io[MAIN_IO_SIZE];

#else
// UNOの場合
const int MAIN_MEM_SIZE = 16;
// メモリ
unsigned char MAIN_mem[MAIN_MEM_SIZE];
const unsigned char MAIN_mem_org[MAIN_MEM_SIZE] = {
  0x2A, 0x0D, 0x00, // LD HL, (000DH) ;HLレジスタにカウンタのアドレスを設定
  0xF9,             // LD SP, HL      ;スタックポインタを設定
  0x36, 0x00,       // LD (HL), 00H   ;カウンタをクリア
  0xCD, 0x0B, 0x00, // CALL 000BH     ;カウントアップ・サブルーチン呼び出し
  0x18, 0xFB,       // JR 0006H       ;繰り返し
  0x34,             // INC (HL)       ;カウンタを+1
  0xC9,             // RET            ;サブルーチン終了
  0x0F, 0x00,       // DW 000FH       ;HLレジスタの初期値 兼 スタック領域
  0xFF,             // DB FFH         ;カウンタ
};

// I/O領域
const int MAIN_IO_SIZE = 16;
unsigned char MAIN_io[MAIN_IO_SIZE];
#endif

// ========== プログラム ==========
// メモリアドレスの上限をチェック
void checkMemADDR(unsigned int addr) {
  if (addr >= MAIN_MEM_SIZE) {
    Serial.println("=====");
    Serial.print("memory address error:");
    Serial.println(addr, HEX);
    Serial.println("=====");
  }
}

// I/Oアドレスの上限をチェック
void checkIOADDR(unsigned int addr) {
  if (addr >= MAIN_IO_SIZE) {
    Serial.println("=====");
    Serial.print("I/O address error:");
    Serial.println(addr, HEX);
    Serial.println("=====");
  }
}

// バス状態を表示
void serialPrintData() {
  static boolean inM1 = false;
  char s[32];

  if (!OUTPUT_BUS_STATE) {
    inM1 = Z80_stat.z_m1;
    return;
  }

  if (!inM1 && Z80_stat.z_m1) Serial.println("---");
  Serial.print(Z80_stat.z_clk ? "CLK=L " : "CLK=H ");

  sprintf(s, "ADDR=%04X ", Z80_stat.z_addr);
  Serial.print(s);

  sprintf(s, "DATA=%02X ", Z80_stat.z_data);
  Serial.print(s);
  
  Serial.print(Z80_stat.z_m1    ? "M1 "    : "   "   );
  Serial.print(Z80_stat.z_rfsh  ? "RFSH "  : "     " );
  Serial.print(Z80_stat.z_mreq  ? "MREQ "  : "     " );
  Serial.print(Z80_stat.z_iorq  ? "IORQ "  : "     " );
  Serial.print(Z80_stat.z_rd    ? "RD "    : "   "   );
  Serial.print(Z80_stat.z_wr    ? "WR "    : "   "   );
  Serial.print(Z80_stat.z_int   ? "INT "   : "    "  );
  Serial.print(Z80_stat.z_nmi   ? "NMI "   : "    "  );
  Serial.print(Z80_stat.z_busrq ? "BUSRQ " : "      ");
  Serial.print(Z80_stat.z_busak ? "BUSAK " : "      ");
  Serial.print(Z80_stat.z_wait  ? "WAIT "  : "     " );
  Serial.print(Z80_stat.z_halt  ? "HALT "  : "     " );

  Serial.println("");
  inM1 = Z80_stat.z_m1;
}

// 制御バスを読む
void readZ80Ctrl() {
  Z80_stat.z_mreq = (digitalRead(Z80_MREQ) == LOW);
  Z80_stat.z_rd   = (digitalRead(Z80_RD)   == LOW);
  Z80_stat.z_wr   = (digitalRead(Z80_WR)   == LOW);
  Z80_stat.z_m1   = (digitalRead(Z80_M1)   == LOW);
  Z80_stat.z_clk  = (digitalRead(Z80_CLK)  == LOW);
#ifdef BOARD_MEGA
  Z80_stat.z_rfsh = (digitalRead(Z80_RFSH) == LOW);
  Z80_stat.z_busak= (digitalRead(Z80_BUSAK)== LOW);
  Z80_stat.z_iorq = (digitalRead(Z80_IORQ) == LOW);
  Z80_stat.z_halt = (digitalRead(Z80_HALT) == LOW);
  Z80_stat.z_int  = (digitalRead(Z80_INT)  == LOW);
  Z80_stat.z_nmi  = (digitalRead(Z80_NMI)  == LOW);
  Z80_stat.z_busrq= (digitalRead(Z80_BUSRQ)== LOW);
  Z80_stat.z_wait = (digitalRead(Z80_WAIT) == LOW);
#endif
}

// 制御バスに書き込む
void writeZ80Ctrl() {
#ifdef BOARD_MEGA
  digitalWrite(Z80_INT,   tf2NegHL(Z80_stat.z_int  ));
  digitalWrite(Z80_NMI,   tf2NegHL(Z80_stat.z_nmi  ));
  digitalWrite(Z80_BUSRQ, tf2NegHL(Z80_stat.z_busrq));
  digitalWrite(Z80_WAIT,  tf2NegHL(Z80_stat.z_wait ));
#endif
}

// アドレスバスを読む
void readZ80Addr() {
  Z80_stat.z_addr =
       HLto10(digitalRead(Z80_A0 ))
    | (HLto10(digitalRead(Z80_A1 )) <<  1)
    | (HLto10(digitalRead(Z80_A2 )) <<  2)
    | (HLto10(digitalRead(Z80_A3 )) <<  3)
#ifdef BOARD_MEGA
    | (HLto10(digitalRead(Z80_A4 )) <<  4)
    | (HLto10(digitalRead(Z80_A5 )) <<  5)
    | (HLto10(digitalRead(Z80_A6 )) <<  6)
    | (HLto10(digitalRead(Z80_A7 )) <<  7)
    | (HLto10(digitalRead(Z80_A8 )) <<  8)
    | (HLto10(digitalRead(Z80_A9 )) <<  9)
    | (HLto10(digitalRead(Z80_A10)) << 10)
    | (HLto10(digitalRead(Z80_A11)) << 11)
    | (HLto10(digitalRead(Z80_A12)) << 12)
    | (HLto10(digitalRead(Z80_A13)) << 13)
    | (HLto10(digitalRead(Z80_A14)) << 14)
    | (HLto10(digitalRead(Z80_A15)) << 15)
#endif
    ;
}

// データバスを読む
void readZ80Data() {
  Z80_stat.z_data =
       HLto10(digitalRead(Z80_D0))
    | (HLto10(digitalRead(Z80_D1)) << 1)
    | (HLto10(digitalRead(Z80_D2)) << 2)
    | (HLto10(digitalRead(Z80_D3)) << 3)
    | (HLto10(digitalRead(Z80_D4)) << 4)
    | (HLto10(digitalRead(Z80_D5)) << 5)
    | (HLto10(digitalRead(Z80_D6)) << 6)
    | (HLto10(digitalRead(Z80_D7)) << 7);
}

// データバスに書き始める
void writeZ80Data() {
  unsigned char n;

  if (WRITING_Z80_data) {
    return;
  }
  WRITING_Z80_data = true;

  n = Z80_stat.z_data;

  pinMode(Z80_D0, OUTPUT);
  pinMode(Z80_D1, OUTPUT);
  pinMode(Z80_D2, OUTPUT);
  pinMode(Z80_D3, OUTPUT);
  pinMode(Z80_D4, OUTPUT);
  pinMode(Z80_D5, OUTPUT);
  pinMode(Z80_D6, OUTPUT);
  pinMode(Z80_D7, OUTPUT);

  digitalWrite(Z80_D0, NZ2PosHL(n & bit(0)));
  digitalWrite(Z80_D1, NZ2PosHL(n & bit(1)));
  digitalWrite(Z80_D2, NZ2PosHL(n & bit(2)));
  digitalWrite(Z80_D3, NZ2PosHL(n & bit(3)));
  digitalWrite(Z80_D4, NZ2PosHL(n & bit(4)));
  digitalWrite(Z80_D5, NZ2PosHL(n & bit(5)));
  digitalWrite(Z80_D6, NZ2PosHL(n & bit(6)));
  digitalWrite(Z80_D7, NZ2PosHL(n & bit(7)));
}

// データバスに書き終わる
void writeEndZ80Data() {
  if (! WRITING_Z80_data) {
    return;
  }

  pinMode(Z80_D0, INPUT_PULLUP);
  pinMode(Z80_D1, INPUT_PULLUP);
  pinMode(Z80_D2, INPUT_PULLUP);
  pinMode(Z80_D3, INPUT_PULLUP);
  pinMode(Z80_D4, INPUT_PULLUP);
  pinMode(Z80_D5, INPUT_PULLUP);
  pinMode(Z80_D6, INPUT_PULLUP);
  pinMode(Z80_D7, INPUT_PULLUP);

  WRITING_Z80_data = false;
}

// リセット出力
void writeRESET() {
  int i;

  digitalWrite(Z80_RESET, LOW);

  // RESET=Lを3クロック以上保持する
  for (i = 0; i < 10; i++) {
    digitalWrite(Z80_CLK, LOW);
    delayMicroseconds(1);
    digitalWrite(Z80_CLK, HIGH);
    delayMicroseconds(1);
  }

  digitalWrite(Z80_RESET, HIGH);
}

// ヘルプ表示
void printHelp() {
  Serial.println("? : Help, o : toggle Output");
  Serial.println("h : Half, c : Cycle, s : Step, g : Go, R : Reset");
#ifdef BOARD_MEGA
  Serial.println("i : toggle INT, n : toggle NMI, w : toggle WAIT, b : toggle BUSRQ");
  Serial.println("m : dump Memory, p : dump I/O, l : Load memory, x : load I/O");
#else
  Serial.println("m : dump Memory");
#endif
}

// 受信データを全て読み捨てる
void discardRxData() {
  int c;

  while ((c = Serial.read()) != -1) ;
}

// メモリ,I/O領域をaddrを中心に256バイト表示
void dumpArray(unsigned char *arrayType, int addr) {
  const int DUMP_SIZE = 256;
  const int LINE_SIZE = 16;
  int p;
  int arrayMax;
  char s[32];

  if (arrayType == MAIN_mem) {
    arrayMax = MAIN_MEM_SIZE;
    Serial.println("Memory");
  } else if (arrayType == MAIN_io) {
    arrayMax = MAIN_IO_SIZE;
    Serial.println("I/O");
  } else {
    return;
  }

  p = (addr & 0xfff0) - DUMP_SIZE/2;
  if (arrayMax - p < DUMP_SIZE) p = arrayMax - DUMP_SIZE;
  if (p < 0) p = 0;

  Serial.println("ADDR  00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F");
  Serial.println("-----------------------------------------------------");
  for (int y = 0; y < DUMP_SIZE/LINE_SIZE; y++) {
    sprintf(s, "%04X: ", p);
    Serial.print(s);
    for (int x = 0; x < LINE_SIZE; x++) {
      sprintf(s, "%02X ", arrayType[p+x]);
      Serial.print(s);
    }
    p += LINE_SIZE;
    Serial.println("");
    if (p >= arrayMax) break;
  }
  Serial.println("---");
}

#ifdef BOARD_MEGA
// Intel Hexファイルの読み込み
#define CHECK_EOL(n)  ((n)=='\r' || (n)=='\n')
int ihLen;
int ihAddr;
int ihType;
const int RXBUF_MAX = 530;
char rxBuf[RXBUF_MAX];
unsigned char ihDat[255];
const int LOADHEX_MEMORY = 0;
const int LOADHEX_IO = 1;

// 1行読み込み
void getLine() {
  int c;

  for (;;) {
    while (!Serial.available()) ;
    c = Serial.peek();
    if (c == ':') break;
    c = Serial.read();
  }
  for (int i = 0; i < RXBUF_MAX; i++) {
    while (!Serial.available()) ;
    c = Serial.read();
    if (CHECK_EOL(c)) c = 0;
    rxBuf[i] = c;
    if (c == 0) break;
  }
  if (c != 0) {
    Serial.println("getLine: EOL not found");
  }
}

// 16進数文字を値に
int hex2Val(char c) {
  int n;

  n = -1;
  if      (('0' <= c) && (c <= '9')) n = c - '0';
  else if (('A' <= c) && (c <= 'F')) n = c - 'A' + 10;
  else if (('a' <= c) && (c <= 'f')) n = c - 'a' + 10;
  else {
    Serial.print("hex2Val: invalid char: ");
    Serial.println(c, HEX);
  }
  return n;
}

// Intel Hexファイルを1行ずつ処理する
void processLine() {
  int ihSum;

  getLine();
  if (rxBuf[0] != ':') {
    Serial.println("processLine: invalid start char");
    return;
  }
  
  ihLen  = hex2Val(rxBuf[1]) *16
         + hex2Val(rxBuf[2]);
  ihAddr = hex2Val(rxBuf[3]) *16*16*16
         + hex2Val(rxBuf[4]) *16*16
         + hex2Val(rxBuf[5]) *16
         + hex2Val(rxBuf[6]);
  ihType = hex2Val(rxBuf[7]) *16
         + hex2Val(rxBuf[8]);
  ihSum = ihLen + ((ihAddr >> 8) & 0xff) + (ihAddr & 0xff) + ihType;
  for (int i = 0; i < ihLen; i++) {
    ihDat[i] = hex2Val(rxBuf[9+i*2])*16 + hex2Val(rxBuf[9+i*2+1]);
    ihSum += ihDat[i];
  }
  ihSum += hex2Val(rxBuf[9+ihLen*2])*16 + hex2Val(rxBuf[9+ihLen*2+1]);
  if (ihSum % 256 != 0) Serial.println("processLine: checksum error");
}

// Intel Hexファイル読み込みコマンド
void readIntelHex(int loadMode) {
  Serial.println("waiting Intel Hex file...");
  do {
    processLine();
    switch (ihType) {
      case 0x00: // データ
        for (int i = 0; i < ihLen; i++) {
          if (loadMode == LOADHEX_MEMORY) {
            checkMemADDR(ihAddr);
            MAIN_mem[ihAddr++] = ihDat[i];
          } else {
            checkIOADDR(ihAddr);
            MAIN_io[ihAddr++] = ihDat[i];
          }
        }
        break;
      case 0x01:  // EOF
        break;
      default:  // その他のタイプは処理しない
        Serial.println("readIntelHex: unknown type");
        break;
    }
  } while (ihType != 0x01);
  discardRxData();
  Serial.println("done");
}
#endif

// Z-80をリセットする
void execRESET() {
  int i;

  Serial.print("Reset ... ");

  writeEndZ80Data();

  for (i = 0; i < MAIN_MEM_SIZE; i++) {
    MAIN_mem[i] = MAIN_mem_org[i];
  }

  writeRESET();

  Z80_stat.z_addr = 0;
  Z80_stat.z_data = 0;
  Z80_stat.z_mreq = false;
  Z80_stat.z_rd   = false;
  Z80_stat.z_wr   = false;
  Z80_stat.z_m1   = false;
  Z80_stat.z_clk  = false;
  Z80_stat.z_rfsh = false;
  Z80_stat.z_int  = false;
  Z80_stat.z_busrq= false;
  Z80_stat.z_nmi  = false;
  Z80_stat.z_busak= false;
  Z80_stat.z_wait = false;
  Z80_stat.z_iorq = false;
  Z80_stat.z_halt = false;

  Serial.println("done");
}

// コマンド入力
cmd_t inputCMD() {
  cmd_t cmd;
  int c;

  cmd = CMD_IDLE;
  if (! Serial.available()) return cmd;
  c = Serial.read();
  switch (c) {
    case 'h':
      cmd = CMD_HALF;
      break;
    case 'c':
      cmd = CMD_CYCLE1;
      break;
    case 's':
      // 前の命令のM1サイクルかどうかで次の状態を決める
      cmd = (Z80_stat.z_m1) ? CMD_STEP1 : CMD_STEP2;
      break;
    case 'g':
      cmd = CMD_GO1;
      break;
    case 'o':
      OUTPUT_BUS_STATE = !OUTPUT_BUS_STATE;
      Serial.print("print bus state:");
      if (OUTPUT_BUS_STATE)
        Serial.println("ON");
      else
        Serial.println("OFF");
      break;
    case 'm':
      dumpArray(MAIN_mem, Z80_stat.z_addr);
      break;
    case 'R':
      execRESET();
      break;
    case '?':
      printHelp();
      break;
#ifdef BOARD_MEGA
    case 'i':
      Z80_stat.z_int = !Z80_stat.z_int;
      writeZ80Ctrl();
      serialPrintData();  // バス状態を表示
      break;
    case 'n':
      Z80_stat.z_nmi = !Z80_stat.z_nmi;
      writeZ80Ctrl();
      serialPrintData();  // バス状態を表示
      break;
    case 'w':
      Z80_stat.z_wait = !Z80_stat.z_wait;
      writeZ80Ctrl();
      serialPrintData();  // バス状態を表示
      break;
    case 'b':
      Z80_stat.z_busrq = !Z80_stat.z_busrq;
      writeZ80Ctrl();
      serialPrintData();  // バス状態を表示
      break;
    case 'p': // I/Oエリアの内容を表示
      dumpArray(MAIN_io, Z80_stat.z_addr);
      break;
    case 'l': // Intel Hexファイルのメモリへの読み込み
      readIntelHex(LOADHEX_MEMORY);
      break;
    case 'x': // Intel HexファイルのI/Oへの読み込み
      readIntelHex(LOADHEX_IO);
      break;
#endif
    default:
      printHelp();
      break;
  }
  discardRxData();
  return cmd;
}

// 半サイクル実行
void execHalfCycle() {
  // クロックを反転
  digitalWrite(Z80_CLK, (digitalRead(Z80_CLK) == LOW) ? HIGH : LOW);
  delayMicroseconds(1); // バス変化を待つ（たぶん不要）

  readZ80Ctrl();  // 制御バスを読み取る
  readZ80Addr();  // アドレスバスを読み取る
  if (Z80_stat.z_mreq) {
    if (Z80_stat.z_rd) {
      checkMemADDR(Z80_stat.z_addr);
      // RDの場合は指定アドレスのメモリ内容をデータバスに書く
      Z80_stat.z_data = MAIN_mem[Z80_stat.z_addr];
      writeZ80Data();
    } else {
      writeEndZ80Data();
    }
    
    if (Z80_stat.z_wr) {
      writeEndZ80Data(); // 念のため
      checkMemADDR(Z80_stat.z_addr);
      // WRの場合はデータバスを読み取って指定アドレスのメモリに書く
      readZ80Data();
      MAIN_mem[Z80_stat.z_addr] = Z80_stat.z_data;
    } else {
      //
    }
#ifdef BOARD_MEGA
  } else if (Z80_stat.z_iorq && Z80_stat.z_m1) { // 割り込み受付後の特別なM1サイクルの場合
    Z80_stat.z_data = BUS_INT_DATA;
    writeZ80Data();
  } else if (Z80_stat.z_iorq) { // 普通のI/O要求の場合
    Z80_stat.z_addr &= 0x00FF; // I/Oアクセスの場合、ここではアドレスの下位8ビットだけを使用する
    if (Z80_stat.z_rd) {
      checkIOADDR(Z80_stat.z_addr);
      // RDの場合は指定アドレスのメモリ内容をデータバスに書く
      Z80_stat.z_data = MAIN_io[Z80_stat.z_addr];
      writeZ80Data();
    } else {
      writeEndZ80Data();
    }

    if (Z80_stat.z_wr) {
      writeEndZ80Data(); // 念のため
      checkIOADDR(Z80_stat.z_addr);
      // WRの場合はデータバスを読み取って指定アドレスのメモリに書く
      readZ80Data();
      MAIN_io[Z80_stat.z_addr] = Z80_stat.z_data;
    } else {
      //
    }
#endif
  } else {
    writeEndZ80Data();
  }

  readZ80Data();  // データバスを読み取る
  serialPrintData();  // バス状態を表示
}

// 初期化
void setup() {
  // put your setup code here, to run once:
  pinMode(Z80_D0    , INPUT_PULLUP);
  pinMode(Z80_D1    , INPUT_PULLUP);
  pinMode(Z80_D2    , INPUT_PULLUP);
  pinMode(Z80_D3    , INPUT_PULLUP);
  pinMode(Z80_D4    , INPUT_PULLUP);
  pinMode(Z80_D5    , INPUT_PULLUP);
  pinMode(Z80_D6    , INPUT_PULLUP);
  pinMode(Z80_D7    , INPUT_PULLUP);
  pinMode(Z80_RD    , INPUT_PULLUP);
  pinMode(Z80_WR    , INPUT_PULLUP);
  pinMode(Z80_M1    , INPUT_PULLUP);
  pinMode(Z80_MREQ  , INPUT_PULLUP);

#ifdef BOARD_MEGA
  pinMode(Z80_RFSH  , INPUT_PULLUP);
  pinMode(Z80_BUSAK , INPUT_PULLUP);
  pinMode(Z80_IORQ  , INPUT_PULLUP);
  pinMode(Z80_HALT  , INPUT_PULLUP);

  pinMode(Z80_INT   , OUTPUT);
  pinMode(Z80_NMI   , OUTPUT);
  pinMode(Z80_BUSRQ , OUTPUT);
  pinMode(Z80_WAIT  , OUTPUT);
  digitalWrite(Z80_INT  , HIGH);
  digitalWrite(Z80_NMI  , HIGH);
  digitalWrite(Z80_BUSRQ, HIGH);
  digitalWrite(Z80_WAIT , HIGH);
#endif

  pinMode(Z80_A0    , INPUT_PULLUP);
  pinMode(Z80_A1    , INPUT_PULLUP);
  pinMode(Z80_A2    , INPUT_PULLUP);
  pinMode(Z80_A3    , INPUT_PULLUP);

#ifdef BOARD_MEGA
  pinMode(Z80_A4    , INPUT_PULLUP);
  pinMode(Z80_A5    , INPUT_PULLUP);
  pinMode(Z80_A6    , INPUT_PULLUP);
  pinMode(Z80_A7    , INPUT_PULLUP);
  pinMode(Z80_A8    , INPUT_PULLUP);
  pinMode(Z80_A9    , INPUT_PULLUP);
  pinMode(Z80_A10   , INPUT_PULLUP);
  pinMode(Z80_A11   , INPUT_PULLUP);
  pinMode(Z80_A12   , INPUT_PULLUP);
  pinMode(Z80_A13   , INPUT_PULLUP);
  pinMode(Z80_A14   , INPUT_PULLUP);
  pinMode(Z80_A15   , INPUT_PULLUP);
#endif

  pinMode(Z80_RESET, OUTPUT);
  digitalWrite(Z80_RESET, LOW);
  pinMode(Z80_CLK, OUTPUT);
  digitalWrite(Z80_CLK, HIGH);

  Serial.begin(115200);
  Serial.println("start");
  Serial.println(PROG_NAME);
  Serial.println(PROG_VER);
  Serial.println(PROG_AUTH);

  execRESET();

  Serial.println("setup done");
  printHelp();
}

// CLK=Lの場合は、あと半サイクル実行する
// CLK=Hの場合は、停止する
int stopIfClkHigh() {
  return ((Z80_stat.z_clk) ? CMD_HALF : CMD_IDLE);
}

// キー入力があれば停止する
int stopIfKeyIn(int oldCmd) {
  int cmd;

  cmd = oldCmd;
  if (Serial.available()) {
    discardRxData();
    cmd = stopIfClkHigh();
  }
  return cmd;
}
      
// メイン
void loop() {
  // put your main code here, to run repeatedly:
  static cmd_t cmd = CMD_IDLE;

  switch (cmd) {
    case CMD_IDLE:
      cmd = inputCMD();
      break;

    // 半サイクル実行
    case CMD_HALF:
      execHalfCycle();
      cmd = CMD_IDLE;
      break;

    case CMD_CYCLE1:// CYCLEコマンドの最初の半サイクル
      execHalfCycle();
      cmd = stopIfClkHigh();
      break;

    case CMD_STEP1: // STEPコマンドで前の命令のM1サイクルの間はここを回る
      execHalfCycle();
      if (! Z80_stat.z_m1) cmd = CMD_STEP2; // 前のM1サイクルが終われば次の段階へ
      else cmd = stopIfKeyIn(cmd);
      break;

    case CMD_STEP2: // STEPコマンドで次の命令のM1サイクルまでここを回る
      execHalfCycle();
      if (Z80_stat.z_m1) cmd = stopIfClkHigh();
      else cmd = stopIfKeyIn(cmd);
      break;

    case CMD_GO1:   // GOコマンドで何か入力があるまでここを回る
      execHalfCycle();
      cmd = stopIfKeyIn(cmd);
      break;

    // 念のため
    default:
      Serial.println("loop:default");
      cmd = CMD_IDLE;
      break;
  }
}

