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
          destinationZ(0.0f), speed(0.0f), direction(0.0f), beenCaught(0),
          flyPokemon(0), pokemonID(-1) {}

    ~Pokemon() {}

    Pokemon(int flyPokemon, int pokemonID) : Pokemon()
    {
        this->pokemonID = pokemonID;
        this->flyPokemon = flyPokemon;
        x = random(-45, 45);
        z = random(-45, 45);

        beenCaught = 0;

        if (flyPokemon)
        {
            y = random(15, 35);
            speed = 5.0f;
            setDestination(x + random(-20, 20),
                           random(15, 35),
                           z + random(-20, 20));
        }
        else
        {
            y = 0;
            speed = 2.5f;
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
                setDestination(x + random(-20, 20),
                               random(15, 35),
                               z + random(-20, 20));
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
    }

    glm::vec3 getPos() { return glm::vec3(x, y, z); }

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
