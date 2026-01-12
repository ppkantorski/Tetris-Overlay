/********************************************************************************
 * File: main.cpp
 * Author: ppkantorski
 * Description: 
 *   This file contains the main logic for the Tetris Overlay project, 
 *   a graphical overlay implementation of the classic Tetris game for the 
 *   Nintendo Switch. It integrates game state management, rendering, 
 *   and user input handling to provide a complete Tetris experience 
 *   within an overlay.
 * 
 *   Key Features:
 *   - Classic Tetris gameplay mechanics with level and score tracking.
 *   - Smooth animations and intuitive controls.
 *   - Save and load game state functionality.
 *   - Dynamic UI rendering with next and stored Tetrimino previews.
 *   - Integration with the Tesla Menu system for in-game overlay management.
 * 
 *   For the latest updates, documentation, and source code, visit the project's
 *   GitHub repository:
 *   (GitHub Repository: https://github.com/ppkantorski/Tetris-Overlay)
 * 
 *   Note: This notice is part of the project's documentation and must remain intact.
 *
 *  Licensed under GPLv2
 *  Copyright (c) 2024 ppkantorski
 ********************************************************************************/

#define NDEBUG
#define STBTT_STATIC
#define TESLA_INIT_IMPL

#include <ultra.hpp>
#include <tesla.hpp>

#include <set>
#include <array>
#include <vector>
#include <ctime>
#include <chrono>
#include <random>
#include <mutex>

using namespace ult;

std::mutex boardMutex;  // Declare a mutex for board access
std::mutex particleMutex;

bool isGameOver = false;
bool firstLoad = false; // Track if it's the first frame after loading

struct Particle {
    float x, y;      // Position
    float vx, vy;    // Velocity
    float life;      // Lifespan
    float alpha;     // Transparency (fades out)
};


std::vector<Particle> particles;


// Define the Tetrimino shapes
constexpr std::array<std::array<size_t, 16>, 7> tetriminoShapes = {{
    // I
    { 0,0,0,0,
      1,1,1,1,
      0,0,0,0,
      0,0,0,0 },

    // J
    { 1,0,0,0,
      1,1,1,0,
      0,0,0,0,
      0,0,0,0 },

    // L
    { 0,0,1,0,
      1,1,1,0,
      0,0,0,0,
      0,0,0,0 },

    // O
    { 1,1,0,0,
      1,1,0,0,
      0,0,0,0,
      0,0,0,0 },

    // S
    { 0,1,1,0,
      1,1,0,0,
      0,0,0,0,
      0,0,0,0 },

    // T
    { 0,1,0,0,
      1,1,1,0,
      0,0,0,0,
      0,0,0,0 },

    // Z
    { 1,1,0,0,
      0,1,1,0,
      0,0,0,0,
      0,0,0,0 }
}};

// Adjusted rotation centers based on official Tetris SRS
constexpr std::array<std::pair<int, int>, 7> rotationCenters = {{
    {1.5f, 1.5f}, // I piece (rotating around the second cell in a 4x4 grid)
    {1, 1}, // J piece
    {1, 1}, // L piece
    {1, 1}, // O piece
    {1, 1}, // S piece
    {1, 1}, // T piece
    {1, 1}  // Z piece
}};

// Wall kicks for I piece (SRS)
constexpr std::array<std::array<std::pair<int, int>, 5>, 4> wallKicksI = {{
    // 0 -> 1, 1 -> 0
    {{ {0, 0}, {-2, 0}, {1, 0}, {-2, -1}, {1, 2} }},
    // 1 -> 2, 2 -> 1
    {{ {0, 0}, {-1, 0}, {2, 0}, {-1, 2}, {2, -1} }},
    // 2 -> 3, 3 -> 2
    {{ {0, 0}, {2, 0}, {-1, 0}, {2, 1}, {-1, -2} }},
    // 3 -> 0, 0 -> 3
    {{ {0, 0}, {1, 0}, {-2, 0}, {1, -2}, {-2, 1} }}
}};

// Wall kicks for J, L, S, T, Z pieces (SRS)
constexpr std::array<std::array<std::pair<int, int>, 5>, 4> wallKicksJLSTZ = {{
    // 0 -> 1, 1 -> 0
    {{ {0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2} }},
    // 1 -> 2, 2 -> 1
    {{ {0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2} }},
    // 2 -> 3, 3 -> 2
    {{ {0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2} }},
    // 3 -> 0, 0 -> 3
    {{ {0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2} }}
}};


// Define colors for each Tetrimino
constexpr std::array<tsl::Color, 7> tetriminoColors = {{
    {0x0, 0xE, 0xF, 0xF}, // Cyan - I (R=0, G=F, B=F, A=F)
    {0x2, 0x2, 0xF, 0xF}, // Blue - J (R=0, G=0, B=F, A=F)
    {0xF, 0xA, 0x0, 0xF}, // Orange - L (R=F, G=A, B=0, A=F)
    {0xE, 0xE, 0x0, 0xF}, // Yellow - O (R=F, G=F, B=0, A=F)
    {0x0, 0xE, 0x0, 0xF}, // Green - S (R=0, G=F, B=0, A=F)
    {0x8, 0x0, 0xF, 0xF}, // Purple - T (R=8, G=0, B=F, A=F)
    {0xE, 0x0, 0x0, 0xF}  // Red - Z (R=F, G=0, B=0, A=F)
}};

// Board dimensions
constexpr int BOARD_WIDTH = 10;
constexpr int BOARD_HEIGHT = 20;

// Updated helper function to get rotated index
int getRotatedIndex(int type, int i, int j, int rotation) {
    // Ensure i and j are within bounds
    if (i < 0 || i >= 4 || j < 0 || j >= 4) return -1;

    if (type == 0) { // I piece
        int rotatedIndex = 0;
        switch (rotation) {
            case 0: rotatedIndex = i * 4 + j; break;
            case 1: rotatedIndex = (3 - i) + j * 4; break;
            case 2: rotatedIndex = (3 - j) + (3 - i) * 4; break;
            case 3: rotatedIndex = i + (3 - j) * 4; break;
        }
        return rotatedIndex;
    } else if (type == 3) { // O piece doesn't rotate
        return i * 4 + j;
    } else {
        // General case for other pieces
        const float centerX = rotationCenters[type].first;
        const float centerY = rotationCenters[type].second;
        const int relX = j - centerX;
        const int relY = i - centerY;
        int rotatedX, rotatedY;

        switch (rotation) {
            case 0: rotatedX = relX; rotatedY = relY; break;
            case 1: rotatedX = -relY; rotatedY = relX; break;
            case 2: rotatedX = -relX; rotatedY = -relY; break;
            case 3: rotatedX = relY; rotatedY = -relX; break;
        }

        const int finalX = static_cast<int>(round(rotatedX + centerX));
        const int finalY = static_cast<int>(round(rotatedY + centerY));

        // Ensure the rotated index is within the 4x4 grid
        if (finalX < 0 || finalX >= 4 || finalY < 0 || finalY >= 4) return -1;
        return finalY * 4 + finalX;
    }
}





struct Tetrimino {
    int x, y;
    int type;
    int rotation;
    Tetrimino(int t) : x(BOARD_WIDTH / 2 - 2), y(0), type(t), rotation(0) {}
};

// Function to check if the current position of a Tetrimino is valid
bool isPositionValid(const Tetrimino& tet, const std::array<std::array<int, BOARD_WIDTH>, BOARD_HEIGHT>& board) {
    int rotatedIndex;
    int x, y;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            rotatedIndex = getRotatedIndex(tet.type, i, j, tet.rotation);

            // Only check cells that contain a block
            if (tetriminoShapes[tet.type][rotatedIndex] != 0) {
                x = tet.x + j;
                y = tet.y + i;

                // Check if x and y are within the bounds of the board horizontally
                if (x < 0 || x >= BOARD_WIDTH) {
                    return false;  // Invalid if out of bounds
                }

                // Allow blocks above the board but not below the bottom
                if (y >= BOARD_HEIGHT) {
                    return false;  // Invalid if out of bounds vertically
                }

                // If the block is above the visible board, ignore it
                if (y < 0) {
                    continue;  // Skip rows above the board
                }

                // Check if the block space is occupied
                if (board[y][x] != 0) {
                    return false;  // Invalid if space is occupied
                }
            }
        }
    }
    return true;  // Position is valid
}




float countOffset = 0.0f;
float counter;

// Helper function to calculate where the Tetrimino will land if hard dropped
int calculateDropDistance(const Tetrimino& tet, const std::array<std::array<int, BOARD_WIDTH>, BOARD_HEIGHT>& board) {
    int dropDistance = 0;
    Tetrimino tempTetrimino = tet;  // Create a temporary copy for simulation
    while (isPositionValid(tempTetrimino, board)) {
        tempTetrimino.y += 1;  // Move down one row
        dropDistance++;
    }
    return std::max(dropDistance - 1, 0);  // Ensure the dropDistance doesn't go negative
}


class TetrisElement : public tsl::elm::Element {
public:
    static bool paused;
    static uint64_t maxHighScore; // Change to a larger data type
    bool gameOver = false; // Add this line

    // Variables for line clear text animation
    std::string linesClearedText;  // Text to show (Single, Double, etc.)
    int linesClearedScore;

    float fadeAlpha = 0.0f;        // Alpha value for fade-in/fade-out
    bool showText = false;         // Flag to control when to show the text
    int clearedLinesYPosition = 0; // Y-position of cleared lines to center text
    std::chrono::time_point<std::chrono::steady_clock> textStartTime;

    // Rain effect variables for Game Over
    std::chrono::time_point<std::chrono::steady_clock> lastRainSpawn;
    static constexpr int RAIN_SPAWN_INTERVAL_MS = 50; // Spawn new rain particles every 50ms

    TetrisElement(u16 w, u16 h, std::array<std::array<int, BOARD_WIDTH>, BOARD_HEIGHT> *board, 
                  Tetrimino *current, Tetrimino *next, Tetrimino *stored, 
                  Tetrimino *next1, Tetrimino *next2)
        : board(board), currentTetrimino(current), nextTetrimino(next), 
          storedTetrimino(stored), nextTetrimino1(next1), nextTetrimino2(next2),
          _w(w), _h(h) {}

