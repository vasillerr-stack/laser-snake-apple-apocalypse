#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
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
const int CELL_SIZE = 20;
const int GRID_WIDTH = 34;
const int GRID_HEIGHT = 23;
const int BOARD_X = 105;
const int BOARD_Y = 158;
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
    GameOver,
    Upgrade
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
        makeSounds();
        configureWindow();
    }

    void handleEvents() {
        sf::Event event{};
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                saveHighScore();
                window.close();
            }
            if (event.type == sf::Event::Resized) updateView();
            if (event.type == sf::Event::LostFocus) {
                laserHeld = false;
                laserSound.stop();
            }
            if (event.type == sf::Event::KeyReleased && event.key.code == sf::Keyboard::Space) {
                laserHeld = false;
                laserSound.stop();
            }
            if (event.type != sf::Event::KeyPressed) continue;
            sf::Keyboard::Key key = event.key.code;

            if (key == sf::Keyboard::F11 || (key == sf::Keyboard::Enter && event.key.alt)) {
                toggleFullscreen();
                continue;
            }

            if (state == GameState::Menu) {
                if (key == sf::Keyboard::Enter || key == sf::Keyboard::Space) startGame();
                continue;
            }
            if (state == GameState::Upgrade) {
                if (key == sf::Keyboard::Num1) chooseUpgrade(1);
                if (key == sf::Keyboard::Num2) chooseUpgrade(2);
                if (key == sf::Keyboard::Num3) chooseUpgrade(3);
                continue;
            }
            if (state == GameState::GameOver) {
                if (key == sf::Keyboard::Enter || key == sf::Keyboard::R) startGame();
                if (key == sf::Keyboard::Escape) state = GameState::Menu;
                continue;
            }
            if (state == GameState::Paused) {
                if (key == sf::Keyboard::P || key == sf::Keyboard::Enter || key == sf::Keyboard::Escape) state = GameState::Playing;
                continue;
            }
            if (key == sf::Keyboard::P || key == sf::Keyboard::Escape) {
                state = GameState::Paused;
                laserHeld = false;
                laserSound.stop();
            } else if (key == sf::Keyboard::R) {
                laserHeld = false;
                laserSound.stop();
                startGame();
            } else if (key == sf::Keyboard::Space) {
                laserHeld = true;
                if (laserSound.getStatus() != sf::Sound::Playing) laserSound.play();
            } else if (key == sf::Keyboard::W || key == sf::Keyboard::Up) requestTurn(Direction::Up);
            else if (key == sf::Keyboard::S || key == sf::Keyboard::Down) requestTurn(Direction::Down);
            else if (key == sf::Keyboard::A || key == sf::Keyboard::Left) requestTurn(Direction::Left);
            else if (key == sf::Keyboard::D || key == sf::Keyboard::Right) requestTurn(Direction::Right);
        }
    }

    void update(float deltaTime) {
        animationTime += deltaTime;
        crtTime += deltaTime;

        if (state != GameState::Playing) return;

        if (invulnerabilityTimer > 0.0f) invulnerabilityTimer -= deltaTime;
        if (rapidFireTimer > 0.0f) rapidFireTimer -= deltaTime;
        if (doubleScoreTimer > 0.0f) doubleScoreTimer -= deltaTime;
        if (laserTimer > 0.0f) laserTimer -= deltaTime;
        if (laserCooldown > 0.0f) laserCooldown -= deltaTime;
        if (laserHeld && laserCooldown <= 0.0f) fireLaser();

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
            drawCrt();
        } else {
            drawGame();

            if (state == GameState::Paused) drawPause();
            if (state == GameState::Upgrade) drawUpgrade();
            if (state == GameState::GameOver) drawGameOver();
        }

        window.display();
    }

