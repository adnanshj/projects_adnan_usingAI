// Cricket Scoreboard: 6-Digit Multiplexed Display with 4x2 Matrix Input
// This code integrates the column-scanning input logic with the 7-segment multiplexing display.

// ------------------------------------
// --- SCOREBOARD STATE VARIABLES ---
// ------------------------------------

// Game state variables
int totalRuns = 0;      // Max 999 (Displays 1-3)
int wickets = 0;        // Max 9 (Display 4)
int overs = 0;          // Displays 5 (O)
int balls = 0;          // Displays 6 (B) - value 0 to 5

// Flag for special rules
bool freeHitFlag = false; // True after a No Ball, prevents the next wicket from counting


// ------------------------------------
// --- DISPLAY PIN MAPPING (OUTPUTS) ---
// ------------------------------------

// Segment Pins (Shared by ALL displays):
// Pin 0=a, Pin 1=b, Pin 2=c, Pin 3=d, Pin 4=e, Pin 5=f, Pin 6=g
// NOTE: Pins 0 & 1 are used for segments, SERIAL COMMUNICATION IS DISABLED.
const int SEGMENT_PINS[] = {0, 1, 2, 3, 4, 5, 6};
const int NUM_SEGMENTS = 7;

// Common Cathode Pins (Control Pins - Active LOW = ON):
// Pin 7=Display 1 (Score Hundreds), Pin 8=Display 2 (Score Tens), ..., Pin 12=Display 6 (Balls)
const int CATHODE_PINS[] = {7, 8, 9, 10, 11, 12};
const int NUM_DIGITS = 6;

// Segment map for common cathode (HIGH = ON). 
// Segment definition: {a, b, c, d, e, f, g}
const byte SEGMENT_MAP[10][NUM_SEGMENTS] = {
    // a, b, c, d, e, f, g (Pins 0 to 6)
    {1, 1, 1, 1, 1, 1, 0}, // 0
    {0, 1, 1, 0, 0, 0, 0}, // 1
    {1, 1, 0, 1, 1, 0, 1}, // 2
    {1, 1, 1, 1, 0, 0, 1}, // 3
    {0, 1, 1, 0, 0, 1, 1}, // 4
    {1, 0, 1, 1, 0, 1, 1}, // 5
    {1, 0, 1, 1, 1, 1, 1}, // 6
    {1, 1, 1, 0, 0, 0, 0}, // 7
    {1, 1, 1, 1, 1, 1, 1}, // 8
    {1, 1, 1, 1, 0, 1, 1}  // 9
};

// ------------------------------------
// --- INPUT MATRIX MAPPING (I/O) ---
// ------------------------------------

// Columns (Analog Pins as Digital OUTPUTS - Drivers)
const int COL_PINS[] = {A0, A1};
const int NUM_COLS = 2;

// Rows (Analog Pins as Digital INPUTS - Readers)
// ASSUMES PULL-DOWN RESISTORS ON ALL THESE PINS (A2-A5)
const int ROW_PINS[] = {A2, A3, A4, A5};
const int NUM_ROWS = 4;

// Button Mapping (R x C -> Button Number 1-8)
const int BUTTON_MAP[NUM_ROWS][NUM_COLS] = {
    {1, 2}, // R1: Button 1 (+1 Run), Button 2 (+2 Runs)
    {3, 4}, // R2: Button 3 (+3 Runs), Button 4 (+4 Runs)
    {5, 6}, // R3: Button 5 (+6 Runs), Button 6 (+1 Wicket)
    {7, 8}  // R4: Button 7 (Wide), Button 8 (No Ball)
};

// Debouncing state array
bool buttonPressedState[NUM_ROWS][NUM_COLS] = {false};


// ------------------------------------
// --- GAME LOGIC FUNCTIONS ---
// ------------------------------------

// Updates the balls and overs counters (used for all valid deliveries)
void updateBalls() {
    balls++;
    if (balls >= 6) {
        balls = 0;
        overs++;
    }
}

// Button 1-5 Logic: Adds runs and increments the ball counter.
void updateRuns(int runs) {
    totalRuns += runs;
    if (totalRuns > 999) totalRuns = 999; // Cap at 999 runs

    // Wides and No Balls bypass this function, so it's safe to always update balls here.
    updateBalls();
    
    // Any successful delivery clears the free hit flag
    freeHitFlag = false; 

    // Serial debug output removed to free pins 0 and 1
}

// Button 6 Logic: Adds a wicket.
void updateWicket() {
    if (freeHitFlag) {
        freeHitFlag = false; // Free hit is used up on this delivery
    } else {
        wickets++;
        if (wickets > 10) wickets = 10; // Cap at 10 wickets
    }
    
    // A wicket counts as a ball regardless of the free hit outcome (as requested).
    updateBalls();

    // Serial debug output removed to free pins 0 and 1
}

// Button 7 Logic: Wide Ball. +1 Run, NO ball counted.
void updateWide() {
    totalRuns += 1;
    if (totalRuns > 999) totalRuns = 999;
    
    // Wide ball clears the free hit flag
    freeHitFlag = false; 

    // Serial debug output removed to free pins 0 and 1
}

// Button 8 Logic: No Ball. +1 Run, NO ball counted. Sets Free Hit.
void updateNoBall() {
    totalRuns += 1;
    if (totalRuns > 999) totalRuns = 999;
    
    freeHitFlag = true;
    // Serial debug output removed to free pins 0 and 1
}


