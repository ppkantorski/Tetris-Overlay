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

std::mutex boardMutex;  // Declare a mutex for board access
std::mutex particleMutex;

bool isGameOver = false;


struct Particle {
    float x, y;      // Position
    float vx, vy;    // Velocity
    float life;      // Lifespan
    float alpha;     // Transparency (fades out)
};

//const int MAX_PARTICLES = 800;
std::vector<Particle> particles;


// Define the Tetrimino shapes
const std::array<std::array<int, 16>, 7> tetriminoShapes = {{
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
const std::array<std::pair<int, int>, 7> rotationCenters = {{
    {1.5f, 1.5f}, // I piece (rotating around the second cell in a 4x4 grid)
    {1, 1}, // J piece
    {1, 1}, // L piece
    {1, 1}, // O piece
    {1, 1}, // S piece
    {1, 1}, // T piece
    {1, 1}  // Z piece
}};

// Wall kicks for I piece (SRS)
const std::array<std::array<std::pair<int, int>, 5>, 4> wallKicksI = {{
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
const std::array<std::array<std::pair<int, int>, 5>, 4> wallKicksJLSTZ = {{
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
const std::array<tsl::Color, 7> tetriminoColors = {{
    {0x0, 0xF, 0xF, 0xF}, // Cyan - I (R=0, G=F, B=F, A=F)
    {0x0, 0x0, 0xF, 0xF}, // Blue - J (R=0, G=0, B=F, A=F)
    {0xF, 0xA, 0x0, 0xF}, // Orange - L (R=F, G=A, B=0, A=F)
    {0xF, 0xF, 0x0, 0xF}, // Yellow - O (R=F, G=F, B=0, A=F)
    {0x0, 0xF, 0x0, 0xF}, // Green - S (R=0, G=F, B=0, A=F)
    {0x8, 0x0, 0xF, 0xF}, // Purple - T (R=8, G=0, B=F, A=F)
    {0xF, 0x0, 0x0, 0xF}  // Red - Z (R=F, G=0, B=0, A=F)
}};

// Board dimensions
const int BOARD_WIDTH = 10;
const int BOARD_HEIGHT = 20;

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
        float centerX = rotationCenters[type].first;
        float centerY = rotationCenters[type].second;
        int relX = j - centerX;
        int relY = i - centerY;
        int rotatedX, rotatedY;

        switch (rotation) {
            case 0: rotatedX = relX; rotatedY = relY; break;
            case 1: rotatedX = -relY; rotatedY = relX; break;
            case 2: rotatedX = -relX; rotatedY = -relY; break;
            case 3: rotatedX = relY; rotatedY = -relX; break;
        }

        int finalX = static_cast<int>(round(rotatedX + centerX));
        int finalY = static_cast<int>(round(rotatedY + centerY));

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
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            int rotatedIndex = getRotatedIndex(tet.type, i, j, tet.rotation);

            // Only check cells that contain a block
            if (tetriminoShapes[tet.type][rotatedIndex] != 0) {
                int x = tet.x + j;
                int y = tet.y + i;

                // Check if x and y are within the bounds of the board
                if (x < 0 || x >= BOARD_WIDTH || y >= BOARD_HEIGHT) {
                    return false;  // Invalid if out of bounds
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
    float fadeAlpha = 0.0f;        // Alpha value for fade-in/fade-out
    bool showText = false;         // Flag to control when to show the text
    int clearedLinesYPosition = 0; // Y-position of cleared lines to center text
    std::chrono::time_point<std::chrono::steady_clock> textStartTime;

    TetrisElement(u16 w, u16 h, std::array<std::array<int, BOARD_WIDTH>, BOARD_HEIGHT> *board, Tetrimino *current, Tetrimino *next, Tetrimino *stored)
        : board(board), currentTetrimino(current), nextTetrimino(next), storedTetrimino(stored), _w(w), _h(h) {}

    virtual void draw(tsl::gfx::Renderer* renderer) override {
        // Center the board in the frame
        int boardWidthInPixels = BOARD_WIDTH * _w;
        int boardHeightInPixels = BOARD_HEIGHT * _h;
        int offsetX = (this->getWidth() - boardWidthInPixels) / 2;
        int offsetY = (this->getHeight() - boardHeightInPixels) / 2;


        // Define the semi-transparent black background color
        tsl::Color overlayColor = tsl::Color({0x0, 0x0, 0x0, 0xA}); // Semi-transparent black color
        
        // Draw the black background rectangle (slightly larger than the frame)
        int backgroundPadding = 4; // Padding around the frame for the black background
        renderer->drawRect(offsetX - backgroundPadding, offsetY - backgroundPadding,
                           boardWidthInPixels + 2 * backgroundPadding, boardHeightInPixels + 2 * backgroundPadding, a(overlayColor));


        // Draw the board frame
        tsl::Color frameColor = tsl::Color({0xF, 0xF, 0xF, 0xF}); // White color for frame
        int frameThickness = 2;
    
        // Top line
        renderer->drawRect(offsetX - frameThickness, offsetY - frameThickness, BOARD_WIDTH * _w + 2 * frameThickness, frameThickness, frameColor);
        // Bottom line
        renderer->drawRect(offsetX - frameThickness, offsetY + BOARD_HEIGHT * _h, BOARD_WIDTH * _w + 2 * frameThickness, frameThickness, frameColor);
        // Left line
        renderer->drawRect(offsetX - frameThickness, offsetY - frameThickness, frameThickness, BOARD_HEIGHT * _h + 2 * frameThickness, frameColor);
        // Right line
        renderer->drawRect(offsetX + BOARD_WIDTH * _w, offsetY - frameThickness, frameThickness, BOARD_HEIGHT * _h + 2 * frameThickness, frameColor);



        // Draw the board
        int drawX, drawY;
        tsl::Color innerColor(0), outerColor(0);

        for (int y = 0; y < BOARD_HEIGHT; ++y) {
            for (int x = 0; x < BOARD_WIDTH; ++x) {
                if ((*board)[y][x] != 0) {
                    drawX = offsetX + x * _w;
                    drawY = offsetY + y * _h;
        
                    // Get the color for the current block
                    outerColor = tetriminoColors[(*board)[y][x] - 1];
        
                    // Draw the outer block
                    renderer->drawRect(drawX, drawY, _w, _h, outerColor);
        
                    // Calculate a darker shade for the inner block
                    innerColor = {
                        static_cast<u8>(outerColor.r * 0.7), // Darker red
                        static_cast<u8>(outerColor.g * 0.7), // Darker green
                        static_cast<u8>(outerColor.b * 0.7), // Darker blue
                        static_cast<u8>(outerColor.a) // Ensure this is within the range of 0-255
                    };

        
                    // Draw the inner block (smaller rectangle)
                    int innerPadding = 4; // Adjust this to control the inner rectangle size
                    renderer->drawRect(drawX + innerPadding, drawY + innerPadding, _w - 2 * innerPadding, _h - 2 * innerPadding, innerColor);
                }
            }
        }

        score.str(std::string());
        score << "Score\n" << getScore();
        renderer->drawString(score.str().c_str(), false, 64, 124, 20, tsl::Color({0xF, 0xF, 0xF, 0xF}));
        
        highScore.str(std::string());
        highScore << "High Score\n" << maxHighScore;
        renderer->drawString(highScore.str().c_str(), false, 268, 124, 20, tsl::Color({0xF, 0xF, 0xF, 0xF}));

        // Update and draw particles
        //tsl::Color particleColor(0);
        //int particleDrawX, particleDrawY;
        //
        //// Update and draw particles (only handle the drawing part here)
        //auto particlesCopy = particles;
        //for (const auto& particle : particlesCopy) {
        //    if (particle.life > 0 && particle.alpha > 0) {
        //        // Calculate particle position relative to the board
        //        particleDrawX = offsetX + static_cast<int>(particle.x);
        //        particleDrawY = offsetY + static_cast<int>(particle.y);
        //        
        //        // Generate a random color for each particle in RGB4444 format
        //        particleColor = tsl::Color({
        //            static_cast<u8>(rand() % 16),  // Random Red component (4 bits, 0x0 to 0xF)
        //            static_cast<u8>(rand() % 16),  // Random Green component (4 bits, 0x0 to 0xF)
        //            static_cast<u8>(rand() % 16),  // Random Blue component (4 bits, 0x0 to 0xF)
        //            static_cast<u8>(particle.alpha * 15)  // Alpha component (4 bits, scaled to 0x0 to 0xF)
        //        });
        //
        //        renderer->drawRect(particleDrawX, particleDrawY, 4, 4, particleColor);
        //    }
        //}


        // Draw the stored Tetrimino
        drawStoredTetrimino(renderer, offsetX - 61, offsetY); // Adjust the position to fit on the left side

        // Draw the next Tetrimino preview
        drawNextTetrimino(renderer, offsetX + BOARD_WIDTH * _w + 12, offsetY);
    
        // Draw the number of lines cleared
        std::ostringstream linesStr;
        linesStr << "Lines\n" << linesCleared;
        renderer->drawString(linesStr.str().c_str(), false, offsetX + BOARD_WIDTH * _w + 14, offsetY + 80, 18, tsl::Color({0xF, 0xF, 0xF, 0xF}));
        
        // Draw the current level
        std::ostringstream levelStr;
        levelStr << "Level\n" << level;
        renderer->drawString(levelStr.str().c_str(), false, offsetX + BOARD_WIDTH * _w + 14, offsetY + 80+50, 18, tsl::Color({0xF, 0xF, 0xF, 0xF}));
        

        renderer->drawString("", false, 74, offsetY + 74, 18, tsl::Color({0xF, 0xF, 0xF, 0xF}));

        std::lock_guard<std::mutex> lock(boardMutex);  // Lock the mutex while rendering
        
        // Draw the current Tetrimino
        drawTetrimino(renderer, *currentTetrimino, offsetX, offsetY);


        // Update the particles
        updateParticles(offsetX, offsetY);
        drawParticles(renderer, offsetX, offsetY);

        // Draw the lines-cleared text with smooth sine wave-based color effect for each character when the text is "Tetris"
        if (showText) {
            auto currentTime = std::chrono::steady_clock::now();
            std::chrono::duration<float, std::milli> elapsedTime = currentTime - textStartTime;
        
            // Define the durations for each phase
            float scrollInDuration = 500.0f;  // 1 seconds to scroll in
            float pauseDuration = 1000.0f;     // 1 second pause
            float scrollOutDuration = 500.0f; // 1 seconds to scroll out
            float totalDuration = scrollInDuration + pauseDuration + scrollOutDuration;
        
            // Calculate board dimensions
            int boardWidthInPixels = BOARD_WIDTH * _w;
            int boardHeightInPixels = BOARD_HEIGHT * _h;
            int offsetX = (this->getWidth() - boardWidthInPixels) / 2;  // Horizontal offset to center the board
            int offsetY = (this->getHeight() - boardHeightInPixels) / 2; // Vertical offset to center the board
        

            



        
            // Draw the text with color effects for "Tetris"
            if (linesClearedText == "Tetris") {
                // Font size and padding
                int fontSize = 24;  // Match font size used for "Lines" and "Level"
                int textWidth = renderer->calculateStringWidth(linesClearedText.c_str(), fontSize) + 16;
                int textY = offsetY + (boardHeightInPixels / 2);  // Center vertically on the board
                
                // Calculate the X position of the text based on the phase
                int textX;
                    
                if (elapsedTime.count() < scrollInDuration) {
                    // Phase 1: Slide the text from being hidden (under the left side of the gameboard) to fully visible on the left side
                    float progress = elapsedTime.count() / scrollInDuration;
                    textX = offsetX - (progress) * textWidth;  // Move left from hidden to fully visible
                } else if (elapsedTime.count() < scrollInDuration + pauseDuration) {
                    // Phase 2: Pause the text fully visible just off the left edge of the gameboard
                    textX = offsetX - textWidth;  // Fully visible, just to the left of the gameboard
                } else if (elapsedTime.count() < totalDuration) {
                    // Phase 3: Slide the text back to the right, scissored by the left edge of the gameboard
                    float progress = (elapsedTime.count() - scrollInDuration - pauseDuration) / scrollOutDuration;
                    textX = offsetX - textWidth + progress * textWidth;  // Move right, getting scissored
                } else {
                    // End the animation after the total duration
                    showText = false;
                    return;
                }
            
                // Enable scissoring to clip the text at the left edge of the gameboard
                renderer->enableScissoring(0, offsetY, offsetX, boardHeightInPixels);

                auto currentTimeCount = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
                static auto dynamicLogoRGB1 = tsl::hexToRGB444Floats("#6929ff");
                static auto dynamicLogoRGB2 = tsl::hexToRGB444Floats("#fff429");
                float countOffset = 0.0f;
        
                tsl::Color highlightColor(0);
                float counter, transitionProgress;
        
                for (char letter : linesClearedText) {
                    counter = (2 * M_PI * (fmod(currentTimeCount / 4.0, 2.0) + countOffset) / 2.0);
                    transitionProgress = std::sin(3.0 * (counter - (2.0 * M_PI / 3.0)));
        
                    highlightColor = {
                        static_cast<u8>((std::get<0>(dynamicLogoRGB2) - std::get<0>(dynamicLogoRGB1)) * (transitionProgress + 1.0) / 2.0 + std::get<0>(dynamicLogoRGB1)),
                        static_cast<u8>((std::get<1>(dynamicLogoRGB2) - std::get<1>(dynamicLogoRGB1)) * (transitionProgress + 1.0) / 2.0 + std::get<1>(dynamicLogoRGB1)),
                        static_cast<u8>((std::get<2>(dynamicLogoRGB2) - std::get<2>(dynamicLogoRGB1)) * (transitionProgress + 1.0) / 2.0 + std::get<2>(dynamicLogoRGB1)),
                        0xF  // Full opacity for the text
                    };
        
                    std::string charStr(1, letter);
                    int charWidth = renderer->calculateStringWidth(charStr.c_str(), fontSize);
        
                    renderer->drawString(charStr.c_str(), false, textX, textY, fontSize, highlightColor);
                    textX += charWidth;
                    countOffset -= 0.2f;
                }
            } else {
                // Font size and padding
                int fontSize = 20;  // Match font size used for "Lines" and "Level"
                int textWidth = renderer->calculateStringWidth(linesClearedText.c_str(), fontSize) + 16;
                int textY = offsetY + (boardHeightInPixels / 2);  // Center vertically on the board
                
                // Calculate the X position of the text based on the phase
                int textX;
                    
                if (elapsedTime.count() < scrollInDuration) {
                    // Phase 1: Slide the text from being hidden (under the left side of the gameboard) to fully visible on the left side
                    float progress = elapsedTime.count() / scrollInDuration;
                    textX = offsetX - (progress) * textWidth;  // Move left from hidden to fully visible
                } else if (elapsedTime.count() < scrollInDuration + pauseDuration) {
                    // Phase 2: Pause the text fully visible just off the left edge of the gameboard
                    textX = offsetX - textWidth;  // Fully visible, just to the left of the gameboard
                } else if (elapsedTime.count() < totalDuration) {
                    // Phase 3: Slide the text back to the right, scissored by the left edge of the gameboard
                    float progress = (elapsedTime.count() - scrollInDuration - pauseDuration) / scrollOutDuration;
                    textX = offsetX - textWidth + progress * textWidth;  // Move right, getting scissored
                } else {
                    // End the animation after the total duration
                    showText = false;
                    return;
                }
                
                // Enable scissoring to clip the text at the left edge of the gameboard
                renderer->enableScissoring(0, offsetY, offsetX, boardHeightInPixels);

                // Draw non-Tetris text in plain white
                tsl::Color textColor(0xF, 0xF, 0xF, 0xF);  // White text
                renderer->drawString(linesClearedText.c_str(), false, textX, textY, fontSize, textColor);
            }
        
            // Disable scissoring after drawing
            renderer->disableScissoring();
        }


        
        
        


        // Draw score and status text
        if (gameOver || paused) {
            // Draw a semi-transparent black overlay over the board
            renderer->drawRect(offsetX, offsetY, boardWidthInPixels, boardHeightInPixels, tsl::Color({0x0, 0x0, 0x0, 0xA}));
        
            // Calculate the center position of the board
            int centerX = offsetX + (BOARD_WIDTH * _w) / 2;
            int centerY = offsetY + (BOARD_HEIGHT * _h) / 2;
        
            if (gameOver) {
                // Set the text color to red
                tsl::Color redColor = tsl::Color({0xF, 0x0, 0x0, 0xF});
        
                // Calculate text width to center the text
                int textWidth = renderer->calculateStringWidth("Game Over", 24);
                
                // Draw "Game Over" at the center of the board
                renderer->drawString("Game Over", false, centerX - textWidth / 2, centerY, 24, redColor);
            } else if (paused) {
                // Set the text color to green
                tsl::Color greenColor = tsl::Color({0x0, 0xF, 0x0, 0xF});
        
                // Calculate text width to center the text
                int textWidth = renderer->calculateStringWidth("Paused", 24);
        
                // Draw "Paused" at the center of the board
                renderer->drawString("Paused", false, centerX - textWidth / 2, centerY, 24, greenColor);
            }
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
        tsl::Color color(0);
        tsl::Color innerColor(0);
        int rotatedIndex;
        int x, y;
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
                    
                    color = tetriminoColors[tet.type];
                    if (isGhost) {
                        // Make the ghost piece semi-transparent
                        color.a = static_cast<u8>(color.a * 0.3);  // Adjust transparency for ghost piece
                    }
                    
                    // Draw the outer block
                    renderer->drawRect(x, y, _w, _h, color);
                    
                    // Draw the inner block (darker shade)
                    innerColor = {
                        static_cast<u8>(color.r * 0.7),
                        static_cast<u8>(color.g * 0.7),
                        static_cast<u8>(color.b * 0.7),
                        static_cast<u8>(color.a)
                    };
                    renderer->drawRect(x + 4, y + 4, _w - 8, _h - 8, innerColor);
                }
            }
        }
    }


    void drawTetrimino(tsl::gfx::Renderer* renderer, const Tetrimino& tet, int offsetX, int offsetY) {
        // Calculate the drop position for the ghost piece
        Tetrimino ghostTetrimino = tet;
        int dropDistance = calculateDropDistance(ghostTetrimino, *board);
        ghostTetrimino.y += dropDistance;
        
        // Draw the ghost piece first (semi-transparent)
        drawSingleTetrimino(renderer, ghostTetrimino, offsetX, offsetY, true);  // `true` indicates ghost
        
        // Draw the active Tetrimino
        drawSingleTetrimino(renderer, tet, offsetX, offsetY, false);  // `false` indicates normal piece
    }

    // Method to draw the next Tetrimino
    void drawNextTetrimino(tsl::gfx::Renderer* renderer, int posX, int posY) {
        int borderWidth = _w * 2 + 8;  // Preview area width
        int borderHeight = _h * 2 + 8; // Preview area height
        int borderThickness = 2;       // Thickness of the border
        int padding = 2;               // Padding inside the border
    
        // Draw the frame for the next Tetrimino preview
        renderer->drawRect(posX - padding - borderThickness, posY - padding - borderThickness,
                           borderWidth + 2 * padding + 2 * borderThickness, borderHeight + 2 * padding + 2 * borderThickness, 
                           tsl::Color({0x0, 0x0, 0x0, 0xA})); // Semi-transparent background
    
        // Draw the white border
        renderer->drawRect(posX - padding, posY - padding, borderWidth + 2 * padding, borderThickness, tsl::Color({0xF, 0xF, 0xF, 0xF}));
        renderer->drawRect(posX - padding, posY + borderHeight, borderWidth + 2 * padding, borderThickness, tsl::Color({0xF, 0xF, 0xF, 0xF}));
        renderer->drawRect(posX - padding, posY - padding, borderThickness, borderHeight + 2 * padding, tsl::Color({0xF, 0xF, 0xF, 0xF}));
        renderer->drawRect(posX + borderWidth, posY - padding, borderThickness, borderHeight + 2 * padding, tsl::Color({0xF, 0xF, 0xF, 0xF}));
        
        int index;

        // Calculate the bounding box of the next Tetrimino (minX, maxX, minY, maxY)
        int minX = 4, maxX = -1, minY = 4, maxY = -1;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                index = getRotatedIndex(nextTetrimino->type, i, j, nextTetrimino->rotation);
                if (tetriminoShapes[nextTetrimino->type][index] != 0) {
                    if (j < minX) minX = j;
                    if (j > maxX) maxX = j;
                    if (i < minY) minY = i;
                    if (i > maxY) maxY = i;
                }
            }
        }
    
        // Calculate the width and height of the next Tetrimino in blocks
        float tetriminoWidth = (maxX - minX + 1) * (_w / 2);
        float tetriminoHeight = (maxY - minY + 1) * (_h / 2);
    
        // Correctly calculate the offset to center the Tetrimino
        int offsetX = std::ceil((borderWidth - tetriminoWidth) / 2. -2.);
        int offsetY = std::ceil((borderHeight - tetriminoHeight) / 2. -2.);
        
        tsl::Color outerColor(0), innerColor(0);
        
        int blockWidth, blockHeight;
        int drawX, drawY;

        // Draw the next Tetrimino within the preview area
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                index = getRotatedIndex(nextTetrimino->type, i, j, nextTetrimino->rotation);
                if (tetriminoShapes[nextTetrimino->type][index] != 0) {
                    blockWidth = _w / 2;
                    blockHeight = _h / 2;
                    drawX = posX + (j - minX) * blockWidth + padding + offsetX;
                    drawY = posY + (i - minY) * blockHeight + padding + offsetY;
    
                    // Draw the outer block of the Tetrimino
                    renderer->drawRect(drawX, drawY, blockWidth, blockHeight, tetriminoColors[nextTetrimino->type]);
    
                    // Calculate a darker shade for the inner block
                    outerColor = tetriminoColors[nextTetrimino->type];
                    innerColor = {
                        static_cast<u8>(outerColor.r * 0.7), // Darker red
                        static_cast<u8>(outerColor.g * 0.7), // Darker green
                        static_cast<u8>(outerColor.b * 0.7), // Darker blue
                        static_cast<u8>(outerColor.a)
                    };
    
                    // Draw the inner block (smaller rectangle)
                    int innerPadding = 2; // Adjust inner block size with padding
                    renderer->drawRect(drawX + innerPadding, drawY + innerPadding, blockWidth - 2 * innerPadding, blockHeight - 2 * innerPadding, innerColor);
                }
            }
        }
    }
    
    // Method to draw the stored Tetrimino (similar to drawNextTetrimino)
    void drawStoredTetrimino(tsl::gfx::Renderer* renderer, int posX, int posY) {
        int borderWidth = _w * 2 + 8;
        int borderHeight = _h * 2 + 8;
        int borderThickness = 2;
        int padding = 2;
    
        // Draw the preview frame
        renderer->drawRect(posX - padding - borderThickness, posY - padding - borderThickness,
                           borderWidth + 2 * padding + 2 * borderThickness, borderHeight + 2 * padding + 2 * borderThickness, 
                           tsl::Color({0x0, 0x0, 0x0, 0xA}));
    
        renderer->drawRect(posX - padding, posY - padding, borderWidth + 2 * padding, borderThickness, tsl::Color({0xF, 0xF, 0xF, 0xF}));
        renderer->drawRect(posX - padding, posY + borderHeight, borderWidth + 2 * padding, borderThickness, tsl::Color({0xF, 0xF, 0xF, 0xF}));
        renderer->drawRect(posX - padding, posY - padding, borderThickness, borderHeight + 2 * padding, tsl::Color({0xF, 0xF, 0xF, 0xF}));
        renderer->drawRect(posX + borderWidth, posY - padding, borderThickness, borderHeight + 2 * padding, tsl::Color({0xF, 0xF, 0xF, 0xF}));
    
        if (storedTetrimino->type != -1) {
            int index;
            int minX = 4, maxX = -1, minY = 4, maxY = -1;
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    index = getRotatedIndex(storedTetrimino->type, i, j, storedTetrimino->rotation);
                    if (tetriminoShapes[storedTetrimino->type][index] != 0) {
                        minX = std::min(minX, j);
                        maxX = std::max(maxX, j);
                        minY = std::min(minY, i);
                        maxY = std::max(maxY, i);
                    }
                }
            }
    
            float tetriminoWidth = (maxX - minX + 1) * (_w / 2);
            float tetriminoHeight = (maxY - minY + 1) * (_h / 2);
    
            int offsetX = std::ceil((borderWidth - tetriminoWidth) / 2. -2.);
            int offsetY = std::ceil((borderHeight - tetriminoHeight) / 2. -2.);
            
            tsl::Color outerColor(0), innerColor(0);

            int blockWidth, blockHeight;
            int drawX, drawY;

            // Draw the stored Tetrimino in the preview area
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    index = getRotatedIndex(storedTetrimino->type, i, j, storedTetrimino->rotation);
                    if (tetriminoShapes[storedTetrimino->type][index] != 0) {
                        blockWidth = _w / 2;
                        blockHeight = _h / 2;
                        drawX = posX + (j - minX) * blockWidth + padding + offsetX;
                        drawY = posY + (i - minY) * blockHeight + padding + offsetY;
                        
                        renderer->drawRect(drawX, drawY, blockWidth, blockHeight, tetriminoColors[storedTetrimino->type]);
                        
                        // Inner block shading
                        outerColor = tetriminoColors[storedTetrimino->type];
                        innerColor = {
                            static_cast<u8>(outerColor.r * 0.7),
                            static_cast<u8>(outerColor.g * 0.7),
                            static_cast<u8>(outerColor.b * 0.7),
                            static_cast<u8>(outerColor.a)
                        };
                        
                        int innerPadding = 2;
                        renderer->drawRect(drawX + innerPadding, drawY + innerPadding, blockWidth - 2 * innerPadding, blockHeight - 2 * innerPadding, innerColor);
                    }
                }
            }
        }
    }
    
    

    
};