    virtual void draw(tsl::gfx::Renderer* renderer) override {
        // Center the board in the frame
        const int boardWidthInPixels = BOARD_WIDTH * _w;
        const int boardHeightInPixels = BOARD_HEIGHT * _h;
        const int offsetX = (this->getWidth() - boardWidthInPixels) / 2;
        const int offsetY = (this->getHeight() - boardHeightInPixels) / 2;


        // Define the semi-transparent black background color
        static constexpr tsl::Color overlayColor = tsl::Color({0x0, 0x0, 0x0, 0x8}); // Semi-transparent black color
        
        // Draw the black background rectangle (slightly larger than the frame)
        static constexpr int backgroundPadding = 4; // Padding around the frame for the black background
        renderer->drawRect(offsetX - backgroundPadding, offsetY - backgroundPadding,
                           boardWidthInPixels + 2 * backgroundPadding, boardHeightInPixels + 2 * backgroundPadding, a(overlayColor));


        // Draw the board frame
        static constexpr tsl::Color frameColor = tsl::Color({0xF, 0xF, 0xF, 0xF}); // White color for frame
        static constexpr int frameThickness = 2;
        
        // Top line
        renderer->drawRect(offsetX - frameThickness, offsetY - frameThickness, BOARD_WIDTH * _w + 2 * frameThickness, frameThickness, frameColor);
        // Bottom line
        renderer->drawRect(offsetX - frameThickness, offsetY + BOARD_HEIGHT * _h, BOARD_WIDTH * _w + 2 * frameThickness, frameThickness, frameColor);
        // Left line
        renderer->drawRect(offsetX - frameThickness, offsetY - frameThickness, frameThickness, BOARD_HEIGHT * _h + 2 * frameThickness, frameColor);
        // Right line
        renderer->drawRect(offsetX + BOARD_WIDTH * _w, offsetY - frameThickness, frameThickness, BOARD_HEIGHT * _h + 2 * frameThickness, frameColor);


        static constexpr int innerPadding = 3; // Adjust this to control the inner rectangle size

        // Draw the board
        int drawX, drawY;
        tsl::Color innerColor(0), outerColor(0);
        tsl::Color highlightColor(0);
        for (int y = 0; y < BOARD_HEIGHT; ++y) {
            for (int x = 0; x < BOARD_WIDTH; ++x) {
                if ((*board)[y][x] != 0) {
                    drawX = offsetX + x * _w;
                    drawY = offsetY + y * _h;
                    
                    // Get the color for the current block (this will be the inner block color)
                    innerColor = tetriminoColors[(*board)[y][x] - 1];
                    
                    // Calculate a darker shade for the outer block
                    outerColor = {
                        static_cast<u8>(innerColor.r * 0xC / 0xF),  // Slightly darker, closer to 60% brightness
                        static_cast<u8>(innerColor.g * 0xC / 0xF),
                        static_cast<u8>(innerColor.b * 0xC / 0xF),
                        static_cast<u8>(innerColor.a)  // Ensure this is within the range of 0-15
                    };
                    
                    // Draw the outer block (darker color)
                    renderer->drawRect(drawX, drawY, _w, _h, outerColor);
                    
                    // Draw the inner block (smaller rectangle with original color)
                    renderer->drawRect(drawX + innerPadding, drawY + innerPadding, _w - 2 * innerPadding, _h - 2 * innerPadding, innerColor);
                    
                    // Highlight at the top-left corner (lighter shade for the inner block)
                    highlightColor = {
                        static_cast<u8>(std::min(innerColor.r + 0x4, 0xF)),  // Lighter shade for highlight
                        static_cast<u8>(std::min(innerColor.g + 0x4, 0xF)),
                        static_cast<u8>(std::min(innerColor.b + 0x4, 0xF)),
                        static_cast<u8>(innerColor.a) // Ensure this is within the range of 0-15
                    };
                    
                    renderer->drawRect(drawX + innerPadding, drawY + innerPadding, _w / 4, _h / 4, highlightColor);
                }
            }
        }


        score.str(std::string());
        score << "Score\n" << getScore();

        static constexpr auto whiteColor = tsl::Color({0xF, 0xF, 0xF, 0xF});

        renderer->drawString(score.str().c_str(), false, 64, 124, 20, whiteColor);
        
        highScore.str(std::string());
        highScore << "High Score\n" << maxHighScore;
        renderer->drawString(highScore.str().c_str(), false, 268, 124, 20, whiteColor);


        // Draw the stored Tetrimino
        drawStoredTetrimino(renderer, offsetX - 61, offsetY); // Adjust the position to fit on the left side

        // Draw the next Tetrimino preview
        drawNextTetrimino(renderer, offsetX + BOARD_WIDTH * _w + 12, offsetY);
        
        drawNextTwoTetriminos(renderer, offsetX + BOARD_WIDTH * _w + 12, offsetY + BORDER_HEIGHT + 12);

        renderer->drawString("", false, offsetX - 85, offsetY + (BORDER_HEIGHT + 12)*0.5 +1, 18, whiteColor);

        renderer->drawString("", false, offsetX + BOARD_WIDTH * _w + 64, offsetY + (BORDER_HEIGHT + 12)*0.5, 18, whiteColor);
        renderer->drawString("", false, offsetX + BOARD_WIDTH * _w + 64, offsetY + (BORDER_HEIGHT + 12)*1.5, 18, whiteColor);
        renderer->drawString("", false, offsetX + BOARD_WIDTH * _w + 64, offsetY + (BORDER_HEIGHT + 12)*2.5, 18, whiteColor);

        // Draw the number of lines cleared
        ult::StringStream linesStr;
        linesStr << "Lines\n" << linesCleared;
        renderer->drawString(linesStr.str().c_str(), false, offsetX + BOARD_WIDTH * _w + 14, offsetY + (BORDER_HEIGHT + 12)*3 + 18, 18, whiteColor);
        
        // Draw the current level
        ult::StringStream levelStr;
        levelStr << "Level\n" << level;
        renderer->drawString(levelStr.str().c_str(), false, offsetX + BOARD_WIDTH * _w + 14, offsetY + (BORDER_HEIGHT + 12)*3 + 63, 18, whiteColor);
        

        renderer->drawString("", false, 74, offsetY + 74, 18, whiteColor);

        std::lock_guard<std::mutex> lock(boardMutex);  // Lock the mutex while rendering
        
        // Draw the current Tetrimino
        drawTetrimino(renderer, *currentTetrimino, offsetX, offsetY);


        // Update the particles
        updateParticles(offsetX, offsetY);
        if (!gameOver) {
            drawParticles(renderer, offsetX, offsetY);
        }
        

        static std::chrono::time_point<std::chrono::steady_clock> gameOverStartTime; // Track the time when game over starts
        static bool gameOverTextDisplayed = false; // Track if the game over text is displayed after the delay

        // Draw score and status text
        if (gameOver || paused) {
            // Draw a semi-transparent black overlay over the board
            renderer->drawRect(offsetX, offsetY, boardWidthInPixels, boardHeightInPixels, tsl::Color({0x0, 0x0, 0x0, 0xA}));
            
            // Calculate the center position of the board
            const int centerX = offsetX + (BOARD_WIDTH * _w) / 2;
            const int centerY = offsetY + (BOARD_HEIGHT * _h) / 2;
            


            if (gameOver) {
                // If this is the first frame or the game was loaded into a game over state, skip the delay
                if (firstLoad) {
                    gameOverTextDisplayed = true;
                    firstLoad = false;
                }
                
                // If the game over text has not been displayed yet, start the timer
                if (!gameOverTextDisplayed) {
                    if (gameOverStartTime == std::chrono::time_point<std::chrono::steady_clock>()) {
                        // Store the time when game over was triggered
                        gameOverStartTime = std::chrono::steady_clock::now();
                    }
                    
                    // Calculate the time since game over was triggered
                    const auto elapsedTime = std::chrono::steady_clock::now() - gameOverStartTime;
                    
                    // If 0.5 seconds have passed, display the "Game Over" text
                    if (elapsedTime >= std::chrono::milliseconds(500)) {
                        gameOverTextDisplayed = true;
                        lastRainSpawn = std::chrono::steady_clock::now(); // Initialize rain spawn timer
                    }
                }
                
                // If the game over text is set to be displayed, draw it
                if (gameOverTextDisplayed) {
                    // Set the text color to red
                    static constexpr tsl::Color redColor = tsl::Color({0xF, 0x0, 0x0, 0xF});
                    
                    // Calculate text width to center the text
                    const int textWidth = tsl::gfx::calculateStringWidth("Game Over", 24);
                    const int textX = centerX - textWidth / 2;
                    
                    // Draw "Game Over" at the center of the board
                    renderer->drawString("Game Over", false, textX, centerY, 24, redColor);
                    
                    // Create rain particles periodically
                    const auto currentTime = std::chrono::steady_clock::now();
                    const auto timeSinceLastRain = std::chrono::duration_cast<std::chrono::milliseconds>(
                        currentTime - lastRainSpawn
                    );
                    
                    if (timeSinceLastRain.count() >= RAIN_SPAWN_INTERVAL_MS) {
                        createRainParticles(textX, textWidth, centerY, offsetX, offsetY);
                        lastRainSpawn = currentTime;
                    }
                    // Draw rain particles ON TOP of the black overlay
                    drawParticles(renderer, offsetX, offsetY);
                }
            
            } else if (paused) {
                // Set the text color to green
                static constexpr tsl::Color greenColor = tsl::Color({0x0, 0xF, 0x0, 0xF});
                
                // Calculate text width to center the text
                const int textWidth = tsl::gfx::calculateStringWidth("Paused", 24);
                
                // Draw "Paused" at the center of the board
                renderer->drawString("Paused", false, centerX - textWidth / 2, centerY, 24, greenColor);
            }
        }
        if (!gameOver) {
            firstLoad = false;
            gameOverTextDisplayed = false;
            gameOverStartTime = std::chrono::time_point<std::chrono::steady_clock>();
        }
        

        // Draw the lines-cleared text with smooth sine wave-based color effect for "Tetris" and other lines
        if (showText) {
            

            // Calculate the center position of the board
            const int centerX = offsetX + (BOARD_WIDTH * _w) / 2;
            const int centerY = offsetY + (BOARD_HEIGHT * _h) / 2;

            renderer->drawRect(offsetX, centerY - 22, boardWidthInPixels, 26, tsl::Color({0x0, 0x0, 0x0, 0x5}));

            // Calculate text width to center the text
            const std::string scoreLine = "+" + std::to_string(linesClearedScore);
            const int textWidth = tsl::gfx::calculateStringWidth(scoreLine, 20);
            renderer->drawString(scoreLine, false, centerX - textWidth / 2, centerY, 20, tsl::Color({0x0, 0xF, 0x0, 0xF}));


            const auto currentTime = std::chrono::steady_clock::now();
            std::chrono::duration<float, std::milli> elapsedTime = currentTime - textStartTime;
            
            // Define the durations for each phase
            static constexpr float scrollInDuration = 300.0f;  // 0.3 seconds to scroll in
            static constexpr float pauseDuration = 1000.0f;    // 1 second pause
            static constexpr float scrollOutDuration = 300.0f; // 0.3 seconds to scroll out
            const float totalDuration = scrollInDuration + pauseDuration + scrollOutDuration;
            
            // Calculate board dimensions
            const int boardWidthInPixels = BOARD_WIDTH * _w +2; // +2 to account for padding
            const int boardHeightInPixels = BOARD_HEIGHT * _h;
            const int offsetX = (this->getWidth() - boardWidthInPixels) / 2;  // Horizontal offset to center the board
            const int offsetY = (this->getHeight() - boardHeightInPixels) / 2; // Vertical offset to center the board
            
            // Font size for non-Tetris text
            static constexpr int regularFontSize = 20;
            static constexpr int dynamicFontSize = 24;
            
            // Calculate the Y position of the text (vertically centered on the board)
            const int textY = offsetY + (boardHeightInPixels / 2);
            
            // Calculate the X position of the text based on the phase
            int textX;
            int totalTextWidth = 0;
            
            // For "Tetris" and "2x Tetris", we need to handle the different font sizes and effects
            if (linesClearedText.find("x Tetris") != std::string::npos) {
                // Extract the prefix (e.g., "2x ", "10x ")
                size_t xPos = linesClearedText.find("x Tetris");
                std::string prefix = linesClearedText.substr(0, xPos + 2);  // Get the "2x " or "10x "
                std::string remainingText = "Tetris";  // The remaining part is always "Tetris"
                
                int prefixWidth = tsl::gfx::calculateStringWidth(prefix.c_str(), regularFontSize);
                int tetrisWidth = tsl::gfx::calculateStringWidth(remainingText.c_str(), dynamicFontSize);
                totalTextWidth = prefixWidth + tetrisWidth + 9;
                
            } else if (linesClearedText == "Tetris") {
                totalTextWidth = tsl::gfx::calculateStringWidth("Tetris", dynamicFontSize) + 12;
                
            } else if (linesClearedText.find("\n") != std::string::npos) {
                // Handle multiline text (e.g., "T-Spin\nSingle")
                std::vector<std::string> lines = splitString(linesClearedText, "\n");
                int maxLineWidth = 0;
                
                int lineWidth;
                // Calculate the maximum width among the lines
                for (const std::string &line : lines) {
                    lineWidth = tsl::gfx::calculateStringWidth(line.c_str(), regularFontSize);
                    if (lineWidth > maxLineWidth) {
                        maxLineWidth = lineWidth;
                    }
                }
                totalTextWidth = maxLineWidth + 18;  // Adjust the total width to include padding
            } else {
                totalTextWidth = tsl::gfx::calculateStringWidth(linesClearedText.c_str(), regularFontSize) + 18;
            }
            
            // Handle the sliding phases
            if (elapsedTime.count() < scrollInDuration) {
                const float progress = elapsedTime.count() / scrollInDuration;
                textX = offsetX - (progress) * totalTextWidth;  // Move left from hidden to fully visible
            } else if (elapsedTime.count() < scrollInDuration + pauseDuration) {
                textX = offsetX - totalTextWidth;  // Fully visible, just to the left of the gameboard
            } else if (elapsedTime.count() < totalDuration) {
                const float progress = (elapsedTime.count() - scrollInDuration - pauseDuration) / scrollOutDuration;
                textX = offsetX - totalTextWidth + progress * totalTextWidth;  // Move right, getting scissored
            } else {
                // End the animation after the total duration
                showText = false;
                return;
            }
            
            // Enable scissoring to clip the text at the left edge of the gameboard
            renderer->enableScissoring(0, offsetY, offsetX, boardHeightInPixels);
            
            static constexpr tsl::Color textColor(0xF, 0xF, 0xF, 0xF);  // White text for non-Tetris strings
            const auto currentTimeCount = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
            static auto dynamicLogoRGB1 = tsl::RGB888("#6929ff");
            static auto dynamicLogoRGB2 = tsl::RGB888("#fff429");
            countOffset = 0.0f;
            
            static tsl::Color highlightColor(0);
            float counter, transitionProgress;
            
            int charWidth;
            // Handle "2x Tetris" special case
            if (linesClearedText.find("x Tetris") != std::string::npos) {
                //std::string prefix = "2x ";
                const size_t xPos = linesClearedText.find("x Tetris");
                const std::string prefix = linesClearedText.substr(0, xPos + 2);  // Get the "2x " or "10x "
                const int prefixWidth = tsl::gfx::calculateStringWidth(prefix.c_str(), regularFontSize);
                tsl::Color whiteColor(0xF, 0xF, 0xF, 0xF);
                renderer->drawString(prefix.c_str(), false, textX, textY, regularFontSize, whiteColor);
                textX += prefixWidth;
                
                static const std::string remainingText = "Tetris";
                
                for (char letter : remainingText) {
                    counter = (2 * ult::_M_PI * (fmod(currentTimeCount / 4.0, 2.0) + countOffset) / 2.0);
                    transitionProgress = std::sin(3.0 * (counter - (2.0 * ult::_M_PI / 3.0)));
                    
                    highlightColor = {
                        static_cast<u8>((dynamicLogoRGB2.r - dynamicLogoRGB1.r) * (transitionProgress + 1.0) / 2.0 + dynamicLogoRGB1.r),
                        static_cast<u8>((dynamicLogoRGB2.g - dynamicLogoRGB1.g) * (transitionProgress + 1.0) / 2.0 + dynamicLogoRGB1.g),
                        static_cast<u8>((dynamicLogoRGB2.b - dynamicLogoRGB1.b) * (transitionProgress + 1.0) / 2.0 + dynamicLogoRGB1.b),
                        15 // Alpha remains constant, or you can interpolate it as well
                    };
                    
                    std::string charStr(1, letter);
                    charWidth = tsl::gfx::calculateStringWidth(charStr.c_str(), dynamicFontSize);
                    renderer->drawString(charStr.c_str(), false, textX, textY, dynamicFontSize, highlightColor);
                    textX += charWidth;
                    countOffset -= 0.2f;
                }
            } else if (linesClearedText == "Tetris") {
                // Handle "Tetris" with dynamic color effect
                for (char letter : linesClearedText) {
                    counter = (2 * ult::_M_PI * (fmod(currentTimeCount / 4.0, 2.0) + countOffset) / 2.0);
                    transitionProgress = std::sin(3.0 * (counter - (2.0 * ult::_M_PI / 3.0)));
                    
                    highlightColor = {
                        static_cast<u8>((dynamicLogoRGB2.r - dynamicLogoRGB1.r) * (transitionProgress + 1.0) / 2.0 + dynamicLogoRGB1.r),
                        static_cast<u8>((dynamicLogoRGB2.g - dynamicLogoRGB1.g) * (transitionProgress + 1.0) / 2.0 + dynamicLogoRGB1.g),
                        static_cast<u8>((dynamicLogoRGB2.b - dynamicLogoRGB1.b) * (transitionProgress + 1.0) / 2.0 + dynamicLogoRGB1.b),
                        15 // Alpha remains constant, or you can interpolate it as well
                    };
                    
                    std::string charStr(1, letter);
                    charWidth = tsl::gfx::calculateStringWidth(charStr.c_str(), dynamicFontSize);
                    renderer->drawString(charStr.c_str(), false, textX, textY, dynamicFontSize, highlightColor);
                    textX += charWidth;
                    countOffset -= 0.2f;
                }
            } else if (linesClearedText.find("\n") != std::string::npos) {
                // Handle multiline text (e.g., "T-Spin\nSingle")
                std::vector<std::string> lines = splitString(linesClearedText, "\n");
                const int lineSpacing = regularFontSize + 4;
                const int totalHeight = lines.size() * lineSpacing;
                int startY = textY - (totalHeight / 2);
                
                // Find the maximum width
                int maxLineWidth = 0;
                int lineWidth;
                for (const std::string &line : lines) {
                    lineWidth = tsl::gfx::calculateStringWidth(line.c_str(), regularFontSize);
                    if (lineWidth > maxLineWidth) {
                        maxLineWidth = lineWidth;
                    }
                }
                int centeredTextX;
                // Draw each line centered based on max width
                for (const std::string &line : lines) {
                    lineWidth = tsl::gfx::calculateStringWidth(line.c_str(), regularFontSize);
                    centeredTextX = textX + (maxLineWidth - lineWidth) / 2;  // Center each line based on the max width
                    renderer->drawString(line.c_str(), false, centeredTextX, startY, regularFontSize, textColor);
                    startY += lineSpacing;
                }
            } else {
                // Handle single-line text like "Single", "Double"
                renderer->drawString(linesClearedText.c_str(), false, textX, textY, regularFontSize, textColor);
            }
            
            // Disable scissoring after drawing
            renderer->disableScissoring();
        }
    }