// ------------------------------------
// --- DISPLAY LOGIC FUNCTIONS ---
// ------------------------------------

// Turns off all segment LEDs
void clearSegments() {
    for (int i = 0; i < NUM_SEGMENTS; i++) {
        digitalWrite(SEGMENT_PINS[i], LOW);
    }
}

// Writes the segment pattern for a specific digit (0-9) to the segment pins (0-6).
void writeDigitToSegments(int digit, bool dp = false) {
    if (digit < 0 || digit > 9) {
        clearSegments();
        return;
    }
    
    // Write the HIGH/LOW pattern for the chosen digit
    for (int i = 0; i < NUM_SEGMENTS; i++) {
        digitalWrite(SEGMENT_PINS[i], SEGMENT_MAP[digit][i]);
    }
}

// Main display routine called continuously by loop()
void displayScoreboard() {
    // Map game state to 6 individual display positions
    int scoreHundreds = (totalRuns / 100) % 10;
    int scoreTens = (totalRuns / 10) % 10;
    int scoreUnits = totalRuns % 10;
    int wicketDigit = wickets % 10;
    int overDigit = overs % 10;
    int ballDigit = balls % 10;
    
    int digitsToDisplay[] = {
        scoreHundreds,  // D1 (Pin 7)
        scoreTens,      // D2 (Pin 8)
        scoreUnits,     // D3 (Pin 9)
        wicketDigit,    // D4 (Pin 10)
        overDigit,      // D5 (Pin 11) - Overs
        ballDigit       // D6 (Pin 12) - Balls
    };

    // Multiplexing Loop: Cycle through each of the six displays
    for (int i = 0; i < NUM_DIGITS; i++) {
        
        // 1. Turn OFF all displays (Active HIGH = OFF)
        for (int j = 0; j < NUM_DIGITS; j++) {
            digitalWrite(CATHODE_PINS[j], HIGH);
        }
        
        // 2. Set the segments for the current digit
        writeDigitToSegments(digitsToDisplay[i]);

        // 3. Turn ON the current display (Active LOW = ON)
        digitalWrite(CATHODE_PINS[i], LOW);

        // 4. Hold the display ON briefly.
        delay(3); // 3 milliseconds delay per display
    }
}


// ------------------------------------
// --- INPUT LOGIC FUNCTION ---
// ------------------------------------

void processInput() {
    // Outer Loop: Iterate through Columns (A0, A1)
    for (int c = 0; c < NUM_COLS; c++) {

        // STEP 1: Activate the current column by driving it HIGH (5V)
        digitalWrite(COL_PINS[c], HIGH);
        
        // STEP 2: Check all four rows for a HIGH signal (button press)
        for (int r = 0; r < NUM_ROWS; r++) {
            
            // Read state of the Row Input Pin
            int rowState = digitalRead(ROW_PINS[r]);

            // Detection is Active HIGH 
            if (rowState == HIGH) {
                // Debounce check
                delay(5);
                if (digitalRead(ROW_PINS[r]) == HIGH) {
                    
                    if (buttonPressedState[r][c] == false) {
                        
                        // --- Button Pressed: Execute Game Logic ---
                        int buttonNumber = BUTTON_MAP[r][c];
                        
                        switch (buttonNumber) {
                            case 1: updateRuns(1); break;
                            case 2: updateRuns(2); break;
                            case 3: updateRuns(3); break;
                            case 4: updateRuns(4); break;
                            case 5: updateRuns(6); break;
                            case 6: updateWicket(); break;
                            case 7: updateWide(); break;
                            case 8: updateNoBall(); break;
                        }
                        
                        buttonPressedState[r][c] = true;
                    }
                }
            } 
            // Button is NOT pressed (LOW) -> Reset state for next press
            else if (buttonPressedState[r][c] == true) {
                 buttonPressedState[r][c] = false;
            }
        }

        // STEP 3: Deactivate the current column by returning it to LOW (GND)
        digitalWrite(COL_PINS[c], LOW);
    }
}


// ------------------------------------
// --- MAIN SETUP AND LOOP ---
// ------------------------------------

void setup() {
    // Removed Serial.begin(9600); to avoid conflict with Pin 0 and 1
    
    // 1. Configure all Segment (0-6) and Cathode (7-12) pins as OUTPUTS
    for (int i = 0; i < NUM_SEGMENTS; i++) {
        pinMode(SEGMENT_PINS[i], OUTPUT);
    }
    for (int i = 0; i < NUM_DIGITS; i++) {
        pinMode(CATHODE_PINS[i], OUTPUT);
        digitalWrite(CATHODE_PINS[i], HIGH); // Turn off all displays
    }

    // 2. Configure Column Pins (A0, A1) as OUTPUTS
    for (int i = 0; i < NUM_COLS; i++) {
        pinMode(COL_PINS[i], OUTPUT);
        digitalWrite(COL_PINS[i], LOW); // Keep inactive columns LOW
    }

    // 3. Configure Row Pins (A2-A5) as standard INPUTS
    // NOTE: Relying on external pull-down resistors for all 4 row pins (A2-A5).
    for (int i = 0; i < NUM_ROWS; i++) {
        pinMode(ROW_PINS[i], INPUT);
    }
}


void loop() {
    // 1. Process Button Input (Handles scoring logic)
    processInput();
    
    // 2. Continuously refresh the display (Multiplexing)
    // The delay inside the displayScoreboard function provides the necessary cycle time.
    displayScoreboard();
}