bool TetrisElement::paused = false;
uint64_t TetrisElement::maxHighScore = 0; // Initialize the max high score


class CustomOverlayFrame : public tsl::elm::OverlayFrame {
public:
    CustomOverlayFrame(const std::string& title, const std::string& subtitle, const std::string& menuMode = "", const std::string& colorSelection = "", const std::string& pageLeftName = "", const std::string& pageRightName = "", const bool& _noClickableItems = false)
        : tsl::elm::OverlayFrame(title, subtitle, menuMode, colorSelection, pageLeftName, pageRightName, _noClickableItems) {}

    // Override the draw method to customize rendering logic for Tetris
    virtual void draw(tsl::gfx::Renderer* renderer) override {
        if (m_noClickableItems != noClickableItems)
            noClickableItems = m_noClickableItems;
        renderer->fillScreen(a(tsl::defaultBackgroundColor));
        
        if (expandedMemory && !refreshWallpaper.load(std::memory_order_acquire)) {
            inPlot.store(true, std::memory_order_release);
            if (!wallpaperData.empty()) {
                // Draw the bitmap at position (0, 0) on the screen
                if (!refreshWallpaper.load(std::memory_order_acquire))
                    renderer->drawBitmap(0, 0, 448, 720, wallpaperData.data());
                else
                    inPlot.store(false, std::memory_order_release);
            } else {
                inPlot.store(false, std::memory_order_release);
            }
        }
        

        if (touchingMenu && inMainMenu) {
            renderer->drawRoundedRect(0.0f, 12.0f, 245.0f, 73.0f, 6.0f, a(tsl::clickColor));
        }
        
        
        x = 20;
        y = 62;
        fontSize = 54;
        offset = 6;
        countOffset = 0;
        

        if (!tsl::disableColorfulLogo) {
            auto currentTimeCount = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
            float progress;
            static auto dynamicLogoRGB1 = tsl::hexToRGB444Floats("#6929ff");
            static auto dynamicLogoRGB2 = tsl::hexToRGB444Floats("#fff429");
            tsl::Color highlightColor(0);
            for (char letter : m_title) {
                counter = (2 * M_PI * (fmod(currentTimeCount/4.0, 2.0) + countOffset) / 2.0);
                progress = std::sin(3.0 * (counter - (2.0 * M_PI / 3.0))); // Faster transition from -1 to 1 and back in the remaining 1/3
                
                highlightColor = {
                    static_cast<u8>((std::get<0>(dynamicLogoRGB2) - std::get<0>(dynamicLogoRGB1)) * (progress + 1.0) / 2.0 + std::get<0>(dynamicLogoRGB1)),
                    static_cast<u8>((std::get<1>(dynamicLogoRGB2) - std::get<1>(dynamicLogoRGB1)) * (progress + 1.0) / 2.0 + std::get<1>(dynamicLogoRGB1)),
                    static_cast<u8>((std::get<2>(dynamicLogoRGB2) - std::get<2>(dynamicLogoRGB1)) * (progress + 1.0) / 2.0 + std::get<2>(dynamicLogoRGB1)),
                    15
                };
                
                renderer->drawString(std::string(1, letter), false, x, y + offset, fontSize, a(highlightColor));
                x += renderer->calculateStringWidth(std::string(1, letter), fontSize);
                countOffset -= 0.2F;
            }
        } else {
            for (char letter : m_title) {
                renderer->drawString(std::string(1, letter), false, x, y + offset, fontSize, a(tsl::logoColor1));
                x += renderer->calculateStringWidth(std::string(1, letter), fontSize);
                countOffset -= 0.2F;
            }
        }
        
        
        if (!(hideBattery && hidePCBTemp && hideSOCTemp && hideClock)) {
            renderer->drawRect(245, 23, 1, 49, a(tsl::separatorColor));
        }
        
        
        y_offset = 45;
        if ((hideBattery && hidePCBTemp && hideSOCTemp) || hideClock) {
            y_offset += 10;
        }
        
        clock_gettime(CLOCK_REALTIME, &currentTime);
        if (!hideClock) {
            static char timeStr[20]; // Allocate a buffer to store the time string
            strftime(timeStr, sizeof(timeStr), datetimeFormat.c_str(), localtime(&currentTime.tv_sec));
            localizeTimeStr(timeStr);
            renderer->drawString(timeStr, false, tsl::cfg::FramebufferWidth - renderer->calculateStringWidth(timeStr, 20, true) - 20, y_offset, 20, a(tsl::clockColor));
            y_offset += 22;
        }
        

        static char PCB_temperatureStr[10];
        static char SOC_temperatureStr[10];


        size_t statusChange = size_t(hideSOCTemp) + size_t(hidePCBTemp) + size_t(hideBattery);
        static size_t lastStatusChange = 0;

        if ((currentTime.tv_sec - timeOut) >= 1 || statusChange != lastStatusChange) {
            if (!hideSOCTemp) {
                ReadSocTemperature(&SOC_temperature);
                snprintf(SOC_temperatureStr, sizeof(SOC_temperatureStr) - 1, "%d°C", SOC_temperature);
            } else {
                strcpy(SOC_temperatureStr, "");
                SOC_temperature=0;
            }
            if (!hidePCBTemp) {
                ReadPcbTemperature(&PCB_temperature);
                snprintf(PCB_temperatureStr, sizeof(PCB_temperatureStr) - 1, "%d°C", PCB_temperature);
            } else {
                strcpy(PCB_temperatureStr, "");
                PCB_temperature=0;
            }
            if (!hideBattery) {
                powerGetDetails(&batteryCharge, &isCharging);
                batteryCharge = std::min(batteryCharge, 100U);
                sprintf(chargeString, "%d%%", batteryCharge);
            } else {
                strcpy(chargeString, "");
                batteryCharge=0;
            }
            timeOut = int(currentTime.tv_sec);
        }

        lastStatusChange = statusChange;
        
        if (!hideBattery && batteryCharge > 0) {
            tsl::Color batteryColorToUse = isCharging ? tsl::Color(0x0, 0xF, 0x0, 0xF) : 
                                    (batteryCharge < 20 ? tsl::Color(0xF, 0x0, 0x0, 0xF) : tsl::batteryColor);
            renderer->drawString(chargeString, false, tsl::cfg::FramebufferWidth - renderer->calculateStringWidth(chargeString, 20, true) - 22, y_offset, 20, a(batteryColorToUse));
        }
        
        offset = 0;
        if (!hidePCBTemp && PCB_temperature > 0) {
            if (!hideBattery)
                offset -= 5;
            renderer->drawString(PCB_temperatureStr, false, tsl::cfg::FramebufferWidth + offset - renderer->calculateStringWidth(PCB_temperatureStr, 20, true) - renderer->calculateStringWidth(chargeString, 20, true) - 22, y_offset, 20, a(tsl::GradientColor(PCB_temperature)));
        }
        
        if (!hideSOCTemp && SOC_temperature > 0) {
            if (!hidePCBTemp || !hideBattery)
                offset -= 5;
            renderer->drawString(SOC_temperatureStr, false, tsl::cfg::FramebufferWidth + offset - renderer->calculateStringWidth(SOC_temperatureStr, 20, true) - renderer->calculateStringWidth(PCB_temperatureStr, 20, true) - renderer->calculateStringWidth(chargeString, 20, true) - 22, y_offset, 20, a(tsl::GradientColor(SOC_temperature)));
        }

        renderer->drawString(this->m_subtitle, false, 184, y-8, 15, a(tsl::versionTextColor));
        
        renderer->drawRect(15, tsl::cfg::FramebufferHeight - 73, tsl::cfg::FramebufferWidth - 30, 1, a(tsl::botttomSeparatorColor));
        

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

        backWidth = renderer->calculateStringWidth(bCommand, 23);
        if (touchingBack) {
            renderer->drawRoundedRect(18.0f, static_cast<float>(tsl::cfg::FramebufferHeight - 73), 
                                      backWidth+68.0f, 73.0f, 6.0f, a(tsl::clickColor));
        }

        selectWidth = renderer->calculateStringWidth(aCommand, 23);
        if (touchingSelect && !m_noClickableItems) {
            renderer->drawRoundedRect(18.0f + backWidth+68.0f, static_cast<float>(tsl::cfg::FramebufferHeight - 73), 
                                      selectWidth+68.0f, 73.0f, 6.0f, a(tsl::clickColor));
        }
        
        if (!(this->m_pageLeftName).empty())
            nextPageWidth = renderer->calculateStringWidth(this->m_pageLeftName, 23);
        else if (!(this->m_pageRightName).empty())
            nextPageWidth = renderer->calculateStringWidth(this->m_pageRightName, 23);
        else if (inMainMenu)
            if (inOverlaysPage)
                nextPageWidth = renderer->calculateStringWidth(PACKAGES,23);
            else if (inPackagesPage)
                nextPageWidth = renderer->calculateStringWidth(OVERLAYS,23);

        if (inMainMenu || !(this->m_pageLeftName).empty() || !(this->m_pageRightName).empty()) {
            if (touchingNextPage) {
                renderer->drawRoundedRect(18.0f + backWidth+68.0f + ((!m_noClickableItems) ? selectWidth+68.0f : 0), static_cast<float>(tsl::cfg::FramebufferHeight - 73), 
                                          nextPageWidth+70.0f, 73.0f, 6.0f, a(tsl::clickColor));
            }
        }


        if (m_noClickableItems)
            menuBottomLine = "\uE0E1"+GAP_2+bCommand+GAP_1;
        else
            menuBottomLine = "\uE0E1"+GAP_2+bCommand+GAP_1+"\uE0E0"+GAP_2+aCommand+GAP_1;

        if (this->m_menuMode == "packages") {
            menuBottomLine += "\uE0ED"+GAP_2+OVERLAYS;
        } else if (this->m_menuMode == "overlays") {
            menuBottomLine += "\uE0EE"+GAP_2+PACKAGES;
        }
        
        if (!(this->m_pageLeftName).empty()) {
            menuBottomLine += "\uE0ED"+GAP_2 + this->m_pageLeftName;
        } else if (!(this->m_pageRightName).empty()) {
            menuBottomLine += "\uE0EE"+GAP_2 + this->m_pageRightName;
        }
        
        
        // Render the text with special character handling
        renderer->drawStringWithColoredSections(menuBottomLine, {"\uE0E1","\uE0E0","\uE0ED","\uE0EE"}, 30, 693, 23, a(tsl::bottomTextColor), a(tsl::buttonColor));

        
        if (this->m_contentElement != nullptr)
            this->m_contentElement->frame(renderer);
    }
};