    virtual void layout(u16 parentX, u16 parentY, u16 parentWidth, u16 parentHeight) override {
        // Define layout boundaries
        this->setBoundaries(parentX, parentY, parentWidth, parentHeight);
    }

    void updateParticles(int offsetX, int offsetY) {
        std::lock_guard<std::mutex> lock(particleMutex);  // Lock when modifying the particle list
    
        bool allParticlesExpired = true;
    
        // Update all particles and check if all of them are expired
        for (auto& particle : particles) {
            particle.x += particle.vx;
            particle.y += particle.vy;
            particle.alpha -= 0.04f;
            particle.life -= 0.02f;
            
            // Ensure the particle stays within bounds of the entire screen (448x720)
            if (particle.x + offsetX < 0 || particle.x + offsetX > 448 || particle.y + offsetY < 0 || particle.y + offsetY > 720) {
                particle.life = 0;  // Mark the particle as dead if out of bounds
            }
    
            // If any particle is still alive, we won't remove the vector
            if (particle.life > 0.0f && particle.alpha > 0.0f) {
                allParticlesExpired = false;
            }
        }
    
        // If all particles are expired (alpha <= 0 or life <= 0), clear the entire vector
        if (allParticlesExpired) {
            particles.clear();  // Clear the vector in one bulk operation
        }
    }

    void createRainParticles(int textX, int textWidth, int textY, int offsetX, int offsetY) {
        std::lock_guard<std::mutex> lock(particleMutex);
        
        // Spawn 3-5 particles across the text width
        const int particleCount = 3 + rand() % 3;
        
        for (int i = 0; i < particleCount; ++i) {
            // Random X position across the text width (convert from screen coords to board coords)
            const float startX = (textX - offsetX) + (rand() % textWidth);
            const float startY = (textY - offsetY) + 10; // Start just below the text (convert to board coords)
            
            // Slight horizontal drift and consistent downward velocity
            const float horizontalDrift = (rand() % 100 / 100.0f - 0.5f) * 0.5f; // Very slight drift
            const float downwardVelocity = 2.0f + (rand() % 100 / 100.0f); // 2-3 pixels per frame
            
            Particle particle = {
                startX,
                startY,
                horizontalDrift,
                downwardVelocity,
                1.0f,  // Lifespan
                1.0f   // Alpha (fully visible)
            };
            particles.push_back(particle);
        }
    }

    uint64_t getScore() {
        return scoreValue;
    }

    void setScore(uint64_t s) {
        scoreValue = s;
        if (scoreValue > maxHighScore) {
            maxHighScore = scoreValue; // Update the max high score
        }
    }


    int getLinesCleared() { return linesCleared; }
    int getLevel() { return level; }
    void setLinesCleared(int lines) { linesCleared = lines; }
    void setLevel(int lvl) { level = lvl; }

private:
    std::array<std::array<int, BOARD_WIDTH>, BOARD_HEIGHT> *board;
    Tetrimino *currentTetrimino;
    Tetrimino *nextTetrimino;
    Tetrimino *storedTetrimino;
    Tetrimino *nextTetrimino1;  // First next Tetrimino
    Tetrimino *nextTetrimino2;  // Second next Tetrimino

    u16 _w;
    u16 _h;
    
    std::ostringstream score;
    std::ostringstream highScore;
    uint64_t scoreValue = 0;

    int linesCleared = 0;
    int level = 1;
    

    void drawParticles(tsl::gfx::Renderer* renderer, int offsetX, int offsetY) {
        tsl::Color particleColor(0);
        int particleDrawX, particleDrawY;
        
        // Lock the particles vector while drawing to avoid race conditions
        std::lock_guard<std::mutex> lock(particleMutex);  
        
        for (const auto& particle : particles) {
            if (particle.life > 0 && particle.alpha > 0) {
                // Calculate particle position relative to the board
                particleDrawX = offsetX + static_cast<int>(particle.x);
                particleDrawY = offsetY + static_cast<int>(particle.y);
                
                // Generate a random color for each particle in RGB4444 format
                particleColor = tsl::Color({
                    static_cast<u8>(rand() % 16),  // Random Red component (4 bits, 0x0 to 0xF)
                    static_cast<u8>(rand() % 16),  // Random Green component (4 bits, 0x0 to 0xF)
                    static_cast<u8>(rand() % 16),  // Random Blue component (4 bits, 0x0 to 0xF)
                    static_cast<u8>(particle.alpha * 15)  // Alpha component (scaled to 0x0 to 0xF)
                });
                
                // Draw the particle
                renderer->drawRect(particleDrawX, particleDrawY, 4, 4, particleColor);
            }
        }
    }


