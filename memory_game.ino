#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Pins
const int ledPins[5] = {2, 3, 4, 5, 6};
const int selectBtn = 7;
const int scrollBtn = 8;
const int buzzer = 9;
bool playerCompleted[4] = {false, false, false, false};

// Game Constants
#define LEVELS 5
#define LED_ON_TIME 700
#define LED_OFF_TIME 300
#define INPUT_TIMEOUT 30000UL

// Difficulty Settings
const int baseLen[3] = {2, 3, 4}; // Easy, Medium, Hard
const char *diffNames[3] = {"Easy", "Medium", "Hard"};

// Melody
int melody[] = {262, 330, 392, 523, 392, 523};
int noteDur[] = {300, 300, 300, 600, 300, 600};
const int melodyLen = sizeof(melody) / sizeof(melody[0]);

// Game State
int mode = 0; // 0 = Single, 1 = Multi
int difficulty = 0;
int playerCount = 2;
int playerScores[4] = {0, 0, 0, 0};
unsigned long playerTimes[4] = {0, 0, 0, 0};

// Sequence Storage
int sequenceArr[4][LEVELS][20]; // [player][level][steps]

// ----------------- Helpers -----------------
void clearAndPrint(const String &l1, const String &l2 = "") {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(l1);
  if (l2 != "") {
    lcd.setCursor(0, 1);
    lcd.print(l2);
  }
}

void waitForSelectPress() {
  while (true) {
    if (digitalRead(selectBtn) == HIGH) {
      while (digitalRead(selectBtn) == HIGH) delay(10);
      delay(200);
      break;
    }
    delay(30);
  }
}

void generateSequence(int *arr, int length) {
  int last = -1;
  for (int i = 0; i < length; i++) {
    int val;
    do {
      val = random(0, 5);
    } while (val == last);
    arr[i] = val;
    last = val;
  }
}

void showSequence(int *arr, int length) {
  clearAndPrint("Watch carefully");
  for (int i = 0; i < 5; i++) pinMode(ledPins[i], OUTPUT);

  for (int i = 0; i < length; i++) {
    int led = arr[i];
    digitalWrite(ledPins[led], HIGH);
    tone(buzzer, 1000, 200);
    delay(LED_ON_TIME);
    digitalWrite(ledPins[led], LOW);
    delay(LED_OFF_TIME);
  }

  for (int i = 0; i < 5; i++) pinMode(ledPins[i], INPUT);
}

int waitForButton(unsigned long deadline) {
  int lastSec = -1;
  while (millis() < deadline) {
    unsigned long left = deadline - millis();
    int secLeft = (int)(left / 1000);
    if (secLeft != lastSec) {
      lcd.setCursor(0, 0);
      lcd.print("                ");
      lcd.setCursor(0, 0);
      lcd.print("Time: ");
      lcd.print(secLeft);
      lcd.print("s");
      lastSec = secLeft;
    }

    for (int i = 0; i < 5; i++) {
      pinMode(ledPins[i], INPUT);
      if (digitalRead(ledPins[i]) == HIGH) {
        while (digitalRead(ledPins[i]) == HIGH) delay(10);
        delay(80);
        return i;
      }
    }

    if (digitalRead(selectBtn) == HIGH) {
      while (digitalRead(selectBtn) == HIGH) delay(10);
      delay(80);
      return -2;
    }

    delay(20);
  }
  return -1;
}

int playerTurn(int *seq, int length, int playerNum) {
  unsigned long start = millis();
  unsigned long deadline = start + INPUT_TIMEOUT;
  int mistakes = 0;
  int index = 0;
  bool gaveInput = false;

  while (index < length) {
    int btn = waitForButton(deadline);

    if (btn == -1) {
      clearAndPrint("Time Up!");
      tone(buzzer, 200, 500);
      delay(1000);
      return 0;
    }

    if (btn == -2) { // Give up
      int penalty = -2 * mistakes;
      clearAndPrint("Gave Up", "Penalty: " + String(penalty));
      delay(1000);
      return penalty;
    }

    gaveInput = true;

    pinMode(ledPins[btn], OUTPUT);
    digitalWrite(ledPins[btn], HIGH);
    delay(150);
    digitalWrite(ledPins[btn], LOW);
    pinMode(ledPins[btn], INPUT);

    if (btn == seq[index]) {
      index++;
    } else {
      mistakes++;
      tone(buzzer, 200, 300);
      delay(300);
    }
  }

  unsigned long elapsed = millis() - start;
  playerTimes[playerNum] = elapsed;

  int score = 10 - (2 * mistakes);

  if (mode == 1) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Player ");
    lcd.print(playerNum + 1);
    lcd.setCursor(0, 1);
    lcd.print("Time: ");
    lcd.print(playerTimes[playerNum] / 1000);
    lcd.print("s");
    delay(2000);
  }
  playerCompleted[playerNum] = true;
  return score;
}

void playVictory() {
  clearAndPrint("You Win!");
  delay(900);
  for (int i = 0; i < melodyLen; i++) {
    for (int j = 0; j < 5; j++) {
      pinMode(ledPins[j], OUTPUT);
      digitalWrite(ledPins[j], HIGH);
    }
    tone(buzzer, melody[i], noteDur[i]);
    delay(noteDur[i]);
    for (int j = 0; j < 5; j++) {
      digitalWrite(ledPins[j], LOW);
      pinMode(ledPins[j], INPUT);
    }
    delay(120);
  }
}