class TetrisGui : public tsl::Gui {
public:
    Tetrimino storedTetrimino{-1}; // -1 indicates no stored Tetrimino
    bool hasSwapped = false; // To track if a swap has already occurred

    int linesClearedForLevelUp = 0;  // Track how many lines cleared for leveling up
    const int LINES_PER_LEVEL = 10;  // Increment level every 10 lines

    // Variables to track time of last rotation or movement
    std::chrono::time_point<std::chrono::steady_clock> lastRotationOrMoveTime;
    const std::chrono::milliseconds lockDelayExtension = std::chrono::milliseconds(500); // 500ms extension

    TetrisGui() : board(), currentTetrimino(rand() % 7), nextTetrimino(rand() % 7) {

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

    virtual tsl::elm::Element* createUI() override {
        //auto rootFrame = new tsl::elm::OverlayFrame("Tetris", APP_VERSION);
        auto rootFrame = new CustomOverlayFrame("Tetris", APP_VERSION);
        tetrisElement = new TetrisElement(_w, _h, &board, &currentTetrimino, &nextTetrimino, &storedTetrimino); // Pass storedTetrimino
        rootFrame->setContent(tetrisElement);
        timeSinceLastFrame = std::chrono::steady_clock::now();
    
        loadGameState();
        return rootFrame;
    }





    virtual void update() override {
        if (!TetrisElement::paused && !tetrisElement->gameOver) {
            auto currentTime = std::chrono::steady_clock::now();
            auto elapsed = currentTime - timeSinceLastFrame;

            // Handle piece falling
            fallCounter += std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
            if (fallCounter >= getFallSpeed()) {
                // Try to move the piece down
                if (!move(0, 1)) { // Move down failed, piece touched the ground
                    lockDelayCounter += fallCounter; // Add elapsed time to lock delay counter

                    // Check if more than 500ms has passed since the last move/rotation
                    auto timeSinceLastRotationOrMove = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastRotationOrMoveTime);

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
        // Clear the board
        for (auto& row : board) {
            row.fill(0);
        }
        
        // Reset tetriminos
        currentTetrimino = Tetrimino(rand() % 7);
        nextTetrimino = Tetrimino(rand() % 7);
        
        // Reset the stored tetrimino
        storedTetrimino = Tetrimino(-1); // Reset stored piece to no stored state
        hasSwapped = false; // Reset swap flag
        
        // Reset score
        tetrisElement->setScore(0);
        
        // Reset linesCleared and level
        tetrisElement->setLinesCleared(0); // Reset lines cleared
        tetrisElement->setLevel(1); // Reset level to 1
        
        // Reset fall speed to initial state
        //adjustFallSpeed();
        
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

    

    void hardDrop() {
        // Calculate how far the piece will fall
        hardDropDistance = calculateDropDistance(currentTetrimino, board);
        currentTetrimino.y += hardDropDistance;
        
        // Award points for hard drop (e.g., 2 points per row)
        int hardDropScore = hardDropDistance * 2;
        tetrisElement->setScore(tetrisElement->getScore() + hardDropScore);
    
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
        json_t* root = json_object();
    
        // Add score, maxHighScore, paused state, and gameOver state
        json_object_set_new(root, "score", json_string(std::to_string(tetrisElement->getScore()).c_str()));
        json_object_set_new(root, "maxHighScore", json_string(std::to_string(TetrisElement::maxHighScore).c_str()));
        json_object_set_new(root, "paused", json_boolean(TetrisElement::paused));
        json_object_set_new(root, "gameOver", json_boolean(tetrisElement->gameOver));  // Add this line
        json_object_set_new(root, "linesCleared", json_integer(tetrisElement->getLinesCleared())); // Use getter
        json_object_set_new(root, "level", json_integer(tetrisElement->getLevel())); // Use getter
        json_object_set_new(root, "hasSwapped", json_boolean(hasSwapped)); // Save hasSwapped state

        // Add current tetrimino state
        json_t* currentTetriminoJson = json_object();
        json_object_set_new(currentTetriminoJson, "type", json_integer(currentTetrimino.type));
        json_object_set_new(currentTetriminoJson, "rotation", json_integer(currentTetrimino.rotation));
        json_object_set_new(currentTetriminoJson, "x", json_integer(currentTetrimino.x));
        json_object_set_new(currentTetriminoJson, "y", json_integer(currentTetrimino.y));
        json_object_set_new(root, "currentTetrimino", currentTetriminoJson);


        json_t* storedTetriminoJson = json_object();
        json_object_set_new(storedTetriminoJson, "type", json_integer(storedTetrimino.type));
        json_object_set_new(storedTetriminoJson, "rotation", json_integer(storedTetrimino.rotation));
        json_object_set_new(storedTetriminoJson, "x", json_integer(storedTetrimino.x));
        json_object_set_new(storedTetriminoJson, "y", json_integer(storedTetrimino.y));
        json_object_set_new(root, "storedTetrimino", storedTetriminoJson);

        // Add next tetrimino state
        json_t* nextTetriminoJson = json_object();
        json_object_set_new(nextTetriminoJson, "type", json_integer(nextTetrimino.type));
        json_object_set_new(root, "nextTetrimino", nextTetriminoJson);
    
        // Add board state
        json_t* boardJson = json_array();
        for (int i = 0; i < BOARD_HEIGHT; ++i) {
            json_t* rowJson = json_array();
            for (int j = 0; j < BOARD_WIDTH; ++j) {
                json_array_append_new(rowJson, json_integer(board[i][j]));
            }
            json_array_append_new(boardJson, rowJson);
        }
        json_object_set_new(root, "board", boardJson);
    
        // Write JSON to file
        std::ofstream file("sdmc:/config/tetris/save_state.json");
        if (file.is_open()) {
            char* jsonString = json_dumps(root, JSON_INDENT(4)); // Convert JSON to string
            file << jsonString; // Write to file
            file.close();
            free(jsonString); // Free the string allocated by json_dumps
        }
    
        // Decrease the reference count of the JSON object to avoid memory leaks
        json_decref(root);
    }


    void loadGameState() {
        json_t* root = readJsonFromFile("sdmc:/config/tetris/save_state.json");
        if (!root) {
            // Failed to load or parse JSON, return
            return;
        }

        const char* scoreStr = json_string_value(json_object_get(root, "score"));
        const char* maxHighScoreStr = json_string_value(json_object_get(root, "maxHighScore"));
        
        if (scoreStr) {
            tetrisElement->setScore(std::stoull(scoreStr)); // Convert string to uint64_t
        }
        
        if (maxHighScoreStr) {
            TetrisElement::maxHighScore = std::stoull(maxHighScoreStr); // Convert string to uint64_t
        }
        TetrisElement::paused = json_is_true(json_object_get(root, "paused"));
        tetrisElement->gameOver = json_is_true(json_object_get(root, "gameOver"));  // Add this line
        tetrisElement->setLinesCleared(json_integer_value(json_object_get(root, "linesCleared"))); // Use setter
        tetrisElement->setLevel(json_integer_value(json_object_get(root, "level"))); // Use setter
        
        // Load hasSwapped state
        if (json_is_boolean(json_object_get(root, "hasSwapped"))) {
            hasSwapped = json_is_true(json_object_get(root, "hasSwapped"));
        } else {
            // If the value doesn't exist in the save file, set it to false
            hasSwapped = false;
        }

        // Load current tetrimino state
        json_t* currentTetriminoJson = json_object_get(root, "currentTetrimino");
        currentTetrimino.type = json_integer_value(json_object_get(currentTetriminoJson, "type"));
        currentTetrimino.rotation = json_integer_value(json_object_get(currentTetriminoJson, "rotation"));
        currentTetrimino.x = json_integer_value(json_object_get(currentTetriminoJson, "x"));
        currentTetrimino.y = json_integer_value(json_object_get(currentTetriminoJson, "y"));
        
        // Load next tetrimino state
        json_t* nextTetriminoJson = json_object_get(root, "nextTetrimino");
        nextTetrimino.type = json_integer_value(json_object_get(nextTetriminoJson, "type"));
        
        // Load stored tetrimino state
        json_t* storedTetriminoJson = json_object_get(root, "storedTetrimino");
        if (storedTetriminoJson) { // Check if the stored Tetrimino state exists in the saved file
            storedTetrimino.type = json_integer_value(json_object_get(storedTetriminoJson, "type"));
            storedTetrimino.rotation = json_integer_value(json_object_get(storedTetriminoJson, "rotation"));
            storedTetrimino.x = json_integer_value(json_object_get(storedTetriminoJson, "x"));
            storedTetrimino.y = json_integer_value(json_object_get(storedTetriminoJson, "y"));
        } else {
            // If not present in the saved state, set it to -1 to indicate no stored piece
            storedTetrimino = Tetrimino(-1);
        }

        // Load board state
        json_t* boardJson = json_object_get(root, "board");
        if (json_is_array(boardJson)) {
            for (int i = 0; i < BOARD_HEIGHT; ++i) {
                json_t* rowJson = json_array_get(boardJson, i);
                if (json_is_array(rowJson)) {
                    for (int j = 0; j < BOARD_WIDTH; ++j) {
                        board[i][j] = json_integer_value(json_array_get(rowJson, j));
                    }
                }
            }
        }
        
        // Decrease the reference count of the JSON object
        json_decref(root);
    }


    // Define constants for DAS (Delayed Auto-Shift) and ARR (Auto-Repeat Rate)
    const int DAS = 300;  // DAS delay in milliseconds
    const int ARR = 40;   // ARR interval in milliseconds
    
    // Variables to track key hold states and timing
    std::chrono::time_point<std::chrono::steady_clock> lastLeftMove, lastRightMove, lastDownMove;
    bool leftHeld = false, rightHeld = false, downHeld = false;
    bool leftARR = false, rightARR = false, downARR = false;
    
    bool handleInput(u64 keysDown, u64 keysHeld, touchPosition touchInput, JoystickPosition leftJoyStick, JoystickPosition rightJoyStick) override {
        auto currentTime = std::chrono::steady_clock::now();
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
                TetrisElement::paused = false;
            }
            // Allow closing the overlay with KEY_B only when paused or game over
            if (keysDown & KEY_B) {
                saveGameState();
                tsl::Overlay::get()->close();
            }
    
            // Return true to indicate input was handled
            return true;
        }
    
        // Handle swapping with the stored Tetrimino
        if (keysDown & KEY_L && !hasSwapped) {
            swapStoredTetrimino();
            hasSwapped = true;
        }
    
        // Handle left movement with DAS and ARR
        if (keysHeld & KEY_LEFT) {
            if (!leftHeld) {
                // First press
                moved = move(-1, 0);
                lastLeftMove = currentTime;
                leftHeld = true;
                leftARR = false; // Reset ARR phase
            } else {
                // DAS check
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastLeftMove).count();
                if (!leftARR && elapsed >= DAS) {
                    // Once DAS is reached, start ARR
                    moved = move(-1, 0);
                    lastLeftMove = currentTime; // Reset time for ARR phase
                    leftARR = true;
                } else if (leftARR && elapsed >= ARR) {
                    // Auto-repeat after ARR interval
                    moved = move(-1, 0);
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
                lastRightMove = currentTime;
                rightHeld = true;
                rightARR = false; // Reset ARR phase
            } else {
                // DAS check
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastRightMove).count();
                if (!rightARR && elapsed >= DAS) {
                    // Once DAS is reached, start ARR
                    moved = move(1, 0);
                    lastRightMove = currentTime;
                    rightARR = true;
                } else if (rightARR && elapsed >= ARR) {
                    // Auto-repeat after ARR interval
                    moved = move(1, 0);
                    lastRightMove = currentTime; // Keep resetting for ARR
                }
            }
        } else {
            rightHeld = false;
        }
    
        // Handle down movement with DAS and ARR for soft dropping
        if (keysHeld & KEY_DOWN) {
            if (!downHeld) {
                // First press
                moved = move(0, 1);
                lastDownMove = currentTime;
                downHeld = true;
                downARR = false; // Reset ARR phase
            } else {
                // DAS check
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastDownMove).count();
                if (!downARR && elapsed >= DAS) {
                    // Once DAS is reached, start ARR
                    moved = move(0, 1);
                    lastDownMove = currentTime;
                    downARR = true;
                } else if (downARR && elapsed >= ARR) {
                    // Auto-repeat after ARR interval
                    moved = move(0, 1);
                    lastDownMove = currentTime;
                }
            }
        } else {
            downHeld = false;
        }
    
        // Handle hard drop with the Up key
        if (keysDown & KEY_UP) {
            hardDrop();  // Perform hard drop immediately
        }
    
        // Handle rotation inputs
        if (keysDown & KEY_A) {
            rotate(); // Rotate clockwise
            moved = true;
        } else if (keysDown & KEY_B) {
            rotateCounterclockwise(); // Rotate counterclockwise
            moved = true;
        }
    
        // Handle pause/unpause
        if (keysDown & KEY_PLUS) {
            TetrisElement::paused = !TetrisElement::paused;
        }
    
        // Reset the lock delay timer if the piece has moved or rotated
        if (moved) {
            lockDelayCounter = std::chrono::milliseconds(0);
        }
    
        return false;
    }
    
    

private:
    std::array<std::array<int, BOARD_WIDTH>, BOARD_HEIGHT> board{};
    Tetrimino currentTetrimino;
    Tetrimino nextTetrimino;
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