    // Helper function to draw a single Tetrimino (handles both ghost and normal rendering)
    void drawSingleTetrimino(tsl::gfx::Renderer* renderer, const Tetrimino& tet, int offsetX, int offsetY, bool isGhost) {
        static tsl::Color color(0);
        static tsl::Color outerColor(0);
        static tsl::Color highlightColor(0);
        int rotatedIndex;
        int x, y;
        
        static constexpr int innerPadding = 3;  // Adjust padding for a more balanced 3D look
        
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                rotatedIndex = getRotatedIndex(tet.type, i, j, tet.rotation);
                if (tetriminoShapes[tet.type][rotatedIndex] != 0) {
                    x = offsetX + (tet.x + j) * _w;
                    y = offsetY + (tet.y + i) * _h;
                    
                    // Skip rendering for blocks above the top of the visible board
                    if (tet.y + i < 0) {
                        continue;
                    }
                    
                    color = tetriminoColors[tet.type];  // The regular color for the inner block
                    if (isGhost) {
                        // Make the ghost piece semi-transparent
                        color.a = static_cast<u8>(color.a * 0.4);  // Adjust transparency for ghost piece
                    }
                    
                    // Calculate and draw the outer block (slightly darker than the regular color)
                    outerColor = {
                        static_cast<u8>(color.r * 0xC / 0xF),  // Slightly darker, closer to 60% brightness
                        static_cast<u8>(color.g * 0xC / 0xF),
                        static_cast<u8>(color.b * 0xC / 0xF),
                        static_cast<u8>(color.a)  // Maintain the alpha channel
                    };
                    
                    // Draw the outer block (darker color)
                    renderer->drawRect(x, y, _w, _h, outerColor);
                    
                    // Draw the inner block (original color)
                    renderer->drawRect(x + innerPadding, y + innerPadding, _w - 2 * innerPadding, _h - 2 * innerPadding, color);
                    
                    // Add a 3D highlight at the top-left corner for light effect
                    highlightColor = {
                        static_cast<u8>(std::min(color.r + 0x4, 0xF)),  // Increase brightness more subtly (max out at 0xF)
                        static_cast<u8>(std::min(color.g + 0x4, 0xF)),
                        static_cast<u8>(std::min(color.b + 0x4, 0xF)),
                        static_cast<u8>(color.a)  // Keep alpha unchanged
                    };
                    
                    renderer->drawRect(x + innerPadding, y + innerPadding, _w / 4, _h / 4, highlightColor);
                }
            }
        }
    }

    void drawTetrimino(tsl::gfx::Renderer* renderer, const Tetrimino& tet, int offsetX, int offsetY) {
        // Calculate the drop position for the ghost piece
        Tetrimino ghostTetrimino = tet;
        const int dropDistance = calculateDropDistance(ghostTetrimino, *board);
        ghostTetrimino.y += dropDistance;
        
        // Draw the ghost piece first (semi-transparent)
        drawSingleTetrimino(renderer, ghostTetrimino, offsetX, offsetY, true);  // `true` indicates ghost
        
        // Draw the active Tetrimino
        drawSingleTetrimino(renderer, tet, offsetX, offsetY, false);  // `false` indicates normal piece
    }

    // Constants for borders and padding
    const int BORDER_WIDTH = _w * 2 + 8;
    const int BORDER_HEIGHT = _w * 2 + 8;
    static constexpr int BORDER_THICKNESS = 2;
    static constexpr int PADDING = 2;
    static constexpr tsl::Color BACKGROUND_COLOR = {0x0, 0x0, 0x0, 0x8};
    static constexpr tsl::Color BORDER_COLOR = {0xF, 0xF, 0xF, 0xF};
    
    // Helper function to draw a 3D block with highlight and shadow
    void draw3DBlock(tsl::gfx::Renderer* renderer, int x, int y, int width, int height, tsl::Color color) {
        // Calculate outer block color (darker than the original color)
        const tsl::Color outerColor = {
            static_cast<u8>(color.r * 0xC / 0xF),  // Slightly darker, closer to 60% brightness
            static_cast<u8>(color.g * 0xC / 0xF),
            static_cast<u8>(color.b * 0xC / 0xF),
            static_cast<u8>(color.a)  // Maintain the alpha channel
        };
        
        // Draw the outer block (darker color)
        renderer->drawRect(x, y, width, height, outerColor);
        
        // Draw the inner block (original color)
        static constexpr int innerPadding = 1;
        renderer->drawRect(x + innerPadding, y + innerPadding, width - 2 * innerPadding, height - 2 * innerPadding, color);
        
        // Highlight at the top-left corner (lighter shade)
        const tsl::Color highlightColor = {
            static_cast<u8>(std::min(color.r + 0x4, 0xF)),  // Slightly lighter than the original color
            static_cast<u8>(std::min(color.g + 0x4, 0xF)),
            static_cast<u8>(std::min(color.b + 0x4, 0xF)),
            static_cast<u8>(color.a)  // Keep alpha unchanged
        };
        
        renderer->drawRect(x + innerPadding, y + innerPadding, width / 4, height / 4, highlightColor);
    }

    
    // Helper function to draw preview frame (borders and background)
    void drawPreviewFrame(tsl::gfx::Renderer* renderer, int posX, int posY) {
        // Draw the background for the preview
        renderer->drawRect(
            posX - PADDING - BORDER_THICKNESS, posY - PADDING - BORDER_THICKNESS,
            BORDER_WIDTH + 2 * PADDING + 2 * BORDER_THICKNESS, BORDER_HEIGHT + 2 * PADDING + 2 * BORDER_THICKNESS, 
            BACKGROUND_COLOR
        );
        
        // Draw the white border around the preview area
        renderer->drawRect(posX - PADDING, posY - PADDING, BORDER_WIDTH + 2 * PADDING, BORDER_THICKNESS, BORDER_COLOR);
        renderer->drawRect(posX - PADDING, posY + BORDER_HEIGHT, BORDER_WIDTH + 2 * PADDING, BORDER_THICKNESS, BORDER_COLOR);
        renderer->drawRect(posX - PADDING, posY - PADDING, BORDER_THICKNESS, BORDER_HEIGHT + 2 * PADDING, BORDER_COLOR);
        renderer->drawRect(posX + BORDER_WIDTH, posY - PADDING, BORDER_THICKNESS, BORDER_HEIGHT + 2 * PADDING, BORDER_COLOR);
    }
    
    // Helper function to calculate Tetrimino bounding box
    void calculateTetriminoBounds(const Tetrimino& tetrimino, int& minX, int& maxX, int& minY, int& maxY) {
        minX = 4; maxX = -1; minY = 4; maxY = -1;
        int index;
        
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                index = getRotatedIndex(tetrimino.type, i, j, tetrimino.rotation);
                if (tetriminoShapes[tetrimino.type][index] != 0) {
                    if (j < minX) minX = j;
                    if (j > maxX) maxX = j;
                    if (i < minY) minY = i;
                    if (i > maxY) maxY = i;
                }
            }
        }
    }
    
    // Helper function to draw a centered Tetrimino
    void drawCenteredTetrimino(tsl::gfx::Renderer* renderer, const Tetrimino& tetrimino, int posX, int posY) {
        static int minX, maxX, minY, maxY;
        calculateTetriminoBounds(tetrimino, minX, maxX, minY, maxY);
        
        // Calculate width and height of the Tetrimino
        const float tetriminoWidth = (maxX - minX + 1) * (_w / 2);
        const float tetriminoHeight = (maxY - minY + 1) * (_h / 2);
        
        // Center the Tetrimino in the preview area
        const int offsetX = std::ceil((BORDER_WIDTH - tetriminoWidth) / 2. - 2.);
        const int offsetY = std::ceil((BORDER_HEIGHT - tetriminoHeight) / 2. - 2.);
        
        static int blockWidth, blockHeight, drawX, drawY;
        
        static int index;
        // Draw each block of the Tetrimino
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                index = getRotatedIndex(tetrimino.type, i, j, tetrimino.rotation);
                if (tetriminoShapes[tetrimino.type][index] != 0) {
                    blockWidth = _w / 2;
                    blockHeight = _h / 2;
                    drawX = posX + (j - minX) * blockWidth + PADDING + offsetX;
                    drawY = posY + (i - minY) * blockHeight + PADDING + offsetY;
    
                    // Use the reusable function to draw the 3D block
                    draw3DBlock(renderer, drawX, drawY, blockWidth, blockHeight, tetriminoColors[tetrimino.type]);
                }
            }
        }
    }
    
    // Updated method to draw the next Tetrimino with 3D effect
    void drawNextTetrimino(tsl::gfx::Renderer* renderer, int posX, int posY) {
        // Draw the frame for the next Tetrimino preview
        drawPreviewFrame(renderer, posX, posY);
    
        // Draw the centered next Tetrimino
        drawCenteredTetrimino(renderer, *nextTetrimino, posX, posY);
    }
    
    // Updated method to draw the next two Tetriminos
    void drawNextTwoTetriminos(tsl::gfx::Renderer* renderer, int posX, int posY) {
        const int posY2 = posY + BORDER_HEIGHT + 12;
        
        // Draw the first next Tetrimino with frame and centered logic
        drawPreviewFrame(renderer, posX, posY);
        drawCenteredTetrimino(renderer, *nextTetrimino1, posX, posY);
        
        // Draw the second next Tetrimino with frame and centered logic
        drawPreviewFrame(renderer, posX, posY2);
        drawCenteredTetrimino(renderer, *nextTetrimino2, posX, posY2);
    }
    
    // Updated method to draw the stored Tetrimino
    void drawStoredTetrimino(tsl::gfx::Renderer* renderer, int posX, int posY) {
        drawPreviewFrame(renderer, posX, posY);
        
        if (storedTetrimino->type != -1) {
            drawCenteredTetrimino(renderer, *storedTetrimino, posX, posY);
        }
    }
    
};

bool TetrisElement::paused = false;
uint64_t TetrisElement::maxHighScore = 0; // Initialize the max high score


class CustomOverlayFrame : public tsl::elm::OverlayFrame {
public:
    CustomOverlayFrame(const std::string& title, const std::string& subtitle, const bool& _noClickableItems = false)
        : tsl::elm::OverlayFrame(title, subtitle, _noClickableItems) {}

    // Override the draw method to customize rendering logic for Tetris
    virtual void draw(tsl::gfx::Renderer* renderer) override {
        if (m_noClickableItems != ult::noClickableItems.load(std::memory_order_acquire)) {
            ult::noClickableItems.store(m_noClickableItems, std::memory_order_release);
        }
    
        if (!ult::themeIsInitialized.load(std::memory_order_acquire)) {
            ult::themeIsInitialized.store(true, std::memory_order_release);
            tsl::initializeThemeVars();
        }
    
        renderer->fillScreen(a(tsl::defaultBackgroundColor));
        renderer->drawWallpaper();
        renderer->drawWidget();
    
        if (touchingMenu && inMainMenu) {
            renderer->drawRoundedRect(0.0f, 12.0f, 245.0f, 73.0f, 6.0f, a(tsl::clickColor));
        }
        
        x = 20;
        y = 62;
        fontSize = 54;
        offset = 6;
        countOffset = 0;
    
        if (ult::useDynamicLogo) {
            const auto currentTimeCount = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
            float progress;
            static const auto dynamicLogoRGB1 = tsl::RGB888("#6929ff");
            static const auto dynamicLogoRGB2 = tsl::RGB888("#fff429");
            static tsl::Color highlightColor(0);
            for (char letter : m_title) {
                counter = (2 * ult::_M_PI * (fmod(currentTimeCount/4.0, 2.0) + countOffset) / 2.0);
                progress = std::sin(3.0 * (counter - (2.0 * ult::_M_PI / 3.0)));
                
                highlightColor = {
                    static_cast<u8>((dynamicLogoRGB2.r - dynamicLogoRGB1.r) * (progress + 1.0) / 2.0 + dynamicLogoRGB1.r),
                    static_cast<u8>((dynamicLogoRGB2.g - dynamicLogoRGB1.g) * (progress + 1.0) / 2.0 + dynamicLogoRGB1.g),
                    static_cast<u8>((dynamicLogoRGB2.b - dynamicLogoRGB1.b) * (progress + 1.0) / 2.0 + dynamicLogoRGB1.b),
                    15
                };
                
                renderer->drawString(std::string(1, letter), false, x, y + offset, fontSize, a(highlightColor));
                x += tsl::gfx::calculateStringWidth(std::string(1, letter), fontSize);
                countOffset -= 0.2F;
            }
        } else {
            for (char letter : m_title) {
                renderer->drawString(std::string(1, letter), false, x, y + offset, fontSize, a(tsl::logoColor1));
                x += tsl::gfx::calculateStringWidth(std::string(1, letter), fontSize);
                countOffset -= 0.2F;
            }
        }
    
        renderer->drawString(this->m_subtitle, false, 184, y-8, 15, (tsl::bannerVersionTextColor));
        renderer->drawRect(15, tsl::cfg::FramebufferHeight - 73, tsl::cfg::FramebufferWidth - 30, 1, a(tsl::bottomSeparatorColor));
    
        // Calculate gap width and store half gap (matching original code)
        const float gapWidth = renderer->getTextDimensions(ult::GAP_1, false, 23).first;
        const float _halfGap = gapWidth / 2.0f;
        if (_halfGap != ult::halfGap.load(std::memory_order_acquire))
            ult::halfGap.store(_halfGap, std::memory_order_release);
    
        // Determine button commands based on game state
        static std::string bCommand;
        static std::string aCommand;
    
        if (isGameOver) {
            bCommand = BACK;
            aCommand = "New Game";
            m_noClickableItems = false;
        } else if (TetrisElement::paused) {
            bCommand = BACK;
            aCommand = "";
            m_noClickableItems = true;
        } else {
            bCommand = "Rotate Left";
            aCommand = "Rotate Right";
            m_noClickableItems = false;
        }
    
        // Calculate text widths for buttons
        const float backTextWidth = renderer->getTextDimensions(
            "\uE0E1" + ult::GAP_2 + bCommand, false, 23).first;
        const float selectTextWidth = renderer->getTextDimensions(
            "\uE0E0" + ult::GAP_2 + aCommand, false, 23).first;
    
        // Total button widths include half-gap padding on both sides (matching original)
        const float _backWidth = backTextWidth + gapWidth;
        if (_backWidth != ult::backWidth.load(std::memory_order_acquire))
            ult::backWidth.store(_backWidth, std::memory_order_release);
        const float _selectWidth = selectTextWidth + gapWidth;
        if (_selectWidth != ult::selectWidth.load(std::memory_order_acquire))
            ult::selectWidth.store(_selectWidth, std::memory_order_release);
    
        // Set button positions (matching original)
        static constexpr float buttonStartX = 30;
        const float buttonY = static_cast<float>(tsl::cfg::FramebufferHeight - 73 + 1);
        
        //static bool triggerOnce = true;

        // Draw back button if touched
        if (ult::touchingBack.load(std::memory_order_acquire)) {
            renderer->drawRoundedRect(buttonStartX+2 - _halfGap, buttonY, _backWidth-1, 73.0f, 10.0f, a(tsl::clickColor));
        }
    
        // Draw select button if touched
        else if (ult::touchingSelect.load(std::memory_order_acquire) && !m_noClickableItems) {
            renderer->drawRoundedRect(buttonStartX+2 - _halfGap + _backWidth+1, buttonY,
                                      _selectWidth-2, 73.0f, 10.0f, a(tsl::clickColor));
        }

        //else
        //    triggerOnce = true;
        
        // Build menu bottom line
        const std::string menuBottomLine = m_noClickableItems ? 
            "\uE0E1" + ult::GAP_2 + bCommand + ult::GAP_1 : 
            "\uE0E1" + ult::GAP_2 + bCommand + ult::GAP_1 + "\uE0E0" + ult::GAP_2 + aCommand + ult::GAP_1;
    
        // Render the text with special character handling
        static const std::vector<std::string> symbols = {"\uE0E1","\uE0E0","\uE0ED","\uE0EE"};
        renderer->drawStringWithColoredSections(menuBottomLine, false, symbols, 
                                                buttonStartX, 693, 23, 
                                                (tsl::bottomTextColor), (tsl::buttonColor));
    
        if (this->m_contentElement != nullptr)
            this->m_contentElement->frame(renderer);
    }
};


