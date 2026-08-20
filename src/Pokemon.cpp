#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <glm/glm.hpp>

// give it a random destination and let it walk there
// if reach destination, give it a new destination
class Pokemon
{
private:
    float x;
    float y;
    float z;
    float destinationX;
    float destinationY;
    float destinationZ;
    float speed;
    float direction;
    float heading;
    float targetHeading;
    float motionPhase;
    int beenCaught;
    int flyPokemon;
    int pokemonID;

public:
    static float random(float min, float max)
    {
        return (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * (max - min) + min;
    }

    Pokemon()
        : x(0.0f), y(0.0f), z(0.0f), destinationX(0.0f), destinationY(0.0f),
          destinationZ(0.0f), speed(0.0f), direction(0.0f), heading(0.0f),
          targetHeading(0.0f), motionPhase(0.0f), beenCaught(0),
          flyPokemon(0), pokemonID(-1) {}

    ~Pokemon() {}

    Pokemon(int flyPokemon, int pokemonID) : Pokemon()
    {
        this->pokemonID = pokemonID;
        this->flyPokemon = flyPokemon;
        motionPhase = static_cast<float>(pokemonID % 11) * 0.53f;
        x = random(-45, 45);
        z = random(-45, 45);

        beenCaught = 0;

        if (flyPokemon)
        {
            y = random(15, 35);
            speed = 3.2f;
            setDestination(x + random(-16, 16),
                           random(18, 30),
                           z + random(-16, 16));
        }
        else
        {
            y = 0;
            speed = 1.8f;
            setDestination(x + random(-5, 5),
                           y + 0,
                           z + random(-5, 5));
        }

    }

    void update(double deltaSeconds)
    {
        if (beenCaught)
        {
            return;
        }

        deltaSeconds = std::max(0.0, std::min(deltaSeconds, 0.05));
        if (isAtDestination())
        {
            if (flyPokemon)
            {
                setDestination(x + random(-16, 16),
                               random(18, 30),
                               z + random(-16, 16));
            }
            else
            {
                setDestination(x + random(-5, 5),
                               y + 0,
                               z + random(-5, 5));
            }
        }
        walkToDestination(deltaSeconds);
    }

    void setCaught(int flag) { beenCaught = flag; }

    int getCaught() { return beenCaught; }

    void setDestination(float x, float y, float z)
    {
        destinationX = std::max(-48.0f, std::min(48.0f, x));
        destinationY = std::max(0.0f, std::min(35.0f, y));
        destinationZ = std::max(-48.0f, std::min(48.0f, z));

        float horizontalX = destinationX - this->x;
        float horizontalZ = destinationZ - this->z;
        if (std::fabs(horizontalX) + std::fabs(horizontalZ) > 0.001f)
        {
            targetHeading = std::atan2(horizontalX, horizontalZ);
        }
    }

    glm::vec3 getPos() { return glm::vec3(x, y, z); }

    float getHeading() const { return heading; }

    float getMotionPhase() const { return motionPhase; }

    void walkToDestination(double deltaSeconds)
    {
        // walk to destination from a point walk to another point

        // get the direction
        glm::vec3 direction = glm::vec3(destinationX - x, destinationY - y, destinationZ - z);
        // normalize the direction
        float distance = glm::length(direction);
        if (distance <= 0.001f)
        {
            return;
        }

        direction /= distance;

        float turn = targetHeading - heading;
        while (turn > 3.1415926f)
        {
            turn -= 6.2831852f;
        }
        while (turn < -3.1415926f)
        {
            turn += 6.2831852f;
        }
        heading += turn * std::min(1.0f, static_cast<float>(deltaSeconds) * 4.0f);

        float step = std::min(speed * static_cast<float>(deltaSeconds), distance);
        x += direction.x * step;
        y += direction.y * step;
        z += direction.z * step;
    }

    int isAtDestination()
    {
        // check if reach destination
        if ((std::fabs(x - destinationX) < 0.5f) && (std::fabs(y - destinationY) < 0.5f) &&
            (std::fabs(z - destinationZ) < 0.5f))
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }

    int getID() { return pokemonID; }
};
