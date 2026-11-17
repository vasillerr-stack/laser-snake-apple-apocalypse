#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <fstream>
#include <optional>
#include <random>
#include <string>
#include <vector>

const int WINDOW_WIDTH = 1100;
const int WINDOW_HEIGHT = 790;
const int CELL_SIZE = 22;
const int GRID_WIDTH = 34;
const int GRID_HEIGHT = 23;
const int BOARD_X = 176;
const int BOARD_Y = 150;
const int BOARD_WIDTH = GRID_WIDTH * CELL_SIZE;
const int BOARD_HEIGHT = GRID_HEIGHT * CELL_SIZE;
const std::string GAME_TITLE = "LASER SNAKE: APPLE APOCALYPSE";

struct Cell {
    int x;
    int y;

    bool operator==(const Cell& other) const {
        return x == other.x && y == other.y;
    }

    bool operator!=(const Cell& other) const {
        return !(*this == other);
    }
};

enum class Direction {
    Up,
    Down,
    Left,
    Right
};

enum class EnemyType {
    Beetle,
    Mantis,
    Wasp,
    Queen
};

enum class PowerType {
    Shield,
    RapidFire,
    DoubleScore
};

enum class GameState {
    Menu,
    Playing,
    Paused,
    GameOver
};

struct Apple {
    Cell position;
    bool golden;
};

struct Enemy {
    Cell position;
    EnemyType type;
    int health;
    float moveTimer;
    float shotTimer;
};

struct EnemyShot {
    Cell position;
    int dx;
    int dy;
    float moveTimer;
};

struct Powerup {
    Cell position;
    PowerType type;
};

Cell directionVector(Direction direction) {
    if (direction == Direction::Up) return {0, -1};
    if (direction == Direction::Down) return {0, 1};
    if (direction == Direction::Left) return {-1, 0};
    return {1, 0};
}

Direction opposite(Direction direction) {
    if (direction == Direction::Up) return Direction::Down;
    if (direction == Direction::Down) return Direction::Up;
    if (direction == Direction::Left) return Direction::Right;
    return Direction::Left;
}

class Game {
public:
    explicit Game(sf::RenderWindow& gameWindow)
        : window(gameWindow), randomEngine(static_cast<unsigned int>(std::time(nullptr))) {
        loadFont();
        loadHighScore();
    }

    void handleEvents() {
        sf::Event event{};

        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                saveHighScore();
                window.close();
            }

            if (event.type != sf::Event::KeyPressed) continue;

            sf::Keyboard::Key key = event.key.code;

            if (state == GameState::Menu) {
                if (key == sf::Keyboard::Enter || key == sf::Keyboard::Space) startGame();
                continue;
            }

            if (state == GameState::GameOver) {
                if (key == sf::Keyboard::Enter || key == sf::Keyboard::R) startGame();
                if (key == sf::Keyboard::Escape) state = GameState::Menu;
                continue;
            }

            if (state == GameState::Paused) {
                if (key == sf::Keyboard::P || key == sf::Keyboard::Enter || key == sf::Keyboard::Escape) {
                    state = GameState::Playing;
                }
                continue;
            }

            if (key == sf::Keyboard::P || key == sf::Keyboard::Escape) {
                state = GameState::Paused;
            } else if (key == sf::Keyboard::R) {
                startGame();
            } else if (key == sf::Keyboard::Space) {
                fireLaser();
            } else if (key == sf::Keyboard::W || key == sf::Keyboard::Up) {
                requestTurn(Direction::Up);
            } else if (key == sf::Keyboard::S || key == sf::Keyboard::Down) {
                requestTurn(Direction::Down);
            } else if (key == sf::Keyboard::A || key == sf::Keyboard::Left) {
                requestTurn(Direction::Left);
            } else if (key == sf::Keyboard::D || key == sf::Keyboard::Right) {
                requestTurn(Direction::Right);
            }
        }
    }

    void update(float deltaTime) {
        animationTime += deltaTime;

        if (state != GameState::Playing) return;

        if (invulnerabilityTimer > 0.0f) invulnerabilityTimer -= deltaTime;
        if (rapidFireTimer > 0.0f) rapidFireTimer -= deltaTime;
        if (doubleScoreTimer > 0.0f) doubleScoreTimer -= deltaTime;
        if (laserTimer > 0.0f) laserTimer -= deltaTime;
        if (laserCooldown > 0.0f) laserCooldown -= deltaTime;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Space)) fireLaser();

        snakeMoveTimer += deltaTime;
        enemySpawnTimer += deltaTime;
        powerupSpawnTimer += deltaTime;

        float snakeSpeed = std::max(0.075f, 0.145f - static_cast<float>(wave - 1) * 0.006f);

        if (snakeMoveTimer >= snakeSpeed) {
            snakeMoveTimer = 0.0f;
            moveSnake();
            if (state != GameState::Playing) return;
        }

        float spawnDelay = std::max(0.85f, 3.5f - static_cast<float>(wave - 1) * 0.17f);
        int enemyLimit = std::min(17, 3 + wave);

        if (enemySpawnTimer >= spawnDelay && static_cast<int>(enemies.size()) < enemyLimit) {
            enemySpawnTimer = 0.0f;
            spawnEnemy();
        }

        if (powerupSpawnTimer >= 19.0f && !powerup.has_value()) {
            powerupSpawnTimer = 0.0f;
            spawnPowerup();
        }

        updateEnemies(deltaTime);
        updateEnemyShots(deltaTime);
        collectPowerup();
    }

    void draw() {
        window.clear(sf::Color(5, 11, 17));
        drawBackground();

        if (state == GameState::Menu) {
            drawMenu();
        } else {
            drawGame();

            if (state == GameState::Paused) drawPause();
            if (state == GameState::GameOver) drawGameOver();
        }

        window.display();
    }