class TetrisGui : public tsl::Gui {
public:
    Tetrimino storedTetrimino{-1}; // -1 indicates no stored Tetrimino
    bool hasSwapped = false; // To track if a swap has already occurred

    int linesClearedForLevelUp = 0;  // Track how many lines cleared for leveling up
    static constexpr int LINES_PER_LEVEL = 10;  // Increment level every 10 lines

    // Variables to track time of last rotation or movement
    std::chrono::time_point<std::chrono::steady_clock> lastRotationOrMoveTime;
    static constexpr std::chrono::milliseconds lockDelayExtension = std::chrono::milliseconds(500); // 500ms extension

    TetrisGui() : board(), currentTetrimino(rand() % 7), nextTetrimino(rand() % 7), 
                  nextTetrimino1(rand() % 7), nextTetrimino2(rand() % 7) {
    
        std::srand(std::time(0));
        _w = 20;
        _h = _w;
        lockDelayTime = std::chrono::milliseconds(500); // Set lock delay to 500ms
        lockDelayCounter = std::chrono::milliseconds(0);
    
        // Initial fall speed (1000 ms = 1 second)
        initialFallSpeed = std::chrono::milliseconds(500);
        fallCounter = std::chrono::milliseconds(0);
    
        lastRotationOrMoveTime = std::chrono::steady_clock::now();  // Initialize with current time
    }

    ~TetrisGui() {
        TetrisElement::paused = true;
        saveGameState();
    }

    virtual tsl::elm::Element* createUI() override {
        //auto rootFrame = new tsl::elm::OverlayFrame("Tetris", APP_VERSION);
        auto rootFrame = new CustomOverlayFrame("Tetris", APP_VERSION);
        tetrisElement = new TetrisElement(_w, _h, &board, &currentTetrimino, &nextTetrimino, &storedTetrimino, &nextTetrimino1, &nextTetrimino2);
        rootFrame->setContent(tetrisElement);
        timeSinceLastFrame = std::chrono::steady_clock::now();
    
        loadGameState();
        return rootFrame;
    }