void modeSelection() {
  int sel = 0;
  int lastShown = -1;
  while (true) {
    if (sel != lastShown) {
      clearAndPrint("Select Mode", sel == 0 ? "Single" : "Multi");
      lastShown = sel;
    }
    if (digitalRead(scrollBtn) == HIGH) {
      while (digitalRead(scrollBtn) == HIGH) delay(10);
      sel = 1 - sel;
      delay(200);
    }
    if (digitalRead(selectBtn) == HIGH) {
      while (digitalRead(selectBtn) == HIGH) delay(10);
      mode = sel;
      delay(200);
      break;
    }
    delay(30);
  }
}

void difficultySelection() {
  int sel = 0;
  int lastShown = -1;
  while (true) {
    if (sel != lastShown) {
      clearAndPrint("Difficulty", diffNames[sel]);
      lastShown = sel;
    }
    if (digitalRead(scrollBtn) == HIGH) {
      while (digitalRead(scrollBtn) == HIGH) delay(10);
      sel = (sel + 1) % 3;
      delay(200);
    }
    if (digitalRead(selectBtn) == HIGH) {
      while (digitalRead(selectBtn) == HIGH) delay(10);
      difficulty = sel;
      delay(200);
      break;
    }
    delay(30);
  }
}

void playerCountSelection() {
  int sel = 2;
  int lastShown = -1;
  while (true) {
    if (sel != lastShown) {
      clearAndPrint("Players", String(sel));
      lastShown = sel;
    }
    if (digitalRead(scrollBtn) == HIGH) {
      while (digitalRead(scrollBtn) == HIGH) delay(10);
      sel = sel == 4 ? 2 : sel + 1;
      delay(200);
    }
    if (digitalRead(selectBtn) == HIGH) {
      while (digitalRead(selectBtn) == HIGH) delay(10);
      playerCount = sel;
      delay(200);
      break;
    }
    delay(30);
  }
}

void showRankings() {
  clearAndPrint("Final Scores");
  delay(1500);
  for (int i = 0; i < playerCount; i++) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Player ");
    lcd.print(i + 1);
    lcd.setCursor(0, 1);
    lcd.print("Score: ");
    lcd.print(playerScores[i]);
    delay(2000);
  }
}

void setup() {
  lcd.init();
  lcd.backlight();

  pinMode(selectBtn, INPUT);
  pinMode(scrollBtn, INPUT);
  pinMode(buzzer, OUTPUT);
  for (int i = 0; i < 5; i++) pinMode(ledPins[i], INPUT);

  randomSeed(analogRead(A0));
}

void loop() {
  modeSelection();

  // Reset scores and times before starting a new game
  for (int i = 0; i < 4; i++) {
    playerScores[i] = 0;
    playerTimes[i] = 0;
  }

  if (mode == 0) {
    difficultySelection();
    int length = baseLen[difficulty];

    for (int level = 0; level < LEVELS; level++) {
      clearAndPrint("Level " + String(level + 1));
      delay(1000);

      generateSequence(sequenceArr[0][level], length);
      showSequence(sequenceArr[0][level], length);

      int score = playerTurn(sequenceArr[0][level], length, 0);
      if (score < 10) {
        clearAndPrint("Game Over");
        delay(1500);
        clearAndPrint("Sel to Restart");
        waitForSelectPress();
        return;
      }

      clearAndPrint("Level " + String(level + 1) + " Done");
      delay(1000);
      length++;
    }

    playVictory();
    clearAndPrint("Sel to Restart");
    waitForSelectPress();
    return;
  }

  // Multiplayer Mode
  playerCountSelection();
  int length = 4;

  for (int level = 0; level < LEVELS; level++) {
    clearAndPrint("Level " + String(level + 1));
    delay(1000);

    for (int p = 0; p < playerCount; p++) {
      clearAndPrint("Player " + String(p + 1), "Get Ready");
      delay(1000);

      generateSequence(sequenceArr[p][level], length);
      showSequence(sequenceArr[p][level], length);

      int score = playerTurn(sequenceArr[p][level], length, p);
      playerScores[p] += score;

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Player ");
      lcd.print(p + 1);
      lcd.setCursor(0, 1);
      lcd.print("Score: ");
      lcd.print(playerScores[p]);
      delay(2000);
    }

   int fastest = -1;
unsigned long bestTime = 4294967295UL;

for (int p = 0; p < playerCount; p++) {
  if (playerCompleted[p]) {
    if (playerTimes[p] < bestTime) {
      bestTime = playerTimes[p];
      fastest = p;
    }
  }
}

if (fastest != -1) {
  playerScores[fastest] += 10;
  clearAndPrint("Fastest: P" + String(fastest + 1), "+10 pts");
  delay(2000);
}



    // Highest scorer so far
    int top = 0;
    for (int p = 1; p < playerCount; p++) {
      if (playerScores[p] > playerScores[top]) {
        top = p;
      }
    }
    clearAndPrint("Top Score: P" + String(top + 1), String(playerScores[top]) + " pts");
    delay(2000);

    length++;
  }

  showRankings();
  clearAndPrint("Sel to Restart");
  waitForSelectPress();
}