private:
    sf::RenderWindow& window;
    sf::Font font;
    bool fontLoaded = false;
    sf::SoundBuffer eatBuffer, killBuffer, deathBuffer, laserBuffer;
    sf::Sound eatSound, killSound, deathSound, laserSound;
    bool fullscreen = false;
    bool laserHeld = false;
    int maxHealth = 3;
    int upgradeChoiceCount = 0;
    float crtTime = 0.0f;
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
    float baseLaserCooldown = 0.22f;
    float laserTimer = 0.0f;
    float animationTime = 0.0f;

    void configureWindow() {
        window.setFramerateLimit(60);
        window.setVerticalSyncEnabled(true);
        updateView();
    }

    void updateView() {
        float width = static_cast<float>(window.getSize().x);
        float height = static_cast<float>(window.getSize().y);
        if (width <= 0.0f || height <= 0.0f) return;
        sf::View view(sf::FloatRect(0.0f, 0.0f, static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT)));
        float windowRatio = width / height;
        float gameRatio = static_cast<float>(WINDOW_WIDTH) / WINDOW_HEIGHT;
        if (windowRatio > gameRatio) {
            float viewportWidth = gameRatio / windowRatio;
            view.setViewport(sf::FloatRect((1.0f - viewportWidth) / 2.0f, 0.0f, viewportWidth, 1.0f));
        } else {
            float viewportHeight = windowRatio / gameRatio;
            view.setViewport(sf::FloatRect(0.0f, (1.0f - viewportHeight) / 2.0f, 1.0f, viewportHeight));
        }
        window.setView(view);
    }

    void toggleFullscreen() {
        fullscreen = !fullscreen;
        if (fullscreen) window.create(sf::VideoMode::getDesktopMode(), GAME_TITLE, sf::Style::Fullscreen);
        else window.create(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), GAME_TITLE, sf::Style::Titlebar | sf::Style::Close | sf::Style::Resize);
        configureWindow();
    }

    void makeTone(sf::SoundBuffer& buffer, float frequency, float duration, bool laser) {
        const unsigned int rate = 44100;
        std::size_t count = static_cast<std::size_t>(rate * duration);
        std::vector<sf::Int16> samples(count);
        for (std::size_t i = 0; i < count; ++i) {
            float t = static_cast<float>(i) / rate;
            float progress = static_cast<float>(i) / static_cast<float>(count);
            float hz = laser ? frequency * (1.0f - 0.72f * progress) : frequency;
            float wave = std::sin(6.2831853f * hz * t);
            if (laser) wave += 0.35f * std::sin(6.2831853f * hz * 2.03f * t);
            float envelope = std::min(1.0f, t * 35.0f) * std::max(0.0f, 1.0f - progress);
            if (!laser) envelope *= std::min(1.0f, (1.0f - progress) * 8.0f);
            samples[i] = static_cast<sf::Int16>(wave * envelope * 9000.0f);
        }
        buffer.loadFromSamples(samples.data(), samples.size(), 1, rate);
    }

    void makeSounds() {
        makeTone(eatBuffer, 760.0f, 0.10f, false);
        makeTone(killBuffer, 180.0f, 0.22f, true);
        makeTone(deathBuffer, 95.0f, 0.65f, true);
        makeTone(laserBuffer, 620.0f, 0.32f, true);
        eatSound.setBuffer(eatBuffer);
        killSound.setBuffer(killBuffer);
        deathSound.setBuffer(deathBuffer);
        laserSound.setBuffer(laserBuffer);
        laserSound.setLoop(true);
        laserSound.setVolume(24.0f);
        eatSound.setVolume(45.0f);
        killSound.setVolume(40.0f);
        deathSound.setVolume(55.0f);
    }

    void loadFont() {
        const std::vector<std::string> paths = {
            "font.ttf",
            "assets/DejaVuSansMono.ttf",
            "C:/Windows/Fonts/consola.ttf",
            "C:/Windows/Fonts/lucon.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
            "/usr/share/fonts/truetype/liberation2/LiberationMono-Regular.ttf"
        };

        for (const std::string& path : paths) {
            if (font.loadFromFile(path)) {
                fontLoaded = true;
                return;
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
        maxHealth = 3;
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
        baseLaserCooldown = 0.22f;
        laserTimer = 0.0f;
        laserHeld = false;
        laserSound.stop();
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
                laserHeld = false;
                laserSound.stop();
                deathSound.play();
                saveHighScore();
                return;
            }
        }

        snake.insert(snake.begin(), newHead);

        if (eating) {
            eatSound.play();
            applesEaten++;
            addScore(apple.golden ? 500 : 100);
            if (applesEaten % 4 == 0) {
                wave++;
                enemySpawnTimer = 0.0f;
            }

            spawnApple();
            if (applesEaten > 0 && applesEaten % 10 == 0) {
                state = GameState::Upgrade;
                laserHeld = false;
                laserSound.stop();
                return;
            }
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
            laserHeld = false;
            laserSound.stop();
            deathSound.play();
            state = GameState::GameOver;
            saveHighScore();
        } else {
            resetSnake();
        }
    }

    void fireLaser() {
        if (laserCooldown > 0.0f || state != GameState::Playing || snake.empty()) return;

        float difficulty = std::min(0.62f, static_cast<float>(wave - 1) * 0.025f);
        float rapidMultiplier = rapidFireTimer > 0.0f ? 0.62f : 1.0f;
        laserCooldown = std::max(0.12f, (baseLaserCooldown + difficulty) * rapidMultiplier);
        laserTimer = laserCooldown;

        Cell head = snake.front();
        int targetIndex = -1;
        int bestDistance = GRID_WIDTH + GRID_HEIGHT + 1;

        for (std::size_t i = 0; i < enemies.size(); ++i) {
            const Cell enemy = enemies[i].position;
            int distance = -1;

            if (direction == Direction::Right && enemy.y == head.y && enemy.x > head.x) {
                distance = enemy.x - head.x;
            } else if (direction == Direction::Left && enemy.y == head.y && enemy.x < head.x) {
                distance = head.x - enemy.x;
            } else if (direction == Direction::Down && enemy.x == head.x && enemy.y > head.y) {
                distance = enemy.y - head.y;
            } else if (direction == Direction::Up && enemy.x == head.x && enemy.y < head.y) {
                distance = head.y - enemy.y;
            }

            if (distance > 0 && distance < bestDistance) {
                bestDistance = distance;
                targetIndex = static_cast<int>(i);
            }
        }

        if (targetIndex >= 0) {
            Enemy& target = enemies[static_cast<std::size_t>(targetIndex)];
            target.health--;

            if (target.health <= 0) {
                int points = 35;
                if (target.type == EnemyType::Mantis) points = 65;
                if (target.type == EnemyType::Wasp) points = 55;
                if (target.type == EnemyType::Queen) points = 1200;

                addScore(points);
                kills++;
                killSound.play();
                enemies.erase(enemies.begin() + targetIndex);
            }
        }
    }
    void chooseUpgrade(int choice) {
        if (state != GameState::Upgrade) return;
        if (choice == 1) baseLaserCooldown = std::max(0.10f, baseLaserCooldown - 0.045f);
        if (choice == 2) health = std::min(maxHealth, health + 1);
        if (choice == 3) {
            maxHealth = std::min(6, maxHealth + 1);
            health = std::min(maxHealth, health + 1);
        }
        upgradeChoiceCount++;
        state = GameState::Playing;
        laserCooldown = 0.0f;
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
        sf::Text text(sf::String::fromUtf8(value.begin(), value.end()), font, size);
        text.setPosition(x, y);
        text.setFillColor(color);
        window.draw(text);
    }

    void drawCenteredText(const std::string& value, float centerX, float y,
                          unsigned int size, sf::Color color) {
        if (!fontLoaded) return;
        sf::Text text(sf::String::fromUtf8(value.begin(), value.end()), font, size);
        text.setFillColor(color);
        sf::FloatRect bounds = text.getLocalBounds();
        text.setOrigin(bounds.left + bounds.width / 2.0f, bounds.top);
        text.setPosition(centerX, y);
        window.draw(text);
    }

    void drawBackground() {
        window.clear(sf::Color(4, 8, 7));
        drawRect(18, 18, WINDOW_WIDTH - 36, WINDOW_HEIGHT - 36, sf::Color(11, 17, 13), sf::Color(65, 92, 65), 3.0f);
        drawRect(31, 31, WINDOW_WIDTH - 62, WINDOW_HEIGHT - 62, sf::Color(5, 10, 8), sf::Color(30, 48, 34), 1.0f);
        drawRect(42, 42, WINDOW_WIDTH - 84, 3, sf::Color(105, 145, 92, 130));
        drawRect(42, WINDOW_HEIGHT - 45, WINDOW_WIDTH - 84, 3, sf::Color(105, 145, 92, 100));
    }

    void drawMenu() {
        drawCenteredText("ЛАЗЕРНАЯ ЗМЕЙКА", WINDOW_WIDTH / 2.0f, 145, 43, sf::Color(155, 220, 130));
        drawCenteredText("ЯБЛОЧНЫЙ АПОКАЛИПСИС", WINDOW_WIDTH / 2.0f, 202, 25, sf::Color(205, 210, 160));
        drawCenteredText("АРКАДНЫЙ АВТОМАТ  /  ВЫПУСК 1986", WINDOW_WIDTH / 2.0f, 270, 16, sf::Color(115, 145, 100));
        drawMenuSnake();
        float pulse = 0.65f + 0.35f * std::sin(animationTime * 3.0f);
        drawCenteredText("НАЖМИ ENTER, ЧТОБЫ НАЧАТЬ", WINDOW_WIDTH / 2.0f, 535, 21,
                         sf::Color(155, 220, 130, static_cast<sf::Uint8>(130 + 125 * pulse)));
        drawCenteredText("WASD / СТРЕЛКИ - ДВИЖЕНИЕ     ПРОБЕЛ - ЛАЗЕР", WINDOW_WIDTH / 2.0f, 595, 15, sf::Color(155, 165, 130));
        drawCenteredText("F11 - ПОЛНЫЙ ЭКРАН     P - ПАУЗА", WINDOW_WIDTH / 2.0f, 625, 15, sf::Color(155, 165, 130));
        drawCenteredText("РЕКОРД: " + std::to_string(highScore), WINDOW_WIDTH / 2.0f, 690, 19, sf::Color(220, 190, 100));
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
        drawSidePanel();
        drawFooter();
        drawCrt();
    }

    void drawHud() {
        drawText("ЛАЗЕРНАЯ ЗМЕЙКА", 105, 68, 25, sf::Color(170, 220, 140));
        drawText("ЯБЛОЧНЫЙ АПОКАЛИПСИС", 390, 75, 15, sf::Color(135, 155, 115));
        drawText("СЧЁТ  " + std::to_string(score), 105, 112, 17, sf::Color(215, 220, 180));
        drawText("РЕКОРД  " + std::to_string(highScore), 295, 112, 17, sf::Color(220, 190, 100));
        drawText("ВОЛНА  " + std::to_string(wave), 515, 112, 17, sf::Color(160, 205, 145));
        drawText("УБИЙСТВА  " + std::to_string(kills), 675, 112, 17, sf::Color(190, 170, 130));
        drawRect(105, 140, 890, 2, sf::Color(65, 90, 60));
    }

    void drawSidePanel() {
        int panelX = 810;
        drawRect(panelX, BOARD_Y, 185, BOARD_HEIGHT, sf::Color(9, 15, 11), sf::Color(75, 100, 65), 2.0f);
        drawText("СОСТОЯНИЕ", panelX + 15, 174, 15, sf::Color(160, 200, 130));
        drawText("ЖИЗНИ", panelX + 15, 215, 13, sf::Color(130, 150, 110));
        for (int i = 0; i < maxHealth; ++i) {
            sf::RectangleShape heart({15.0f, 15.0f});
            heart.setPosition(static_cast<float>(panelX + 16 + i * 23), 241.0f);
            heart.setFillColor(i < health ? sf::Color(190, 85, 75) : sf::Color(55, 55, 45));
            window.draw(heart);
        }
        drawText("ЯБЛОКИ", panelX + 15, 282, 13, sf::Color(130, 150, 110));
        drawText(std::to_string(applesEaten) + " / 10", panelX + 15, 304, 20, sf::Color(220, 195, 115));
        drawText("ЗАРЯД ЛАЗЕРА", panelX + 15, 356, 13, sf::Color(130, 150, 110));
        float full = std::max(0.01f, baseLaserCooldown + std::min(0.62f, static_cast<float>(wave - 1) * 0.025f));
        float charge = std::clamp(1.0f - laserCooldown / full, 0.0f, 1.0f);
        drawRect(panelX + 15, 382, 150, 12, sf::Color(35, 45, 32), sf::Color(85, 110, 70), 1.0f);
        drawRect(panelX + 17, 384, 146 * charge, 8, sf::Color(145, 220, 105));
        drawText(laserCooldown <= 0.0f ? "ГОТОВ" : "ЗАРЯДКА", panelX + 15, 405, 12,
                 laserCooldown <= 0.0f ? sf::Color(170, 220, 130) : sf::Color(170, 165, 115));
        drawText("УСИЛЕНИЯ", panelX + 15, 454, 13, sf::Color(130, 150, 110));
        int y = 480;
        if (shieldActive) { drawText("ЩИТ", panelX + 15, static_cast<float>(y), 14, sf::Color(120, 190, 210)); y += 24; }
        if (rapidFireTimer > 0.0f) { drawText("БЫСТРЫЙ ЛАЗЕР", panelX + 15, static_cast<float>(y), 12, sf::Color(190, 145, 220)); y += 24; }
        if (doubleScoreTimer > 0.0f) { drawText("ОЧКИ x2", panelX + 15, static_cast<float>(y), 13, sf::Color(220, 190, 100)); }
        drawText("ВРАГИ", panelX + 15, 570, 13, sf::Color(130, 150, 110));
        drawText("ЖУК  /  БОГОМОЛ", panelX + 15, 594, 11, sf::Color(180, 165, 125));
        drawText("ОСА  /  КОРОЛЕВА", panelX + 15, 614, 11, sf::Color(180, 165, 125));
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
        if (!laserHeld || snake.empty()) return;

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
                center.x + move.x * 8.0f - move.y * 5.0f * sign,
                center.y + move.y * 8.0f + move.x * 5.0f * sign
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
        drawRect(105, 640, 890, 2, sf::Color(65, 90, 60));
        drawText("WASD / СТРЕЛКИ  ДВИЖЕНИЕ", 105, 658, 13, sf::Color(145, 160, 120));
        drawText("ПРОБЕЛ  ЛАЗЕР", 390, 658, 13, sf::Color(145, 160, 120));
        drawText("P  ПАУЗА", 575, 658, 13, sf::Color(145, 160, 120));
        drawText("F11  ЭКРАН", 710, 658, 13, sf::Color(145, 160, 120));
        drawText("R  ЗАНОВО", 850, 658, 13, sf::Color(145, 160, 120));
        drawText("СОБЕРИ 10 ЯБЛОК - ПОЛУЧИ УЛУЧШЕНИЕ", 105, 688, 14, sf::Color(185, 175, 125));
    }

    void drawCrt() {
        for (int y = 0; y < WINDOW_HEIGHT; y += 4) drawRect(0, static_cast<float>(y), WINDOW_WIDTH, 1, sf::Color(0, 0, 0, 38));
        float flicker = 0.025f + 0.015f * std::sin(crtTime * 37.0f);
        drawRect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, sf::Color(100, 140, 90, static_cast<sf::Uint8>(flicker * 255.0f)));
        for (int i = 0; i < 26; ++i) {
            int x = (i * 173 + static_cast<int>(crtTime * 19.0f)) % WINDOW_WIDTH;
            int y = (i * 97 + static_cast<int>(crtTime * 31.0f)) % WINDOW_HEIGHT;
            drawRect(static_cast<float>(x), static_cast<float>(y), 2, 2, sf::Color(170, 205, 140, 35));
        }
    }

    void drawPause() {
        drawRect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, sf::Color(0, 0, 0, 190));
        drawCenteredText("ПАУЗА", WINDOW_WIDTH / 2.0f, 300, 48, sf::Color(170, 220, 140));
        drawCenteredText("НАЖМИ P, ЧТОБЫ ПРОДОЛЖИТЬ", WINDOW_WIDTH / 2.0f, 380, 18, sf::Color(205, 210, 175));
    }

    void drawUpgrade() {
        drawRect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, sf::Color(0, 0, 0, 220));
        drawRect(230, 170, 640, 440, sf::Color(10, 19, 12), sf::Color(130, 160, 95), 3.0f);
        drawCenteredText("ПЕРЕРЫВ НА ОБСЛУЖИВАНИЕ", WINDOW_WIDTH / 2.0f, 215, 26, sf::Color(185, 220, 145));
        drawCenteredText("ВЫБЕРИ ОДНО УЛУЧШЕНИЕ", WINDOW_WIDTH / 2.0f, 265, 17, sf::Color(165, 175, 135));
        drawText("1   УСКОРИТЬ ЗАРЯДКУ ЛАЗЕРА", 300, 335, 19, sf::Color(190, 220, 145));
        drawText("    Перезарядка станет короче", 300, 365, 14, sf::Color(145, 165, 125));
        drawText("2   ВОССТАНОВИТЬ 1 ЖИЗНЬ", 300, 425, 19, sf::Color(215, 160, 135));
        drawText("    Вернуть потерянное здоровье", 300, 455, 14, sf::Color(145, 165, 125));
        drawText("3   УВЕЛИЧИТЬ ЗАПАС ЖИЗНЕЙ", 300, 515, 19, sf::Color(160, 195, 220));
        drawText("    +1 к максимуму и +1 к здоровью", 300, 545, 14, sf::Color(145, 165, 125));
        drawCenteredText("НАЖМИ 1, 2 ИЛИ 3", WINDOW_WIDTH / 2.0f, 575, 15, sf::Color(220, 195, 115));
        drawCrt();
    }

    void drawGameOver() {
        drawRect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, sf::Color(0, 0, 0, 205));
        drawCenteredText("ИГРА ОКОНЧЕНА", WINDOW_WIDTH / 2.0f, 245, 46, sf::Color(205, 105, 85));
        drawCenteredText("АПОКАЛИПСИС ПОБЕДИЛ В ЭТОТ РАЗ", WINDOW_WIDTH / 2.0f, 325, 17, sf::Color(195, 185, 150));
        drawCenteredText("СЧЁТ  " + std::to_string(score), WINDOW_WIDTH / 2.0f, 390, 25, sf::Color(170, 220, 140));
        drawCenteredText("УБИЙСТВА  " + std::to_string(kills) + "     ВОЛНА  " + std::to_string(wave), WINDOW_WIDTH / 2.0f, 435, 18, sf::Color(170, 180, 145));
        drawCenteredText("ENTER ИЛИ R - ЕЩЁ РАЗ", WINDOW_WIDTH / 2.0f, 520, 18, sf::Color(220, 195, 115));
        drawCenteredText("ESC - В ГЛАВНОЕ МЕНЮ", WINDOW_WIDTH / 2.0f, 555, 15, sf::Color(145, 160, 125));
        drawCrt();
    }

};

int main() {
    sf::RenderWindow window(
        sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT),
        GAME_TITLE,
        sf::Style::Titlebar | sf::Style::Close | sf::Style::Resize
    );

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