    virtual void update() override {
        if (!TetrisElement::paused && !tetrisElement->gameOver) {
            const auto currentTime = std::chrono::steady_clock::now();
            const auto elapsed = currentTime - timeSinceLastFrame;

            // Handle piece falling
            fallCounter += std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
            if (fallCounter >= getFallSpeed()) {
                // Try to move the piece down
                if (!move(0, 1)) { // Move down failed, piece touched the ground
                    lockDelayCounter += fallCounter; // Add elapsed time to lock delay counter

                    // Check if more than 500ms has passed since the last move/rotation
                    const auto timeSinceLastRotationOrMove = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastRotationOrMoveTime);

                    if (lockDelayCounter >= lockDelayTime && timeSinceLastRotationOrMove >= lockDelayExtension) {
                        // Lock the piece after the lock delay has passed and no rotation occurred recently
                        placeTetrimino();
                        clearLines();
                        spawnNewTetrimino();
                        lockDelayCounter = std::chrono::milliseconds(0); // Reset the lock delay counter
                    }
                } else {
                    // Piece successfully moved down, reset lock delay
                    lockDelayCounter = std::chrono::milliseconds(0);
                }
                fallCounter = std::chrono::milliseconds(0); // Reset fall counter
            }

            timeSinceLastFrame = currentTime;
        }
    }
    
    

    void resetGame() {
        // Create an explosion effect before resetting the game
        createCenterExplosionParticles();
    
        // Delay the actual reset slightly to allow the explosion to be visible
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
        isGameOver = false;
    
        // Reset variables related to game state
        lastWallKickApplied = false;
        previousClearWasTetris = false;
        previousClearWasTSpin = false;
        backToBackCount = 1;
    
        // Clear the board
        for (auto& row : board) {
            row.fill(0);
        }
    
        // Reset tetriminos
        spawnNewTetrimino();  // Spawn the first piece here
        nextTetrimino = Tetrimino(rand() % 7);
        nextTetrimino1 = Tetrimino(rand() % 7);
        nextTetrimino2 = Tetrimino(rand() % 7);
    
        // Reset the stored tetrimino
        storedTetrimino = Tetrimino(-1); // Reset stored piece to no stored state
        hasSwapped = false; // Reset swap flag
    
        // Reset score
        tetrisElement->setScore(0);
    
        // Reset linesCleared and level
        tetrisElement->setLinesCleared(0); // Reset lines cleared
        tetrisElement->setLevel(1); // Reset level to 1
    
        // Reset game over state
        tetrisElement->gameOver = false;
    
        // Unpause the game
        TetrisElement::paused = false;
    }


    
    void swapStoredTetrimino() {
        if (storedTetrimino.type == -1) {
            // No stored Tetrimino, store the current one and spawn a new one
            storedTetrimino = currentTetrimino;
            storedTetrimino.rotation = 0;  // Reset the rotation of the stored piece to its default
            spawnNewTetrimino();
        } else {
            // Swap the current Tetrimino with the stored one
            std::swap(currentTetrimino, storedTetrimino);
            currentTetrimino.x = BOARD_WIDTH / 2 - 2;
            currentTetrimino.y = 0;
            currentTetrimino.rotation = 0;  // Reset the swapped piece's rotation to default
            storedTetrimino.rotation = 0;  // Reset the stored piece's rotation to default
        }
    }

    void createImpactParticles(int dropDistance) {
        std::lock_guard<std::mutex> lock(particleMutex);  // Lock to ensure safe access to the particle list
        
        // Cap the maximum drop distance to avoid excessive velocity
        const float velocityFactor = std::min(dropDistance / 10.0f, 2.0f);  // Adjust the divisor and cap for desired effect
        
        // Set minimum and maximum horizontal and vertical velocities
        static constexpr float minVelocity = 0.5f;  // Minimum velocity value
        const float maxHorizontalVelocity = 2.0f * velocityFactor;
        const float maxVerticalVelocity = 4.0f * velocityFactor;
        
        // Calculate lifespan based on drop distance with a minimum of 0.2 and a maximum of 0.6
        const float lifespanFactor = std::clamp(dropDistance / 20.0f, 0.2f, 0.6f);
        
        // Calculate the number of particles based on drop distance, clamped between 2 and 5 particles
        const int particleCount = std::clamp(2 + dropDistance / 5, 2, 5);

        int bottomRow;
        int rotatedIndex;
        int blockX, blockY;
        Particle particle;
        float horizontalVelocity, verticalVelocity;

        // Iterate over each column of the Tetrimino to find the bottom edge
        for (int j = 0; j < 4; ++j) {
            bottomRow = -1;
    
            for (int i = 0; i < 4; ++i) {
                rotatedIndex = getRotatedIndex(currentTetrimino.type, i, j, currentTetrimino.rotation);
                if (tetriminoShapes[currentTetrimino.type][rotatedIndex] != 0) {
                    bottomRow = i;  // Keep track of the bottom-most row for this column
                }
            }
    
            // If a bottom block is found, generate particles
            if (bottomRow != -1) {
                blockX = currentTetrimino.x + j;
                blockY = currentTetrimino.y + bottomRow;
    
                // Create several particles falling from this block
                for (int p = 0; p < particleCount; ++p) {  // Adjust this number to control particle count
                    // Generate horizontal and vertical velocities, clamped between min and max
                    horizontalVelocity = std::clamp((rand() % 100 / 50.0f - 1.0f) * velocityFactor, -maxHorizontalVelocity, maxHorizontalVelocity);
                    verticalVelocity = std::clamp((rand() % 100 / 50.0f) * (2.0f * velocityFactor), minVelocity, maxVerticalVelocity);
    
                    particle = {
                        static_cast<float>(blockX * _w + rand() % _w),  // X-position within the block
                        static_cast<float>(blockY * _h + _h),           // Y-position at the bottom of the block
                        horizontalVelocity,                             // Clamped horizontal velocity
                        verticalVelocity,                               // Clamped downward velocity
                        lifespanFactor,                                 // Lifespan based on drop distance, clamped between 0.2 and 0.6
                        1.0f                                            // Alpha (fully visible)
                    };
                    particles.push_back(particle);
                }
            }
        }
    }



    void hardDrop() {
        // Calculate how far the piece will fall
        hardDropDistance = calculateDropDistance(currentTetrimino, board);
        currentTetrimino.y += hardDropDistance;
        
        // Award points for hard drop (e.g., 2 points per row)
        const int hardDropScore = hardDropDistance * 2;
        tetrisElement->setScore(tetrisElement->getScore() + hardDropScore);
        
        createImpactParticles(hardDropDistance);

        // Place the piece and reset drop distance trackers
        placeTetrimino();
        clearLines();
        spawnNewTetrimino();
    
        // Reset distances after placing
        totalSoftDropDistance = 0;
        hardDropDistance = 0;
        
        if (!isPositionValid(currentTetrimino, board)) {
            tetrisElement->gameOver = true;
        }
    }



    
    void saveGameState() {
        
        cJSON* root = cJSON_CreateObject();
    
        // Save general game state
        cJSON_AddStringToObject(root, "score", std::to_string(tetrisElement->getScore()).c_str());
        cJSON_AddStringToObject(root, "maxHighScore", std::to_string(TetrisElement::maxHighScore).c_str());
        cJSON_AddBoolToObject(root, "paused", TetrisElement::paused);
        cJSON_AddBoolToObject(root, "gameOver", tetrisElement->gameOver);
        cJSON_AddNumberToObject(root, "linesCleared", tetrisElement->getLinesCleared());
        cJSON_AddNumberToObject(root, "level", tetrisElement->getLevel());
        cJSON_AddBoolToObject(root, "hasSwapped", hasSwapped);
    
        // Save additional variables
        cJSON_AddBoolToObject(root, "lastWallKickApplied", lastWallKickApplied);
        cJSON_AddBoolToObject(root, "previousClearWasTetris", previousClearWasTetris);
        cJSON_AddBoolToObject(root, "previousClearWasTSpin", previousClearWasTSpin);
        cJSON_AddNumberToObject(root, "backToBackCount", backToBackCount);
    
        // Save current Tetrimino
        cJSON* currentTetriminoJson = cJSON_CreateObject();
        cJSON_AddNumberToObject(currentTetriminoJson, "type", currentTetrimino.type);
        cJSON_AddNumberToObject(currentTetriminoJson, "rotation", currentTetrimino.rotation);
        cJSON_AddNumberToObject(currentTetriminoJson, "x", currentTetrimino.x);
        cJSON_AddNumberToObject(currentTetriminoJson, "y", currentTetrimino.y);
        cJSON_AddItemToObject(root, "currentTetrimino", currentTetriminoJson);
    
        // Save stored Tetrimino
        cJSON* storedTetriminoJson = cJSON_CreateObject();
        cJSON_AddNumberToObject(storedTetriminoJson, "type", storedTetrimino.type);
        cJSON_AddNumberToObject(storedTetriminoJson, "rotation", storedTetrimino.rotation);
        cJSON_AddNumberToObject(storedTetriminoJson, "x", storedTetrimino.x);
        cJSON_AddNumberToObject(storedTetriminoJson, "y", storedTetrimino.y);
        cJSON_AddItemToObject(root, "storedTetrimino", storedTetriminoJson);
    
        // Save next Tetrimino states (including the two new next pieces)
        cJSON* nextTetriminoJson = cJSON_CreateObject();
        cJSON_AddNumberToObject(nextTetriminoJson, "type", nextTetrimino.type);
        cJSON_AddItemToObject(root, "nextTetrimino", nextTetriminoJson);
    
        cJSON* nextTetrimino1Json = cJSON_CreateObject();
        cJSON_AddNumberToObject(nextTetrimino1Json, "type", nextTetrimino1.type);
        cJSON_AddItemToObject(root, "nextTetrimino1", nextTetrimino1Json);
    
        cJSON* nextTetrimino2Json = cJSON_CreateObject();
        cJSON_AddNumberToObject(nextTetrimino2Json, "type", nextTetrimino2.type);
        cJSON_AddItemToObject(root, "nextTetrimino2", nextTetrimino2Json);
    
        // Save the board state
        cJSON* boardJson = cJSON_CreateArray();
        for (int i = 0; i < BOARD_HEIGHT; ++i) {
            cJSON* rowJson = cJSON_CreateArray();
            for (int j = 0; j < BOARD_WIDTH; ++j) {
                cJSON_AddItemToArray(rowJson, cJSON_CreateNumber(board[i][j]));
            }
            cJSON_AddItemToArray(boardJson, rowJson);
        }
        cJSON_AddItemToObject(root, "board", boardJson);
    
        // Write to the file
        FILE* file = fopen("sdmc:/config/tetris/save_state.json", "w");
        if (file) {
            char* jsonString = cJSON_Print(root);
            if (jsonString) {
                fwrite(jsonString, 1, strlen(jsonString), file);
                free(jsonString);
            }
            fclose(file);
        }
        
        cJSON_Delete(root);
    }
    
    void loadGameState() {
        // Open file using C-style file I/O
        FILE* file = fopen("sdmc:/config/tetris/save_state.json", "r");
        if (!file) return;
        
        // Get file size
        fseek(file, 0, SEEK_END);
        long fileSize = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        // Read file content
        std::string jsonContent;
        jsonContent.resize(fileSize);
        size_t bytesRead = fread(&jsonContent[0], 1, fileSize, file);
        fclose(file);
        
        // Resize string to actual bytes read (in case of early EOF)
        jsonContent.resize(bytesRead);
            
        cJSON* root = cJSON_Parse(jsonContent.c_str());
        if (!root) return;
        
        // Load general game state
        cJSON* scoreJson = cJSON_GetObjectItem(root, "score");
        cJSON* maxHighScoreJson = cJSON_GetObjectItem(root, "maxHighScore");
        
        if (cJSON_IsString(scoreJson)) tetrisElement->setScore(std::stoull(scoreJson->valuestring));
        if (cJSON_IsString(maxHighScoreJson)) TetrisElement::maxHighScore = std::stoull(maxHighScoreJson->valuestring);
        
        cJSON* pausedJson = cJSON_GetObjectItem(root, "paused");
        if (cJSON_IsBool(pausedJson)) TetrisElement::paused = cJSON_IsTrue(pausedJson);
        
        cJSON* gameOverJson = cJSON_GetObjectItem(root, "gameOver");
        if (cJSON_IsBool(gameOverJson)) tetrisElement->gameOver = cJSON_IsTrue(gameOverJson);
    
        cJSON* linesClearedJson = cJSON_GetObjectItem(root, "linesCleared");
        if (cJSON_IsNumber(linesClearedJson)) tetrisElement->setLinesCleared(linesClearedJson->valueint);
        
        cJSON* levelJson = cJSON_GetObjectItem(root, "level");
        if (cJSON_IsNumber(levelJson)) tetrisElement->setLevel(levelJson->valueint);
        
        cJSON* hasSwappedJson = cJSON_GetObjectItem(root, "hasSwapped");
        if (cJSON_IsBool(hasSwappedJson)) hasSwapped = cJSON_IsTrue(hasSwappedJson);
        
        // Load additional variables
        cJSON* lastWallKickAppliedJson = cJSON_GetObjectItem(root, "lastWallKickApplied");
        if (cJSON_IsBool(lastWallKickAppliedJson)) lastWallKickApplied = cJSON_IsTrue(lastWallKickAppliedJson);
        
        cJSON* previousClearWasTetrisJson = cJSON_GetObjectItem(root, "previousClearWasTetris");
        if (cJSON_IsBool(previousClearWasTetrisJson)) previousClearWasTetris = cJSON_IsTrue(previousClearWasTetrisJson);
        
        cJSON* previousClearWasTSpinJson = cJSON_GetObjectItem(root, "previousClearWasTSpin");
        if (cJSON_IsBool(previousClearWasTSpinJson)) previousClearWasTSpin = cJSON_IsTrue(previousClearWasTSpinJson);
        
        cJSON* backToBackCountJson = cJSON_GetObjectItem(root, "backToBackCount");
        if (cJSON_IsNumber(backToBackCountJson)) backToBackCount = backToBackCountJson->valueint;
    
        // Load current Tetrimino
        cJSON* currentTetriminoJson = cJSON_GetObjectItem(root, "currentTetrimino");
        if (cJSON_IsObject(currentTetriminoJson)) {
            cJSON* typeJson = cJSON_GetObjectItem(currentTetriminoJson, "type");
            if (cJSON_IsNumber(typeJson)) currentTetrimino.type = typeJson->valueint;
            
            cJSON* rotationJson = cJSON_GetObjectItem(currentTetriminoJson, "rotation");
            if (cJSON_IsNumber(rotationJson)) currentTetrimino.rotation = rotationJson->valueint;
            
            cJSON* xJson = cJSON_GetObjectItem(currentTetriminoJson, "x");
            if (cJSON_IsNumber(xJson)) currentTetrimino.x = xJson->valueint;
            
            cJSON* yJson = cJSON_GetObjectItem(currentTetriminoJson, "y");
            if (cJSON_IsNumber(yJson)) currentTetrimino.y = yJson->valueint;
        }
    
        // Load stored Tetrimino
        cJSON* storedTetriminoJson = cJSON_GetObjectItem(root, "storedTetrimino");
        if (cJSON_IsObject(storedTetriminoJson)) {
            cJSON* typeJson = cJSON_GetObjectItem(storedTetriminoJson, "type");
            if (cJSON_IsNumber(typeJson)) storedTetrimino.type = typeJson->valueint;
            
            cJSON* rotationJson = cJSON_GetObjectItem(storedTetriminoJson, "rotation");
            if (cJSON_IsNumber(rotationJson)) storedTetrimino.rotation = rotationJson->valueint;
            
            cJSON* xJson = cJSON_GetObjectItem(storedTetriminoJson, "x");
            if (cJSON_IsNumber(xJson)) storedTetrimino.x = xJson->valueint;
            
            cJSON* yJson = cJSON_GetObjectItem(storedTetriminoJson, "y");
            if (cJSON_IsNumber(yJson)) storedTetrimino.y = yJson->valueint;
        }
    
        // Load next Tetrimino states (including the two new next pieces)
        cJSON* nextTetriminoJson = cJSON_GetObjectItem(root, "nextTetrimino");
        if (cJSON_IsObject(nextTetriminoJson)) {
            cJSON* typeJson = cJSON_GetObjectItem(nextTetriminoJson, "type");
            if (cJSON_IsNumber(typeJson)) nextTetrimino.type = typeJson->valueint;
        }
    
        cJSON* nextTetrimino1Json = cJSON_GetObjectItem(root, "nextTetrimino1");
        if (cJSON_IsObject(nextTetrimino1Json)) {
            cJSON* typeJson = cJSON_GetObjectItem(nextTetrimino1Json, "type");
            if (cJSON_IsNumber(typeJson)) nextTetrimino1.type = typeJson->valueint;
        }
    
        cJSON* nextTetrimino2Json = cJSON_GetObjectItem(root, "nextTetrimino2");
        if (cJSON_IsObject(nextTetrimino2Json)) {
            cJSON* typeJson = cJSON_GetObjectItem(nextTetrimino2Json, "type");
            if (cJSON_IsNumber(typeJson)) nextTetrimino2.type = typeJson->valueint;
        }
    
        // Load the board state
        cJSON* boardJson = cJSON_GetObjectItem(root, "board");
        if (cJSON_IsArray(boardJson)) {
            for (int i = 0; i < BOARD_HEIGHT; ++i) {
                cJSON* rowJson = cJSON_GetArrayItem(boardJson, i);
                if (cJSON_IsArray(rowJson)) {
                    for (int j = 0; j < BOARD_WIDTH; ++j) {
                        cJSON* cellJson = cJSON_GetArrayItem(rowJson, j);
                        if (cJSON_IsNumber(cellJson)) {
                            board[i][j] = cellJson->valueint;
                        }
                    }
                }
            }
        }
    
        cJSON_Delete(root);
    }


    // Define constants for DAS (Delayed Auto-Shift) and ARR (Auto-Repeat Rate)
    static constexpr int DAS = 300;  // DAS delay in milliseconds
    static constexpr int ARR = 40;   // ARR interval in milliseconds
    
    // Variables to track key hold states and timing
    std::chrono::time_point<std::chrono::steady_clock> lastLeftMove, lastRightMove, lastDownMove;
    bool leftHeld = false, rightHeld = false, downHeld = false;
    bool leftARR = false, rightARR = false, downARR = false;
    
    bool handleInput(u64 keysDown, u64 keysHeld, touchPosition touchInput, JoystickPosition leftJoyStick, JoystickPosition rightJoyStick) override {
        const auto currentTime = std::chrono::steady_clock::now();
        bool moved = false;
    
        // Handle the rest of the input only if the game is not paused and not over
        if (simulatedBack) {
            keysDown |= KEY_B;
            simulatedBack = false;
        }
    
        if (simulatedSelect) {
            keysDown |= KEY_A;
            simulatedSelect = false;
        }
    
        // Handle input when the game is paused or over
        if (TetrisElement::paused || tetrisElement->gameOver) {
            if (tetrisElement->gameOver) {
                isGameOver = true;
                if (keysDown & KEY_A || keysDown & KEY_PLUS) {
                    // Restart game
                    //disableSound.exchange(true, std::memory_order_acq_rel);
                    triggerRumbleDoubleClick.store(true, std::memory_order_release);
                    resetGame();
                    return true;
                }
                // Allow closing the overlay with KEY_B only when paused or game over
                if (keysDown & KEY_B) {
                    //resetGame();
                }
            }
            // Unpause if KEY_PLUS is pressed
            if (keysDown & KEY_PLUS) {
                //disableSound.exchange(true, std::memory_order_acq_rel);
                triggerRumbleClick.store(true, std::memory_order_release);
                TetrisElement::paused = false;
            }
            // Allow closing the overlay with KEY_B only when paused or game over
            if (keysDown & KEY_B) {
                //saveGameState();
                triggerRumbleDoubleClick.store(true, std::memory_order_release);
                tsl::Overlay::get()->close();
            }
    
            // Return true to indicate input was handled
            return true;
        } else {
            
            // Handle swapping with the stored Tetrimino
            if (keysDown & KEY_L && !(keysHeld & ~(KEY_L|KEY_LEFT|KEY_RIGHT|KEY_DOWN|KEY_UP) & ALL_KEYS_MASK) && !hasSwapped) {
                triggerRumbleDoubleClick.store(true, std::memory_order_release);
                swapStoredTetrimino();
                hasSwapped = true;
            }
            
            // Handle left movement with DAS and ARR
            if (keysHeld & KEY_LEFT) {
                if (!leftHeld) {
                    // First press
                    moved = move(-1, 0);
                    if (moved) {
                        triggerRumbleClick.store(true, std::memory_order_release);
                    }
                    lastLeftMove = currentTime;
                    leftHeld = true;
                    leftARR = false; // Reset ARR phase
                } else {
                    // DAS check
                    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastLeftMove).count();
                    if (!leftARR && elapsed >= DAS) {
                        // Once DAS is reached, start ARR
                        moved = move(-1, 0);
                        if (moved) {
                            triggerRumbleClick.store(true, std::memory_order_release);
                        }
                        lastLeftMove = currentTime; // Reset time for ARR phase
                        leftARR = true;
                    } else if (leftARR && elapsed >= ARR) {
                        // Auto-repeat after ARR interval
                        moved = move(-1, 0);
                        if (moved) {
                            triggerRumbleClick.store(true, std::memory_order_release);
                        }
                        lastLeftMove = currentTime; // Keep resetting for ARR
                    }
                }
                
            } else {
                leftHeld = false;
            }
            
            // Handle right movement with DAS and ARR
            if (keysHeld & KEY_RIGHT) {
                if (!rightHeld) {
                    // First press
                    
                    moved = move(1, 0);
                    if (moved) {
                        triggerRumbleClick.store(true, std::memory_order_release);
                    }
                    lastRightMove = currentTime;
                    rightHeld = true;
                    rightARR = false; // Reset ARR phase
                } else {
                    // DAS check
                    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastRightMove).count();
                    if (!rightARR && elapsed >= DAS) {
                        // Once DAS is reached, start ARR
                        moved = move(1, 0);
                        if (moved) {
                            triggerRumbleClick.store(true, std::memory_order_release);
                        }
                        lastRightMove = currentTime;
                        rightARR = true;
                    } else if (rightARR && elapsed >= ARR) {
                        // Auto-repeat after ARR interval
                        moved = move(1, 0);
                        if (moved) {
                            triggerRumbleClick.store(true, std::memory_order_release);
                        }
                        lastRightMove = currentTime; // Keep resetting for ARR
                    }
                }
            } else {
                rightHeld = false;
            }
            
            // Handle down movement with DAS and ARR for soft dropping
            if (keysHeld & KEY_DOWN) {
                if (!downHeld) {
                    // Check if the piece is on the floor and lock it immediately
                    if (isOnFloor()) {
                        triggerRumbleClick.store(true, std::memory_order_release);
                        hardDrop();
                    } else {
                        // First press
                        moved = move(0, 1);
                        lastDownMove = currentTime;
                        downHeld = true;
                        downARR = false; // Reset ARR phase
                    }
    
                } else {
                    // DAS check
                    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastDownMove).count();
                    if (!downARR && elapsed >= DAS) {
                        if (isOnFloor()) {
                            triggerRumbleClick.store(true, std::memory_order_release);
                            hardDrop();
                        } else {
                            // Once DAS is reached, start ARR
                            moved = move(0, 1);
                            lastDownMove = currentTime;
                            downARR = true;
                        }
                    } else if (downARR && elapsed >= ARR) {
                        if (isOnFloor()) {
                            triggerRumbleClick.store(true, std::memory_order_release);
                            hardDrop();
                        } else {
                            // Auto-repeat after ARR interval
                            moved = move(0, 1);
                            lastDownMove = currentTime;
                        }
                    }
                }
    
            } else {
                downHeld = false;
            }
            
            // Handle hard drop with the Up key
            if (keysDown & KEY_UP) {
                triggerRumbleClick.store(true, std::memory_order_release);
                hardDrop();  // Perform hard drop immediately
            }
            
            // Handle rotation inputs
            if (keysDown & KEY_A) {
                triggerRumbleClick.store(true, std::memory_order_release);
                //bool rotated = rotate(); // Rotate clockwise
                if (rotate()) {
                    moved = true;
                }
            } else if (keysDown & KEY_B) {
                triggerRumbleClick.store(true, std::memory_order_release);
                //bool rotated = rotateCounterclockwise(); // Rotate counterclockwise
                if (rotateCounterclockwise()) {
                    moved = true;
                }
            }
            
            // Handle pause/unpause
            if (keysDown & KEY_PLUS) {
                triggerRumbleClick.store(true, std::memory_order_release);
                TetrisElement::paused = !TetrisElement::paused;
            }
            
            // Reset the lock delay timer if the piece has moved or rotated
            if (moved) {
                lockDelayCounter = std::chrono::milliseconds(0);
                return true;
            }
            
        }
        return false;
    }
    
    