private:
    sf::RenderWindow& window;
    sf::Font font;
    bool fontLoaded = false;
    std::mt19937 randomEngine;
    GameState state = GameState::Menu;

    std::vector<Cell> snake;
    std::vector<Enemy> enemies;
    std::vector<EnemyShot> enemyShots;
    Apple apple{};
    std::optional<Powerup> powerup;

    Direction direction = Direction::Right;
    Direction nextDirection = Direction::Right;
    bool turnPending = false;

    int score = 0;
    int highScore = 0;
    int health = 3;
    int wave = 1;
    int kills = 0;
    int applesEaten = 0;
    int bossWaveSpawned = 0;

    bool shieldActive = false;
    float rapidFireTimer = 0.0f;
    float doubleScoreTimer = 0.0f;
    float invulnerabilityTimer = 0.0f;
    float snakeMoveTimer = 0.0f;
    float enemySpawnTimer = 0.0f;
    float powerupSpawnTimer = 0.0f;
    float laserCooldown = 0.0f;
    float laserTimer = 0.0f;
    float animationTime = 0.0f;

    void loadFont() {
        const std::vector<std::string> paths = {
            "C:/Windows/Fonts/consola.ttf",
            "C:/Windows/Fonts/lucon.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
            "/usr/share/fonts/truetype/liberation2/LiberationMono-Regular.ttf",
            "/System/Library/Fonts/Menlo.ttc"
        };

        for (const std::string& path : paths) {
            if (font.loadFromFile(path)) {
                fontLoaded = true;
                break;
            }
        }
    }

    void loadHighScore() {
        std::ifstream file("highscore.txt");
        if (file) file >> highScore;
    }

    void saveHighScore() {
        if (score > highScore) highScore = score;
        std::ofstream file("highscore.txt");
        if (file) file << highScore;
    }

    int randomInt(int minimum, int maximum) {
        std::uniform_int_distribution<int> distribution(minimum, maximum);
        return distribution(randomEngine);
    }

    void startGame() {
        score = 0;
        health = 3;
        wave = 1;
        kills = 0;
        applesEaten = 0;
        bossWaveSpawned = 0;
        shieldActive = false;
        rapidFireTimer = 0.0f;
        doubleScoreTimer = 0.0f;
        invulnerabilityTimer = 0.0f;
        snakeMoveTimer = 0.0f;
        enemySpawnTimer = 1.5f;
        powerupSpawnTimer = 0.0f;
        laserCooldown = 0.0f;
        laserTimer = 0.0f;
        direction = Direction::Right;
        nextDirection = Direction::Right;
        turnPending = false;
        enemies.clear();
        enemyShots.clear();
        powerup.reset();
        resetSnake();
        spawnApple();
        state = GameState::Playing;
    }

    void resetSnake() {
        snake.clear();
        int startX = GRID_WIDTH / 2;
        int startY = GRID_HEIGHT / 2;

        for (int i = 0; i < 5; ++i) {
            snake.push_back({startX - i, startY});
        }

        if (snakeContains(apple.position)) spawnApple();

        direction = Direction::Right;
        nextDirection = Direction::Right;
        turnPending = false;
    }

    void requestTurn(Direction newDirection) {
        if (newDirection == opposite(direction)) return;
        if (turnPending && newDirection == opposite(nextDirection)) return;
        nextDirection = newDirection;
        turnPending = true;
    }

    bool insideBoard(Cell cell) const {
        return cell.x >= 0 && cell.x < GRID_WIDTH && cell.y >= 0 && cell.y < GRID_HEIGHT;
    }

    bool snakeContains(Cell cell) const {
        for (const Cell& part : snake) {
            if (part == cell) return true;
        }
        return false;
    }

    bool enemyContains(Cell cell) const {
        for (const Enemy& enemy : enemies) {
            if (enemy.position == cell) return true;
        }
        return false;
    }

    Cell randomFreeCell() {
        for (int attempt = 0; attempt < 600; ++attempt) {
            Cell cell{randomInt(0, GRID_WIDTH - 1), randomInt(0, GRID_HEIGHT - 1)};
            if (snakeContains(cell) || enemyContains(cell) || apple.position == cell) continue;
            if (powerup.has_value() && powerup->position == cell) continue;
            return cell;
        }

        for (int y = 0; y < GRID_HEIGHT; ++y) {
            for (int x = 0; x < GRID_WIDTH; ++x) {
                Cell cell{x, y};
                if (!snakeContains(cell) && !enemyContains(cell) && cell != apple.position &&
                    (!powerup.has_value() || cell != powerup->position)) return cell;
            }
        }

        return {1, 1};
    }

    void spawnApple() {
        apple.position = randomFreeCell();
        apple.golden = randomInt(1, 8) == 1;
    }

    void spawnPowerup() {
        PowerType type = static_cast<PowerType>(randomInt(0, 2));
        powerup = Powerup{randomFreeCell(), type};
    }

    void spawnEnemy() {
        bool bossExists = false;
        for (const Enemy& enemy : enemies) {
            if (enemy.type == EnemyType::Queen) bossExists = true;
        }

        EnemyType type = EnemyType::Beetle;

        bool spawningQueen = false;

        if (wave % 5 == 0 && !bossExists && bossWaveSpawned != wave) {
            type = EnemyType::Queen;
            spawningQueen = true;
        } else {
            int choice = randomInt(0, 99);
            if (wave >= 3 && choice < 24) type = EnemyType::Wasp;
            else if (wave >= 2 && choice < 53) type = EnemyType::Mantis;
        }

        Cell position{};
        bool found = false;

        for (int attempt = 0; attempt < 100; ++attempt) {
            int side = randomInt(0, 3);

            if (side == 0) position = {randomInt(0, GRID_WIDTH - 1), 0};
            if (side == 1) position = {randomInt(0, GRID_WIDTH - 1), GRID_HEIGHT - 1};
            if (side == 2) position = {0, randomInt(0, GRID_HEIGHT - 1)};
            if (side == 3) position = {GRID_WIDTH - 1, randomInt(0, GRID_HEIGHT - 1)};

            int distance = std::abs(position.x - snake.front().x) + std::abs(position.y - snake.front().y);
            if (!enemyContains(position) && !snakeContains(position) && position != apple.position &&
                (!powerup.has_value() || position != powerup->position) && distance > 8) {
                found = true;
                break;
            }
        }

        if (!found) return;

        int enemyHealth = 1;
        if (type == EnemyType::Mantis) enemyHealth = 2;
        if (type == EnemyType::Queen) enemyHealth = 14;

        enemies.push_back({position, type, enemyHealth, 0.0f, 0.0f});
        if (spawningQueen) bossWaveSpawned = wave;
    }

    void moveSnake() {
        if (turnPending) {
            direction = nextDirection;
            turnPending = false;
        }

        Cell move = directionVector(direction);
        Cell newHead{snake.front().x + move.x, snake.front().y + move.y};

        if (!insideBoard(newHead)) {
            hurtPlayer();
            if (state == GameState::Playing) resetSnake();
            return;
        }

        bool eating = newHead == apple.position;

        for (std::size_t i = 0; i < snake.size(); ++i) {
            if (snake[i] == newHead && !(i == snake.size() - 1 && !eating)) {
                state = GameState::GameOver;
                saveHighScore();
                return;
            }
        }

        snake.insert(snake.begin(), newHead);

        if (eating) {
            applesEaten++;
            addScore(apple.golden ? 500 : 100);
            if (applesEaten % 4 == 0) {
                wave++;
                enemySpawnTimer = 0.0f;
            }

            spawnApple();
        } else {
            snake.pop_back();
        }

        for (std::size_t i = 0; i < enemies.size();) {
            if (enemies[i].position == snake.front()) {
                enemies.erase(enemies.begin() + static_cast<long>(i));
                hurtPlayer();
                if (state != GameState::Playing) return;
            } else {
                ++i;
            }
        }
    }

    void addScore(int points) {
        int multiplier = doubleScoreTimer > 0.0f ? 2 : 1;
        score += points * multiplier;
        if (score > highScore) highScore = score;
    }

    void hurtPlayer() {
        if (invulnerabilityTimer > 0.0f) return;

        if (shieldActive) {
            shieldActive = false;
            invulnerabilityTimer = 0.9f;
            return;
        }

        health--;
        invulnerabilityTimer = 1.5f;

        if (health <= 0) {
            state = GameState::GameOver;
            saveHighScore();
        } else {
            resetSnake();
        }
    }

    void fireLaser() {
        if (laserCooldown > 0.0f || state != GameState::Playing) return;

        laserCooldown = rapidFireTimer > 0.0f ? 0.19f : 0.48f;
        laserTimer = 0.15f;
        Cell move = directionVector(direction);
        Cell head = snake.front();

        for (std::size_t i = 0; i < enemies.size();) {
            Cell enemyPosition = enemies[i].position;
            int distance = 0;
            bool inLine = false;

            if (move.x != 0 && enemyPosition.y == head.y) {
                distance = (enemyPosition.x - head.x) * move.x;
                inLine = true;
            }

            if (move.y != 0 && enemyPosition.x == head.x) {
                distance = (enemyPosition.y - head.y) * move.y;
                inLine = true;
            }

            if (inLine && distance >= 1 && distance <= 10) {
                enemies[i].health--;

                if (enemies[i].health <= 0) {
                    int points = 35;
                    if (enemies[i].type == EnemyType::Mantis) points = 65;
                    if (enemies[i].type == EnemyType::Wasp) points = 55;
                    if (enemies[i].type == EnemyType::Queen) points = 1200;
                    addScore(points);
                    kills++;
                    enemies.erase(enemies.begin() + static_cast<long>(i));
                    continue;
                }
            }

            ++i;
        }
    }

    void updateEnemies(float deltaTime) {
        for (std::size_t i = 0; i < enemies.size();) {
            Enemy& enemy = enemies[i];
            float moveDelay = 0.68f;

            if (enemy.type == EnemyType::Mantis) moveDelay = 0.34f;
            if (enemy.type == EnemyType::Wasp) moveDelay = 0.58f;
            if (enemy.type == EnemyType::Queen) moveDelay = 0.29f;

            moveDelay = std::max(0.15f, moveDelay - static_cast<float>(wave - 1) * 0.012f);
            enemy.moveTimer += deltaTime;
            enemy.shotTimer += deltaTime;

            if (enemy.moveTimer >= moveDelay) {
                enemy.moveTimer = 0.0f;
                moveEnemy(enemy, i);
            }

            if ((enemy.type == EnemyType::Wasp && enemy.shotTimer >= 1.8f) ||
                (enemy.type == EnemyType::Queen && enemy.shotTimer >= 1.15f)) {
                enemy.shotTimer = 0.0f;
                shootAtSnake(enemy.position);
            }

            if (state != GameState::Playing) return;

            if (snakeContains(enemy.position)) {
                enemies.erase(enemies.begin() + static_cast<long>(i));
                hurtPlayer();
                if (state != GameState::Playing) return;
            } else {
                ++i;
            }
        }
    }

    void moveEnemy(Enemy& enemy, std::size_t enemyIndex) {
        Cell head = snake.front();
        int differenceX = head.x - enemy.position.x;
        int differenceY = head.y - enemy.position.y;
        Cell step{0, 0};

        if (std::abs(differenceX) > std::abs(differenceY)) {
            step.x = differenceX > 0 ? 1 : -1;
        } else if (std::abs(differenceY) > std::abs(differenceX)) {
            step.y = differenceY > 0 ? 1 : -1;
        } else if (randomInt(0, 1) == 0) {
            step.x = differenceX > 0 ? 1 : -1;
        } else {
            step.y = differenceY > 0 ? 1 : -1;
        }

        if (enemy.type == EnemyType::Wasp && randomInt(0, 3) == 0) {
            step = {0, 0};
        }

        Cell next{enemy.position.x + step.x, enemy.position.y + step.y};
        if (!insideBoard(next)) return;

        for (std::size_t i = 0; i < enemies.size(); ++i) {
            if (i != enemyIndex && enemies[i].position == next) return;
        }

        if (next == apple.position) return;
        if (powerup.has_value() && next == powerup->position) return;

        enemy.position = next;
    }

    void shootAtSnake(Cell start) {
        Cell head = snake.front();
        int differenceX = head.x - start.x;
        int differenceY = head.y - start.y;
        int dx = 0;
        int dy = 0;

        if (std::abs(differenceX) > std::abs(differenceY)) {
            dx = differenceX > 0 ? 1 : -1;
        } else if (std::abs(differenceY) > 0) {
            dy = differenceY > 0 ? 1 : -1;
        } else {
            int choice = randomInt(0, 3);
            if (choice == 0) dx = 1;
            if (choice == 1) dx = -1;
            if (choice == 2) dy = 1;
            if (choice == 3) dy = -1;
        }

        enemyShots.push_back({start, dx, dy, 0.0f});
    }

    void updateEnemyShots(float deltaTime) {
        for (std::size_t i = 0; i < enemyShots.size();) {
            enemyShots[i].moveTimer += deltaTime;

            if (enemyShots[i].moveTimer < 0.12f) {
                ++i;
                continue;
            }

            enemyShots[i].moveTimer = 0.0f;
            enemyShots[i].position.x += enemyShots[i].dx;
            enemyShots[i].position.y += enemyShots[i].dy;

            if (!insideBoard(enemyShots[i].position)) {
                enemyShots.erase(enemyShots.begin() + static_cast<long>(i));
                continue;
            }

            if (snakeContains(enemyShots[i].position)) {
                enemyShots.erase(enemyShots.begin() + static_cast<long>(i));
                hurtPlayer();
                if (state != GameState::Playing) return;
                continue;
            }

            ++i;
        }
    }

    void collectPowerup() {
        if (!powerup.has_value() || !(snake.front() == powerup->position)) return;

        if (powerup->type == PowerType::Shield) shieldActive = true;
        if (powerup->type == PowerType::RapidFire) rapidFireTimer = 10.0f;
        if (powerup->type == PowerType::DoubleScore) doubleScoreTimer = 12.0f;

        powerup.reset();
    }

    sf::Vector2f cellPosition(Cell cell) const {
        return {
            static_cast<float>(BOARD_X + cell.x * CELL_SIZE),
            static_cast<float>(BOARD_Y + cell.y * CELL_SIZE)
        };
    }

    sf::Vector2f cellCenter(Cell cell) const {
        sf::Vector2f position = cellPosition(cell);
        return {position.x + CELL_SIZE / 2.0f, position.y + CELL_SIZE / 2.0f};
    }

    void drawRect(float x, float y, float width, float height, sf::Color color,
                  sf::Color outline = sf::Color::Transparent, float outlineSize = 0.0f) {
        sf::RectangleShape shape({width, height});
        shape.setPosition(x, y);
        shape.setFillColor(color);
        shape.setOutlineColor(outline);
        shape.setOutlineThickness(outlineSize);
        window.draw(shape);
    }

    void drawText(const std::string& value, float x, float y, unsigned int size, sf::Color color) {
        if (!fontLoaded) return;
        sf::Text text(value, font, size);
        text.setPosition(x, y);
        text.setFillColor(color);
        window.draw(text);
    }

    void drawCenteredText(const std::string& value, float centerX, float y,
                          unsigned int size, sf::Color color) {
        if (!fontLoaded) return;
        sf::Text text(value, font, size);
        text.setFillColor(color);
        sf::FloatRect bounds = text.getLocalBounds();
        text.setOrigin(bounds.left + bounds.width / 2.0f, bounds.top);
        text.setPosition(centerX, y);
        window.draw(text);
    }

    void drawBackground() {
        for (int i = 0; i < 18; ++i) {
            float x = static_cast<float>((i * 83) % WINDOW_WIDTH);
            float y = static_cast<float>((i * 137) % WINDOW_HEIGHT);
            sf::CircleShape dot(1.5f);
            dot.setPosition(x, y);
            dot.setFillColor(sf::Color(35, 75, 70, 90));
            window.draw(dot);
        }

        drawRect(0, 0, WINDOW_WIDTH, 3, sf::Color(25, 255, 125, 150));
    }

    void drawMenu() {
        drawCenteredText("ARCADE SURVIVAL PROTOCOL", WINDOW_WIDTH / 2.0f, 155, 17, sf::Color(75, 170, 135));
        drawCenteredText("LASER SNAKE", WINDOW_WIDTH / 2.0f, 215, 58, sf::Color(80, 255, 145));
        drawCenteredText("APPLE APOCALYPSE", WINDOW_WIDTH / 2.0f, 285, 31, sf::Color(220, 255, 225));
        drawCenteredText("A tiny snake. A very big problem.", WINDOW_WIDTH / 2.0f, 342, 17, sf::Color(115, 150, 145));

        drawMenuSnake();

        float pulse = 0.65f + 0.35f * std::sin(animationTime * 3.0f);
        sf::Color promptColor(85, 255, 150, static_cast<sf::Uint8>(120 + 135 * pulse));
        drawCenteredText("PRESS ENTER TO START", WINDOW_WIDTH / 2.0f, 545, 23, promptColor);
        drawCenteredText("WASD / ARROWS  MOVE     SPACE  LASER     P  PAUSE", WINDOW_WIDTH / 2.0f, 610, 15, sf::Color(125, 155, 150));
        drawCenteredText("Collect apples. Survive the swarm. Do not become lunch.", WINDOW_WIDTH / 2.0f, 645, 14, sf::Color(80, 115, 110));
        drawCenteredText("BEST SCORE: " + std::to_string(highScore), WINDOW_WIDTH / 2.0f, 700, 16, sf::Color(255, 205, 85));
    }

    void drawMenuSnake() {
        float startX = WINDOW_WIDTH / 2.0f - 88.0f;
        float y = 440.0f;

        for (int i = 0; i < 7; ++i) {
            sf::RectangleShape part({20.0f, 20.0f});
            part.setPosition(startX + i * 22.0f, y);
            part.setFillColor(i == 0 ? sf::Color(80, 255, 145) : sf::Color(24, 170, 95));
            window.draw(part);
        }

        sf::CircleShape eye(3.0f);
        eye.setFillColor(sf::Color::White);
        eye.setPosition(startX + 8.0f, y + 4.0f);
        window.draw(eye);
        eye.setPosition(startX + 8.0f, y + 13.0f);
        window.draw(eye);

        drawRect(startX + 150.0f, y + 8.0f, 95.0f, 3.0f, sf::Color(50, 255, 130, 170));
        drawRect(startX + 150.0f, y + 14.0f, 95.0f, 3.0f, sf::Color(50, 255, 130, 170));

        sf::CircleShape appleShape(9.0f);
        appleShape.setPosition(startX + 270.0f, y + 2.0f);
        appleShape.setFillColor(sf::Color(255, 75, 80));
        window.draw(appleShape);
    }

    void drawGame() {
        drawHud();
        drawBoard();
        drawApple();
        drawPowerup();
        drawEnemyShots();
        drawEnemies();
        drawLasers();
        drawSnake();
        drawFooter();
    }

    void drawHud() {
        drawText("LASER SNAKE", BOARD_X, 24, 25, sf::Color(90, 255, 150));
        drawText("APPLE APOCALYPSE", BOARD_X + 205, 31, 15, sf::Color(110, 145, 135));
        drawText("SCORE  " + std::to_string(score), BOARD_X, 83, 17, sf::Color(225, 245, 230));
        drawText("BEST  " + std::to_string(highScore), BOARD_X + 190, 83, 17, sf::Color(255, 205, 90));
        drawText("WAVE  " + std::to_string(wave), BOARD_X + 390, 83, 17, sf::Color(115, 225, 255));
        drawText("KILLS  " + std::to_string(kills), BOARD_X + 515, 83, 17, sf::Color(230, 150, 255));

        for (int i = 0; i < 3; ++i) {
            sf::CircleShape heart(7.0f);
            heart.setPosition(BOARD_X + 680 + i * 19.0f, 86.0f);
            heart.setFillColor(i < health ? sf::Color(255, 85, 105) : sf::Color(65, 55, 65));
            window.draw(heart);
        }

        drawRect(BOARD_X, 119, BOARD_WIDTH, 1, sf::Color(35, 75, 65));
    }

    void drawBoard() {
        drawRect(static_cast<float>(BOARD_X - 5), static_cast<float>(BOARD_Y - 5),
                 static_cast<float>(BOARD_WIDTH + 10), static_cast<float>(BOARD_HEIGHT + 10),
                 sf::Color(7, 18, 21), sf::Color(38, 105, 75), 2.0f);

        drawRect(static_cast<float>(BOARD_X), static_cast<float>(BOARD_Y),
                 static_cast<float>(BOARD_WIDTH), static_cast<float>(BOARD_HEIGHT),
                 sf::Color(8, 23, 23));

        for (int x = 0; x <= GRID_WIDTH; ++x) {
            float lineX = static_cast<float>(BOARD_X + x * CELL_SIZE);
            drawRect(lineX, static_cast<float>(BOARD_Y), 1.0f, static_cast<float>(BOARD_HEIGHT),
                     sf::Color(18, 48, 42));
        }

        for (int y = 0; y <= GRID_HEIGHT; ++y) {
            float lineY = static_cast<float>(BOARD_Y + y * CELL_SIZE);
            drawRect(static_cast<float>(BOARD_X), lineY, static_cast<float>(BOARD_WIDTH), 1.0f,
                     sf::Color(18, 48, 42));
        }
    }

    void drawApple() {
        sf::Vector2f center = cellCenter(apple.position);
        sf::CircleShape fruit(8.0f);
        fruit.setOrigin(8.0f, 8.0f);
        fruit.setPosition(center);
        fruit.setFillColor(apple.golden ? sf::Color(255, 200, 55) : sf::Color(255, 75, 85));
        fruit.setOutlineColor(apple.golden ? sf::Color(255, 245, 150) : sf::Color(255, 150, 145));
        fruit.setOutlineThickness(1.0f);
        window.draw(fruit);

        sf::RectangleShape stem({2.0f, 6.0f});
        stem.setPosition(center.x + 1.0f, center.y - 12.0f);
        stem.setRotation(20.0f);
        stem.setFillColor(sf::Color(115, 205, 105));
        window.draw(stem);
    }

    void drawPowerup() {
        if (!powerup.has_value()) return;

        sf::Color color = sf::Color(80, 220, 255);
        if (powerup->type == PowerType::RapidFire) color = sf::Color(190, 100, 255);
        if (powerup->type == PowerType::DoubleScore) color = sf::Color(255, 210, 70);

        sf::CircleShape shape(9.0f, 4);
        shape.setOrigin(9.0f, 9.0f);
        shape.setPosition(cellCenter(powerup->position));
        shape.setRotation(45.0f + animationTime * 30.0f);
        shape.setFillColor(color);
        window.draw(shape);
    }

    void drawSnake() {
        for (std::size_t i = snake.size(); i > 1; --i) {
            Cell part = snake[i - 1];
            sf::Vector2f position = cellPosition(part);
            sf::RectangleShape body({CELL_SIZE - 4.0f, CELL_SIZE - 4.0f});
            body.setPosition(position.x + 2.0f, position.y + 2.0f);

            int shade = std::max(75, 190 - static_cast<int>(i) * 3);
            body.setFillColor(sf::Color(30, static_cast<sf::Uint8>(shade), 100));
            body.setOutlineColor(sf::Color(90, 255, 145, 100));
            body.setOutlineThickness(1.0f);
            window.draw(body);
        }

        if (snake.empty()) return;

        sf::Vector2f headPosition = cellPosition(snake.front());
        sf::RectangleShape head({CELL_SIZE - 2.0f, CELL_SIZE - 2.0f});
        head.setPosition(headPosition.x + 1.0f, headPosition.y + 1.0f);
        head.setFillColor(invulnerabilityTimer > 0.0f && static_cast<int>(animationTime * 14.0f) % 2 == 0
                              ? sf::Color(160, 255, 195)
                              : sf::Color(70, 255, 135));
        head.setOutlineColor(sf::Color(200, 255, 210));
        head.setOutlineThickness(1.0f);
        window.draw(head);

        Cell move = directionVector(direction);
        sf::Vector2f center = cellCenter(snake.front());
        sf::Vector2f side(-static_cast<float>(move.y), static_cast<float>(move.x));

        for (int sign : {-1, 1}) {
            sf::Vector2f eyePosition(
                center.x + move.x * 4.0f + side.x * 5.0f * sign - 3.0f,
                center.y + move.y * 4.0f + side.y * 5.0f * sign - 3.0f
            );

            sf::CircleShape eye(3.0f);
            eye.setPosition(eyePosition);
            eye.setFillColor(sf::Color(245, 255, 250));
            window.draw(eye);

            sf::CircleShape pupil(1.3f);
            pupil.setPosition(eyePosition.x + 1.2f, eyePosition.y + 1.2f);
            pupil.setFillColor(sf::Color(5, 35, 20));
            window.draw(pupil);
        }
    }

    void drawLasers() {
        if (laserTimer <= 0.0f || snake.empty()) return;

        Cell move = directionVector(direction);
        sf::Vector2f center = cellCenter(snake.front());
        sf::Vector2f side(-static_cast<float>(move.y), static_cast<float>(move.x));
        float angle = 0.0f;
        if (direction == Direction::Down) angle = 90.0f;
        if (direction == Direction::Left) angle = 180.0f;
        if (direction == Direction::Up) angle = 270.0f;

        float range = 10.0f;
        if (direction == Direction::Right) range = static_cast<float>(GRID_WIDTH - 1 - snake.front().x);
        if (direction == Direction::Left) range = static_cast<float>(snake.front().x);
        if (direction == Direction::Down) range = static_cast<float>(GRID_HEIGHT - 1 - snake.front().y);
        if (direction == Direction::Up) range = static_cast<float>(snake.front().y);

        for (int sign : {-1, 1}) {
            sf::Vector2f start(
                center.x + move.x * 8.0f + side.x * 5.0f * sign,
                center.y + move.y * 8.0f + side.y * 5.0f * sign
            );

            sf::RectangleShape glow({CELL_SIZE * range, 7.0f});
            glow.setOrigin(0.0f, 3.5f);
            glow.setPosition(start);
            glow.setRotation(angle);
            glow.setFillColor(sf::Color(35, 255, 125, 65));
            window.draw(glow);

            sf::RectangleShape beam({CELL_SIZE * range, 2.0f});
            beam.setOrigin(0.0f, 1.0f);
            beam.setPosition(start);
            beam.setRotation(angle);
            beam.setFillColor(sf::Color(165, 255, 190, 230));
            window.draw(beam);
        }
    }

    void drawEnemies() {
        for (const Enemy& enemy : enemies) {
            sf::Vector2f center = cellCenter(enemy.position);

            if (enemy.type == EnemyType::Beetle) {
                sf::CircleShape shell(8.0f);
                shell.setOrigin(8.0f, 8.0f);
                shell.setPosition(center);
                shell.setFillColor(sf::Color(220, 125, 55));
                shell.setOutlineColor(sf::Color(255, 190, 95));
                shell.setOutlineThickness(1.0f);
                window.draw(shell);

                drawRect(center.x - 1.0f, center.y - 7.0f, 2.0f, 14.0f, sf::Color(90, 45, 30));
                sf::CircleShape eye(2.0f);
                eye.setPosition(center.x - 5.0f, center.y - 2.0f);
                eye.setFillColor(sf::Color(255, 245, 205));
                window.draw(eye);
                eye.setPosition(center.x + 2.0f, center.y - 2.0f);
                window.draw(eye);
            }

            if (enemy.type == EnemyType::Mantis) {
                sf::CircleShape body(8.0f, 6);
                body.setOrigin(8.0f, 8.0f);
                body.setPosition(center);
                body.setFillColor(sf::Color(170, 225, 75));
                body.setOutlineColor(sf::Color(230, 255, 150));
                body.setOutlineThickness(1.0f);
                window.draw(body);

                sf::VertexArray arm(sf::Lines, 4);
                arm[0].position = {center.x - 4.0f, center.y - 3.0f};
                arm[1].position = {center.x - 10.0f, center.y - 11.0f};
                arm[2].position = {center.x + 4.0f, center.y - 3.0f};
                arm[3].position = {center.x + 10.0f, center.y - 11.0f};
                for (int j = 0; j < 4; ++j) arm[j].color = sf::Color(205, 255, 120);
                window.draw(arm);
            }

            if (enemy.type == EnemyType::Wasp) {
                sf::CircleShape wing(5.0f);
                wing.setOrigin(5.0f, 5.0f);
                wing.setPosition(center.x - 5.0f, center.y - 5.0f);
                wing.setScale(1.2f, 0.65f);
                wing.setFillColor(sf::Color(150, 225, 255, 170));
                window.draw(wing);
                wing.setPosition(center.x + 5.0f, center.y - 5.0f);
                window.draw(wing);

                sf::CircleShape body(7.0f, 6);
                body.setOrigin(7.0f, 7.0f);
                body.setPosition(center);
                body.setFillColor(sf::Color(255, 205, 55));
                body.setOutlineColor(sf::Color(255, 245, 165));
                body.setOutlineThickness(1.0f);
                window.draw(body);
                drawRect(center.x - 6.0f, center.y - 1.0f, 12.0f, 3.0f, sf::Color(65, 50, 25));
            }

            if (enemy.type == EnemyType::Queen) {
                sf::CircleShape body(13.0f, 8);
                body.setOrigin(13.0f, 13.0f);
                body.setPosition(center);
                body.setFillColor(sf::Color(200, 55, 115));
                body.setOutlineColor(sf::Color(255, 150, 200));
                body.setOutlineThickness(2.0f);
                window.draw(body);

                sf::VertexArray horns(sf::Lines, 4);
                horns[0].position = {center.x - 6.0f, center.y - 7.0f};
                horns[1].position = {center.x - 13.0f, center.y - 17.0f};
                horns[2].position = {center.x + 6.0f, center.y - 7.0f};
                horns[3].position = {center.x + 13.0f, center.y - 17.0f};
                for (int j = 0; j < 4; ++j) horns[j].color = sf::Color(255, 170, 205);
                window.draw(horns);

                drawRect(center.x - 12.0f, center.y - 21.0f, 24.0f, 3.0f, sf::Color(70, 25, 45));
                drawRect(center.x - 12.0f, center.y - 21.0f,
                         24.0f * static_cast<float>(enemy.health) / 14.0f, 3.0f,
                         sf::Color(255, 80, 130));
            }
        }
    }

    void drawEnemyShots() {
        for (const EnemyShot& shot : enemyShots) {
            sf::Vector2f center = cellCenter(shot.position);
            sf::CircleShape bullet(4.5f, 4);
            bullet.setOrigin(4.5f, 4.5f);
            bullet.setPosition(center);
            bullet.setRotation(45.0f);
            bullet.setFillColor(sf::Color(255, 65, 85));
            bullet.setOutlineColor(sf::Color(255, 175, 125));
            bullet.setOutlineThickness(1.0f);
            window.draw(bullet);
        }
    }

    void drawFooter() {
        drawRect(BOARD_X, 681, BOARD_WIDTH, 1, sf::Color(35, 75, 65));
        drawText("WASD / ARROWS  MOVE", BOARD_X, 699, 13, sf::Color(120, 160, 145));
        drawText("SPACE  LASER", BOARD_X + 220, 699, 13, sf::Color(120, 160, 145));
        drawText("P  PAUSE", BOARD_X + 375, 699, 13, sf::Color(120, 160, 145));
        drawText("R  RESTART", BOARD_X + 490, 699, 13, sf::Color(120, 160, 145));

        std::string status;
        if (shieldActive) status += "SHIELD  ";
        if (rapidFireTimer > 0.0f) status += "RAPID FIRE " + std::to_string(static_cast<int>(rapidFireTimer + 1)) + "s  ";
        if (doubleScoreTimer > 0.0f) status += "x2 SCORE " + std::to_string(static_cast<int>(doubleScoreTimer + 1)) + "s";
        if (!status.empty()) drawText(status, BOARD_X, 730, 13, sf::Color(100, 230, 255));
    }

    void drawPause() {
        drawRect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, sf::Color(0, 0, 0, 175));
        drawCenteredText("PAUSED", WINDOW_WIDTH / 2.0f, 300, 48, sf::Color(100, 255, 155));
        drawCenteredText("Press P or ENTER to continue", WINDOW_WIDTH / 2.0f, 380, 18, sf::Color(220, 240, 225));
    }

    void drawGameOver() {
        drawRect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, sf::Color(0, 0, 0, 190));
        drawCenteredText("GAME OVER", WINDOW_WIDTH / 2.0f, 245, 52, sf::Color(255, 85, 105));
        drawCenteredText("The apple apocalypse wins this round.", WINDOW_WIDTH / 2.0f, 325, 18, sf::Color(220, 220, 220));
        drawCenteredText("SCORE  " + std::to_string(score), WINDOW_WIDTH / 2.0f, 390, 25, sf::Color(100, 255, 155));
        drawCenteredText("KILLS  " + std::to_string(kills) + "      WAVE  " + std::to_string(wave),
                         WINDOW_WIDTH / 2.0f, 435, 18, sf::Color(170, 200, 190));
        drawCenteredText("Press ENTER or R to play again", WINDOW_WIDTH / 2.0f, 520, 18, sf::Color(255, 215, 100));
        drawCenteredText("Press ESC to return to menu", WINDOW_WIDTH / 2.0f, 555, 15, sf::Color(130, 155, 145));
    }
};

int main() {
    sf::RenderWindow window(
        sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT),
        GAME_TITLE,
        sf::Style::Titlebar | sf::Style::Close
    );

    window.setFramerateLimit(60);
    window.setVerticalSyncEnabled(true);

    Game game(window);
    sf::Clock clock;

    while (window.isOpen()) {
        float deltaTime = std::min(clock.restart().asSeconds(), 0.05f);
        game.handleEvents();
        game.update(deltaTime);
        game.draw();
    }

    return 0;
}