    // Function to adjust the fall speed based on the current level
    //void adjustFallSpeed() {
    //    int minSpeed = 200; // Minimum fall speed (200 ms)
    //    int speedDecrease = tetrisElement->getLevel() * 50; // Decrease fall time by 50ms per level
    //    fallSpeed = std::max(initialFallSpeed - std::chrono::milliseconds(speedDecrease), std::chrono::milliseconds(minSpeed));
    //}

    // Function to dynamically calculate fall speed based on the current level
    std::chrono::milliseconds getFallSpeed() {
        // Define the fall speeds in milliseconds based on levels (simulating classic Tetris)
        const std::array<int, 30> fallSpeeds = {
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
        int level = std::min(tetrisElement->getLevel(), static_cast<int>(fallSpeeds.size() - 1));
        
        // Set a minimum threshold for fall speed to avoid it becoming too fast
        int fallSpeed = std::max(fallSpeeds[level], 16); // Minimum 16ms
        
        return std::chrono::milliseconds(fallSpeed);
    }


    bool move(int dx, int dy) {
        std::lock_guard<std::mutex> lock(boardMutex);  // Lock to prevent race conditions
        bool success = false;
        currentTetrimino.x += dx;
        currentTetrimino.y += dy;

        if (!isPositionValid(currentTetrimino, board)) {
            currentTetrimino.x -= dx;
            currentTetrimino.y -= dy;
        } else {
            success = true;

            if (dy > 0) {
                // Accumulate points for soft drops (1 point per row)
                totalSoftDropDistance += dy;
            } else if (dx != 0) {
                // Reset lock delay if there was a horizontal movement
                lockDelayCounter = std::chrono::milliseconds(0);
                lastRotationOrMoveTime = std::chrono::steady_clock::now();  // Update the last move time
            }
        }

        return success;
    }


    
    void rotate() {
        rotatePiece(-1); // Clockwise rotation
    }

    // New method to rotate counterclockwise
    void rotateCounterclockwise() {
        rotatePiece(1); // Counterclockwise rotation
    }

    bool tSpinOccurred = false; // Add this member to TetrisGui class
    
    void rotatePiece(int direction) {
        std::lock_guard<std::mutex> lock(boardMutex);  // Lock the board for safe rotation
    
        int previousRotation = currentTetrimino.rotation;
        int previousX = currentTetrimino.x;
        int previousY = currentTetrimino.y;
    
        // Update rotation
        currentTetrimino.rotation = (currentTetrimino.rotation + direction + 4) % 4;
    
        const auto& kicks = (currentTetrimino.type == 0) ? wallKicksI : wallKicksJLSTZ;
        
        int kickIndex;
        // Try all the wall kick possibilities
        for (int i = 0; i < 5; ++i) {
            kickIndex = (direction > 0) ? previousRotation : currentTetrimino.rotation;
            const auto& kick = kicks[kickIndex][i];
    
            currentTetrimino.x = previousX + kick.first;
            currentTetrimino.y = previousY + kick.second;
    
            if (isPositionValid(currentTetrimino, board)) {
                // Reset lock delay to prevent immediate locking after rotation
                lockDelayCounter = std::chrono::milliseconds(0);
                lastRotationOrMoveTime = std::chrono::steady_clock::now();
                return;
            }
        }
    
        // Revert if no valid rotation was found
        currentTetrimino.rotation = previousRotation;
        currentTetrimino.x = previousX;
        currentTetrimino.y = previousY;
    }






    bool isTSpin() {
        if (currentTetrimino.type != 5) return false; // Only T piece can T-Spin
        // Check corners around the T piece center
        int centerX = currentTetrimino.x + 1;
        int centerY = currentTetrimino.y + 1;
        int blockedCorners = 0;
    
        // Check four corners
        if (!isWithinBounds(centerX - 1, centerY - 1) || board[centerY - 1][centerX - 1] != 0) blockedCorners++;
        if (!isWithinBounds(centerX + 1, centerY - 1) || board[centerY - 1][centerX + 1] != 0) blockedCorners++;
        if (!isWithinBounds(centerX - 1, centerY + 1) || board[centerY + 1][centerX - 1] != 0) blockedCorners++;
        if (!isWithinBounds(centerX + 1, centerY + 1) || board[centerY + 1][centerX + 1] != 0) blockedCorners++;
    
        return blockedCorners >= 3; // T-Spin if 3 or more corners are blocked
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
    
        // If any part of the piece was above the top of the board, trigger game over
        if (pieceAboveTop) {
            tetrisElement->gameOver = true;
            return;  // Early return to prevent further processing
        }
    
        // Award points for soft drops (apply accumulated points)
        if (totalSoftDropDistance > 0) {
            int softDropScore = totalSoftDropDistance * 1;  // 1 point per row for soft drops
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
        int centerRow = BOARD_HEIGHT / 2;
    
        // Generate particles at the center row
        for (int x = 0; x < BOARD_WIDTH; ++x) {
            for (int p = 0; p < 10; ++p) {
                particles.push_back(Particle{
                    static_cast<float>(x * _w + _w / 2),  // X position in the center row
                    static_cast<float>(centerRow * _h + _h / 2),  // Y position in the center row
                    (rand() % 100 / 50.0f - 1.0f) * 8,  // Random velocity in X direction
                    (rand() % 100 / 50.0f - 1.0f) * 8,  // Random velocity in Y direction
                    0.5f,  // Lifespan
                    1.0f   // Initial alpha (fully visible)
                });
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
    
            // Update score based on how many lines were cleared
            int scoreMultiplier = 0;
            switch (linesClearedInThisTurn) {
                case 1: scoreMultiplier = 100; break;
                case 2: scoreMultiplier = 300; break;
                case 3: scoreMultiplier = 500; break;
                case 4: scoreMultiplier = 800; break;
            }
            int newScore = scoreMultiplier * tetrisElement->getLevel();
            tetrisElement->setScore(tetrisElement->getScore() + newScore);
    
            // Level up after clearing a certain number of lines
            linesClearedForLevelUp += linesClearedInThisTurn;
            if (linesClearedForLevelUp >= LINES_PER_LEVEL) {
                linesClearedForLevelUp -= LINES_PER_LEVEL;  // Reset the count for the next level
                tetrisElement->setLevel(tetrisElement->getLevel() + 1);  // Increase the level
            }
    
            // Show feedback text based on the number of lines cleared
            switch (linesClearedInThisTurn) {
                case 1: tetrisElement->linesClearedText = "Single"; break;
                case 2: tetrisElement->linesClearedText = "Double"; break;
                case 3: tetrisElement->linesClearedText = "Triple"; break;
                case 4: tetrisElement->linesClearedText = "Tetris"; break;
            }
    
            tetrisElement->showText = true;
            tetrisElement->fadeAlpha = 0.0f;  // Start fade animation
            tetrisElement->textStartTime = std::chrono::steady_clock::now();  // Track animation start time
        }
    }
    

    void spawnNewTetrimino() {
        currentTetrimino = Tetrimino(nextTetrimino.type);
        currentTetrimino.x = BOARD_WIDTH / 2 - 2;
        
        int rotatedIndex;

        // Calculate the topmost row with a block to adjust the starting Y position
        int topmostRow = 4; // Start with the assumption that the piece may be 4 rows high
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                rotatedIndex = getRotatedIndex(currentTetrimino.type, i, j, currentTetrimino.rotation);
                if (tetriminoShapes[currentTetrimino.type][rotatedIndex] != 0) {
                    topmostRow = std::min(topmostRow, i); // Find the topmost row with a block
                    break; // No need to continue checking this row
                }
            }
        }
    
        // Set the initial Y position, adjusted for the topmost block
        currentTetrimino.y = -topmostRow; // Allow the piece to start partially off-screen if necessary
    
        // Check if the new Tetrimino is in a valid position
        if (!isPositionValid(currentTetrimino, board)) {
            // Game over: the new Tetrimino can't be placed
            tetrisElement->gameOver = true;
        } else {
            nextTetrimino = Tetrimino(rand() % 7); // Prepare the next piece
        }
    }




};

class Overlay : public tsl::Overlay {
public:

    virtual void initServices() override {
        fsdevMountSdmc();
        splInitialize();
        spsmInitialize();
        i2cInitialize();
        ASSERT_FATAL(socketInitializeDefault());
        ASSERT_FATAL(nifmInitialize(NifmServiceType_User));
        ASSERT_FATAL(smInitialize());

        if (isFileOrDirectory("sdmc:/config/tetris/theme.ini"))
            THEME_CONFIG_INI_PATH = "sdmc:/config/tetris/theme.ini"; // Override theme path (optional)
        if (isFileOrDirectory("sdmc:/config/tetris/wallpaper.rgba"))
            WALLPAPER_PATH = "sdmc:/config/tetris/wallpaper.rgba"; // Overrride wallpaper path (optional)

        tsl::initializeThemeVars(); // for ultrahand themes
        tsl::initializeUltrahandSettings(); // for opaque screenshots and swipe to open
    }

    virtual void exitServices() override {
        socketExit();
        nifmExit();
        i2cExit();
        smExit();
        spsmExit();
        splExit();
        fsdevUnmountAll();
    }

    virtual void onShow() override {}
    virtual void onHide() override { TetrisElement::paused = true; }

    virtual std::unique_ptr<tsl::Gui> loadInitialGui() override {
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