private:
    std::array<std::array<int, BOARD_WIDTH>, BOARD_HEIGHT> board{};
    Tetrimino currentTetrimino;
    Tetrimino nextTetrimino;
    Tetrimino nextTetrimino1;
    Tetrimino nextTetrimino2;
    TetrisElement* tetrisElement;
    u16 _w;
    u16 _h;
    std::chrono::time_point<std::chrono::steady_clock> timeSinceLastFrame;

    // Lock delay variables
    std::chrono::milliseconds lockDelayTime;
    std::chrono::milliseconds lockDelayCounter;

    // Fall speed variables
    std::chrono::milliseconds initialFallSpeed; // No fallSpeed in game state now
    std::chrono::milliseconds fallCounter;

    int totalSoftDropDistance = 0;  // Tracks the number of rows dropped for soft drops
    int hardDropDistance = 0;       // Tracks the number of rows dropped for hard drops

    static constexpr int maxLockDelayMoves = 15;  // Maximum number of times the player can move left/right before the piece locks
    int lockDelayMoves = 0;  // Number of times the player has moved left/right since the piece hit the ground

    // Add a member variable to track if a wall kick was applied
    bool lastWallKickApplied = false;
    bool previousClearWasTetris = false; // Track if the previous clear was a Tetris
    bool previousClearWasTSpin = false;  // Track if the previous clear was a T-Spin
    int backToBackCount = 1;

    bool pieceWasKickedUp = false;

    // Function to adjust the fall speed based on the current level
    //void adjustFallSpeed() {
    //    int minSpeed = 200; // Minimum fall speed (200 ms)
    //    int speedDecrease = tetrisElement->getLevel() * 50; // Decrease fall time by 50ms per level
    //    fallSpeed = std::max(initialFallSpeed - std::chrono::milliseconds(speedDecrease), std::chrono::milliseconds(minSpeed));
    //}

    // Function to dynamically calculate fall speed based on the current level
    std::chrono::milliseconds getFallSpeed() {
        // Define the fall speeds in milliseconds based on levels (simulating classic Tetris)
        static constexpr std::array<int, 30> fallSpeeds = {
            800, // Level 0: 800ms per row drop
            720, // Level 1
            630, // Level 2
            550, // Level 3
            470, // Level 4
            380, // Level 5
            300, // Level 6
            220, // Level 7
            130, // Level 8
            100, // Level 9
            80,  // Level 10
            80,  // Level 11
            80,  // Level 12
            80,  // Level 13
            70,  // Level 14
            70,  // Level 15
            70,  // Level 16
            50,  // Level 17
            50,  // Level 18
            50,  // Level 19
            30,  // Level 20
            30,  // Level 21
            30,  // Level 22
            20,  // Level 23
            20,  // Level 24
            20,  // Level 25
            20,  // Level 26
            20,  // Level 27
            20,  // Level 28
            16   // Level 29 and above (maximum speed, 16ms per row)
        };
    
        // Get the appropriate fall speed for the current level, clamping if necessary
        const int level = std::min(tetrisElement->getLevel(), static_cast<int>(fallSpeeds.size() - 1));
        
        // Set a minimum threshold for fall speed to avoid it becoming too fast
        const int fallSpeed = std::max(fallSpeeds[level], 16); // Minimum 16ms
        
        return std::chrono::milliseconds(fallSpeed);
    }

    bool isOnFloor() {
        // If the piece was kicked up, it's not on the floor
        if (pieceWasKickedUp) {
            return true;
        }

        int rotatedIndex;
        int x, y;

        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                rotatedIndex = getRotatedIndex(currentTetrimino.type, i, j, currentTetrimino.rotation);
    
                if (tetriminoShapes[currentTetrimino.type][rotatedIndex] != 0) {
                    x = currentTetrimino.x + j;
                    y = currentTetrimino.y + i;
    
                    // Check if it's at the bottom of the board or on top of another block
                    if (y + 1 >= BOARD_HEIGHT || board[y + 1][x] != 0) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    bool move(int dx, int dy) {
        std::lock_guard<std::mutex> lock(boardMutex);  // Lock to prevent race conditions
        bool success = false;
    
        // Attempt to move the Tetrimino
        currentTetrimino.x += dx;
        currentTetrimino.y += dy;
        
        // Check if the new position is valid
        if (!isPositionValid(currentTetrimino, board)) {
            // Revert the move if invalid
            currentTetrimino.x -= dx;
            currentTetrimino.y -= dy;
        } else {
            success = true;
    
            // If the piece moved down
            if (dy > 0) {
                totalSoftDropDistance += dy;  // Accumulate soft drop distance for scoring
                
                // Only reset lock delay if not recently kicked up and not on the floor
                if (!pieceWasKickedUp) {
                    lockDelayMoves = 0;  // Reset horizontal move counter
                    lockDelayCounter = std::chrono::milliseconds(0);  // Reset lock delay
                }
            }
    
            // Horizontal movement logic remains the same
            else if (dx != 0) {
                if (isOnFloor()) {
                    if (lockDelayMoves < maxLockDelayMoves) {
                        lockDelayCounter = std::chrono::milliseconds(0);
                        lastRotationOrMoveTime = std::chrono::steady_clock::now();
                        lockDelayMoves++;
                    }
                } else {
                    lockDelayCounter = std::chrono::milliseconds(0);
                    lastRotationOrMoveTime = std::chrono::steady_clock::now();
                }
            }
        }
    
        return success;
    }

    bool rotate() {
        const int previousRotation = currentTetrimino.rotation;
        const int previousX = currentTetrimino.x;
        const int previousY = currentTetrimino.y;
        
        rotatePiece(-1); // Clockwise rotation
        
        // Check if rotation actually succeeded by comparing state
        return (currentTetrimino.rotation != previousRotation || 
                currentTetrimino.x != previousX || 
                currentTetrimino.y != previousY);
    }
    
    bool rotateCounterclockwise() {
        const int previousRotation = currentTetrimino.rotation;
        const int previousX = currentTetrimino.x;
        const int previousY = currentTetrimino.y;
        
        rotatePiece(1); // Counterclockwise rotation
        
        // Check if rotation actually succeeded by comparing state
        return (currentTetrimino.rotation != previousRotation || 
                currentTetrimino.x != previousX || 
                currentTetrimino.y != previousY);
    }

    bool tSpinOccurred = false; // Add this member to TetrisGui class
    
    void rotatePiece(int direction) {
        std::lock_guard<std::mutex> lock(boardMutex);
        
        const int previousRotation = currentTetrimino.rotation;
        const int previousX = currentTetrimino.x;
        const int previousY = currentTetrimino.y;
        
        // O piece doesn't rotate - early return
        if (currentTetrimino.type == 3) {
            return;
        }
        
        // Perform rotation
        currentTetrimino.rotation = (currentTetrimino.rotation + direction + 4) % 4;
        
        const auto& kicks = (currentTetrimino.type == 0) ? wallKicksI : wallKicksJLSTZ;
        
        lastWallKickApplied = false;
        bool rotationSuccessful = false;
        
        // First, check if the piece can fit without any kick
        if (isPositionValid(currentTetrimino, board)) {
            rotationSuccessful = true;
            pieceWasKickedUp = false;
        } else {
            // Calculate the correct wall kick index based on rotation transition
            int kickIndex;
            if (direction < 0) {
                // Clockwise: 0->1, 1->2, 2->3, 3->0
                kickIndex = previousRotation;
            } else {
                // Counter-clockwise: 1->0, 2->1, 3->2, 0->3
                kickIndex = currentTetrimino.rotation;
            }
            
            // Try the standard wall kicks FIRST
            for (int i = 0; i < 5; ++i) {
                const auto& kick = kicks[kickIndex][i];
                
                // Apply the kick
                currentTetrimino.x = previousX + kick.first;
                currentTetrimino.y = previousY + kick.second;
                
                if (isPositionValid(currentTetrimino, board)) {
                    rotationSuccessful = true;
                    lastWallKickApplied = (kick.first != 0 || kick.second != 0);
                    pieceWasKickedUp = (kick.second < 0);
                    break;
                }
            }
    
            // If standard kicks fail, try extra kicks with MORE aggressive options for ALL pieces
            if (!rotationSuccessful) {
                // Extended kicks that work for L, J, and other pieces against walls
                std::array<std::pair<int, int>, 16> extraKicks;
                
                if (currentTetrimino.type == 0) {
                    // I-piece needs more upward kicks
                    extraKicks = {{
                        {0, -1}, {0, -2}, {0, -3}, {0, 1}, 
                        {1, 0}, {-1, 0}, {2, 0}, {-2, 0},
                        {1, -1}, {-1, -1}, {0, 2},
                        {1, 1}, {-1, 1}, {2, -1}, {-2, -1}, {1, -2}
                    }};
                } else {
                    // More comprehensive kicks for L, J, S, T, Z pieces
                    extraKicks = {{
                        {0, 1}, {0, -1}, {1, 0}, {-1, 0}, 
                        {0, 2}, {2, 0}, {-2, 0},
                        {1, 1}, {-1, 1}, {1, -1}, {-1, -1},
                        {0, -2}, {2, 1}, {-2, 1}, {2, -1}, {-2, -1}
                    }};
                }
                
                for (const auto& kick : extraKicks) {
                    currentTetrimino.x = previousX + kick.first;
                    currentTetrimino.y = previousY + kick.second;
                    
                    if (isPositionValid(currentTetrimino, board)) {
                        rotationSuccessful = true;
                        lastWallKickApplied = true;
                        pieceWasKickedUp = (kick.second < 0);
                        break;
                    }
                }
            }
        }
    
        // If rotation failed, revert to the previous state
        if (!rotationSuccessful) {
            currentTetrimino.rotation = previousRotation;
            currentTetrimino.x = previousX;
            currentTetrimino.y = previousY;
            pieceWasKickedUp = false;
            return; // Exit early - no lock delay reset needed
        }
    
        // Reset lock delay only if the rotation was successful
        if (isOnFloor()) {
            if (lockDelayMoves < maxLockDelayMoves) {
                lockDelayCounter = std::chrono::milliseconds(0);
                lastRotationOrMoveTime = std::chrono::steady_clock::now();
                lockDelayMoves++;
            }
        } else {
            lockDelayCounter = std::chrono::milliseconds(0);
            lastRotationOrMoveTime = std::chrono::steady_clock::now();
        }
    }
    
    
    bool performedWallKick() {
        return lastWallKickApplied;  // Simply return whether the last rotation involved a wall kick
    }

    bool isMiniTSpin() {
        if (currentTetrimino.type != 5) return false; // Only T piece can T-Spin
    
        // Mini T-Spins often occur when a rotation involves a wall kick but isn't surrounded as a full T-spin.
        return !isTSpin() && lastWallKickApplied;
    }
    
    bool isTSpin() {
        if (currentTetrimino.type != 5) return false; // Only T piece can T-Spin
    
        // Check corners around the T piece center
        const int centerX = currentTetrimino.x + 1;
        const int centerY = currentTetrimino.y + 1;
        int blockedCorners = 0;
    
        // Check four corners
        if (!isWithinBounds(centerX - 1, centerY - 1) || board[centerY - 1][centerX - 1] != 0) blockedCorners++;
        if (!isWithinBounds(centerX + 1, centerY - 1) || board[centerY - 1][centerX + 1] != 0) blockedCorners++;
        if (!isWithinBounds(centerX - 1, centerY + 1) || board[centerY + 1][centerX - 1] != 0) blockedCorners++;
        if (!isWithinBounds(centerX + 1, centerY + 1) || board[centerY + 1][centerX + 1] != 0) blockedCorners++;
    
        // A T-Spin occurs if 3 or more corners are blocked
        return blockedCorners >= 3 && lastWallKickApplied;
    }
    
    bool isWithinBounds(int x, int y) {
        return x >= 0 && x < BOARD_WIDTH && y >= 0 && y < BOARD_HEIGHT;
    }
    


    void placeTetrimino() {
        std::lock_guard<std::mutex> lock(boardMutex); // Lock the mutex for board access
        bool pieceAboveTop = false;  // Track if any part of the piece is above the top of the board

        int rotatedIndex;
        int x, y;
        // Place the Tetrimino on the board
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                rotatedIndex = getRotatedIndex(currentTetrimino.type, i, j, currentTetrimino.rotation);
                
                if (tetriminoShapes[currentTetrimino.type][rotatedIndex] != 0) {
                    x = currentTetrimino.x + j;
                    y = currentTetrimino.y + i;
    
                    // If any part of the piece is above the top of the board (y < 0)
                    if (y < 0) {
                        pieceAboveTop = true;
                        continue;  // Skip placing this block
                    }
    
                    // Only place the block if y is within the board (y >= 0)
                    if (y >= 0) {
                        board[y][x] = currentTetrimino.type + 1;  // Place the block
                    }
                }
            }
        }
        pieceWasKickedUp = false;

        // If any part of the piece was above the top of the board, trigger game over
        if (pieceAboveTop) {
            tetrisElement->gameOver = true;
            return;  // Early return to prevent further processing
        }
    
        // Award points for soft drops (apply accumulated points)
        if (totalSoftDropDistance > 0) {
            const int softDropScore = totalSoftDropDistance * 1;  // 1 point per row for soft drops
            tetrisElement->setScore(tetrisElement->getScore() + softDropScore);
        }
    
        // Reset drop distance trackers after placement
        totalSoftDropDistance = 0;
        hardDropDistance = 0;
    
        // Reset the swap flag after placing a Tetrimino
        hasSwapped = false;

        
    }

    // Create line clear particles outside the main line-clear loop to reduce mutex locking time
    void createLineClearParticles(int row) {
        for (int x = 0; x < BOARD_WIDTH; ++x) {
            for (int p = 0; p < 10; ++p) {
                particles.push_back(Particle{
                    static_cast<float>(x * _w + _w / 2),
                    static_cast<float>(row * _h + _h / 2),
                    (rand() % 100 / 50.0f - 1.0f) * 8,
                    (rand() % 100 / 50.0f - 1.0f) * 8,
                    0.5f,
                    1.0f
                });
            }
        }
    }

    void createCenterExplosionParticles() {
        // Calculate the center row of the board
        //int centerRow = BOARD_HEIGHT / 2;
        for (int y = 0; y < BOARD_HEIGHT; ++y) {
            // Generate particles at the center row
            for (int x = 0; x < BOARD_WIDTH; ++x) {
                for (int p = 0; p < 10; ++p) {
                    particles.push_back(Particle{
                        static_cast<float>(x * _w + _w / 2),  // X position in the center row
                        static_cast<float>(y * _h + _h / 2),  // Y position in the center row
                        (rand() % 100 / 50.0f - 1.0f) * 8,  // Random velocity in X direction
                        (rand() % 100 / 50.0f - 1.0f) * 8,  // Random velocity in Y direction
                        0.5f,  // Lifespan
                        1.0f   // Initial alpha (fully visible)
                    });
                }
            }
        }
    }

    
    // Modify the clearLines function to handle scoring and leveling up
    void clearLines() {
        std::lock_guard<std::mutex> particleLock(particleMutex);  // Lock the particle system to avoid concurrent access
        std::lock_guard<std::mutex> lock(boardMutex);  // Lock during line clearing
        
        int linesClearedInThisTurn = 0;
        int totalYPosition = 0;
        
        bool fullLine;
        for (int i = 0; i < BOARD_HEIGHT; ++i) {
            fullLine = true;
    
            for (int j = 0; j < BOARD_WIDTH; ++j) {
                if (board[i][j] == 0) {
                    fullLine = false;
                    break;
                }
            }
    
            if (fullLine) {
                linesClearedInThisTurn++;
                totalYPosition += i * _h;
    
                // Particle creation moved to another method to reduce mutex lock time
                createLineClearParticles(i);
    
                // Shift rows down after clearing the full line
                for (int y = i; y > 0; --y) {
                    for (int x = 0; x < BOARD_WIDTH; ++x) {
                        board[y][x] = board[y - 1][x];
                    }
                }
    
                // Clear the top row (with extra check to prevent top-bound crashes)
                for (int x = 0; x < BOARD_WIDTH; ++x) {
                    if (board[0][x] != 0) {
                        board[0][x] = 0;
                    }
                }
            }
        }
    
        // If lines were cleared, update the score and level, and show feedback text
        if (linesClearedInThisTurn > 0) {
            // Update the total lines cleared
            tetrisElement->setLinesCleared(tetrisElement->getLinesCleared() + linesClearedInThisTurn);
            
            int baseScore = 0;
            float backToBackBonus = 1.0f;
        
            // Handle back-to-back bonus
            const bool isBackToBack = (previousClearWasTetris || previousClearWasTSpin) &&
                                (linesClearedInThisTurn == 4 || isTSpin());
        

            // Track the back-to-back chain count
            //static int backToBackCount = 1;
            if (isBackToBack) {
                backToBackBonus = 1.5f;  // 50% bonus for back-to-back Tetrises or T-Spins
                backToBackCount++;  // Increment back-to-back count
            } else {
                backToBackCount = 1;  // Reset back-to-back count
            }
        
            // Update score based on how many lines were cleared
            switch (linesClearedInThisTurn) {
                case 1:
                    if (isTSpin()) {
                        baseScore = isMiniTSpin() ? 100 : 400;  // Mini T-Spin or T-Spin Single
                    } else {
                        baseScore = 100;  // Single line clear
                    }
                    break;
                case 2:
                    if (isTSpin()) {
                        baseScore = 700;  // T-Spin Double
                    } else {
                        baseScore = 300;  // Double line clear
                    }
                    break;
                case 3:
                    baseScore = 500;  // Triple line clear
                    break;
                case 4:
                    baseScore = 800;  // Base Tetris
                    break;
            }
        
            // Apply back-to-back bonus for Tetrises and T-Spins
            if ((linesClearedInThisTurn == 4 || isTSpin()) && isBackToBack) {
                baseScore = static_cast<int>(baseScore * backToBackBonus);  // Apply bonus
            }
        
            // Multiply base score by the current level
            const int newScore = baseScore * tetrisElement->getLevel();
            tetrisElement->setScore(tetrisElement->getScore() + newScore);
        
            // Store the score for the current lines-cleared move in linesClearedScore
            tetrisElement->linesClearedScore = newScore;
        
            // Handle back-to-back state
            if (linesClearedInThisTurn == 4) {
                previousClearWasTetris = true;
                previousClearWasTSpin = false;
            } else if (isTSpin()) {
                previousClearWasTSpin = true;
                previousClearWasTetris = false;
            } else {
                previousClearWasTetris = false;
                previousClearWasTSpin = false;
            }
        
            // Level up after clearing a certain number of lines
            linesClearedForLevelUp += linesClearedInThisTurn;
            if (linesClearedForLevelUp >= LINES_PER_LEVEL) {
                linesClearedForLevelUp -= LINES_PER_LEVEL;  // Reset the count for the next level
                tetrisElement->setLevel(tetrisElement->getLevel() + 1);  // Increase the level
            }
        
            // Show feedback text based on the number of lines cleared
            switch (linesClearedInThisTurn) {
                case 1:
                    tetrisElement->linesClearedText = isTSpin() ? "T-Spin\nSingle" : "Single";
                    break;
                case 2:
                    tetrisElement->linesClearedText = isTSpin() ? "T-Spin\nDouble" : "Double";
                    break;
                case 3:
                    tetrisElement->linesClearedText = "Triple";
                    break;
                case 4:
                    tetrisElement->linesClearedText = isBackToBack ? std::to_string(backToBackCount) + "x Tetris" : "Tetris";
                    break;
            }
        
            tetrisElement->showText = true;
            tetrisElement->fadeAlpha = 0.0f;  // Start fade animation
            tetrisElement->textStartTime = std::chrono::steady_clock::now();  // Track animation start time

            triggerRumbleDoubleClick.store(true, std::memory_order_release);
        }
        

    }
    
    
    void spawnNewTetrimino() {
        triggerRumbleClick.store(true, std::memory_order_release);
        // Move nextTetrimino to currentTetrimino
        currentTetrimino = nextTetrimino;
        
        int rotatedIndex;
        // Calculate the width of the current Tetrimino
        int minX = 4, maxX = -1;  // Initialize with the opposite extremes
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                rotatedIndex = getRotatedIndex(currentTetrimino.type, i, j, currentTetrimino.rotation);
                if (tetriminoShapes[currentTetrimino.type][rotatedIndex] != 0) {
                    if (j < minX) minX = j;
                    if (j > maxX) maxX = j;
                }
            }
        }
    
        // Calculate the actual width of the Tetrimino
        const int pieceWidth = maxX - minX + 1;
    
        // Set the X position to center the Tetrimino on the board
        currentTetrimino.x = (BOARD_WIDTH - pieceWidth) / 2 - minX;
    
        // Move nextTetrimino1 to nextTetrimino
        nextTetrimino = nextTetrimino1;
    
        // Move nextTetrimino2 to nextTetrimino1
        nextTetrimino1 = nextTetrimino2;
    
        // Generate a new random piece for nextTetrimino2
        nextTetrimino2 = Tetrimino(rand() % 7);
    
        // Calculate the bottommost row with a block
        int bottommostRow = -1;
        for (int i = 3; i >= 0; --i) {  // Start from bottom and go up
            for (int j = 0; j < 4; ++j) {
                rotatedIndex = getRotatedIndex(currentTetrimino.type, i, j, currentTetrimino.rotation);
                if (tetriminoShapes[currentTetrimino.type][rotatedIndex] != 0) {
                    bottommostRow = i;
                    break;
                }
            }
            if (bottommostRow != -1) break;  // Found the bottommost row
        }
    
        // Set the Y position so the bottommost blocks are at row 0 (1 block into the board)
        currentTetrimino.y = -bottommostRow;
    
        // Check if the new Tetrimino is in a valid position
        if (!isPositionValid(currentTetrimino, board)) {
            // Game over: the new Tetrimino can't be placed
            tetrisElement->gameOver = true;
        }
    }
    


};

class Overlay : public tsl::Overlay {
public:

    virtual void initServices() override {
        tsl::overrideBackButton = true; // for properly overriding the always go back functionality of KEY_B
        ult::createDirectory("sdmc:/config/tetris/");
    }

    virtual void exitServices() override {}

    virtual void onShow() override {}
    virtual void onHide() override { TetrisElement::paused = true; }

    virtual std::unique_ptr<tsl::Gui> loadInitialGui() override {
        firstLoad = true;
        auto r = initially<TetrisGui>();
        gameGui = (TetrisGui*)r.get();
        return r;
    }

private:
    std::string savedGameData;
    TetrisGui* gameGui;
};

/**
 * @brief The entry point of the application.
 *
 * This function serves as the entry point for the application. It takes command-line arguments,
 * initializes necessary services, and starts the main loop of the overlay. The `argc` parameter
 * represents the number of command-line arguments, and `argv` is an array of C-style strings
 * containing the actual arguments.
 *
 * @param argc The number of command-line arguments.
 * @param argv An array of C-style strings representing command-line arguments.
 * @return The application's exit code.
 */
int main(int argc, char* argv[]) {
    return tsl::loop<Overlay, tsl::impl::LaunchFlags::None>(argc, argv);
}
